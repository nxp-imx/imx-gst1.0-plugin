/*
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2018-2026 NXP
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Includes
 */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/video/video-event.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/encoding-target.h>
#include "recorder_engine.h"
#include <gstimxsocfeatures.h>
#include <gst/allocators/gstdmabuf.h>
#include <libdrm/drm_fourcc.h>
#include <gst/app/gstappsink.h>

/*
 * debug logging
 */
GST_DEBUG_CATEGORY_STATIC (recorder_engine);
#define GST_CAT_DEFAULT recorder_engine

#define TIME_DIFF(a,b) ((((gint64)(a)) - ((gint64)(b))) / (gdouble) GST_SECOND)

#define TIME_FORMAT "02d.%09u"
#define TIMEDIFF_FORMAT "0.6lf"

#define ADD_DATE_TIME
#ifdef ADD_DATE_TIME
#define DATE_TIME "clockoverlay halignment=left valignment=top time-format=\"%Y/%m/%d  %H:%M:%S \" ! "
#else
#define DATE_TIME ""
#endif

// this is only for test purpose to measure end to end latency, defaul not enabled
//#define ADD_TIME_OVERLAY 
#ifdef ADD_TIME_OVERLAY
#define TIME_OVERLAY "timeoverlay halignment=right valignment=top text=\"Stream time:\" ! "
#define DATE_TIME "" //we only have one watermark to save performance
#else
#define TIME_OVERLAY ""
#endif

#define USE_HW_COMPOSITOR
#ifdef USE_HW_COMPOSITOR
#define HW_COMPOSITOR "queue ! imxvideoconvert_g2d composition-meta-enable=true in-place=true ! "
#else
#define HW_COMPOSITOR ""
#endif

#define TIME_ARGS(t) \
        (GST_CLOCK_TIME_IS_VALID (t) && (t) < 99 * GST_SECOND) ? \
        (gint) ((((GstClockTime)(t)) / GST_SECOND) % 60) : 99, \
        (GST_CLOCK_TIME_IS_VALID (t) && ((t) < 99 * GST_SECOND)) ? \
        (guint) (((GstClockTime)(t)) % GST_SECOND) : 999999999

#define TIMEDIFF_ARGS(t) (t)
#define CHECK_PARAM(value, threshold) \
    do{\
        if (value >= threshold){\
            return RE_RESULT_PARAMETER_INVALID;\
        }\
    }while(0)

typedef enum {
  RE_STATE_NULL,
  RE_STATE_INITED,
  RE_STATE_PREPARED,
  RE_STATE_RUNNING,
  RE_STATE_PAUSED,
  RE_STATE_STOPPED
} RecorderState;

typedef struct _KeyMap {
  int nKey;
  REchar * tag;
} KeyMap;

typedef struct _CaptureTiming
{
  GstClockTime start_capture;
  GstClockTime got_preview;
  GstClockTime capture_done;
  GstClockTime precapture;
  GstClockTime camera_capture;
} CaptureTiming;

typedef struct _CaptureTimingStats
{
  GstClockTime shot_to_shot;
  GstClockTime shot_to_save;
  GstClockTime shot_to_snapshot;
  GstClockTime preview_to_precapture;
  GstClockTime shot_to_buffer;
} CaptureTimingStats;

static void
capture_timing_stats_add (CaptureTimingStats * a, CaptureTimingStats * b)
{
  a->shot_to_shot += b->shot_to_shot;
  a->shot_to_snapshot += b->shot_to_snapshot;
  a->shot_to_save += b->shot_to_save;
  a->preview_to_precapture += b->preview_to_precapture;
  a->shot_to_buffer += b->shot_to_buffer;
}

static void
capture_timing_stats_div (CaptureTimingStats * stats, gint div)
{
  stats->shot_to_shot /= div;
  stats->shot_to_snapshot /= div;
  stats->shot_to_save /= div;
  stats->preview_to_precapture /= div;
  stats->shot_to_buffer /= div;
}

#define PRINT_STATS(d,s) g_print ("%02d | %" TIME_FORMAT " | %" \
    TIME_FORMAT "   | %" TIME_FORMAT " | %" TIME_FORMAT \
    "    | %" TIME_FORMAT "\n", d, \
    TIME_ARGS ((s)->shot_to_save), TIME_ARGS ((s)->shot_to_snapshot), \
    TIME_ARGS ((s)->shot_to_shot), \
    TIME_ARGS ((s)->preview_to_precapture), \
    TIME_ARGS ((s)->shot_to_buffer))

#define SHOT_TO_SAVE(t) ((t)->capture_done - (t)->start_capture)
#define SHOT_TO_SNAPSHOT(t) ((t)->got_preview - (t)->start_capture)
#define PREVIEW_TO_PRECAPTURE(t) ((t)->precapture - (t)->got_preview)
#define SHOT_TO_BUFFER(t) ((t)->camera_capture - (t)->start_capture)

#define MODE_VIDEO 2
#define MODE_IMAGE 1

#define EV_COMPENSATION_NONE -G_MAXFLOAT
#define APERTURE_NONE -G_MAXINT
#define FLASH_MODE_NONE -G_MAXINT
#define SCENE_MODE_NONE -G_MAXINT
#define EXPOSURE_NONE -G_MAXINT64
#define ISO_SPEED_NONE -G_MAXINT
#define WHITE_BALANCE_MODE_NONE -G_MAXINT
#define COLOR_TONE_MODE_NONE -G_MAXINT

typedef struct _gRecorderEngine
{
  GstElement *camerabin;
  GstElement *viewfinder_sink;
  GstElement *video_sink;
  GstElement *video_src;
  gulong camera_probe_id;
  gulong viewfinder_probe_id;
  GMainLoop *loop;

  /* commandline options */
  gchar *videosrc_name;
  gchar *videodevice_name;
  gchar *audiosrc_name;
  gchar *audiodevice_name;
  gchar *wrappersrc_name;
  gchar *imagepp_name;
  gchar *vfsink_name;
  gchar *video_effect_name;
  gchar *video_detect_name;
  gchar *date_time;
  gint image_width;
  gint image_height;
  gint image_format;
  gint view_framerate_num;
  gint view_framerate_den;
  gboolean no_xwindow;
  gchar *gep_targetname;
  gchar *gep_profilename;
  gchar *gep_filename;
  gchar *image_capture_caps_str;
  gchar *viewfinder_caps_str;
  gchar *video_capture_caps_str;
  gchar *audio_capture_caps_str;
  gboolean performance_measure;
  gchar *performance_targets_str;
  gchar *camerabin_flags;

  gint mode;
  gint zoom;

  gint capture_time;
  gint capture_count;
  gint capture_total;
  gulong stop_capture_cb_id;

  /* photography interface command line options */
  gfloat ev_compensation;
  gint aperture;
  gint flash_mode;
  gint scene_mode;
  gint64 exposure;
  gint iso_speed;
  gint wb_mode;
  gint color_mode;

  gchar *viewfinder_filter;

  int x_width;
  int x_height;

  GString *filename;
  GString *host;
  gint port;
  gint max_files;
  gint64 max_file_size;
  gint64 max_file_duration;
  guint fragment_duration;

  /* State used by the keyframe-aligned file-split probe.
   * All fields are protected by split_lock because the buffer probe fires
   * on the streaming thread while other accesses could occur on different
   * threads (e.g. the application thread or a GLib main loop callback). */
  GMutex split_lock;
  gint64 current_file_bytes;
  GstClockTime current_file_start_ts;
  gboolean waiting_for_keyframe;
  guint force_key_unit_count;

  gchar *preview_caps_name;

  /* X window variables */
  guintptr window;

  /* timing data */
  GstClockTime initial_time;
  GstClockTime startup_time;
  GstClockTime change_mode_before;
  GstClockTime change_mode_after;
  GList *capture_times;

  GstClockTime target_startup;
  GstClockTime target_change_mode;
  GstClockTime target_shot_to_shot;
  GstClockTime target_shot_to_save;
  GstClockTime target_shot_to_snapshot;
  GstClockTime target_preview_to_precapture;
  GstClockTime target_shot_to_buffer;
  REuint32 container_format;
  REuint32 audio_encoder_format;
  REuint32 video_encoder_format;
  RecorderState state;
  gboolean stop_done;
  gboolean disable_viewfinder;
  REtime base_media_timeUs;
  GstCaps * camera_caps;
  GstCaps * camera_output_caps;
  RecorderEngineEventHandler app_callback;
  gpointer pAppData;
  GMutex lock;
  GstCaps *audio_output_caps;
  gboolean record_screen;
} gRecorderEngine;


/*
 * Prototypes
 */
static REresult run_pipeline (gRecorderEngine *recorder);
static void set_metadata (GstElement * camera);
static REresult get_media_time(RecorderEngineHandle handle, REtime *pMediaTimeUs);

static GstEncodingProfile *
create_encording_profile (gRecorderEngine *recorder)
{
  GstEncodingContainerProfile *container = NULL;
  GstEncodingProfile *sprof = NULL;
  GstCaps *caps = NULL;

  GST_DEBUG ("container format:%d", recorder->container_format);
  switch (recorder->container_format) {
    case RE_OUTPUT_FORMAT_DEFAULT:
    case RE_OUTPUT_FORMAT_MOV:
    case RE_OUTPUT_FORMAT_FMP4:
      caps = gst_caps_new_simple ("video/quicktime", "variant", G_TYPE_STRING, 
            "apple", NULL);
      container = gst_encoding_container_profile_new ("mov", NULL, caps, NULL);
      gst_caps_unref (caps);
      break;
    case RE_OUTPUT_FORMAT_MKV:
      caps = gst_caps_new_empty_simple ("video/x-matroska");
      container = gst_encoding_container_profile_new ("mkv", NULL, caps, NULL);
      gst_caps_unref (caps);
      break;
    case RE_OUTPUT_FORMAT_AVI:
      caps = gst_caps_new_empty_simple ("video/x-msvideo");
      container = gst_encoding_container_profile_new ("avi", NULL, caps, NULL);
      gst_caps_unref (caps);
      break;
    case RE_OUTPUT_FORMAT_FLV:
      caps = gst_caps_new_empty_simple ("video/x-flv");
      container = gst_encoding_container_profile_new ("flv", NULL, caps, NULL);
      gst_caps_unref (caps);
      break;
    case RE_OUTPUT_FORMAT_TS:
      caps = gst_caps_new_simple ("video/mpegts", "systemstream", G_TYPE_BOOLEAN, 
            TRUE, "packetsize", G_TYPE_INT, 188, NULL);
      container = gst_encoding_container_profile_new ("ts", NULL, caps, NULL);
      gst_caps_unref (caps);
      break;
    default:
      break;
  }

  switch (recorder->video_encoder_format) {
    case RE_VIDEO_ENCODER_DEFAULT:
    case RE_VIDEO_ENCODER_H264:
        caps = gst_caps_new_empty_simple ("video/x-h264");
        sprof = (GstEncodingProfile *)
          gst_encoding_video_profile_new (caps, NULL, NULL, 1);
        //FIXME: videorate has issue.
        gst_encoding_video_profile_set_variableframerate ((GstEncodingVideoProfile
              *) sprof, TRUE);
        gst_encoding_container_profile_add_profile (container, sprof);
        gst_caps_unref (caps);
        break;
    case RE_VIDEO_ENCODER_H265:
        caps = gst_caps_new_empty_simple ("video/x-h265");
        sprof = (GstEncodingProfile *)
          gst_encoding_video_profile_new (caps, NULL, NULL, 1);
        //FIXME: videorate has issue.
        gst_encoding_video_profile_set_variableframerate ((GstEncodingVideoProfile
              *) sprof, TRUE);
        gst_encoding_container_profile_add_profile (container, sprof);
        gst_caps_unref (caps);
        break;
    case RE_VIDEO_ENCODER_MPEG4:
        caps = gst_caps_new_simple
            ("video/mpeg", "mpegversion", G_TYPE_INT, 4, "systemstream", 
             G_TYPE_BOOLEAN, FALSE, NULL);
        sprof = (GstEncodingProfile *)
          gst_encoding_video_profile_new (caps, NULL, NULL, 1);
        //FIXME: videorate has issue.
        gst_encoding_video_profile_set_variableframerate ((GstEncodingVideoProfile
              *) sprof, TRUE);
        gst_encoding_container_profile_add_profile (container, sprof);
        gst_caps_unref (caps);
        break;
     case RE_VIDEO_ENCODER_H263:
        caps = gst_caps_new_empty_simple ("video/x-h263");
        sprof = (GstEncodingProfile *)
          gst_encoding_video_profile_new (caps, NULL, NULL, 1);
        //FIXME: videorate has issue.
        gst_encoding_video_profile_set_variableframerate ((GstEncodingVideoProfile
              *) sprof, TRUE);
        gst_encoding_container_profile_add_profile (container, sprof);
        gst_caps_unref (caps);
        break;
     case RE_VIDEO_ENCODER_MJPEG:
        caps = gst_caps_new_empty_simple ("image/jpeg");
        sprof = (GstEncodingProfile *)
          gst_encoding_video_profile_new (caps, NULL, NULL, 1);
        //FIXME: videorate has issue.
        gst_encoding_video_profile_set_variableframerate ((GstEncodingVideoProfile
              *) sprof, TRUE);
        gst_encoding_container_profile_add_profile (container, sprof);
        gst_caps_unref (caps);
        break;
    case RE_VIDEO_ENCODER_VP8:
        caps = gst_caps_new_empty_simple ("video/x-vp8");
        sprof = (GstEncodingProfile *)
          gst_encoding_video_profile_new (caps, NULL, NULL, 1);
        //FIXME: videorate has issue.
        gst_encoding_video_profile_set_variableframerate ((GstEncodingVideoProfile
              *) sprof, TRUE);
        gst_encoding_container_profile_add_profile (container, sprof);
        gst_caps_unref (caps);
        break;
    default:
      break;
  }

  switch (recorder->audio_encoder_format) {
    case RE_AUDIO_ENCODER_DEFAULT:
    case RE_AUDIO_ENCODER_MP3:
      /* make pipeline auto plug lamemp3enc as audio encoder */
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 3, NULL);
      gst_encoding_container_profile_add_profile (container, (GstEncodingProfile *)
          gst_encoding_audio_profile_new (caps, NULL, NULL, 1));
      gst_caps_unref (caps);
      break;
    default:
      break;
  }

  return (GstEncodingProfile *) container;
}

