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
#include "gstavbmpegtssrc.h"

#include "cip.h"
#include "avtp.h"
#include "ethernet.h"

#define MPEGTS_CAPS \
  "video/mpegts, "\
  "systemstream = (boolean)true, " \
  "packetsize = (int)188"

#define OUTPUT_BUFFER_SIZE (188)
#define MPEGTS_TIMESTAMP_SIZE (4)
#define MPEGTS_PACKET_SIZE (188)

#define CLOCK_BASE 9LL

#define MPEGTIME_TO_GSTTIME(time) (gst_util_uint64_scale ((time), \
            GST_MSECOND/10, CLOCK_BASE))

static GstStaticPadTemplate src_template =
  GST_STATIC_PAD_TEMPLATE ("src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (MPEGTS_CAPS));

GST_DEBUG_CATEGORY_STATIC (avb_mpegts_src_debug);
#define GST_CAT_DEFAULT avb_mpegts_src_debug

#define gst_avb_mpegts_src_parent_class parent_class
G_DEFINE_TYPE (GstAvbMpegtsSrc, gst_avb_mpegts_src, GST_TYPE_AVBSRC);

static int gst_avb_mpegts_src_isValid_cip_header(GstAvbSrc *src,CIP_HEADER * header)
{
  int ret = -1;

  if(src == NULL || header == NULL)
    return -1;

  if(GET_CIP_1QI(header) == 0 && GET_CIP_2QI(header) == 2 && GET_CIP_SID(header) == CIP_DEFAULT_SID
     && GET_CIP_DBS(header) == 6 && GET_CIP_FN(header) == 3 && GET_CIP_SPH(header) == 1
     && GET_CIP_FMT(header) == CIP_FMT_MPEGTS && GET_CIP_SYT(header) == 0)
     ret = 0;

  return ret;
}
static GstCaps * gst_avb_mpegts_src_parse_and_get_caps(GstAvbSrc *src,guint8 * readData,gint readsize)
{
  GstCaps * caps = NULL;

  if(src == NULL || readData == NULL || readsize <= AVB_HEADER_SIZE)
    return NULL;

  caps = gst_caps_from_string (MPEGTS_CAPS);

  return caps;
}
static guint32 gst_avb_mpegts_src_get_output_buffer_size(GstAvbSrc *src,guint32 pktsize)
{
  guint32 count = 0;

  if(src == NULL)
    return 0;

  return (pktsize-MPEGTS_TIMESTAMP_SIZE);
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

static gboolean
gst_avb_mpegts_src_get_buffer_timestamp(GstAvbMpegtsSrc * src,guint8 * inBuffer, guint32 bufferSize,
  guint64 * pts_out, guint64* dts_out)
{
  gboolean ret = FALSE;
  int i = 0;
  guint32 packageCount = 0;
  guint32 offset = 0;
  guint8 * pes_buffer = NULL;
  guint8 adaptation = 0;
  guint8 start_indicator = 0;
  guint8 adaptation_len = 0;
  guint8 has_payload = 0;
  guint8 payload_offset = 8;//timestamp header + ts header
  guint8 strema_id = 0;
  guint8 flag = 0;
  guint64 pts = GST_CLOCK_TIME_NONE;
  guint64 dts = GST_CLOCK_TIME_NONE;
  gboolean get_time = FALSE;

  if(src == NULL || inBuffer == NULL || bufferSize == 0 || pts_out == NULL || dts_out == NULL)
    return ret;

  packageCount = bufferSize/MPEGTS_PACKET_SIZE;

  for(i = 0; i < packageCount; i++){
    offset = i*MPEGTS_PACKET_SIZE;

    if(inBuffer[offset] != 0x47){
      GST_LOG_OBJECT(src,"no sync byte");
      continue;
    }

    offset ++;
    start_indicator = inBuffer[offset] & 0x40;

    if(start_indicator == 0){
      GST_LOG_OBJECT(src,"no start_indicator");
      continue;
    }

    offset +=2;
    adaptation = ((inBuffer[offset] & 0x30) >> 4);
    has_payload = adaptation & 0x1;

    if(has_payload == 0){
      GST_LOG_OBJECT(src,"no has_payload");
      continue;
    }

    offset ++;

    if(adaptation > 1){
      adaptation_len = inBuffer[offset];
      offset += adaptation_len+1;
    }

    if(offset > bufferSize){
      GST_LOG_OBJECT(src,"payload offset is wrong");
      continue;
    }

    pes_buffer = inBuffer+offset;
    offset = 0;


    if(pes_buffer[offset] != 0 || pes_buffer[offset+1] != 0 || pes_buffer[offset+2] != 0x1){
      GST_LOG_OBJECT(src,"pes start code is wrong");
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
      GST_LOG_OBJECT(src,"pes buffer is wrong");
      continue;
    }

    offset ++;

    flag = pes_buffer[offset];
    offset +=2;

    GST_LOG_OBJECT(src,"flag=%x,offset=%d",flag,offset);

    if((flag & 0x80) == 0x80){
      if(read_ts_time(&pes_buffer[offset],&pts)){
        offset +=5;
        pts = MPEGTIME_TO_GSTTIME (pts);
        *pts_out = pts;
        ret = TRUE;
        GST_LOG_OBJECT(src,"read PTS=%"GST_TIME_FORMAT, GST_TIME_ARGS (pts));
      }
    }
    if((flag & 0x40) == 0x40) {
      if(read_ts_time(&pes_buffer[offset],&dts)){
        offset +=5;
        dts = MPEGTIME_TO_GSTTIME(dts);
        *dts_out = dts;
      }
    }
    if(ret)
      break;
  }

  return ret;

}
static void
gst_avb_mpegts_src_reset_ts(GstAvbSrc * src)
{
  GstAvbMpegtsSrc * avbmpegtssrc;
  avbmpegtssrc = GST_AVB_MPEGTS_SRC(src);
  avbmpegtssrc->last_output_ts = GST_CLOCK_TIME_NONE;

  GST_DEBUG_OBJECT (avbmpegtssrc,"gst_avb_pcm_src_reset_ts");
}


static gboolean
gst_avb_mpegts_src_process_buffer(GstAvbSrc * src, uint8 *srcBuffer,
  uint32 srcSize, GstBuffer * descBuffer, uint32 * outSize)
{
  GstMapInfo info;
  guint8 * descPtr = NULL;
  GstAvbMpegtsSrc * avbmpegtssrc;
  guint32 descBufferSize = 0;

  GstClock *clock = NULL;
  GstClockTime output_ts,now;
  guint32 avtp_ts,u32_now;
  GstClockTime base_time;

  if(src == NULL || srcBuffer == NULL || descBuffer == NULL)
    return FALSE;

  avbmpegtssrc = GST_AVB_MPEGTS_SRC(src);
  descBufferSize = srcSize - AVB_HEADER_SIZE - MPEGTS_TIMESTAMP_SIZE;

  descBuffer = gst_buffer_make_writable (descBuffer);

  gst_buffer_map (descBuffer, &info, GST_MAP_WRITE);

  descPtr = info.data;
  memcpy(descPtr,(srcBuffer + AVB_HEADER_SIZE + MPEGTS_TIMESTAMP_SIZE),descBufferSize);
  info.size = descBufferSize;

  gst_buffer_unmap (descBuffer, &info);

  *outSize = descBufferSize;

  memcpy(&avtp_ts, (srcBuffer + AVB_HEADER_SIZE), MPEGTS_TIMESTAMP_SIZE);

  if(avtp_ts != avbmpegtssrc->last_ts){

    avbmpegtssrc->last_ts = avtp_ts;

    clock = GST_ELEMENT_CLOCK (avbmpegtssrc);
    now = gst_clock_get_time (clock);
    base_time = GST_ELEMENT_CAST (avbmpegtssrc)->base_time;
    u32_now = now & 0xFFFFFFFF;

    if(avtp_ts >= u32_now)
      avbmpegtssrc->output_ts = now - base_time + avtp_ts - u32_now;
    else
      avbmpegtssrc->output_ts = now - base_time + avtp_ts + 0xFFFFFFFF - u32_now;

    if(avbmpegtssrc->last_output_ts != GST_CLOCK_TIME_NONE &&
      avbmpegtssrc->last_output_ts > avbmpegtssrc->output_ts)
      avbmpegtssrc->output_ts += 0xFFFFFFFF;

    avbmpegtssrc->last_output_ts = avbmpegtssrc->output_ts;

    GST_LOG_OBJECT (avbmpegtssrc,"avtp_ts=%"GST_TIME_FORMAT,GST_TIME_ARGS (avtp_ts));
    GST_LOG_OBJECT (avbmpegtssrc,"u32_now=%"GST_TIME_FORMAT,GST_TIME_ARGS (u32_now));

    GST_LOG_OBJECT (avbmpegtssrc,"now=%"GST_TIME_FORMAT",ts=%"GST_TIME_FORMAT,
      GST_TIME_ARGS (now),GST_TIME_ARGS (avbmpegtssrc->output_ts));
  }

  GST_BUFFER_PTS(descBuffer) = GST_BUFFER_DTS(descBuffer) = avbmpegtssrc->output_ts;
  GST_BUFFER_DURATION(descBuffer) = 0;

#if 0
  if(gst_avb_mpegts_src_get_buffer_timestamp(avbmpegtssrc,descPtr,descBufferSize,&pts,&dts)){
    if(avbmpegtssrc->start_ts == GST_CLOCK_TIME_NONE)
      avbmpegtssrc->start_ts = pts;

    //clock of pipeline has started when first buffer arrives, so adjust the buffer timestamp
    if(avbmpegtssrc->output_ts == GST_CLOCK_TIME_NONE){
        GstClockTime base_time,now;
        GstClock *clock = NULL;
        GstClockTimeDiff offset = 0;
        base_time = GST_ELEMENT_CAST (avbmpegtssrc)->base_time;
        clock = GST_ELEMENT_CLOCK (avbmpegtssrc);
        if(clock != NULL){
            now = gst_clock_get_time (clock);
            offset = now - base_time;
            if(offset > 0)
                avbmpegtssrc->output_ts = offset;
        }

        GST_DEBUG_OBJECT (avbmpegtssrc,"basetime=%"GST_TIME_FORMAT,GST_TIME_ARGS (base_time));
        GST_DEBUG_OBJECT (avbmpegtssrc,"now=%"GST_TIME_FORMAT,GST_TIME_ARGS (now));
        GST_DEBUG_OBJECT (avbmpegtssrc,"ts=%"GST_TIME_FORMAT,GST_TIME_ARGS (avbmpegtssrc->output_ts));
    }
    GST_DEBUG_OBJECT(src,"process_buffer output_ts=%"GST_TIME_FORMAT",pts=%"GST_TIME_FORMAT",last ts=%"GST_TIME_FORMAT
      , GST_TIME_ARGS (avbmpegtssrc->output_ts),GST_TIME_ARGS (pts),GST_TIME_ARGS (avbmpegtssrc->start_ts));

    GST_BUFFER_PTS(descBuffer) = avbmpegtssrc->output_ts + pts - avbmpegtssrc->start_ts;
    GST_BUFFER_DTS(descBuffer) = GST_CLOCK_TIME_NONE;

    GST_DEBUG_OBJECT(src,"process_buffer PTS=%"GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_PTS(descBuffer)));
  }else{
    GST_BUFFER_PTS(descBuffer) = GST_BUFFER_DTS(descBuffer) = GST_CLOCK_TIME_NONE;
    GST_DEBUG_OBJECT(src,"process_buffer NULL PTS");
  }
  GST_BUFFER_DURATION(descBuffer) = 0;
#endif
  return TRUE;
}

static void
gst_avb_mpegts_src_class_init (GstAvbMpegtsSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstAvbSrcClass *gstavbsrc_class;

  gobject_class = G_OBJECT_CLASS(klass);
  gstelement_class = GST_ELEMENT_CLASS(klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS(klass);
  gstavbsrc_class = GST_AVBSRC_CLASS(klass);

  gstavbsrc_class->isValid_cip_header = gst_avb_mpegts_src_isValid_cip_header;
  gstavbsrc_class->parse_and_get_caps = gst_avb_mpegts_src_parse_and_get_caps;
  gstavbsrc_class->process_buffer = gst_avb_mpegts_src_process_buffer;
  gstavbsrc_class->get_output_buffer_size = gst_avb_mpegts_src_get_output_buffer_size;
  gstavbsrc_class->reset_ts = gst_avb_mpegts_src_reset_ts;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  GST_DEBUG_CATEGORY_INIT (avb_mpegts_src_debug, "avbmpegtssrc", 0, "freescale avb mpegts src");

  gst_element_class_set_static_metadata (gstelement_class,"avb mpegts src",
      "Src/Network",
      "Receive mpegts data over 1722 network",
      IMX_GST_PLUGIN_AUTHOR);

  GST_LOG_OBJECT (gobject_class,"gst_avb_mpegts_src_class_init");

}

static void gst_avb_mpegts_src_init (GstAvbMpegtsSrc * src)
{
  src->output_ts = GST_CLOCK_TIME_NONE;
  src->last_output_ts = GST_CLOCK_TIME_NONE;
  src->last_ts = 0;

  GST_LOG_OBJECT (src,"gst_avb_mpegts_src_init");
}

