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

#ifndef __GST_AVB_CLOCK_H__
#define __GST_AVB_CLOCK_H__

#include <gst/gst.h>
#include <gst/gstsystemclock.h>
#include <net/if.h>


G_BEGIN_DECLS

#define GST_TYPE_AVB_CLOCK \
  (gst_avb_clock_get_type())
#define GST_AVB_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVB_CLOCK,GstAvbClock))
#define GST_AVB_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVB_CLOCK,GstAvbClockClass))
#define GST_IS_AVB_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVB_CLOCK))
#define GST_IS_AVB_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVB_CLOCK))
#define GST_AVB_CLOCK_CAST(obj) \
  ((GstAvbClock*)(obj))

typedef struct _GstAvbClock GstAvbClock;
typedef struct _GstAvbClockClass GstAvbClockClass;

/**
 * GstavbClock:
 * @clock: parent #GstSystemClock
 *
 * Opaque #GstavbClock.
 */
struct _GstAvbClock {
  GstSystemClock clock;

  int socket_fd;
  gchar net_name[IF_NAMESIZE];
};

struct _GstAvbClockClass {
  GstSystemClockClass parent_class;
};

GType           gst_avb_clock_get_type        (void);
GstAvbClock*   gst_avb_clock_new             (const gchar *name);

/**
 * function to get ptp time
 *
 * @param fd [in] socket fd.
 * @param name [in] net card name.
 * @param ptp_time [out] ptp time.
 * @return TRUE if success
 */
gboolean gst_avb_clock_get_gptp_time(int fd, gchar* name, guint64 *ptp_time);

G_END_DECLS

#endif /* __GST_AVB_CLOCK_H__ */