static gboolean
file_split_threshold_reached (gRecorderEngine *recorder, GstBuffer *buf)
{
  if (recorder->max_file_size > 0 &&
      recorder->current_file_bytes >= recorder->max_file_size)
    return TRUE;

  if (recorder->max_file_duration > 0 &&
      GST_BUFFER_PTS_IS_VALID (buf) &&
      GST_CLOCK_TIME_IS_VALID (recorder->current_file_start_ts)) {
    GstClockTime elapsed = GST_BUFFER_PTS (buf) - recorder->current_file_start_ts;
    if (elapsed >= (GstClockTime) recorder->max_file_duration * GST_SECOND)
      return TRUE;
  }

  return FALSE;
}

/*
 * Pad probe installed on the muxer's video sink pad (inside encodebin).
 * At this point buffers are individual encoded frames and carry the
 * DELTA_UNIT flag reliably.
 *
 * Strategy (next-file=3 / KEY_UNIT_EVENT on multifilesink):
 *   - Track encoded bytes and PTS-based duration for the current segment.
 *   - When the configured threshold is exceeded, send an upstream
 *     force-key-unit event so the encoder emits a fresh IDR/keyframe.
 *   - Mark waiting_for_keyframe=TRUE to suppress further threshold checks.
 *   - When the next keyframe arrives, send a downstream force-key-unit
 *     event toward multifilesink (via the muxer src pad) so it closes
 *     the current file and opens the next one starting from that keyframe.
 */
static GstPadProbeReturn
file_split_probe (GstPad * pad, GstPadProbeInfo * info, gpointer udata)
{
  gRecorderEngine *recorder = (gRecorderEngine *) udata;
  GstBuffer *buf;
  gboolean is_keyframe;
  guint fku_count = 0;
  GstClockTime pts;
  gboolean do_upstream_fku = FALSE;
  gboolean do_downstream_fku = FALSE;

  if (!(GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER))
    return GST_PAD_PROBE_OK;

  buf = GST_PAD_PROBE_INFO_BUFFER (info);
  is_keyframe = !GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
  pts = GST_BUFFER_PTS (buf);

  g_mutex_lock (&recorder->split_lock);

  /* Latch the segment start timestamp on the very first keyframe of the
   * recording. Subsequent segments are latched after each split (below).
   * Only keyframes are used as anchor points so that duration measurement
   * stays aligned to actual GOP boundaries. */
  if (!GST_CLOCK_TIME_IS_VALID (recorder->current_file_start_ts) && is_keyframe) {
    recorder->current_file_start_ts = pts;
    GST_DEBUG ("first keyframe, segment start PTS %" GST_TIME_FORMAT,
        GST_TIME_ARGS (pts));
  }

  if (recorder->waiting_for_keyframe) {
    if (is_keyframe) {
      /* The encoder has delivered the requested IDR. Embed the current
       * count value in the downstream FKU so mux_src_event_probe can let
       * it through while dropping the mpegtsmux automatic echo (count=0). */
      fku_count = ++recorder->force_key_unit_count;
      do_downstream_fku = TRUE;

      recorder->waiting_for_keyframe = FALSE;
      recorder->current_file_bytes = 0;
      recorder->current_file_start_ts = pts;
      GST_DEBUG ("IDR received, splitting segment at PTS %" GST_TIME_FORMAT
          ", FKU count=%u", GST_TIME_ARGS (pts), fku_count);
    }
    /* Continue accumulating bytes even while waiting for the IDR so the
     * byte counter reflects the true encoded size of the segment. */
  }

  if (!recorder->waiting_for_keyframe &&
      file_split_threshold_reached (recorder, buf)) {
    /* Threshold reached: request an IDR from the encoder. */
    do_upstream_fku = TRUE;
    recorder->waiting_for_keyframe = TRUE;
    GST_DEBUG ("threshold reached at PTS %" GST_TIME_FORMAT
        ", bytes=%" G_GINT64_FORMAT ", requesting IDR",
        GST_TIME_ARGS (pts), recorder->current_file_bytes);
  }

  g_mutex_unlock (&recorder->split_lock);

  /* Send events outside the lock to avoid potential deadlocks when
   * GStreamer's pad machinery takes internal locks during event delivery. */
  if (do_downstream_fku) {
    GstElement *muxer = GST_PAD_PARENT (pad);
    GstPad *mux_src = muxer ? gst_element_get_static_pad (muxer, "src") : NULL;
    if (mux_src) {
      gst_pad_push_event (mux_src,
          gst_video_event_new_downstream_force_key_unit (
              pts, pts, pts, TRUE, fku_count));
      gst_object_unref (mux_src);
    }
  }

  if (do_upstream_fku) {
    GstPad *peer = gst_pad_get_peer (pad);
    if (peer) {
      gst_pad_send_event (peer,
          gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE,
              TRUE, 0));
      gst_object_unref (peer);
    }
  }

  return GST_PAD_PROBE_OK;
}

/*
 * Buffer probe on the muxer src pad.
 * Counts actual TS output bytes into current_file_bytes so the split
 * threshold is compared against the real on-disk file size (including TS
 * packetization overhead and PAT/PMT/PCR packets) rather than raw ES bytes.
 * Protected by split_lock shared with file_split_probe.
 */
static GstPadProbeReturn
mux_src_buffer_probe (GstPad * pad, GstPadProbeInfo * info, gpointer udata)
{
  gRecorderEngine *recorder = (gRecorderEngine *) udata;
  gsize size = 0;

  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER) {
    size = gst_buffer_get_size (GST_PAD_PROBE_INFO_BUFFER (info));
  } else if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *list = GST_PAD_PROBE_INFO_BUFFER_LIST (info);
    guint i, n = gst_buffer_list_length (list);
    for (i = 0; i < n; i++)
      size += gst_buffer_get_size (gst_buffer_list_get (list, i));
  }

  if (size > 0) {
    g_mutex_lock (&recorder->split_lock);
    recorder->current_file_bytes += size;
    g_mutex_unlock (&recorder->split_lock);
  }

  return GST_PAD_PROBE_OK;
}

/*
 * Event probe on the muxer src pad.
 * mpegtsmux automatically echoes every upstream force-key-unit as a downstream
 * force-key-unit when it starts a new IDR access unit.  The upstream event we
 * send carries count=0; its echo therefore also has count=0.  Our own
 * downstream FKU (pushed from file_split_probe) uses force_key_unit_count >= 1.
 * This probe drops only count==0 echoes to prevent a double file split.
 */
static GstPadProbeReturn
mux_src_event_probe (GstPad * pad, GstPadProbeInfo * info, gpointer udata)
{
  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);
    if (GST_EVENT_TYPE (event) == GST_EVENT_CUSTOM_DOWNSTREAM &&
        gst_video_event_is_force_key_unit (event)) {
      GstClockTime running_time, timestamp, stream_time;
      gboolean all_headers;
      guint count;
      gst_video_event_parse_downstream_force_key_unit (event,
          &timestamp, &stream_time, &running_time, &all_headers, &count);
      if (count == 0) {
        GST_DEBUG ("dropping mpegtsmux FKU echo (count=0) to prevent double file split");
        return GST_PAD_PROBE_DROP;
      }
    }
  }
  return GST_PAD_PROBE_OK;
}

/*
 * One-shot probe installed on every muxer sink pad by setup_file_split_probe().
 * Called from the streaming thread on the first buffer, at which point pad
 * caps are guaranteed to be negotiated (gst_pad_get_current_caps() is valid).
 * If the pad carries a video media type the real file_split_probe (and the
 * mux_src_event_probe) are installed and this probe removes itself.
 * Non-video pads simply remove this probe without doing anything else.
 */
static GstPadProbeReturn
autodetect_video_pad_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer udata)
{
  gRecorderEngine *recorder = (gRecorderEngine *) udata;
  GstCaps *caps;
  const gchar *name = NULL;

  caps = gst_pad_get_current_caps (pad);
  if (caps) {
    name = gst_structure_get_name (gst_caps_get_structure (caps, 0));
    gst_caps_unref (caps);
  }

  if (!name || !g_str_has_prefix (name, "video/")) {
    /* Not a video pad. Byte counting is done at the mux src pad
     * (mux_src_buffer_probe) so no per-pad probe is needed here. */
    GST_DEBUG ("autodetect: pad %s:%s is audio/other (%s), removing probe",
        GST_DEBUG_PAD_NAME (pad), name ? name : "(no caps)");
    return GST_PAD_PROBE_REMOVE;
  }

  /* This is the video pad. Install the real split probe on it. */
  GST_DEBUG ("autodetect: pad %s:%s is video (%s), installing split probe",
      GST_DEBUG_PAD_NAME (pad), name);

  recorder->current_file_bytes = 0;
  recorder->current_file_start_ts = GST_CLOCK_TIME_NONE;
  recorder->waiting_for_keyframe = FALSE;
  recorder->force_key_unit_count = 0;

  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      file_split_probe, recorder, NULL);
  GST_DEBUG ("file_split_probe installed on pad %s:%s",
      GST_DEBUG_PAD_NAME (pad));

  /* Install event probe on the muxer src pad to drop the automatic
   * downstream FKU echo (count=0) that mpegtsmux emits for each IDR,
   * preventing a double file-split when we push our own FKU event. */
  {
    GstElement *muxer = GST_ELEMENT (gst_pad_get_parent (pad));
    if (muxer) {
      GstPad *mux_src = gst_element_get_static_pad (muxer, "src");
      if (mux_src) {
        /* Count actual TS output bytes for accurate file-size tracking. */
        gst_pad_add_probe (mux_src,
            GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
            mux_src_buffer_probe, recorder, NULL);
        GST_DEBUG ("mux_src_buffer_probe installed on pad %s:%s",
            GST_DEBUG_PAD_NAME (mux_src));
        gst_pad_add_probe (mux_src,
            GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, mux_src_event_probe,
            recorder, NULL);
        GST_DEBUG ("mux_src_event_probe installed on pad %s:%s",
            GST_DEBUG_PAD_NAME (mux_src));
        gst_object_unref (mux_src);
      }
      gst_object_unref (muxer);
    }
  }

  /* Remove this autodetect probe; file_split_probe takes over. */
  return GST_PAD_PROBE_REMOVE;
}

/*
 * Install autodetect_video_pad_probe on every muxer sink pad.
 * Called after the pipeline reaches READY so the muxer element already
 * exists. The probes defer actual video-pad identification to the
 * streaming thread where pad caps are guaranteed to be valid (see
 * autodetect_video_pad_probe above).
 */
static void
setup_file_split_probe (gRecorderEngine *recorder)
{
  GstElement *muxer;
  GstIterator *it;
  GValue item = G_VALUE_INIT;
  gint n_probes = 0;

  muxer = gst_bin_get_by_name (GST_BIN (recorder->camerabin), "muxer");
  if (!muxer) {
    GST_WARNING ("muxer element not found, file-split probe not installed");
    return;
  }

  it = gst_element_iterate_sink_pads (muxer);
  while (gst_iterator_next (it, &item) == GST_ITERATOR_OK) {
    GstPad *p = g_value_get_object (&item);
    gst_pad_add_probe (p, GST_PAD_PROBE_TYPE_BUFFER,
        autodetect_video_pad_probe, recorder, NULL);
    GST_DEBUG ("autodetect probe installed on pad %s:%s",
        GST_DEBUG_PAD_NAME (p));
    n_probes++;
    g_value_reset (&item);
  }
  gst_iterator_free (it);

  if (n_probes == 0)
    GST_WARNING ("muxer has no sink pads, file-split probe not installed");

  gst_object_unref (muxer);
}

static GstPadProbeReturn
camera_src_get_timestamp_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer udata)
{
  gRecorderEngine *recorder = (gRecorderEngine *)udata;
  CaptureTiming *timing;

  timing = (CaptureTiming *) g_list_first (recorder->capture_times)->data;
  timing->camera_capture = gst_util_get_timestamp ();

  return GST_PAD_PROBE_REMOVE;
}

static GstPadProbeReturn
viewfinder_get_timestamp_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer udata)
{
  gRecorderEngine *recorder = (gRecorderEngine *)udata;
  CaptureTiming *timing;

  timing = (CaptureTiming *) g_list_first (recorder->capture_times)->data;
  timing->precapture = gst_util_get_timestamp ();

  return GST_PAD_PROBE_REMOVE;
}

