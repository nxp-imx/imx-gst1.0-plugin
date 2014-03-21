/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. 

 */



/*
 * Module Name:    aiurdemux.c
 *
 * Description:    Implementation of unified parser gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
     
#include "aiurdemux.h"

#define GST_TAG_VIDEO_FRAMERATE "frame_rate"
#define GST_TAG_VIDEO_BITRATE "video_bitrate"
#define GST_TAG_VIDEO_WIDTH "image_width"
#define GST_TAG_VIDEO_HEIGHT "image_height"

GST_DEBUG_CATEGORY (aiurdemux_debug);

typedef struct{


  guint32 type;
  guint32 subtype;
  const gchar* codec_str;
  const gchar* mime;
}AiurdemuxCodecStruct;


static AiurdemuxCodecStruct aiurdemux_videocodec_tab[] ={
  {VIDEO_H263, 0, "H.263", "video/x-h263, variant=(string)itu"},
  {VIDEO_H264, 0, "H.264/AVC", "video/x-h264, parsed = (boolean)true, alignment=(string)au"},
  {VIDEO_MPEG2, 0, "MPEG2", "video/mpeg, systemstream = (boolean)false, parsed = (boolean)true, mpegversion=(int)2"},
  {VIDEO_MPEG4, 0, "MPEG4", "video/mpeg, systemstream = (boolean)false, parsed = (boolean)true, mpegversion=(int)4"},
  {VIDEO_JPEG, 0, "JPEG", "image/jpeg"},
  {VIDEO_MJPG, VIDEO_MJPEG_FORMAT_A, "Motion JPEG format A", "image/jpeg"},
  {VIDEO_MJPG, VIDEO_MJPEG_FORMAT_B, "Motion JPEG format B", "image/jpeg"},
  {VIDEO_MJPG, VIDEO_MJPEG_2000, "Motion JPEG 2000", "image/x-j2c"},
  {VIDEO_MJPG, 0, "Motion JPEG format unknow", "image/jpeg"},
  {VIDEO_DIVX, VIDEO_DIVX3, "Divx3", "video/x-divx, divxversion=(int)3"},
  {VIDEO_DIVX, VIDEO_DIVX4, "Divx4", "video/x-divx, divxversion=(int)4"},
  {VIDEO_DIVX, VIDEO_DIVX5_6, "Divx", "video/x-divx, divxversion=(int)5"},
  {VIDEO_DIVX, 0, "Divx", "video/x-divx, divxversion=(int)5"},
  {VIDEO_XVID, 0, "Xvid", "video/x-xvid"},
  {VIDEO_WMV, VIDEO_WMV7, "WMV7", "video/x-wmv, wmvversion=(int)1, format=(string)WMV1"},
  {VIDEO_WMV, VIDEO_WMV8, "WMV8", "video/x-wmv, wmvversion=(int)2, format=(string)WMV2"},
  {VIDEO_WMV, VIDEO_WMV9, "WMV9", "video/x-wmv, wmvversion=(int)3, format=(string)WMV3"},
  {VIDEO_WMV, VIDEO_WVC1, "VC1", "video/x-wmv, wmvversion=(int)3, format=(string)WVC1"},
  {VIDEO_WMV, 0, NULL, NULL},
  {VIDEO_REAL, 0, "RealVideo", "video/x-pn-realvideo"},
  {VIDEO_SORENSON_H263, 0, "Sorenson H.263", "video/x-flash-video"},
  {VIDEO_FLV_SCREEN, 0, "Flash Screen", "video/x-flash-video"},
  {VIDEO_ON2_VP, VIDEO_VP6A, "VP6 Alpha", "video/x-vp6-alpha"},
  {VIDEO_ON2_VP, VIDEO_VP6, "VP6 Flash", "video/x-vp6-flash"},
  {VIDEO_ON2_VP, VIDEO_VP8, "VP8", "video/x-vp8"},
  {VIDEO_ON2_VP, 0, NULL, NULL}
};

static GstStaticPadTemplate gst_aiurdemux_videosrc_template =
GST_STATIC_PAD_TEMPLATE ("video_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_aiurdemux_audiosrc_template =
GST_STATIC_PAD_TEMPLATE ("audio_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_aiurdemux_subsrc_template =
GST_STATIC_PAD_TEMPLATE ("subtitle_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstsutilsOptionEntry g_aiurdemux_option_table[] = {
  {PROP_PROGRAM_NUMBER, "program-number",
        "program-number",
        "Program number to demux for (-1 to ignore), valid only when multiprogram-enabled=false",
        G_TYPE_INT,
        G_STRUCT_OFFSET (AiurDemuxOption, program_number),
      "-1", "-1", G_MAXINT_STR},
  {PROP_MULTIPROGRAM_ENABLED, "multiprogram-enabled", "multiprogram enabled",
        "enable/disable multiprogram",
        G_TYPE_BOOLEAN, G_STRUCT_OFFSET (AiurDemuxOption, multiprogram_enabled),
      "false"},
  {PROP_PROGRAM_MASK, "program-mask", "program mask",
        "set program track bit mask (valid for first 32 programs, 0 for  enable all)",
        G_TYPE_UINT,
        G_STRUCT_OFFSET (AiurDemuxOption, program_mask),
      "0x0", "0", "0xffffffff"},
  {PROP_INTERLEAVE_QUEUE_SIZE, "interleave-queue-size", "interleave queue size",
        "set length of interleave queue in bytes for file read mode only",
        G_TYPE_UINT,
        G_STRUCT_OFFSET (AiurDemuxOption, interleave_queue_size),
      "10240000", "0", G_MAXUINT_STR},
  {PROP_STREAMING_LATENCY, "streaming_latency", "latency for streaming",
        "set the latency in ms seconds for streaming mode",
        G_TYPE_UINT,
        G_STRUCT_OFFSET (AiurDemuxOption, streaming_latency),
      "400", "0", G_MAXUINT_STR},
    {PROP_INDEX_ENABLED, "enable-index", "enable index file for clips",
            "enable/disable index file for clips",
            G_TYPE_BOOLEAN,
            G_STRUCT_OFFSET (AiurDemuxOption, index_enabled),
          "false"},
    {PROP_DISABLE_VORBIS_CODEC_DATA, "disable_vorbis_codec_data", "do not send vorbis codec data",
            "whether to parse vorbis codec data to three buffers to send",
            G_TYPE_BOOLEAN,
            G_STRUCT_OFFSET (AiurDemuxOption, disable_vorbis_codec_data),
          "true"},
  {-1, NULL, NULL, NULL, 0, 0, NULL}    /* terminator */
};

typedef struct
{
  gint core_tag;
  gint format;
  const gchar *gst_tag_name;
  const gchar *print_string;
} AiurDemuxTagEntry;
static AiurDemuxTagEntry g_user_data_entry[] = {
  {USER_DATA_TITLE, USER_DATA_FORMAT_UTF8, GST_TAG_TITLE,
      "Title : %s\n"},
  {USER_DATA_LANGUAGE, USER_DATA_FORMAT_UTF8, GST_TAG_LANGUAGE_CODE,
      "Langurage : %s\n"},
  {USER_DATA_GENRE, USER_DATA_FORMAT_UTF8, GST_TAG_GENRE,
      "Genre : %s\n"},
  {USER_DATA_ARTIST, USER_DATA_FORMAT_UTF8, GST_TAG_ARTIST,
      "Artist : %s\n"},
  {USER_DATA_COPYRIGHT, USER_DATA_FORMAT_UTF8, GST_TAG_COPYRIGHT,
      "Copy Right : %s\n"},
  {USER_DATA_COMMENTS, USER_DATA_FORMAT_UTF8, GST_TAG_COMMENT,
      "Comments : %s\n"},
  {USER_DATA_CREATION_DATE, USER_DATA_FORMAT_UTF8, GST_TAG_DATE,
      "Creation Date : %s\n"},
  {USER_DATA_ALBUM, USER_DATA_FORMAT_UTF8, GST_TAG_ALBUM,
      "Album  : %s\n"},
  {USER_DATA_VCODECNAME, USER_DATA_FORMAT_UTF8, GST_TAG_VIDEO_CODEC,
      "Video Codec Name : %s\n"},
  {USER_DATA_ACODECNAME, USER_DATA_FORMAT_UTF8, GST_TAG_AUDIO_CODEC,
      "Audio Codec Name : %s\n"},
  {USER_DATA_ARTWORK, USER_DATA_FORMAT_JPEG, GST_TAG_IMAGE,
      "Found Artwork : %" GST_PTR_FORMAT ", %d bytes\n"},
  {USER_DATA_COMPOSER, USER_DATA_FORMAT_UTF8, GST_TAG_COMPOSER,
      "Composer : %s\n"},
  //{USER_DATA_DIRECTOR,        USER_DATA_FORMAT_UTF8, ?,                       "Director : %s\n"}, /* tag is not defined */
  //{USER_DATA_INFORMATION,     USER_DATA_FORMAT_UTF8, ?,                       "Information : %s\n"}, /* tag is not defined */
  //{USER_DATA_CREATOR,         USER_DATA_FORMAT_UTF8, ?,                       "Creator : %s\n"}, /* tag is not defined */
  //{USER_DATA_PRODUCER,        USER_DATA_FORMAT_UTF8, ?,                       "Producer : %s\n"}, /* tag is not defined */
  {USER_DATA_PERFORMER, USER_DATA_FORMAT_UTF8, GST_TAG_PERFORMER,
      "Performer : %s\n"},
  //{USER_DATA_REQUIREMENTS,    USER_DATA_FORMAT_UTF8, ?,                       "Requirements : %s\n"}, /* tag is not defined */
  //{USER_DATA_SONGWRITER,      USER_DATA_FORMAT_UTF8, ?,                       "Song Writer : %s\n"}, /* tag is not defined */
  //{USER_DATA_MOVIEWRITER,     USER_DATA_FORMAT_UTF8, ?,                       "Movie Writer : %s\n"}, /* tag is not defined */
  {USER_DATA_TOOL, USER_DATA_FORMAT_UTF8, GST_TAG_ENCODER,
      "Writing Application : %s\n"},
  {USER_DATA_DESCRIPTION, USER_DATA_FORMAT_UTF8, GST_TAG_DESCRIPTION,
      "Description  : %s\n"},
  {USER_DATA_TRACKNUMBER, USER_DATA_FORMAT_UTF8, GST_TAG_TRACK_NUMBER,
      "Track Number : %s\n"},
  {USER_DATA_TOTALTRACKNUMBER, USER_DATA_FORMAT_UTF8, GST_TAG_TRACK_COUNT,
      "Track Count : %s\n"},

};


#define gst_aiurdemux_parent_class parent_class
G_DEFINE_TYPE (GstAiurDemux, gst_aiurdemux, GST_TYPE_ELEMENT);


static void gst_aiurdemux_finalize (GObject * object);
static GstStateChangeReturn gst_aiurdemux_change_state (GstElement * element,
    GstStateChange transition);

static GstFlowReturn gst_aiurdemux_chain (GstPad * sinkpad, GstObject * parent,
    GstBuffer * inbuf);

static gboolean gst_aiurdemux_handle_sink_event (GstPad * sinkpad, GstObject * parent,
    GstEvent * event);
static gboolean gst_aiurdemux_handle_src_event (GstPad * sinkpad, GstObject * parent,
    GstEvent * event);
static gboolean
gst_aiurdemux_handle_src_query (GstPad * pad, GstObject * parent, GstQuery * query);
static void
gst_aiurdemux_push_tags (GstAiurDemux * demux, AiurDemuxStream * stream);
static void
gst_aiurdemux_push_event (GstAiurDemux * demux, GstEvent * event);


static GstPadTemplate *gst_aiurdemux_sink_pad_template (void);

static gboolean aiurdemux_sink_activate (GstPad * sinkpad,GstObject * parent);

static gboolean aiurdemux_sink_activate_mode (GstPad * sinkpad,
    GstObject * parent, GstPadMode mode, gboolean active);
static gboolean aiurdemux_sink_activate_pull (GstPad * sinkpad, GstObject * parent,
    gboolean active);
static gboolean aiurdemux_sink_activate_push (GstPad * sinkpad, GstObject * parent,
    gboolean active);
gpointer aiurdemux_loop_push (gpointer * data);

static void aiurdemux_pull_task (GstPad * pad);
static void aiurdemux_push_task (GstAiurDemux * demux);

static gboolean gst_aiurdemux_setcaps (GstPad * pad, GstObject * parent, GstCaps * caps);

static GstFlowReturn aiurdemux_loop_state_probe (GstAiurDemux * demux);
static GstFlowReturn aiurdemux_loop_state_init (GstAiurDemux * demux);
static GstFlowReturn aiurdemux_loop_state_header (GstAiurDemux * demux);
static GstFlowReturn aiurdemux_loop_state_movie (GstAiurDemux * demux);


static gboolean aiurdemux_set_readmode (GstAiurDemux * demux);

static gboolean
aiurdemux_parse_programs (GstAiurDemux * demux);
static gboolean
aiurdemux_parse_pmt (GstAiurDemux * demux);

gboolean
aiurdemux_parse_program_info (GstAiurDemux * demux);

static void
aiurdemux_select_programs (GstAiurDemux * demux);




static GstTagList * aiurdemux_add_user_tags (GstAiurDemux * demux);

static int aiurdemux_parse_streams (GstAiurDemux * demux);
static void aiurdemux_parse_video (GstAiurDemux * demux, AiurDemuxStream * stream,
    gint track_index);
static void aiurdemux_parse_audio (GstAiurDemux * demux, AiurDemuxStream * stream,
    gint track_index);
static void aiurdemux_parse_text (GstAiurDemux * demux, AiurDemuxStream * stream,
    gint track_index);
static void aiurdemux_check_interleave_stream_eos (GstAiurDemux * demux);
static void
_gst_buffer_copy_into_mem (GstBuffer * dest, gsize offset, const guint8 * src,
    gsize size);

static GstFlowReturn aiurdemux_read_buffer(GstAiurDemux * demux, uint32* track_idx,
    AiurDemuxStream** stream_out);
static GstFlowReturn aiurdemux_parse_vorbis_codec_data(GstAiurDemux * demux, AiurDemuxStream* stream);

static gint aiurdemux_choose_next_stream (GstAiurDemux * demux);

static AiurDemuxStream * aiurdemux_trackidx_to_stream (GstAiurDemux * demux, guint32 stream_idx);
static void
aiurdemux_check_start_offset (GstAiurDemux * demux, AiurDemuxStream * stream);
static void
aiurdemux_adjust_timestamp (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstBuffer * buffer);
static void
aiurdemux_update_stream_position (GstAiurDemux * demux,
    AiurDemuxStream * stream, GstBuffer * buffer);
static void
aiurdemux_send_stream_newsegment (GstAiurDemux * demux,
    AiurDemuxStream * stream);
static GstFlowReturn
aiurdemux_send_stream_eos (GstAiurDemux * demux, AiurDemuxStream * stream);
static GstFlowReturn
aiurdemux_send_stream_eos_all (GstAiurDemux * demux);

