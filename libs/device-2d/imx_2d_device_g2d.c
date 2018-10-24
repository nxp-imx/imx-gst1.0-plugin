/* GStreamer IMX G2D Device
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

#include <fcntl.h>
#include <sys/ioctl.h>
#include "g2d.h"
#include "g2dExt.h"
#include "imx_2d_device.h"

GST_DEBUG_CATEGORY_EXTERN (imx2ddevice_debug);
#define GST_CAT_DEFAULT imx2ddevice_debug

typedef struct _Imx2DDeviceG2d {
  gint capabilities;
  struct g2d_surfaceEx src;
  struct g2d_surfaceEx dst;
} Imx2DDeviceG2d;

typedef struct {
  GstVideoFormat gst_video_format;
  guint g2d_format;
  guint bpp;
} G2dFmtMap;

static G2dFmtMap g2d_fmts_map[] = {
    {GST_VIDEO_FORMAT_RGB16,  G2D_RGB565,   16},
    {GST_VIDEO_FORMAT_RGBx,   G2D_RGBX8888, 32},
    {GST_VIDEO_FORMAT_RGBA,   G2D_RGBA8888, 32},
    {GST_VIDEO_FORMAT_BGRA,   G2D_BGRA8888, 32},
    {GST_VIDEO_FORMAT_BGRx,   G2D_BGRX8888, 32},
    {GST_VIDEO_FORMAT_BGR16,  G2D_BGR565,   16},
    {GST_VIDEO_FORMAT_ARGB,   G2D_ARGB8888, 32},
    {GST_VIDEO_FORMAT_ABGR,   G2D_ABGR8888, 32},
    {GST_VIDEO_FORMAT_xRGB,   G2D_XRGB8888, 32},
    {GST_VIDEO_FORMAT_xBGR,   G2D_XBGR8888, 32},

    //this only for separate YUV format and RGB format
    {GST_VIDEO_FORMAT_UNKNOWN, -1,          1},

    {GST_VIDEO_FORMAT_I420,   G2D_I420,     12},
    {GST_VIDEO_FORMAT_NV12,   G2D_NV12,     12},
    // no dpu
    {GST_VIDEO_FORMAT_UYVY,   G2D_UYVY,     16},
    {GST_VIDEO_FORMAT_YUY2,   G2D_YUYV,     16},
    {GST_VIDEO_FORMAT_YVYU,   G2D_YVYU,     16},

    {GST_VIDEO_FORMAT_YV12,   G2D_YV12,     12},
    {GST_VIDEO_FORMAT_NV16,   G2D_NV16,     16},
    {GST_VIDEO_FORMAT_NV21,   G2D_NV21,     12},

/* There is no corresponding GST Video format for those G2D formats
    {GST_VIDEO_FORMAT_VYUY,   G2D_VYUY,     16},
    {GST_VIDEO_FORMAT_NV61,   G2D_NV61,     16},
*/
    {GST_VIDEO_FORMAT_UNKNOWN, -1,          0}
};

static G2dFmtMap g2d_fmts_map_dpu[] = {
    {GST_VIDEO_FORMAT_RGB16,  G2D_RGB565,   16},
    {GST_VIDEO_FORMAT_RGBx,   G2D_RGBX8888, 32},
    {GST_VIDEO_FORMAT_RGBA,   G2D_RGBA8888, 32},
    {GST_VIDEO_FORMAT_BGRA,   G2D_BGRA8888, 32},
    {GST_VIDEO_FORMAT_BGRx,   G2D_BGRX8888, 32},
    {GST_VIDEO_FORMAT_BGR16,  G2D_BGR565,   16},
    {GST_VIDEO_FORMAT_ARGB,   G2D_ARGB8888, 32},
    {GST_VIDEO_FORMAT_ABGR,   G2D_ABGR8888, 32},
    {GST_VIDEO_FORMAT_xRGB,   G2D_XRGB8888, 32},
    {GST_VIDEO_FORMAT_xBGR,   G2D_XBGR8888, 32},
    //HAS_DPU
    {GST_VIDEO_FORMAT_UYVY,   G2D_UYVY,     16},
    {GST_VIDEO_FORMAT_YUY2,   G2D_YUYV,     16},

    //this only for separate YUV format and RGB format
    {GST_VIDEO_FORMAT_UNKNOWN, -1,          1},

    {GST_VIDEO_FORMAT_I420,   G2D_I420,     12},
    {GST_VIDEO_FORMAT_NV12,   G2D_NV12,     12},

    {GST_VIDEO_FORMAT_YV12,   G2D_YV12,     12},
    {GST_VIDEO_FORMAT_NV16,   G2D_NV16,     16},
    {GST_VIDEO_FORMAT_NV21,   G2D_NV21,     12},

/* There is no corresponding GST Video format for those G2D formats
    {GST_VIDEO_FORMAT_VYUY,   G2D_VYUY,     16},
    {GST_VIDEO_FORMAT_NV61,   G2D_NV61,     16},
*/
    {GST_VIDEO_FORMAT_UNKNOWN, -1,          0}
};