static GstBusSyncReply
sync_bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  gRecorderEngine *recorder = (gRecorderEngine *)data;
  const GstStructure *st;
  const GValue *image;
  GstBuffer *buf = NULL;
  gchar *preview_filename = NULL;
  FILE *f = NULL;
  size_t written;

  GST_LOG ("Got %s sync message\n", GST_MESSAGE_TYPE_NAME (message));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      if(recorder->app_callback != NULL) {
        (*(recorder->app_callback))(recorder->pAppData, RE_EVENT_ERROR_UNKNOWN, 0);
      }
      break;
    }
    case GST_MESSAGE_ELEMENT:{
      st = gst_message_get_structure (message);
      if (st) {
        if (gst_message_has_name (message, "prepare-xwindow-id")) {
          if (!recorder->no_xwindow && recorder->window) {
            gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY
                (GST_MESSAGE_SRC (message)), recorder->window);
            gst_message_unref (message);
            message = NULL;
            return GST_BUS_DROP;
          }
        } else if (gst_structure_has_name (st, "preview-image")) {
          CaptureTiming *timing;

          GST_DEBUG ("preview-image");

          timing = (CaptureTiming *) g_list_first (recorder->capture_times)->data;
          timing->got_preview = gst_util_get_timestamp ();

          {
            /* set up probe to check when the viewfinder gets data */
            GstPad *pad = gst_element_get_static_pad (recorder->viewfinder_sink, "sink");

            recorder->viewfinder_probe_id =
                gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
                viewfinder_get_timestamp_probe, recorder, NULL);

            gst_object_unref (pad);
          }

          /* extract preview-image from msg */
          image = gst_structure_get_value (st, "buffer");
          if (image) {
            buf = gst_value_get_buffer (image);
            preview_filename = g_strdup_printf ("test_vga.rgb");
            f = g_fopen (preview_filename, "w");
            if (f) {
              GstMapInfo map;

              gst_buffer_map (buf, &map, GST_MAP_READ);
              written = fwrite (map.data, map.size, 1, f);
              gst_buffer_unmap (buf, &map);
              if (!written) {
                g_print ("error writing file\n");
              }
              fclose (f);
            } else {
              g_print ("error opening file for raw image writing\n");
            }
            g_free (preview_filename);
          }
        } else if (gst_structure_has_name (st, "facedetect")) {
          const GValue *value_list = gst_structure_get_value (st, "faces");
          REVideoRect object_pos;
          const GstStructure *str;
          gchar *sstr;
          guint i, n;

          sstr = gst_structure_to_string (st);
          GST_INFO ("Got sync message: %s\n", sstr);
          g_free (sstr);

          n = gst_value_list_get_size (value_list);
          for (i = 0; i < n; i++) {
            const GValue *value = gst_value_list_get_value (value_list, i);
            str = gst_value_get_structure (value);

            gst_structure_get_uint (str, "x", (guint *)(&object_pos.left));
            gst_structure_get_uint (str, "y", (guint *)(&object_pos.top));
            gst_structure_get_uint (str, "width", (guint *)(&object_pos.width));
            gst_structure_get_uint (str, "height", (guint *)(&object_pos.height));

            if(recorder->app_callback != NULL) {
              (*(recorder->app_callback))(recorder->pAppData, RE_EVENT_OBJECT_POSITION, &object_pos);
            }
          }
        }
      }
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC (message) == (GstObject *) recorder->camerabin) {
        GstState newstate;

        gst_message_parse_state_changed (message, NULL, &newstate, NULL);
        if (newstate == GST_STATE_PLAYING) {
          recorder->startup_time = gst_util_get_timestamp ();
        }
      }
      break;
    default:
      /* unhandled message */
      break;
  }
  return GST_BUS_PASS;
}

static gboolean
bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  gRecorderEngine *recorder = (gRecorderEngine *)data;

  GST_LOG ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_print ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);

      /* Write debug graph to file */
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (recorder->camerabin),
          GST_DEBUG_GRAPH_SHOW_ALL, "camerabin.error");

      if(recorder->app_callback != NULL) {
        (*(recorder->app_callback))(recorder->pAppData, RE_EVENT_ERROR_UNKNOWN, 0);
      }
      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_IS_BIN (GST_MESSAGE_SRC (message))) {
        GstState oldstate, newstate;

        gst_message_parse_state_changed (message, &oldstate, &newstate, NULL);
        GST_DEBUG_OBJECT (GST_MESSAGE_SRC (message), "state-changed: %s -> %s",
            gst_element_state_get_name (oldstate),
            gst_element_state_get_name (newstate));
      }
      break;
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      GST_INFO ("got eos() - should not happen");

      if(recorder->app_callback != NULL) {
        (*(recorder->app_callback))(recorder->pAppData, RE_EVENT_EOS, 0);
      }
      break;
    case GST_MESSAGE_ELEMENT:
      if (GST_MESSAGE_SRC (message) == (GstObject *) recorder->camerabin) {
        const GstStructure *structure = gst_message_get_structure (message);

        if (gst_structure_has_name (structure, "image-done")) {
          CaptureTiming *timing;
          const gchar *fname = gst_structure_get_string (structure, "filename");

          GST_DEBUG ("image done: %s", fname);
          timing = (CaptureTiming *) g_list_first (recorder->capture_times)->data;
          timing->capture_done = gst_util_get_timestamp ();

          if (recorder->capture_count < recorder->capture_total) {
            run_pipeline (recorder);
          } else {
            recorder->stop_done = TRUE;
            if(recorder->app_callback != NULL) {
              (*(recorder->app_callback))(recorder->pAppData, RE_EVENT_MAX_FILE_COUNT_REACHED, 0);
            }
          }
        }
      }
      break;
    default:
      /* unhandled message */
      break;
  }
  return TRUE;
}

static void
cleanup_pipeline (gRecorderEngine *recorder)
{
  if (recorder->camerabin) {
    GST_INFO_OBJECT (recorder->camerabin, "stopping and destroying");
    gst_element_set_state (recorder->camerabin, GST_STATE_NULL);
    gst_object_unref (recorder->camerabin);
    recorder->camerabin = NULL;
  }
}

static GstElement *
create_ipp_bin (gRecorderEngine *recorder)
{
  GstElement *bin = NULL, *element = NULL;
  GstPad *pad = NULL;
  gchar **elements;
  GList *element_list = NULL, *current = NULL, *next = NULL;
  int i;

  bin = gst_bin_new ("ippbin");

  elements = g_strsplit (recorder->imagepp_name, ",", 0);

  for (i = 0; elements[i] != NULL; i++) {
    element = gst_element_factory_make (elements[i], NULL);
    if (element) {
      element_list = g_list_append (element_list, element);
      gst_bin_add (GST_BIN (bin), element);
    } else
      GST_WARNING ("Could create element %s for ippbin", elements[i]);
  }

  for (i = 1; i < g_list_length (element_list); i++) {
    current = g_list_nth (element_list, i - 1);
    next = g_list_nth (element_list, i);
    gst_element_link (current->data, next->data);
  }

  current = g_list_first (element_list);
  pad = gst_element_get_static_pad (current->data, "sink");
  gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
  gst_object_unref (GST_OBJECT (pad));

  current = g_list_last (element_list);
  pad = gst_element_get_static_pad (current->data, "src");
  gst_element_add_pad (bin, gst_ghost_pad_new ("src", pad));
  gst_object_unref (GST_OBJECT (pad));

  g_list_free (element_list);
  g_strfreev (elements);

  return bin;
}

static GstEncodingProfile *
load_encoding_profile (gRecorderEngine *recorder)
{
  GstEncodingProfile *prof = NULL;
  GstEncodingTarget *target = NULL;
  GError *error = NULL;

  /* if profile file was given, try to load profile from there */
  if (recorder->gep_filename && recorder->gep_profilename) {
    target = gst_encoding_target_load_from_file (recorder->gep_filename, &error);
    if (!target) {
      GST_WARNING ("Could not load target %s from file %s", recorder->gep_targetname,
          recorder->gep_filename);
      if (error) {
        GST_WARNING ("Error from file loading: %s", error->message);
        g_error_free (error);
        error = NULL;
      }
    } else {
      prof = gst_encoding_target_get_profile (target, recorder->gep_profilename);
      if (prof)
        GST_DEBUG ("Loaded encoding profile %s from %s", recorder->gep_profilename,
            recorder->gep_filename);
      else
        GST_WARNING
            ("Could not load specified encoding profile %s from file %s",
            recorder->gep_profilename, recorder->gep_filename);
    }
    /* if we could not load profile from file then try to find one from system */
  } else if (recorder->gep_profilename && recorder->gep_targetname) {
    prof = gst_encoding_profile_find (recorder->gep_targetname, recorder->gep_profilename, NULL);
    if (prof)
      GST_DEBUG ("Loaded encoding profile %s from target %s", recorder->gep_profilename,
          recorder->gep_targetname);
  } else {
    prof = create_encording_profile (recorder);
    if (prof)
      GST_DEBUG ("created encoding profile");
    else
      GST_WARNING
        ("Could not create specified encoding profile");
  }

  return prof;
}

static gboolean
setup_pipeline_element_bin (GstElement * element, const gchar * property_name,
    const gchar * element_name, GstElement ** res_elem)
{
  gboolean res = TRUE;
  GstElement *elem = NULL;

  if (element_name) {
    GError *error = NULL;

    elem = gst_parse_bin_from_description (element_name, TRUE, &error);
    if (elem) {
      g_object_set (element, property_name, elem, NULL);
      g_object_unref (elem);
    } else {
      GST_WARNING ("can't create element '%s' for property '%s'", element_name,
          property_name);
      if (error) {
        GST_ERROR ("%s", error->message);
        g_error_free (error);
      }
      res = FALSE;
    }
  } else {
    GST_DEBUG ("no element for property '%s' given", property_name);
  }
  if (res_elem)
    *res_elem = elem;
  return res;
}

static gboolean
setup_pipeline_element (GstElement * element, const gchar * property_name,
    const gchar * element_name, GstElement ** res_elem)
{
  gboolean res = TRUE;
  GstElement *elem = NULL;

  if (element_name) {
    GError *error = NULL;

    elem = gst_parse_launch (element_name, &error);
    if (elem) {
      g_object_set (element, property_name, elem, NULL);
      g_object_unref (elem);
    } else {
      GST_WARNING ("can't create element '%s' for property '%s'", element_name,
          property_name);
      if (error) {
        GST_ERROR ("%s", error->message);
        g_error_free (error);
      }
      res = FALSE;
    }
  } else {
    GST_DEBUG ("no element for property '%s' given", property_name);
  }
  if (res_elem)
    *res_elem = elem;
  return res;
}

