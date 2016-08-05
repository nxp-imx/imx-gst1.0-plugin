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

#ifndef __IMX_V4L2_ALLOCATOR_H__
#define __IMX_V4L2_ALLOCATOR_H__

#include <gst/allocators/gstallocatorphymem.h>

#define GST_TYPE_ALLOCATOR_IMXV4L2             (gst_allocator_imxv4l2_get_type())
#define GST_ALLOCATOR_IMXV4L2(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ALLOCATOR_IMXV4L2, GstAllocatorImxV4l2))
#define GST_ALLOCATOR_IMXV4L2_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ALLOCATOR_IMXV4L2, GstAllocatorImxV4l2Class))
#define GST_IS_ALLOCATOR_IMXV4L2(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ALLOCATOR_IMXV4L2))
#define GST_IS_ALLOCATOR_IMXV4L2_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ALLOCATOR_IMXV4L2))

typedef struct _GstAllocatorImxV4l2 GstAllocatorImxV4l2;
typedef struct _GstAllocatorImxV4l2Class GstAllocatorImxV4l2Class;

typedef gint (*IMXV4l2AllocatorCb) (gpointer user_data, gint *buffer_count);

typedef struct {
  gpointer v4l2_handle;
  gpointer user_data;
  IMXV4l2AllocatorCb callback;
} IMXV4l2AllocatorContext;

struct _GstAllocatorImxV4l2 {
  GstAllocatorPhyMem parent;
  IMXV4l2AllocatorContext context;
  gint buffer_count;
  gint allocated;
};

struct _GstAllocatorImxV4l2Class {
  GstAllocatorPhyMemClass parent_class;
};

GType gst_allocator_imxv4l2_get_type (void);
GstAllocator *gst_imx_v4l2_allocator_new (IMXV4l2AllocatorContext *context);

#endif