static const G2dFmtMap * imx_g2d_get_format(GstVideoFormat format)
{
  const G2dFmtMap *map;
  if (HAS_DPU()) {
    map = g2d_fmts_map_dpu;
  } else {
    map = g2d_fmts_map;
  }
  while(map->bpp > 0) {
    if (map->gst_video_format == format)
      return map;
    map++;
  };

  GST_ERROR ("g2d : format (%x) is not supported.",
              gst_video_format_to_string(format));

  return NULL;
}

static gint imx_g2d_open(Imx2DDevice *device)
{
  if (!device)
    return -1;

  Imx2DDeviceG2d *g2d = g_slice_alloc(sizeof(Imx2DDeviceG2d));
  if (!g2d) {
    GST_ERROR("allocate g2d structure failed\n");
    return -1;
  }

  memset(g2d, 0, sizeof (Imx2DDeviceG2d));
  device->priv = (gpointer)g2d;

  return 0;
}

static gint imx_g2d_close(Imx2DDevice *device)
{
  if (!device)
    return -1;

  if (device) {
    Imx2DDeviceG2d *g2d = (Imx2DDeviceG2d *) (device->priv);
    if (g2d)
      g_slice_free1(sizeof(Imx2DDeviceG2d), g2d);
    device->priv = NULL;
  }
  return 0;
}


static gint
imx_g2d_alloc_mem(Imx2DDevice *device, PhyMemBlock *memblk)
{
  struct g2d_buf *pbuf = NULL;

  if (!device || !device->priv || !memblk)
    return -1;

  memblk->size = PAGE_ALIGN(memblk->size);

  pbuf = g2d_alloc (memblk->size, 0);
  if (!pbuf) {
    GST_ERROR("G2D allocate %u bytes memory failed: %s",
              memblk->size, strerror(errno));
    return -1;
  }

  memblk->vaddr = (guchar*) pbuf->buf_vaddr;
  memblk->paddr = (guchar*) pbuf->buf_paddr;
  memblk->user_data = (gpointer) pbuf;
  GST_DEBUG("G2D allocated memory (%p)", memblk->paddr);

  return 0;
}

static gint imx_g2d_free_mem(Imx2DDevice *device, PhyMemBlock *memblk)
{
  if (!device || !device->priv || !memblk)
    return -1;

  GST_DEBUG("G2D free memory (%p)", memblk->paddr);
  gint ret = g2d_free ((struct g2d_buf*)(memblk->user_data));
  memblk->user_data = NULL;
  memblk->vaddr = NULL;
  memblk->paddr = NULL;
  memblk->size = 0;

  return ret;
}

static gint imx_g2d_copy_mem(Imx2DDevice* device, PhyMemBlock *dst_mem,
                             PhyMemBlock *src_mem, guint offset, guint size)
{
  struct g2d_buf *pbuf = NULL;
  void *g2d_handle = NULL;
  struct g2d_buf src, dst;

  dst_mem->size = src_mem->size;

  pbuf = g2d_alloc (dst_mem->size, 0);
  if (!pbuf) {
    GST_ERROR ("g2d_alloc failed.");
    return -1;
  }
  dst_mem->vaddr = (gchar*) pbuf->buf_vaddr;
  dst_mem->paddr = (gchar*) pbuf->buf_paddr;
  dst_mem->user_data = (gpointer) pbuf;

  if(g2d_open(&g2d_handle) == -1 || g2d_handle == NULL) {
    GST_ERROR ("Failed to open g2d device.");
    return -1;
  }

  src.buf_handle = NULL;
  src.buf_vaddr = src_mem->vaddr + offset;
  src.buf_paddr = (gint)(src_mem->paddr + offset);
  src.buf_size = src_mem->size - offset;
  dst.buf_handle = NULL;
  dst.buf_vaddr = dst_mem->vaddr;
  dst.buf_paddr = (gint)(dst_mem->paddr);
  dst.buf_size = dst_mem->size;

  if (size > dst.buf_size)
    size = dst.buf_size;

  g2d_copy (g2d_handle, &dst, &src, size);
  g2d_finish(g2d_handle);
  g2d_close (g2d_handle);

  GST_DEBUG ("G2D copy from vaddr (%p), paddr (%p), size (%d) to "
      "vaddr (%p), paddr (%p), size (%d)",
      src_mem->vaddr, src_mem->paddr, src_mem->size,
      dst_mem->vaddr, dst_mem->paddr, dst_mem->size);

  return 0;
}

