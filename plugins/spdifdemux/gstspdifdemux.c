/* GStreamer
 * Copyright 2020-2021 NXP
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-spdifdemux
 *
 * Parse iec958 stream into compressed audio or linear pcm audio.
 * Or parse iec937 stream into compressed audio.
 *
 * Spdifdemux supports both push and pull mode operations, making it possible to
 * stream from a file source.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location= audio.file ! spdifdemux ! audioconvert ! alsasink
 * ]| Read a iec958 format or iec937 format file and output to the soundcard using the ALSA element.
 * The iec958 format file is assumed to contain linear PCM audio or compressed audio.  
 * The iec937 format file is assumed to contain compressed audio.
 * |[
 * gst-launch-1.0 alsasrc ! queue ! spdifdemux ! decodebin ! alsasink
 * ]| Playback data from a alsasrc.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>

#include <dlfcn.h>
#define SPDIF_PARSER_LIB_PATH  "/usr/lib/imx-mm/parser/libspdifparser.so"
#include "spdifparser.h"
#include "gstspdifdemux.h"
#include "gstimxcommon.h"

GST_DEBUG_CATEGORY_STATIC (spdifdemux_debug);
#define GST_CAT_DEFAULT (spdifdemux_debug)

static void gst_spdifdemux_dispose (GObject * object);

static gboolean gst_spdifdemux_sink_activate (GstPad * sinkpad,
    GstObject * parent);
static gboolean gst_spdifdemux_sink_activate_mode (GstPad * sinkpad,
    GstObject * parent, GstPadMode mode, gboolean active);
static GstStateChangeReturn gst_spdifdemux_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_spdifdemux_pad_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static GstFlowReturn gst_spdifdemux_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);
static gboolean gst_spdifdemux_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static void gst_spdifdemux_loop (GstPad * pad);
static gboolean gst_spdifdemux_srcpad_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static void gst_spdifdemux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_spdifdemux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

#define DEFAULT_IEC958_FORMAT		  IEC958_FORMAT_UNKNOWN
enum
{
  PROP_0,
  PROP_IEC958_FORMAT,
};

/* audio/x-raw for iec937 frame, audio/x-iec958 for iec958 frame */
static GstStaticPadTemplate sink_template_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_FORMATS_ALL ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, MAX ]; "
        "audio/x-iec958, "
        "format = U32LE, "
        "layout = (string) interleaved, "
        "rate = (int) [ 1, MAX ], " 
        "channels = (int) [ 1, MAX ]")
    );

static GstStaticPadTemplate src_template_factory =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_FORMATS_ALL ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, MAX ]; "
        "audio/x-ac3;"
        "audio/x-eac3; "
        "audio/mpeg, mpegversion = (int) 1; "
        "audio/mpeg, mpegversion = (int) { 2, 4 }; ")
    );

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (spdifdemux_debug, "spdifdemux", 0, "SPDIF demuxer");

#define gst_spdifdemux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstSpdifDemux, gst_spdifdemux, GST_TYPE_ELEMENT,
    DEBUG_INIT);

/* spdif parser interface index definition */
uint32_t spdif_parser_if_id_tbl[] = {
  SPDIF_PARSER_API_GET_VERSION_INFO,
  SPDIF_PARSER_API_OPEN,
  SPDIF_PARSER_API_CLOSE,
  SPDIF_PARSER_API_SET_MODE,
  SPDIF_PARSER_API_SET_IEC958_TYPE,
  SPDIF_PARSER_API_GET_MODE,
  SPDIF_PARSER_API_SEARCH_HEADER,
  SPDIF_PARSER_API_GET_COMPRESS_AUDIO_FRAME_SIZE,
  SPDIF_PARSER_API_GET_COMPRESS_AUDIO_LEN,
  SPDIF_PARSER_API_READ,
  SPDIF_PARSER_API_READ_WITH_SYNC,
  SPDIF_PARSER_API_GET_AUDIO_INFO,
  SPDIF_PARSER_API_GET_AUDIO_TYPE,
  SPDIF_PARSER_API_GET_IEC937_TYPE,
  SPDIF_PARSER_API_GET_SAMPLE_RATE,
  SPDIF_PARSER_API_GET_CHANNEL_NUM,
  SPDIF_PARSER_API_GET_DATA_LENGTH,
};

gboolean
gst_spdifdemux_create_spdif_parser_interface (GstSpdifDemux * spdif,
    gchar * dl_name)
{
  int total = G_N_ELEMENTS (spdif_parser_if_id_tbl);
  spdif_parser_query_interface_t spdif_parser_query_interface;
  void **papi;
  gint32 err;
  spdif_parser_if_t *inf = NULL;

  /* Open lib */
  GST_DEBUG_OBJECT (spdif, "open lib");
  spdif->dl_handle = dlopen (dl_name, RTLD_LAZY);
  spdif_parser_query_interface =
      dlsym (spdif->dl_handle, "spdif_parser_query_interface");
  if (spdif_parser_query_interface == NULL) {
    g_print ("can not find symbol %s\n", "spdif_parser_query_interface");
    goto fail;
  }

  /* Create data structure for spdif parser interface */
  inf = g_new0 (spdif_parser_if_t, 1);
  memset (inf, 0, sizeof (spdif_parser_if_t));
  if (inf == NULL)
    goto fail;

  /* Fill data */
  papi = (void **) inf;

  for (guint i = 0; i < total; i++) {
    err = spdif_parser_query_interface (spdif_parser_if_id_tbl[i], papi);
    if (err) {
      *papi = NULL;
    }

    if ((*papi == 0)) {
      GST_DEBUG_OBJECT (spdif,
          "spdif_parser_if_id_tbl[%d] = %d, the function is null", i,
          spdif_parser_if_id_tbl[i]);
    }
    papi++;
  }

  if (inf->spdif_parser_get_version_info) {
    spdif->spdif_parser_id = (inf->spdif_parser_get_version_info) ();
    if (spdif->spdif_parser_id) {
      g_print ("spdif parser version: %s\n file: %s\n", spdif->spdif_parser_id,
          dl_name);
    } else {
      g_print (" can not read spdif parser version information ");
    }
  } else {
    g_print (" No valid version reading function ");
  }

  spdif->spdif_parser_if = inf;
  return TRUE;

fail:
  GST_ERROR_OBJECT (spdif, "exec fail");
  if (spdif->dl_handle) {
    dlclose (spdif->dl_handle);
  }
  return FALSE;
}

