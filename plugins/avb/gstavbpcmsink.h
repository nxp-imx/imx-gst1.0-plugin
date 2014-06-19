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
#ifndef __GST_AVB_PCM_SINK_H__
#define __GST_AVB_PCM_SINK_H__
#include "gstavbsink.h"

G_BEGIN_DECLS

#define GST_TYPE_AVB_PCM_SINK \
  (gst_avb_pcm_sink_get_type())

#define GST_AVB_PCM_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVB_PCM_SINK,GstAvbPcmSink))

#define GST_AVB_PCM_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVB_PCM_SINK,GstAvbPcmSinkClass))

#define GST_IS_AVB_PCM_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVB_PCM_SINK))

#define GST_IS_AVB_PCM_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVB_PCM_SINK))



typedef struct _GstAvbPcmSink GstAvbPcmSink;
typedef struct _GstAvbPcmSinkClass GstAvbPcmSinkClass;

struct _GstAvbPcmSink
{
  GstAvbSink parent;
  GstCaps * caps;
  guint32 width;
  guint32 channels;
  guint32 rate;

  guint8 syt_internal;
  guint8 sfc;
  guint8 dbs;
  guint8 label;
};

struct _GstAvbPcmSinkClass
{
  GstAvbSinkClass parent_class;
};

GType gst_avb_pcm_sink_get_type(void);
G_END_DECLS


#endif /* __GST_AVB_PCM_SINK_H__ */
