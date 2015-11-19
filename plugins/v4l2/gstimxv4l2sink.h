/*
 * Copyright (c) 2013-2015, Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef __IMX_V4L2_SINK_H__
#define __IMX_V4L2_SINK_H__

#include <gst/video/gstvideosink.h>
#include <gst/video/gstvideometa.h>
#include "gstimxcommon.h"
#include "gstimxv4l2.h"
#include "imxoverlaycompositionmeta.h"

#define GST_TYPE_IMX_V4L2SINK \
  (gst_imx_v4l2sink_get_type())
#define GST_IMX_V4L2SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_IMX_V4L2SINK, GstImxV4l2Sink))
#define GST_IMX_V4L2SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_IMX_V4L2SINK, GstImxV4l2SinkClass))
#define GST_IS_IMX_V4L2SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_IMX_V4L2SINK))
#define GST_IS_IMX_V4L2SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_IMX_V4L2SINK))

typedef struct _GstImxV4l2Sink GstImxV4l2Sink;
typedef struct _GstImxV4l2SinkClass GstImxV4l2SinkClass;

struct _GstImxV4l2Sink {
  GstVideoSink videosink;
  gchar *device;
  guint v4l2fmt;
  guint w, h;
  IMXV4l2Rect overlay, crop;
  guint rotate;
  IMXV4l2Rect prev_overlay;
  guint prev_rotate;
  gboolean do_deinterlace;
  guint deinterlace_motion;
  gboolean config;
  guint config_flag;
  gpointer v4l2handle;
  GstBufferPool *pool;
  GstAllocator *allocator;
  GstVideoCropMeta cropmeta;
  GstVideoAlignment video_align;
  gboolean keep_video_ratio;
  guint64 frame_showed;
  GstClockTime run_time;
  guint min_buffers;
  gboolean self_pool_configed;
  gboolean pool_activated;
  gboolean use_userptr_mode;
  GHashTable *v4l2buffer2buffer_table;
  void *imxoverlay;
  GstImxVideoOverlayComposition video_comp;
  gboolean composition_meta_enable;
  Imx2DDevice *blend_dev;
  gboolean invisible;
};

struct _GstImxV4l2SinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_imx_v4l2sink_get_type(void);

#endif