static void
set_camerabin_caps_from_string (gRecorderEngine *recorder)
{
  GstCaps *caps = NULL;
  if (recorder->image_capture_caps_str != NULL) {
    caps = gst_caps_from_string (recorder->image_capture_caps_str);
    if (GST_CAPS_IS_SIMPLE (caps) && recorder->image_width > 0 && recorder->image_height > 0) {
      gst_caps_set_simple (caps, "width", G_TYPE_INT, recorder->image_width, "height",
          G_TYPE_INT, recorder->image_height, NULL);
    }
    GST_DEBUG ("setting image-capture-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (recorder->camerabin, "image-capture-caps", caps, NULL);
    gst_caps_unref (caps);
  }

  if (recorder->viewfinder_caps_str != NULL) {
    caps = gst_caps_from_string (recorder->viewfinder_caps_str);
    if (GST_CAPS_IS_SIMPLE (caps) && recorder->view_framerate_num > 0
        && recorder->view_framerate_den > 0) {
      gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION,
          recorder->view_framerate_num, recorder->view_framerate_den, NULL);
    }
    GST_DEBUG ("setting viewfinder-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (recorder->camerabin, "viewfinder-caps", caps, NULL);
    gst_caps_unref (caps);
  }

  if (recorder->video_capture_caps_str != NULL) {
    caps = gst_caps_from_string (recorder->video_capture_caps_str);
    GST_DEBUG ("setting video-capture-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (recorder->camerabin, "video-capture-caps", caps, NULL);
    gst_caps_unref (caps);
  }

  if (recorder->audio_capture_caps_str != NULL) {
    caps = gst_caps_from_string (recorder->audio_capture_caps_str);
    GST_DEBUG ("setting audio-capture-caps: %" GST_PTR_FORMAT, caps);
    g_object_set (recorder->camerabin, "audio-capture-caps", caps, NULL);
    gst_caps_unref (caps);
  }
}

static void set_muxer_property (gRecorderEngine *recorder)
{
  if (recorder->container_format == RE_OUTPUT_FORMAT_FMP4) {
    GstElement *mux = gst_bin_get_by_name (GST_BIN (recorder->camerabin), "muxer");

    if (mux && !g_strcmp0 (GST_OBJECT_NAME (gst_element_get_factory (mux)), "qtmux")) {
      guint fragment_duration = recorder->fragment_duration;

      /* The default duration is 1 second */
      if (fragment_duration == 0) {
        fragment_duration = 1000;
      }
      g_object_set (mux, "fragment-duration", fragment_duration, NULL);
      gst_object_unref (mux);
    } else {
       g_warning ("qtmux was not found, can't set fragment duration\n");
    }
  } else if (recorder->container_format == RE_OUTPUT_FORMAT_MKV) {
    GstElement *mux = gst_bin_get_by_name (GST_BIN (recorder->camerabin), "muxer");

    if (mux && !g_strcmp0 (GST_OBJECT_NAME (gst_element_get_factory (mux)), "matroskamux")) {
      gint64 min_cluster_duration = (gint64)recorder->fragment_duration * GST_MSECOND;

      /* The default duration is 500 ms */
      if (min_cluster_duration == 0) {
        min_cluster_duration = 500 * GST_MSECOND;
      }
      g_object_set (mux, "min-cluster-duration", min_cluster_duration, NULL);
      gst_object_unref (mux);
    } else {
       g_warning ("matroskamux was not found, can't set cluster duration\n");
    }
  }
}

static gboolean
app_sink_propose_allocation (GstAppSink *appsink, GstQuery *query, gpointer user_data)
{
  GstCaps *caps;
  GstBufferPool *pool = NULL;
  gboolean need_pool;
  guint size = 0;

  gst_query_parse_allocation (query, &caps, &need_pool);
  /* Amphion vpu encoder driver need at least 6 buffers */
  gst_query_add_allocation_pool (query, pool, size, 6, 0);
  GST_DEBUG ("appsink_propose_allocation\n");

  return TRUE;
}

static GstFlowReturn
app_sink_new_samples (GstAppSink * elt, gpointer user_data)
{
  GstSample *sample;

  sample = gst_app_sink_pull_sample (GST_APP_SINK (elt));
  gst_sample_unref (sample);

  return GST_FLOW_OK;
}

static REresult
setup_pipeline (gRecorderEngine *recorder)
{
  REresult ret = RE_RESULT_SUCCESS;
  gboolean res = TRUE;
  GstBus *bus;
  GstElement *sink = NULL, *ipp = NULL, *audio_src = NULL;
  GstEncodingProfile *prof = NULL;
  GstElement *camerasrc = NULL;

  recorder->initial_time = gst_util_get_timestamp ();

  recorder->camerabin = gst_element_factory_make ("camerabin", NULL);
  if (NULL == recorder->camerabin) {
    g_warning ("can't create camerabin element\n");
    goto error;
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (recorder->camerabin));
  /* Add sync handler for time critical messages that need to be handled fast */
  gst_bus_set_sync_handler (bus, sync_bus_callback, recorder, NULL);
  /* Handle normal messages asynchronously */
  gst_bus_add_watch (bus, bus_callback, recorder);
  gst_object_unref (bus);

  GST_INFO_OBJECT (recorder->camerabin, "camerabin created");

  if (recorder->camerabin_flags)
    gst_util_set_object_arg (G_OBJECT (recorder->camerabin), "flags", recorder->camerabin_flags);
  else
    gst_util_set_object_arg (G_OBJECT (recorder->camerabin), "flags", "");

  if (recorder->videosrc_name) {
    GstElement *wrapper;
    GstElement *videosrc_filter;
    GstElement *actual_video_source;
    gchar *video_filter_str = NULL;

    if (recorder->wrappersrc_name)
      wrapper = gst_element_factory_make (recorder->wrappersrc_name, NULL);
    else
      wrapper = gst_element_factory_make ("wrappercamerabinsrc", NULL);

    camerasrc = gst_parse_bin_from_description (recorder->videosrc_name, TRUE, NULL);

    if (g_strcmp0(recorder->videosrc_name, "videotestsrc") == 0) {
      GValue item = G_VALUE_INIT;
      GstIterator *it = gst_bin_iterate_sources ((GstBin*)camerasrc);
      if (gst_iterator_next (it, &item) != GST_ITERATOR_OK)
      {
        g_warning("%s(): gst_iterator_next failed\n", __FUNCTION__);
        gst_iterator_free (it);
        return RE_RESULT_INTERNAL_ERROR;
      }
      actual_video_source = g_value_get_object (&item);
      g_value_unset (&item);
      gst_iterator_free (it);
      g_object_set (actual_video_source, "is-live", TRUE, NULL);
    }

    if (g_strcmp0(recorder->videosrc_name, "v4l2src") == 0
      || g_strcmp0(recorder->videosrc_name, "imxv4l2src") == 0) {
      GValue item = G_VALUE_INIT;
      GstIterator *it = gst_bin_iterate_sources ((GstBin*)camerasrc);
      if (gst_iterator_next (it, &item) != GST_ITERATOR_OK)
      {
        g_warning("%s(): gst_iterator_next failed\n", __FUNCTION__);
        gst_iterator_free (it);
        return RE_RESULT_INTERNAL_ERROR;
      }
      recorder->video_src = g_value_get_object (&item);
      g_value_unset (&item);
      gst_iterator_free (it);

      if (recorder->video_src && recorder->videodevice_name )
        g_object_set (recorder->video_src, "device", recorder->videodevice_name, NULL);
    }

    if (g_strcmp0(recorder->videosrc_name, "pipewiresrc") == 0) {
      GValue item = G_VALUE_INIT;
      GstIterator *it = gst_bin_iterate_sources ((GstBin*)camerasrc);
      if (gst_iterator_next (it, &item) != GST_ITERATOR_OK)
      {
        g_warning("%s(): gst_iterator_next failed\n", __FUNCTION__);
        gst_iterator_free (it);
        return RE_RESULT_INTERNAL_ERROR;
      }
      recorder->video_src = g_value_get_object (&item);
      g_value_unset (&item);
      gst_iterator_free (it);

      if (recorder->video_src && recorder->videodevice_name ) {
        g_object_set (recorder->video_src, "path", recorder->videodevice_name, NULL);
        g_object_set (recorder->video_src, "provide-clock", FALSE, NULL);

        if (recorder->record_screen){
          g_object_set (recorder->video_src, "keepalive-time", 50, NULL);
        }
      }
    }

    g_object_set (wrapper, "video-source", camerasrc, NULL);
    g_object_set (wrapper, "post-previews", FALSE, NULL);

    if (recorder->camera_output_caps) {
      video_filter_str = g_strdup_printf ("%s\"%s\"", "capsfilter caps=",
          gst_caps_to_string (recorder->camera_output_caps));
    }

    /* Amphion VPU encoder supports NV12 and i.MX952 supports NV12/I420/NV21,
     * add i.MX video converter in camera source */
    if (recorder->record_screen && (imx_soc_in_group ("amphion") || imx_soc_is_chip ("MX952"))) {
      gchar *temp = NULL;
      if (video_filter_str) {
        temp = g_strdup_printf ("%s ! %s", video_filter_str, "imxvideoconvert_g2d");
        g_free (video_filter_str);
        video_filter_str = temp;
      } else {
        video_filter_str = g_strdup_printf ("%s", "imxvideoconvert_g2d");
      }
    }

    if (recorder->date_time) {
      gchar *temp = NULL;
      if (video_filter_str) {
        temp = g_strdup_printf ("%s ! %s", video_filter_str, recorder->date_time);
        g_free (video_filter_str);
        video_filter_str = temp;
      } else {
        video_filter_str = g_strdup_printf ("%s", recorder->date_time);
      }
    }
    if (recorder->video_effect_name) {
      gchar *temp = NULL;
      if (video_filter_str) {
        temp = g_strdup_printf ("%s ! %s ! capsfilter caps=\"%s\" ! %s ! capsfilter caps=\"%s\"",
            video_filter_str, recorder->video_effect_name, "video/x-raw, format=(string)RGBA",
            "queue ! imxvideoconvert_ipu", "video/x-raw, format=(string)NV12");
        g_free (video_filter_str);
        video_filter_str = temp;
      } else {
        video_filter_str = g_strdup_printf ("%s ! capsfilter caps=\"%s\" ! %s ! capsfilter caps=\"%s\"",
            recorder->video_effect_name, "video/x-raw, format=(string)RGBA",
            "queue ! imxvideoconvert_ipu", "video/x-raw, format=(string)NV12");
      }
    }
    if (video_filter_str) {
      GST_INFO_OBJECT (recorder->camerabin, "video filter string: %s", video_filter_str);
      videosrc_filter = gst_parse_bin_from_description (
          video_filter_str, TRUE, NULL);
      g_object_set (wrapper, "video-source-filter", videosrc_filter, NULL);
      g_object_unref (videosrc_filter);
      g_free (video_filter_str);
    }

    g_object_set (recorder->camerabin, "camera-source", wrapper, NULL);
    g_object_unref (wrapper);
  }

  if (recorder->video_detect_name) {
    recorder->viewfinder_filter = recorder->video_detect_name;
  }
  GST_INFO_OBJECT (recorder->camerabin, "view finder filter string: %s",
      recorder->viewfinder_filter);

  if (recorder->record_screen && ((imx_soc_in_group ("amphion") || imx_soc_is_chip ("MX952"))))
    recorder->vfsink_name = "appsink";
  else if (recorder->disable_viewfinder || recorder->record_screen)
    recorder->vfsink_name = "fakesink";
  else
    if (recorder->video_detect_name)
      recorder->vfsink_name = "imxv4l2sink";
    else if (imx_soc_is_chip ("MX6Q"))
      recorder->vfsink_name = "overlaysink";
    else
      recorder->vfsink_name = "autovideosink";

  /* Configure audio filter if needed */
  if (recorder->audio_output_caps) {
    gchar *filter_desc = NULL;
    GstElement *audio_filter = NULL;

    filter_desc = g_strdup_printf ("%s\"%s\"", "capsfilter caps=",
        gst_caps_to_string (recorder->audio_output_caps));
    GST_INFO_OBJECT (recorder->camerabin, "audio filter description: %s", filter_desc);

    audio_filter = gst_parse_bin_from_description (filter_desc, TRUE, NULL);
    if (audio_filter) {
      g_object_set (recorder->camerabin, "audio-filter", audio_filter, NULL);
      g_object_unref (audio_filter);
    }
    g_free (filter_desc);
  }

  /* configure used elements */
  res &=
      setup_pipeline_element (recorder->camerabin, "audio-source", recorder->audiosrc_name, &audio_src);

  if (audio_src && g_strcmp0(recorder->audiosrc_name, "pipewiresrc") == 0) {
    g_object_set (audio_src, "path", recorder->audiodevice_name, NULL);
    g_object_set (audio_src, "provide-clock", FALSE, NULL);
    g_object_set (audio_src, "use-bufferpool", FALSE, NULL);
  }

  res &=
      setup_pipeline_element (recorder->camerabin, "viewfinder-sink", recorder->vfsink_name, &sink);
  if (recorder->record_screen && (imx_soc_in_group ("amphion") || imx_soc_is_chip ("MX952"))) {
    GstAppSinkCallbacks callbacks = { 0, };

    g_object_set (sink, "sync", FALSE, "enable-last-sample", FALSE, NULL);
    callbacks.propose_allocation = app_sink_propose_allocation;
    callbacks.new_sample = app_sink_new_samples;
    gst_app_sink_set_callbacks ((GstAppSink *) sink, &callbacks, NULL, NULL);

    /* Amphion VPU encoder supports NV12 format and i.MX952 supports NV12/I420/NV21,
     * add caps filter to guarantee that camera source can output NV12 format */
    recorder->viewfinder_caps_str = g_strdup_printf ("video/x-raw(memory:DMABuf), "
      "format=DMA_DRM, drm-format=NV12");
  }

  res &=
      setup_pipeline_element_bin (recorder->camerabin, "viewfinder-filter", 
          recorder->viewfinder_filter, NULL);

  if ((recorder->max_files && recorder->max_file_size) || recorder->max_file_duration) {
    if (recorder->container_format != RE_OUTPUT_FORMAT_TS) {
      g_print ("file splitting only supported for TS container.");
      return RE_RESULT_PARAMETER_INVALID;
    }

    res &=
      setup_pipeline_element (recorder->camerabin, "video-sink", "multifilesink",
          NULL);
    g_object_get (recorder->camerabin, "video-sink", &recorder->video_sink, NULL);
    /* next-file=3: KEY_UNIT_EVENT - split on downstream force-key-unit event.
     * The probe in setup_file_split_probe() drives the actual split timing. */
    g_object_set (recorder->video_sink, "next-file", 3, NULL);
    if (recorder->max_files)
      g_object_set (recorder->video_sink, "max-files", recorder->max_files, NULL);
    g_object_set (recorder->video_sink, "async", FALSE, NULL);
  } else if (recorder->host) {
    if (recorder->container_format != RE_OUTPUT_FORMAT_TS) {
      g_print ("web camera only supported for TS container.");
      return RE_RESULT_PARAMETER_INVALID;
    }

    gchar *video_sink_str = g_strdup_printf ("%s%s", "rtpmp2tpay ! udpsink async=false sync=false host=", (char *)recorder->host);
    res &=
      setup_pipeline_element_bin (recorder->camerabin, "video-sink", 
          video_sink_str, NULL);
    g_free (video_sink_str);
  } else {
    if (recorder->container_format == RE_OUTPUT_FORMAT_FMP4
        || recorder->container_format == RE_OUTPUT_FORMAT_MKV) {
      res &=
        setup_pipeline_element (recorder->camerabin, "video-sink", "filesink",
            NULL);
      g_object_get (recorder->camerabin, "video-sink", &recorder->video_sink, NULL);
      /* Enabling synchronous IO to prevent data from not being
       * written to the disk in time due to power failure
       */
      g_object_set (recorder->video_sink, "o-sync", TRUE, NULL);
    }
  }

  if (recorder->imagepp_name) {
    ipp = create_ipp_bin (recorder);
    if (ipp) {
      g_object_set (recorder->camerabin, "image-filter", ipp, NULL);
      g_object_unref (ipp);
    }
    else
      GST_WARNING ("Could not create ipp elements");
  }

  prof = load_encoding_profile (recorder);
  if (prof) {
    g_object_set (G_OBJECT (recorder->camerabin), "video-profile", prof, NULL);
    gst_encoding_profile_unref (prof);
  }

  GST_INFO_OBJECT (recorder->camerabin, "elements created");

  if (sink) {
    g_object_set (sink, "sync", FALSE, NULL);
  } else {
    /* Get the inner viewfinder sink, this uses fixed names given
     * by default in camerabin */
    sink = gst_bin_get_by_name (GST_BIN (recorder->camerabin), "vf-bin");
    g_assert (sink);
    gst_object_unref (sink);

    sink = gst_bin_get_by_name (GST_BIN (sink), "vfbin-sink");
    g_assert (sink);
    gst_object_unref (sink);
  }
  recorder->viewfinder_sink = sink;

  GST_INFO_OBJECT (recorder->camerabin, "elements configured");

  /* configure a resolution and framerate */
/*
  if (recorder->image_width > 0 && recorder->image_height > 0) {
    if (recorder->mode == MODE_VIDEO) {
      GstCaps *caps = NULL;
      if (recorder->view_framerate_num > 0)
        caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
                "width", G_TYPE_INT, recorder->image_width,
                "height", G_TYPE_INT, recorder->image_height,
                "framerate", GST_TYPE_FRACTION, recorder->view_framerate_num,
                recorder->view_framerate_den, NULL), NULL);
      else
        caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
                "width", G_TYPE_INT, recorder->image_width,
                "height", G_TYPE_INT, recorder->image_height, NULL), NULL);

      GST_INFO_OBJECT (recorder->camerabin, "video-capture-caps is %", 
          GST_PTR_FORMAT, caps);
      g_object_set (recorder->camerabin, "viewfinder-caps", caps, NULL);
      g_object_set (recorder->camerabin, "video-capture-caps", caps, NULL);
      gst_caps_unref (caps);
    } else {
      GstCaps *caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
              "width", G_TYPE_INT, recorder->image_width,
              "height", G_TYPE_INT, recorder->image_height, NULL), NULL);

      g_object_set (recorder->camerabin, "viewfinder-caps", caps, NULL);
      g_object_set (recorder->camerabin, "image-capture-caps", caps, NULL);
      gst_caps_unref (caps);
    }
  }
  */

  set_camerabin_caps_from_string (recorder);

  /* change to the wrong mode if timestamping if performance mode is on so
   * we can change it back and measure the time after in playing */
  if (recorder->performance_measure) {
    g_object_set (recorder->camerabin, "mode",
        recorder->mode == MODE_VIDEO ? MODE_IMAGE : MODE_VIDEO, NULL);
  }

  /* handle imxcamera's preivewwidget window id */
  if (GST_IS_VIDEO_OVERLAY (recorder->viewfinder_sink)) {
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(recorder->viewfinder_sink),
        recorder->window);
  } else {
    GST_WARNING ("view finder sink isn't video overlay");
  }

  if (GST_STATE_CHANGE_FAILURE ==
      gst_element_set_state (recorder->camerabin, GST_STATE_READY)) {
    g_warning ("can't set camerabin to ready\n");
    goto error;
  }
  GST_INFO_OBJECT (recorder->camerabin, "camera ready");
  set_muxer_property (recorder);

  /* Install autodetect probes on muxer sink pads now that the muxer
   * element exists (pipeline is in READY). The probes identify the video
   * pad on the first real buffer (streaming thread), where caps are valid.
   * Only needed when multifilesink is used with size/duration splitting. */
  if (recorder->video_sink && (recorder->max_file_size || recorder->max_file_duration))
    setup_file_split_probe (recorder);

  if (GST_STATE_CHANGE_FAILURE ==
      gst_element_set_state (recorder->camerabin, GST_STATE_PLAYING)) {
    g_warning ("can't set camerabin to playing\n");
    goto error;
  }

  if (g_strcmp0(recorder->videosrc_name, "autovideosrc") == 0 && camerasrc) {
    GstElement *autosrc;
    GValue item = G_VALUE_INIT;
    /* get autovideosrc from bin */
    GstIterator *it = gst_bin_iterate_sources ((GstBin*)camerasrc);
    if (gst_iterator_next (it, &item) != GST_ITERATOR_OK)
    {
      g_warning("%s(): gst_iterator_next failed\n", __FUNCTION__);
      gst_iterator_free (it);
      return RE_RESULT_INTERNAL_ERROR;
    }
    autosrc = g_value_get_object (&item);
    g_value_unset (&item);
    gst_iterator_free (it);

    /* get actual video src from autovideosrc */
    it = gst_bin_iterate_sources ((GstBin*)autosrc);
    if (gst_iterator_next (it, &item) != GST_ITERATOR_OK)
    {
      g_warning("%s(): gst_iterator_next failed\n", __FUNCTION__);
      gst_iterator_free (it);
      return RE_RESULT_INTERNAL_ERROR;
    }
    recorder->video_src = g_value_get_object (&item);
    g_value_unset (&item);
    gst_iterator_free (it);
    GST_INFO_OBJECT (recorder->camerabin, "camera source element is %s",
        gst_element_get_name (recorder->video_src));

    if (recorder->video_src && recorder->videodevice_name) {
      g_print ("autovideosrc don't support set camera id\n");
    }
  }
  if (camerasrc) {
    g_object_unref (camerasrc);
  }

  GST_INFO_OBJECT (recorder->camerabin, "camera started");

  /* do the mode change timestamping if performance mode is on */
  if (recorder->performance_measure) {
    recorder->change_mode_before = gst_util_get_timestamp ();
    g_object_set (recorder->camerabin, "mode", recorder->mode, NULL);
    recorder->change_mode_after = gst_util_get_timestamp ();
  }

  return ret;
error:
  ret = RE_RESULT_INTERNAL_ERROR;
  cleanup_pipeline (recorder);
  return ret;
}

static void
stop_capture_cb (GObject * self, GParamSpec * pspec, gpointer user_data)
{
  gRecorderEngine *recorder = (gRecorderEngine *)user_data;
  gboolean idle = FALSE;

  GST_DEBUG ("stop_capture_cb");
  g_object_get (recorder->camerabin, "idle", &idle, NULL);

  if (idle) {
    if (recorder->capture_count < recorder->capture_total) {
      run_pipeline (recorder);
    } else {
      recorder->stop_done = TRUE;
      if(recorder->app_callback != NULL) {
        (*(recorder->app_callback))(recorder->pAppData, RE_EVENT_MAX_FILE_COUNT_REACHED, 0);
      }
    }
  }

  g_signal_handler_disconnect (recorder->camerabin, recorder->stop_capture_cb_id);
}

static gboolean
stop_capture (gRecorderEngine *recorder)
{
  recorder->stop_capture_cb_id = g_signal_connect (recorder->camerabin, "notify::idle",
      (GCallback) stop_capture_cb, recorder);
  g_signal_emit_by_name (recorder->camerabin, "stop-capture", 0);

  return FALSE;
}

static void
set_metadata (GstElement * camera)
{
  GstTagSetter *setter = GST_TAG_SETTER (camera);
  GstDateTime *datetime;
  gchar *desc_str;

  datetime = gst_date_time_new_now_local_time ();

  desc_str = g_strdup_printf ("captured by %s", g_get_real_name ());

  gst_tag_setter_add_tags (setter, GST_TAG_MERGE_REPLACE,
      GST_TAG_DATE_TIME, datetime,
      GST_TAG_DESCRIPTION, desc_str,
      GST_TAG_TITLE, "grecorder capture",
      GST_TAG_GEO_LOCATION_LONGITUDE, 1.0,
      GST_TAG_GEO_LOCATION_LATITUDE, 2.0,
      GST_TAG_GEO_LOCATION_ELEVATION, 3.0,
      GST_TAG_DEVICE_MANUFACTURER, "grecorder manufacturer",
      GST_TAG_DEVICE_MODEL, "grecorder model", NULL);

  g_free (desc_str);
  gst_date_time_unref (datetime);
}

static REresult 
run_pipeline (gRecorderEngine *recorder)
{
  REresult ret = RE_RESULT_SUCCESS;
  GstCaps *preview_caps = NULL;
  GstElement *video_source = NULL;
  CaptureTiming *timing;

  g_object_set (recorder->camerabin, "mode", recorder->mode, NULL);

  if (recorder->preview_caps_name != NULL) {
    preview_caps = gst_caps_from_string (recorder->preview_caps_name);
    if (preview_caps) {
      g_object_set (recorder->camerabin, "preview-caps", preview_caps, NULL);
      GST_DEBUG ("Preview caps set");
    } else
      GST_DEBUG ("Preview caps set but could not create caps from string");
  }

  set_metadata (recorder->camerabin);

  GST_DEBUG ("Setting filename: %s", (char *) recorder->filename);

  if (recorder->mode == MODE_VIDEO) {
    if (recorder->video_sink) {
      if(!g_strcmp0 (GST_OBJECT_NAME (gst_element_get_factory (recorder->video_sink)), "filesink")) {
        g_object_set (recorder->video_sink, "location", recorder->filename, NULL);
      } else {
        const gchar *filename_suffix;
        gchar *filename_str;
        filename_suffix = strrchr((char *) recorder->filename, '.');
        filename_str =
          g_strdup_printf ("%s%s%s", (char *) recorder->filename, "%05d", filename_suffix);
        GST_DEBUG ("Setting filename: %s", filename_str);
        g_object_set (recorder->video_sink, "location", filename_str, NULL);
        g_free (filename_str);
      }
    } else if (recorder->host) {
      GST_DEBUG ("web camera host: %s", (char *) recorder->host);
    } else
      g_object_set (recorder->camerabin, "location", recorder->filename, NULL);
  } else
    g_object_set (recorder->camerabin, "location", recorder->filename, NULL);

  g_object_get (recorder->camerabin, "camera-source", &video_source, NULL);
  if (video_source) {
#if 0
    if (GST_IS_ELEMENT (video_source) && GST_IS_PHOTOGRAPHY (video_source)) {
      /* Set GstPhotography interface options. If option not given as
         command-line parameter use default of the source element. */
      if (recorder->scene_mode != SCENE_MODE_NONE)
        g_object_set (video_source, "scene-mode", recorder->scene_mode, NULL);
      if (recorder->ev_compensation != EV_COMPENSATION_NONE)
        g_object_set (video_source, "ev-compensation", recorder->ev_compensation, NULL);
      if (recorder->aperture != APERTURE_NONE)
        g_object_set (video_source, "aperture", recorder->aperture, NULL);
      if (recorder->flash_mode != FLASH_MODE_NONE)
        g_object_set (video_source, "flash-mode", recorder->flash_mode, NULL);
      if (recorder->exposure != EXPOSURE_NONE)
        g_object_set (video_source, "exposure", recorder->exposure, NULL);
      if (recorder->iso_speed != ISO_SPEED_NONE)
        g_object_set (video_source, "iso-speed", recorder->iso_speed, NULL);
      if (recorder->wb_mode != WHITE_BALANCE_MODE_NONE)
        g_object_set (video_source, "white-balance-mode", recorder->wb_mode, NULL);
      if (recorder->color_mode != COLOR_TONE_MODE_NONE)
        g_object_set (video_source, "colour-tone-mode", recorder->color_mode, NULL);
    }
#endif
  } else {
    video_source = gst_bin_get_by_name (GST_BIN (recorder->camerabin), "camerasrc");
  }
  g_object_set (recorder->camerabin, "zoom", recorder->zoom / 100.0f, NULL);

  recorder->capture_count++;

  timing = g_slice_new0 (CaptureTiming);
  recorder->capture_times = g_list_prepend (recorder->capture_times, timing);

  /* set pad probe to check when buffer leaves the camera source */
  if (recorder->mode == MODE_IMAGE) {
    GstPad *pad;

    pad = gst_element_get_static_pad (video_source, "imgsrc");
    recorder->camera_probe_id = gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
        camera_src_get_timestamp_probe, recorder, NULL);

    gst_object_unref (pad);
  }
  gst_object_unref (video_source);

  timing->start_capture = gst_util_get_timestamp ();
  g_signal_emit_by_name (recorder->camerabin, "start-capture", 0);

  if (recorder->mode == MODE_VIDEO && recorder->capture_time) {
    g_timeout_add ((recorder->capture_time * 1000), (GSourceFunc) stop_capture, recorder);
  }

  return ret;
}