static GstFlowReturn aiurdemux_push_pad_buffer (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstBuffer * buffer);
static GstFlowReturn
aiurdemux_combine_flows (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstFlowReturn ret);

static void aiurdemux_reset_stream (GstAiurDemux * demux, AiurDemuxStream * stream);


static gboolean
gst_aiurdemux_convert_seek (GstPad * pad, GstFormat * format,
    GstSeekType cur_type, gint64 * cur, GstSeekType stop_type, gint64 * stop);

static gboolean
gst_aiurdemux_perform_seek (GstAiurDemux * demux, GstSegment * segment,
    gint accurate);
static gboolean
aiurdemux_do_push_seek (GstAiurDemux * demux, GstPad * pad,
    GstEvent * event);
static gboolean
aiurdemux_do_seek (GstAiurDemux * demux, GstPad * pad, GstEvent * event);

static gboolean
gst_aiurdemux_get_duration (GstAiurDemux * demux, gint64 * duration);

static void
aiurdemux_send_pending_events (GstAiurDemux * demux);


static void aiurdemux_release_resource (GstAiurDemux * demux);
static GstFlowReturn gst_aiurdemux_close_core (GstAiurDemux * demux);





#define AIUR_MEDIATYPE2STR(media) \
    (((media)==MEDIA_VIDEO)?"video":(((media)==MEDIA_AUDIO)?"audio":"subtitle"))

#define AIUR_CORETS_2_GSTTS(ts) (((ts)==PARSER_UNKNOWN_TIME_STAMP)? GST_CLOCK_TIME_NONE : (ts*1000))
#define AIUR_GSTTS_2_CORETS(ts) ((ts)/1000)

#define AIUR_COREDURATION_2_GSTDURATION(ts) (((ts)==PARSER_UNKNOWN_DURATION)? 0 : (ts*1000))

#define AIUR_RESET_SAMPLE_STAT(stat)\
    do {\
        (stat).start = GST_CLOCK_TIME_NONE;\
        (stat).duration = 0;\
        (stat).flag = 0;\
    }while(0)

#define AIUR_UPDATE_SAMPLE_STAT(stat,timestamp, dura, sflag)\
    do {\
        if (((stat).start==GST_CLOCK_TIME_NONE) && \
            ((timestamp)!=GST_CLOCK_TIME_NONE))\
            (stat).start = (timestamp);\
        (stat).duration += dura;\
        (stat).flag |= sflag;\
    }while(0)

#define MARK_STREAM_EOS(demux, stream)\
          do {\
            (stream)->valid = FALSE;\
            (stream)->pending_eos = TRUE;\
            (demux)->pending_event = TRUE;\
          }while(0)




#define AIURDEMUX_FUNCTION_IMPLEMENTATION 1

static void gst_aiurdemux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAiurDemux *self = GST_AIURDEMUX (object);
  if (gstsutils_options_set_option (g_aiurdemux_option_table,
          (gchar *) & self->option, prop_id, value) == FALSE) {
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

}
static void gst_aiurdemux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAiurDemux *self = GST_AIURDEMUX (object);
  if (gstsutils_options_get_option (g_aiurdemux_option_table,
          (gchar *) & self->option, prop_id, value) == FALSE) {
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

}


static void gst_aiurdemux_class_init (GstAiurDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_aiurdemux_finalize;
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_aiurdemux_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_aiurdemux_get_property);

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_aiurdemux_change_state);

  gstsutils_options_install_properties_by_options (g_aiurdemux_option_table,
      gobject_class);

  gst_element_class_add_pad_template (gstelement_class,
      gst_aiurdemux_sink_pad_template ());
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_aiurdemux_videosrc_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_aiurdemux_audiosrc_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_aiurdemux_subsrc_template));
  
  gst_element_class_set_static_metadata (gstelement_class, "Aiur universal demuxer",
      "Codec/Demuxer",
      "demux container file to video, audio, and subtitle",
      "FreeScale Multimedia Team <shamm@freescale.com>");

  GST_DEBUG_CATEGORY_INIT (aiurdemux_debug, "aiurdemux", 0, "aiurdemux plugin");

}


static void gst_aiurdemux_init (GstAiurDemux * demux)
{

  demux->sinkpad =
      gst_pad_new_from_template (gst_aiurdemux_sink_pad_template (), "sink");

  gst_pad_set_activate_function (demux->sinkpad, aiurdemux_sink_activate);
  gst_pad_set_activatemode_function (demux->sinkpad,
      aiurdemux_sink_activate_mode);

  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiurdemux_chain));
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_aiurdemux_handle_sink_event));
  gst_element_add_pad (GST_ELEMENT_CAST (demux), demux->sinkpad);

  gstsutils_options_load_default (g_aiurdemux_option_table,
      (gchar *) & demux->option);
  gstsutils_options_load_from_keyfile (g_aiurdemux_option_table,
      (gchar *) & demux->option, (gchar *)FSL_GST_CONF_DEFAULT_FILENAME,
      (gchar *)"aiurdemux");

  demux->state = AIURDEMUX_STATE_PROBE;
  demux->pullbased = FALSE;
  demux->core_interface = NULL;
  demux->core_handle = NULL;

  demux->stream_cache = gst_aiur_stream_cache_new (AIUR_STREAM_CACHE_SIZE,
    AIUR_STREAM_CACHE_SIZE_MAX, demux);

  g_mutex_init (&demux->runmutex);
  demux->play_mode = AIUR_PLAY_MODE_NORMAL;

  GST_LOG_OBJECT(demux,"gst_aiurdemux_init");
  gst_segment_init (&demux->segment, GST_FORMAT_TIME);
}

static void gst_aiurdemux_finalize (GObject * object)
{

  GstAiurDemux *demux = GST_AIURDEMUX (object);
  GST_LOG_OBJECT(demux,"gst_aiurdemux_finalize");

  if (demux->stream_cache) {
    gst_mini_object_unref (GST_MINI_OBJECT_CAST (demux->stream_cache));
    g_free(demux->stream_cache);
    demux->stream_cache = NULL;
  }

  g_mutex_clear (&demux->runmutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);

}

static GstStateChangeReturn gst_aiurdemux_change_state (GstElement * element,
    GstStateChange transition)
{
  GstAiurDemux *demux = GST_AIURDEMUX (element);
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;
  
  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT(demux,"change_state READY_TO_PAUSED");

      gst_tag_register (GST_TAG_VIDEO_FRAMERATE, GST_TAG_FLAG_META,
          G_TYPE_UINT, "frame rate", "frame rate", NULL);
      gst_tag_register (GST_TAG_VIDEO_BITRATE, GST_TAG_FLAG_META,
          G_TYPE_UINT, "video bitrate", "video bitrate", NULL);
      gst_tag_register (GST_TAG_VIDEO_WIDTH, GST_TAG_FLAG_DECODED,
          G_TYPE_UINT, "image width", "image width", NULL);
      gst_tag_register (GST_TAG_VIDEO_HEIGHT, GST_TAG_FLAG_DECODED,
          G_TYPE_UINT, "image height", "image height", NULL);

      demux->clock_offset = GST_CLOCK_TIME_NONE;
      demux->start_time = GST_CLOCK_TIME_NONE;
      demux->tag_list = gst_tag_list_new_empty ();
      aiurcontent_new(&demux->content_info);
      break;
    default:
      GST_LOG_OBJECT(demux,"change_state transition=%x",transition);
      break;
  }
  
  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  
  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      GST_DEBUG_OBJECT(demux,"change_state PAUSED_TO_READY");
      demux->state = AIURDEMUX_STATE_PROBE;
      demux->pullbased = FALSE;


      demux->play_mode = AIUR_PLAY_MODE_NORMAL;
      demux->valid_mask = 0;
      demux->n_streams = 0;
      demux->n_video_streams = 0;
      demux->n_audio_streams = 0;
      demux->n_sub_streams = 0;

      
      if (demux->tag_list)
        gst_tag_list_unref (demux->tag_list);
      
      aiurdemux_release_resource (demux);
      
      gst_aiurdemux_close_core (demux);
      aiurcontent_release(demux->content_info);
      demux->content_info = NULL;
      
      gst_segment_init (&demux->segment, GST_FORMAT_TIME);

      break;
    }
  
    default:
      break;
  }
  
  return result;

}

static GstFlowReturn gst_aiurdemux_chain (GstPad * sinkpad, GstObject * parent,
    GstBuffer * inbuf)
{
  GstAiurDemux *demux;
  
  demux = GST_AIURDEMUX (parent);
  if (inbuf) {
    gst_aiur_stream_cache_add_buffer (demux->stream_cache, inbuf);
  }

  return GST_FLOW_OK;

}
static gboolean gst_aiurdemux_handle_sink_event(GstPad * sinkpad, GstObject * parent,
    GstEvent * event)
{
  GstAiurDemux *demux = GST_AIURDEMUX (parent);
  gboolean res;
  
  
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      const GstSegment* segment;

      gst_event_parse_segment(event,&segment);

      /* we only expect a BYTE segment, e.g. following a seek */
      if (segment->format == GST_FORMAT_BYTES) {
        if (demux->pullbased == FALSE) {
          gst_aiur_stream_cache_set_segment (demux->stream_cache, segment->start, segment->stop);
        }
      } else if (segment->format == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (demux, "handling new segment from%lld", segment->start);
        goto exit;
      } else {
        GST_DEBUG_OBJECT (demux, "unsupported segment format, ignoring");
        goto exit;
      }

      GST_DEBUG_OBJECT (demux,
          "Pushing newseg  rate %g, "
          "format %d, start %"
          GST_TIME_FORMAT ", " "stop %" GST_TIME_FORMAT,
          segment->rate, GST_FORMAT_TIME,
          GST_TIME_ARGS (segment->start), GST_TIME_ARGS (segment->stop));

      if (segment->stop < segment->start) {
        gst_event_unref (event);
        return FALSE;
      }

      /* clear leftover in current segment, if any */
    exit:
      gst_event_unref (event);
      res = TRUE;
      goto drop;
      break;
    }

    case GST_EVENT_FLUSH_START:
    case GST_EVENT_FLUSH_STOP:
    {
      gint i;

      /* reset flow return, e.g. following seek */
      for (i = 0; i < demux->n_streams; i++) {
        demux->streams[i]->last_ret = GST_FLOW_OK;
      }

      gst_event_unref (event);
      res = TRUE;
      goto drop;
      break;
    }
    case GST_EVENT_EOS:
      /* If we are in push mode, and get an EOS before we've seen any streams,
       * then error out - we have nowhere to send the EOS */
      if (demux->pullbased) {
        gint i;
        gboolean has_valid_stream = FALSE;
        for (i = 0; i < demux->n_streams; i++) {
          if (demux->streams[i]->pad != NULL) {
            has_valid_stream = TRUE;
            break;
          }
        }
        if (!has_valid_stream){
         ;// gst_aiurdemux_post_no_playable_stream_error (demux);
            }
      } else {
        gst_aiur_stream_cache_seteos (demux->stream_cache, TRUE);
        gst_event_unref (event);
        goto drop;
      }
      break;
    case GST_EVENT_CAPS:
        {
        GstCaps *caps = NULL;
        gst_event_parse_caps (event, &caps);
        res = gst_aiurdemux_setcaps(sinkpad, parent, caps);
        gst_event_unref (event);
        goto drop;
        break;
        }
    default:
        GST_LOG_OBJECT(demux,"gst_aiurdemux_handle_sink_event event=%x",GST_EVENT_TYPE (event));
      break;
  }

  res = gst_pad_event_default (demux->sinkpad, parent, event);

drop:
  return res;

}
static gboolean gst_aiurdemux_handle_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res = TRUE;
  GstAiurDemux *demux = GST_AIURDEMUX (parent);
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      if (!demux->seekable && !aiurcontent_is_seelable(demux->content_info)) {
        goto not_support;
      }

      if ((demux->state == AIURDEMUX_STATE_MOVIE) && demux->n_streams) {
        if (demux->pullbased) {
          res = aiurdemux_do_seek (demux, pad, event);
        } else {
          res = aiurdemux_do_push_seek (demux, pad, event);
        }
      } else {
        GST_DEBUG_OBJECT (demux,
            "ignoring seek in push mode in current state");
        res = FALSE;
      }
      gst_event_unref (event);
      break;

    case GST_EVENT_QOS:
    case GST_EVENT_NAVIGATION:
      res = FALSE;
      gst_event_unref (event);
      break;
    default:
      GST_LOG_OBJECT(demux,"gst_aiurdemux_handle_src_event event=%x",GST_EVENT_TYPE (event));
      res = gst_pad_event_default (pad,parent, event);
      break;
  }

  return res;

  /* ERRORS */
not_support:
  {
    GST_WARNING ("Unsupport source event %s. ", GST_EVENT_TYPE_NAME (event));
    gst_event_unref (event);
    return FALSE;
  }
}
static gboolean
gst_aiurdemux_handle_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = FALSE;
  GstAiurDemux *demux = GST_AIURDEMUX (parent);

  GST_LOG_OBJECT (pad, "%s query", GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {

    case GST_QUERY_DURATION:{
      GstFormat fmt;

      gst_query_parse_duration (query, &fmt, NULL);
      if (fmt == GST_FORMAT_TIME) {
        gint64 duration = -1;

        gst_aiurdemux_get_duration (demux, &duration);
        if (duration > 0) {
          gst_query_set_duration (query, GST_FORMAT_TIME, duration);
          res = TRUE;
        }
      }
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;
      gboolean seekable = FALSE;

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      if (fmt == GST_FORMAT_TIME) {
        gint64 duration = -1;

        gst_aiurdemux_get_duration (demux, &duration);

        if (demux->seekable && aiurcontent_is_seelable(demux->content_info)) {
          seekable = TRUE;
        }

        gst_query_set_seeking (query, GST_FORMAT_TIME, seekable, 0, duration);
        res = TRUE;
      }
      break;
    }
    case GST_QUERY_POSITION:
      if (GST_CLOCK_TIME_IS_VALID (demux->segment.position)) {
        gst_query_set_position (query, GST_FORMAT_TIME,
            demux->segment.position);
        res = TRUE;
      }
      break;

    default:
      GST_LOG_OBJECT(demux,"gst_aiurdemux_handle_src_query event=%x",GST_QUERY_TYPE (query));
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

  return res;
}
static void
gst_aiurdemux_push_tags (GstAiurDemux * demux, AiurDemuxStream * stream)
{
  if (G_LIKELY (stream->pad)) {
    GST_DEBUG_OBJECT (demux, "Checking pad %s:%s for tags",
        GST_DEBUG_PAD_NAME (stream->pad));

    if (G_UNLIKELY (stream->pending_tags)) {
        gst_pad_push_event (stream->pad,
            gst_event_new_tag (stream->pending_tags));

      stream->pending_tags = NULL;
    }

    if (G_UNLIKELY (stream->send_global_tags && demux->tag_list)) {
      GST_DEBUG_OBJECT (demux, "Sending global tags %" GST_PTR_FORMAT,
          demux->tag_list);
      gst_pad_push_event (stream->pad,
          gst_event_new_tag (gst_tag_list_ref (demux->tag_list)));
      stream->send_global_tags = FALSE;
    }

  }
}

/* push event on all source pads; takes ownership of the event */
static void
gst_aiurdemux_push_event (GstAiurDemux * demux, GstEvent * event)
{
  gint n;
  //gboolean pushed_sucessfully = FALSE;

  for (n = 0; n < demux->n_streams; n++) {
    GstPad *pad = demux->streams[n]->pad;

    if (pad) {
      if (gst_pad_push_event (pad, gst_event_ref (event))) {
        ;//pushed_sucessfully = TRUE;
      }
    }

  }

  
  gst_event_unref (event);
}

static GstPadTemplate *
gst_aiurdemux_sink_pad_template (void)
{
  static GstPadTemplate *templ = NULL;

  if (!templ) {
    GstCaps *caps = aiur_core_get_caps ();

    if (caps) {
      templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
      gst_caps_unref (caps);
    }
  }
  return templ;
}

static gboolean aiurdemux_sink_activate (GstPad * sinkpad,GstObject * parent)
{
  GstQuery *query;
  gboolean pull_mode;

  query = gst_query_new_scheduling ();

  if (!gst_pad_peer_query (sinkpad, query)) {
      gst_query_unref (query);
      goto activate_push;
  }

  pull_mode = gst_query_has_scheduling_mode_with_flags (query,
      GST_PAD_MODE_PULL, GST_SCHEDULING_FLAG_SEEKABLE);
  gst_query_unref (query);

  if (!pull_mode)
    goto activate_push;
  
  GST_LOG_OBJECT (sinkpad, "activating pull");
  return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PULL, TRUE);

activate_push:
  GST_LOG_OBJECT (sinkpad, "activating push");
  return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);

}

