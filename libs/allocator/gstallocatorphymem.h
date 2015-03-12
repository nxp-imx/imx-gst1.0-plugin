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

#ifndef __ALLOCATOR_PHYMEM_H__
#define __ALLOCATOR_PHYMEM_H__

#include <gst/gst.h>
#include <gst/gstallocator.h>

#define PAGE_ALIGN(x) (((x) + 4095) & ~4095)

#define GST_TYPE_ALLOCATOR_PHYMEM             (gst_allocator_phymem_get_type())
#define GST_ALLOCATOR_PHYMEM(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ALLOCATOR_PHYMEM, GstAllocatorPhyMem))
#define GST_ALLOCATOR_PHYMEM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ALLOCATOR_PHYMEM, GstAllocatorPhyMemClass))
#define GST_IS_ALLOCATOR_PHYMEM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ALLOCATOR_PHYMEM))
#define GST_IS_ALLOCATOR_PHYMEM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ALLOCATOR_PHYMEM))

typedef struct _GstAllocatorPhyMem GstAllocatorPhyMem;
typedef struct _GstAllocatorPhyMemClass GstAllocatorPhyMemClass;

/* also change gst-libs/gst/gl/gstglvivdirecttexture.c in gst-plugins-bad git
 * if changed below structure */
typedef struct {
  guint8 *vaddr;
  guint8 *paddr;
  guint8 *caddr;
  gsize size;
  gpointer *user_data;
} PhyMemBlock;

struct _GstAllocatorPhyMem {
  GstAllocator parent;
};

struct _GstAllocatorPhyMemClass {
  GstAllocatorClass parent_class;
  int (*alloc_phymem) (GstAllocatorPhyMem *allocator, PhyMemBlock *phy_mem);
  int (*free_phymem) (GstAllocatorPhyMem *allocator, PhyMemBlock *phy_mem);
  int (*copy_phymem) (GstAllocatorPhyMem *allocator, PhyMemBlock *det_mem,
                      PhyMemBlock *src_mem, guint offset, guint size);
};

GType gst_allocator_phymem_get_type (void);
gboolean gst_buffer_is_phymem (GstBuffer *buffer);
PhyMemBlock *gst_buffer_query_phymem_block (GstBuffer *buffer);
PhyMemBlock *gst_memory_query_phymem_block (GstMemory *mem);

#endif
