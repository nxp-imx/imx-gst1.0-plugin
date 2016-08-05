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

#ifndef __GST_IMXCOMPOSITOR_PAD_H__
#define __GST_IMXCOMPOSITOR_PAD_H__

#define GST_USE_UNSTABLE_API

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoaggregator.h>
#include "gstimxcompositor.h"

G_BEGIN_DECLS

#define GST_TYPE_IMXCOMPOSITOR_PAD (gst_imxcompositor_pad_get_type())
#define GST_IMXCOMPOSITOR_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_IMXCOMPOSITOR_PAD, \
            GstImxCompositorPad))
#define GST_IMXCOMPOSITOR_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_IMXCOMPOSITOR_PAD, \
            GstImxCompositorPadClass))
#define GST_IS_IMXCOMPOSITOR_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_IMXCOMPOSITOR_PAD))
#define GST_IS_IMXCOMPOSITOR_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_IMXCOMPOSITOR_PAD))

typedef struct _GstImxCompositorPad GstImxCompositorPad;
typedef struct _GstImxCompositorPadClass GstImxCompositorPadClass;

struct _GstImxCompositorPad
{
  GstVideoAggregatorPad parent;

  GstBufferPool *sink_pool;
  gboolean sink_pool_update;
  GstVideoAlignment align;
  GstBuffer *sink_tmp_buf;
  guint sink_tmp_buf_size;
  GstVideoInfo info;
  GstVideoRectangle src_crop;
  GstVideoRectangle dst_crop;

  /* properties */
  gint xpos;
  gint ypos;
  gint width;
  gint height;
  gint rotate;
  gdouble alpha;
  gboolean keep_ratio;
};

struct _GstImxCompositorPadClass
{
  GstVideoAggregatorPadClass parent_class;
};

GType gst_imxcompositor_pad_get_type (void);
void gst_imxcompositor_pad_get_output_size (GstVideoAggregator * comp,
                  GstImxCompositorPad * comp_pad, gint * width, gint * height);

G_END_DECLS

#endif /* __GST_IMXCOMPOSITOR_PAD_H__ */