static void
parse_target_values (gRecorderEngine *recorder)
{
  gdouble startup = 0, change_mode = 0, shot_to_save = 0, shot_to_snapshot = 0;
  gdouble shot_to_shot = 0, preview_to_precapture = 0, shot_to_buffer = 0;

  if (recorder->performance_targets_str == NULL)
    return;

  /*
     startup time, change mode time, shot to save, shot to snapshot,
     shot to shot, preview to precapture, shot to buffer.
   */
  sscanf (recorder->performance_targets_str, "%lf,%lf,%lf,%lf,%lf,%lf,%lf",
      &startup, &change_mode, &shot_to_save,
      &shot_to_snapshot, &shot_to_shot, &preview_to_precapture,
      &shot_to_buffer);

  recorder->target_startup = (GstClockTime) (startup * GST_SECOND);
  recorder->target_change_mode = (GstClockTime) (change_mode * GST_SECOND);
  recorder->target_shot_to_save = (GstClockTime) (shot_to_save * GST_SECOND);
  recorder->target_shot_to_snapshot = (GstClockTime) (shot_to_snapshot * GST_SECOND);
  recorder->target_shot_to_shot = (GstClockTime) (shot_to_shot * GST_SECOND);
  recorder->target_preview_to_precapture =
      (GstClockTime) (preview_to_precapture * GST_SECOND);
  recorder->target_shot_to_buffer = (GstClockTime) (shot_to_buffer * GST_SECOND);
}