static gboolean aiurdemux_sink_activate_mode (GstPad * sinkpad,
    GstObject * parent, GstPadMode mode, gboolean active)
{
  gboolean res = FALSE;
  
  switch (mode) {
    case GST_PAD_MODE_PUSH:
      return aiurdemux_sink_activate_push(sinkpad, parent, active);
    case GST_PAD_MODE_PULL:
      return aiurdemux_sink_activate_pull(sinkpad, parent, active);
    default:
      break;
      }
  return res;
}

static gboolean aiurdemux_sink_activate_pull (GstPad * sinkpad, GstObject * parent, gboolean active)
{
  GstAiurDemux *demux = GST_AIURDEMUX (parent);

  if (active) {
      demux->pullbased = TRUE;
      return gst_pad_start_task (sinkpad,
          (GstTaskFunction) aiurdemux_pull_task, sinkpad, NULL);
  } else {
      return gst_pad_stop_task (sinkpad);
  }
}

gpointer aiurdemux_loop_push (gpointer * data)
{
  GstAiurDemux *demux = (GstAiurDemux *) data;

  g_mutex_lock (&demux->runmutex);

  while (demux->loop_push) {
    aiurdemux_push_task (demux);
  }

  g_mutex_unlock (&demux->runmutex);

  return NULL;
}

static gboolean aiurdemux_sink_activate_push (GstPad * sinkpad, GstObject * parent, gboolean active)
{
  GstAiurDemux *demux = GST_AIURDEMUX (parent);
  demux->pullbased = FALSE;


  if (active) {
    demux->loop_push = TRUE;
    g_thread_new ("aiur_push",(GThreadFunc) aiurdemux_loop_push, (gpointer) demux);

    return TRUE;

  } else {
    demux->loop_push = FALSE;
    gst_aiur_stream_cache_close (demux->stream_cache);
    /* make sure task is closed */
    g_mutex_lock (&demux->runmutex);
    g_mutex_unlock (&demux->runmutex);
    return gst_pad_stop_task (sinkpad);
  }
}

static void aiurdemux_pull_task (GstPad * pad)
{
  GstAiurDemux *demux;
  GstFlowReturn ret;

  demux = GST_AIURDEMUX (gst_pad_get_parent (pad));

  switch (demux->state) {
    case AIURDEMUX_STATE_PROBE:
      ret = aiurdemux_loop_state_probe (demux);
      break;
    case AIURDEMUX_STATE_INITIAL:
      ret = aiurdemux_loop_state_init (demux);
      break;
    case AIURDEMUX_STATE_HEADER:
      ret = aiurdemux_loop_state_header (demux);
      break;
    case AIURDEMUX_STATE_MOVIE:
      ret = aiurdemux_loop_state_movie (demux);
      break;
    default:
      /* ouch */
      goto invalid_state;
  }

  /* if something went wrong, pause */
  if (ret != GST_FLOW_OK)
    goto pause;

done:
  gst_object_unref (demux);
  return;

invalid_state:
  aiurdemux_send_stream_eos_all (demux);

pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_WARNING ("pausing task, reason %s \r\n", reason);

    gst_pad_pause_task (pad);

    if (ret < GST_FLOW_EOS) {
      GST_ELEMENT_ERROR (demux, STREAM, FAILED,
          (NULL), ("streaming stopped, reason %s, state %d",
              reason, demux->state));
    }

    goto done;
  }
}

static void aiurdemux_push_task (GstAiurDemux * demux)
{
  GstFlowReturn ret;

  switch (demux->state) {
    case AIURDEMUX_STATE_PROBE:
      ret = aiurdemux_loop_state_probe (demux);
      break;
    case AIURDEMUX_STATE_INITIAL:
      ret = aiurdemux_loop_state_init (demux);
      break;
    case AIURDEMUX_STATE_HEADER:
      ret = aiurdemux_loop_state_header (demux);
      break;
    case AIURDEMUX_STATE_MOVIE:
      ret = aiurdemux_loop_state_movie (demux);
      break;
    default:
      /* ouch */
      goto invalid_state;
  }

  /* if something went wrong, pause */
  if (ret != GST_FLOW_OK)
    goto pause;

done:
  return;

invalid_state:
  aiurdemux_send_stream_eos_all (demux);

pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_LOG_OBJECT (demux, "pausing task, reason %s \r\n", reason);

    demux->loop_push = FALSE;

    if (ret < GST_FLOW_EOS) {
      GST_ELEMENT_ERROR (demux, STREAM, FAILED,
          (NULL), ("streaming stopped, reason %s, state %d",
              reason, demux->state));
    }

    goto done;
  }
}

static gboolean gst_aiurdemux_setcaps(GstPad * pad, GstObject * parent, GstCaps * caps)
{
  GstAiurDemux *demux = GST_AIURDEMUX (parent);

  if (!demux->pullbased)
      gst_aiur_stream_cache_attach_pad (demux->stream_cache, pad);

  GST_DEBUG_OBJECT(demux,"gst_aiurdemux_setcaps=%s",gst_caps_to_string(caps));


  

  

  if(demux->core_interface == NULL)
    demux->core_interface = aiur_core_create_interface_from_caps (caps);

  if ((demux->core_interface) && (demux->core_interface->name)
    && (demux->core_interface->name)) {
    gst_tag_list_add (demux->tag_list, GST_TAG_MERGE_REPLACE,
        GST_TAG_CONTAINER_FORMAT, (demux->core_interface->name), NULL);
  }

  if (demux->core_interface) {
    demux->state = AIURDEMUX_STATE_INITIAL;

    return TRUE;
  } else{
    return FALSE;
  }
}

static GstFlowReturn
aiurdemux_loop_state_probe (GstAiurDemux * demux)
{
  GstBuffer *buffer = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstCaps * caps;

  if (demux->pullbased) {
    gst_pad_pull_range (demux->sinkpad, (guint64) 0,
        AIURDEMUX_INIT_BLOCK_SIZE, &buffer);
    gst_buffer_unref (buffer);

    caps = gst_pad_peer_query_caps (demux->sinkpad, NULL);
    GST_LOG_OBJECT(demux,"state_probe CAPS=%s",gst_caps_to_string(caps));
    gst_aiurdemux_setcaps(demux->sinkpad,(GstObject*) demux, caps);
    gst_caps_unref (caps);
  }

  return ret;
}


static GstFlowReturn
aiurdemux_loop_state_init (GstAiurDemux * demux)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  int32 parser_result = PARSER_SUCCESS;
  FslParserHandle core_handle;
  gboolean isLive = FALSE;
  AiurCoreInterface *IParser = demux->core_interface;
  FslFileStream *file_cbks;
  ParserMemoryOps *mem_cbks;
  ParserOutputBufferOps *buf_cbks;
  uint32 flag;

  if (IParser == NULL)
      return GST_FLOW_OK;

  file_cbks = g_new0 (FslFileStream, 1);
  mem_cbks = g_new0 (ParserMemoryOps, 1);
  buf_cbks = g_new0 (ParserOutputBufferOps, 1);

  if ((!file_cbks) || (!mem_cbks) || (!buf_cbks))
      goto fail;

  if(!demux->content_info)
    goto fail;
  if(demux->pullbased)
      aiurcontent_get_pullfile_callback(demux->content_info,file_cbks);

  else
      aiurcontent_get_pushfile_callback(demux->content_info,file_cbks);


  aiurcontent_get_memory_callback(demux->content_info,mem_cbks);


  aiurcontent_get_buffer_callback(demux->content_info,buf_cbks);


  aiurcontent_init(demux->content_info,demux->sinkpad,demux->stream_cache);


  isLive = aiurcontent_is_live(demux->content_info);

  if(IParser->createParser2){
    flag = FLAG_H264_NO_CONVERT;
    if(isLive){
        flag |= FILE_FLAG_NON_SEEKABLE;
        flag |= FILE_FLAG_READ_IN_SEQUENCE;
    }
    parser_result = IParser->createParser2(flag ,file_cbks, mem_cbks,
      buf_cbks, (void *)(demux->content_info), &core_handle);
  }else{
  parser_result = IParser->createParser((bool)isLive,file_cbks, mem_cbks,
                              buf_cbks, (void *)(demux->content_info), &core_handle);
  }

  if (parser_result != PARSER_SUCCESS) {
      GST_ERROR_OBJECT(demux,"Failed to create fsl parser! ret=%d",parser_result);
      goto fail;
  }


  demux->core_handle = core_handle;
  demux->state = AIURDEMUX_STATE_HEADER;
  ret = GST_FLOW_OK;

  g_free (file_cbks);
  g_free (mem_cbks);
  g_free (buf_cbks);
  GST_LOG_OBJECT(demux,"aiurdemux_loop_state_init SUCCESS");

  return ret;
fail:
  GST_LOG_OBJECT(demux,"aiurdemux_loop_state_init FAIL");
  if (file_cbks) {
      g_free (file_cbks);
  }
  if (mem_cbks) {
      g_free (mem_cbks);
  }
  if (buf_cbks) {
      g_free (buf_cbks);
  }
  if (core_handle) {
      IParser->deleteParser(core_handle);
  }
  return ret;
}

static GstFlowReturn aiurdemux_loop_state_header (GstAiurDemux * demux)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  int32 parser_result = PARSER_SUCCESS;
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  uint64 start_time_us = 0;
  int32 i = 0;
  uint64 duration = 0;
  gboolean need_init_index = TRUE;
  gchar * index_file = NULL;

  do{
    if(IParser == NULL || handle == NULL)
        break;

    index_file = aiurcontent_get_index_file(demux->content_info);

      if ((demux->option.index_enabled) && index_file) {
    AiurIndexTable *idxtable =
        aiurdemux_import_idx_table (index_file);
    if (idxtable) {

      if ((idxtable->coreid) && (IParser->coreid)
          && (strlen (IParser->coreid) == idxtable->coreid_len)
          && (!memcmp (idxtable->coreid, IParser->coreid, idxtable->coreid_len))) {

        if ((IParser->initializeIndex)
            && (IParser->importIndex)
            && (idxtable->info.size)) {

          parser_result = IParser->importIndex(handle, idxtable->idx,
              idxtable->info.size);
          if (parser_result == PARSER_SUCCESS) {
            GST_INFO ("Index table %s[size %d] imported.",
                index_file, idxtable->info.size);
            need_init_index = FALSE;
          }
        }
      }
      aiurdemux_destroy_idx_table (idxtable);
    }
    
  }

    if (need_init_index && IParser->initializeIndex != NULL) {
      parser_result = IParser->initializeIndex(handle);
    }

    parser_result = IParser->isSeekable(handle, &demux->seekable);
    if(parser_result != PARSER_SUCCESS)
        break;

    demux->movie_duration = GST_CLOCK_TIME_NONE;
    parser_result = IParser->getMovieDuration(handle,&duration);
        demux->movie_duration = AIUR_CORETS_2_GSTTS (duration);
    if(parser_result != PARSER_SUCCESS)
        break;

    if(aiurdemux_set_readmode(demux) == FALSE)
        break;

    parser_result = IParser->getNumTracks(handle,&demux->track_count);
    if(parser_result != PARSER_SUCCESS)
        break;

    //get progreme number and parse programes
    if(IParser->getNumPrograms)
      IParser->getNumPrograms(handle, &demux->program_num);

    if(demux->program_num > 0){
      demux->programs = g_new0 (AiurDemuxProgram*, demux->program_num);
      aiurdemux_parse_programs (demux);
    }

    demux->tag_list = aiurdemux_add_user_tags (demux);

    //parse stream and tracks
    parser_result = aiurdemux_parse_streams (demux);

    gst_element_no_more_pads (GST_ELEMENT_CAST (demux));

    for (i = 0; i < demux->n_streams; i++) {
        IParser->seek(handle,i,&start_time_us,SEEK_FLAG_NO_LATER);
    }

    if (parser_result == PARSER_SUCCESS && demux->n_streams > 0) {
        GST_LOG_OBJECT(demux,"aiurdemux_loop_state_header next MOVIE");
        demux->state = AIURDEMUX_STATE_MOVIE;
        ret = GST_FLOW_OK;
    }
    GST_LOG_OBJECT(demux,"aiurdemux_loop_state_header SUCCESS");
  }while(0);

  if(ret != GST_FLOW_OK){
      aiurdemux_release_resource (demux);
      GST_LOG_OBJECT(demux,"aiurdemux_loop_state_header FAILED");
  }

  return ret;
}