static gint imx_g2d_frame_copy(Imx2DDevice *device,
                               PhyMemBlock *from, PhyMemBlock *to)
{
  struct g2d_buf src, dst;
  gint ret = 0;

  if (!device || !device->priv || !from || !to)
    return -1;

  Imx2DDeviceG2d *g2d = (Imx2DDeviceG2d *) (device->priv);

  void *g2d_handle = NULL;

  if(g2d_open(&g2d_handle) == -1 || g2d_handle == NULL) {
    GST_ERROR ("%s Failed to open g2d device.",__FUNCTION__);
    return -1;
  }

  src.buf_handle = NULL;
  src.buf_vaddr = (void*)(from->vaddr);
  src.buf_paddr = (gint)(from->paddr);
  src.buf_size = from->size;
  dst.buf_handle = NULL;
  dst.buf_vaddr = (void *)(to->vaddr);
  dst.buf_paddr = (gint)(to->paddr);
  dst.buf_size = to->size;

  ret = g2d_copy (g2d_handle, &dst, &src, dst.buf_size);

  g2d_finish(g2d_handle);
  g2d_close(g2d_handle);
  GST_LOG("G2D frame memory (%p)->(%p)", from->paddr, to->paddr);

  return ret;
}

static gint imx_g2d_config_input(Imx2DDevice *device, Imx2DVideoInfo* in_info)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDeviceG2d *g2d = (Imx2DDeviceG2d *) (device->priv);
  const G2dFmtMap *in_map = imx_g2d_get_format(in_info->fmt);
  if (!in_map)
    return -1;

  g2d->src.base.width = in_info->w;
  g2d->src.base.height = in_info->h;
  g2d->src.base.stride = g2d->src.base.width;//stride / (in_map->bpp/8);
  g2d->src.base.format = in_map->g2d_format;
  g2d->src.base.left = 0;
  g2d->src.base.top = 0;
  g2d->src.base.right = in_info->w;
  g2d->src.base.bottom = in_info->h;
  if (in_info->tile_type == IMX_2D_TILE_AMHPION) {
    g2d->src.base.stride = in_info->stride / (in_map->bpp/8);
    g2d->src.tiling = G2D_AMPHION_TILED;
  } else
    g2d->src.tiling = G2D_LINEAR;
  GST_TRACE("input format = %s", gst_video_format_to_string(in_info->fmt));

  return 0;
}

static gint imx_g2d_config_output(Imx2DDevice *device, Imx2DVideoInfo* out_info)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDeviceG2d *g2d = (Imx2DDeviceG2d *) (device->priv);
  const G2dFmtMap *out_map = imx_g2d_get_format(out_info->fmt);
  if (!out_map)
    return -1;

  g2d->dst.base.width = out_info->w;
  g2d->dst.base.height = out_info->h;
  // G2D stride is pixel, not bytes.
  if (out_info->stride < g2d->dst.base.width * (out_map->bpp / 8))
    g2d->dst.base.stride = g2d->dst.base.width;
  else
    g2d->dst.base.stride = out_info->stride / (out_map->bpp / 8);
  g2d->dst.base.format = out_map->g2d_format;
  g2d->dst.base.left = 0;
  g2d->dst.base.top = 0;
  g2d->dst.base.right = out_info->w;
  g2d->dst.base.bottom = out_info->h;
  GST_TRACE("output format = %s", gst_video_format_to_string(out_info->fmt));

  return 0;
}

