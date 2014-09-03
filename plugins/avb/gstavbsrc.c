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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/net/gstnetaddressmeta.h>
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
#include <string.h>
#include <stdlib.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include "cip.h"
#include "avtp.h"
#include "ethernet.h"
#include "gstavbsrc.h"

#if GLIB_CHECK_VERSION (2, 35, 7)
#include <gio/gnetworking.h>
#else

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif

#define AVB_SRC_DEFAULT_CAPS NULL
#define AVB_SRC_DEFAULT_BUFFER_SIZE 4096000
#define AVB_SRC_DEFAULT_TIMEOUT 0
#define AVB_SRC_MAX_MTU 1500
enum
{
  PROP_0,
  PROP_BUFFER_SIZE,
  PROP_TIMEOUT,
  PROP_LAST
};


GST_DEBUG_CATEGORY_STATIC (avbsrc_debug);
#define GST_CAT_DEFAULT avbsrc_debug

#define gst_avbsrc_parent_class parent_class
G_DEFINE_TYPE (GstAvbSrc, gst_avbsrc, GST_TYPE_PUSH_SRC);


static GstCaps *
gst_avbsrc_getcaps (GstBaseSrc * src, GstCaps * filter)
{
  GstAvbSrc *avbsrc;
  GstCaps *caps, *result;

  avbsrc = GST_AVBSRC (src);

  GST_OBJECT_LOCK (src);
  if ((caps = avbsrc->caps))
    gst_caps_ref (caps);
  GST_OBJECT_UNLOCK (src);

  if (caps) {
    if (filter) {
      result = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
    } else {
      result = caps;
    }
  } else {
    result = (filter) ? gst_caps_ref (filter) : gst_caps_new_any ();
  }
  return result;
}

static GstFlowReturn
gst_avbsrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstFlowReturn ret;
  gint intRet = 0;
  GstAvbSrc *avbsrc;
  GstAvbSrcClass * avbsrcClass;
  GstBuffer *outbuf = NULL;
  gint readsize;
  guint8 *readData = NULL;
  ETHERNET_HEADER * ethernetHeader;
  AVTPDU_DATA_HEADER * avtpHeader;
  CIP_HEADER * cipHeader;
  gint avtpDataLen;
  gint pktsize;
  guint32 output_buffer_size;

  GstClockTime timeoutValue;
  gboolean try_again;
  gboolean discont = FALSE;

  avbsrc = GST_AVBSRC (psrc);

  if(avbsrc == NULL)
    return GST_FLOW_ERROR;

  avbsrcClass = GST_AVBSRC_GET_CLASS(avbsrc);

retry:
  /* quick check, avoid going in select when we already have data */
  readsize = 0;
  if (ioctl (avbsrc->sock.fd, FIONREAD, &readsize) < 0)
    goto ioctl_failed;

  if (readsize > AVB_HEADER_SIZE)
    goto no_select;

  if (avbsrc->timeout > 0) {
    timeoutValue = avbsrc->timeout;
  } else {
    timeoutValue = GST_CLOCK_TIME_NONE;
  }

  do {

    try_again = FALSE;

    ret = gst_poll_wait (avbsrc ->fdset, timeoutValue);

    if (G_UNLIKELY (ret < 0)) {
      if (errno == EBUSY)
        goto stopped;
      if (errno != EAGAIN && errno != EINTR){
        GST_WARNING_OBJECT(avbsrc,"select_error");
        goto select_error;
      }

      try_again = TRUE;
    } else if (G_UNLIKELY (ret == 0)) {
      /* timeout, post element message */
      gst_element_post_message (GST_ELEMENT_CAST (avbsrc),
          gst_message_new_element (GST_OBJECT_CAST (avbsrc),
              gst_structure_new ("GstAvbSrcTimeout",
                  "timeout", G_TYPE_UINT64, avbsrc->timeout, NULL)));

      try_again = TRUE;
    }
  } while (G_UNLIKELY (try_again));

  readsize = 0;
  if ((ret = ioctl (avbsrc->sock.fd, FIONREAD, &readsize)) < 0){
    GST_WARNING_OBJECT(avbsrc,"ioctl_failed");
    goto ioctl_failed;
  }

  if (readsize <= AVB_HEADER_SIZE)
    goto retry;