static GstFlowReturn aiurdemux_loop_state_movie (GstAiurDemux * demux)
{

  GstFlowReturn ret = GST_FLOW_OK;
  AiurDemuxStream *stream = NULL;
  GstBuffer *gstbuf = NULL;
  uint32 track_idx = 0;
  AiurCoreInterface *IParser = demux->core_interface;

  GST_LOG_OBJECT(demux,"aiurdemux_loop_state_movie BEGIN");

  
  if (demux->pending_event) {
    aiurdemux_send_pending_events (demux);
    demux->pending_event = FALSE;
    if (demux->valid_mask == 0) {
      return GST_FLOW_EOS;
    }
  }

  //select a track to read
  track_idx = aiurdemux_choose_next_stream(demux);


  //check long interleave for file mode
  if(demux->read_mode == PARSER_READ_MODE_FILE_BASED){
      aiurdemux_check_interleave_stream_eos(demux);
      stream = aiurdemux_trackidx_to_stream (demux, track_idx);

      //push buffer from interleave queue
      if ((stream->buf_queue)
          && (!g_queue_is_empty (stream->buf_queue))) {
        gstbuf = g_queue_pop_head (stream->buf_queue);
        stream->buf_queue_size -= gst_buffer_get_size(gstbuf);//GST_BUFFER_SIZE (gstbuf);
        ret = aiurdemux_push_pad_buffer (demux, stream, gstbuf);
        goto bail;
      }
  }
    
  //read stream buffer
  ret = aiurdemux_read_buffer(demux,&track_idx,&stream);

  GST_LOG_OBJECT(demux,"aiurdemux_loop_state_movie read buffer ret=%d",ret);
  if(ret != GST_FLOW_OK)
      goto beach;
  
  //update time
  if (!stream || !stream->buffer)
    goto bail;
    GST_LOG_OBJECT (demux, "CHECK track_idx=%d,usStartTime=%lld,sampleFlags=%x",track_idx,stream->sample_stat.start,stream->sample_stat.flag);

  if((demux->seekable == FALSE)
      && !aiurcontent_is_seelable(demux->content_info)
      && !aiurcontent_is_random_access(demux->content_info))
        aiurdemux_check_start_offset(demux, stream);

    
    aiurdemux_adjust_timestamp (demux, stream, stream->buffer);

    //send new segment
    if (stream->new_segment) {
      GST_BUFFER_FLAG_UNSET (stream->buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      GST_BUFFER_FLAG_SET (stream->buffer, GST_BUFFER_FLAG_DISCONT);
      aiurdemux_send_stream_newsegment (demux, stream);
      gst_aiurdemux_push_tags (demux, stream);
    }
     
    //for vorbis codec data
    if(demux->option.disable_vorbis_codec_data &&
      (stream->type == MEDIA_AUDIO)
      && (stream->codec_type == AUDIO_VORBIS)
      && (stream->send_codec_data == FALSE)
      && (stream->codec_data.length)){
      aiurdemux_parse_vorbis_codec_data(demux, stream);
      stream->send_codec_data = TRUE;
    }
    //push buffer to pad
    if (demux->interleave_queue_size) {
     stream->buf_queue_size += gst_buffer_get_size(stream->buffer);
     if (stream->buf_queue_size > stream->buf_queue_size_max) {
       stream->buf_queue_size_max = stream->buf_queue_size;
     }

     g_queue_push_tail (stream->buf_queue, stream->buffer);
     stream->buffer = NULL;
    } else {
     ret = aiurdemux_push_pad_buffer (demux, stream, stream->buffer);
     stream->buffer = NULL;
    }

    AIUR_RESET_SAMPLE_STAT (stream->sample_stat);

    ret = aiurdemux_combine_flows (demux, stream, ret);

  GST_LOG_OBJECT (demux, "STATE MOVIE END ret=%d",ret);

bail:    
  return ret;


beach:
  if (stream) {
  if (stream->buffer) {
    gst_buffer_unref (stream->buffer);
    stream->buffer = NULL;
  }

  gst_adapter_clear (stream->adapter);

  AIUR_RESET_SAMPLE_STAT (stream->sample_stat);
  }
  return ret;
}

static void
aiurdemux_print_track_info (AiurDemuxStream * stream)
{

  if ((stream) && (stream->pad) && (stream->caps)) {
    gchar *mime = gst_caps_to_string (stream->caps);
    gchar *padname = gst_pad_get_name (stream->pad);
    g_print ("Track %02d [%s] Enabled\n", stream->track_idx,
            padname ? padname : "");

    g_print ("\tDuration: %" GST_TIME_FORMAT "\n",
            GST_TIME_ARGS (stream->track_duration));
    g_print ("\tLanguage: %s\n", stream->lang);
    if (mime) {
      g_print ("Mime %s \r\n", mime);
      g_free (mime);
    }
    if (padname) {
      g_free (padname);
    }
  } else {
    g_print ("Track %02d [%s]: Disabled\n", stream->track_idx,
            AIUR_MEDIATYPE2STR (stream->type));
    g_print ("\tCodec: %ld, SubCodec: %ld\n",
            stream->codec_type, stream->codec_sub_type);
  }
}


static gboolean aiurdemux_set_readmode (GstAiurDemux * demux)
{
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;

  gchar *format = NULL;
  gboolean isMPEG = FALSE;

  int32 parser_result;
  uint32 readmode;

  gboolean ret = gst_tag_list_get_string (demux->tag_list, GST_TAG_CONTAINER_FORMAT, &format);
  if(ret && format && strncmp(format,"MPEG",4) == 0){
      isMPEG = TRUE;
  }
  if(format ){
    g_free(format);
    format = NULL;
  }
  
  //use track mode for local file and file mode for streaming file.
  if(aiurcontent_is_random_access(demux->content_info) && !isMPEG){
      readmode = PARSER_READ_MODE_TRACK_BASED;
  }else{
      readmode = PARSER_READ_MODE_FILE_BASED;
  }

  do{
      parser_result = IParser->setReadMode(handle, readmode);

      if (parser_result != PARSER_SUCCESS) {

          //some parser does not have track mode API, so use file mode.
          readmode = PARSER_READ_MODE_FILE_BASED;
          parser_result = IParser->setReadMode(handle, readmode);
          if (parser_result != PARSER_SUCCESS)
              break;
      }
  
      //check readmode API
      if ((readmode == PARSER_READ_MODE_TRACK_BASED) && (IParser->getNextSample == NULL))
          break;

      if((readmode == PARSER_READ_MODE_FILE_BASED) && (IParser->getFileNextSample == NULL))
          break;
  
      demux->read_mode = readmode;

      if (readmode == PARSER_READ_MODE_FILE_BASED){
          demux->interleave_queue_size = demux->option.interleave_queue_size;
          GST_DEBUG_OBJECT(demux,"read mode = file mode");
      } else {
          demux->interleave_queue_size = 0;
          GST_DEBUG_OBJECT(demux,"read mode = track mode");
      }
       
      return TRUE;

  }while(0);

  return FALSE;
}
static gboolean
aiurdemux_parse_programs (GstAiurDemux * demux)
{
  gboolean ret = FALSE;
  if ((aiurdemux_parse_pmt (demux))
      || (aiurdemux_parse_program_info (demux))) {
    aiurdemux_select_programs (demux);
    ret = TRUE;
  }
  return ret;
}
static gboolean
aiurdemux_parse_pmt (GstAiurDemux * demux)
{
  gboolean ret = FALSE;
  int32 core_ret = PARSER_SUCCESS;
  //AiurDemuxClipInfo *clip_info = &demux->clip_info;
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  UserDataID id = USER_DATA_PMT;
  UserDataFormat format = USER_DATA_FORMAT_PMT_INFO;
  PMTInfoList *pmt = NULL;
  uint32 userDataSize = 0;

  core_ret = IParser->getMetaData(handle,id, &format, (uint8 **) & pmt, &userDataSize);
  if ((core_ret == PARSER_SUCCESS) && (pmt != NULL)
      && (userDataSize > 0)) {
    int n;
    for (n = 0;
        ((n < demux->program_num) && (n < pmt->m_dwProgramNum)); n++) {
      AiurDemuxProgram *program;
      PMTInfo *pmtinfo = &pmt->m_ptPMTInfo[n];
      program = g_try_malloc (sizeof (AiurDemuxProgram) +
              sizeof (AiurDemuxProgramTrack) * pmtinfo->m_dwTrackNum);
      if (program) {
        int m;
        program->program_number = pmtinfo->m_dwChannel;
        program->enabled = FALSE;
        program->pid = pmtinfo->m_dwPID;
        program->track_num = pmtinfo->m_dwTrackNum;
        for (m = 0; m < program->track_num; m++) {
          program->tracks[m].id = pmtinfo->m_ptTrackInfo[m].m_dwTrackNo;
          program->tracks[m].pid = pmtinfo->m_ptTrackInfo[m].m_dwPID;
          memcpy (program->tracks[m].lang,
              pmtinfo->m_ptTrackInfo[m].m_byLan, 3);
        }
        demux->programs[n] = program;
      }
    }
    ret = TRUE;
  }

  return ret;
}
gboolean
aiurdemux_parse_program_info (GstAiurDemux * demux)
{
  gboolean ret = FALSE;
  int32 core_ret = PARSER_SUCCESS;
  //AiurDemuxClipInfo *clip_info = &demux->clip_info;
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  UserDataID id = USER_DATA_PROGRAMINFO;
  UserDataFormat format = USER_DATA_FORMAT_PROGRAM_INFO;
  ProgramInfoMenu *pinfo = NULL;
  uint32 userDataSize = 0;
  int n;

  core_ret = IParser->getMetaData(handle, id, &format, (uint8 **) & pinfo, &userDataSize);
  if ((core_ret != PARSER_SUCCESS) || (userDataSize == 0)) {
    pinfo = NULL;
  }

  for (n = 0; n < demux->program_num; n++) {
    AiurDemuxProgram *program;
    uint32 track_num, *tracklist;
    core_ret = IParser->getProgramTracks(handle, n,
        &track_num, &tracklist);
    if (core_ret == PARSER_SUCCESS) {
      program =
          g_try_malloc (sizeof (AiurDemuxProgram) +
              sizeof (AiurDemuxProgramTrack) * track_num);
      if (program) {
        int m;
        program->enabled = FALSE;
        if ((pinfo) && (n < pinfo->m_dwProgramNum)) {
          program->program_number = pinfo->m_atProgramInfo[n].m_dwChannel;
          program->pid = pinfo->m_atProgramInfo[n].m_dwPID;
        } else {
          program->program_number = -1;
          program->pid = -1;
        }
        program->track_num = track_num;
        for (m = 0; m < track_num; m++) {
          program->tracks[m].pid = -1;
          program->tracks[m].id = tracklist[m];
          program->tracks[m].lang[0] = '\0';
        }
        demux->programs[n] = program;
      }
    }
  }
  ret = TRUE;

  return ret;
}
static void
aiurdemux_select_programs (GstAiurDemux * demux)
{
  int n;
  for (n = 0; n < demux->program_num; n++) {
    AiurDemuxProgram *program;
    program = demux->programs[n];
    if (program) {
      if (demux->option.multiprogram_enabled) {
        if (((demux->option.program_mask == 0))
            || ((1 << n) & demux->option.program_mask)) {
          program->enabled = TRUE;
        }
      } else {
        if (demux->option.program_number >= 0) {
          if (demux->option.program_number == program->program_number) {
            program->enabled = TRUE;
            break;
          }
        } else {
          program->enabled = TRUE;
          break;
        }
      }
    }
  }
}




static GstTagList *
aiurdemux_add_user_tags (GstAiurDemux * demux)
{
  int32 core_ret = 0;
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  GstTagList *list = demux->tag_list;

  int i;

  if (list == NULL)
    return list;

  if (IParser->getMetaData) {
    UserDataID id;
    UserDataFormat format;

    for (i = 0; i < G_N_ELEMENTS (g_user_data_entry); i++) {
      uint8 *userData = NULL;
      uint32 userDataSize = 0;

      id = g_user_data_entry[i].core_tag;
      format = g_user_data_entry[i].format;
      core_ret = IParser->getMetaData(handle,id, &format, &userData, &userDataSize);
      if ((core_ret == PARSER_SUCCESS) &&
          (userData != NULL) && (userDataSize > 0)) {
        if (USER_DATA_FORMAT_UTF8 == format) {
          GString *string = g_string_new_len ((const gchar *)userData, userDataSize);
          if (string) {
            /* FIXME : create GDate object for GST_TAG_DATA */
            if (USER_DATA_CREATION_DATE == id) {
              g_string_free (string, TRUE);
              continue;
            }
            gst_tag_list_add (list, GST_TAG_MERGE_APPEND,
                g_user_data_entry[i].gst_tag_name, string->str, NULL);

            GST_INFO_OBJECT (demux, g_user_data_entry[i].print_string, string->str);

            g_string_free (string, TRUE);
          }
        } else if ((USER_DATA_FORMAT_JPEG == format) ||
            (USER_DATA_FORMAT_PNG == format) ||
            (USER_DATA_FORMAT_BMP == format) ||
            (USER_DATA_FORMAT_GIF == format)) {
            GstSample *sample = gst_tag_image_data_to_image_sample (userData,
              userDataSize, GST_TAG_IMAGE_TYPE_UNDEFINED);
          if (sample) {

            gst_tag_list_add (list, GST_TAG_MERGE_APPEND,
                g_user_data_entry[i].gst_tag_name, sample, NULL);

            GST_INFO_OBJECT (demux, g_user_data_entry[i].print_string, format,userDataSize);
            
            gst_sample_unref (sample);
          }
        }
      }
    }
  } else if (IParser->getUserData) {

    for (i = 0; i < G_N_ELEMENTS (g_user_data_entry); i++) {
      uint16 *userData = NULL;
      uint32 userDataSize = 0;

      core_ret = IParser->getUserData(handle,g_user_data_entry[i].core_tag, &userData, &userDataSize);
      if (core_ret == PARSER_SUCCESS) {
        if ((userData) && (userDataSize)) {
          gsize in, out;
          gchar *value_utf8;
          value_utf8 =
              g_convert ((const char *) userData, userDataSize * 2, "UTF-8",
              "UTF-16LE", &in, &out, NULL);
          if (value_utf8) {
            gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
                g_user_data_entry[i].gst_tag_name, value_utf8, NULL);

            GST_INFO_OBJECT (demux, g_user_data_entry[i].print_string, value_utf8);

            g_free (value_utf8);
          }
        }
      }
    }
  }


  if (gst_tag_list_is_empty (list)) {
    gst_tag_list_unref (list);
    list = NULL;
  }

  return list;
}

