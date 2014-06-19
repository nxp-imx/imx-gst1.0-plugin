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
#include "gstavbmpegtssink.h"
#include "avtp.h"
#include "cip.h"

#define MPEGTS_PACKET_SIZE (188)
#define MPEGTS_MAX_PACKAGE_IN_CIP (7)//count of 188 package in one avb package
#define MPEGTS_TIMESTAMP_SIZE (4)
#define MPEGTS_DEFAULT_DBS (6)//according to IEEE61883-4, set dbs to 6

#define CLOCK_BASE 9LL

#define MPEGTIME_TO_GSTTIME(time) (gst_util_uint64_scale ((time), \
            GST_MSECOND/10, CLOCK_BASE))
#define GSTTIME_TO_MPEGTIME(time) (gst_util_uint64_scale ((time), \
            CLOCK_BASE, GST_MSECOND/10))
#define PCRTIME_TO_GSTTIME(t) ((t) * 1000 / 27)


GST_DEBUG_CATEGORY_STATIC (avb_mpegts_sink_debug);
#define GST_CAT_DEFAULT avb_mpegts_sink_debug

#define gst_avb_mpegts_sink_parent_class parent_class
G_DEFINE_TYPE (GstAvbMpegtsSink, gst_avb_mpegts_sink, GST_TYPE_AVBSINK);

#define MPEGTS_CAPS \
  "video/mpegts, "\
  "systemstream = (boolean)true, " \
  "packetsize = (int)188"