static gint imx_g2d_set_src_plane(struct g2d_surface *g2d_src, gchar *paddr)
{
  switch(g2d_src->format) {
    case G2D_I420:
      g2d_src->planes[0] = (gint)(paddr);
      g2d_src->planes[1] = (gint)(paddr + g2d_src->width * g2d_src->height);
      g2d_src->planes[2] = g2d_src->planes[1]+g2d_src->width*g2d_src->height/4;
      break;
    case G2D_YV12:
      g2d_src->planes[0] = (gint)(paddr);
      g2d_src->planes[2] = (gint)(paddr + g2d_src->width * g2d_src->height);
      g2d_src->planes[1] = g2d_src->planes[2]+g2d_src->width*g2d_src->height/4;
      break;
    case G2D_NV12:
    case G2D_NV21:
      g2d_src->planes[0] = (gint)(paddr);
      g2d_src->planes[1] = (gint)(paddr + g2d_src->width * g2d_src->height);
      break;
    case G2D_NV16:
      g2d_src->planes[0] = (gint)(paddr);
      g2d_src->planes[1] = (gint)(paddr + g2d_src->width * g2d_src->height);
      break;

    case G2D_RGB565:
    case G2D_RGBX8888:
    case G2D_RGBA8888:
    case G2D_BGRA8888:
    case G2D_BGRX8888:
    case G2D_BGR565:
    case G2D_ARGB8888:
    case G2D_ABGR8888:
    case G2D_XRGB8888:
    case G2D_XBGR8888:
    case G2D_UYVY:
    case G2D_YUYV:
    case G2D_YVYU:
      g2d_src->planes[0] = (gint)(paddr);
      break;
    default:
      GST_ERROR ("G2D: not supported format.");
      return -1;
  }
  return 0;
}

static gboolean is_format_has_alpha(enum g2d_format fmt) {
  if (fmt == G2D_RGBA8888 || fmt == G2D_BGRA8888 ||
      fmt == G2D_ARGB8888 || fmt == G2D_ABGR8888)
    return TRUE;
  return FALSE;
}

