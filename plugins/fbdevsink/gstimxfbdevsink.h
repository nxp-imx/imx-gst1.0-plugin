/*
 * Copyright 2017 NXP
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


#ifndef __GST_IMX_FBDEVSINK_H__
#define __GST_IMX_FBDEVSINK_H__

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>

#include <linux/fb.h>
/*FIXME: Add imx specified fbdev head file if need */

G_BEGIN_DECLS

#define GST_TYPE_IMX_FBDEVSINK \
  (gst_imx_fbdevsink_get_type())
#define GST_IMX_FBDEVSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IMX_FBDEVSINK,GstImxFBDEVSink))
#define GST_IMX_FBDEVSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IMX_FBDEVSINK,GstImxFBDEVSinkClass))
#define GST_IS_IMX_FBDEVSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IMX_FBDEVSINK))
#define GST_IS_IMX_FBDEVSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IMX_FBDEVSINK))

typedef struct _GstImxFBDEVSink GstImxFBDEVSink;
typedef struct _GstImxFBDEVSinkClass GstImxFBDEVSinkClass;

struct _GstImxFBDEVSink {
  GstVideoSink videosink;

  /*< private >*/
  int fd;
  char *device;

  struct fb_fix_screeninfo fixinfo;
  struct fb_var_screeninfo varinfo;
  struct fb_var_screeninfo stored;

  /* status flags */
  gboolean var_stored;
  gboolean unblanked;

  int display_width, display_height; 

  int width, height;
  GstVideoFormat vfmt;

  void *imxoverlay;
  GstVideoOrientationMethod method;

  GstBuffer *last_buffer;
  GstBufferPool *pool;
  GstAllocator *allocator;
  GstVideoCropMeta cropmeta;

  GstVideoInfo vinfo;

  /* video render rectangle for resize */
  GstVideoRectangle video_geo;
  gboolean need_reconfigure;

  gboolean keep_ratio;
  /* debug performance, calculate fps */
  guint64 frame_showed;
  GstClockTime run_time;
};

struct _GstImxFBDEVSinkClass {
  GstVideoSinkClass videosink_class;

};

GType gst_imx_fbdevsink_get_type(void);

G_END_DECLS

#endif /* __GST_IMX_FBDEVSINK_H__ */