static void
print_performance_data (gRecorderEngine *recorder)
{
  GList *iter;
  gint i = 0;
  GstClockTime last_start = 0;
  CaptureTimingStats max;
  CaptureTimingStats min;
  CaptureTimingStats avg;
  CaptureTimingStats avg_wo_first;
  GstClockTime shot_to_shot;

  if (!recorder->performance_measure)
    return;

  parse_target_values (recorder);

  /* Initialize stats */
  min.shot_to_shot = -1;
  min.shot_to_save = -1;
  min.shot_to_snapshot = -1;
  min.preview_to_precapture = -1;
  min.shot_to_buffer = -1;
  memset (&avg, 0, sizeof (CaptureTimingStats));
  memset (&avg_wo_first, 0, sizeof (CaptureTimingStats));
  memset (&max, 0, sizeof (CaptureTimingStats));

  g_print ("-- Performance results --\n");
  g_print ("Startup time: %" TIME_FORMAT "; Target: %" TIME_FORMAT "\n",
      TIME_ARGS (recorder->startup_time - recorder->initial_time), TIME_ARGS (recorder->target_startup));
  g_print ("Change mode time: %" TIME_FORMAT "; Target: %" TIME_FORMAT "\n",
      TIME_ARGS (recorder->change_mode_after - recorder->change_mode_before),
      TIME_ARGS (recorder->target_change_mode));

  g_print
      ("\n   | Shot to save |Shot to snapshot| Shot to shot |"
      "Preview to precap| Shot to buffer\n");
  recorder->capture_times = g_list_reverse (recorder->capture_times);
  for (iter = recorder->capture_times; iter; iter = g_list_next (iter)) {
    CaptureTiming *t = (CaptureTiming *) iter->data;
    CaptureTimingStats stats;

    stats.shot_to_save = SHOT_TO_SAVE (t);
    stats.shot_to_snapshot = SHOT_TO_SNAPSHOT (t);
    stats.shot_to_shot = i == 0 ? 0 : t->start_capture - last_start;
    stats.preview_to_precapture = PREVIEW_TO_PRECAPTURE (t);
    stats.shot_to_buffer = SHOT_TO_BUFFER (t);

    PRINT_STATS (i, &stats);

    if (i != 0) {
      capture_timing_stats_add (&avg_wo_first, &stats);
    }
    capture_timing_stats_add (&avg, &stats);

    if (stats.shot_to_save < min.shot_to_save) {
      min.shot_to_save = stats.shot_to_save;
    }
    if (stats.shot_to_snapshot < min.shot_to_snapshot) {
      min.shot_to_snapshot = stats.shot_to_snapshot;
    }
    if (stats.shot_to_shot < min.shot_to_shot && stats.shot_to_shot > 0) {
      min.shot_to_shot = stats.shot_to_shot;
    }
    if (stats.preview_to_precapture < min.preview_to_precapture) {
      min.preview_to_precapture = stats.preview_to_precapture;
    }
    if (stats.shot_to_buffer < min.shot_to_buffer) {
      min.shot_to_buffer = stats.shot_to_buffer;
    }


    if (stats.shot_to_save > max.shot_to_save) {
      max.shot_to_save = stats.shot_to_save;
    }
    if (stats.shot_to_snapshot > max.shot_to_snapshot) {
      max.shot_to_snapshot = stats.shot_to_snapshot;
    }
    if (stats.shot_to_shot > max.shot_to_shot) {
      max.shot_to_shot = stats.shot_to_shot;
    }
    if (stats.preview_to_precapture > max.preview_to_precapture) {
      max.preview_to_precapture = stats.preview_to_precapture;
    }
    if (stats.shot_to_buffer > max.shot_to_buffer) {
      max.shot_to_buffer = stats.shot_to_buffer;
    }

    last_start = t->start_capture;
    i++;
  }

  if (i > 1)
    shot_to_shot = avg.shot_to_shot / (i - 1);
  else
    shot_to_shot = GST_CLOCK_TIME_NONE;
  capture_timing_stats_div (&avg, i);
  avg.shot_to_shot = shot_to_shot;
  if (i > 1)
    capture_timing_stats_div (&avg_wo_first, i - 1);
  else {
    memset (&avg_wo_first, 0, sizeof (CaptureTimingStats));
  }

  g_print ("\n    Stats             |     MIN      |     MAX      |"
      "     AVG      | AVG wo First |   Target     | Diff \n");
  g_print ("Shot to shot          | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIMEDIFF_FORMAT "\n",
      TIME_ARGS (min.shot_to_shot), TIME_ARGS (max.shot_to_shot),
      TIME_ARGS (avg.shot_to_shot),
      TIME_ARGS (avg_wo_first.shot_to_shot),
      TIME_ARGS (recorder->target_shot_to_shot),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_shot, recorder->target_shot_to_shot)));
  g_print ("Shot to save          | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIMEDIFF_FORMAT "\n",
      TIME_ARGS (min.shot_to_save), TIME_ARGS (max.shot_to_save),
      TIME_ARGS (avg.shot_to_save),
      TIME_ARGS (avg_wo_first.shot_to_save),
      TIME_ARGS (recorder->target_shot_to_save),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_save, recorder->target_shot_to_save)));
  g_print ("Shot to snapshot      | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT
      " | %" TIMEDIFF_FORMAT "\n",
      TIME_ARGS (min.shot_to_snapshot),
      TIME_ARGS (max.shot_to_snapshot),
      TIME_ARGS (avg.shot_to_snapshot),
      TIME_ARGS (avg_wo_first.shot_to_snapshot),
      TIME_ARGS (recorder->target_shot_to_snapshot),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_snapshot,
              recorder->target_shot_to_snapshot)));
  g_print ("Preview to precapture | %" TIME_FORMAT " | %" TIME_FORMAT " | %"
      TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIMEDIFF_FORMAT
      "\n", TIME_ARGS (min.preview_to_precapture),
      TIME_ARGS (max.preview_to_precapture),
      TIME_ARGS (avg.preview_to_precapture),
      TIME_ARGS (avg_wo_first.preview_to_precapture),
      TIME_ARGS (recorder->target_preview_to_precapture),
      TIMEDIFF_ARGS (TIME_DIFF (avg.preview_to_precapture,
              recorder->target_preview_to_precapture)));
  g_print ("Shot to buffer        | %" TIME_FORMAT " | %" TIME_FORMAT " | %"
      TIME_FORMAT " | %" TIME_FORMAT " | %" TIME_FORMAT " | %" TIMEDIFF_FORMAT
      "\n", TIME_ARGS (min.shot_to_buffer), TIME_ARGS (max.shot_to_buffer),
      TIME_ARGS (avg.shot_to_buffer), TIME_ARGS (avg_wo_first.shot_to_buffer),
      TIME_ARGS (recorder->target_shot_to_buffer),
      TIMEDIFF_ARGS (TIME_DIFF (avg.shot_to_buffer, recorder->target_shot_to_buffer)));
}

static REchar * key_value_pair (REuint32 key, KeyMap * kKeyMap, REuint32 KeyMapSize)
{
  gint j;
  gint kNumMapEntries = KeyMapSize / sizeof(kKeyMap[0]);

  for (j = 0; j < kNumMapEntries; j ++) {
    if (key == kKeyMap[j].nKey) {
      return kKeyMap[j].tag;
    }
  }

  return NULL;
}
 
static REresult set_audio_source(RecorderEngineHandle handle, REuint32 as)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  CHECK_PARAM (as, RE_AUDIO_SOURCE_LIST_END);

  static KeyMap kKeyMap[] = {
    { RE_AUDIO_SOURCE_DEFAULT, (REchar *)"pulsesrc" },
    { RE_AUDIO_SOURCE_MIC, (REchar *)"pulsesrc" },
    { RE_AUDIO_SOURCE_TEST, (REchar *)"audiotestsrc" },
    { RE_AUDIO_SOURCE_PIPEWIRE, (REchar *)"pipewiresrc" },
  };

  recorder->audiosrc_name = key_value_pair (as, kKeyMap, sizeof(kKeyMap));

  return RE_RESULT_SUCCESS;
}

static REresult set_audio_id(RecorderEngineHandle handle, REuint32 audioId)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  if (recorder->audiodevice_name) {
    g_free (recorder->audiodevice_name);
    recorder->audiodevice_name = NULL;
  }

  if (strcmp (recorder->audiosrc_name, "pipewiresrc") == 0)
    recorder->audiodevice_name = g_strdup_printf ("%d", audioId);

  GST_INFO ("chose audio device %s", recorder->audiodevice_name);

  return RE_RESULT_SUCCESS;
}

static REresult get_audio_supported_sample_rate(RecorderEngineHandle handle, REuint32 index, REuint32 *sampleRate)
{
  return RE_RESULT_SUCCESS;
}

static REresult set_audio_sample_rate(RecorderEngineHandle handle, REuint32 sampleRate)
{
  return RE_RESULT_SUCCESS;
}

static REresult get_audio_supported_channel(RecorderEngineHandle handle, REuint32 index, REuint32 *channels)
{
  return RE_RESULT_SUCCESS;
}

static REresult set_audio_channel(RecorderEngineHandle handle, REuint32 channels)
{
  return RE_RESULT_SUCCESS;
}

static REresult set_audio_output_settings(RecorderEngineHandle handle, RERawAudioSettings *audioProperty)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  gchar *format = NULL;
  int sample_rate = 0;
  int channels = 0;

  if (recorder->audio_output_caps) {
    gst_caps_unref (recorder->audio_output_caps);
    recorder->audio_output_caps = NULL;
  }

  sample_rate = audioProperty->sampleRate;
  channels = audioProperty->channels;
  if (audioProperty->sampleFormat == 8) {
    format = "S8";
  } else if (audioProperty->sampleFormat == 16) {
    format = "S16LE";
  } else if (audioProperty->sampleFormat == 24) {
    format = "S24LE";
  } else if (audioProperty->sampleFormat == 32) {
    format = "S32LE";
  }

  /* Set default parameters for screen recording */
  if (recorder->record_screen) {
    if (!format) {
      format = "S16LE";
    }
    if (!sample_rate) {
      sample_rate = 48000;
    }
    if (!channels) {
      channels = 2;
    }
  }

  if (!format || !sample_rate || !channels) {
    return RE_RESULT_SUCCESS;
  }

  recorder->audio_output_caps = gst_caps_new_full (
      gst_structure_new ("audio/x-raw",
      "format", G_TYPE_STRING, format,
      "rate", G_TYPE_INT, sample_rate,
      "channels", G_TYPE_INT, channels, NULL),
      NULL);

  return RE_RESULT_SUCCESS;
}

static REresult set_video_source(RecorderEngineHandle handle, REuint32 vs)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  CHECK_PARAM (vs, RE_VIDEO_SOURCE_LIST_END);

  static KeyMap kKeyMap[] = {
    { RE_VIDEO_SOURCE_DEFAULT, (REchar *)"autovideosrc" },
    { RE_VIDEO_SOURCE_CAMERA, (REchar *)"v4l2src" },
    { RE_VIDEO_SOURCE_IMXCAMERA, (REchar *)"imxv4l2src" },
    { RE_VIDEO_SOURCE_TEST, (REchar *)"videotestsrc" },
    { RE_VIDEO_SOURCE_SCREEN, (REchar *)"ximagesrc ! queue ! imxcompositor_ipu" },
    { RE_VIDEO_SOURCE_PIPEWIRE, (REchar *)"pipewiresrc" },
  };

  recorder->videosrc_name = key_value_pair (vs, kKeyMap, sizeof(kKeyMap));

  return RE_RESULT_SUCCESS;
}

static REresult set_camera_id(RecorderEngineHandle handle, REuint32 cameraId)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  if (recorder->videodevice_name) {
    g_free (recorder->videodevice_name);
    recorder->videodevice_name = NULL;
  }

  if (strcmp (recorder->videosrc_name, "pipewiresrc") == 0)
    recorder->videodevice_name = g_strdup_printf ("%d", cameraId);
  else
    recorder->videodevice_name = g_strdup_printf ("/dev/video%d", cameraId);

  GST_INFO ("chose video device %s", recorder->videodevice_name);

  return RE_RESULT_SUCCESS;
}

static REresult record_screen (RecorderEngineHandle handle, REboolean bRecordScreen)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  recorder->record_screen = bRecordScreen;
  if (recorder->record_screen) {
    if (imx_soc_has_feature ("g2d-dpuv1") || imx_soc_has_feature ("g2d-dpuv2")) {
      recorder->imagepp_name = "imxvideoconvert_g2d";
    } else if (imx_soc_is_chip ("MX8MP") || imx_soc_is_chip ("MX8MM")){
      g_print ("Can not support snapshot function\n");
    } else {
      g_print ("Can not support screen record function\n");
      return RE_RESULT_PARAMETER_INVALID;
    }
  }

  return RE_RESULT_SUCCESS;
}

static REresult get_camera_capabilities(RecorderEngineHandle handle, REuint32 index)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  if (!recorder->camera_caps) {
    GstElement *wrapper;
    GstElement *videosrc;
    if (!recorder->video_src) {
      g_object_get (recorder->camerabin, "camera-source", &wrapper, NULL);
      g_object_get (wrapper, "video-source", &videosrc, NULL);
    } else {
      videosrc = recorder->video_src;
    }
    if (videosrc) {
      GstPad *pad;
      const gchar *padname;

      padname = "src";

      pad = gst_element_get_static_pad (videosrc, padname);

      g_assert (pad != NULL);

      recorder->camera_caps = gst_pad_query_caps (pad, NULL);
      //recorder->camera_caps = gst_pad_get_allowed_caps (pad);
      if (recorder->camera_caps == NULL) {
        GST_ERROR ("get video source caps fail.");
        gst_object_unref (pad);
        return RE_RESULT_INTERNAL_ERROR;
      }

      gst_object_unref (pad);
    } else {
      GST_DEBUG_OBJECT (recorder_engine, "Source not created, can't get "
          "supported caps");
    }

    GST_DEBUG ("video-source-supported-caps: %" GST_PTR_FORMAT, recorder->camera_caps);
  }

  if (recorder->camera_caps) {
    GstCaps * tmp;

    GST_DEBUG ("index: %d caps size: %d\n", index, gst_caps_get_size (recorder->camera_caps));
    if (index >= gst_caps_get_size (recorder->camera_caps)) {
      return RE_RESULT_NO_MORE;
    }

    tmp = gst_caps_copy_nth (recorder->camera_caps, index);
    g_print ("%s\n", gst_caps_to_string (tmp));
    gst_caps_unref (tmp);
  } else {
    return RE_RESULT_NO_MORE;
  }

  return RE_RESULT_SUCCESS;
}