static GstStaticPadTemplate sink_template =
  GST_STATIC_PAD_TEMPLATE ("sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (MPEGTS_CAPS));

static gstsutils_property avbsink_property[]=
{
  {"latency", G_TYPE_UINT, gst_avbsink_set_latency},
  {"use_ptp_time", G_TYPE_BOOLEAN, gst_avbsink_set_use_ptp_time},
  {"package_count",G_TYPE_UINT,gst_avbsink_set_package_count}
};
static gstsutils_property basesink_property[]=
{
    {"sync", G_TYPE_BOOLEAN, gst_base_sink_set_sync}
};

static guint8 gst_avb_mpegts_sink_get_dbs(GstAvbSink * sink)
{
  GstAvbMpegtsSink *mpegtssink;
  mpegtssink = GST_AVB_MPEGTS_SINK(sink);
  return mpegtssink->dbs;
}
static guint8 gst_avb_mpegts_sink_get_fn(GstAvbSink * sink)
{
  return 3;
}
static guint8 gst_avb_mpegts_sink_get_sph(GstAvbSink * sink)
{
  return 1;
}
static guint8 gst_avb_mpegts_sink_get_dbc(GstAvbSink * sink,guint32 payloadLen)
{
  GstAvbMpegtsSink *mpegtssink;
  mpegtssink = GST_AVB_MPEGTS_SINK(sink);
  return payloadLen/mpegtssink->dbs/4;
}
static guint8 gst_avb_mpegts_sink_get_fmt(GstAvbSink * sink)
{
  return CIP_FMT_MPEGTS;
}
static guint8 gst_avb_mpegts_sink_get_fdf(GstAvbSink * sink)
{
  //this may need a property to config
  //(1 << 7) time-shifted, 0 non-time-shifted
  return 0;
}
static guint16 gst_avb_mpegts_sink_get_syt(GstAvbSink * sink)
{
  return 0;
}

static guint32 gst_avb_mpegts_sink_get_packet_len(GstAvbSink * sink,guint32 leftsize)
{
  guint32 count = 0;
  guint32 max_count = 0;

  max_count = gst_avbsink_get_package_count(sink);
  count = leftsize/MPEGTS_PACKET_SIZE;

  if(count > max_count)
    count = max_count;

  if(count == 0)
    count = 1;

  return (count * MPEGTS_PACKET_SIZE+MPEGTS_TIMESTAMP_SIZE);
}
static gboolean
gst_avb_mpegts_sink_set_caps (GstBaseSink * basesink, GstCaps * caps)
{
  gboolean ret;
  GstAvbMpegtsSink *sink;
  const gchar *mimetype;
  GstStructure *structure;

  sink = GST_AVB_MPEGTS_SINK (basesink);

  if (G_UNLIKELY (sink->caps && gst_caps_is_equal (sink->caps, caps))) {
    GST_DEBUG_OBJECT (sink,
        "caps haven't changed, skipping reconfiguration");
    return TRUE;
  }

  structure = gst_caps_get_structure (caps, 0);
  GST_DEBUG_OBJECT (sink,"gst_avb_mpegts_sink_set_caps %"GST_PTR_FORMAT,caps);

  /* we have to differentiate between int and float formats */
  mimetype = gst_structure_get_name (structure);

  if (!g_str_equal (mimetype, "video/mpegts"))
    return FALSE;

  sink->caps = caps;

  return TRUE;
}
static GstCaps *
gst_avb_mpegts_sink_get_caps (GstBaseSink * basesink, GstCaps * filter)
{
  GstCaps *caps;
  GstAvbMpegtsSink *sink = GST_AVB_MPEGTS_SINK (basesink);
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
gst_avb_mpegts_sink_process_buffer(GstAvbSink * sink, guint8 *srcBuffer,
  guint32 * offset, guint32 descSize, guint8 * descBuffer, guint64 * outDur)
{
  GstAvbMpegtsSink *mpegtssink;
  guint32 usedSize = 0;

  if(sink == NULL || srcBuffer == NULL || offset == NULL || descBuffer == NULL)
    return FALSE;

  mpegtssink = GST_AVB_MPEGTS_SINK(sink);
  usedSize = *offset;

  memcpy(descBuffer+MPEGTS_TIMESTAMP_SIZE,srcBuffer+usedSize,descSize-MPEGTS_TIMESTAMP_SIZE);
  usedSize += (descSize-MPEGTS_TIMESTAMP_SIZE);
  *outDur = 0;
  *offset = usedSize;
  return TRUE;
}
static gboolean write_ts_time(guint8 *inBuffer,guint64 time)
{
  guint8 start = 0;

  if(inBuffer == NULL)
    return FALSE;

  if((inBuffer[0] & 0x01) != 0x01)
    return FALSE;
  if((inBuffer[2] & 0x01) != 0x01)
    return FALSE;
  if((inBuffer[4] & 0x01) != 0x01)
    return FALSE;

  inBuffer[0] = inBuffer[0] & 0xF0;

  inBuffer[0] |= (time >> 29) & 0x7;
  inBuffer[0] |= 0x1;

  inBuffer[1] = (time >> 22) & 0xFF;
  inBuffer[2] = 0x1;
  inBuffer[2] |= (time >> 14) & 0xFE;

  inBuffer[3] = (time >> 7) & 0xFF;
  inBuffer[4] = 0x1;
  inBuffer[4] |= (time << 1) & 0xFE;

  return TRUE;
}
static gboolean read_ts_time(guint8 *inBuffer,guint64 * time)
{
  guint64 out_time = 0;

  if(inBuffer == NULL || time == NULL)
    return FALSE;

  if((inBuffer[0] & 0x01) != 0x01)
    return FALSE;
  if((inBuffer[2] & 0x01) != 0x01)
    return FALSE;
  if((inBuffer[4] & 0x01) != 0x01)
    return FALSE;

  out_time  = ((guint64)(inBuffer[0] & 0x0E) << 29);
  out_time |= ((guint64)inBuffer[1] << 22);
  out_time |= ((guint64)(inBuffer[2]& 0xFE) << 14);
  out_time |= ((guint64)inBuffer[3] << 7);
  out_time |= ((guint64)(inBuffer[4]& 0xFE) >> 1);

  *time = out_time;
  return TRUE;
}
static inline guint64
get_mpegts_pcr (guint8 * data)
{
  guint32 pcr1;
  guint16 pcr2;
  guint64 pcr, pcr_ext;

  pcr1 = GST_READ_UINT32_BE (data);
  pcr2 = GST_READ_UINT16_BE (data + 4);
  pcr = ((guint64) pcr1) << 1;
  pcr |= (pcr2 & 0x8000) >> 15;
  pcr_ext = (pcr2 & 0x01ff);
  return pcr * 300 + pcr_ext % 300;
}

static gboolean
gst_avb_mpegts_sink_rewrite_time(GstAvbSink * sink, guint8 *inBuffer, guint32 bufferSize, guint64 ptp_ts, guint64 ts)
{
  GstAvbMpegtsSink *mpegtssink;
  int i = 0;
  guint32 packageCount = 0;
  guint32 in_ts;
#if 1
  guint32 offset = 0;
  guint32 latency = 0;
  guint8 * pes_buffer = NULL;
  guint8 adaptation = 0;
  guint8 start_indicator = 0;
  guint8 adaptation_len = 0;
  guint8 has_payload = 0;
  guint8 strema_id = 0;
  guint8 flag = 0;
  guint64 pts = 0;
  guint64 desc_pts = 0;
  guint64 test_pts = 0;
  guint64 dts = 0;
  gboolean write = FALSE;
  guint8 adaptation_flag = 0;
#endif
  if(sink == NULL || inBuffer == NULL)
    return FALSE;

  mpegtssink = GST_AVB_MPEGTS_SINK(sink);



  //copy ptp time to header mpeg ts header

  //todo: convert PTP time
  in_ts = AVTPDU_GET_U32_TS(ptp_ts);
  memcpy(inBuffer,&in_ts,MPEGTS_TIMESTAMP_SIZE);

  GST_LOG_OBJECT(mpegtssink,"gst_avb_mpegts_sink_rewrite_time ptp_ts=%"GST_TIME_FORMAT",u23=%"GST_TIME_FORMAT,
    GST_TIME_ARGS (ptp_ts),GST_TIME_ARGS (in_ts));

  //write latency into pst time in pes header
  #if 0
  offset = MPEGTS_TIMESTAMP_SIZE;

  packageCount = (bufferSize-MPEGTS_TIMESTAMP_SIZE)/MPEGTS_PACKET_SIZE;

  for(i = 0; i < packageCount; i++){
    offset = MPEGTS_TIMESTAMP_SIZE + i*MPEGTS_PACKET_SIZE;

    if(inBuffer[offset] != 0x47){
      GST_LOG_OBJECT(mpegtssink,"no sync byte");
      continue;
    }

    offset ++;
    start_indicator = inBuffer[offset] & 0x40;

    if(start_indicator == 0){
      GST_LOG_OBJECT(mpegtssink,"no start_indicator");
      continue;
    }

    offset +=2;
    adaptation = ((inBuffer[offset] & 0x30) >> 4);
    has_payload = adaptation & 0x1;

    if(has_payload == 0){
      GST_LOG_OBJECT(mpegtssink,"no has_payload");
      continue;
    }

    offset ++;

    if(adaptation > 1){
      adaptation_len = inBuffer[offset];

      adaptation_flag = inBuffer[offset+1];

      if(adaptation_flag & 0x10){
        guint64 pcr_gst;
        pcr_gst = PCRTIME_TO_GSTTIME(get_mpegts_pcr(&inBuffer[offset+2]));
        mpegtssink->pcr = GSTTIME_TO_MPEGTIME(pcr_gst);
        GST_DEBUG_OBJECT(mpegtssink,"pcr=%"GST_TIME_FORMAT,GST_TIME_ARGS(mpegtssink->pcr));
      }
      offset += adaptation_len+1;
    }

    if(offset > bufferSize){
      GST_LOG_OBJECT(mpegtssink,"payload offset is wrong");
      continue;
    }

    pes_buffer = inBuffer+offset;
    offset = 0;


    if(pes_buffer[offset] != 0 || pes_buffer[offset+1] != 0 || pes_buffer[offset+2] != 0x1){
      GST_LOG_OBJECT(mpegtssink,"pes start code is wrong");
      continue;
    }


    offset += 3;
    strema_id = pes_buffer[offset];
    offset ++;

    #if 0
    if(strema_id == 0xbc || strema_id == 0xbe
            || strema_id == 0xbf || (strema_id >= 0xf0
                && strema_id <= 0xf2) || strema_id == 0xff
            || strema_id == 0xf8)
      return FALSE;
    #endif

    //skip pes length
    offset += 2;

    if((pes_buffer[offset] >> 6) != 0x2){//first 2 bit is 10
      GST_LOG_OBJECT(mpegtssink,"pes buffer is wrong");
      continue;
    }

    offset ++;

    flag = pes_buffer[offset];
    offset +=2;

    GST_LOG_OBJECT(mpegtssink,"flag=%x,offset=%d",flag,offset);

    if((flag & 0x80) == 0x80){
      if(read_ts_time(&pes_buffer[offset],&pts)){
        if(mpegtssink->pcr_offset == 0 && mpegtssink->pcr > 0 && mpegtssink->pcr < pts){
          mpegtssink->pcr_offset = pts - mpegtssink->pcr;
          GST_DEBUG_OBJECT(mpegtssink,"pcr=%"GST_TIME_FORMAT",pcr_offset=%"GST_TIME_FORMAT,
            GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (mpegtssink->pcr)),GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (mpegtssink->pcr_offset)));
        }


        desc_pts = GSTTIME_TO_MPEGTIME(ts) + mpegtssink->pcr/* + GSTTIME_TO_MPEGTIME(sink->latency)*/;
        write_ts_time(&pes_buffer[offset],desc_pts);
        read_ts_time(&pes_buffer[offset],&test_pts);//read again to check the value
        GST_DEBUG_OBJECT(mpegtssink,"read PTS=%"GST_TIME_FORMAT",write PTS=%"GST_TIME_FORMAT" inTime=%"GST_TIME_FORMAT,
          GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (pts)),GST_TIME_ARGS(MPEGTIME_TO_GSTTIME(desc_pts)),
          GST_TIME_ARGS(MPEGTIME_TO_GSTTIME(test_pts)));
        offset +=5;
        write = TRUE;
      }
    }
    if((flag & 0x40) == 0x40) {
      if(read_ts_time(&pes_buffer[offset],&dts)){

        //pts = MPEGTIME_TO_GSTTIME(pts);
        desc_pts = GSTTIME_TO_MPEGTIME(ts) + mpegtssink->pcr;
        GST_DEBUG_OBJECT(mpegtssink,"read DTS=%"GST_TIME_FORMAT",write DTS=%"GST_TIME_FORMAT,
          GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (dts)),GST_TIME_ARGS(MPEGTIME_TO_GSTTIME(desc_pts)));
        write_ts_time(&pes_buffer[offset],desc_pts);
        offset +=5;
      }
    }
    if(write)
      break;
  }
  #endif
  return TRUE;
}

