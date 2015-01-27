/* GStreamer IMX Video Processing Device Abstract
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

#ifndef __IMX_VIDEO_PROCESS_DEVICE_H__
#define __IMX_VIDEO_PROCESS_DEVICE_H__

#include <gst/video/video.h>
#include "gstallocatorphymem.h"
#include "gstimxcommon.h"

#define ALIGNMENT (16)
#define ISALIGNED(a, b) (!(a & (b-1)))
#define ALIGNTO(a, b) ((a + (b-1)) & (~(b-1)))

typedef enum {
  IMX_VP_DEVICE_G2D,
  IMX_VP_DEVICE_IPU,
  IMX_VP_DEVICE_PXP,
  IMX_VP_DEVICE_GLES2,
} ImxVpDeviceType;

typedef enum {
  IMX_VP_DEVICE_CAP_SCALE        = 0x01,
  IMX_VP_DEVICE_CAP_CSC          = 0x02,
  IMX_VP_DEVICE_CAP_ROTATE       = 0x04,
  IMX_VP_DEVICE_CAP_DEINTERLACE  = 0x08,
  IMX_VP_DEVICE_CAP_ALPHA        = 0x10,
  IMX_VP_DEVICE_CAP_ALL          = 0x1F
} ImxVpDeviceCap;

typedef enum {
  IMX_VIDEO_ROTATION_0,
  IMX_VIDEO_ROTATION_90,
  IMX_VIDEO_ROTATION_180,
  IMX_VIDEO_ROTATION_270,
  IMX_VIDEO_ROTATION_HFLIP,
  IMX_VIDEO_ROTATION_VFLIP
} ImxVideoRotationMode;

typedef enum {
  IMX_VIDEO_DEINTERLACE_NONE,
  IMX_VIDEO_DEINTERLACE_LOW_MOTION,
  IMX_VIDEO_DEINTERLACE_MID_MOTION,
  IMX_VIDEO_DEINTERLACE_HIGH_MOTION
} ImxVideoDeinterlaceMode;

typedef enum {
  IMX_VIDEO_INTERLACE_PROGRESSIVE,
  IMX_VIDEO_INTERLACE_INTERLEAVED,
  IMX_VIDEO_INTERLACE_FIELDS
} ImxVideoInterlaceType;

typedef struct {
  GstVideoFormat in_fmt;
  GstVideoFormat out_fmt;
  gint  complexity;
  gint  loss;
} ImxVideoTransformMap;

typedef struct _ImxVideoCrop {
  guint         x;
  guint         y;
  guint         w;
  guint         h;
} ImxVideoCrop;

typedef struct _ImxVideoInfo {
  GstVideoFormat fmt;
  guint w;
  guint h;
  guint stride;
} ImxVideoInfo;

typedef struct _ImxVideoProcessDevice  ImxVideoProcessDevice;
struct _ImxVideoProcessDevice {
  ImxVpDeviceType  device_type;

  /* point to concrete device object */
  gpointer priv;

  /* device interfaces */
  gint     (*open)                    (ImxVideoProcessDevice* device);
  gint     (*close)                   (ImxVideoProcessDevice* device);
  gint     (*alloc_mem)               (ImxVideoProcessDevice* device,
                                        PhyMemBlock *memblk);
  gint     (*free_mem)                (ImxVideoProcessDevice* device,
                                        PhyMemBlock *memblk);
  gint     (*frame_copy)              (ImxVideoProcessDevice* device,
                                        PhyMemBlock *from, PhyMemBlock *to);
  gint     (*set_deinterlace)         (ImxVideoProcessDevice* device,
                                        ImxVideoDeinterlaceMode mode);
  gint     (*set_rotate)              (ImxVideoProcessDevice* device,
                                        ImxVideoRotationMode mode);
  gint     (*config_input)            (ImxVideoProcessDevice* device,
                                        ImxVideoInfo* in_info);
  gint     (*config_output)           (ImxVideoProcessDevice* device,
                                        ImxVideoInfo* out_info);
  gint     (*do_convert)              (ImxVideoProcessDevice* device,
                                       PhyMemBlock *from, PhyMemBlock *to,
                                       ImxVideoInterlaceType interlace_type,
                                       ImxVideoCrop incrop,
                                       ImxVideoCrop outcrop);

  gint                    (*get_capabilities)        (void);
  GList*                  (*get_supported_in_fmts)   (void);
  GList*                  (*get_supported_out_fmts)  (void);
  ImxVideoRotationMode    (*get_rotate)     (ImxVideoProcessDevice* device);
  ImxVideoDeinterlaceMode (*get_deinterlace)(ImxVideoProcessDevice* device);
};

typedef struct _ImxVideoProcessDeviceInfo {
  gchar *name;
  gchar *description;
  gchar *detail;
  ImxVideoProcessDevice*  (*create)   (void);
  gint                    (*destroy)  (ImxVideoProcessDevice* dev);
} ImxVideoProcessDeviceInfo;

const ImxVideoProcessDeviceInfo * imx_get_video_process_devices(void);

#endif /* __IMX_VIDEO_PROCESS_DEVICE_H__ */
