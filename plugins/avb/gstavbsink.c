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
#include <string.h>
#include <gst/tag/tag.h>
#include "gstavbsink.h"
#include "ethernet.h"
#include "avtp.h"
#include "cip.h"
#include <gst/gst.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netdb.h>
#include "gstavbclock.h"

enum
{
  PROP_0,
  PROP_LATENCY,
  PROP_USE_PTP_TS,
  PROP_PACKAGE_COUNT
};

//subclass may change the default value
#define AVBSINK_DEFAULT_PACKAGE_COUNT 1
#define AVBSINK_DEFAULT_LATENCY -1
#define AVBSINK_DEFAULT_LATENCY_LIVE  100000000  //100ms
#define AVBSINK_DEFAULT_LATENCY_LOCAL 1000000000 //1.5s

#define AVBSINK_DEFAULT_USE_PTP_TS TRUE

#define AVB_HEADER_SIZE (sizeof(ETHERNET_HEADER) + sizeof(AVTPDU_DATA_HEADER) + sizeof(CIP_HEADER))


GST_DEBUG_CATEGORY_STATIC (avbsink_debug);
#define GST_CAT_DEFAULT avbsink_debug

#define gst_avbsink_parent_class parent_class
G_DEFINE_TYPE (GstAvbSink, gst_avbsink, GST_TYPE_BASE_SINK);


void
gst_avbsink_set_latency(GstAvbSink *avbsink, guint32 latency)
{
  g_return_if_fail (GST_IS_AVBSINK (avbsink));

  GST_OBJECT_LOCK (avbsink);
  avbsink->latency = latency;
  GST_OBJECT_UNLOCK (avbsink);
}
guint32
gst_avbsink_get_latency(GstAvbSink *avbsink)
{
  guint32 res;
  g_return_val_if_fail (GST_IS_AVBSINK (avbsink), 0);

  GST_OBJECT_LOCK (avbsink);
  res = avbsink->latency;
  GST_OBJECT_UNLOCK (avbsink);

  return res;
}

void gst_avbsink_set_package_count(GstAvbSink *avbsink, guint32 count)
{
  g_return_if_fail (GST_IS_AVBSINK (avbsink));

  GST_OBJECT_LOCK (avbsink);
  avbsink->package_count = count;
  GST_OBJECT_UNLOCK (avbsink);
}
guint32 gst_avbsink_get_package_count(GstAvbSink *avbsink)
{
  guint32 res;
  g_return_val_if_fail (GST_IS_AVBSINK (avbsink), 0);

  GST_OBJECT_LOCK (avbsink);
  res = avbsink->package_count;
  GST_OBJECT_UNLOCK (avbsink);

  return res;
}

void
gst_avbsink_set_use_ptp_time(GstAvbSink *avbsink, gboolean enabled)
{
  g_return_if_fail (GST_IS_AVBSINK (avbsink));

  GST_OBJECT_LOCK (avbsink);
  avbsink->use_ptp_time = enabled;
  GST_OBJECT_LOCK (avbsink);
}
gboolean
gst_avbsink_get_use_ptp_time (GstAvbSink * avbsink)
{
  gboolean res;
  g_return_val_if_fail (GST_IS_AVBSINK (avbsink), FALSE);
  GST_OBJECT_LOCK (avbsink);
  res = avbsink->use_ptp_time;
  GST_OBJECT_LOCK (avbsink);

  return res;
}

static void avbsink_query_and_set_latency(GstAvbSink *sink)
{
  gboolean ret = FALSE;
  GstBaseSink *basesink;

  gboolean is_live;
  GstClockTime min_latency;
  GstClockTime max_latency;

  basesink = GST_BASE_SINK(sink);

  ret = gst_base_sink_query_latency(basesink,NULL,&is_live,&min_latency,&max_latency);
  GST_DEBUG_OBJECT(sink,"query_latency ret=%d,live=%d",ret,is_live);

  if(ret && sink->latency == AVBSINK_DEFAULT_LATENCY){
    if(is_live)
      gst_avbsink_set_latency(sink,AVBSINK_DEFAULT_LATENCY_LIVE);
    else
      gst_avbsink_set_latency(sink,AVBSINK_DEFAULT_LATENCY_LOCAL);
  }

}