no_select:
  if(readsize > AVB_SRC_MAX_MTU)
    readsize = AVB_SRC_MAX_MTU;

  //GST_LOG_OBJECT(avbsrc,"ioctl says %d bytes available\n", (int) readsize);

  readData = g_malloc (readsize);
  if(readData == NULL){
    GST_WARNING_OBJECT(avbsrc,"g_malloc failed");
    goto ioctl_failed;
  }

  ret = recvfrom(avbsrc->sock.fd, readData, readsize, 0, NULL, NULL);

  if(ret != readsize){
    GST_WARNING_OBJECT(avbsrc,"readsize error");
    goto receive_error;
  }

  ethernetHeader = (ETHERNET_HEADER *)readData;
  avtpHeader = (AVTPDU_DATA_HEADER *)(readData + sizeof(ETHERNET_HEADER));
  cipHeader = (CIP_HEADER *)(readData + sizeof(ETHERNET_HEADER)+ sizeof(AVTPDU_DATA_HEADER));

  intRet = Is_Valid_Ethernet_Header(ethernetHeader);
  if(intRet != 0){
    GST_WARNING_OBJECT(avbsrc,"invalid ethernet header");
    goto drop_frame;
  }

  intRet = Is_Valid_AVTPDU_Header(avtpHeader);

  if(intRet != 0){
    GST_WARNING_OBJECT(avbsrc,"invalid avtpdu header");
    goto drop_frame;
  }

  ret = avbsrcClass->isValid_cip_header(avbsrc,cipHeader);
  if(ret != 0){
    goto drop_frame;
  }

  if((uint8)(avbsrc->last_sequence_num + 1) != GET_AVTPDU_SEQUENCE_NUM(avtpHeader)){
    discont = TRUE;
    GST_WARNING_OBJECT(avbsrc,"discont, last_sequence_num=%d,sequence=%d",avbsrc->last_sequence_num,GET_AVTPDU_SEQUENCE_NUM(avtpHeader));
  }
  avbsrc->last_sequence_num = GET_AVTPDU_SEQUENCE_NUM(avtpHeader);

  avtpDataLen = GET_AVTPDU_STREAM_DATA_LEN(avtpHeader);
  pktsize = avtpDataLen - sizeof(CIP_HEADER);

  if(pktsize <= 0)
    goto drop_frame;

  if(avbsrc->caps == NULL){
    avbsrc->caps = avbsrcClass->parse_and_get_caps(avbsrc,readData,readsize);

    if(avbsrc->caps == NULL)
      goto not_negotiated;
    gst_base_src_set_caps (GST_BASE_SRC (avbsrc), avbsrc->caps);
  }


  avbsrc->output_buffer_size = avbsrcClass->get_output_buffer_size(avbsrc,pktsize);
  if(0 == avbsrc->output_buffer_size)
    goto not_negotiated;

  ret = GST_BASE_SRC_CLASS (parent_class)->alloc (GST_BASE_SRC_CAST (avbsrc),
        -1, avbsrc->output_buffer_size, &outbuf);
  if(discont){
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
    if(avbsrcClass->reset_ts)
      avbsrcClass->reset_ts(avbsrc);
  }

  if(TRUE == avbsrcClass->process_buffer(avbsrc, readData, readsize,outbuf,&output_buffer_size)){
    *buf = GST_BUFFER_CAST (outbuf);
    GST_LOG_OBJECT(avbsrc,"output size=%d,ts=%"GST_TIME_FORMAT",duration=%"GST_TIME_FORMAT,
      output_buffer_size,GST_TIME_ARGS (GST_BUFFER_PTS (outbuf)),GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)));
  }

  if(!avbsrc->init_segment){
    gst_base_src_new_seamless_segment(GST_BASE_SRC (avbsrc), 0, -1,  GST_BUFFER_PTS (outbuf));
    avbsrc->init_segment = TRUE;
  }

  if(discont)
     GST_DEBUG_OBJECT(avbsrc,"output ts=%"GST_TIME_FORMAT,GST_TIME_ARGS (GST_BUFFER_PTS (outbuf)));

  g_free (readData);

  return GST_FLOW_OK;

/* ERRORS */
select_error:
  {
    return GST_FLOW_ERROR;
  }
stopped:
  {
    return GST_FLOW_FLUSHING;
  }
ioctl_failed:
  {
    return GST_FLOW_ERROR;
  }
receive_error:
  {
    if(readData)
      g_free (readData);
    return GST_FLOW_ERROR;
  }
not_negotiated:
  {
    if(readData)
      g_free (readData);
    return GST_FLOW_NOT_NEGOTIATED;
  }
drop_frame:
  {
    if(readData)
      g_free(readData);
    goto retry;
  }
}

static void
gst_avbsrc_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstAvbSrc *avbsrc = GST_AVBSRC (object);

  switch (prop_id) {
    case PROP_BUFFER_SIZE:
      avbsrc->buffer_size = g_value_get_int (value);
      break;
    case PROP_TIMEOUT:
      avbsrc->timeout = g_value_get_uint64 (value);
      break;
    default:
      break;
  }
}

