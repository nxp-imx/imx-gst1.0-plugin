/*
 * Copyright (c) 2014, Freescale Semiconductor, Inc. All rights reserved.
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
#include <gst/tag/tag.h>
#include <gst/audio/audio.h>
#include "gstavbpcmsink.h"
#include "cip.h"


typedef struct{
  guint32 rate;
  guint8 syt_internal;
}SFC_STRUCT;
static SFC_STRUCT SFC_TABLE[] ={
  {32000,8},//0
  {44100,8},//1
  {48000,8},//2
  {88200,16},//3
  {96000,16},//4
  {176400,32},//5
  {192000,32}//6
};

#define DEFAULT_PACKET_COUNT 128
#define MAX_PACKET_LEN (1024)

GST_DEBUG_CATEGORY_STATIC (avb_pcm_sink_debug);
#define GST_CAT_DEFAULT avb_pcm_sink_debug

#define gst_avb_pcm_sink_parent_class parent_class
G_DEFINE_TYPE (GstAvbPcmSink, gst_avb_pcm_sink, GST_TYPE_AVBSINK);

#define PCM_AUDIO_CAPS \
  "audio/x-raw, "\
  "format = (string){S16LE, S24LE}, "\
  "rate = (int){32000, 44100, 48000, 88200, 96000, 176400, 192000}, "\
  "channels = (int)[1, 8]"

static GstStaticPadTemplate sink_template =
  GST_STATIC_PAD_TEMPLATE ("sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (PCM_AUDIO_CAPS));

static gstsutils_property avbsink_property[]=
{
  {"latency", G_TYPE_UINT, gst_avbsink_set_latency},
  {"use_ptp_time", G_TYPE_BOOLEAN, gst_avbsink_set_use_ptp_time},
  {"package_count",G_TYPE_UINT, gst_avbsink_set_package_count}
};
static gstsutils_property basesink_property[]=
{
    {"sync", G_TYPE_BOOLEAN, gst_base_sink_set_sync}
};

static gboolean
gst_avb_pcm_sink_parse_audio_info(GstAvbPcmSink * sink,GstAudioInfo * info)
{
  int i = 0;
  if(sink == NULL || info == NULL)
    return FALSE;

  sink->width = GST_AUDIO_INFO_WIDTH (info);
  sink->channels = GST_AUDIO_INFO_CHANNELS (info);
  sink->rate = GST_AUDIO_INFO_RATE (info);
  if(sink->width == 16){
    sink->label = 0x42;
  }else if(sink->width == 24){
    sink->label = 0x40;
  }else{
    GST_ERROR_OBJECT (sink,"process_buffer width is wrong");
    return FALSE;
  }

  for(i = 0; i < sizeof(SFC_TABLE)/sizeof(SFC_STRUCT); i++){
    if(sink->rate == SFC_TABLE[i].rate){
      sink->sfc = i;
      sink->syt_internal = SFC_TABLE[i].syt_internal;
      sink->dbs = sink->channels;
      return TRUE;
    }
  }

  return FALSE;
}
static guint8 gst_avb_pcm_sink_get_dbs(GstAvbSink * sink)
{
  GstAvbPcmSink *pcmsink;
  pcmsink = GST_AVB_PCM_SINK(sink);
  return pcmsink->dbs;
}
static guint8 gst_avb_pcm_sink_get_fn(GstAvbSink * sink){
  return 0;
}
static guint8 gst_avb_pcm_sink_get_sph(GstAvbSink * sink){
  return 0;
}
static guint8 gst_avb_pcm_sink_get_dbc(GstAvbSink * sink,guint32 payloadLen)
{
  GstAvbPcmSink *pcmsink;
  pcmsink = GST_AVB_PCM_SINK(sink);
  return (guint8)(payloadLen/pcmsink->dbs/sizeof(uint32));
}
static guint8 gst_avb_pcm_sink_get_fmt(GstAvbSink * sink)
{
  return CIP_FMT_AUDIO;
}
static guint8 gst_avb_pcm_sink_get_fdf(GstAvbSink * sink)
{
  GstAvbPcmSink *pcmsink;
  pcmsink = GST_AVB_PCM_SINK(sink);
  return pcmsink->sfc;
}
static guint16 gst_avb_pcm_sink_get_syt(GstAvbSink * sink)
{
  return 0xFFFF;
}
static guint32 gst_avb_pcm_sink_get_packet_len(GstAvbSink * sink,guint32 leftsize)
{
  GstAvbPcmSink *pcmsink;
  guint32 len = 0;
  guint32 count=0;
  guint32 max_count = 0;

  pcmsink = GST_AVB_PCM_SINK(sink);

  len = pcmsink->channels * pcmsink ->syt_internal * sizeof(uint32);
  max_count = gst_avbsink_get_package_count(sink);

  if(pcmsink->width == 16){
    leftsize = leftsize *2;
  }else{
    leftsize = leftsize * 4/3;
  }

  if(leftsize > MAX_PACKET_LEN)
    leftsize = MAX_PACKET_LEN;

  count = leftsize/len;

  if(count == 0)
    count = 1;

  if(count > max_count)
    count = max_count;

  return len * count;
}
static gboolean
gst_avb_pcm_sink_set_caps (GstBaseSink * basesink, GstCaps * caps)
{
  gboolean ret;
  GstAvbPcmSink *sink;
  const gchar *mimetype;
  GstStructure *structure;

  sink = GST_AVB_PCM_SINK (basesink);
  GST_DEBUG_OBJECT (sink,"gst_avb_pcm_sink_set_caps %"GST_PTR_FORMAT,caps);

  if (G_UNLIKELY (sink->caps && gst_caps_is_equal (sink->caps, caps))) {
    GST_DEBUG_OBJECT (sink,
        "caps haven't changed, skipping reconfiguration");
    return TRUE;
  }

  structure = gst_caps_get_structure (caps, 0);

  /* we have to differentiate between int and float formats */
  mimetype = gst_structure_get_name (structure);

  if (g_str_equal (mimetype, "audio/x-raw")) {
    GstAudioInfo info;
    sink->caps = caps;
    if (!gst_audio_info_from_caps (&info,caps)){
      return FALSE;
    }
    ret = gst_avb_pcm_sink_parse_audio_info(sink,&info);
    return ret;
  }

  return FALSE;
}
static GstCaps *
gst_avb_pcm_sink_get_caps (GstBaseSink * basesink, GstCaps * filter)
{
  GstCaps *caps;
  GstAvbPcmSink *sink = GST_AVB_PCM_SINK (basesink);
  if ((caps = sink->caps)) {
    if (filter)
      caps = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    else
      gst_caps_ref (caps);
  }
  GST_DEBUG_OBJECT (sink, "got caps %" GST_PTR_FORMAT, caps);

  return caps;

}