static int aiurdemux_parse_streams (GstAiurDemux * demux)
{
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  int ret = PARSER_SUCCESS;
  uint32 i = 0;
  AiurDemuxStream *stream = NULL;
  

  demux->n_streams = 0;
  demux->n_video_streams = 0;
  demux->n_audio_streams = 0;
  demux->n_sub_streams = 0;
  memset(demux->streams, 0,GST_AIURDEMUX_MAX_STREAMS*sizeof(AiurDemuxStream *));

  for(i = 0; i < demux->track_count; i++){
    uint64 duration = 0;
    stream = g_new0 (AiurDemuxStream, 1);

    if(stream == NULL){
        ret = PARSER_INSUFFICIENT_MEMORY;
        break;
    }

    memset(stream, 0, sizeof(AiurDemuxStream));
    stream->pid = -1;
    stream->track_idx = i;
    stream->partial_sample = FALSE;

    ret = IParser->getTrackType(handle, i, &stream->type,
        &stream->codec_type, &stream->codec_sub_type);

    if(ret != PARSER_SUCCESS)
        break;

    ret = IParser->getTrackDuration(handle, i, &duration);

    if(ret != PARSER_SUCCESS)
        break;
    stream->track_duration = AIUR_CORETS_2_GSTTS(duration);

    if(IParser->getLanguage){
    ret = IParser->getLanguage(handle, i, (uint8 *)stream->lang);
    if(ret != PARSER_SUCCESS){
        stream->lang[0] = '\0';
      }
    }

    ret = IParser->getBitRate(handle, i, &stream->bitrate);
    if (ret != PARSER_SUCCESS) {
        stream->bitrate = 0;
    }

    ret = IParser->getDecoderSpecificInfo(handle, i,
        &stream->codec_data.codec_data, &stream->codec_data.length);

    ret = PARSER_SUCCESS;
    
    switch (stream->type) {
        case MEDIA_VIDEO:
            aiurdemux_parse_video (demux, stream, i);
            break;
        case MEDIA_AUDIO:
            aiurdemux_parse_audio (demux, stream, i);
            break;
        case MEDIA_TEXT:
            aiurdemux_parse_text (demux, stream, i);
            break;
        default:
            break;
    }
    
    if ((stream->send_codec_data) && (stream->codec_data.length)
        && (stream->codec_data.length < AIURDEMUX_CODEC_DATA_MAX_LEN)) {
      GstBuffer *gstbuf;
      gstbuf = gst_buffer_new_and_alloc (stream->codec_data.length);
      gst_buffer_fill(gstbuf,0,(guint8 *)stream->codec_data.codec_data,stream->codec_data.length);

      GST_DEBUG_OBJECT(demux,"set codec data for caps len=%d",stream->codec_data.length);
      if(stream->caps){
        gst_caps_set_simple (stream->caps, "codec_data",
            GST_TYPE_BUFFER, gstbuf, NULL);
      }
      gst_buffer_unref (gstbuf);
    }
    
    aiurdemux_print_track_info (stream);
    
    if (stream->pad) {
      //gboolean padRet = TRUE;
      gchar *stream_id;
      GST_PAD_ELEMENT_PRIVATE (stream->pad) = stream;
      gst_pad_use_fixed_caps (stream->pad);
      gst_pad_set_event_function (stream->pad, gst_aiurdemux_handle_src_event);
      gst_pad_set_query_function (stream->pad, gst_aiurdemux_handle_src_query);
      gst_pad_set_active (stream->pad, TRUE);


      ret = IParser->enableTrack(handle, stream->track_idx, TRUE);
      if(ret != PARSER_SUCCESS)
          break;

      stream_id =
          gst_pad_create_stream_id_printf (stream->pad,
          GST_ELEMENT_CAST (demux), "%u", stream->track_idx);
      gst_pad_push_event (stream->pad, gst_event_new_stream_start (stream_id));
      g_free (stream_id);

      gst_pad_set_caps (stream->pad, stream->caps);

      GST_INFO_OBJECT (demux, "adding pad %s %p to demux %p, caps string=%s",
            GST_OBJECT_NAME (stream->pad), stream->pad, demux,gst_caps_to_string(stream->caps));
      gst_element_add_pad (GST_ELEMENT_CAST (demux), stream->pad);

      // global tags go on each pad anyway
      stream->send_global_tags = TRUE;

      stream->mask = (1 << demux->n_streams);
      stream->adapter = gst_adapter_new ();

      aiurdemux_reset_stream (demux, stream);
      if (demux->interleave_queue_size) {
        stream->buf_queue = g_queue_new ();
      }
      
      demux->streams[demux->n_streams] = stream;
      demux->n_streams++;
      stream->discont = TRUE;
    }else{
      IParser->enableTrack(handle, i, FALSE);

      if (stream) {
        if (stream->caps) {
          gst_caps_unref (stream->caps);
        }
        if (stream->pending_tags) {
          gst_tag_list_unref (stream->pending_tags);
        }
        g_free (stream);
      }
    }
  }

  if(ret != PARSER_SUCCESS){

    for (; i < demux->track_count; i++) {
        IParser->enableTrack(handle, i, FALSE);
    }
  }
  GST_LOG_OBJECT(demux,"aiurdemux_parse_streams ret=%d",ret);

  return ret;
}
static void aiurdemux_parse_video (GstAiurDemux * demux, AiurDemuxStream * stream,
    gint track_index)
{
  gchar *mime = NULL, *codec = NULL;
  gchar *padname;

  int32 parser_ret = PARSER_SUCCESS;
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  AiurdemuxCodecStruct * codec_struct = NULL;
  int i;


  memset(&stream->info.video, 0, sizeof(AiurDemuxVideoInfo));

  parser_ret = IParser->getVideoFrameWidth(handle,track_index,&stream->info.video.width);

  if(parser_ret != PARSER_SUCCESS)
      goto bail;

  parser_ret = IParser->getVideoFrameHeight(handle,track_index,&stream->info.video.height);

  if(parser_ret != PARSER_SUCCESS)
      goto bail;

  parser_ret = IParser->getVideoFrameRate(handle,track_index,&stream->info.video.fps_n,
      &stream->info.video.fps_d);

  if(parser_ret != PARSER_SUCCESS)
      goto bail;

  if ((stream->info.video.fps_n == 0) || (stream->info.video.fps_d == 0)) {
    stream->info.video.fps_n = AIURDEMUX_FRAME_N_DEFAULT;
    stream->info.video.fps_d = AIURDEMUX_FRAME_D_DEFAULT;
  }

  stream->send_codec_data = TRUE;

  for(i = 0; i < sizeof(aiurdemux_videocodec_tab)/sizeof(AiurdemuxCodecStruct); i++){
      codec_struct = &aiurdemux_videocodec_tab[i];
      if (stream->codec_type == codec_struct->type){
          if((codec_struct->subtype > 0) && (stream->codec_sub_type == codec_struct->subtype)){
              mime = (gchar *)codec_struct->mime;
              codec = (gchar *)codec_struct->codec_str;
              break;
          }else if(codec_struct->subtype == 0){
              mime = (gchar *)codec_struct->mime;
              codec = (gchar *)codec_struct->codec_str;
              break;
          }
      }
  }

  if(mime == NULL)
      goto bail;

  if(stream->codec_type == VIDEO_H264){
    if(stream->codec_data.length > 0 && stream->codec_data.codec_data != NULL){
      mime = g_strdup_printf
    ("%s, stream-format=(string)avc, width=(int)%ld, height=(int)%ld, framerate=(fraction)%ld/%ld",
    mime, stream->info.video.width, stream->info.video.height,
    stream->info.video.fps_n, stream->info.video.fps_d);
      stream->send_codec_data = TRUE;
    }else{
      mime =
    g_strdup_printf
    ("%s, stream-format=(string)byte-stream, width=(int)%ld, height=(int)%ld, framerate=(fraction)%ld/%ld",
    mime, stream->info.video.width, stream->info.video.height,
    stream->info.video.fps_n, stream->info.video.fps_d);
      stream->send_codec_data = FALSE;
    }
  }else{
  mime =
    g_strdup_printf
    ("%s, width=(int)%ld, height=(int)%ld, framerate=(fraction)%ld/%ld",
    mime, stream->info.video.width, stream->info.video.height,
    stream->info.video.fps_n, stream->info.video.fps_d);
  }
  stream->caps = gst_caps_from_string (mime);
  g_free (mime);

  if (stream->pid < 0) {
      stream->pid = demux->n_video_streams;
  }

  padname = g_strdup_printf ("video_%u", stream->pid);

  GST_INFO ("Create video pad %s \r\n", padname);

  stream->pad =
    gst_pad_new_from_static_template (&gst_aiurdemux_videosrc_template,
    padname);
  g_free (padname);

  demux->n_video_streams++;

  stream->pending_tags = gst_tag_list_new (GST_TAG_CODEC, codec, NULL);

  if (stream->lang[0] != '\0') {
  gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
      GST_TAG_LANGUAGE_CODE, stream->lang, NULL);

  }

  if (stream->bitrate) {
  gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
      GST_TAG_BITRATE, stream->bitrate, NULL);
  }

  if (demux->tag_list) {

    gst_tag_list_add (demux->tag_list, GST_TAG_MERGE_REPLACE, GST_TAG_VIDEO_CODEC,
        codec, NULL);

    if ((stream->info.video.fps_n > 0) && (stream->info.video.fps_d > 0)) {
        gst_tag_list_add (demux->tag_list, GST_TAG_MERGE_REPLACE, GST_TAG_VIDEO_FRAMERATE,
            stream->info.video.fps_n / stream->info.video.fps_d, NULL);
    }

    if (stream->bitrate) {
        gst_tag_list_add (demux->tag_list, GST_TAG_MERGE_REPLACE, GST_TAG_VIDEO_BITRATE,
            stream->bitrate, NULL);
    }

    gst_tag_list_add (demux->tag_list, GST_TAG_MERGE_REPLACE, GST_TAG_VIDEO_WIDTH,
      stream->info.video.width, NULL);
    gst_tag_list_add (demux->tag_list, GST_TAG_MERGE_REPLACE, GST_TAG_VIDEO_HEIGHT,
      stream->info.video.height, NULL);

  }

  return;


bail:
    GST_WARNING ("Unknown Video code-type=%d, sub-type=%d", stream->codec_type, stream->codec_sub_type);
    return;
}

static void aiurdemux_parse_audio (GstAiurDemux * demux, AiurDemuxStream * stream,
    gint track_index)
{
  gchar *mime = NULL;
  const gchar *codec = NULL;
  const gchar *codec_mime = NULL;
  const gchar *stream_type = NULL;
  gchar *padname;
  
  int32 parser_ret = PARSER_SUCCESS;
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;


  memset(&stream->info.audio, 0, sizeof(AiurDemuxAudioInfo));

  parser_ret = IParser->getAudioNumChannels(handle,track_index,&stream->info.audio.n_channels);

  if(parser_ret != PARSER_SUCCESS)
      goto bail;

  parser_ret = IParser->getAudioSampleRate(handle,track_index,&stream->info.audio.rate);

  if(parser_ret != PARSER_SUCCESS)
      goto bail;

  if(IParser->getAudioBitsPerSample)
  parser_ret = IParser->getAudioBitsPerSample(handle,track_index,&stream->info.audio.sample_width);

  if(parser_ret != PARSER_SUCCESS)
      goto bail;

  if (stream->info.audio.n_channels == 0) {
      stream->info.audio.n_channels = AIURDEMUX_AUDIO_CHANNEL_DEFAULT;
  }
  if (stream->info.audio.rate == 0) {
      stream->info.audio.rate = AIURDEMUX_AUDIO_SAMPLERATE_DEFAULT;
  }
  if (stream->info.audio.sample_width == 0) {
      stream->info.audio.sample_width = AIURDEMUX_AUDIO_SAMPLEWIDTH_DEFAULT;
  }

  stream->send_codec_data = TRUE;
    
 switch (stream->codec_type) {
      case AUDIO_AAC:
        codec_mime = "audio/mpeg, mpegversion=(int)4";
        codec = "AAC";
        stream->send_codec_data= TRUE;
        switch (stream->codec_sub_type) {
          case AUDIO_AAC_ADTS:
            stream_type = "adts";
            break;
          case AUDIO_AAC_ADIF:
            stream_type = "adif";
            break;
          case AUDIO_AAC_RAW:
            stream_type = "raw";
            break;
          default:
            stream_type = NULL;
            break;
        }
        if (stream_type) {
          mime =
              g_strdup_printf
              ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld, framed=(boolean)true, stream-format=%s",
              codec_mime, stream->info.audio.n_channels,
              stream->info.audio.rate, stream->bitrate, stream_type);
        } else {
          mime =
              g_strdup_printf
              ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld, framed=(boolean)true",
              codec_mime, stream->info.audio.n_channels,
              stream->info.audio.rate, stream->bitrate);
        }
        break;
      case AUDIO_MPEG2_AAC:
        codec_mime = "audio/mpeg, mpegversion=(int)2, framed=(boolean)true";
        codec = "AAC";
        stream->send_codec_data = TRUE;
        mime =
            g_strdup_printf
            ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld", codec_mime,
            stream->info.audio.n_channels, stream->info.audio.rate,
            stream->bitrate);
        break;
      case AUDIO_MP3:
        codec_mime =
            "audio/mpeg, mpegversion=(int)1, framed=(boolean)true";
        codec = "MP3";
        mime =
            g_strdup_printf
            ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld", codec_mime,
            stream->info.audio.n_channels, stream->info.audio.rate,
            stream->bitrate);
        break;
      case AUDIO_AC3:
        codec_mime = "audio/x-ac3";
        codec = "AC3";
        mime =
            g_strdup_printf
            ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld, framed=(boolean)true",
            codec_mime, stream->info.audio.n_channels, stream->info.audio.rate,
            stream->bitrate);
        break;
      case AUDIO_WMA:
          parser_ret = IParser->getAudioBlockAlign(handle,track_index,&stream->info.audio.block_align);

          if(parser_ret != PARSER_SUCCESS)
              goto bail;
        switch (stream->codec_sub_type) {
          case AUDIO_WMA1:
            codec_mime = "audio/x-wma, wmaversion=(int)1";
            codec = "WMA7";
            break;
          case AUDIO_WMA2:
            codec_mime = "audio/x-wma, wmaversion=(int)2";
            codec = "WMA8";
            break;
          case AUDIO_WMA3:
            codec_mime = "audio/x-wma, wmaversion=(int)3";
            codec = "WMA9";
            break;
          default:
            goto bail;
            break;
        }
        stream->send_codec_data = TRUE;
        mime =
            g_strdup_printf
            ("%s, channels=(int)%ld, rate=(int)%ld, block_align=(int)%ld, depth=(int)%ld, bitrate=(int)%ld",
            codec_mime, stream->info.audio.n_channels, stream->info.audio.rate,
            stream->info.audio.block_align, stream->info.audio.sample_width,
            stream->bitrate);
        break;
      case AUDIO_PCM:
      {
        int width, depth, endian;
        gboolean sign = TRUE;
        switch (stream->codec_sub_type) {
          case AUDIO_PCM_U8:
            width = depth = 8;
            endian = G_BYTE_ORDER;
            sign = FALSE;
            codec_mime = "format=(string)U8";
            break;
          case AUDIO_PCM_S16LE:
            width = depth = 16;
            endian = G_LITTLE_ENDIAN;
            codec_mime = "format=(string)S16LE";
            break;
          case AUDIO_PCM_S24LE:
            width = depth = 24;
            endian = G_LITTLE_ENDIAN;
            codec_mime = "format=(string)S24LE";
            break;
          case AUDIO_PCM_S32LE:
            width = depth = 32;
            endian = G_LITTLE_ENDIAN;
            codec_mime = "format=(string)S32LE";
            break;
          case AUDIO_PCM_S16BE:
            width = depth = 16;
            endian = G_BIG_ENDIAN;
            codec_mime = "format=(string)S16BE";
            break;
          case AUDIO_PCM_S24BE:
            width = depth = 24;
            endian = G_BIG_ENDIAN;
            codec_mime = "format=(string)S24BE";
            break;
          case AUDIO_PCM_S32BE:
            width = depth = 32;
            endian = G_BIG_ENDIAN;
            codec_mime = "format=(string)S32BE";
            break;
          default:
            goto bail;
            break;
        }
        codec = "PCM";
        mime =
            g_strdup_printf
            ("audio/x-raw, %s,channels=(int)%ld, layout=(string)interleaved, rate=(int)%ld,bitrate=(int)%ld",codec_mime,
            stream->info.audio.n_channels, stream->info.audio.rate,stream->bitrate);
      }
        break;
      case AUDIO_REAL:
        switch (stream->codec_sub_type) {
          case REAL_AUDIO_RAAC:
            mime =
                g_strdup_printf
                ("audio/mpeg, mpegversion=(int)4, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
                stream->info.audio.n_channels, stream->info.audio.rate,
                stream->bitrate);
            codec = "AAC";
            break;
          default:
          {
            uint32 frame_bit;
            parser_ret = IParser->getAudioBitsPerFrame(handle,track_index,&frame_bit);
            if(parser_ret != PARSER_SUCCESS)
              goto bail;
            mime =
                g_strdup_printf
                ("audio/x-pn-realaudio, channels=(int)%ld, rate=(int)%ld, frame_bit=(int)%ld",
                stream->info.audio.n_channels, stream->info.audio.rate,
                frame_bit);
            codec = "RealAudio";
            stream->send_codec_data = TRUE;
          }
            break;
        }
  
        break;
      case AUDIO_VORBIS:
        codec = "VORBIS";
        mime =
            g_strdup_printf
            ("audio/x-vorbis, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld, framed=(boolean)true",
            stream->info.audio.n_channels, stream->info.audio.rate,
            stream->bitrate);
        if(demux->option.disable_vorbis_codec_data){
          stream->send_codec_data = FALSE;
        }else{
        stream->send_codec_data = TRUE;
        }
        break;
      case AUDIO_FLAC:
        codec = "FLAC";
        mime =
            g_strdup_printf
            ("audio/x-flac, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
            stream->info.audio.n_channels, stream->info.audio.rate,
            stream->bitrate);
        break;
      case AUDIO_DTS:
        codec = "DTS";
        mime =
            g_strdup_printf
            ("audio/x-dts, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
            stream->info.audio.n_channels, stream->info.audio.rate,
            stream->bitrate);
        break;
      case AUIDO_SPEEX:
        codec = "SPEEX";
        mime =
            g_strdup_printf
            ("audio/x-speex, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
            stream->info.audio.n_channels, stream->info.audio.rate,
            stream->bitrate);
        break;
      case AUDIO_AMR:
        switch (stream->codec_sub_type) {
          case AUDIO_AMR_NB:
            codec_mime = "audio/AMR";
            codec = "AMR-NB";
            if (stream->info.audio.rate != 8000) {
              GST_WARNING
                  ("amr-nb should using 8k rate, this maybe a core parser BUG!");
              stream->info.audio.rate = 8000;
            }
            break;
          case AUDIO_AMR_WB:
            codec_mime = "audio/AMR-WB";
            codec = "AMR-WB";
            break;
          default:
            goto bail;
            break;
        }
        stream->send_codec_data = TRUE;
        //only support one channel amr
        stream->info.audio.n_channels = 1;
        mime =
            g_strdup_printf
            ("%s, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
            codec_mime, stream->info.audio.n_channels, stream->info.audio.rate,
            stream->info.audio.sample_width, stream->bitrate);
        break;
      case AUDIO_EC3:
        codec = "Dobly Digital Plus (E-AC3)";
        mime =
            g_strdup_printf
            ("audio/eac3, channels=(int)%ld, rate=(int)%ld, bitrate=(int)%ld",
            stream->info.audio.n_channels, stream->info.audio.rate,
            stream->bitrate);
        break;
       default:
        break;
    }

    if(mime == NULL)
      goto bail;

    stream->caps = gst_caps_from_string (mime);
    g_free (mime);
  
    if (stream->pid < 0) {
      stream->pid = demux->n_audio_streams;
    }
  
    padname = g_strdup_printf ("audio_%u", stream->pid);
  
    GST_INFO ("Create audio pad %s", padname);
  
    stream->pad =
        gst_pad_new_from_static_template (&gst_aiurdemux_audiosrc_template,
        padname);
    g_free (padname);
  
    demux->n_audio_streams++;
  
    stream->pending_tags = gst_tag_list_new (GST_TAG_CODEC, codec, NULL);

    if (stream->lang[0] != '\0') {
      gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
          GST_TAG_LANGUAGE_CODE, stream->lang, NULL);
    }
  
    if (stream->bitrate) {
      gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
          GST_TAG_BITRATE, stream->bitrate, NULL);
    }
    
    if (demux->tag_list) {
        gst_tag_list_add (demux->tag_list,
            GST_TAG_MERGE_REPLACE, GST_TAG_AUDIO_CODEC, codec, NULL);
    }

  return;