static gint imx_g2d_blit(Imx2DDevice *device,
                            Imx2DFrame *dst, Imx2DFrame *src, gboolean alpha_en)
{
  gint ret = 0;
  void *g2d_handle = NULL;

  if (!device || !device->priv || !dst || !src || !dst->mem || !src->mem)
    return -1;

  // Open g2d
  if(g2d_open(&g2d_handle) == -1 || g2d_handle == NULL) {
    GST_ERROR ("%s Failed to open g2d device.",__FUNCTION__);
    return -1;
  }

  Imx2DDeviceG2d *g2d = (Imx2DDeviceG2d *) (device->priv);

  GST_DEBUG ("src paddr fd vaddr: %p %d %p dst paddr fd vaddr: %p %d %p",
      src->mem->paddr, src->fd[0], src->mem->vaddr, dst->mem->paddr,
      dst->fd[0], dst->mem->vaddr);

  unsigned long paddr = 0;
  if (!src->mem->paddr) {
    if (src->fd[0] >= 0) {
      paddr = phy_addr_from_fd (src->fd[0]);
    } else if (src->mem->vaddr) {
      paddr = phy_addr_from_vaddr (src->mem->vaddr, PAGE_ALIGN(src->mem->size));
    } else {
      GST_ERROR ("Invalid parameters.");
      return -1;
    }
    if (paddr) {
      src->mem->paddr = paddr;
    } else {
      GST_ERROR ("Can't get physical address.");
      return -1;
    }
  }
  if (!dst->mem->paddr) {
    paddr = phy_addr_from_fd (dst->fd[0]);
    if (paddr) {
      dst->mem->paddr = paddr;
    } else {
      GST_ERROR ("Can't get physical address.");
      return -1;
    }
  }
  GST_DEBUG ("src paddr: %p dst paddr: %p", src->mem->paddr, dst->mem->paddr);

  // Set input
  g2d->src.base.global_alpha = src->alpha;
  g2d->src.base.left = src->crop.x;
  g2d->src.base.top = src->crop.y;
  g2d->src.base.right = src->crop.x + MIN(src->crop.w, g2d->src.base.width-src->crop.x);
  g2d->src.base.bottom = src->crop.y + MIN(src->crop.h, g2d->src.base.height-src->crop.y);

  if (g2d->src.base.left >= g2d->src.base.width || g2d->src.base.top >= g2d->src.base.height ||
      g2d->src.base.right <= 0 || g2d->src.base.bottom <= 0) {
    GST_WARNING("input crop outside of source");
    g2d_close (g2d_handle);
    return 0;
  }

  if (g2d->src.base.left < 0)
    g2d->src.base.left = 0;
  if (g2d->src.base.top < 0)
    g2d->src.base.top = 0;
  if (g2d->src.base.right > g2d->src.base.width)
    g2d->src.base.right = g2d->src.base.width;
  if (g2d->src.base.bottom > g2d->src.base.height)
    g2d->src.base.bottom = g2d->src.base.height;

  if (imx_g2d_set_src_plane (&g2d->src.base, src->mem->paddr) < 0) {
    g2d_close (g2d_handle);
    return -1;
  }

  if (g2d->src.tiling == G2D_AMPHION_TILED && src->fd[1] >= 0)
  {
    if (!src->mem->user_data)
      src->mem->user_data = g2d->src.base.planes[1] = phy_addr_from_fd (src->fd[1]);
    else
      g2d->src.base.planes[1] = src->mem->user_data;
  }
  switch (src->interlace_type) {
    case IMX_2D_INTERLACE_INTERLEAVED:
      g2d->src.tiling |= G2D_AMPHION_INTERLACED;
      break;
    default:
      break;
  }

  GST_TRACE ("g2d src : %dx%d,%d(%d,%d-%d,%d), alpha=%d, format=%d, deinterlace: %d",
      g2d->src.base.width, g2d->src.base.height,g2d->src.base.stride, g2d->src.base.left,
      g2d->src.base.top, g2d->src.base.right, g2d->src.base.bottom, g2d->src.base.global_alpha,
      g2d->src.base.format, g2d->src.tiling);

  // Set output
  g2d->dst.base.global_alpha = dst->alpha;
  g2d->dst.base.planes[0] = (gint)(dst->mem->paddr);
  g2d->dst.base.left = dst->crop.x;
  g2d->dst.base.top = dst->crop.y;
  g2d->dst.base.right = dst->crop.x + dst->crop.w;
  g2d->dst.base.bottom = dst->crop.y + dst->crop.h;

  if (g2d->dst.base.left >= g2d->dst.base.width || g2d->dst.base.top >= g2d->dst.base.height ||
      g2d->dst.base.right <= 0 || g2d->dst.base.bottom <= 0) {
    GST_WARNING("output crop outside of destination");
    g2d_close (g2d_handle);
    return 0;
  }

  if (g2d->dst.base.left < 0)
    g2d->dst.base.left = 0;
  if (g2d->dst.base.top < 0)
    g2d->dst.base.top = 0;
  if (g2d->dst.base.right > g2d->dst.base.width)
    g2d->dst.base.right = g2d->dst.base.width;
  if (g2d->dst.base.bottom > g2d->dst.base.height)
    g2d->dst.base.bottom = g2d->dst.base.height;

  //adjust incrop size by outcrop size and output resolution
  guint src_w, src_h, dst_w, dst_h, org_src_left, org_src_top;
  src_w = g2d->src.base.right-g2d->src.base.left;
  src_h = g2d->src.base.bottom-g2d->src.base.top;
  dst_w = dst->crop.w;
  dst_h = dst->crop.h;
  org_src_left = g2d->src.base.left;
  org_src_top = g2d->src.base.top;

  g2d->src.base.left = org_src_left + (g2d->dst.base.left-dst->crop.x) * src_w / dst_w;
  g2d->src.base.top = org_src_top + (g2d->dst.base.top-dst->crop.y) * src_h / dst_h;
  g2d->src.base.right = org_src_left + (g2d->dst.base.right-dst->crop.x) * src_w / dst_w;
  g2d->src.base.bottom = org_src_top + (g2d->dst.base.bottom-dst->crop.y) * src_h / dst_h;

  GST_TRACE ("g2d dest : %dx%d,%d(%d,%d-%d,%d), alpha=%d, format=%d",
      g2d->dst.base.width, g2d->dst.base.height,g2d->dst.base.stride, g2d->dst.base.left,
      g2d->dst.base.top, g2d->dst.base.right, g2d->dst.base.bottom, g2d->dst.base.global_alpha,
      g2d->dst.base.format);

  // Final blending
  if (alpha_en &&
      (g2d->src.base.global_alpha < 0xFF || is_format_has_alpha(g2d->src.base.format))) {
    g2d->src.base.blendfunc = G2D_ONE;
    g2d->dst.base.blendfunc = G2D_ONE_MINUS_SRC_ALPHA;
    g2d_enable(g2d_handle, G2D_BLEND);
    g2d_enable(g2d_handle, G2D_GLOBAL_ALPHA);

    ret = g2d_blitEx(g2d_handle, &g2d->src, &g2d->dst);

    g2d_disable(g2d_handle, G2D_GLOBAL_ALPHA);
    g2d_disable(g2d_handle, G2D_BLEND);
  } else {
    ret = g2d_blitEx(g2d_handle, &g2d->src, &g2d->dst);
  }

  ret |= g2d_finish(g2d_handle);
  g2d_close (g2d_handle);

  return ret;
}