static void
gst_spdifdemux_destroy_spdif_parser_interface (GstSpdifDemux * spdif)
{
  GST_DEBUG_OBJECT (spdif, "free mem");

  if (spdif->dl_handle) {
    dlclose (spdif->dl_handle);
  }

  if (spdif->spdif_parser_if) {
    g_free (spdif->spdif_parser_if);
  }
}

GType
gst_spdifdemux_get_iec958_format (void)
{
  static GType gst_spdifdemux_iec958_format = 0;

  if (!gst_spdifdemux_iec958_format) {
    static GEnumValue iec958_format_values[] = {
      {IEC958_FORMAT_UNKNOWN, "unknown iec958 format", "unknown"},
      {IEC958_FORMAT_PCM, "iec958 pcm audio", "pcm"},
      {IEC958_FORMAT_IEC937, "iec958 compress audio", "iec937"},
      {0, NULL, NULL},
    };

    gst_spdifdemux_iec958_format =
        g_enum_register_static ("ImxSpdifdemuxIec958Format",
        iec958_format_values);
  }

  return gst_spdifdemux_iec958_format;
}

static void
gst_spdifdemux_class_init (GstSpdifDemuxClass * klass)
{
  GstElementClass *gstelement_class;
  GObjectClass *object_class;

  gstelement_class = (GstElementClass *) klass;
  object_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = gst_spdifdemux_dispose;
  object_class->set_property = gst_spdifdemux_set_property;
  object_class->get_property = gst_spdifdemux_get_property;

  g_object_class_install_property (object_class, PROP_IEC958_FORMAT,
      g_param_spec_enum ("iec958-format",
          "Iec958 format",
          "Iec958 format type which can be unknown, linear pcm or iec937. Default is unknown",
          gst_spdifdemux_get_iec958_format (),
          DEFAULT_IEC958_FORMAT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state = gst_spdifdemux_change_state;

  /* register pads */
  gst_element_class_add_static_pad_template (gstelement_class,
      &sink_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &src_template_factory);

  gst_element_class_set_static_metadata (gstelement_class, "SPDIF demuxer",
      "Parser/Audio",
      "Parse a iec958 or iec937 file into audio",
      "Elliot chen <elliot.chen@nxp.com>");
}

static void
gst_spdifdemux_reset (GstSpdifDemux * spdif)
{
  GST_DEBUG_OBJECT (spdif, "gst_spdifdemux_reset");
  spdif->state = GST_SPDIFDEMUX_HEADER;
  spdif->bps = 0;
  spdif->offset = 0;
  spdif->audio_offset = 0;
  spdif->got_fmt = FALSE;
  spdif->first = TRUE;

  /* initialize spdif paser audio information */
  memset(&spdif->spdif_audio_info, 0, sizeof(spdif_audio_info_t));

  /* latency calculation */
  spdif->clock_offset = GST_CLOCK_TIME_NONE;
  spdif->start_time = GST_CLOCK_TIME_NONE;
  spdif->pipeline_latency = 0;
  /* segment init */
  gst_segment_init (&spdif->segment, GST_FORMAT_TIME);
  /* initialize fs calculation parameters */
  memset (&spdif->fs_calc_param, 0, sizeof (BpsCalcInfo));

  if (spdif->adapter) {
    gst_adapter_clear (spdif->adapter);
    g_object_unref (spdif->adapter);
    spdif->adapter = NULL;
  }

  if (spdif->caps)
    gst_caps_unref (spdif->caps);
  spdif->caps = NULL;

  if (spdif->start_segment)
    gst_event_unref (spdif->start_segment);
  spdif->start_segment = NULL;
}

static void
gst_spdifdemux_dispose (GObject * object)
{
  GstSpdifDemux *spdif = GST_SPDIFDEMUX (object);

  GST_DEBUG_OBJECT (spdif, "SPDIF: Dispose");
  gst_spdifdemux_reset (spdif);
  spdif->spdif_parser_if->spdif_parser_close (&spdif->handle);
  gst_spdifdemux_destroy_spdif_parser_interface (spdif);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_spdifdemux_init (GstSpdifDemux * spdifdemux)
{
  gst_spdifdemux_reset (spdifdemux);

  /* Create spdif parser interface */
  if (!gst_spdifdemux_create_spdif_parser_interface (spdifdemux,
          SPDIF_PARSER_LIB_PATH)) {
    GST_ERROR_OBJECT (spdifdemux, "create spdif parser interface error");
    return;
  }

  spdif_parser_memory_ops mem_ops =
      { (void *(*)(guint32 size)) g_malloc0, g_free };
  spdifdemux->spdif_parser_if->spdif_parser_open (&spdifdemux->handle,
      &mem_ops);

  /* sink */
  spdifdemux->sinkpad =
      gst_pad_new_from_static_template (&sink_template_factory, "sink");
  gst_pad_set_activate_function (spdifdemux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_spdifdemux_sink_activate));
  gst_pad_set_activatemode_function (spdifdemux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_spdifdemux_sink_activate_mode));
  gst_pad_set_chain_function (spdifdemux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_spdifdemux_chain));
  gst_pad_set_event_function (spdifdemux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_spdifdemux_sink_event));
  gst_element_add_pad (GST_ELEMENT_CAST (spdifdemux), spdifdemux->sinkpad);

  /* src */
  spdifdemux->srcpad =
      gst_pad_new_from_static_template (&src_template_factory, "src");
  gst_pad_set_query_function (spdifdemux->srcpad,
      GST_DEBUG_FUNCPTR (gst_spdifdemux_pad_query));
  gst_pad_set_event_function (spdifdemux->srcpad,
      GST_DEBUG_FUNCPTR (gst_spdifdemux_srcpad_event));
  gst_element_add_pad (GST_ELEMENT_CAST (spdifdemux), spdifdemux->srcpad);
}

