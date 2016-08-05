/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef __OSINK_OBJECT_H__
#define __OSINK_OBJECT_H__

#include "osink_common.h"
#include <gst/allocators/gstallocatorphymem.h>

#define IMX_2D_DEVICE_TYPE_USED     IMX_2D_DEVICE_G2D

gpointer osink_object_new ();
void osink_object_ref (gpointer osink_handle);
void osink_object_unref (gpointer osink_handle);
int osink_object_get_display_count (gpointer osink_handle);
int osink_object_get_display_info (gpointer osink_handle, DisplayInfo *info, gint display_idx);
int osink_object_enable_display (gpointer osink_handle, gint display_idx);
gpointer osink_object_create_overlay (gpointer osink_handle, guint display_idx, SurfaceInfo *surface_info);
void osink_object_destroy_overlay (gpointer ohandle, gpointer overlay);
int osink_object_config_overlay (gpointer ohandle, gpointer overlay, SurfaceInfo *surface_info);
int osink_object_update_overlay (gpointer ohandle, gpointer overlay, SurfaceBuffer *buffer);
gint64 osink_object_get_overlay_showed_frames (gpointer ohandle, gpointer overlay);
int osink_object_allocate_memory (gpointer handle, PhyMemBlock *memblk);
int osink_object_free_memory (gpointer handle, PhyMemBlock *memblk);
int osink_object_copy_memory (gpointer osink_handle, PhyMemBlock *dst_mem,
    PhyMemBlock *src_mem, guint offset, guint size);
void osink_object_set_global_alpha(gpointer ohandle, gint display_idx, gint alpha);
void osink_object_set_color_key(gpointer ohandle, gint display_idx, gboolean enable, guint colorkey);

#endif
