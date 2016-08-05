/* GStreamer IMX video compositor plugin
 * Copyright (c) 2015, Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef __GSTIMXCOMPOSITOR_H__
#define __GSTIMXCOMPOSITOR_H__

#define GST_USE_UNSTABLE_API

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoaggregator.h>
#include "gstimxcommon.h"
#include "imx_2d_device.h"
#include "imxoverlaycompositionmeta.h"

G_BEGIN_DECLS

typedef struct _GstImxCompositor GstImxCompositor;
typedef struct _GstImxCompositorClass GstImxCompositorClass;

struct _GstImxCompositor
{
  GstVideoAggregator videoaggregator;
  guint background;
  gboolean background_enable;
  gboolean negotiated;

  Imx2DDevice *device;
  GstBufferPool *out_pool;
  GstBufferPool *self_out_pool;
  GstAllocator *allocator;
  GstVideoAlignment out_align;
  gboolean out_pool_update;
  gint capabilities;
  GstImxVideoOverlayComposition video_comp;
  gboolean composition_meta_enable;
};

struct _GstImxCompositorClass
{
  GstVideoAggregatorClass parent_class;
  const Imx2DDeviceInfo *in_plugin;
};

G_END_DECLS

#endif /* __GSTIMXCOMPOSITOR_H__ */