static void
gst_spdifdemux_add_src_pad (GstSpdifDemux * spdif, GstBuffer * buf)
{
  GST_DEBUG_OBJECT (spdif, "adding src pad");
  g_assert (spdif->caps != NULL);

  if (spdif->streaming == FALSE){
    GST_DEBUG_OBJECT (spdif, "gst_event_new_stream_start\n");
    gchar *stream_id = gst_pad_create_stream_id_printf (spdif->srcpad,
    GST_ELEMENT_CAST (spdif), "%u", 0);
    gst_pad_push_event (spdif->srcpad, gst_event_new_stream_start (stream_id));
    g_free (stream_id);
  }

  GST_DEBUG_OBJECT (spdif, "sending caps %" GST_PTR_FORMAT, spdif->caps);
  gst_pad_set_caps (spdif->srcpad, spdif->caps);

  if (spdif->start_segment) {
    GST_DEBUG_OBJECT (spdif, "Send start segment event on newpad");
    gst_pad_push_event (spdif->srcpad, spdif->start_segment);
    spdif->start_segment = NULL;
  }
  else{
    GST_DEBUG_OBJECT (spdif, "Create and send start segment event on newpad");
    spdif->start_segment = gst_event_new_segment (&spdif->segment);
    gst_pad_push_event (spdif->srcpad, spdif->start_segment);
    spdif->start_segment = NULL;
    GST_DEBUG_OBJECT (spdif, "gst_event_new_segment\n");
  }
}

gboolean
gst_spdifdemux_record_bps_params (BpsCalcInfo * p_param, GstBuffer * buf)
{
  p_param->buf_count++;

#define BPS_RECORD_OFFSET         2
  if (p_param->buf_count < BPS_RECORD_OFFSET) {
    return FALSE;
  } else {
    if (p_param->buf_count == BPS_RECORD_OFFSET) {
      p_param->start_ts = GST_BUFFER_TIMESTAMP (buf);
      p_param->data_len = 0;
    } else {
      p_param->flag = TRUE;
      p_param->end_ts = GST_BUFFER_TIMESTAMP (buf);
      p_param->data_len += gst_buffer_get_size (buf);
    }
  }

  return TRUE;
}

guint32
gst_spdifdemux_check_fs_by_bps (guint32 fs)
{
  guint32 i = 0;
  guint32 fs_tab[11] = { 16000, 22050, 24000, 32000, 44100,
    48000, 88200, 96000, 176400, 192000, 768000
  };

#define SPDIFDEMUX_BPS_DIFF(a, b)           ((a > b)? (a - b): (b - a))
#define SPDIFDEMUX_BPS_DIFF_MAX_VALUE       (1000)

  for (i = 0; i < sizeof (fs_tab) / sizeof (fs_tab[0]); i++) {
    if (SPDIFDEMUX_BPS_DIFF (fs, fs_tab[i]) < SPDIFDEMUX_BPS_DIFF_MAX_VALUE) {
      return fs_tab[i];
    }
  }

  return 0;
}

guint32
gst_spdifdemux_get_fs_by_bps (GstSpdifDemux * spdif, guint32 channel_num)
{
  guint32 calc_fs = 0;
  guint32 check_fs = 0;
  guint64 sample_num = 0;
  BpsCalcInfo *p_param = &spdif->fs_calc_param;
  guint64 time_us = 0;

  if (!p_param->flag) {
    GST_DEBUG_OBJECT (spdif, "No valid params");
    return 0;
  }
#define SPDIFDEMUX_BPS_CHECK_BUF_SIZE  (4000 << 3)
  if (p_param->data_len >= SPDIFDEMUX_BPS_CHECK_BUF_SIZE) {
    sample_num = (p_param->data_len >> 2) / channel_num;
    time_us = (p_param->end_ts - p_param->start_ts) / 1000;
    calc_fs = (sample_num * 1000000) / time_us;
    check_fs = gst_spdifdemux_check_fs_by_bps (calc_fs);

    GST_DEBUG_OBJECT (spdif,
        "data_len = %ld, spdif->offset = %ld, time_us = %ld, calc_fs = %d, check_fs = %d\n",
        p_param->data_len, spdif->offset, time_us, calc_fs, check_fs);
  }

  return check_fs;
}