static gint imx_g2d_convert(Imx2DDevice *device,
                            Imx2DFrame *dst, Imx2DFrame *src)
{
  return imx_g2d_blit(device, dst, src, FALSE);
}

static gint imx_g2d_set_rotate(Imx2DDevice *device, Imx2DRotationMode rot)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDeviceG2d *g2d = (Imx2DDeviceG2d *) (device->priv);
  gint g2d_rotate = G2D_ROTATION_0;
  switch (rot) {
  case IMX_2D_ROTATION_0:      g2d_rotate = G2D_ROTATION_0;    break;
  case IMX_2D_ROTATION_90:     g2d_rotate = G2D_ROTATION_90;   break;
  case IMX_2D_ROTATION_180:    g2d_rotate = G2D_ROTATION_180;  break;
  case IMX_2D_ROTATION_270:    g2d_rotate = G2D_ROTATION_270;  break;
  case IMX_2D_ROTATION_HFLIP:  g2d_rotate = G2D_FLIP_H;        break;
  case IMX_2D_ROTATION_VFLIP:  g2d_rotate = G2D_FLIP_V;        break;
  default:                     g2d_rotate = G2D_ROTATION_0;    break;
  }

  g2d->dst.base.rot = g2d_rotate;
  return 0;
}

static gint imx_g2d_set_deinterlace(Imx2DDevice *device,
                                    Imx2DDeinterlaceMode mode)
{
  return 0;
}

static Imx2DRotationMode imx_g2d_get_rotate (Imx2DDevice* device)
{
  if (!device || !device->priv)
    return 0;

  Imx2DDeviceG2d *g2d = (Imx2DDeviceG2d *) (device->priv);
  Imx2DRotationMode rot = IMX_2D_ROTATION_0;
  switch (g2d->dst.base.rot) {
  case G2D_ROTATION_0:    rot = IMX_2D_ROTATION_0;     break;
  case G2D_ROTATION_90:   rot = IMX_2D_ROTATION_90;    break;
  case G2D_ROTATION_180:  rot = IMX_2D_ROTATION_180;   break;
  case G2D_ROTATION_270:  rot = IMX_2D_ROTATION_270;   break;
  case G2D_FLIP_H:        rot = IMX_2D_ROTATION_HFLIP; break;
  case G2D_FLIP_V:        rot = IMX_2D_ROTATION_VFLIP; break;
  default:                rot = IMX_2D_ROTATION_0;     break;
  }

  return rot;
}

static Imx2DDeinterlaceMode imx_g2d_get_deinterlace (Imx2DDevice* device)
{
  return IMX_2D_DEINTERLACE_NONE;
}

static gint imx_g2d_get_capabilities (Imx2DDevice* device)
{
  gint capabilities = IMX_2D_DEVICE_CAP_SCALE|IMX_2D_DEVICE_CAP_CSC \
                      | IMX_2D_DEVICE_CAP_ROTATE | IMX_2D_DEVICE_CAP_ALPHA
                      | IMX_2D_DEVICE_CAP_BLEND;

  return capabilities;
}

static GList* imx_g2d_get_supported_in_fmts(Imx2DDevice* device)
{
  GList* list = NULL;
  const G2dFmtMap *map;
  if (HAS_DPU()) {
    map = g2d_fmts_map_dpu;
  } else {
    map = g2d_fmts_map;
  }
  while (map->bpp > 0) {
    if (map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN)
      list = g_list_append(list, (gpointer)(map->gst_video_format));
    map++;
  }

  return list;
}