//write static value for cip header
static gboolean
avbsink_prepare(GstAvbSink *sink)
{
  guint8 dbs;
  guint8 fn;
  guint8 fmt;
  guint8 fdf;
  guint16 syt;

  CIP_HEADER * cip_header = NULL;

  GstAvbSinkClass *avbsinkclass;

  if(sink == NULL){
    return FALSE;
  }

  avbsinkclass = GST_AVBSINK_GET_CLASS (sink);

  cip_header = (CIP_HEADER *)(sink->header + sizeof(ETHERNET_HEADER) + sizeof(AVTPDU_DATA_HEADER));

  dbs = avbsinkclass->get_dbs(sink);
  fn = avbsinkclass->get_fn(sink);
  sink->sph = avbsinkclass->get_sph(sink);

  fmt = avbsinkclass->get_fmt(sink);
  fdf = avbsinkclass->get_fdf(sink);
  syt = avbsinkclass->get_syt(sink);

  SET_CIP_DBS(cip_header,dbs);
  SET_CIP_FN(cip_header,fn);
  SET_CIP_SPH(cip_header,sink->sph);
  //do not set dbc here because it maybe change in different package
  SET_CIP_FMT(cip_header,fmt);
  SET_CIP_FDF(cip_header,fdf);
  SET_CIP_SYT(cip_header,syt);

  avbsink_query_and_set_latency(sink);

  GST_DEBUG_OBJECT(sink,"prepare success,fmt=%x,fdf=%x",fmt,fdf);

  return TRUE;
}

