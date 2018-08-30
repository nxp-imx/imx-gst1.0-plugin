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

#include "gstvpuenc.h"
#include <gst/allocators/gstphysmemory.h>
#include "gstimxcommon.h"

gint
gst_vpu_find_std (GstCaps * caps)
{
  VPUMapper *mapper = vpu_mappers;

  while (mapper->mime) {
    GstCaps *scaps = gst_caps_from_string (mapper->mime);
    if (scaps) {
      if (gst_caps_is_subset (caps, scaps)) {
        gst_caps_unref (scaps);
        return mapper->std;
      }
      gst_caps_unref (scaps);
    }
    mapper++;
  }

  return -1;
}

gboolean
gst_vpu_free_internal_mem (VpuInternalMem * vpu_internal_mem)
{
  g_list_foreach (vpu_internal_mem->internal_virt_mem, (GFunc) g_free, NULL);
  g_list_free (vpu_internal_mem->internal_virt_mem);
  vpu_internal_mem->internal_virt_mem = NULL;
  g_list_foreach (vpu_internal_mem->internal_phy_mem, (GFunc) gst_memory_unref, NULL);
  g_list_free (vpu_internal_mem->internal_phy_mem);
  vpu_internal_mem->internal_phy_mem = NULL;

  return TRUE;
}
 
gboolean
gst_vpu_allocate_internal_mem (VpuInternalMem * vpu_internal_mem)
{
  GstAllocationParams params;
  GstMemory * gst_memory;
  PhyMemBlock *memory;
	gint size;
  guint8 *ptr;
  gint i;

	memset(&params, 0, sizeof(GstAllocationParams));
	for (i = 0; i < vpu_internal_mem->mem_info.nSubBlockNum; ++i) {
		size = vpu_internal_mem->mem_info.MemSubBlock[i].nAlignment \
           + vpu_internal_mem->mem_info.MemSubBlock[i].nSize;
		GST_DEBUG_OBJECT(vpu_internal_mem, "sub block %d  type: %s  size: %d", i, \
        (vpu_internal_mem->mem_info.MemSubBlock[i].MemType == VPU_MEM_VIRT) ? \
        "virtual" : "phys", size);
 
		if (vpu_internal_mem->mem_info.MemSubBlock[i].MemType == VPU_MEM_VIRT) {
      ptr = g_malloc(size);
      if (ptr == NULL) {
        GST_ERROR ("Could not allocate memory");
        return FALSE;
      }

			vpu_internal_mem->mem_info.MemSubBlock[i].pVirtAddr = (unsigned char*)ALIGN( \
          ptr, vpu_internal_mem->mem_info.MemSubBlock[i].nAlignment);
      vpu_internal_mem->internal_virt_mem = g_list_append (vpu_internal_mem->internal_virt_mem, ptr);
		} else if (vpu_internal_mem->mem_info.MemSubBlock[i].MemType == VPU_MEM_PHY) {
      params.align = vpu_internal_mem->mem_info.MemSubBlock[i].nAlignment - 1;
      gst_memory = gst_allocator_alloc (gst_vpu_allocator_obtain(), size, &params);
      memory = gst_memory_query_phymem_block (gst_memory);
      if (memory == NULL) {
        GST_ERROR ("Could not allocate memory using VPU allocator");
        return FALSE;
      }

			vpu_internal_mem->mem_info.MemSubBlock[i].pVirtAddr = memory->vaddr;
			vpu_internal_mem->mem_info.MemSubBlock[i].pPhyAddr = memory->paddr;
      vpu_internal_mem->internal_phy_mem = g_list_append (vpu_internal_mem->internal_phy_mem, gst_memory);
		} else {
			GST_WARNING ("sub block %d type is unknown - skipping", i);
		}
 	}

  return TRUE;
}

gboolean
gst_vpu_register_frame_buffer (GList * gstbuffer_in_vpudec, \
    GstVideoInfo *info, VpuFrameBuffer * vpuframebuffers)
{
  VpuFrameBuffer *vpu_frame;
  GstVideoFrame frame;
  PhyMemBlock * mem_block;
  GstBuffer *buffer;
  guint i;

  for (i=0; i<g_list_length (gstbuffer_in_vpudec); i++) {
    buffer = g_list_nth_data (gstbuffer_in_vpudec, i);
    GST_DEBUG ("gstbuffer index: %d get from list: %x\n", \
        i, buffer);
    vpu_frame = &(vpuframebuffers[i]);

    if (IS_HANTRO()) {
      if (!gst_video_frame_map (&frame, info, buffer, GST_MAP_WRITE | GST_MAP_READ)) {
        GST_ERROR ("Could not map video buffer");
        return FALSE;
      }
    } else {
      if (!gst_video_frame_map (&frame, info, buffer, GST_MAP_READ)) {
        GST_ERROR ("Could not map video buffer");
        return FALSE;
      }
    }
    vpu_frame->nStrideY = GST_VIDEO_FRAME_COMP_STRIDE (&frame, 0);
    vpu_frame->nStrideC = GST_VIDEO_FRAME_COMP_STRIDE (&frame, 1);

    if (!(gst_buffer_is_phymem (buffer)
        || gst_is_phys_memory (gst_buffer_peek_memory (buffer, 0)))) {
      GST_ERROR ("isn't physical memory allocator");
      gst_video_frame_unmap (&frame);
      return FALSE;
    }

    if (gst_is_phys_memory (gst_buffer_peek_memory (buffer, 0))) {
      vpu_frame->pbufY = gst_phys_memory_get_phys_addr(gst_buffer_peek_memory (buffer, 0));
      GST_DEBUG ("video buffer phys add: %p", vpu_frame->pbufY);
    } else {
      mem_block = gst_buffer_query_phymem_block (buffer);
      vpu_frame->pbufY = mem_block->paddr;
      GST_DEBUG ("video buffer phys add: %p", vpu_frame->pbufY);
    }
    vpu_frame->pbufCb = vpu_frame->pbufY + \
      (GST_VIDEO_FRAME_COMP_DATA (&frame, 1) - GST_VIDEO_FRAME_COMP_DATA (&frame, 0));
    vpu_frame->pbufCr = vpu_frame->pbufCb + \
      (GST_VIDEO_FRAME_COMP_DATA (&frame, 2) - GST_VIDEO_FRAME_COMP_DATA (&frame, 1));

    vpu_frame->pbufVirtY = GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
    vpu_frame->pbufVirtCb = GST_VIDEO_FRAME_PLANE_DATA (&frame, 1);
    vpu_frame->pbufVirtCr = GST_VIDEO_FRAME_PLANE_DATA (&frame, 2);

    gst_video_frame_unmap (&frame);
  }

  return TRUE;
}