static REresult set_camera_output_settings(RecorderEngineHandle handle, RERawVideoSettings *videoProperty)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  CHECK_PARAM (videoProperty->videoFormat, RE_COLORFORMAT_LIST_END);
  gchar *video_format_name;

  recorder->image_format = videoProperty->videoFormat;
  recorder->image_width = videoProperty->width;
  recorder->image_height = videoProperty->height;

  static KeyMap kKeyMap[] = {
    { RE_COLORFORMAT_DEFAULT, (REchar *)"I420" },
    { RE_COLORFORMAT_YUV420PLANAR, (REchar *)"I420" },
    { RE_COLORFORMAT_YUV420SEMIPLANAR, (REchar *)"NV12" },
    { RE_COLORFORMAT_YUVY, (REchar *)"YUY2" },
    { RE_COLORFORMAT_UYVY, (REchar *)"UYVY" },
    { RE_COLORFORMAT_BGRA, (REchar *)"BGRA" },
    { RE_COLORFORMAT_BGRx, (REchar *)"BGRx" },
    { RE_COLORFORMAT_RGB16, (REchar *)"RGB16" },
  };

  video_format_name = key_value_pair (recorder->image_format, kKeyMap, sizeof(kKeyMap));

  //FIXME: work around for USB camera 15/2 fps.
  if (videoProperty->framesPerSecond == 7) {
    recorder->view_framerate_num = 15;
    recorder->view_framerate_den = 2;
  } else {
    recorder->view_framerate_num = videoProperty->framesPerSecond;
    recorder->view_framerate_den = 1;
  }

  if (recorder->image_width > 0 && recorder->image_height > 0) {
    if (recorder->camera_output_caps) {
      gst_caps_unref (recorder->camera_output_caps);
      recorder->camera_output_caps = NULL;
    }

    /* pipewiresrc applies DMA DRM caps when recording screen */
    if (g_strcmp0(recorder->videosrc_name, "pipewiresrc") == 0 &&
        recorder->record_screen) {
      GstVideoFormat fmt;
      guint64 modifier;
      guint32 drm_fourcc;
      gchar *drm_fmt_str;

      fmt = gst_video_format_from_string (video_format_name);
      drm_fourcc = gst_video_dma_drm_format_from_gst_format (fmt, &modifier);
      if (drm_fourcc == DRM_FORMAT_INVALID)
        goto done;

      drm_fmt_str = gst_video_dma_drm_fourcc_to_string (drm_fourcc, modifier);
      recorder->camera_output_caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
          "format", G_TYPE_STRING, "DMA_DRM",
          "drm-format", G_TYPE_STRING, drm_fmt_str,
          "width", G_TYPE_INT, recorder->image_width,
          "height", G_TYPE_INT, recorder->image_height,
          NULL), NULL);
      GstCapsFeatures *f = gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_DMABUF, NULL);
      gst_caps_set_features(recorder->camera_output_caps, 0, f);
      goto done;
    }

    if (recorder->mode == MODE_VIDEO) {
      if (recorder->view_framerate_num > 0)
        recorder->camera_output_caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
                "width", G_TYPE_INT, recorder->image_width,
                "height", G_TYPE_INT, recorder->image_height,
                "format", G_TYPE_STRING, video_format_name,
                "framerate", GST_TYPE_FRACTION, recorder->view_framerate_num,
                recorder->view_framerate_den, NULL), NULL);
      else
        recorder->camera_output_caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
                "width", G_TYPE_INT, recorder->image_width,
                "height", G_TYPE_INT, recorder->image_height,
                "format", G_TYPE_STRING, video_format_name,
                NULL), NULL);

      GST_INFO_OBJECT (recorder->camerabin, "camera output caps is %"
          GST_PTR_FORMAT, recorder->camera_output_caps);
    } else {
      recorder->camera_output_caps = gst_caps_new_full (gst_structure_new ("video/x-raw",
              "width", G_TYPE_INT, recorder->image_width,
              "height", G_TYPE_INT, recorder->image_height,
              "format", G_TYPE_STRING, video_format_name,
              NULL), NULL);
    }
  }
done:
  return RE_RESULT_SUCCESS;
}

static REresult disable_viewfinder (RecorderEngineHandle handle, REboolean bDisableViewfinder)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  recorder->disable_viewfinder = bDisableViewfinder;

  return RE_RESULT_SUCCESS;
}

static REresult set_preview_region(RecorderEngineHandle handle, REVideoRect *rect)
{
  return RE_RESULT_SUCCESS;
}

static REresult set_preview_win_id(RecorderEngineHandle handle, void *wid)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  recorder->window = (guintptr)wid;

  return RE_RESULT_SUCCESS;
}

static REresult need_preview_buffer(RecorderEngineHandle handle, REboolean bNeedPreviewBuffer)
{
  return RE_RESULT_SUCCESS;
}

static REresult get_preview_buffer_format(RecorderEngineHandle handle, RERawVideoSettings *videoProperty)
{
  return RE_RESULT_SUCCESS;
}

static REresult add_time_stamp(RecorderEngineHandle handle, REboolean bAddTimeStamp)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  if (bAddTimeStamp) {
      /* INFO: cannot use HAS_G2D, encoder on 8qm/qxp/imx952 only support NV12 */
      if (imx_soc_in_group ("g2d") && !imx_soc_in_group ("imx-legacy")
          && !imx_soc_in_group ("amphion") && !imx_soc_is_chip ("MX952")) {
          recorder->date_time = DATE_TIME TIME_OVERLAY HW_COMPOSITOR "queue";
      }
      else if (imx_soc_in_group ("amphion") || imx_soc_is_chip ("MX952")) {
          recorder->date_time = DATE_TIME TIME_OVERLAY "queue";
      }
      else if (imx_soc_is_chip ("MX6Q")){
          recorder->date_time = DATE_TIME TIME_OVERLAY "queue ! imxvideoconvert_ipu composition-meta-enable=true in-place=true ! queue";
      } else {
          recorder->date_time = NULL;
      }
  } else {
    recorder->date_time = NULL;
  }

  return RE_RESULT_SUCCESS;
}

static REresult add_video_effect(RecorderEngineHandle handle, REuint32 videoEffect)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  CHECK_PARAM (videoEffect, RE_VIDEO_EFFECT_LIST_END);

  if (!imx_soc_is_chip ("MX6Q")) {
      g_print("***Video effect is not supported!\n");
      return RE_RESULT_FEATURE_UNSUPPORTED;
  }

/* check gstreamer version, pipeline is different in 1.4.5 and 1.6.0 */
/* gray shader effect has been removed in GST-1.8.0 */
#if GST_CHECK_VERSION(1, 8, 0)
  static KeyMap kKeyMap[] = {
    { RE_VIDEO_EFFECT_DEFAULT, NULL },
    { RE_VIDEO_EFFECT_CUBE, (REchar *)"glupload ! glfiltercube ! gldownload" },
    { RE_VIDEO_EFFECT_MIRROR, (REchar *)"glupload ! gleffects effect=1 ! gldownload" },
    { RE_VIDEO_EFFECT_SQUEEZE, (REchar *)"glupload ! gleffects effect=2 ! gldownload" },
    { RE_VIDEO_EFFECT_FISHEYE, (REchar *)"glupload ! gleffects effect=5 ! gldownload" },
    { RE_VIDEO_EFFECT_TUNNEL, (REchar *)"glupload ! gleffects effect=4 ! gldownload" },
    { RE_VIDEO_EFFECT_TWIRL, (REchar *)"glupload ! gleffects effect=6 ! gldownload" },
  };
#elif GST_CHECK_VERSION(1, 6, 0)
  static KeyMap kKeyMap[] = {
    { RE_VIDEO_EFFECT_DEFAULT, NULL },
    { RE_VIDEO_EFFECT_CUBE, (REchar *)"glupload ! glfiltercube ! gldownload" },
    { RE_VIDEO_EFFECT_MIRROR, (REchar *)"glupload ! gleffects effect=1 ! gldownload" },
    { RE_VIDEO_EFFECT_SQUEEZE, (REchar *)"glupload ! gleffects effect=2 ! gldownload" },
    { RE_VIDEO_EFFECT_FISHEYE, (REchar *)"glupload ! gleffects effect=5 ! gldownload" },
    { RE_VIDEO_EFFECT_GRAY, (REchar *)"glupload ! glshader location=/usr/share/gray_shader.fs ! gldownload" }, 
    { RE_VIDEO_EFFECT_TUNNEL, (REchar *)"glupload ! gleffects effect=4 ! gldownload" },
    { RE_VIDEO_EFFECT_TWIRL, (REchar *)"glupload ! gleffects effect=6 ! gldownload" },
  };
#else
  static KeyMap kKeyMap[] = {
    { RE_VIDEO_EFFECT_DEFAULT, NULL },
    { RE_VIDEO_EFFECT_CUBE, (REchar *)"glfiltercube" },
    { RE_VIDEO_EFFECT_MIRROR, (REchar *)"gleffects effect=1" },
    { RE_VIDEO_EFFECT_SQUEEZE, (REchar *)"gleffects effect=2" },
    { RE_VIDEO_EFFECT_FISHEYE, (REchar *)"glshader location=/usr/share/fisheye_shader.fs" },
    { RE_VIDEO_EFFECT_GRAY, (REchar *)"glshader location=/usr/share/gray_shader.fs" },
    { RE_VIDEO_EFFECT_TUNNEL, (REchar *)"glshader location=/usr/share/tunnel_shader.fs" },
    { RE_VIDEO_EFFECT_TWIRL, (REchar *)"glshader location=/usr/share/twirl_shader.fs" },
  };
#endif

  recorder->video_effect_name = key_value_pair (videoEffect, kKeyMap, sizeof(kKeyMap));

  return RE_RESULT_SUCCESS;
}

static REresult add_video_detect(RecorderEngineHandle handle, REuint32 videoDetect)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  CHECK_PARAM (videoDetect, RE_VIDEO_DETECT_LIST_END);

  if (imx_soc_in_group ("imx8") || imx_soc_in_group ("imx9")) {
      g_print("***Video detect is not supported!\n");
      return RE_RESULT_FEATURE_UNSUPPORTED;
  }

  static KeyMap kKeyMap[] = {
    { RE_VIDEO_DETECT_DEFAULT, NULL },
    { RE_VIDEO_DETECT_FACEDETECT, (REchar *)"imxvideoconvert_ipu ! queue ! video/x-raw,width=176,height=144 ! queue ! facedetect profile=/usr/share/gst1.0-fsl-plugins/1.0/opencv_haarcascades/haarcascade_frontalface_old_format.xml display=true scale-factor=2 min-size-width=32 min-size-height=32 updates=2 min-neighbors=3 ! queue" },
    { RE_VIDEO_DETECT_FACEBLUR, (REchar *)"imxvideoconvert_ipu ! queue ! video/x-raw,width=176,height=144 ! queue ! faceblur profile=/usr/share/gst1.0-fsl-plugins/1.0/opencv_haarcascades/haarcascade_frontalface_old_format.xml ! queue" },
  };

  recorder->video_detect_name = key_value_pair (videoDetect, kKeyMap, sizeof(kKeyMap));

  return RE_RESULT_SUCCESS;
}

static REresult set_audio_encoder_settings(RecorderEngineHandle handle, REAudioEncoderSettings *audioEncoderSettings)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  CHECK_PARAM (audioEncoderSettings->encoderType, RE_AUDIO_ENCODER_LIST_END);

  GST_DEBUG ("set audio encoder format: %d", audioEncoderSettings->encoderType);
  recorder->audio_encoder_format = audioEncoderSettings->encoderType;

  return RE_RESULT_SUCCESS;
}

static REresult set_video_encoder_settings(RecorderEngineHandle handle, REVideoEncoderSettings *videoEncoderSettings)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  CHECK_PARAM (videoEncoderSettings->encoderType, RE_VIDEO_ENCODER_LIST_END);

  GST_DEBUG ("set video encoder format: %d", videoEncoderSettings->encoderType);
  recorder->video_encoder_format = videoEncoderSettings->encoderType;

  return RE_RESULT_SUCCESS;
}

static REresult set_container_format (RecorderEngineHandle handle, REuint32 of)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  CHECK_PARAM (of, RE_OUTPUT_FORMAT_LIST_END);

  GST_DEBUG ("set container format: %d", of);
  recorder->container_format = of;

  return RE_RESULT_SUCCESS;
}

static REresult set_fragment_duration(RecorderEngineHandle handle, REmillisecond timeMs)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  recorder->fragment_duration = timeMs;
  return RE_RESULT_SUCCESS;
}

static REresult set_output_file_path (RecorderEngineHandle handle, const REchar *path)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  if (recorder->filename) {
    g_free (recorder->filename);
    recorder->filename = NULL;
  }

  recorder->filename = (GString *) g_strdup (path);

  return RE_RESULT_SUCCESS;
}

static REresult set_rtp_host (RecorderEngineHandle handle, const REchar *host, REuint32 port)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  if (recorder->host) {
    g_free (recorder->host);
    recorder->host = NULL;
  }

  recorder->host = (GString *) g_strdup (host);
  recorder->port = port;

  return RE_RESULT_SUCCESS;
}

static REresult set_file_count(RecorderEngineHandle handle, REuint32 fileCount)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  recorder->max_files = fileCount;

  return RE_RESULT_SUCCESS;
}

static REresult set_max_file_duration(RecorderEngineHandle handle, REtime timeUs)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  recorder->max_file_duration = timeUs;
  return RE_RESULT_SUCCESS;
}

static REresult set_max_file_size_bytes (RecorderEngineHandle handle, REuint64 bytes)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  recorder->max_file_size = bytes;

  return RE_RESULT_SUCCESS;
}

static REresult set_output_file_settings (RecorderEngineHandle handle, REOutputFileSettings *outputFileSettings)
{
  return RE_RESULT_SUCCESS;
}

static REresult set_snapshot_output_format(RecorderEngineHandle handle, REuint32 of)
{
  return RE_RESULT_SUCCESS;
}

static REresult set_snapshot_output_file(RecorderEngineHandle handle, const REchar *path)
{
  return RE_RESULT_SUCCESS;
}

static REresult set_ext_ctrls (RecorderEngineHandle handle, const REchar *ext_ctrls)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  GstStructure * ctrls;

  if (!imx_soc_is_chip ("MX8MP")) {
    g_printf ("only platform with isp support camera extra control\n");
    return RE_RESULT_SUCCESS;
  }

  gchar *str =
        g_strdup_printf ("ext_ctrls, viv_ext_ctrl=\"%s\"", ext_ctrls);

  ctrls = gst_structure_new_from_string (str);

  if (recorder->video_src &&
      g_object_class_find_property (G_OBJECT_GET_CLASS (recorder->video_src),
            "extra-controls"))
    g_object_set (recorder->video_src, "extra-controls", ctrls, NULL);

  gst_structure_free (ctrls);
  g_free (str);

  return RE_RESULT_SUCCESS;
}