static gboolean
gst_spdifdemux_map_type (GstSpdifDemux * spdif)
{
  const char *format = 0;
  guint32 fs_calc = 0;
  spdif_audio_info_t *p_audio_info = &(spdif->spdif_audio_info);

  if (spdif->caps) {
    GST_DEBUG_OBJECT (spdif,
        "already set, spdif->caps: sink caps %" GST_PTR_FORMAT, spdif->caps);
    return TRUE;
  }

  spdif->spdif_parser_if->spdif_parser_get_audio_info (spdif->handle, p_audio_info);
  if (SPDIF_AUDIO_FORMAT_UNSET == SPDIF_AUDIO_FORMAT_PCM) {
    GST_DEBUG_OBJECT (spdif, "have no audio format");
    return FALSE;
  }else if (p_audio_info->audio_type == SPDIF_AUDIO_FORMAT_PCM) {
    spdif->caps = gst_caps_new_empty_simple ("audio/x-raw");

    if (p_audio_info->channel_num &&
        p_audio_info->sample_rate && p_audio_info->data_length) {
      if (p_audio_info->data_length == 16) {
        format = "S16LE";
      } else if (p_audio_info->data_length == 18) {
        format = "S18LE";
      } else if (p_audio_info->data_length == 20) {
        format = "S20LE";
      } else if (p_audio_info->data_length == 24) {
        format = "S24LE";
      } else {
        GST_ERROR_OBJECT (spdif, "error format: %d",
            p_audio_info->data_length);
        return FALSE;
      }

      fs_calc = gst_spdifdemux_get_fs_by_bps (spdif, p_audio_info->channel_num);
      if ((p_audio_info->sample_rate != fs_calc) && fs_calc) {
        GST_DEBUG_OBJECT (spdif, "channel data fs = %d, check_fs = %d",
            p_audio_info->sample_rate, fs_calc);
      }
      
      spdif->bps = p_audio_info->sample_rate * \
      p_audio_info->channel_num * p_audio_info->data_length;

      gst_caps_set_simple (spdif->caps, "format", G_TYPE_STRING, format,
          "channels", G_TYPE_INT, p_audio_info->channel_num, "rate",
          G_TYPE_INT, p_audio_info->sample_rate, "layout", G_TYPE_STRING,
          "interleaved", NULL);
    } else {
      GST_ERROR_OBJECT (spdif,
          "channel_num = %d, sample_rate = %d, data_length = %d ",
          p_audio_info->channel_num, p_audio_info->sample_rate,
          p_audio_info->data_length);
      return FALSE;
    }
  } else {
    switch (p_audio_info->iec937_type) {
      case SPDIF_IEC937_FORMAT_TYPE_AC3:
      {
        spdif->caps = gst_caps_new_empty_simple ("audio/x-ac3");
      }
        break;
      case SPDIF_IEC937_FORMAT_TYPE_EAC3:
      {
        spdif->caps =
            gst_caps_new_simple ("audio/x-eac3", "alignment", G_TYPE_STRING,
            "iec61937", NULL);
      }
        break;
      case SPDIF_IEC937_FORMAT_TYPE_MPEG1L1:
      {
        spdif->caps =
            gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
            "mpegaudioversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 1, NULL);
      }
        break;
      case SPDIF_IEC937_FORMAT_TYPE_MPEG2L1:
      {
        spdif->caps =
            gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
            "mpegaudioversion", G_TYPE_INT, 2, "layer", G_TYPE_INT, 1, NULL);
      }
        break;
      case SPDIF_IEC937_FORMAT_TYPE_MPEG2L2:
      {
        spdif->caps =
            gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
            "mpegaudioversion", G_TYPE_INT, 2, "layer", G_TYPE_INT, 2, NULL);
      }
        break;
      case SPDIF_IEC937_FORMAT_TYPE_MPEG2:
      case SPDIF_IEC937_FORMAT_TYPE_MPEG1L23:
      case SPDIF_IEC937_FORMAT_TYPE_MPEG2L3:
      {
        spdif->caps =
            gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
            "mpegaudioversion", G_TYPE_INT, 2, "layer", G_TYPE_INT, 3, NULL);
      }
        break;
      case SPDIF_IEC937_FORMAT_TYPE_MPEG2_4_AAC:
      case SPDIF_IEC937_FORMAT_TYPE_MPEG2_4_AAC_2:
      case SPDIF_IEC937_FORMAT_TYPE_MPEG2_4_AAC_3:
      {
        spdif->caps =
            gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 2,
            "stream-format", G_TYPE_STRING, "adts", NULL);
      }
        break;
      default:
        GST_ERROR_OBJECT (spdif, "Unkonw format!!!");
        return FALSE;
    }

    if (p_audio_info->sample_rate) {
      gst_caps_set_simple (spdif->caps, "rate", G_TYPE_INT,
          p_audio_info->sample_rate, NULL);
    }
  }

  GST_DEBUG_OBJECT (spdif, "spdif->caps: sink caps %" GST_PTR_FORMAT,
      spdif->caps);

  return TRUE;
}

static SPDIF_RET_TYPE
gst_spdifdemux_search_header_push (GstSpdifDemux * spdif, GstAdapter * adapter)
{
  guint8 *p_buf = NULL;
  guint32 len = 0;
  SPDIF_RET_TYPE ret = SPDIF_ERR_INSUFFICIENT_DATA;
  guint32 header_pos = 0;

  while (gst_adapter_available (adapter) >= SPDIF_PARSER_IEC958_BLOCK_BYTE_LEN) {
    len = gst_adapter_available (adapter);
    p_buf = (guint8 *) gst_adapter_map (adapter, len);

    ret = spdif->spdif_parser_if->spdif_parser_search_header (spdif->handle,\
          p_buf, len, &header_pos);
    gst_adapter_unmap (adapter);

    if (header_pos) {
      gst_adapter_flush (adapter, header_pos);
      spdif->offset += header_pos;
    }
    GST_DEBUG_OBJECT (spdif,
        "stream: len = 0x%x, spdif->offset = 0x%lx, header_pos = 0x%x, ret = %d\n",
        len, spdif->offset, header_pos, ret);

    if ((ret == SPDIF_OK) || (ret == SPDIF_ERR_INSUFFICIENT_DATA)
        || (ret == SPDIF_ERR_PARAM)) {
      if (ret == SPDIF_OK) {
        GST_DEBUG_OBJECT (spdif,
            "stream: search ok, spdif->offset = %" G_GINT64_FORMAT "\n",
            spdif->offset);
      }
      break;
    }
  }

  return ret;
}