static int avbsink_render_buffer(GstAvbSink *avbsink,GstBuffer * buffer)
{
  int ret = 0;
  GstAvbSinkClass *avbsinkclass;

  AVTPDU_DATA_HEADER * avtp_header = NULL;
  CIP_HEADER * cip_header = NULL;

  GstMapInfo info;
  guint32 bufferSize = 0;
  guint8 * bufferData;

  guint32 payloadLen = 0;
  guint32 newBufferLen = 0;
  guint8 * newBuffer;
  guint8 * bufferPtr;
  GstClockTime timestamp;

  guint32 consumeLen = 0;
  guint16 streamLen;
  guint64 outDur;


  if(avbsink == NULL || buffer == NULL){
    ret = -1;
    goto bail;
  }
  avbsinkclass = GST_AVBSINK_GET_CLASS (avbsink);

  gst_buffer_map(buffer,&info,GST_MAP_READ);
  bufferSize = info.size;
  bufferData = info.data;
  gst_buffer_unmap(buffer,&info);

  /* first use DTS, else use PTS */
  //timestamp = GST_BUFFER_DTS (buffer);
  //if (!GST_CLOCK_TIME_IS_VALID (timestamp))
  timestamp = GST_BUFFER_PTS (buffer);

  //get ptp time for the first time, then use buffer time stamp
  if(avbsink->ts == GST_CLOCK_TIME_NONE){
    if(avbsink->use_ptp_time &&
      gst_avb_clock_get_gptp_time(avbsink->ptp_fd, avbsink->net_name, &avbsink->ts)){
        GST_DEBUG_OBJECT(avbsink,"gptp_time=%lld,latency=%d",avbsink->ts ,avbsink->latency);
        avbsink->ts += avbsink->latency;
    }else{
      avbsink->ts = timestamp + avbsink->latency;
    }
    avbsink->start_ts = timestamp;
  }

  GST_DEBUG_OBJECT(avbsink,"avbsink_render_buffer size=%d,%"GST_TIME_FORMAT ,
    bufferSize,GST_TIME_ARGS(timestamp));

  //send all buffers to avb socket
  while(consumeLen < bufferSize){

    payloadLen = avbsinkclass->get_packet_len(avbsink,(bufferSize - consumeLen));

    newBufferLen = payloadLen + AVB_HEADER_SIZE;

    newBuffer = g_try_malloc (newBufferLen * sizeof(guint8));
    if(NULL == newBuffer){
      GST_ERROR_OBJECT(avbsink,"Process_AVB_Audio_Package malloc failed");
      ret = -1;
      goto bail;
    }
    memset(newBuffer,0,newBufferLen * sizeof(guint8));

    //write common data got from default header
    memcpy(newBuffer, avbsink->header, AVB_HEADER_SIZE);

    avtp_header = (AVTPDU_DATA_HEADER *)(newBuffer + sizeof(ETHERNET_HEADER));
    cip_header = (CIP_HEADER *)(newBuffer + sizeof(ETHERNET_HEADER) + sizeof(AVTPDU_DATA_HEADER));

    //increase 1 each time send a avb package for listener to detect transmission lost
    SET_AVTPDU_SEQUENCE_NUM(avtp_header,avbsink->sequence_num);
    avbsink->sequence_num ++;

    //write the stream len into avtp header
    streamLen = (uint16)(payloadLen + sizeof(CIP_HEADER));
    SET_AVTPDU_STREAM_DATA_LEN(avtp_header,streamLen);

    //set the index of first data block in the stream data.
    SET_CIP_DBC(cip_header,avbsink->data_block_count);

    //get the data block count in the stream data.
    avbsink->data_block_count += avbsinkclass->get_dbc(avbsink,payloadLen);
    //GST_LOG_OBJECT(avbsink,"render payloadLen=%d,consumeLen=%d,data_block_count=%lld"
    //  ,payloadLen,consumeLen,avbsink->data_block_count);


    bufferPtr = newBuffer + AVB_HEADER_SIZE;

    //fill the cip stream data
    if(!avbsinkclass->process_buffer(avbsink,bufferData,&consumeLen,payloadLen,bufferPtr,&outDur)){
      ret = -1;
      g_free(newBuffer);
      GST_ERROR_OBJECT(avbsink,"process_buffer failed");
      goto bail;
    }

    //set avtp timestamp
    if(avbsink->sph == 0){
      SET_AVTPDU_TV(avtp_header,AVTPDU_TV_VALID);
      SET_AVTPDU_AVTP_TS(avtp_header,AVTPDU_GET_U32_TS(avbsink->ts));
      GST_LOG_OBJECT(avbsink,"render 0 ptp_ts=%"GST_TIME_FORMAT",u32_ts=%"GST_TIME_FORMAT,
        GST_TIME_ARGS(avbsink->ts),GST_TIME_ARGS(AVTPDU_GET_U32_TS(avbsink->ts)));

      avbsink->ts += outDur;
    }else if(avbsink->sph == 1){
      guint64 ptp_ts;
      ptp_ts = avbsink->ts + timestamp - avbsink->start_ts;
      GST_LOG_OBJECT(avbsink,"render 1 ptp_ts=%"GST_TIME_FORMAT",u32_ts=%"GST_TIME_FORMAT,
        GST_TIME_ARGS(ptp_ts),GST_TIME_ARGS(AVTPDU_GET_U32_TS(ptp_ts)));

      //write the timestamp into stream data.
      if(avbsinkclass->rewrite_time)
        avbsinkclass->rewrite_time(avbsink,bufferPtr,payloadLen,ptp_ts,timestamp);
    }

    ret = sendto(avbsink->avb_fd, newBuffer, newBufferLen,
      0, (struct sockaddr *)avbsink->avb_sll, sizeof(*avbsink->avb_sll));

#if 0
    FILE * pfile;
    pfile = fopen("/home/root/temp/Test/1.dat","ab");
    if(pfile){
        fwrite(newBuffer,1,newBufferLen,pfile);
        fclose(pfile);
    }
#endif

    if (newBufferLen != ret){
      GST_ERROR_OBJECT(avbsink,"send failed,newBufferLen=%d,ret=%d",newBufferLen,ret);
      ret = -1;
      goto bail;
    }else{
      ret = 0;
    }
    g_free(newBuffer);

  }

bail:
  GST_DEBUG_OBJECT(avbsink,"render END ret=%d",ret);

  return ret;
}