static void
gst_avbsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstAvbSrc *avbsrc = GST_AVBSRC (object);

  switch (prop_id) {
    case PROP_BUFFER_SIZE:
      g_value_set_int (value, avbsrc->buffer_size);
      break;
    case PROP_TIMEOUT:
      g_value_set_uint64 (value, avbsrc->timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
static gboolean
avbsrc_create_socket(GstAvbSrc * src)
{
  int sockfd, intrface;
  struct sockaddr_ll sll, *psll = NULL;
  struct ifreq ifstruct[16];
  struct ifconf ifc;

  if(src == NULL)
    return FALSE;

  sockfd = socket (PF_PACKET, SOCK_RAW, htons(ETH_P_8021Q));
  if (sockfd < 0) {
    return FALSE;
  }

  setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &src->buffer_size, sizeof(src->buffer_size));
  memset(&sll, 0, sizeof(sll));
  sll.sll_family = PF_PACKET;
  sll.sll_protocol = htons(ETH_P_8021Q);

  ifc.ifc_len = sizeof ifstruct;
  ifc.ifc_buf =  (char *)&ifstruct;
  ioctl (sockfd, SIOCGIFCONF, (char *) &ifc);
  intrface = ifc.ifc_len / sizeof (struct ifreq);


  while (!strcmp("lo", ifstruct[intrface--].ifr_name)){
  ;
  }

  ioctl(sockfd, SIOCGIFINDEX, &ifstruct[intrface]);
  sll.sll_ifindex = ifstruct[intrface].ifr_ifindex;

  GST_DEBUG_OBJECT(src,"net device %s\n", ifstruct[intrface].ifr_name);

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

  src->avb_sll= psll;
  src->sock.fd = sockfd;

  GST_DEBUG_OBJECT(src,"create socket %d\n", src->sock.fd);

  return TRUE;

}
static gboolean
avbsrc_close_socket(GstAvbSrc * src)
{
  if(src == NULL)
      return FALSE;

  if(src->avb_sll)
      g_free(src->avb_sll);

  src->avb_sll = NULL;

  close(src->sock.fd);
  src->sock.fd = -1;

  return TRUE;
}

/* create a socket for sending to remote machine */
static gboolean
gst_avbsrc_open (GstAvbSrc * src)
{
  gboolean ret = FALSE;
  GstAvbSrcClass * avbsrcClass;

  if(src == NULL)
    return ret;

  ret = avbsrc_create_socket(src);

  if(ret == TRUE){
    src->fdset = gst_poll_new (TRUE);
    gst_poll_add_fd (src->fdset, &src->sock);
    gst_poll_fd_ctl_read (src->fdset, &src->sock, TRUE);
  }

  avbsrcClass = GST_AVBSRC_GET_CLASS(src);

  if(avbsrcClass->isValid_cip_header == NULL
    || avbsrcClass->parse_and_get_caps == NULL
    || avbsrcClass->get_output_buffer_size == NULL
    || avbsrcClass->process_buffer == NULL)
    ret = FALSE;

  src->last_sequence_num = 0;
  src->init_segment = FALSE;
  GST_DEBUG_OBJECT (src,"gst_avbsrc_open bool ret=%d",ret);

  return ret;
}

static gboolean
gst_avbsrc_unlock (GstBaseSrc * bsrc)
{
  GstAvbSrc *src;

  src = GST_AVBSRC (bsrc);

  GST_LOG_OBJECT (src, "Flushing");
  gst_poll_set_flushing (src->fdset, TRUE);

  return TRUE;
}

static gboolean
gst_avbsrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstAvbSrc *src;

  src = GST_AVBSRC (bsrc);

  GST_LOG_OBJECT (src, "No longer flushing");
  gst_poll_set_flushing (src->fdset, FALSE);

  return TRUE;
}

static gboolean
gst_avbsrc_close (GstAvbSrc * src)
{
  gboolean ret = FALSE;
  GstAvbSrcClass * avbsrcClass;

  if(src == NULL)
    return ret;

  avbsrcClass = GST_AVBSRC_GET_CLASS(src);

  if(avbsrcClass->reset_ts)
    avbsrcClass->reset_ts(src);

  if (src->fdset) {
    gst_poll_free (src->fdset);
    src->fdset = NULL;
  }

  if(src->caps)
    gst_caps_unref(src->caps);

  ret = avbsrc_close_socket(src);

  GST_DEBUG_OBJECT (src,"gst_avbsrc_close bool ret=%d",ret);

  return ret;
}