static GstFlowReturn
gst_spdifdemux_search_header_pull (GstSpdifDemux * spdif)
{
  guint32 len = SPDIF_PARSER_IEC958_BLOCK_BYTE_LEN;
  guint32 header_pos = 0;
  GstFlowReturn res = GST_FLOW_ERROR;
  SPDIF_RET_TYPE ret = SPDIF_ERR_INSUFFICIENT_DATA;
  GstBuffer *buf = NULL;
  GstMapInfo map;

  while (TRUE) {
    res = gst_pad_pull_range (spdif->sinkpad, spdif->offset, len, &buf);
    if (res == GST_FLOW_OK) {
      if (gst_buffer_get_size (buf) < len) {
        gsize size = gst_buffer_get_size (buf);
        GST_LOG_OBJECT (spdif, "Got only %" G_GSIZE_FORMAT " bytes of data",
            size);
        gst_buffer_unref (buf);
        res = GST_FLOW_EOS;
        break;
      }

      gst_buffer_map (buf, &map, GST_MAP_READ);
      ret =
          spdif->spdif_parser_if->spdif_parser_search_header (spdif->handle,
          (guint8 *) map.data, map.size, &header_pos);
      GST_DEBUG_OBJECT (spdif,
          "file: spdif->offset = 0x%lx, len = 0x%x, header_pos = 0x%x, ret = %d \n",
          spdif->offset, len, header_pos, ret);
      if (header_pos) {
        spdif->offset += header_pos;
      }
      gst_buffer_unmap (buf, &map);
      gst_buffer_unref (buf);
      buf = NULL;

      if (ret == SPDIF_OK) {
        GST_DEBUG_OBJECT (spdif, "stream: search ok, spdif->offset = 0x%lx\n",
            spdif->offset);
        break;
      }
    } else {
      if (res == GST_FLOW_EOS) {
        GST_DEBUG_OBJECT (spdif, "found EOS");
        if (buf) {
          gst_buffer_unref (buf);
        }
      }
      break;
    }
  }

  return res;
}

static GstFlowReturn
gst_spdifdemux_search_header (GstSpdifDemux * spdif)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  SPDIF_RET_TYPE status = SPDIF_ERR_PARAM;

  if (spdif->streaming) {
    status = gst_spdifdemux_search_header_push (spdif, spdif->adapter);
    if (status != SPDIF_OK) {
      return GST_FLOW_OK;
    }
  } else {
    ret = gst_spdifdemux_search_header_pull (spdif);
    if (ret != GST_FLOW_OK) {
      return ret;
    }
  }

  if (!spdif->got_fmt) {
    if (!gst_spdifdemux_map_type (spdif)) {
      return GST_FLOW_ERROR;
    }
    spdif->got_fmt = TRUE;
  }

  return GST_FLOW_OK;
}

static void
gst_spdifdemux_handle_audio_frame (GstSpdifDemux * spdif, GstBuffer * buf,
    guint len, guint * p_audio_len)
{
  GstMapInfo map;
  guint8 *p8 = 0;

  buf = gst_buffer_make_writable (buf);
  gst_buffer_map (buf, &map, GST_MAP_READ | GST_MAP_WRITE);
  p8 = map.data;
  if (SPDIF_AUDIO_FORMAT_PCM != spdif->spdif_audio_info.audio_type) {
    len = spdif->spdif_audio_info.audio_size;
  }
  spdif->spdif_parser_if->spdif_parser_read (spdif->handle, p8, p8, len, p_audio_len);
  GST_DEBUG_OBJECT (spdif, "iec937 audio len = %d, raw audio len = %d\n",
            len, *p_audio_len);

  gst_buffer_unmap (buf, &map);
  gst_buffer_resize (buf, 0, *p_audio_len);
}

static GstFlowReturn
gst_spdifdemux_stream_data (GstSpdifDemux * spdif, gboolean flushing)
{
  GstBuffer *buf = NULL;
  GstFlowReturn res = GST_FLOW_OK;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstClockTime duration = GST_CLOCK_TIME_NONE;
  guint frame_size = 0;
  guint audio_len = 0;

iterate_adapter:
  if (SPDIF_AUDIO_FORMAT_PCM != spdif->spdif_audio_info.audio_type) {
    /* sync and get frame size */
    if (spdif->streaming) {
      if (SPDIF_OK != gst_spdifdemux_search_header_push (spdif, spdif->adapter)) {
        return res;
      }
    } else {
      res = gst_spdifdemux_search_header_pull (spdif);
      if (GST_FLOW_OK != res) {
        if (res == GST_FLOW_EOS) {
          return res;
        } else {
          GST_WARNING_OBJECT (spdif, "Lost sync!!!. Try to resync.");
          return GST_FLOW_OK;
        }
      }
    }

    frame_size = spdif->spdif_audio_info.frame_size;
  } else {
    /* PCM audio */
    frame_size = SPDIF_PARSER_IEC958_BLOCK_BYTE_LEN;
    if (spdif->streaming) {
      frame_size = gst_adapter_available (spdif->adapter);
      if (!frame_size) {
        GST_INFO_OBJECT (spdif, "NO data in adapter");
        return GST_FLOW_OK;
      }

      frame_size &= (~0x03);
    }
  }
  GST_DEBUG_OBJECT (spdif, "frame size: %d, offset: 0x%lx", frame_size,
      spdif->offset);

  /* Get buffer data */
  if (spdif->streaming) {
    guint avail = gst_adapter_available (spdif->adapter);
    if (avail < frame_size) {
      GST_DEBUG_OBJECT (spdif, "Got only %u bytes of data from the sinkpad",
          avail);
      return GST_FLOW_OK;
    } else {
      buf = gst_adapter_take_buffer (spdif->adapter, frame_size);
      if (gst_buffer_get_size (buf) < frame_size) {
        GST_ERROR_OBJECT (spdif, "frame size: %d bigger then buffer size: %ld!",
            frame_size, gst_buffer_get_size (buf));
      }
    }
  } else {
    if ((res = gst_pad_pull_range (spdif->sinkpad, \
              spdif->offset, frame_size, &buf)) != GST_FLOW_OK)
      goto pull_error;

    /* we may get a short buffer at the end of the file */
    if (gst_buffer_get_size (buf) < frame_size) {
      gsize size = gst_buffer_get_size (buf);

      GST_LOG_OBJECT (spdif, "Got only %" G_GSIZE_FORMAT " bytes of data",
          size);
      gst_buffer_unref (buf);
      goto found_eos;
    }
  }

  /* handle audio data */
  gst_spdifdemux_handle_audio_frame (spdif, buf, frame_size, &audio_len);

  /* first chunk of data? create the source pad. We do this only here so
   * we can detect broken .spdif files with dts disguised as raw PCM (sigh) */
  if (G_UNLIKELY (spdif->first)) {
    spdif->first = FALSE;
    /* this will also push the segment events */
    gst_spdifdemux_add_src_pad (spdif, buf);
  } else {
    /* If we have a pending start segment, send it now. */
    if (G_UNLIKELY (spdif->start_segment != NULL)) {
      gst_pad_push_event (spdif->srcpad, spdif->start_segment);
      spdif->start_segment = NULL;
      GST_DEBUG_OBJECT (spdif, "gst_pad_push_event: start segment \n");
    }
  } 

  if (spdif->discont) {
    GST_DEBUG_OBJECT (spdif, "marking DISCONT");
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
    spdif->discont = FALSE;
  }

  if (spdif->streaming) {
    if (spdif->clock_offset != GST_CLOCK_TIME_NONE) {
      /* system clock */
      if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
        timestamp = GST_BUFFER_TIMESTAMP (buf) - spdif->start_time + spdif->clock_offset;
        GST_BUFFER_OFFSET(buf) = spdif->audio_offset;

        if (spdif->bps) {
          /* PCM audio */
          duration = gst_util_uint64_scale_ceil (audio_len << 3, \
                      GST_SECOND, (guint64) spdif->bps);
        }

        GST_LOG_OBJECT (spdif, 
        "Got buffer, compensated timestamp:%" GST_TIME_FORMAT 
        ", duration:%" GST_TIME_FORMAT
        ", size:%" G_GSIZE_FORMAT 
        ", offset = %ld"
        ", running time: %" GST_TIME_FORMAT
        ", pipeline_latency = %ld", \
        GST_TIME_ARGS (timestamp), GST_TIME_ARGS (duration), \
        gst_buffer_get_size (buf), GST_BUFFER_OFFSET(buf), \
        GST_TIME_ARGS(spdif->running_time), \
        spdif->pipeline_latency);
      }
    }
  }

  GST_BUFFER_TIMESTAMP (buf) = timestamp;
  GST_BUFFER_DURATION (buf) = duration;

  if ((res = gst_pad_push (spdif->srcpad, buf)) != GST_FLOW_OK){
      goto push_error;
    }
  spdif->offset += frame_size;
  spdif->audio_offset += audio_len;

  /* Iterate until need more data, so adapter size won't grow */
  if (spdif->streaming) {
    GST_LOG_OBJECT (spdif, "offset: %" G_GINT64_FORMAT, spdif->offset);
    goto iterate_adapter;
  }
  return res;

  /* ERROR */
