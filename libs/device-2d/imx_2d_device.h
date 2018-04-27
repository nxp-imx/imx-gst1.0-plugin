/* GStreamer IMX Video 2D Device Abstract
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2018 NXP
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

#ifndef __IMX_VIDEO_2D_DEVICE_H__
#define __IMX_VIDEO_2D_DEVICE_H__

#include <gst/video/video.h>
#include "gstimxcommon.h"
#include "gstimx.h"
#include "imx_2d_device_allocator.h"

#define ALIGNMENT (16)
#define ISALIGNED(a, b) (!(a & (b-1)))
#define ALIGNTO(a, b) ((a + (b-1)) & (~(b-1)))

typedef enum {
  IMX_2D_DEVICE_G2D,
  IMX_2D_DEVICE_IPU,
  IMX_2D_DEVICE_PXP,
  IMX_2D_DEVICE_GLES2,
} Imx2DDeviceType;

typedef enum {
  IMX_2D_DEVICE_CAP_SCALE        = 0x01,
  IMX_2D_DEVICE_CAP_CSC          = 0x02,
  IMX_2D_DEVICE_CAP_ROTATE       = 0x04,
  IMX_2D_DEVICE_CAP_DEINTERLACE  = 0x08,
  IMX_2D_DEVICE_CAP_ALPHA        = 0x10,
  IMX_2D_DEVICE_CAP_BLEND        = 0x20,
  IMX_2D_DEVICE_CAP_OVERLAY      = 0x40,
  IMX_2D_DEVICE_CAP_ALL          = 0x1F
} Imx2DDeviceCap;

typedef enum {
  IMX_2D_TILE_NULL,
  IMX_2D_TILE_AMHPION
} Imx2DTileType;

typedef enum {
  IMX_2D_ROTATION_0,
  IMX_2D_ROTATION_90,
  IMX_2D_ROTATION_180,
  IMX_2D_ROTATION_270,
  IMX_2D_ROTATION_HFLIP,
  IMX_2D_ROTATION_VFLIP
} Imx2DRotationMode;

typedef enum {
  IMX_2D_DEINTERLACE_NONE,
  IMX_2D_DEINTERLACE_LOW_MOTION,
  IMX_2D_DEINTERLACE_MID_MOTION,
  IMX_2D_DEINTERLACE_HIGH_MOTION
} Imx2DDeinterlaceMode;

typedef enum {
  IMX_2D_INTERLACE_PROGRESSIVE,
  IMX_2D_INTERLACE_INTERLEAVED,
  IMX_2D_INTERLACE_FIELDS
} Imx2DInterlaceType;

typedef struct {
  GstVideoFormat in_fmt;
  GstVideoFormat out_fmt;
  gint  complexity;
  gint  loss;
} Imx2DTransformMap;

typedef struct _Imx2DCrop {
  gint x;
  gint y;
  guint w;
  guint h;
} Imx2DCrop;

typedef struct _Imx2DVideoInfo {
  GstVideoFormat fmt;
  guint w;
  guint h;
  guint stride;
  Imx2DTileType tile_type;
} Imx2DVideoInfo;

typedef struct _Imx2DFrame {
  PhyMemBlock           *mem;
  gint                  fd[4];
  Imx2DVideoInfo        info;
  Imx2DCrop             crop;
  Imx2DRotationMode     rotate;
  Imx2DInterlaceType    interlace_type;
  gint                  alpha;
} Imx2DFrame;

typedef struct _Imx2DDevice  Imx2DDevice;
struct _Imx2DDevice {
  Imx2DDeviceType  device_type;

  /* point to concrete device object */
  gpointer priv;

  /* device interfaces */
  gint (*open)        (Imx2DDevice* device);
  gint (*close)       (Imx2DDevice* device);
  gint (*alloc_mem)   (Imx2DDevice* device, PhyMemBlock *memblk);
  gint (*free_mem)    (Imx2DDevice* device, PhyMemBlock *memblk);
  gint (*copy_mem)    (Imx2DDevice* device, PhyMemBlock *dst_mem,
                       PhyMemBlock *src_mem, guint offset, guint size);
  gint (*frame_copy)  (Imx2DDevice* device, PhyMemBlock *from, PhyMemBlock *to);
  gint (*set_deinterlace) (Imx2DDevice* device, Imx2DDeinterlaceMode mode);
  gint (*set_rotate)      (Imx2DDevice* device, Imx2DRotationMode mode);
  gint (*config_input)    (Imx2DDevice* device, Imx2DVideoInfo* in_info);
  gint (*config_output)   (Imx2DDevice* device, Imx2DVideoInfo* out_info);
  gint (*convert)   (Imx2DDevice* device, Imx2DFrame *dst, Imx2DFrame *src);
  gint (*blend)        (Imx2DDevice* device, Imx2DFrame *dst, Imx2DFrame *src);
  gint (*blend_finish) (Imx2DDevice* device);
  gint (*fill)         (Imx2DDevice* device, Imx2DFrame *dst, guint RGBA8888);

  gint                 (*get_capabilities)        (Imx2DDevice* device);
  GList*               (*get_supported_in_fmts)   (Imx2DDevice* device);
  GList*               (*get_supported_out_fmts)  (Imx2DDevice* device);
  Imx2DRotationMode    (*get_rotate)              (Imx2DDevice* device);
  Imx2DDeinterlaceMode (*get_deinterlace)         (Imx2DDevice* device);
};

typedef struct _Imx2DDeviceInfo {
  gchar *name;
  Imx2DDeviceType  device_type;
  Imx2DDevice*  (*create)   (Imx2DDeviceType  device_type);
  gint          (*destroy)  (Imx2DDevice* dev);
  gboolean      (*is_exist) (void);
} Imx2DDeviceInfo;

const Imx2DDeviceInfo * imx_get_2d_devices(void);
Imx2DDevice * imx_2d_device_create(Imx2DDeviceType  device_type);
gint imx_2d_device_destroy(Imx2DDevice *device);

#endif /* __IMX_2D_DEVICE_H__ */
