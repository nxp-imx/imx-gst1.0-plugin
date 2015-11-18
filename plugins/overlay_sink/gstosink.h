/*
 * Copyright (c) 2014-2015, Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef __OSINK_PLUGIN_H__
#define __OSINK_PLUGIN_H__

#include <gst/video/gstvideosink.h>
#include <gst/video/gstvideometa.h>
#include "gstimxcommon.h"
#include "osink_common.h"

#define GST_TYPE_OVERLAY_SINK \
  (gst_overlay_sink_get_type())
#define GST_OVERLAY_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_OVERLAY_SINK, GstOverlaySink))
#define GST_OVERLAY_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_OVERLAY_SINK, GstOverlaySinkClass))
#define GST_IS_OVERLAY_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_OVERLAY_SINK))
#define GST_IS_OVERLAY_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_OVERLAY_SINK))

typedef struct _GstOverlaySink GstOverlaySink;
typedef struct _GstOverlaySinkClass GstOverlaySinkClass;

typedef struct {
  gint x;
  gint y;
  gint w;
  gint h;
  gint rot;
  gboolean keep_ratio;
  gint zorder;
} OverlayInfo;

struct _GstOverlaySink {
  GstVideoSink videosink;
  gpointer osink_obj;
  GstBufferPool *pool;
  GstAllocator *allocator;
  gint w;
  gint h;
  SurfaceInfo surface_info;
  OverlayInfo overlay[MAX_DISPLAY];
  OverlayInfo pre_overlay_info[MAX_DISPLAY];
  gpointer hoverlay[MAX_DISPLAY];
  gint disp_count;
  DisplayInfo disp_info[MAX_DISPLAY];
  gboolean disp_on[MAX_DISPLAY];
  gboolean config[MAX_DISPLAY];
  GstVideoCropMeta cropmeta;
  GstVideoAlignment video_align;
  gboolean pool_alignment_checked;
  gboolean no_phy_buffer;
  gboolean pool_activated;
  guint64 frame_showed;
  GstClockTime run_time;
  gint min_buffers;
  gint max_buffers;
  GstBuffer *prv_buffer;
  void *imxoverlay;
  gboolean composition_meta_enable;
};

struct _GstOverlaySinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_overlay_sink_get_type(void);

#endif