found_eos:
  {
    GST_DEBUG_OBJECT (spdif, "found EOS");
    return GST_FLOW_EOS;
  }
pull_error:
  {
    /* check if we got EOS */
    if (res == GST_FLOW_EOS)
      goto found_eos;

    GST_WARNING_OBJECT (spdif,
        "Error getting %d bytes from the sinkpad, "
        "spdif->offset = %ld", frame_size, spdif->offset);
    return res;
  }
push_error:
  {
    GST_INFO_OBJECT (spdif,
        "Error pushing on srcpad %s:%s, reason %s, is linked? = %d",
        GST_DEBUG_PAD_NAME (spdif->srcpad), gst_flow_get_name (res),
        gst_pad_is_linked (spdif->srcpad));
    return res;
  }
}

static void
gst_spdifdemux_loop (GstPad * pad)
{
  GstFlowReturn ret;
  GstSpdifDemux *spdif = GST_SPDIFDEMUX (GST_PAD_PARENT (pad));

  GST_LOG_OBJECT (spdif, "process data");

  switch (spdif->state) {
    case GST_SPDIFDEMUX_HEADER:
      GST_INFO_OBJECT (spdif, "GST_SPDIFDEMUX_HEADER");
      if ((ret = gst_spdifdemux_search_header (spdif)) != GST_FLOW_OK)
        goto pause;

      if (!spdif->got_fmt)
        break;

      spdif->state = GST_SPDIFDEMUX_DATA;
      GST_INFO_OBJECT (spdif, "GST_SPDIFDEMUX_DATA");

    case GST_SPDIFDEMUX_DATA:
      if ((ret = gst_spdifdemux_stream_data (spdif, FALSE)) != GST_FLOW_OK)
        goto pause;
      break;
    default:
      g_assert_not_reached ();
  }
  return;

  /* ERRORS */
pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_DEBUG_OBJECT (spdif, "pausing task, reason %s", reason);
    gst_pad_pause_task (pad);

    if (ret == GST_FLOW_EOS) {
      /* handle end-of-stream/segment */
      /* so align our position with the end of it, if there is one
       * this ensures a subsequent will arrive at correct base/acc time */
      if (spdif->segment.format == GST_FORMAT_TIME) {
        if (spdif->segment.rate > 0.0 &&
            GST_CLOCK_TIME_IS_VALID (spdif->segment.stop))
          spdif->segment.position = spdif->segment.stop;
        else if (spdif->segment.rate < 0.0)
          spdif->segment.position = spdif->segment.start;
      }
      if (spdif->state == GST_SPDIFDEMUX_HEADER || !spdif->caps) {
        GST_ELEMENT_ERROR (spdif, STREAM, WRONG_TYPE, (NULL),
            ("No valid input found before end of stream"));
        gst_pad_push_event (spdif->srcpad, gst_event_new_eos ());
      } else {
        /* add pad before we perform EOS */
        if (G_UNLIKELY (spdif->first)) {
          spdif->first = FALSE;
          gst_spdifdemux_add_src_pad (spdif, NULL);
        }

        /* perform EOS logic */
        if (spdif->segment.flags & GST_SEEK_FLAG_SEGMENT) {
          GstClockTime stop;

          if ((stop = spdif->segment.stop) == -1)
            stop = spdif->segment.duration;

          gst_element_post_message (GST_ELEMENT_CAST (spdif),
              gst_message_new_segment_done (GST_OBJECT_CAST (spdif),
                  spdif->segment.format, stop));
          gst_pad_push_event (spdif->srcpad,
              gst_event_new_segment_done (spdif->segment.format, stop));
        } else {
          gst_pad_push_event (spdif->srcpad, gst_event_new_eos ());
        }
      }
    } else if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_EOS) {
      /* for fatal errors we post an error message, post the error
       * first so the app knows about the error first. */
      GST_ELEMENT_FLOW_ERROR (spdif, ret);
      gst_pad_push_event (spdif->srcpad, gst_event_new_eos ());
    }
    return;
  }
}