static gboolean
avbsink_create_socket(GstAvbSink *sink)
{

  int sockfd, intrface;
  struct sockaddr_ll sll, *psll = NULL;
  struct ifreq ifstruct[16];
  struct ifconf ifc;
  int receiveSize = 200000;
  struct sockaddr_in udp_addr;
  guint bc_val;
  gint reuse = 1;
  struct ifreq ptp_ifreq;

  if(sink == NULL)
      return FALSE;

  sockfd = socket (PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sockfd < 0) {
      return FALSE;
  }

  memset(&sll, 0, sizeof(sll));
  sll.sll_family = PF_PACKET;
  sll.sll_protocol = htons(ETH_P_ALL);

  ifc.ifc_len = sizeof(ifstruct);
  ifc.ifc_buf =  (char*)&ifstruct;
  ioctl (sockfd, SIOCGIFCONF, (char *) &ifc);
  intrface = ifc.ifc_len / sizeof (struct ifreq);

  while (!strcmp("lo", ifstruct[intrface--].ifr_name)){
  ;
  }
  GST_DEBUG_OBJECT (sink,"net device %s\n", ifstruct[intrface].ifr_name);


  ioctl(sockfd, SIOCGIFINDEX, &ifstruct[intrface]);
  sll.sll_ifindex = ifstruct[intrface].ifr_ifindex;

  /* get local mac addr */
  ioctl(sockfd, SIOCGIFHWADDR, &ifstruct[intrface]);
  memcpy(sll.sll_addr, ifstruct[intrface].ifr_ifru.ifru_hwaddr.sa_data, ETH_ALEN);
  sll.sll_halen = ETH_ALEN;

  /* bind the netcard  */
  if(bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) == -1)
  {
      return FALSE;
  }

  psll = (struct sockaddr_ll *)g_try_malloc(sizeof(struct sockaddr_ll));
  if(psll == NULL){
      return FALSE;
  }

  memcpy(psll, &sll, sizeof(struct sockaddr_ll));

  sink->avb_fd = sockfd;
  sink->avb_sll= psll;

  memset(&sink->net_name, 0, IF_NAMESIZE);
  strncpy(&sink->net_name[0], &ifstruct[intrface].ifr_name[0], IF_NAMESIZE);

  sink->ptp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(sink->ptp_fd < 0)
    return FALSE;


  GST_DEBUG_OBJECT(sink,"create socket success");
  return TRUE;
}
static gboolean
avbsink_close_socket(GstAvbSink *sink)
{
  if(sink == NULL)
      return FALSE;

  if(sink->avb_sll)
      g_free(sink->avb_sll);

  sink->avb_sll = NULL;
  close(sink->avb_fd);
  sink->avb_fd = -1;

  close(sink->ptp_fd);
  sink->ptp_fd = -1;

  return TRUE;
}
static gboolean
avbsink_create_1722_header(GstAvbSink *sink)
{
  GstAvbSinkClass *avbsinkclass;

  gboolean ret = FALSE;
  ETHERNET_HEADER * ethernet_header = NULL;
  AVTPDU_DATA_HEADER * avtp_header = NULL;
  CIP_HEADER * cip_header = NULL;
  guint8 src_addr[MAC_LEN] = {0};
  struct sockaddr_ll * avb_sll;
  guint32 streamId[2];

  if(sink == NULL){
      goto bail;
  }

  sink->header = g_try_malloc (AVB_HEADER_SIZE * sizeof(uint8));
  if(sink->header == NULL){
      goto bail;
  }

  ethernet_header = (ETHERNET_HEADER *)sink->header;
  avtp_header = (AVTPDU_DATA_HEADER *)(sink->header + sizeof(ETHERNET_HEADER));
  cip_header = (CIP_HEADER *)(sink->header + sizeof(ETHERNET_HEADER) + sizeof(AVTPDU_DATA_HEADER));

  Ethernet_Header_Init(ethernet_header);
  AVTPDU_Header_Init(avtp_header);
  CIP_Header_Init(cip_header);

  avb_sll = sink->avb_sll;
  memcpy(&src_addr[0], avb_sll->sll_addr, MAC_LEN);
  Ethernet_Set_SA(ethernet_header,src_addr);

  avbsinkclass = GST_AVBSINK_GET_CLASS (sink);

  if(CIP_FMT_AUDIO == avbsinkclass->get_fmt(sink))
    SET_ETHERNET_PCP(ethernet_header,2);//audio
  else
    SET_ETHERNET_PCP(ethernet_header,7);//video

  streamId[0] = (src_addr[0] << 24) + (src_addr[1] << 16) + (src_addr[2] << 8) + src_addr[3];
  streamId[1]= (src_addr[4] << 24) + (src_addr[5] << 16);

  SET_AVTPDU_SV(avtp_header,AVTPDU_SV_VALID);

  SET_AVTPDU_STREAM_ID0(avtp_header,streamId[0]);
  SET_AVTPDU_STREAM_ID1(avtp_header,streamId[1]);

  ret = TRUE;
  GST_DEBUG_OBJECT(sink,"avbsink_create_1722_header success");

bail:
  if(ret == FALSE && sink->header){
    g_free (sink->header);
  }
  return ret;

}

