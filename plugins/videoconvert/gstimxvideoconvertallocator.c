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

#include "gstimxvideoconvertallocator.h"
#include "videoprocessdevice.h"

GST_DEBUG_CATEGORY_EXTERN (imxvideoconvert_debug);
#define GST_CAT_DEFAULT imxvideoconvert_debug

static void
gst_imx_video_convert_allocator_class_init (
                                GstImxVideoConvertAllocatorClass * klass);
static void
gst_imx_video_convert_allocator_init (GstImxVideoConvertAllocator * allocator);

G_DEFINE_TYPE (GstImxVideoConvertAllocator, gst_imx_video_convert_allocator, \
               GST_TYPE_ALLOCATOR_PHYMEM);

static gint
imx_video_convert_allocate (GstAllocatorPhyMem *allocator, PhyMemBlock *memblk)
{
  GstImxVideoConvertAllocator *vct_allocator =
                            GST_IMX_VIDEO_CONVERT_ALLOCATOR(allocator);

  ImxVideoProcessDevice *dev = (ImxVideoProcessDevice*)(vct_allocator->device);
  if (dev) {
    if (dev->alloc_mem(dev, memblk) < 0)  {
      GST_ERROR ("imx video convert allocate memory failed.");
    } else {
      GST_LOG ("imx video convert allocated memory (%p), by (%p)",
                memblk->paddr, allocator);
      return 0;
    }
  }

  return -1;
}

static gint
imx_video_convert_free (GstAllocatorPhyMem *allocator, PhyMemBlock *memblk)
{
  GstImxVideoConvertAllocator *vct_allocator =
                              GST_IMX_VIDEO_CONVERT_ALLOCATOR(allocator);

  ImxVideoProcessDevice *dev = (ImxVideoProcessDevice*)(vct_allocator->device);
  if (dev) {
    GST_LOG ("imx video convert free memory (%p) of (%p)",
              memblk->paddr, allocator);
    if (dev->free_mem(dev, memblk) < 0)
      GST_ERROR ("imx video convert free memory failed.");
    else
      return 0;
  }

  return -1;
}

static void
gst_imx_video_convert_allocator_class_init (
                                GstImxVideoConvertAllocatorClass * klass)
{
  GstAllocatorPhyMemClass *parent_class;

  parent_class = (GstAllocatorPhyMemClass *) klass;

  parent_class->alloc_phymem = imx_video_convert_allocate;
  parent_class->free_phymem = imx_video_convert_free;
}

static void
gst_imx_video_convert_allocator_init (GstImxVideoConvertAllocator * allocator)
{
  return;
}

GstAllocator *gst_imx_video_convert_allocator_new (gpointer device)
{
  GstImxVideoConvertAllocator *allocator = NULL;

  allocator = g_object_new(gst_imx_video_convert_allocator_get_type(), NULL);
  if (!allocator) {
    GST_ERROR ("new imx video convert allocator failed.\n");
  } else {
    allocator->device = device;
    GST_DEBUG ("created imx video convert allocator(%p).", allocator);
  }

  return (GstAllocator*) allocator;
}