bail:
  GST_WARNING ("Unknown Audio code-type=%d, sub-type=%d",
      stream->codec_type, stream->codec_sub_type);

  return;
}

static void aiurdemux_parse_text (GstAiurDemux * demux, AiurDemuxStream * stream,
  gint track_index)
{
  gchar *mime = NULL;
  const gchar *codec_mime = NULL;
  gchar *padname;
  
  int32 parser_ret = PARSER_SUCCESS;
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;


  parser_ret = IParser->getTextTrackWidth(handle,track_index,&stream->info.subtitle.width);

  if(parser_ret != PARSER_SUCCESS)
      goto bail;
  
  parser_ret = IParser->getTextTrackHeight(handle,track_index,&stream->info.subtitle.height);

  if(parser_ret != PARSER_SUCCESS)
      goto bail;

  
    switch (stream->codec_type) {
      case TXT_DIVX_FEATURE_SUBTITLE:
        codec_mime = "video/x-avi-unknown, fourcc=(fourcc)DXSB";
        mime =
            g_strdup_printf ("%s, width=(int)%ld, height=(int)%ld", codec_mime,
            stream->info.subtitle.width, stream->info.subtitle.height);
        break;

      default:
        goto bail;
    }
    
    stream->caps = gst_caps_from_string (mime);
    g_free (mime);
  
    if (stream->pid < 0) {
      stream->pid = demux->n_sub_streams;
    }
  
    padname = g_strdup_printf ("subtitle_%u", stream->pid);
  
    GST_INFO ("Create video pad %s", padname);
  
    stream->pad =
        gst_pad_new_from_static_template (&gst_aiurdemux_subsrc_template,
        padname);
    g_free (padname);
  
    stream->pending_tags = gst_tag_list_new (GST_TAG_SUBTITLE_CODEC,mime,NULL);
    if (stream->lang[0] != '\0') {
      gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
          GST_TAG_LANGUAGE_CODE, stream->lang, NULL);
    }
  
    demux->n_sub_streams++;
  
  bail:
  
    GST_WARNING ("Unknown Text code-type=%d, sub-type=%d",
        stream->codec_type, stream->codec_sub_type);
    return;

}
static void aiurdemux_check_interleave_stream_eos (GstAiurDemux * demux)
{
  int n;
  gint track_num = 0;
  gint64 min_time = -1;
  AiurDemuxStream *stream;
  AiurDemuxStream *min_stream = NULL;
  gboolean bQueueFull = FALSE;
  if(demux->n_streams <= 1)
      return;
  for (n = 0; n < demux->n_streams; n++) {
      stream = demux->streams[n];
      if (!stream->valid) {
        continue;
      }
      if ((demux->interleave_queue_size)
          && (stream->buf_queue_size > demux->interleave_queue_size)) {
        GST_LOG_OBJECT(demux,"bQueueFull ");
        bQueueFull = TRUE;
      }
      if(min_time < 0){
          min_time = stream->last_stop;
          track_num = stream->track_idx;
          min_stream = stream;
      }else if (min_time > 0 && stream->last_stop < min_time ) {
        min_time = stream->last_stop;
        track_num = stream->track_idx;
        min_stream = stream;
      }
  }
  if(bQueueFull && (min_stream->buf_queue) && (g_queue_is_empty (min_stream->buf_queue))){
    MARK_STREAM_EOS (demux, min_stream);
    GST_ERROR_OBJECT(demux,"file mode interleave detect !!! send EOS to track%d",track_num);
  }
  return;

}

static void
_gst_buffer_copy_into_mem (GstBuffer * dest, gsize offset, const guint8 * src,
    gsize size)
{
  gsize bsize;

  g_return_if_fail (gst_buffer_is_writable (dest));

  bsize = gst_buffer_get_size (dest);
  g_return_if_fail (bsize >= offset + size);

  gst_buffer_fill (dest, offset, src, size);
}

static GstFlowReturn aiurdemux_read_buffer (GstAiurDemux * demux, uint32* track_idx, AiurDemuxStream** stream_out) 
{
  GstFlowReturn ret = GST_FLOW_OK;
  int32 parser_ret = PARSER_ERR_UNKNOWN;
  GstBuffer *gstbuf;
  uint8 *buffer;
  uint32 buffer_size;
  uint64 usStartTime;
  uint64 usDuration;
  uint32 direction;

  uint32 sampleFlags = 0;
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  AiurDemuxStream *stream = NULL;


  do{

    gstbuf = NULL;
    buffer = NULL;
    buffer_size = 0;
    usStartTime = 0;
    usDuration = 0;
    sampleFlags = 0;

    if(demux->play_mode != AIUR_PLAY_MODE_NORMAL){
        if (demux->play_mode == AIUR_PLAY_MODE_TRICK_FORWARD) {
            direction = FLAG_FORWARD;
        } else {
            direction = FLAG_BACKWARD;
        }
    }

    if (demux->read_mode == PARSER_READ_MODE_FILE_BASED) {
        
        if (demux->play_mode == AIUR_PLAY_MODE_NORMAL) {

            parser_ret = IParser->getFileNextSample(handle,track_idx,&buffer,(void *) (&gstbuf),
                &buffer_size,&usStartTime, &usDuration, &sampleFlags);
            
        }else {

            parser_ret = IParser->getFileNextSyncSample(handle, direction, track_idx, &buffer,
                (void *) (&gstbuf), &buffer_size, &usStartTime, &usDuration, &sampleFlags);
        }

    }else{
    
        if (demux->play_mode == AIUR_PLAY_MODE_NORMAL) {
            parser_ret = IParser->getNextSample(handle,(uint32)(*track_idx),&buffer,(void *) (&gstbuf),
                            &buffer_size,&usStartTime, &usDuration, &sampleFlags);

        }else{
            parser_ret = IParser->getNextSyncSample(handle, direction, (uint32)(*track_idx), &buffer,
                (void *) (&gstbuf), &buffer_size, &usStartTime, &usDuration, &sampleFlags);

        }
    }
    
    GST_LOG_OBJECT(demux,"aiurdemux_read_buffer ret=%d,track=%d,size=%d,time=%lld,duration=%lld,flag=%x",
        parser_ret,*track_idx,buffer_size,usStartTime,usDuration,sampleFlags);
    stream = aiurdemux_trackidx_to_stream (demux, *track_idx);


    if((parser_ret == PARSER_EOS) || (PARSER_BOS == parser_ret)
        || (PARSER_READ_ERROR == parser_ret)){
        if (demux->read_mode == PARSER_READ_MODE_FILE_BASED) {
            aiurdemux_send_stream_eos_all (demux);
            ret = GST_FLOW_EOS;
            goto beach;
        }else{
            aiurdemux_send_stream_eos (demux, stream);
            if (demux->valid_mask == 0) {
            ret = GST_FLOW_EOS;
            goto beach;
            }
            if(PARSER_READ_ERROR == parser_ret)
                goto beach;
        }
    } else if (PARSER_ERR_INVALID_MEDIA == parser_ret) {
      GST_WARNING ("Movie parser interrupt, track_idx %d, error = %d",
          *track_idx, parser_ret);
      if (stream) {
        aiurdemux_send_stream_eos (demux, stream);
        if (demux->valid_mask == 0) {
          ret = GST_FLOW_EOS;
        }
      } else {
        aiurdemux_send_stream_eos_all (demux);
        ret = GST_FLOW_EOS;
      }
      goto beach;
    } else if (PARSER_SUCCESS != parser_ret) {
      GST_ERROR ("Movie parser failed, error = %d", parser_ret);
      aiurdemux_send_stream_eos_all (demux);
      ret = GST_FLOW_ERROR;
      goto beach;
    }

    if(gstbuf && gst_buffer_get_size(gstbuf) != buffer_size){
        gst_buffer_set_size(gstbuf,buffer_size);
    }
  

    AIUR_UPDATE_SAMPLE_STAT (stream->sample_stat,
      AIUR_CORETS_2_GSTTS (usStartTime),
      AIUR_COREDURATION_2_GSTDURATION (usDuration), sampleFlags);
   if(!(sampleFlags & FLAG_SAMPLE_NOT_FINISHED) && stream->partial_sample == FALSE){
          stream->buffer = gstbuf;
          GST_LOG_OBJECT(demux,"get stream buffer 1");
   }else{

      if(buffer_size > 0){
        stream->adapter_buffer_size += gst_buffer_get_size(gstbuf);
        gst_adapter_push (stream->adapter, gstbuf);
        gstbuf = NULL;
        GST_LOG_OBJECT(demux,"push to adapter 2 ");
      }

    if (sampleFlags & FLAG_SAMPLE_NOT_FINISHED) {
      stream->partial_sample = TRUE;
    }else{
      gint sample_size = 0;
      stream->partial_sample = FALSE;
      sample_size = gst_adapter_available (stream->adapter);
      if(sample_size > 0){
        stream->buffer = gst_adapter_take_buffer (stream->adapter, stream->adapter_buffer_size);

        stream->adapter_buffer_size = 0;
        gst_adapter_clear (stream->adapter);
            GST_LOG_OBJECT(demux,"get stream buffer 2");
        }
      
        }
        
      }
  
readend:
  ;
  }while(sampleFlags & FLAG_SAMPLE_NOT_FINISHED);
  if(stream->buffer){
    if (!(stream->sample_stat.flag & FLAG_SYNC_SAMPLE)) {
      GST_BUFFER_FLAG_SET (stream->buffer, GST_BUFFER_FLAG_DELTA_UNIT);
    }

  if (demux->play_mode != AIUR_PLAY_MODE_NORMAL && stream->buffer) {
      GST_BUFFER_FLAG_SET (stream->buffer, GST_BUFFER_FLAG_DISCONT);
    }
  }
  
  if (stream->sample_stat.start == stream->last_start
      && !(stream->type == MEDIA_VIDEO && stream->codec_type == VIDEO_ON2_VP)) {
    stream->sample_stat.start = GST_CLOCK_TIME_NONE;
  }

  *stream_out = stream;
  return ret;
beach:
  if (stream) {
    if (stream->buffer) {
      gst_buffer_unref (stream->buffer);
      stream->buffer = NULL;
    }
  
      gst_adapter_clear (stream->adapter);
  
      stream->sample_stat.duration = 0;
      stream->sample_stat.start = GST_CLOCK_TIME_NONE;
      stream->sample_stat.flag = 0;

  }
  return ret;

}
static GstFlowReturn aiurdemux_parse_vorbis_codec_data(GstAiurDemux * demux, AiurDemuxStream* stream)
{
  int i = 0;
  int num = 0;
  int len = 0;
  int offset = 0;
  num = (int)*(stream->codec_data.codec_data);
  offset = num+1;
  if(demux == NULL || stream == NULL)
    return GST_FLOW_ERROR;
  if(stream->codec_data.length == 0 || stream->codec_data.codec_data == NULL)
    return GST_FLOW_ERROR;
  GST_DEBUG_OBJECT (demux, "vorbis lace num=%d",num);
  for( i = 1; i <= num; i++){
    GstBuffer *gstbuf;
    len = (int)*(stream->codec_data.codec_data+i);
    gstbuf = gst_buffer_new_and_alloc (len);
    gst_buffer_fill(gstbuf,0,(guint8 *)stream->codec_data.codec_data+offset,len);
    gst_pad_push (stream->pad, gstbuf);
    offset +=len;
    GST_DEBUG_OBJECT (demux, "push vorbis codec buffer %d len=%d",i,len);
  }
  if(offset < stream->codec_data.length){
    GstBuffer *gstbuf;
    len = stream->codec_data.length - offset;
    gstbuf = gst_buffer_new_and_alloc (len);
    gst_buffer_fill(gstbuf,0,(guint8 *)stream->codec_data.codec_data+offset,len);
    gst_pad_push (stream->pad, gstbuf);
    GST_DEBUG_OBJECT (demux, "push vorbis codec buffer last len=%d",len);
  }

  return GST_FLOW_OK;
}

