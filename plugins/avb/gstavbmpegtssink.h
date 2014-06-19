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
#ifndef __GST_AVB_MPEGTS_SINK_H__
#define __GST_AVB_MPEGTS_SINK_H__
#include "gstavbsink.h"

G_BEGIN_DECLS

typedef struct _GstAvbMpegtsSink GstAvbMpegtsSink;
typedef struct _GstAvbMpegtsSinkClass GstAvbMpegtsSinkClass;

#define GST_TYPE_AVB_MPEGTS_SINK \
  (gst_avb_mpegts_sink_get_type())

#define GST_AVB_MPEGTS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVB_MPEGTS_SINK,GstAvbMpegtsSink))

#define GST_AVB_MPEGTS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVB_MPEGTS_SINK,GstAvbMpegtsSinkClass))

#define GST_IS_AVB_MPEGTS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVB_MPEGTS_SINK))

#define GST_IS_AVB_MPEGTS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVB_MPEGTS_SINK))


struct _GstAvbMpegtsSink
{
  GstAvbSink element;
  GstCaps * caps;

  guint8 dbs;

  guint32 latency;
  guint64 pcr;
  guint64 pcr_offset;
};

struct _GstAvbMpegtsSinkClass
{
  GstAvbSinkClass parent_class;
};

GType gst_avb_mpegts_sink_get_type(void);
G_END_DECLS


#endif /* __GST_AVB_MPEGTS_SINK_H__ */