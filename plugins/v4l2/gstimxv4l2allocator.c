/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. All rights reserved.
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

#include <string.h>
#include "gstimxv4l2allocator.h"
#include "gstimxv4l2.h"

static int
imx_v4l2_allocate_memory (GstAllocatorPhyMem *allocator, PhyMemBlock *memblk)
{
  GstAllocatorImxV4l2 *v4l2_allocator = GST_ALLOCATOR_IMXV4L2(allocator);
  IMXV4l2AllocatorContext *context = &v4l2_allocator->context;

  if (!v4l2_allocator->buffer_count) {
    if ((*context->callback) (context->user_data, &v4l2_allocator->buffer_count) < 0) {
      GST_ERROR ("do allocator callback failed.\n");
      return -1;
    }
  }

  GST_DEBUG ("allocate buffer index(%d), total count(%d).", v4l2_allocator->allocated, v4l2_allocator->buffer_count);

  if (v4l2_allocator->allocated < v4l2_allocator->buffer_count) {
    gst_imx_v4l2_allocate_buffer (context->v4l2_handle, memblk);
    v4l2_allocator->allocated ++;
  }
  else {
    GST_ERROR ("No more v4l2 buffer for allocating.\n");
  }

  return 0;
}

static int
imx_v4l2_free_memory (GstAllocatorPhyMem *allocator, PhyMemBlock *phy_mem)
{
  GstAllocatorImxV4l2 *v4l2_allocator = GST_ALLOCATOR_IMXV4L2(allocator);
  IMXV4l2AllocatorContext *context = &v4l2_allocator->context;

  gst_imx_v4l2_free_buffer (context->v4l2_handle, phy_mem);
  memset(phy_mem, 0, sizeof(PhyMemBlock));
  v4l2_allocator->allocated --;

  return 0;
}

G_DEFINE_TYPE (GstAllocatorImxV4l2, gst_allocator_imxv4l2, GST_TYPE_ALLOCATOR_PHYMEM);

  static void
gst_allocator_imxv4l2_class_init (GstAllocatorImxV4l2Class * klass)
{
  GstAllocatorPhyMemClass *parent_class;

  parent_class = (GstAllocatorPhyMemClass *) klass;

  parent_class->alloc_phymem = imx_v4l2_allocate_memory;
  parent_class->free_phymem = imx_v4l2_free_memory;
}

  static void
gst_allocator_imxv4l2_init (GstAllocatorImxV4l2 * allocator)
{
  memset (&allocator->context, 0, sizeof(IMXV4l2AllocatorContext));
  allocator->buffer_count = 0;
  allocator->allocated = 0;
}

// global function
  GstAllocator *
gst_imx_v4l2_allocator_new (IMXV4l2AllocatorContext *context)
{
  GstAllocatorImxV4l2 *allocator;

  allocator = g_object_new(gst_allocator_imxv4l2_get_type (), NULL);
  if (!allocator) {
    g_print ("new imxv4l2 allocator failed.\n");
    return NULL;
  }

  GST_DEBUG ("Create imx v4l2 allocator(%p).", allocator);

  memcpy (&allocator->context, context, sizeof(IMXV4l2AllocatorContext));

  return (GstAllocator*) allocator;
}


