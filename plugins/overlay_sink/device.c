/*
 * Copyright (c) 2013-2014, Freescale Semiconductor, Inc. All rights reserved.
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

#include <gst/video/video-format.h>

#include "g2d.h"
#include "device.h"

GST_DEBUG_CATEGORY_EXTERN (overlay_sink_debug);
#define GST_CAT_DEFAULT overlay_sink_debug

typedef struct {
  int fmt;
  int width;
  int height;
} G2DHandle;

typedef struct {
  struct g2d_surface src;
  struct g2d_surface dst;
} G2DSurface;

static gpointer
g2d_device_open (gint fmt, gint width, gint height)
{
  G2DHandle *handle;
  int g2d_fmt;

  if(fmt == GST_MAKE_FOURCC('R', 'G', 'B', 'P'))
    g2d_fmt = G2D_RGB565;
  else if(fmt == GST_MAKE_FOURCC('R', 'G', 'B', 'x'))
    g2d_fmt = G2D_RGBX8888;
  else {
    GST_ERROR ("dst format (%x) is not supported.", fmt);
    return NULL;
  }

  handle = (G2DHandle*) g_slice_alloc (sizeof (G2DHandle)); 
  if (!handle) {
    GST_ERROR ("allocate memory for G2DHandle failed.");
    return NULL;
  }

  handle->fmt = g2d_fmt;
  handle->width = width;
  handle->height = height;

  return handle;
}

static void
g2d_device_close (gpointer device)
{
  G2DHandle *hdevice = (G2DHandle*) device;

  g_slice_free1 (sizeof (G2DHandle), hdevice);

  return;
}

static gint
g2d_device_update_surface_info (gpointer device, SurfaceInfo *info, gpointer surface)
{
  G2DHandle *hdevice = (G2DHandle*) device;
  G2DSurface *hsurface = (G2DSurface*) surface;

  //set input param
  if(info->fmt == GST_VIDEO_FORMAT_I420)
    hsurface->src.format = G2D_I420;
  else if(info->fmt == GST_VIDEO_FORMAT_NV12)
    hsurface->src.format = G2D_NV12;
  else if(info->fmt == GST_VIDEO_FORMAT_YV12)
    hsurface->src.format = G2D_YV12;
  else if(info->fmt == GST_VIDEO_FORMAT_NV16)
    hsurface->src.format = G2D_NV16;
  else if(info->fmt == GST_VIDEO_FORMAT_YUY2)
    hsurface->src.format = G2D_YUYV;
  else if(info->fmt == GST_VIDEO_FORMAT_UYVY)
    hsurface->src.format = G2D_UYVY;
  else if(info->fmt == GST_VIDEO_FORMAT_RGB16)
    hsurface->src.format = G2D_RGB565;
  else if(info->fmt == GST_VIDEO_FORMAT_RGBx)
    hsurface->src.format = G2D_RGBX8888;
  else {
    GST_ERROR ("source format (%x) is not supported.", info->fmt);
    return -1;
  }

  hsurface->src.width = info->src.width;
  hsurface->src.height = info->src.height;
  hsurface->src.stride = info->src.width;
  hsurface->src.left = info->src.left;
  hsurface->src.top = info->src.top;
  hsurface->src.right = info->src.right;
  hsurface->src.bottom = info->src.bottom;

  //GST_DEBUG ("source, format (%x), res (%d,%d), crop (%d,%d) --> (%d,%d)",
  //    hsurface->src.format, hsurface->src.width, hsurface->src.height,
  //    hsurface->src.left, hsurface->src.top, hsurface->src.right, hsurface->src.bottom);

  hsurface->dst.format = hdevice->fmt;
  hsurface->dst.width = hdevice->width;
  hsurface->dst.height = hdevice->height;
  hsurface->dst.stride = hdevice->width;
  hsurface->dst.left = info->dst.left;
  hsurface->dst.top = info->dst.top;
  hsurface->dst.right = info->dst.right;
  hsurface->dst.bottom = info->dst.bottom;
  switch (info->rot) {
    case 0:
      hsurface->dst.rot = G2D_ROTATION_0;
      break;
    case 90:
      hsurface->dst.rot = G2D_ROTATION_90;
      break;
    case 180:
      hsurface->dst.rot = G2D_ROTATION_180;
      break;
    case 270:
      hsurface->dst.rot = G2D_ROTATION_270;
      break;
    default:
      hsurface->dst.rot = G2D_ROTATION_0;
      break;
  }

  //GST_DEBUG ("dest, format (%x), res (%d,%d), crop (%d,%d) --> (%d,%d)",
  //    hsurface->dst.format, hsurface->dst.width, hsurface->dst.height,
  //    hsurface->dst.left, hsurface->dst.top, hsurface->dst.right, hsurface->dst.bottom);

  return 0;
}

static gpointer
g2d_device_create_surface (gpointer device, SurfaceInfo *info)
{
  G2DSurface *surface;

  surface = g_slice_alloc (sizeof(G2DSurface));
  if (!surface) {
    GST_ERROR ("failed allocate G2DSurface.");
    return NULL;
  }

  memset(surface, 0, sizeof(G2DSurface));

  if (g2d_device_update_surface_info (device, info, surface) < 0) {
    g_slice_free1 (sizeof(G2DSurface), surface);
    return NULL;
  }

  return surface;
}

static void
g2d_device_destroy_surface (gpointer device, gpointer surface)
{
  if (surface)
    g_slice_free1 (sizeof(G2DSurface), surface);

  return;
}

static gint
g2d_device_blit_surface (gpointer device, gpointer surface, SurfaceBuffer *buffer, SurfaceBuffer *dest)
{
  G2DHandle *hdevice = (G2DHandle*) device;
  G2DSurface *hsurface = (G2DSurface *)surface;
  void *g2d_handle = NULL;

  if(g2d_open(&g2d_handle) == -1 || g2d_handle == NULL) {
    GST_ERROR ("Failed to open g2d device.");
    return -1;
  }

  switch(hsurface->src.format) {
    case G2D_I420:
    case G2D_YV12:
      hsurface->src.planes[0] = buffer->paddr;
      hsurface->src.planes[1] = buffer->paddr + hsurface->src.width * hsurface->src.height;
      hsurface->src.planes[2] = hsurface->src.planes[1]  + hsurface->src.width * hsurface->src.height / 4;
      //GST_DEBUG ("YUV address: %p, %p, %p", hsurface->src.planes[0], hsurface->src.planes[1], hsurface->src.planes[2]);
      break;
    case G2D_NV12:
      hsurface->src.planes[0] = buffer->paddr;
      hsurface->src.planes[1] = buffer->paddr + hsurface->src.width * hsurface->src.height;
      break;
    case G2D_NV16:
      hsurface->src.planes[0] = buffer->paddr;
      hsurface->src.planes[1] = buffer->paddr + hsurface->src.width * hsurface->src.height;
      break;
    case G2D_RGB565:
    case G2D_RGBX8888:
    case G2D_UYVY:
    case G2D_YUYV:
      hsurface->src.planes[0] = buffer->paddr;
      break;
    default:
      GST_ERROR ("not supported format.");
      return -1;
  }

  hsurface->dst.planes[0] = dest->paddr;

  //GST_DEBUG ("dest, format (%x), res (%d,%d), crop (%d,%d) --> (%d,%d)",
  //    hsurface->dst.format, hsurface->dst.width, hsurface->dst.height,
  //    hsurface->dst.left, hsurface->dst.top, hsurface->dst.right, hsurface->dst.bottom);

  g2d_blit(g2d_handle, &hsurface->src, &hsurface->dst);
  g2d_finish(g2d_handle);
  g2d_close (g2d_handle);

  return 0;
}

static gint
g2d_device_allocate_memory (gpointer device, PhyMemBlock *memblk)
{
  struct g2d_buf *pbuf = NULL;

  // g2d allocate momory is page alignment, so it is ok align size to page.
  // V4l2 capture will check physical memory size when registry buffer.
  memblk->size = PAGE_ALIGN(memblk->size);

  pbuf = g2d_alloc (memblk->size, 0);
  if (!pbuf) {
    GST_ERROR ("g2d_alloc failed.");
    return -1;
  }

  memblk->vaddr = (guint8*) pbuf->buf_vaddr;
  memblk->paddr = (guint8*) pbuf->buf_paddr;
  memblk->user_data = (gpointer) pbuf;

  return 0;
}

static gint
g2d_device_free_memory (gpointer device, PhyMemBlock *memblk)
{
  struct g2d_buf *pbuf = (struct g2d_buf*) memblk->user_data;

  g2d_free (pbuf);

  return 0;
}

static gint
g2d_device_copy (gpointer device, PhyMemBlock *dstblk, PhyMemBlock *srcblk)
{
  void *g2d_handle = NULL;
  struct g2d_buf src, dst;

  if(g2d_open(&g2d_handle) == -1 || g2d_handle == NULL) {
    GST_ERROR ("Failed to open g2d device.");
    return -1;
  }

  src.buf_handle = NULL;
  src.buf_vaddr = srcblk->vaddr;
  src.buf_paddr = srcblk->paddr;
  src.buf_size = srcblk->size;
  dst.buf_handle = NULL;
  dst.buf_vaddr = dstblk->vaddr;
  dst.buf_paddr = dstblk->paddr;
  dst.buf_size = dstblk->size;
  g2d_copy (g2d_handle, &dst, &src, dstblk->size);

  g2d_close (g2d_handle);
}

// global functions

gpointer compositor_device_open (DEVICE_TYPE type, gint fmt, gint width, gint height)
{
  gpointer ret;

  if (type == DEVICE_G2D) {
    ret = g2d_device_open (fmt, width, height);
  }

  return ret;
}

void comositor_device_close (gpointer device)
{
  return g2d_device_close (device);
}

gpointer compositor_device_create_surface (gpointer device, SurfaceInfo *surface_info)
{
  return g2d_device_create_surface (device, surface_info);
}

void compositor_device_destroy_surface (gpointer device, gpointer surface)
{
  return g2d_device_destroy_surface (device, surface);
}

gint compositor_device_update_surface_info (gpointer device, SurfaceInfo *surface_info, gpointer surface)
{
  return g2d_device_update_surface_info (device, surface_info, surface);
}

gint compositor_device_blit_surface (gpointer device, gpointer surface, SurfaceBuffer *buffer, SurfaceBuffer *dest)
{
  return g2d_device_blit_surface (device, surface, buffer, dest);
}

gint compositor_device_allocate_memory (gpointer device, PhyMemBlock *memblk)
{
  return g2d_device_allocate_memory (device, memblk);
}

gint compositor_device_free_memory (gpointer device, PhyMemBlock *memblk)
{
  return g2d_device_free_memory (device, memblk);
}

gint compositor_device_copy (gpointer device, PhyMemBlock *dstblk, PhyMemBlock *srcblk)
{
  return g2d_device_copy (device, dstblk, srcblk);
}
