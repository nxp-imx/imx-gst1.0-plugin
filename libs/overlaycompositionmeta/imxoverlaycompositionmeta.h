/* Process video overlay composition meta by IMX 2D devices
 * Copyright (c) 2015-2016, Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef __IMXOVERLAYCOMPOSITIONMETA_H__
#define __IMXOVERLAYCOMPOSITIONMETA_H__

#include <gst/gst.h>
#include "gstimxcommon.h"
#include "../device-2d/imx_2d_device.h"

#define IMX_OVERLAY_COMPOSITION_INIT_BUFFER_SIZE       ((1920*1088*4)/4)

typedef struct _GstImxVideoOverlayComposition {
  Imx2DDevice *device;
  GstAllocator *allocator;
  GstBuffer *tmp_buf;
  guint tmp_buf_size;
} GstImxVideoOverlayComposition;

typedef struct _VideoCompositionVideoInfo {
  GstVideoFormat fmt;
  guint width;
  guint height;
  guint stride;
  gint crop_x;
  gint crop_y;
  guint crop_w;
  guint crop_h;
  Imx2DRotationMode rotate;
  GstVideoAlignment align;
  PhyMemBlock *mem;
  int fd[4];
  GstBuffer *buf;
} VideoCompositionVideoInfo;

gboolean imx_video_overlay_composition_is_out_fmt_support(Imx2DDevice *device,
                                                    GstVideoFormat fmt);
void imx_video_overlay_composition_init(GstImxVideoOverlayComposition *vcomp,
                                        Imx2DDevice *device);
void imx_video_overlay_composition_deinit(GstImxVideoOverlayComposition *vcomp);
gboolean imx_video_overlay_composition_has_meta(GstBuffer *in);
void imx_video_overlay_composition_add_caps(GstCaps *caps);
void imx_video_overlay_composition_remove_caps(GstCaps *caps);
void imx_video_overlay_composition_add_query_meta(GstQuery *query);
gint imx_video_overlay_composition_composite(
                                      GstImxVideoOverlayComposition *vcomp,
                                      VideoCompositionVideoInfo *in,
                                      VideoCompositionVideoInfo *out,
                                      gboolean config_out);
void imx_video_overlay_composition_copy_meta(GstBuffer *dst, GstBuffer *src,
                                             guint in_width, guint in_height,
                                             guint out_width, guint out_height);
gint imx_video_overlay_composition_remove_meta(GstBuffer *buffer);

#endif /* __IMXOVERLAYCOMPOSITIONMETA_H__ */
