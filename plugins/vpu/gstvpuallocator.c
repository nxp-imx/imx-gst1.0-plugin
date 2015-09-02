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
#include "gstvpuallocator.h"
#include "vpu_wrapper.h"

GST_DEBUG_CATEGORY_STATIC(vpu_allocator_debug);
#define GST_CAT_DEFAULT vpu_allocator_debug

static void gst_vpu_allocator_finalize(GObject *object);

static int gst_vpu_alloc_phys_mem(GstAllocatorPhyMem *allocator, PhyMemBlock *memory);
static int gst_vpu_free_phys_mem(GstAllocatorPhyMem *allocator, PhyMemBlock *memory);

G_DEFINE_TYPE(GstVpuAllocator, gst_vpu_allocator, GST_TYPE_ALLOCATOR_PHYMEM)

static void 
gst_vpu_mem_init(void)
{
	GstAllocator *allocator = g_object_new(gst_vpu_allocator_get_type(), NULL);
	gst_allocator_register(GST_VPU_ALLOCATOR_MEM_TYPE, allocator);
}

GstAllocator* 
gst_vpu_allocator_obtain(void)
{
	static GOnce dmabuf_allocator_once = G_ONCE_INIT;
	GstAllocator *allocator;

	g_once(&dmabuf_allocator_once, (GThreadFunc)gst_vpu_mem_init, NULL);

	allocator = gst_allocator_find(GST_VPU_ALLOCATOR_MEM_TYPE);
	if (allocator == NULL)
		GST_WARNING("No allocator named %s found", GST_VPU_ALLOCATOR_MEM_TYPE);

	return allocator;
}

static int
gst_vpu_alloc_phys_mem(G_GNUC_UNUSED GstAllocatorPhyMem *allocator, PhyMemBlock *memory)
{
	VpuDecRetCode ret;
	VpuMemDesc mem_desc;

	GST_DEBUG_OBJECT(allocator, "vpu allocator malloc size: %d\n", memory->size);
	memset(&mem_desc, 0, sizeof(VpuMemDesc));
  // VPU allocate momory is page alignment, so it is ok align size to page.
  // V4l2 capture will check physical memory size when registry buffer.
	mem_desc.nSize = PAGE_ALIGN(memory->size);
	ret = VPU_DecGetMem(&mem_desc);

	if (ret == VPU_DEC_RET_SUCCESS) {
		memory->size         = mem_desc.nSize;
		memory->paddr        = (guint8 *)(mem_desc.nPhyAddr);
		memory->vaddr         = (guint8 *)(mem_desc.nVirtAddr);
		memory->caddr         = (guint8 *)(mem_desc.nCpuAddr);
    GST_DEBUG_OBJECT(allocator, "vpu allocator malloc paddr: %x vaddr: %x\n", \
        memory->paddr, memory->vaddr);
    return 0;
	} else {
		return -1;
        }
}

static int
gst_vpu_free_phys_mem(G_GNUC_UNUSED GstAllocatorPhyMem *allocator, PhyMemBlock *memory)
{
  VpuDecRetCode ret;
  VpuMemDesc mem_desc;

	GST_DEBUG_OBJECT(allocator, "vpu allocator free size: %d\n", memory->size);
  memset(&mem_desc, 0, sizeof(VpuMemDesc));
	mem_desc.nSize     = memory->size;
	mem_desc.nPhyAddr  = (unsigned long)(memory->paddr);
	mem_desc.nVirtAddr  = (unsigned long)(memory->vaddr);
	mem_desc.nCpuAddr  = (unsigned long)(memory->caddr);

	ret = VPU_DecFreeMem(&mem_desc);

	return (ret == VPU_DEC_RET_SUCCESS) ? 0 : -1;
}

static int
gst_vpu_copy_phys_mem(G_GNUC_UNUSED GstAllocatorPhyMem *allocator, PhyMemBlock *dest_mem,
    PhyMemBlock *src_mem, guint offset, guint size)
{
  VpuDecRetCode ret;
  VpuMemDesc mem_desc;

  GST_DEBUG_OBJECT(allocator, "vpu allocator copy size: %d\n", src_mem->size);
  memset(&mem_desc, 0, sizeof(VpuMemDesc));

  if (size > src_mem->size - offset)
    size = src_mem->size - offset;

  mem_desc.nSize = PAGE_ALIGN(size);
  ret = VPU_DecGetMem(&mem_desc);

  if (ret == VPU_DEC_RET_SUCCESS) {
    dest_mem->size         = mem_desc.nSize;
    dest_mem->paddr        = (guint8 *)(mem_desc.nPhyAddr);
    dest_mem->vaddr         = (guint8 *)(mem_desc.nVirtAddr);
    dest_mem->caddr         = (guint8 *)(mem_desc.nCpuAddr);
    GST_DEBUG_OBJECT(allocator, "vpu allocator malloc paddr: %x vaddr: %x\n", \
        dest_mem->paddr, dest_mem->vaddr);

    memcpy(dest_mem->vaddr, src_mem->vaddr+offset, size);
    GST_WARNING("TODO: use relevant hardware to accelerate memory copying!");

    return 0;
  } else {
    return -1;
  }
}

static void 
gst_vpu_allocator_class_init(GstVpuAllocatorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GstAllocatorPhyMemClass *parent_class = GST_ALLOCATOR_PHYMEM_CLASS(klass);

	object_class->finalize       = GST_DEBUG_FUNCPTR(gst_vpu_allocator_finalize);
	parent_class->alloc_phymem = GST_DEBUG_FUNCPTR(gst_vpu_alloc_phys_mem);
	parent_class->free_phymem  = GST_DEBUG_FUNCPTR(gst_vpu_free_phys_mem);
	parent_class->copy_phymem = GST_DEBUG_FUNCPTR(gst_vpu_copy_phys_mem);

	GST_DEBUG_CATEGORY_INIT(vpu_allocator_debug, "vpuallocator", 0, "VPU physical memory allocator");
}

static void 
gst_vpu_allocator_init(GstVpuAllocator *allocator)
{
	GstAllocator *base = GST_ALLOCATOR(allocator);
	base->mem_type = GST_VPU_ALLOCATOR_MEM_TYPE;
}

static void 
gst_vpu_allocator_finalize(GObject *object)
{
	GST_DEBUG_OBJECT(object, "shutting down VPU allocator");
	G_OBJECT_CLASS(gst_vpu_allocator_parent_class)->finalize(object);
}

