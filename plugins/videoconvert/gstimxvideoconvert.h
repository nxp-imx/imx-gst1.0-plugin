/* GStreamer IMX video convert plugin
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

#ifndef __GST_IMX_VIDEO_CONVERT_H__
#define __GST_IMX_VIDEO_CONVERT_H__

#include <gst/gst.h>
#include "videoprocessdevice.h"

G_BEGIN_DECLS

/* chose a best output format by comparing the loss of the conversion
 * if this is not defined, then the output for will select the first format
 * of it supports
 */
#define COMPARE_CONVERT_LOSS

//#define PASSTHOUGH_FOR_UNSUPPORTED_OUTPUT_FORMAT

/* video convert object and class definition */
typedef struct _GstImxVideoConvert {
  GstVideoFilter element;

  ImxVideoProcessDevice *device;
  GstBufferPool *in_pool;
  GstBufferPool *out_pool;
  GstBuffer *in_buf;
} GstImxVideoConvert;

typedef struct _GstImxVideoConvertClass {
  GstVideoFilterClass parent_class;

  const ImxVideoProcessDeviceInfo *in_plugin;
} GstImxVideoConvertClass;

G_END_DECLS

#endif /* __GST_IMX_VIDEO_CONVERT_H__ */