gboolean
gst_spdifdemux_check_start_offset (GstSpdifDemux * spdif, GstBuffer * buf)
{
  GstClock *clock = NULL;
  gint clock_type;
  GstClockTime base_time, now;
  GstClockTimeDiff offset = 0;

  clock = gst_element_get_clock (GST_ELEMENT(spdif));
  if (clock == NULL) {
    GST_LOG_OBJECT (spdif, "clock is null");
    return FALSE;
  }

  if (G_OBJECT_TYPE (clock) == GST_TYPE_SYSTEM_CLOCK) {
    g_object_get (clock, "clock-type", &clock_type, NULL);
    if (clock_type != GST_CLOCK_TYPE_MONOTONIC) {
      GST_LOG_OBJECT (spdif, "clock type is not monotonic, clocktype = %d",
          clock_type);
      return FALSE;
    }

    now = gst_clock_get_time (clock);
    base_time = gst_element_get_base_time (GST_ELEMENT (spdif));
    offset = now - base_time;
    spdif->running_time = offset;
    GST_LOG_OBJECT (spdif, "audio running time = %" GST_TIME_FORMAT
        ", buf duration time = %" GST_TIME_FORMAT,
        GST_TIME_ARGS (offset), GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));

    /* clock of pipeline has started when first buffer arrives, so adjust the buffer timestamp */
    if ((spdif->clock_offset == GST_CLOCK_TIME_NONE) && (offset > 0)) {
      spdif->clock_offset = offset;
      GST_LOG_OBJECT (spdif, "clock offset = %" GST_TIME_FORMAT,
          GST_TIME_ARGS (offset));
    }
  } else {
    GST_LOG_OBJECT (spdif, "Not system clock, directly return");
    return FALSE;
  }

  if (spdif->start_time == GST_CLOCK_TIME_NONE) {
    if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
      spdif->start_time = GST_BUFFER_TIMESTAMP (buf);
    } else {
      GST_LOG_OBJECT (spdif, "first buffer timestamp is invalid");
    }
  }

  gst_spdifdemux_record_bps_params (&(spdif->fs_calc_param), buf);
  return TRUE;
}

static GstFlowReturn
gst_spdifdemux_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstFlowReturn ret;
  GstSpdifDemux *spdif = GST_SPDIFDEMUX (parent);

  GST_LOG_OBJECT (spdif, "adapter_push %" G_GSIZE_FORMAT 
                         " bytes, timestamp: %" GST_TIME_FORMAT,
                          gst_buffer_get_size (buf), \
                          GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buf)));

  gst_spdifdemux_check_start_offset (spdif, buf);
  gst_adapter_push (spdif->adapter, buf);

  switch (spdif->state) {
    case GST_SPDIFDEMUX_HEADER:
      GST_INFO_OBJECT (spdif, "GST_SPDIFDEMUX_HEADER");
      if ((ret = gst_spdifdemux_search_header (spdif)) != GST_FLOW_OK)
        goto done;

      if (!spdif->got_fmt)
        break;

      spdif->state = GST_SPDIFDEMUX_DATA;
      GST_INFO_OBJECT (spdif, "GST_SPDIFDEMUX_DATA");

      /* fall-through */
    case GST_SPDIFDEMUX_DATA:
      if (buf && GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT)) {
        spdif->discont = TRUE;
      }

      if ((ret = gst_spdifdemux_stream_data (spdif, FALSE)) != GST_FLOW_OK)
        goto done;
      break;
    default:
      g_return_val_if_reached (GST_FLOW_ERROR);
  }
done:
  if (G_UNLIKELY (spdif->abort_buffering)) {
    spdif->abort_buffering = FALSE;
    ret = GST_FLOW_ERROR;
    /* sort of demux/parse error */
    GST_ELEMENT_ERROR (spdif, STREAM, DEMUX, (NULL), ("unhandled buffer size"));
  }

  return ret;
}

static GstFlowReturn
gst_spdifdemux_flush_data (GstSpdifDemux * spdif)
{
  gsize len = gst_adapter_available (spdif->adapter);
  GstFlowReturn ret = GST_FLOW_OK;

  if (len > 0) {
    if (spdif->state == GST_SPDIFDEMUX_DATA)
    {
      ret = gst_spdifdemux_stream_data (spdif, TRUE);
    }
    
    len = gst_adapter_available (spdif->adapter);
    gst_adapter_flush (spdif->adapter, len);
    GST_LOG_OBJECT (spdif, "adapter size = %ld, start flush", len);
  }

  return ret;
}

static gboolean
gst_spdifdemux_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSpdifDemux *spdif = GST_SPDIFDEMUX (parent);
  gboolean ret = TRUE;

  GST_LOG_OBJECT (spdif, "handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gst_event_parse_caps (event, &caps);
      GST_DEBUG_OBJECT (spdif, "sink caps %" GST_PTR_FORMAT, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      GstSegment segment;

      gst_event_copy_segment (event, &segment);
      GST_DEBUG_OBJECT (spdif, "received newsegment %" GST_SEGMENT_FORMAT, &segment);

      if (segment.format != GST_FORMAT_TIME){
         gst_event_unref (event);
         break;
      }else{
        gst_segment_copy_into (&segment, &spdif->segment);
      }

      /* also store the newsegment event for the streaming thread */
      if (spdif->start_segment){
        gst_event_unref (spdif->start_segment);
      }
        
      GST_DEBUG_OBJECT (spdif, "Storing newseg %" GST_SEGMENT_FORMAT, &segment);
      spdif->start_segment = gst_event_new_segment (&spdif->segment);

      /* stream leftover data in current segment */
      gst_spdifdemux_flush_data (spdif);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_EOS:
      if (spdif->state == GST_SPDIFDEMUX_HEADER || !spdif->caps) {
        GST_ELEMENT_ERROR (spdif, STREAM, WRONG_TYPE, (NULL),
            ("No valid input found before end of stream"));
      } else {
        /* add pad if needed so EOS is seen downstream */
        if (G_UNLIKELY (spdif->first)) {
          spdif->first = FALSE;
          gst_spdifdemux_add_src_pad (spdif, NULL);
        }

        /* stream leftover data in current segment */
        gst_spdifdemux_flush_data (spdif);
      }

      /* fall-through */
    case GST_EVENT_FLUSH_STOP:
    {
      GstClockTime dur;

      if (spdif->adapter)
        gst_adapter_clear (spdif->adapter);

      spdif->discont = TRUE;
      dur = spdif->segment.duration;
      gst_segment_init (&spdif->segment, spdif->segment.format);
      spdif->segment.duration = dur;
      /* fall-through */
    }
    default:
      ret = gst_pad_event_default (spdif->sinkpad, parent, event);
      break;
  }

  return ret;
}