static gint aiurdemux_choose_next_stream (GstAiurDemux * demux)
{
  int n;
  gint track_index = 0;
  gint64 min_time = -1;
  AiurDemuxStream *stream;
  for (n = 0; n < demux->n_streams; n++) {
    stream = demux->streams[n];
    if (!stream->valid) {
      continue;
    }
    if ((demux->read_mode == PARSER_READ_MODE_TRACK_BASED)
        && (stream->partial_sample)) {
      track_index = stream->track_idx;
      break;
    }
  
    if ((demux->interleave_queue_size)
        && (stream->buf_queue_size > demux->interleave_queue_size)) {
      track_index = stream->track_idx;
      break;
    }
  
    if (min_time >= 0) {
      if (stream->last_stop < min_time) {
        min_time = stream->last_stop;
        track_index = stream->track_idx;
      }
    } else {
      min_time = stream->last_stop;
      track_index = stream->track_idx;
    }
  
  }
  
  return track_index;

}
static AiurDemuxStream* aiurdemux_trackidx_to_stream (GstAiurDemux * demux, guint32 stream_idx)
{
  AiurDemuxStream *stream = NULL;
  int i;
  for (i = 0; i < demux->n_streams; i++) {
    if (demux->streams[i]->track_idx == stream_idx) {
      stream = demux->streams[i];
      break;
    }
  }
  return stream;
}

static void
aiurdemux_check_start_offset (GstAiurDemux * demux, AiurDemuxStream * stream)
{

    //clock of pipeline has started when first buffer arrives, so adjust the buffer timestamp
    if(demux->clock_offset == GST_CLOCK_TIME_NONE){
        GstClockTime base_time,now;
        GstClock *clock = NULL;
        GstClockTimeDiff offset = 0;
        base_time = GST_ELEMENT_CAST (demux)->base_time;
        clock = GST_ELEMENT_CLOCK (demux);
        if(clock != NULL){
            now = gst_clock_get_time (clock);
            offset = now - base_time;
            if(offset > 0)
                demux->clock_offset = offset;
        }
        GST_LOG_OBJECT (demux,"basetime=%"GST_TIME_FORMAT,GST_TIME_ARGS (base_time));
        GST_LOG_OBJECT (demux,"now=%"GST_TIME_FORMAT,GST_TIME_ARGS (now));
        GST_LOG_OBJECT (demux,"clock_offset=%"GST_TIME_FORMAT,GST_TIME_ARGS (demux->clock_offset));
    }
    if(stream->last_stop == 0 && (GST_CLOCK_TIME_IS_VALID (demux->clock_offset))){
        stream->last_stop = demux->clock_offset;
        GST_LOG_OBJECT (demux,"last_stop =%"GST_TIME_FORMAT,GST_TIME_ARGS (stream->last_stop));
    }
    if(demux->start_time == GST_CLOCK_TIME_NONE){
        demux->start_time = stream->sample_stat.start;
        GST_LOG_OBJECT (demux,"start_time=%"GST_TIME_FORMAT,GST_TIME_ARGS (demux->start_time));
    }
    if((GST_CLOCK_TIME_IS_VALID (demux->clock_offset))
        && (GST_CLOCK_TIME_IS_VALID (demux->start_time))
        && (GST_CLOCK_TIME_IS_VALID (stream->sample_stat.start))){
        stream->sample_stat.start = stream->sample_stat.start - demux->start_time + demux->clock_offset + (GST_MSECOND * demux->option.streaming_latency);
        GST_LOG_OBJECT (demux,"***start=%"GST_TIME_FORMAT,GST_TIME_ARGS (stream->sample_stat.start));
    }
}
static void
aiurdemux_adjust_timestamp (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstBuffer * buffer)
{

  //g_print("adjust orig %"GST_TIME_FORMAT" base %"GST_TIME_FORMAT" pos %"GST_TIME_FORMAT"\n",
  //     GST_TIME_ARGS(stream->sample_stat.start), GST_TIME_ARGS(demux->base_offset), GST_TIME_ARGS(stream->time_position));
  buffer = gst_buffer_make_writable (buffer);

  if ((demux->base_offset == 0)
      || (!GST_CLOCK_TIME_IS_VALID (stream->sample_stat.start))) {
    GST_BUFFER_TIMESTAMP (buffer) = stream->sample_stat.start;
  } else {
    GST_BUFFER_TIMESTAMP (buffer) =
        ((stream->sample_stat.start >=
            demux->base_offset) ? (stream->sample_stat.start -
            demux->base_offset) : 0);

    if ((GST_BUFFER_TIMESTAMP (buffer) < (stream->time_position))) {
      GST_BUFFER_TIMESTAMP (buffer) = stream->time_position;
    }
  }

  GST_BUFFER_DTS(buffer) = GST_BUFFER_TIMESTAMP (buffer);

  GST_BUFFER_DURATION (buffer) = stream->sample_stat.duration;
  GST_BUFFER_OFFSET (buffer) = -1;
  GST_BUFFER_OFFSET_END (buffer) = -1;


}
static void
aiurdemux_update_stream_position (GstAiurDemux * demux,
    AiurDemuxStream * stream, GstBuffer * buffer)
{

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buffer))) {

    stream->last_stop = stream->last_start = GST_BUFFER_TIMESTAMP (buffer);
    stream->last_stop += GST_BUFFER_DURATION (buffer);
    /* sample duration is wrong sometimes, so using the last_start here to
     * compare with clip duration */
    if (demux->n_video_streams > 0) {
      if ((MEDIA_VIDEO == stream->type) &&
          (GST_BUFFER_TIMESTAMP (buffer) > demux->movie_duration)) {
        demux->movie_duration = GST_BUFFER_TIMESTAMP (buffer);
      }
    } else {
      if (GST_BUFFER_TIMESTAMP (buffer) > demux->movie_duration) {
        demux->movie_duration = GST_BUFFER_TIMESTAMP (buffer);
      }
    }
  }
}

static void
aiurdemux_send_stream_newsegment (GstAiurDemux * demux,
    AiurDemuxStream * stream)
{
    GstSegment segment;
    gst_segment_init (&segment, GST_FORMAT_TIME);

  if (demux->segment.rate >= 0) {

    
    if (stream->buffer) {

      if ((GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (stream->buffer)))
          && (GST_BUFFER_TIMESTAMP (stream->buffer) > stream->time_position)) {
        GST_WARNING ("Timestamp unexpect, maybe a core parser bug!");
        if (demux->n_video_streams == 0) {
          stream->time_position = GST_BUFFER_TIMESTAMP (stream->buffer);
        }
      }

      GST_WARNING ("Pad %s: Send newseg %" GST_TIME_FORMAT " first buffer %"
          GST_TIME_FORMAT " ", AIUR_MEDIATYPE2STR (stream->type),
          GST_TIME_ARGS (stream->time_position),
          GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (stream->buffer)));

    }

    segment.format = GST_FORMAT_TIME;
    segment.rate = demux->segment.rate;
    segment.start = stream->time_position;
    segment.stop = GST_CLOCK_TIME_NONE;
    segment.position = segment.time = stream->time_position;
    gst_pad_push_event (stream->pad, gst_event_new_segment (&segment));
  } else {
    if(stream->type == MEDIA_AUDIO){
        stream->new_segment = FALSE;
        return;
    }
    if (stream->buffer) {
      GST_WARNING ("Pad %s: Send newseg %" GST_TIME_FORMAT " first buffer %"
          GST_TIME_FORMAT " ", AIUR_MEDIATYPE2STR (stream->type),
          GST_TIME_ARGS (stream->time_position),
          GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (stream->buffer)));
    }
    segment.format = GST_FORMAT_TIME;
    segment.rate = demux->segment.rate;
    segment.start = 0;
    segment.stop = stream->time_position;
    segment.position = segment.time = 0;
    gst_pad_push_event (stream->pad, gst_event_new_segment (&segment));
  }
  stream->new_segment = FALSE;

}
static GstFlowReturn
aiurdemux_send_stream_eos (GstAiurDemux * demux, AiurDemuxStream * stream)
{
  GstFlowReturn ret = GST_FLOW_OK;

  if (stream) {
    if (stream->new_segment) {
      aiurdemux_send_stream_newsegment (demux, stream);
    }

    ret = gst_pad_push_event (stream->pad, gst_event_new_eos ());

    stream->valid = FALSE;
    demux->valid_mask &= (~stream->mask);

    GST_WARNING ("Pad %s: Send eos. ", AIUR_MEDIATYPE2STR (stream->type));
  }

  return ret;
}

static GstFlowReturn
aiurdemux_send_stream_eos_all (GstAiurDemux * demux)
{
  GstFlowReturn ret = GST_FLOW_OK;
  AiurDemuxStream *stream;
  gint n;
  if (demux->interleave_queue_size) {
    while (demux->valid_mask) {
      gint track = aiurdemux_choose_next_stream (demux);
      stream = aiurdemux_trackidx_to_stream (demux, track);
      if (stream) {
        GstBuffer *gstbuf;
        //GstFlowReturn ret;
        gstbuf = g_queue_pop_head (stream->buf_queue);
        if (gstbuf) {
          stream->buf_queue_size -= gst_buffer_get_size(gstbuf);//GST_BUFFER_SIZE (gstbuf);
          aiurdemux_push_pad_buffer (demux, stream, gstbuf);
        } else {
          aiurdemux_send_stream_eos (demux, stream);
        }
      } else {
        break;
      }
    }
  } else {

    for (n = 0; n < demux->n_streams; n++) {
      stream = demux->streams[n];

      if ((stream->valid) && (stream->type == MEDIA_AUDIO)) {
        aiurdemux_send_stream_eos (demux, stream);
      }

    }

    for (n = 0; n < demux->n_streams; n++) {
      stream = demux->streams[n];

      if (stream->valid) {
        aiurdemux_send_stream_eos (demux, stream);
      }

    }
  }

  return ret;
}


static GstFlowReturn aiurdemux_push_pad_buffer (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstBuffer * buffer)
{
  GstFlowReturn ret;

  aiurdemux_update_stream_position (demux, stream, buffer);

  if (stream->block) {
    if ((GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buffer)))
        && (GST_BUFFER_TIMESTAMP (buffer) +
            GST_BUFFER_DURATION (buffer) <
            stream->time_position)) {
      gst_buffer_unref(buffer);
      ret = GST_FLOW_OK;
      GST_LOG("drop %s buffer for block",AIUR_MEDIATYPE2STR (stream->type));
      goto bail;
    }
    stream->block = FALSE;
  }

  GST_DEBUG_OBJECT (demux,"%s push sample %" GST_TIME_FORMAT " size %d is discont: %d is delta unit: %d",
      AIUR_MEDIATYPE2STR (stream->type),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)), gst_buffer_get_size (buffer), \
      GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT), \
      GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT));

  ret = gst_pad_push (stream->pad, buffer);

  if ((ret != GST_FLOW_OK)) {
    GST_WARNING ("Pad %s push error type %d",
        AIUR_MEDIATYPE2STR (stream->type), ret);
    if (ret == GST_FLOW_EOS) {
      aiurdemux_send_stream_eos (demux, stream);
      if (demux->valid_mask == 0) {
        ret = GST_FLOW_EOS;
        goto bail;
      }

    }
    if (ret < GST_FLOW_EOS) {
        goto bail;
    }
  }

  ret = GST_FLOW_OK;
bail:
  return ret;

}
static GstFlowReturn
aiurdemux_combine_flows (GstAiurDemux * demux, AiurDemuxStream * stream,
    GstFlowReturn ret)
{
  gint i;
  gboolean unexpected = FALSE, not_linked = TRUE;
  stream->last_ret = ret;
  if (G_LIKELY (ret != GST_FLOW_EOS && ret != GST_FLOW_NOT_LINKED))
    goto done;

  for (i = 0; i < demux->n_streams; i++) {
    AiurDemuxStream *ostream = demux->streams[i];
    ret = ostream->last_ret;
    if (G_LIKELY (ret != GST_FLOW_EOS && ret != GST_FLOW_NOT_LINKED))
      goto done;
    unexpected |= (ret == GST_FLOW_EOS);
    not_linked &= (ret == GST_FLOW_NOT_LINKED);
  }

  if (not_linked)
    ret = GST_FLOW_NOT_LINKED;
  else if (unexpected)
    ret = GST_FLOW_EOS;
done:
  return ret;
}

static void aiurdemux_reset_stream (GstAiurDemux * demux, AiurDemuxStream * stream)
{
    stream->valid = TRUE;
    stream->new_segment = TRUE;
    stream->last_ret = GST_FLOW_OK;
    stream->last_stop = 0;
    stream->last_start = GST_CLOCK_TIME_NONE;
    stream->pending_eos = FALSE;
    stream->block = FALSE;

    if (stream->buffer) {
      gst_buffer_unref (stream->buffer);
      stream->buffer = NULL;
    }
    
    stream->adapter_buffer_size = 0;
    if (stream->adapter) {
      gst_adapter_clear (stream->adapter);
    }
    
    if (stream->buf_queue) {
        GstBuffer * gbuf; 
        while ((gbuf = g_queue_pop_head ((stream)->buf_queue))) {
            gst_buffer_unref (gbuf);
        };
    }
      
    stream->buf_queue_size = 0;
    
    AIUR_RESET_SAMPLE_STAT(stream->sample_stat);

    

    demux->valid_mask |= stream->mask;

}

