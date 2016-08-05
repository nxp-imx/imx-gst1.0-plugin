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

#ifndef __OSINK_ALLOCATOR_H__
#define __OSINK_ALLOCATOR_H__

#include <gst/allocators/gstallocatorphymem.h>
#include "osink_common.h"

#define GST_TYPE_ALLOCATOR_OSINK             (gst_allocator_osink_get_type())
#define GST_ALLOCATOR_OSINK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ALLOCATOR_OSINK, GstAllocatorOsink))
#define GST_ALLOCATOR_OSINK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ALLOCATOR_OSINK, GstAllocatorOsinkClass))
#define GST_IS_ALLOCATOR_OSINK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ALLOCATOR_OSINK))
#define GST_IS_ALLOCATOR_OSINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ALLOCATOR_OSINK))

typedef struct _GstAllocatorOsink GstAllocatorOsink;
typedef struct _GstAllocatorOsinkClass GstAllocatorOsinkClass;

typedef struct {
} OsinkAllocatorContext;

struct _GstAllocatorOsink {
  GstAllocatorPhyMem parent;
  gpointer hosink_obj;
};

struct _GstAllocatorOsinkClass {
  GstAllocatorPhyMemClass parent_class;
};

GType gst_allocator_osink_get_type (void);
GstAllocator *gst_osink_allocator_new (gpointer osink_object);

#endif