static GstStateChangeReturn
gst_avbsrc_change_state (GstElement * element, GstStateChange transition)
{
  GstAvbSrc *src;
  GstStateChangeReturn result = GST_STATE_CHANGE_SUCCESS;

  src = GST_AVBSRC (element);
  GST_LOG_OBJECT (src,"gst_avbsrc_change_state provided_clock=%p",src->provided_clock);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_avbsrc_open (src))
        goto open_failed;
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      gst_element_post_message (element,
          gst_message_new_clock_lost (GST_OBJECT_CAST (element),
              GST_CLOCK_CAST (src->provided_clock)));
      break;
    default:
      break;
  }
  if ((result =
          GST_ELEMENT_CLASS (parent_class)->change_state (element,
              transition)) == GST_STATE_CHANGE_FAILURE)
    goto failure;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_avbsrc_close (src);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      gst_element_post_message (element,
          gst_message_new_clock_provide (GST_OBJECT_CAST (element),
              GST_CLOCK_CAST (src->provided_clock), TRUE));
      break;
    default:
      break;
  }
  return result;
  /* ERRORS */
open_failed:
  {
    GST_WARNING_OBJECT (src, "failed to open socket");
    return GST_STATE_CHANGE_FAILURE;
  }
failure:
  {
    GST_WARNING_OBJECT (src, "parent failed state change");
    return result;
  }
}
static GstClock *
gst_avbsrc_provide_clock (GstElement * element)
{
  GstAvbSrc *avbsrc = GST_AVBSRC (element);
  GST_DEBUG_OBJECT (avbsrc,"gst_avbsrc_provide_clock obj=%p",avbsrc->provided_clock);

  return GST_CLOCK_CAST (gst_object_ref (avbsrc->provided_clock));
}

static void
gst_avbsrc_init (GstAvbSrc * avbsrc)
{

  avbsrc->buffer_size = AVB_SRC_DEFAULT_BUFFER_SIZE;
  avbsrc->timeout = AVB_SRC_DEFAULT_TIMEOUT;
  avbsrc->caps = AVB_SRC_DEFAULT_CAPS;

  avbsrc->output_buffer_size = 0;

  avbsrc->last_sequence_num = 0;

  /* configure basesrc to be a live source */
  gst_base_src_set_live (GST_BASE_SRC (avbsrc), TRUE);
  /* make basesrc output a segment in time */
  gst_base_src_set_format (GST_BASE_SRC (avbsrc), GST_FORMAT_TIME);
  /* make basesrc set timestamps on outgoing buffers based on the running_time
   * when they were captured */
  gst_base_src_set_do_timestamp (GST_BASE_SRC (avbsrc), FALSE);

  GST_OBJECT_FLAG_SET (avbsrc, GST_ELEMENT_FLAG_PROVIDE_CLOCK);

  avbsrc->provided_clock = gst_avb_clock_new ("avbclock");
  avbsrc->init_segment = FALSE;

  g_print("======AVBSRC: %s build on %s %s. ======\n",  (VERSION),__DATE__,__TIME__);
  
  GST_LOG_OBJECT (avbsrc,"gst_avbsrc_init provided_clock=%p",avbsrc->provided_clock);

}
static void
gst_avbsrc_dispose (GObject * object)
{
  GstAvbSrc *src = GST_AVBSRC (object);

  if (src->provided_clock) {
    g_object_unref (src->provided_clock);
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_avbsrc_class_init (GstAvbSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = G_OBJECT_CLASS(klass);
  gstelement_class = GST_ELEMENT_CLASS(klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS(klass);
  gstpushsrc_class = GST_PUSH_SRC_CLASS(klass);

  gobject_class->set_property = gst_avbsrc_set_property;
  gobject_class->get_property = gst_avbsrc_get_property;
  gobject_class->dispose = gst_avbsrc_dispose;


  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_int ("buffer-size", "Buffer Size",
          "Size of the kernel receive buffer in bytes, 200K=default", 0, G_MAXINT,
          AVB_SRC_DEFAULT_BUFFER_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint64 ("timeout", "Timeout",
          "Post a message after timeout nanoseconds (0 = disabled)", 0,
          G_MAXUINT64, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->provide_clock = gst_avbsrc_provide_clock;

  gstelement_class->change_state = gst_avbsrc_change_state;

  gstbasesrc_class->unlock = gst_avbsrc_unlock;
  gstbasesrc_class->unlock_stop = gst_avbsrc_unlock_stop;
  gstbasesrc_class->get_caps = gst_avbsrc_getcaps;

  gstpushsrc_class->create = gst_avbsrc_create;
  GST_DEBUG_CATEGORY_INIT (avbsrc_debug, "avbsrc", 0, "avb src");
}