static gboolean
gst_aiurdemux_convert_seek (GstPad * pad, GstFormat * format,
    GstSeekType cur_type, gint64 * cur, GstSeekType stop_type, gint64 * stop)
{
  gboolean res;
  GstFormat fmt;

  g_return_val_if_fail (format != NULL, FALSE);
  g_return_val_if_fail (cur != NULL, FALSE);
  g_return_val_if_fail (stop != NULL, FALSE);

  if (*format == GST_FORMAT_TIME)
    return TRUE;

  fmt = GST_FORMAT_TIME;
  res = TRUE;
  if (cur_type != GST_SEEK_TYPE_NONE)
    res = gst_pad_query_convert (pad, *format, *cur, fmt, cur);
  if (res && stop_type != GST_SEEK_TYPE_NONE)
    res = gst_pad_query_convert (pad, *format, *stop, fmt, stop);

  if (res)
    *format = GST_FORMAT_TIME;

  return res;
}


static gboolean
gst_aiurdemux_perform_seek (GstAiurDemux * demux, GstSegment * segment,
    gint accurate)
{
  gint64 desired_offset;
  gint n;
  int32 core_ret = 0;
  gdouble rate = segment->rate;

  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;

  if (rate >= 0) {

    demux->play_mode = AIUR_PLAY_MODE_NORMAL;
    if ((rate > 2.0)
        && (((demux->read_mode == PARSER_READ_MODE_FILE_BASED)
                && (IParser->getFileNextSyncSample))
            || ((demux->read_mode == PARSER_READ_MODE_TRACK_BASED)
                && (IParser->getNextSyncSample)))) {
      demux->play_mode = AIUR_PLAY_MODE_TRICK_FORWARD;
    }
    desired_offset = segment->start;

  } else if (rate < 0) {
    if (((demux->read_mode == PARSER_READ_MODE_FILE_BASED)
            && (IParser->getFileNextSyncSample))
        || ((demux->read_mode == PARSER_READ_MODE_TRACK_BASED)
            && (IParser->getNextSyncSample))) {
      demux->play_mode = AIUR_PLAY_MODE_TRICK_BACKWARD;
      desired_offset = segment->stop;
    } else {
      return FALSE;
    }
  }

  //do not change play mode for pure audio file.
  if((0 == demux->n_video_streams) && (demux->n_audio_streams >= 1) && (rate >= 0)){
    demux->play_mode = AIUR_PLAY_MODE_NORMAL;
  }
  GST_WARNING ("Seek to %" GST_TIME_FORMAT ".", GST_TIME_ARGS (desired_offset));

  demux->pending_event = FALSE;


  demux->valid_mask = 0;

  if ((accurate) || (demux->n_video_streams > 1)
      || (demux->n_video_streams == 0)) {
    /* and set all streams to the final position */
    for (n = 0; n < demux->n_streams; n++) {
      AiurDemuxStream *stream = demux->streams[n];
      guint64 usSeekTime = AIUR_GSTTS_2_CORETS (desired_offset);

      aiurdemux_reset_stream (demux, stream);

      IParser->seek(handle, stream->track_idx,&usSeekTime, SEEK_FLAG_NO_LATER);



      stream->time_position = desired_offset;

      if ((rate >= 0) && (stream->type == MEDIA_AUDIO)
          && (demux->n_video_streams))
        stream->block = TRUE;
      else
        stream->block = FALSE;

      if ((core_ret != PARSER_SUCCESS)
          || ((demux->play_mode != AIUR_PLAY_MODE_NORMAL)
              && (stream->type == MEDIA_AUDIO))) {
        MARK_STREAM_EOS (demux, stream);
      }
    }

  } else {
    AiurDemuxStream *stream = NULL;
    guint64 usSeekTime = AIUR_GSTTS_2_CORETS (desired_offset);
    core_ret = PARSER_SUCCESS;
    
    for (n = 0; n < demux->n_streams; n++) {
      if (demux->streams[n]->type == MEDIA_VIDEO) {
        stream = demux->streams[n];
        break;
      }
    }

    if (stream) {
      aiurdemux_reset_stream (demux, stream);

      IParser->seek(handle, stream->track_idx,
          &usSeekTime, SEEK_FLAG_NO_LATER);
      GST_INFO ("Video seek return %d time %" GST_TIME_FORMAT, core_ret,
          GST_TIME_ARGS (AIUR_CORETS_2_GSTTS (usSeekTime)));
    }
    if (core_ret != PARSER_SUCCESS) {
      MARK_STREAM_EOS (demux, stream);
    } else {
      desired_offset = AIUR_CORETS_2_GSTTS (usSeekTime);
    }
    stream->time_position = desired_offset;

    for (n = 0; n < demux->n_streams; n++) {
      stream = demux->streams[n];
      usSeekTime = AIUR_GSTTS_2_CORETS (desired_offset);


      if (stream->type != MEDIA_VIDEO) {
        aiurdemux_reset_stream (demux, stream);
        if (core_ret == PARSER_SUCCESS) {
            
            IParser->seek(handle, stream->track_idx,
                &usSeekTime, SEEK_FLAG_NO_LATER);


          stream->time_position = desired_offset;

          if ((rate >= 0) && (stream->type == MEDIA_AUDIO)
              && (demux->n_video_streams))
            stream->block = TRUE;
          else
            stream->block = FALSE;

          if ((core_ret != PARSER_SUCCESS)
              || ((demux->play_mode != AIUR_PLAY_MODE_NORMAL)
                  && (stream->type == MEDIA_AUDIO))) {
            MARK_STREAM_EOS (demux, stream);
          }

          core_ret = PARSER_SUCCESS;
        } else {
          MARK_STREAM_EOS (demux, stream);
        }

      }


    }
  }
//bail:
  segment->stop = desired_offset;
  segment->time = desired_offset;

  return TRUE;
}


/* do a seek in push based mode */
static gboolean
aiurdemux_do_push_seek (GstAiurDemux * demux, GstPad * pad,
    GstEvent * event)
{
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  gboolean flush;
  gboolean update;
  GstSegment seeksegment;
  int i;
  gboolean ret = FALSE;


  if (event) {
    GST_DEBUG_OBJECT (demux, "doing seek with event");

    gst_event_parse_seek (event, &rate, &format, &flags,
        &cur_type, &cur, &stop_type, &stop);

    /* we have to have a format as the segment format. Try to convert
     * if not. */
    if (!gst_aiurdemux_convert_seek (pad, &format, cur_type, &cur,
            stop_type, &stop)) {
      goto no_format;
    }
    if (stop == (gint64) 0) {
      stop = (gint64) - 1;
    }
  } else {
    GST_DEBUG_OBJECT (demux, "doing seek without event");
    flags = 0;
  }

  flush = flags & GST_SEEK_FLAG_FLUSH;

  /* stop streaming, either by flushing or by pausing the task */
  if (flush) {
    gst_aiurdemux_push_event (demux, gst_event_new_flush_start ());
  }

  demux->loop_push = FALSE;
  gst_aiur_stream_cache_close (demux->stream_cache);

  /* wait for streaming to finish */
  g_mutex_lock (&demux->runmutex);

  gst_aiur_stream_cache_open (demux->stream_cache);

  memcpy (&seeksegment, &demux->segment, sizeof (GstSegment));

  if (event) {
    gst_segment_do_seek(&seeksegment, rate,
        format, flags, cur_type, cur, stop_type, stop, &update);
  }

  /* now do the seek, this actually never returns FALSE */
  ret =
      gst_aiurdemux_perform_seek (demux, &seeksegment,
      (flags & GST_SEEK_FLAG_ACCURATE));

  /* prepare for streaming again */
  if (flush) {
    gst_aiurdemux_push_event (demux, gst_event_new_flush_stop (TRUE));
  }

  /* commit the new segment */
  memcpy (&demux->segment, &seeksegment, sizeof (GstSegment));


  demux->loop_push = TRUE;
  gst_aiur_stream_cache_open (demux->stream_cache);
  for (i = 0; i < demux->n_streams; i++)
    demux->streams[i]->last_ret = GST_FLOW_OK;


  g_thread_new ("aiur_push",(GThreadFunc) aiurdemux_loop_push, (gpointer) demux);
  g_mutex_unlock (&demux->runmutex);

  return ret;

  /* ERRORS */
no_format:
  {
    GST_DEBUG_OBJECT (demux, "unsupported format given, seek aborted.");
    return ret;
  }
}


/* do a seek in pull based mode */
static gboolean
aiurdemux_do_seek (GstAiurDemux * demux, GstPad * pad, GstEvent * event)
{
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  gboolean flush;
  gboolean update;
  GstSegment seeksegment;
  int i;
  gboolean ret = FALSE;

  if (event) {
    GST_DEBUG_OBJECT (demux, "doing seek with event");

    gst_event_parse_seek (event, &rate, &format, &flags,
        &cur_type, &cur, &stop_type, &stop);

    if (stop == (gint64) 0) {
      stop = (gint64) - 1;
    }

    /* we have to have a format as the segment format. Try to convert
     * if not. */
    if (!gst_aiurdemux_convert_seek (pad, &format, cur_type, &cur,
            stop_type, &stop))
      goto no_format;

    GST_DEBUG_OBJECT (demux, "seek format %s",
        gst_format_get_name (format));
  } else {
    GST_DEBUG_OBJECT (demux, "doing seek without event");
    flags = 0;
  }

  flush = flags & GST_SEEK_FLAG_FLUSH;

  /* stop streaming by pausing the task */
  if (flush) {
    gst_aiurdemux_push_event (demux, gst_event_new_flush_start ());
  }

  gst_pad_pause_task (demux->sinkpad);

  /* wait for streaming to finish */
  GST_PAD_STREAM_LOCK (demux->sinkpad);

  /* copy segment, we need this because we still need the old
   * segment when we close the current segment. */
  memcpy (&seeksegment, &demux->segment, sizeof (GstSegment));

  if (event) {
    gst_segment_do_seek (&seeksegment, rate, format, flags,
        cur_type, cur, stop_type, stop, &update);
  }

  if (flush) {
    gst_aiurdemux_push_event (demux, gst_event_new_flush_stop (TRUE));
  }

  /* now do the seek, this actually never returns FALSE */
  ret =
      gst_aiurdemux_perform_seek (demux, &seeksegment,
      (flags & GST_SEEK_FLAG_ACCURATE));

  /* commit the new segment */
  memcpy (&demux->segment, &seeksegment, sizeof (GstSegment));


  /* restart streaming, NEWSEGMENT will be sent from the streaming
   * thread. */
  for (i = 0; i < demux->n_streams; i++)
    demux->streams[i]->last_ret = GST_FLOW_OK;

  gst_pad_start_task (demux->sinkpad,
      (GstTaskFunction) aiurdemux_pull_task, demux->sinkpad,NULL);

  GST_PAD_STREAM_UNLOCK (demux->sinkpad);

  return ret;

  /* ERRORS */
no_format:
  {
    GST_DEBUG_OBJECT (demux, "unsupported format given, seek aborted.");
    return ret;
  }
}
static gboolean
gst_aiurdemux_get_duration (GstAiurDemux * demux, gint64 * duration)
{
  gboolean res = TRUE;

  *duration = GST_CLOCK_TIME_NONE;

  if (demux->movie_duration!= 0) {
    if (demux->movie_duration != G_MAXINT64) {
      *duration = demux->movie_duration;
    }
  }
  return res;
}

static void
aiurdemux_send_pending_events (GstAiurDemux * demux)
{
  guint n;

  for (n = 0; n < demux->n_streams; n++) {
    AiurDemuxStream *stream = demux->streams[n];

    if (stream->pending_eos) {
      aiurdemux_send_stream_eos (demux, stream);
    }
  }
}

static void
aiurdemux_release_resource (GstAiurDemux * demux)
{
  int n;


    for (n = 0; n < demux->n_streams; n++) {
        AiurDemuxStream *stream = demux->streams[n];

        if(stream == NULL){
            continue;
        }
        if (stream->pad) {
            gst_element_remove_pad (GST_ELEMENT_CAST (demux), stream->pad);
            stream->pad = NULL;
        }
        if (stream->caps) {
            gst_caps_unref (stream->caps);
            stream->caps = NULL;
        }

        if (stream->pending_tags) {
            gst_tag_list_unref (stream->pending_tags);
            stream->pending_tags = NULL;
        }

        if (stream->buffer) {
            gst_buffer_unref (stream->buffer);
            stream->buffer = NULL;
        }

        if (stream->adapter) {
            gst_adapter_clear (stream->adapter);
            g_object_unref (stream->adapter);
            stream->adapter = NULL;
        }

        if (stream->buf_queue) {
            GstBuffer * gbuf; 
            while ((gbuf = g_queue_pop_head ((stream)->buf_queue))) {
                gst_buffer_unref (gbuf);
            };
            g_queue_free (stream->buf_queue);
            stream->buf_queue = NULL;
        }

        g_free (stream);
        stream = NULL;
    }

  if (demux->programs) {
    for (n = 0; n < demux->program_num; n++) {
      AiurDemuxProgram *program = demux->programs[n];
      if (program) {
        g_free (program);
      }
    }
    g_free (demux->programs);
    demux->programs = NULL;
  }

}
static GstFlowReturn
gst_aiurdemux_close_core (GstAiurDemux * demux)
{
  int32 core_ret = PARSER_SUCCESS;
  AiurCoreInterface *IParser = demux->core_interface;
  FslParserHandle handle = demux->core_handle;
  gchar * index_file = NULL;

  if (IParser) {
    if (handle) {

      index_file = aiurcontent_get_index_file(demux->content_info);

      if ((demux->option.index_enabled) &&(index_file)
          && (IParser->coreid) && (strlen (IParser->coreid))) {
        uint32 size = 0;
        AiurIndexTable *itab;

        if (IParser->exportIndex) {
          core_ret = IParser->exportIndex( handle, NULL, &size);

          if (((core_ret != PARSER_SUCCESS) && (size < 0))
              || (size > AIUR_IDX_TABLE_MAX_SIZE)) {
            size = 0;
          }
        }
        itab = aiurdemux_create_idx_table (size, IParser->coreid);
        if (itab) {
          if (size) {
            core_ret = IParser->exportIndex(handle, itab->idx, &size);
            if (core_ret != PARSER_SUCCESS) {
              size = 0;
            }
          }
          core_ret =
              aiurdemux_export_idx_table (index_file, itab);
          if (core_ret == 0)
            GST_INFO ("Index table %s[size:%d] exported.",
                index_file, size);
          aiurdemux_destroy_idx_table (itab);
        }

      }

      IParser->deleteParser(handle);
    }
    aiur_core_destroy_interface (IParser);
    demux->core_interface = NULL;
  }

  return GST_FLOW_OK;
}