static void
gst_avbsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvbSink * avbsink;
  avbsink = GST_AVBSINK (object);

  switch (prop_id) {
    case PROP_LATENCY:
      avbsink->latency = g_value_get_uint (value);
      break;
    case PROP_USE_PTP_TS:
      avbsink->use_ptp_time = g_value_get_boolean (value);
      break;
    case PROP_PACKAGE_COUNT:
      avbsink->package_count = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;

  }

}
static void
gst_avbsink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstAvbSink * avbsink;
  avbsink = GST_AVBSINK (object);

  switch (prop_id)
  {
    case PROP_LATENCY:
      g_value_set_uint (value, avbsink->latency);
      break;
    case PROP_USE_PTP_TS:
      g_value_set_boolean (value, avbsink->use_ptp_time);
      break;
    case PROP_PACKAGE_COUNT:
      g_value_set_uint (value, avbsink->package_count);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

}

static gboolean
gst_avbsink_start (GstBaseSink * basesink)
{
  GstAvbSink *sink;
  gboolean ret;
  sink = GST_AVBSINK (basesink);

  ret = avbsink_create_socket(sink);

  if(!ret)
    return ret;

  ret = avbsink_create_1722_header(sink);
  if(!ret)
    return ret;

  sink->prepared = FALSE;

  sink->ts = GST_CLOCK_TIME_NONE;
  sink->start_ts = GST_CLOCK_TIME_NONE;

  sink->sequence_num = 0;
  sink->data_block_count = 0;

  return ret;
}
static gboolean
gst_avbsink_stop (GstBaseSink * basesink)
{
  GstAvbSink *sink;
  gboolean ret;

  sink = GST_AVBSINK (basesink);

  ret = avbsink_close_socket(sink);

  if(sink->header)
    g_free(sink->header);

  return ret;
}
static gboolean
gst_avbsink_unlock (GstBaseSink * basesink)
{
  //TODO
  return TRUE;
}

static gboolean
gst_avbsink_unlock_stop (GstBaseSink * basesink)
{
  //TODO
  return TRUE;
}
static GstFlowReturn
gst_avbsink_render (GstBaseSink * basesink, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  int process_ret = 0;
  GstAvbSink *avbsink;

  avbsink = GST_AVBSINK (basesink);

  if(!avbsink->prepared && avbsink_prepare(avbsink)){
    avbsink->prepared = TRUE;
  }

  if(!avbsink->prepared)
    return GST_FLOW_NOT_NEGOTIATED;

  process_ret = avbsink_render_buffer(avbsink,buffer);

  if(process_ret != 0)
    ret = GST_FLOW_ERROR;

  return ret;
}
static void
gst_avbsink_dispose (GObject * object)
{
  GstAvbSink *sink = GST_AVBSINK (object);

  if (sink->provided_clock) {
    g_object_unref (sink->provided_clock);
  }
  GST_LOG_OBJECT (sink,"gst_avbsink_dispose");

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static GstClock *
gst_avbsink_provide_clock (GstElement * element)
{
  GstAvbSink *sink = GST_AVBSINK(element);

  GST_LOG_OBJECT (sink,"gst_avbsink_provide_clock obj=%p",sink->provided_clock);

  return GST_CLOCK_CAST (gst_object_ref(sink->provided_clock));
}
static GstStateChangeReturn
gst_avbsink_change_state (GstElement * element, GstStateChange transition)
{
  GstAvbSink *sink;
  GstStateChangeReturn result = GST_STATE_CHANGE_SUCCESS;

  sink = GST_AVBSINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      gst_element_post_message (element,
          gst_message_new_clock_lost (GST_OBJECT_CAST (element),
              GST_CLOCK_CAST (sink->provided_clock)));
      break;
    default:
      break;
  }
  if ((result =
          GST_ELEMENT_CLASS (parent_class)->change_state (element,
              transition)) == GST_STATE_CHANGE_FAILURE)
    goto failure;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      gst_element_post_message (element,
          gst_message_new_clock_provide (GST_OBJECT_CAST (element),
              GST_CLOCK_CAST (sink->provided_clock), TRUE));
      break;
    default:
      break;
  }
  return result;
  /* ERRORS */