static gboolean
gst_avb_pcm_sink_process_buffer(GstAvbSink * sink, guint8 *srcBuffer,
  guint32 * offset, guint32 descSize, guint8 * descBuffer, guint64 * outDur)
{
  GstAvbPcmSink *pcmsink;
  guint8 * outPtr;
  guint32 usedSize = 0;
  int i;
  guint32 bpf = (pcmsink->channels / pcmsink->width) / 8;

  if(sink == NULL || srcBuffer == NULL || offset == NULL || descBuffer == NULL)
    return FALSE;

  pcmsink = GST_AVB_PCM_SINK(sink);
  outPtr = descBuffer;
  usedSize = *offset;

  for(i = 0; i < descSize/4; i++){
    *outPtr = pcmsink->label;
    outPtr ++;

    *outPtr = *(srcBuffer+usedSize);
    outPtr ++;
    usedSize ++;

    *outPtr = *(srcBuffer+usedSize);
    outPtr ++;
    usedSize ++;


    if(pcmsink->width == 24){
      *outPtr = *(srcBuffer+usedSize);
      outPtr ++;
      usedSize ++;
    }else{
      outPtr ++;
    }
  }
  //*outDur = gst_util_uint64_scale ((usedSize - *offset)/bpf, GST_SECOND, pcmsink->rate);
  *outDur = (guint64)(usedSize - *offset) * 8 * GST_SECOND/ pcmsink->channels / pcmsink->width / pcmsink->rate;
  *offset = usedSize;
  return TRUE;
}
static void
gst_avb_pcm_sink_class_init (GstAvbPcmSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstAvbSinkClass *gstavbsink_class;

  gobject_class = G_OBJECT_CLASS(klass);
  gstelement_class = GST_ELEMENT_CLASS(klass);
  gstbasesink_class = GST_BASE_SINK_CLASS(klass);
  gstavbsink_class = GST_AVBSINK_CLASS(klass);

  gstbasesink_class->set_caps = gst_avb_pcm_sink_set_caps;
  gstbasesink_class->get_caps = gst_avb_pcm_sink_get_caps;

  gstavbsink_class->get_dbs = gst_avb_pcm_sink_get_dbs;
  gstavbsink_class->get_fn = gst_avb_pcm_sink_get_fn;
  gstavbsink_class->get_sph = gst_avb_pcm_sink_get_sph;
  gstavbsink_class->get_dbc = gst_avb_pcm_sink_get_dbc;
  gstavbsink_class->get_fmt = gst_avb_pcm_sink_get_fmt;
  gstavbsink_class->get_fdf = gst_avb_pcm_sink_get_fdf;
  gstavbsink_class->get_syt = gst_avb_pcm_sink_get_syt;

  gstavbsink_class->get_packet_len = gst_avb_pcm_sink_get_packet_len;
  gstavbsink_class->process_buffer = gst_avb_pcm_sink_process_buffer;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  GST_DEBUG_CATEGORY_INIT (avb_pcm_sink_debug, "avbpcmsink", 0, "freescale avb pcm sink");

  gst_element_class_set_static_metadata (gstelement_class,"avb pcm sink",
      "Sink/Network",
      "Send raw pcm data over 1722 network",
      IMX_GST_PLUGIN_AUTHOR);

  GST_LOG_OBJECT (gobject_class,"gst_avb_pcm_sink_class_init");

}

static void gst_avb_pcm_sink_init (GstAvbPcmSink * sink)
{
  GstBaseSink *basesink;
  GstAvbSink *avbsink;

  sink->caps = NULL;
  sink->width = 0;
  sink->channels = 0;
  sink->rate = 0;
  sink->sfc = 0;
  sink->dbs = 0;
  sink->syt_internal = 0;
  sink->label = 0;

  basesink = GST_BASE_SINK(sink);
  avbsink = GST_AVBSINK(sink);

  gst_avbsink_set_package_count(avbsink,DEFAULT_PACKET_COUNT);

  gstsutils_load_default_property(basesink_property,basesink,
        FSL_GST_CONF_DEFAULT_FILENAME,"avbpcmsink");
  gstsutils_load_default_property(avbsink_property,avbsink,
        FSL_GST_CONF_DEFAULT_FILENAME,"avbpcmsink");

  GST_LOG_OBJECT (sink,"gst_avb_pcm_sink_init");
}