/* handle queries for location and length in requested format */
static gboolean
gst_spdifdemux_pad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = TRUE;
  GstSpdifDemux *spdif = GST_SPDIFDEMUX (parent);

  /* only if we know */
  if (spdif->state != GST_SPDIFDEMUX_DATA) {
    return FALSE;
  }

  GST_LOG_OBJECT (pad, "%s query", GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_SEGMENT:
    {
      GstFormat format;
      gint64 start, stop;

      format = spdif->segment.format;

      GST_DEBUG_OBJECT (spdif, "newsegment %" GST_SEGMENT_FORMAT "format = %d",
          &spdif->segment, format);

      start = gst_segment_to_stream_time (&spdif->segment, format,
          spdif->segment.start);
      if ((stop = spdif->segment.stop) == -1)
        stop = spdif->segment.duration;
      else
        stop = gst_segment_to_stream_time (&spdif->segment, format, stop);

      gst_query_set_segment (query, spdif->segment.rate, format, start, stop);
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }
  return res;
}

static gboolean
gst_spdifdemux_srcpad_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSpdifDemux *spdifdemux = GST_SPDIFDEMUX (parent);
  gboolean res = FALSE;
  GstClockTime latency;

  GST_DEBUG_OBJECT (spdifdemux, "%s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_LATENCY:
      gst_event_parse_latency (event, &latency);
      spdifdemux->pipeline_latency = latency;
      GST_LOG_OBJECT(spdifdemux,"pipeline latency to %ld", latency);
    default:
      res = gst_pad_push_event (spdifdemux->sinkpad, event);
      break;
  }
  return res;
}

static gboolean
gst_spdifdemux_sink_activate (GstPad * sinkpad, GstObject * parent)
{
  GstSpdifDemux *spdif = GST_SPDIFDEMUX (parent);
  GstQuery *query;
  gboolean pull_mode;

  if (spdif->adapter) {
    gst_adapter_clear (spdif->adapter);
    g_object_unref (spdif->adapter);
    spdif->adapter = NULL;
  }

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

  GST_DEBUG_OBJECT (sinkpad, "activating pull");
  spdif->streaming = FALSE;
  return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PULL, TRUE);

activate_push:
  {
    GST_DEBUG_OBJECT (sinkpad, "activating push");
    spdif->streaming = TRUE;
    spdif->adapter = gst_adapter_new ();
    return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);
  }
}

static gboolean
gst_spdifdemux_sink_activate_mode (GstPad * sinkpad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  gboolean res;

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      res = TRUE;
      break;
    case GST_PAD_MODE_PULL:
      if (active) {
        /* if we have a scheduler we can start the task */
        res =
            gst_pad_start_task (sinkpad, (GstTaskFunction) gst_spdifdemux_loop,
            sinkpad, NULL);
      } else {
        res = gst_pad_stop_task (sinkpad);
      }
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
}

static GstStateChangeReturn
gst_spdifdemux_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstSpdifDemux *spdif = GST_SPDIFDEMUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_spdifdemux_reset (spdif);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_spdifdemux_reset (spdif);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }
  return ret;
}

static void
gst_spdifdemux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSpdifDemux *self;

  g_return_if_fail (GST_IS_SPDIFDEMUX (object));
  self = GST_SPDIFDEMUX (object);

  switch (prop_id) {
    case PROP_IEC958_FORMAT:
    {
      self->iec958_format = g_value_get_enum (value);

      if (self->iec958_format == IEC958_FORMAT_PCM) {
        if (self->spdif_parser_if->spdif_parser_set_iec958_type) {
          self->spdif_parser_if->spdif_parser_set_iec958_type (self->handle,
              SPDIF_AUDIO_FORMAT_PCM);
          GST_DEBUG_OBJECT (self, "iec958_format: SPDIF_AUDIO_FORMAT_PCM");
        }
      } else if (self->iec958_format == IEC958_FORMAT_IEC937) {
        if (self->spdif_parser_if->spdif_parser_set_iec958_type) {
          self->spdif_parser_if->spdif_parser_set_iec958_type (self->handle,
              SPDIF_AUDIO_FORMAT_COMPRESS);
          GST_DEBUG_OBJECT (self, "iec958_format: SPDIF_AUDIO_FORMAT_COMPRESS");
        }
      } else {
        GST_DEBUG_OBJECT (self, "iec958_format: unknown format");
      }
    }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static void
gst_spdifdemux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSpdifDemux *self;

  g_return_if_fail (GST_IS_SPDIFDEMUX (object));
  self = GST_SPDIFDEMUX (object);

  switch (prop_id) {
    case PROP_IEC958_FORMAT:
      g_value_set_enum (value, self->iec958_format);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "spdifdemux", GST_RANK_PRIMARY,
      GST_TYPE_SPDIFDEMUX);
}

IMX_GST_PLUGIN_DEFINE (spdifdemux, "spdifdemux Plugins", plugin_init);