open_failed:
  {
    GST_WARNING_OBJECT (sink, "failed to open socket");
    return GST_STATE_CHANGE_FAILURE;
  }
failure:
  {
    GST_WARNING_OBJECT (sink, "parent failed state change");
    return result;
  }
}

static void
gst_avbsink_class_init (GstAvbSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = G_OBJECT_CLASS(klass);
  gstelement_class = GST_ELEMENT_CLASS( klass);
  gstbasesink_class = GST_BASE_SINK_CLASS( klass);

  gstelement_class->provide_clock = gst_avbsink_provide_clock;
  gstelement_class->change_state = gst_avbsink_change_state;

  gobject_class->dispose = gst_avbsink_dispose;

  gobject_class->set_property = gst_avbsink_set_property;
  gobject_class->get_property = gst_avbsink_get_property;
  gstbasesink_class->render = gst_avbsink_render;
  gstbasesink_class->start = gst_avbsink_start;
  gstbasesink_class->stop = gst_avbsink_stop;
  gstbasesink_class->unlock = gst_avbsink_unlock;
  gstbasesink_class->unlock_stop = gst_avbsink_unlock_stop;

  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_uint ("latency", "latency",
        "The latency in ns which was added into avtp timestamp when send the package, -1:auto",
        0, G_MAXUINT, AVBSINK_DEFAULT_LATENCY, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_USE_PTP_TS,
      g_param_spec_boolean ("use_ptp_time", "use ptp time",
          "if ptp time is used avtp timestamp", AVBSINK_DEFAULT_USE_PTP_TS,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PACKAGE_COUNT,
      g_param_spec_uint ("package_count", "package count",
        "package count in one avb package, avb package length should not exceed 1500",
        0, G_MAXUINT, AVBSINK_DEFAULT_PACKAGE_COUNT, G_PARAM_READWRITE));

  GST_DEBUG_CATEGORY_INIT (avbsink_debug, "avbsink", 0, "avb sink");
  GST_LOG_OBJECT (gobject_class,"gst_avbsink_class_init");

}
static void gst_avbsink_init (GstAvbSink * sink)
{
  GstBaseSink *basesink;

  sink->latency = AVBSINK_DEFAULT_LATENCY;
  sink->use_ptp_time = AVBSINK_DEFAULT_USE_PTP_TS;
  sink->package_count = AVBSINK_DEFAULT_PACKAGE_COUNT;

  sink->header = NULL;

  sink->avb_fd = -1;
  sink->avb_sll = NULL;
  sink->ptp_fd = -1;

  sink->prepared = FALSE;

  sink->ts = GST_CLOCK_TIME_NONE;
  sink->start_ts = GST_CLOCK_TIME_NONE;

  sink->sph = 0;

  sink->sequence_num = 0;
  sink->data_block_count = 0;

  basesink = GST_BASE_SINK(sink);

  gst_base_sink_set_last_sample_enabled (basesink, FALSE);
  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_FLAG_PROVIDE_CLOCK);

  sink->provided_clock = gst_avb_clock_new("avbclock");
  //gst_element_set_clock (sink,sink->provided_clock);

  g_print("====== AVBSINK: %s build on %s %s. ======\n",  (VERSION),__DATE__,__TIME__);

  GST_LOG_OBJECT (sink,"gst_avbsink_init");

}

