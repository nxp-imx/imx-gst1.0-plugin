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

#include "gstavbclock.h"
#include <sys/time.h>
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

#define USE_LINUXPTP 0

#if USE_LINUXPTP

#include <linux/ptp_clock.h>
//#include <linux/posix-timers.h>
#endif

typedef struct
{
  guint64 u48_sec; /* ATTENTION, 48bit !! */
  guint32 u32_Nsec;
}PTP_TIME_STRUCT;

#define k_PTP_GET_TIME (SIOCDEVPRIVATE + 7)

#define CLOCKFD 3

#define FD_TO_CLOCKID(fd)       ((~(clockid_t) (fd) << 3) | CLOCKFD)


GST_DEBUG_CATEGORY_STATIC (gst_avb_clock_debug);
#define GST_CAT_DEFAULT gst_avb_clock_debug

static void gst_avb_clock_class_init (GstAvbClockClass * klass);
static void gst_avb_clock_init (GstAvbClock * clock);

static GstClockTime gst_avb_clock_get_internal_time (GstClock * clock);

static GstSystemClockClass *parent_class = NULL;

GType
gst_avb_clock_get_type (void)
{
  static GType clock_type = 0;

  if (!clock_type) {
    static const GTypeInfo clock_info = {
      sizeof (GstAvbClockClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_avb_clock_class_init,
      NULL,
      NULL,
      sizeof (GstAvbClock),
      4,
      (GInstanceInitFunc) gst_avb_clock_init,
      NULL
    };

    clock_type = g_type_register_static (GST_TYPE_SYSTEM_CLOCK, "GstAvbClock",
        &clock_info, 0);
  }
  return clock_type;
}

static void
gst_avb_clock_dispose (GObject * object)
{
  GstAvbClock *clock = GST_AVB_CLOCK (object);

  close(clock->socket_fd);
  clock->socket_fd = -1;
  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
gst_avb_clock_class_init (GstAvbClockClass * klass)
{
  GObjectClass *gobject_class;
  GstClockClass *gstclock_class;

  gobject_class = (GObjectClass *) klass;
  gstclock_class = (GstClockClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gstclock_class->get_internal_time = gst_avb_clock_get_internal_time;
  gobject_class->dispose = gst_avb_clock_dispose;

  GST_DEBUG_CATEGORY_INIT (gst_avb_clock_debug, "avbclock", 0, "avb clock");
}

static gboolean
gst_avb_clock_create_socket(GstAvbClock * clock)
{
  int sockfd, intrface;
  struct sockaddr_ll sll, *psll = NULL;
  struct ifreq ifstruct[16];
  struct ifconf ifc;

  if(clock == NULL)
    return FALSE;

#if USE_LINUXPTP
  sockfd = open("/dev/ptp0", O_RDWR);
  if (sockfd < 0) {
    GST_WARNING_OBJECT(clock,"open devide /dev/ptp0 failed");
    return FALSE;
  }
  GST_INFO_OBJECT(clock,"open /dev/ptp0 success. \r\n");
#else
  sockfd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0) {
    return FALSE;
  }

  ifc.ifc_len = sizeof(ifstruct);
  ifc.ifc_buf =  (char *)&ifstruct;
  ioctl (sockfd, SIOCGIFCONF, (char *) &ifc);
  intrface = ifc.ifc_len / sizeof (struct ifreq);

  while (!strcmp("lo", ifstruct[intrface--].ifr_name)){
  ;
  }

  ioctl(sockfd, SIOCGIFINDEX, &ifstruct[intrface]);

  memset(&clock->net_name, 0, IF_NAMESIZE);
  strncpy(&clock->net_name[0], &ifstruct[intrface].ifr_name[0], IF_NAMESIZE);
#endif
  clock->socket_fd = sockfd;

  return TRUE;
}

static void
gst_avb_clock_init (GstAvbClock * clock)
{
  GST_OBJECT_FLAG_SET (clock, GST_CLOCK_FLAG_CAN_SET_MASTER);

  gst_avb_clock_create_socket(clock);
}

/**
 * gst_avb_clock_new:
 * @name: the name of the clock
 *
 * Create a new #GstAvbClock instance.
 *
 * Returns: a new #GstAvbClock
 */
GstAvbClock *
gst_avb_clock_new(const gchar * name)
{
  GstAvbClock *avbclock =
      GST_AVB_CLOCK (g_object_new (GST_TYPE_AVB_CLOCK, "name", name,
          "clock-type", GST_CLOCK_TYPE_OTHER, NULL));

  return avbclock;
}

gboolean
gst_avb_clock_get_gptp_time(int fd, gchar* name, guint64 *ptp_time)
{
  gboolean ret = FALSE;
  struct ifreq ps_ifReq;
  PTP_TIME_STRUCT ptp_ts;
  PTP_TIME_STRUCT * ts;


#if USE_LINUXPTP
  struct timespec current_timer;
  clockid_t clkid;

  if(ptp_time == NULL || fd < 0)
    return ret;

  clkid = FD_TO_CLOCKID(fd);
  
  clock_gettime(clkid , &current_timer);

  *ptp_time= current_timer.tv_sec * GST_SECOND + current_timer.tv_nsec;
  ret = TRUE;


#else
  if(ptp_time == NULL || name == NULL || fd < 0)
    return ret;

  memset(&ps_ifReq,0,sizeof(ps_ifReq));
  ptp_ts.u32_Nsec = 0;
  ptp_ts.u48_sec = 0;

  strncpy(&ps_ifReq.ifr_name[0], name, IF_NAMESIZE);
  ps_ifReq.ifr_data = (guint8*)&ptp_ts;

  if(-1 != ioctl(fd, k_PTP_GET_TIME, &ps_ifReq)){
    ts = (PTP_TIME_STRUCT *)ps_ifReq.ifr_data;
    *ptp_time = ts->u48_sec * GST_SECOND + ts->u32_Nsec;
    ret = TRUE;
  }
#endif
  return ret;
}

static GstClockTime
gst_avb_clock_get_internal_time (GstClock * clock)
{
  GstAvbClock *avbclock;
  GstClockTime result;

  avbclock = GST_AVB_CLOCK_CAST (clock);


  if(gst_avb_clock_get_gptp_time(avbclock->socket_fd, avbclock->net_name, &result)){
    GST_DEBUG_OBJECT(avbclock,"get_internal_time %lld,",result) ;
    return result;
  }else
    result = GST_CLOCK_TIME_NONE;

  GST_WARNING_OBJECT(avbclock,"get internal time failed");

  return result;
}

