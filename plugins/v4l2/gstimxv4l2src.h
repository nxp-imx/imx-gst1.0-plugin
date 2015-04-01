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

#ifndef __IMX_V4L2_SRC_H__
#define __IMX_V4L2_SRC_H__

#include <gst/base/gstpushsrc.h>
#include <gst/video/gstvideometa.h>
#include "gstimxcommon.h"
#include "gstimxv4l2.h"

#define GST_TYPE_IMX_V4L2SRC \
  (gst_imx_v4l2src_get_type())
#define GST_IMX_V4L2SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_IMX_V4L2SRC, GstImxV4l2Src))
#define GST_IMX_V4L2SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_IMX_V4L2SRC, GstImxV4l2SrcClass))
#define GST_IS_IMX_V4L2SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_IMX_V4L2SRC))
#define GST_IS_IMX_V4L2SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_IMX_V4L2SRC))

typedef struct _GstImxV4l2Src GstImxV4l2Src;
typedef struct _GstImxV4l2SrcClass GstImxV4l2SrcClass;

struct _GstImxV4l2Src {
  GstPushSrc videosrc;
  gchar *device;
  guint frame_plus;
  GstBufferPool *pool;
  GstAllocator *allocator;
  gpointer v4l2handle;
  GstCaps *probed_caps;
  GstCaps *old_caps;
  GList * gstbuffer_in_v4l2;
  guint w, h, fps_n, fps_d;
  guint v4l2fmt;
  guint actual_buf_cnt;
  GstVideoAlignment video_align;
  GstClockTime duration;
  GstClockTime base_time_org;
  gboolean stream_on;
  gboolean use_my_allocator;
  gboolean use_v4l2_memory;
};

struct _GstImxV4l2SrcClass {
  GstPushSrcClass parent_class;
};

GType gst_imx_v4l2src_get_type(void);

#endif