static REresult take_snapshot(RecorderEngineHandle handle)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  REresult ret = RE_RESULT_SUCCESS;
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  g_mutex_lock (&recorder->lock);

  if (recorder->state != RE_STATE_PREPARED) {
    GST_ERROR ("wrong state.");
    g_mutex_unlock (&recorder->lock);
    return RE_RESULT_WRONG_STATE;
  }

  recorder->mode = MODE_IMAGE;
  recorder->stop_done = FALSE;
  ret = run_pipeline (recorder);
  if (ret != RE_RESULT_SUCCESS) {
    GST_ERROR ("run_pipeline fail.");
    g_mutex_unlock (&recorder->lock);
    return ret;
  }

  //FIXME: can't received image done event.
  //while (recorder->stop_done == FALSE) {
    GST_DEBUG ("wait until received stop done event.");
    g_usleep (G_USEC_PER_SEC / 10);
  //}

  print_performance_data (recorder);

  g_mutex_unlock (&recorder->lock);

  return ret;
}

static REresult init(RecorderEngineHandle handle)
{
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  recorder->camerabin = NULL;
  recorder->viewfinder_sink = NULL;
  recorder->video_sink = NULL;
  recorder->video_src = NULL;
  recorder->camera_probe_id = 0;
  recorder->viewfinder_probe_id = 0;
  recorder->loop = NULL;

  recorder->videosrc_name = NULL;
  recorder->videodevice_name = NULL;
  recorder->audiosrc_name = NULL;
  recorder->audiodevice_name = NULL;
  recorder->wrappersrc_name = NULL;
  recorder->imagepp_name = NULL;
  recorder->vfsink_name = NULL;
  recorder->video_effect_name = NULL;
  recorder->video_detect_name = NULL;
  recorder->date_time = NULL;
  recorder->image_width = 0;
  recorder->image_height = 0;
  recorder->view_framerate_num = 0;
  recorder->view_framerate_den = 0;
  recorder->no_xwindow = FALSE;
  recorder->disable_viewfinder = FALSE;
  recorder->gep_targetname = NULL;
  recorder->gep_profilename = NULL;
  recorder->gep_filename = NULL;
  recorder->image_capture_caps_str = NULL;
  recorder->viewfinder_caps_str = NULL;
  recorder->video_capture_caps_str = NULL;
  recorder->audio_capture_caps_str = NULL;
  recorder->performance_measure = FALSE;
  recorder->performance_targets_str = NULL;
  recorder->camerabin_flags = "0x0000000f";

  recorder->mode = MODE_VIDEO;
  recorder->zoom = 100;

  recorder->capture_time = 0;
  recorder->capture_count = 0;
  recorder->capture_total = 1;
  recorder->stop_capture_cb_id = 0;

  recorder->ev_compensation = EV_COMPENSATION_NONE;
  recorder->aperture = APERTURE_NONE;
  recorder->flash_mode = FLASH_MODE_NONE;
  recorder->scene_mode = SCENE_MODE_NONE;
  recorder->exposure = EXPOSURE_NONE;
  recorder->iso_speed = ISO_SPEED_NONE;
  recorder->wb_mode = WHITE_BALANCE_MODE_NONE;
  recorder->color_mode = COLOR_TONE_MODE_NONE;

  recorder->viewfinder_filter = NULL;

  recorder->x_width = 320;
  recorder->x_height = 240;

  recorder->filename = NULL;
  recorder->host = NULL;
  recorder->port = 0;
  recorder->max_files = 0;
  recorder->max_file_size = 0;
  recorder->max_file_duration = 0;
  recorder->current_file_bytes = 0;
  recorder->current_file_start_ts = GST_CLOCK_TIME_NONE;
  recorder->waiting_for_keyframe = FALSE;

  recorder->preview_caps_name = NULL;

  recorder->window = 0;

  recorder->initial_time = 0;
  recorder->startup_time = 0;
  recorder->change_mode_before = 0;
  recorder->change_mode_after = 0;
  recorder->capture_times = NULL;

  // recorder->target_startup;
  // recorder->target_change_mode;
  // recorder->target_shot_to_shot;
  // recorder->target_shot_to_save;
  // recorder->target_shot_to_snapshot;
  // recorder->target_preview_to_precapture;
  // recorder->target_shot_to_buffer;
  recorder->camera_caps = NULL;
  recorder->camera_output_caps = NULL;
  recorder->audio_output_caps = NULL;

  g_mutex_init (&recorder->lock);
  g_mutex_init (&recorder->split_lock);

  recorder->state = RE_STATE_INITED;
  recorder->container_format = RE_OUTPUT_FORMAT_DEFAULT;
  recorder->video_encoder_format = RE_VIDEO_ENCODER_DEFAULT;
  recorder->audio_encoder_format = RE_AUDIO_ENCODER_DEFAULT;

  return RE_RESULT_SUCCESS;
}

static REresult register_event_handler(RecorderEngineHandle handle,
    void * context, RecorderEngineEventHandler handler)
{
  REresult ret = RE_RESULT_SUCCESS;
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  recorder->pAppData = context;
  recorder->app_callback = handler;

  return ret;
}

static REresult prepare(RecorderEngineHandle handle)
{
  REresult ret = RE_RESULT_SUCCESS;
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  g_mutex_lock (&recorder->lock);

  if (recorder->state != RE_STATE_INITED) {
    GST_ERROR ("wrong state.");
    g_mutex_unlock (&recorder->lock);
    return RE_RESULT_WRONG_STATE;
  }

  ret = setup_pipeline (recorder);
  if (ret != RE_RESULT_SUCCESS) {
    GST_ERROR ("setup_pipeline fail.");
    g_mutex_unlock (&recorder->lock);
    return ret;
  }

  recorder->state = RE_STATE_PREPARED;
  g_mutex_unlock (&recorder->lock);

  return ret;
}

static REresult start(RecorderEngineHandle handle)
{
  REresult ret = RE_RESULT_SUCCESS;
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  g_mutex_lock (&recorder->lock);

  if (recorder->state != RE_STATE_PREPARED) {
    GST_ERROR ("wrong state.");
    g_mutex_unlock (&recorder->lock);
    return RE_RESULT_WRONG_STATE;
  }

  if (recorder->image_width > 1920 || recorder->image_height > 1080) {
    GST_ERROR ("invalid parameter for video recording.");
    g_mutex_unlock (&recorder->lock);
    return RE_RESULT_PARAMETER_INVALID;
  }

  recorder->mode = MODE_VIDEO;
  ret = run_pipeline (recorder);
  if (ret != RE_RESULT_SUCCESS) {
    GST_ERROR ("run_pipeline fail.");
    g_mutex_unlock (&recorder->lock);
    return ret;
  }

  recorder->state = RE_STATE_RUNNING;
  recorder->base_media_timeUs = 0;
  get_media_time (handle, &recorder->base_media_timeUs);

  g_mutex_unlock (&recorder->lock);
  
  return ret;
}

static REresult pause(RecorderEngineHandle handle)
{
  return RE_RESULT_SUCCESS;
}

static REresult resume(RecorderEngineHandle handle)
{
  return RE_RESULT_SUCCESS;
}

static REresult stop(RecorderEngineHandle handle)
{
  REresult ret = RE_RESULT_SUCCESS;
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
 
  g_mutex_lock (&recorder->lock);

  if (recorder->state != RE_STATE_RUNNING) {
    GST_WARNING ("wrong state.");
    g_mutex_unlock (&recorder->lock);
    return RE_RESULT_WRONG_STATE;
  }

  GST_DEBUG ("stopping.");
  recorder->stop_done = FALSE;
  ret = stop_capture (recorder);
  if (ret != RE_RESULT_SUCCESS) {
    GST_ERROR ("stop_pipeline fail.");
    g_mutex_unlock (&recorder->lock);
    return ret;
  }

  while (recorder->stop_done == FALSE) {
    GST_DEBUG ("wait until received stop done event.");
    g_usleep (G_USEC_PER_SEC / 10);
  }

  print_performance_data (recorder);

  recorder->state = RE_STATE_PREPARED;
  g_mutex_unlock (&recorder->lock);
  
  return ret;
}

static REresult close(RecorderEngineHandle handle)
{
  REresult ret = RE_RESULT_SUCCESS;
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
 
  stop (handle);

  g_mutex_lock (&recorder->lock);

  GST_DEBUG ("closing.");
  cleanup_pipeline (recorder);
  if (ret != RE_RESULT_SUCCESS) {
    GST_ERROR ("cleanup_pipeline fail.");
    g_mutex_unlock (&recorder->lock);
    return ret;
  }

  recorder->state = RE_STATE_INITED;
  g_mutex_unlock (&recorder->lock);
 
  return ret;
}

static REresult reset(RecorderEngineHandle handle)
{
  REresult ret = RE_RESULT_SUCCESS;
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
 
  ret = close (handle);
  if (ret != RE_RESULT_SUCCESS) {
    GST_ERROR ("close fail.");
    return ret;
  }

  if (recorder->camera_caps) {
    gst_caps_unref (recorder->camera_caps);
    recorder->camera_caps = NULL;
  }

  return prepare (handle);
}

static REresult get_max_amplitude(RecorderEngineHandle handle, REuint32 *max)
{
  return RE_RESULT_SUCCESS;
}

static REresult get_media_time(RecorderEngineHandle handle, REtime *pMediaTimeUs)
{
  REresult ret = RE_RESULT_SUCCESS;
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);
  gint64 cur = GST_CLOCK_TIME_NONE;

  if (recorder->state != RE_STATE_RUNNING) {
    *pMediaTimeUs = 0;
    return ret;
  }

  if (!gst_element_query_position(recorder->camerabin, GST_FORMAT_TIME, &cur)) {
    GST_ERROR ("query position fail");
    *pMediaTimeUs = 0;
    return ret;
  }

  GST_DEBUG ("current media time: %" G_GINT64_FORMAT, cur);
  *pMediaTimeUs = cur/1000 - recorder->base_media_timeUs;

  return ret;
}

static REresult delete_it(RecorderEngineHandle handle)
{
  REresult ret = RE_RESULT_SUCCESS;
  RecorderEngine *h = (RecorderEngine *)(handle);
  gRecorderEngine *recorder = (gRecorderEngine *)(h->pData);

  close (handle);

  if (recorder->camera_caps) {
    gst_caps_unref (recorder->camera_caps);
    recorder->camera_caps = NULL;
  }
  if (recorder->camera_output_caps) {
    gst_caps_unref (recorder->camera_output_caps);
    recorder->camera_output_caps = NULL;
  }
  if (recorder->filename) {
    g_free (recorder->filename);
    recorder->filename = NULL;
  }
  if (recorder->host) {
    g_free (recorder->host);
    recorder->host = NULL;
  }

  if (recorder->videodevice_name) {
    g_free (recorder->videodevice_name);
    recorder->videodevice_name = NULL;
  }

  if (recorder->audio_output_caps) {
    gst_caps_unref (recorder->audio_output_caps);
    recorder->audio_output_caps = NULL;
  }

  if (recorder->audiodevice_name) {
    g_free (recorder->audiodevice_name);
    recorder->audiodevice_name = NULL;
  }

  if (recorder->viewfinder_caps_str) {
    g_free (recorder->viewfinder_caps_str);
    recorder->viewfinder_caps_str = NULL;
  }

  g_mutex_clear (&recorder->split_lock);
  g_mutex_clear (&recorder->lock);
  g_slice_free (gRecorderEngine, recorder);
  g_slice_free (RecorderEngine, h);
  gst_deinit();

  return ret;
}

RecorderEngine * recorder_engine_create()
{
  RecorderEngine *h;
  gRecorderEngine *recorder;

  gst_init(NULL, NULL);
  GST_DEBUG_CATEGORY_INIT (recorder_engine, "recorder-engine", 0,
      "recorder engine");

  h = g_slice_new0 (RecorderEngine);
  if (!h) {
    GST_ERROR ("allocate memory error.");
    return NULL;
  }

  recorder = g_slice_new0 (gRecorderEngine);
  if (!recorder) {
    GST_ERROR ("allocate memory error.");
    g_slice_free (RecorderEngine, h);
    return NULL;
  }

  h->set_audio_source = set_audio_source;
  h->set_audio_id = set_audio_id;
  h->get_audio_supported_sample_rate = get_audio_supported_sample_rate;
  h->set_audio_sample_rate = set_audio_sample_rate;
  h->get_audio_supported_channel = get_audio_supported_channel;
  h->set_audio_channel = set_audio_channel;
  h->set_audio_output_settings = set_audio_output_settings;
  h->set_video_source = set_video_source;
  h->set_camera_id = set_camera_id;
  h->record_screen = record_screen;
  h->get_camera_capabilities = get_camera_capabilities;
  h->set_camera_output_settings = set_camera_output_settings;
  h->disable_viewfinder = disable_viewfinder;
  h->set_preview_region = set_preview_region;
  h->set_preview_win_id = set_preview_win_id;
  h->need_preview_buffer = need_preview_buffer;
  h->get_preview_buffer_format = get_preview_buffer_format;
  h->add_time_stamp = add_time_stamp;
  h->add_video_effect = add_video_effect;
  h->add_video_detect = add_video_detect;
  h->set_audio_encoder_settings = set_audio_encoder_settings;
  h->set_video_encoder_settings = set_video_encoder_settings;
  h->set_container_format = set_container_format;
  h->set_fragment_duration = set_fragment_duration;
  h->set_output_file_path = set_output_file_path;
  h->set_rtp_host = set_rtp_host;
  h->set_file_count = set_file_count;
  h->set_max_file_duration = set_max_file_duration;
  h->set_max_file_size_bytes = set_max_file_size_bytes;
  h->set_output_file_settings = set_output_file_settings;
  h->set_snapshot_output_format = set_snapshot_output_format;
  h->set_snapshot_output_file = set_snapshot_output_file;
  h->set_ext_ctrls = set_ext_ctrls;
  h->take_snapshot = take_snapshot;
  h->init = init;
  h->register_event_handler = register_event_handler;
  h->prepare = prepare;
  h->start = start;
  h->pause = pause;
  h->resume = resume;
  h->stop = stop;
  h->close = close;
  h->reset = reset;
  h->get_max_amplitude = get_max_amplitude;
  h->get_media_time = get_media_time;
  h->delete_it = delete_it;
  h->pData = recorder;

  return h;
}