static GList* imx_g2d_get_supported_out_fmts(Imx2DDevice* device)
{
  GList* list = NULL;
  const G2dFmtMap *map;
  if (HAS_DPU()) {
    map = g2d_fmts_map_dpu;
  } else {
    map = g2d_fmts_map;
  }

  while (map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN) {
    list = g_list_append(list, (gpointer)(map->gst_video_format));
    map++;
  }

  return list;
}

static gint imx_g2d_blend(Imx2DDevice *device, Imx2DFrame *dst, Imx2DFrame *src)
{
  return imx_g2d_blit(device, dst, src, TRUE);
}

static gint imx_g2d_blend_finish(Imx2DDevice *device)
{
  //do nothing
  return 0;
}

static gint imx_g2d_fill_color(Imx2DDevice *device, Imx2DFrame *dst,
                                guint RGBA8888)
{
  void *g2d_handle = NULL;
  gint ret = 0;

  if (!device || !device->priv || !dst || !dst->mem)
    return -1;

  if(g2d_open(&g2d_handle) == -1 || g2d_handle == NULL) {
    GST_ERROR ("%s Failed to open g2d device.",__FUNCTION__);
    return -1;
  }

  Imx2DDeviceG2d *g2d = (Imx2DDeviceG2d *) (device->priv);

  GST_DEBUG ("dst paddr: %p fd: %d", dst->mem->paddr, dst->fd[0]);
  unsigned long paddr = 0;
  if (!dst->mem->paddr) {
    paddr = phy_addr_from_fd (dst->fd[0]);
    if (paddr) {
      dst->mem->paddr = paddr;
    } else {
      GST_ERROR ("Can't get physical address.");
      return -1;
    }
  }
  GST_DEBUG ("dst paddr: %p", dst->mem->paddr);

  g2d->dst.base.clrcolor = RGBA8888;
  g2d->dst.base.planes[0] = (gint)(dst->mem->paddr);
  g2d->dst.base.left = 0;
  g2d->dst.base.top = 0;
  g2d->dst.base.right = g2d->dst.base.width;
  g2d->dst.base.bottom = g2d->dst.base.height;

  GST_TRACE ("g2d clear : %dx%d,%d(%d,%d-%d,%d), format=%d",
      g2d->dst.base.width, g2d->dst.base.height, g2d->dst.base.stride, g2d->dst.base.left,
      g2d->dst.base.top, g2d->dst.base.right, g2d->dst.base.bottom, g2d->dst.base.format);

  ret = g2d_clear(g2d_handle, &g2d->dst.base);
  ret |= g2d_finish(g2d_handle);
  g2d_close(g2d_handle);

  return ret;
}

Imx2DDevice * imx_g2d_create(Imx2DDeviceType  device_type)
{
  Imx2DDevice * device = g_slice_alloc(sizeof(Imx2DDevice));
  if (!device) {
    GST_ERROR("allocate device structure failed\n");
    return NULL;
  }

  device->device_type = device_type;
  device->priv = NULL;

  device->open                = imx_g2d_open;
  device->close               = imx_g2d_close;
  device->alloc_mem           = imx_g2d_alloc_mem;
  device->free_mem            = imx_g2d_free_mem;
  device->copy_mem            = imx_g2d_copy_mem;
  device->frame_copy          = imx_g2d_frame_copy;
  device->config_input        = imx_g2d_config_input;
  device->config_output       = imx_g2d_config_output;
  device->convert             = imx_g2d_convert;
  device->blend               = imx_g2d_blend;
  device->blend_finish        = imx_g2d_blend_finish;
  device->fill                = imx_g2d_fill_color;
  device->set_rotate          = imx_g2d_set_rotate;
  device->set_deinterlace     = imx_g2d_set_deinterlace;
  device->get_rotate          = imx_g2d_get_rotate;
  device->get_deinterlace     = imx_g2d_get_deinterlace;
  device->get_capabilities    = imx_g2d_get_capabilities;
  device->get_supported_in_fmts  = imx_g2d_get_supported_in_fmts;
  device->get_supported_out_fmts = imx_g2d_get_supported_out_fmts;

  return device;
}

gint imx_g2d_destroy(Imx2DDevice *device)
{
  if (!device)
    return -1;

  g_slice_free1(sizeof(Imx2DDevice), device);

  return 0;
}

gboolean imx_g2d_is_exist (void)
{
  return HAS_G2D();
}
