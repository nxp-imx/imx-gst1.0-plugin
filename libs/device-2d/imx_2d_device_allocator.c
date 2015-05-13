/*
 * Copyright (c) 2014-2015, Freescale Semiconductor, Inc. All rights reserved.
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

#include "imx_2d_device_allocator.h"
#include "imx_2d_device.h"

GST_DEBUG_CATEGORY_EXTERN (imx2ddevice_debug);
#define GST_CAT_DEFAULT imx2ddevice_debug

static void
gst_imx_2d_device_allocator_class_init (GstImx2DDeviceAllocatorClass * klass);
static void
gst_imx_2d_device_allocator_init (GstImx2DDeviceAllocator * allocator);

G_DEFINE_TYPE (GstImx2DDeviceAllocator, gst_imx_2d_device_allocator, \
               GST_TYPE_ALLOCATOR_PHYMEM);

static gint
imx_2d_device_allocate (GstAllocatorPhyMem *allocator, PhyMemBlock *memblk)
{
  GstImx2DDeviceAllocator *_allocator = GST_IMX_2D_DEVICE_ALLOCATOR(allocator);

  Imx2DDevice *dev = (Imx2DDevice*)(_allocator->device);
  if (dev) {
    gint ret = dev->alloc_mem(dev, memblk);
    if (ret < 0)  {
      GST_ERROR ("imx 2d device allocate memory failed (%d).", ret);
    } else {
      GST_LOG ("imx 2d device allocated memory (%p), by (%p)",
                memblk->paddr, allocator);
      return 0;
    }
  }

  return -1;
}

static gint
imx_2d_device_free (GstAllocatorPhyMem *allocator, PhyMemBlock *memblk)
{
  GstImx2DDeviceAllocator *_allocator = GST_IMX_2D_DEVICE_ALLOCATOR(allocator);

  Imx2DDevice *dev = (Imx2DDevice*)(_allocator->device);
  if (dev) {
    GST_LOG ("imx 2d device free memory (%p) of (%p)",
              memblk->paddr, allocator);
    gint ret = dev->free_mem(dev, memblk);
    if (ret < 0)
      GST_ERROR ("imx 2d device free memory failed (%d).", ret);
    else
      return 0;
  }

  return -1;
}

static gint
imx_2d_device_copy (GstAllocatorPhyMem *allocator, PhyMemBlock *dst_mem,
                    PhyMemBlock *src_mem, guint offset, guint size)
{
  GstImx2DDeviceAllocator *_allocator = GST_IMX_2D_DEVICE_ALLOCATOR(allocator);

  Imx2DDevice *dev = (Imx2DDevice*)(_allocator->device);
  if (dev) {
    GST_LOG ("imx 2d device copy memory (%p)->(%p) of (%p)",
        src_mem->paddr, dst_mem->paddr, allocator);
    gint ret = dev->copy_mem(dev, dst_mem, src_mem, offset, size);
    if (ret < 0)
      GST_ERROR ("imx 2d device free memory failed (%d).", ret);
    else
      return 0;
  }

  return -1;
}

static void
gst_imx_2d_device_allocator_class_init (GstImx2DDeviceAllocatorClass * klass)
{
  GstAllocatorPhyMemClass *parent_class;

  parent_class = (GstAllocatorPhyMemClass *) klass;

  parent_class->alloc_phymem = imx_2d_device_allocate;
  parent_class->free_phymem = imx_2d_device_free;
  parent_class->copy_phymem = imx_2d_device_copy;
}

static void
gst_imx_2d_device_allocator_init (GstImx2DDeviceAllocator * allocator)
{
  return;
}

GstAllocator *gst_imx_2d_device_allocator_new (gpointer device)
{
  GstImx2DDeviceAllocator *allocator = NULL;

  allocator = g_object_new(gst_imx_2d_device_allocator_get_type(), NULL);
  if (!allocator) {
    GST_ERROR ("new imx 2d device allocator failed.\n");
  } else {
    allocator->device = device;
    GST_DEBUG ("created imx 2d device allocator(%p).", allocator);
  }

  return (GstAllocator*) allocator;
}



