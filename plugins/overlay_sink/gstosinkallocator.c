/*
 * Copyright (c) 2013-2015, Freescale Semiconductor, Inc. All rights reserved.
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

#include "gstosinkallocator.h"
#include "osink_object.h"

GST_DEBUG_CATEGORY_EXTERN (overlay_sink_debug);
#define GST_CAT_DEFAULT overlay_sink_debug

static int
osink_allocate_memory (GstAllocatorPhyMem *allocator, PhyMemBlock *memblk)
{
  GstAllocatorOsink *osink_allocator = GST_ALLOCATOR_OSINK(allocator);

  if (osink_object_allocate_memory (osink_allocator->hosink_obj, memblk) < 0) {
    GST_ERROR ("osink allocate memory failed.");
    return -1;
  }

  return 0;
}

static int
osink_free_memory (GstAllocatorPhyMem *allocator, PhyMemBlock *memblk)
{
  GstAllocatorOsink *osink_allocator = GST_ALLOCATOR_OSINK(allocator);

  if (osink_object_free_memory (osink_allocator->hosink_obj, memblk) < 0) {
    GST_ERROR ("osink free memory failed.");
    return -1;
  }

  return 0;
}

static int
osink_copy_memory (GstAllocatorPhyMem *allocator, PhyMemBlock *dst_mem,
    PhyMemBlock *src_mem, guint offset, guint size)
{
  GstAllocatorOsink *osink_allocator = GST_ALLOCATOR_OSINK(allocator);

  if (osink_object_copy_memory (osink_allocator->hosink_obj,
      dst_mem, src_mem, offset, size) < 0) {
    GST_ERROR ("osink copy memory failed.");
    return -1;
  }

  return 0;
}

G_DEFINE_TYPE (GstAllocatorOsink, gst_allocator_osink, GST_TYPE_ALLOCATOR_PHYMEM);

  static void
gst_allocator_osink_class_init (GstAllocatorOsinkClass * klass)
{
  GstAllocatorPhyMemClass *parent_class;

  parent_class = (GstAllocatorPhyMemClass *) klass;

  parent_class->alloc_phymem = osink_allocate_memory;
  parent_class->free_phymem = osink_free_memory;
  parent_class->copy_phymem = osink_copy_memory;
}

static void
gst_allocator_osink_init (GstAllocatorOsink * allocator)
{
  return;
}

// global function
GstAllocator *
gst_osink_allocator_new (gpointer osink_object)
{
  GstAllocatorOsink *allocator;

  allocator = g_object_new(gst_allocator_osink_get_type (), NULL);
  if (!allocator) {
    g_print ("new osink allocator failed.\n");
    return NULL;
  }

  allocator->hosink_obj = osink_object;

  GST_DEBUG ("Create osink allocator(%p).", allocator);

  return (GstAllocator*) allocator;
}
