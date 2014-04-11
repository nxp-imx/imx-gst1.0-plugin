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

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "osink_common.h"
#include "gstallocatorphymem.h"

typedef enum {
  DEVICE_G2D,
  DEVICE_IPU,
  DEVICE_PXP,
  DEVICE_GLES2,
} DEVICE_TYPE;

gpointer compositor_device_open (DEVICE_TYPE type, gint fmt, gint width, gint height);
void comositor_device_close (gpointer device);
gpointer compositor_device_create_surface (gpointer device, SurfaceInfo *surface_info);
void compositor_device_destroy_surface (gpointer device, gpointer surface);
gint compositor_device_update_surface_info (gpointer device, SurfaceInfo *surface_info, gpointer surface);
gint compositor_device_blit_surface (gpointer device, gpointer surface, SurfaceBuffer *buffer, SurfaceBuffer *dest);
gint compositor_device_allocate_memory (gpointer device, PhyMemBlock *memblk);
gint compositor_device_free_memory (gpointer device, PhyMemBlock *memblk);
gint compositor_device_copy (gpointer device, PhyMemBlock *dstblk, PhyMemBlock *srcblk);

#endif