static void
gst_avb_mpegts_sink_class_init (GstAvbMpegtsSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstAvbSinkClass *gstavbsink_class;

  gobject_class = G_OBJECT_CLASS(klass);
  gstelement_class = GST_ELEMENT_CLASS(klass);
  gstbasesink_class = GST_BASE_SINK_CLASS(klass);
  gstavbsink_class = GST_AVBSINK_CLASS(klass);

  gstbasesink_class->set_caps = gst_avb_mpegts_sink_set_caps;
  gstbasesink_class->get_caps = gst_avb_mpegts_sink_get_caps;

  gstavbsink_class->get_dbs = gst_avb_mpegts_sink_get_dbs;
  gstavbsink_class->get_fn = gst_avb_mpegts_sink_get_fn;
  gstavbsink_class->get_sph = gst_avb_mpegts_sink_get_sph;
  gstavbsink_class->get_dbc = gst_avb_mpegts_sink_get_dbc;
  gstavbsink_class->get_fmt = gst_avb_mpegts_sink_get_fmt;
  gstavbsink_class->get_fdf = gst_avb_mpegts_sink_get_fdf;
  gstavbsink_class->get_syt = gst_avb_mpegts_sink_get_syt;

  gstavbsink_class->get_packet_len = gst_avb_mpegts_sink_get_packet_len;
  gstavbsink_class->process_buffer = gst_avb_mpegts_sink_process_buffer;
  gstavbsink_class->rewrite_time = gst_avb_mpegts_sink_rewrite_time;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  GST_DEBUG_CATEGORY_INIT (avb_mpegts_sink_debug, "avbmpegtssink", 0, "freescale avb mpegts sink");

  gst_element_class_set_static_metadata (gstelement_class,"avb mpegts sink",
      "Sink/Network",
      "Send mpegts data over 1722 network",
      IMX_GST_PLUGIN_AUTHOR);


  GST_LOG_OBJECT (gobject_class,"gst_avb_mpegts_sink_class_init");

}

static void gst_avb_mpegts_sink_init (GstAvbMpegtsSink * sink)
{
  GstBaseSink *basesink;
  GstAvbSink *avbsink;

  sink->caps = NULL;

  sink->dbs = MPEGTS_DEFAULT_DBS;

  sink->pcr = 0;
  sink->pcr_offset = 0;

  basesink = GST_BASE_SINK(sink);
  avbsink = GST_AVBSINK(sink);

  gst_avbsink_set_package_count(avbsink,MPEGTS_MAX_PACKAGE_IN_CIP);

  gstsutils_load_default_property(basesink_property,basesink,
        FSL_GST_CONF_DEFAULT_FILENAME,"avbmpegtssink");
  gstsutils_load_default_property(avbsink_property,avbsink,
        FSL_GST_CONF_DEFAULT_FILENAME,"avbmpegtssink");

  sink->latency = gst_avbsink_get_latency(avbsink);
  GST_LOG_OBJECT (sink,"gst_avb_mpegts_sink_init");
}

