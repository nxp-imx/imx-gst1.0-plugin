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
#include "gstavbpcmsrc.h"



#include "cip.h"
#include "avtp.h"
#include "ethernet.h"

#define PCM_AUDIO_CAPS \
  "audio/x-raw, "\
  "format = (string){S16LE, S24LE}, "\
  "rate = (int){32000, 44100, 48000, 88200, 96000, 176400, 192000}, "\
  "channels = (int)[1, 8]"

static GstStaticPadTemplate src_template =
  GST_STATIC_PAD_TEMPLATE ("src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (PCM_AUDIO_CAPS));

GST_DEBUG_CATEGORY_STATIC (avb_pcm_src_debug);
#define GST_CAT_DEFAULT avb_pcm_src_debug

#define gst_avb_pcm_src_parent_class parent_class
G_DEFINE_TYPE (GstAvbPcmSrc, gst_avb_pcm_src, GST_TYPE_AVBSRC);


typedef struct{
  gint32 rate;
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

static int gst_avb_pcm_src_isValid_cip_header(GstAvbSrc *src,CIP_HEADER * header)
{
  int ret = -1;

  if(src == NULL || header == NULL)
    return -1;

  if(GET_CIP_1QI(header) == 0 && GET_CIP_2QI(header) == 2 && GET_CIP_SID(header) == CIP_DEFAULT_SID
     && GET_CIP_FN(header) == 0 && GET_CIP_SPH(header) == 0
     && GET_CIP_FMT(header) == CIP_FMT_AUDIO )

  ret = 0;

  return ret;
}
static GstCaps * gst_avb_pcm_src_parse_and_get_caps(GstAvbSrc *src,guint8 * readData,gint readsize)
{
  CIP_HEADER * cipHeader;
  guint32 payloadLen;
  guint8 SFC;
  guint8 syt_internal;

  guint8 vbl = 0;
  gint bitPerSample;
  GstAvbPcmSrc * avbpcmsrc;
  gchar * mime = NULL;
  GstCaps * caps = NULL;
  guint8 * payloadData;
  GstAudioInfo info;
  GstAudioFormat audio_format;

  if(src == NULL || readData == NULL || readsize <= AVB_HEADER_SIZE)
    return NULL;

  avbpcmsrc = GST_AVB_PCM_SRC(src);
  cipHeader = (CIP_HEADER *)(readData + sizeof(ETHERNET_HEADER)+ sizeof(AVTPDU_DATA_HEADER));
  payloadLen = readsize - AVB_HEADER_SIZE;


  if(payloadLen % 4 != 0)
    GST_WARNING_OBJECT (avbpcmsrc,"payload size mod 4 not zero");

  SFC = GET_CIP_FDF(cipHeader) & 0x7;

  avbpcmsrc->rate = SFC_TABLE[SFC].rate;
  syt_internal = SFC_TABLE[SFC].syt_internal;

  avbpcmsrc->channels = GET_CIP_DBS(cipHeader);

  payloadData = readData + AVB_HEADER_SIZE;

  if((payloadData[0] >> 4) != 0x4){
    GST_WARNING_OBJECT (avbpcmsrc,"it is not raw pcm data");
    return NULL;
  }

  vbl = payloadData[0] & 0x3;

  if(vbl == 0){
    avbpcmsrc->bitPerSample = 24;
    GST_DEBUG_OBJECT (avbpcmsrc,"24 bit pcm");

  }else if(vbl == 2){
    avbpcmsrc->bitPerSample = 16;
    GST_DEBUG_OBJECT (avbpcmsrc,"16 bit pcm");
  }

  audio_format = gst_audio_format_build_integer (TRUE, G_BYTE_ORDER,
              avbpcmsrc->bitPerSample, avbpcmsrc->bitPerSample);

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, audio_format, avbpcmsrc->rate, avbpcmsrc->channels, NULL);

  caps = gst_audio_info_to_caps(&info);

  return caps;
}
static guint32 gst_avb_pcm_src_get_output_buffer_size(GstAvbSrc *src,guint32 pktsize)
{
  GstAvbPcmSrc * avbpcmsrc;

  if(src == NULL)
    return 0;

  avbpcmsrc = GST_AVB_PCM_SRC(src);

  if(avbpcmsrc->bitPerSample == 16)
    return pktsize/2;
  else
    return pktsize / 4 * 3;
}
static gboolean
gst_avb_pcm_src_process_buffer(GstAvbSrc * src, uint8 *inBuffer,
uint32 size,GstBuffer * outBuffer,uint32 * outSize)
{
  int i = 0;
  GstMapInfo info;
  guint32 src_offset = AVB_HEADER_SIZE;
  guint32 desc_offset = 0;
  guint8 * descPtr = NULL;
  guint32 outputBufferSize;
  guint32 len;
  guint32 payloadLen;
  GstAvbPcmSrc * avbpcmsrc;
  AVTPDU_DATA_HEADER * avtpHeader;
  CIP_HEADER * cipHeader;
  guint32 avtp_ts;


  if(src == NULL || inBuffer == NULL || outBuffer == NULL)
    return FALSE;

  avbpcmsrc = GST_AVB_PCM_SRC(src);

  outBuffer = gst_buffer_make_writable (outBuffer);

  gst_buffer_map (outBuffer, &info, GST_MAP_WRITE);
  descPtr = info.data;
  outputBufferSize = info.size;


  //since we have checked in gstavbsrc.c, we use payload len directly
  len = size;
  payloadLen = len - AVB_HEADER_SIZE;

  for(i =0; i < payloadLen/4; i++){
    src_offset ++;

    descPtr[desc_offset] = inBuffer[src_offset];
    desc_offset ++;
    src_offset ++;

    descPtr[desc_offset] = inBuffer[src_offset];
    desc_offset ++;
    src_offset ++;

    if(avbpcmsrc->bitPerSample == 24){
      descPtr[desc_offset] = inBuffer[src_offset];
      desc_offset ++;
      src_offset ++;
    }else if(avbpcmsrc->bitPerSample == 16){
      src_offset ++;
    }
  }
  info.size = desc_offset;
  gst_buffer_unmap (outBuffer, &info);
  * outSize = info.size;

  avtpHeader = (AVTPDU_DATA_HEADER *)(inBuffer + sizeof(ETHERNET_HEADER));
  cipHeader = (CIP_HEADER * )(inBuffer + sizeof(ETHERNET_HEADER) + sizeof(AVTPDU_DATA_HEADER));

  GST_BUFFER_DURATION(outBuffer) = (guint64)desc_offset * 8 * 1000000000 /avbpcmsrc->channels \
        /avbpcmsrc->bitPerSample/avbpcmsrc->rate;

  if(avbpcmsrc->output_ts == GST_CLOCK_TIME_NONE){
    GstClock *clock = NULL;
    GstClockTime output_ts,now;
    guint32 avtp_ts,u32_now;
    GstClockTime base_time;

    clock = GST_ELEMENT_CLOCK (avbpcmsrc);
    now = gst_clock_get_time (clock);
    base_time = GST_ELEMENT_CAST (avbpcmsrc)->base_time;

    u32_now = now & 0xFFFFFFFF;

    if(GET_AVTPDU_TV(avtpHeader)){
      avtp_ts = GET_AVTPDU_AVTP_TS(avtpHeader);

      if(avtp_ts >= u32_now)
        avbpcmsrc->output_ts = now - base_time + avtp_ts - u32_now;
      else
        avbpcmsrc->output_ts = now - base_time + avtp_ts + 0xFFFFFFFF - u32_now;

      GST_LOG_OBJECT (avbpcmsrc,"avtp_ts=%"GST_TIME_FORMAT,GST_TIME_ARGS (avtp_ts));
      GST_LOG_OBJECT (avbpcmsrc,"u32_now=%"GST_TIME_FORMAT,GST_TIME_ARGS (u32_now));
      GST_LOG_OBJECT (avbpcmsrc,"now=%"GST_TIME_FORMAT,GST_TIME_ARGS (now));

    }else{

      avbpcmsrc->output_ts = GST_CLOCK_TIME_NONE;
    }
  }else{
    avbpcmsrc->output_ts += GST_BUFFER_DURATION(outBuffer);
  }

  GST_LOG_OBJECT (avbpcmsrc,"ts=%"GST_TIME_FORMAT,GST_TIME_ARGS (avbpcmsrc->output_ts));

  GST_BUFFER_PTS(outBuffer) = GST_BUFFER_DTS(outBuffer) = avbpcmsrc->output_ts;

  return TRUE;
}
static void
gst_avb_pcm_src_reset_ts(GstAvbSrc * src)
{
  GstAvbPcmSrc * avbpcmsrc;
  avbpcmsrc = GST_AVB_PCM_SRC(src);
  avbpcmsrc->output_ts = GST_CLOCK_TIME_NONE;

  GST_DEBUG_OBJECT (avbpcmsrc,"gst_avb_pcm_src_reset_ts");
}
static void
gst_avb_pcm_src_class_init (GstAvbPcmSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstAvbSrcClass *gstavbsrc_class;

  gobject_class = G_OBJECT_CLASS(klass);
  gstelement_class = GST_ELEMENT_CLASS(klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS(klass);
  gstavbsrc_class = GST_AVBSRC_CLASS(klass);
  gstavbsrc_class->isValid_cip_header = gst_avb_pcm_src_isValid_cip_header;
  gstavbsrc_class->parse_and_get_caps = gst_avb_pcm_src_parse_and_get_caps;
  gstavbsrc_class->process_buffer = gst_avb_pcm_src_process_buffer;
  gstavbsrc_class->get_output_buffer_size = gst_avb_pcm_src_get_output_buffer_size;
  gstavbsrc_class->reset_ts = gst_avb_pcm_src_reset_ts;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  GST_DEBUG_CATEGORY_INIT (avb_pcm_src_debug, "avbpcmsrc", 0, "freescale avb pcm src");

  gst_element_class_set_static_metadata (gstelement_class,"avb pcm src",
      "Src/Network",
      "Receive raw pcm data over 1722 network",
      IMX_GST_PLUGIN_AUTHOR);

  GST_LOG_OBJECT (gobject_class,"gst_avb_pcm_src_class_init");

}

static void gst_avb_pcm_src_init (GstAvbPcmSrc * src)
{
  src->bitPerSample = 0;
  src->channels = 0;
  src->rate = 0;
  src->output_ts = GST_CLOCK_TIME_NONE;

  GST_LOG_OBJECT (src,"gst_avb_pcm_src_init");
}

