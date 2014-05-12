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

#ifndef __COMPOSITOR_H__
#define __COMPOSITOR_H__

#include "osink_common.h"

typedef struct {
  gpointer context;
  int (*get_dst_buffer) (gpointer context, SurfaceBuffer *buffer);
  int (*flip_dst_buffer) (gpointer context, SurfaceBuffer *buffer);
} CompositorDstBufferCb;

gpointer create_compositor(gpointer device, CompositorDstBufferCb *pcallback);
void destroy_compositor(gpointer compositor);
gpointer compositor_add_surface (gpointer compositor, SurfaceInfo *surface_info);
gint compositor_remove_surface (gpointer compositor, gpointer surface);
gint compositor_config_surface (gpointer compositor, gpointer surface, SurfaceInfo *surface_info);
gboolean compositor_check_need_clear_display (gpointer compositor);
gint compositor_update_surface (gpointer compositor, gpointer surface, SurfaceBuffer *buffer);
gint64 compositor_get_surface_showed_frames (gpointer compositor, gpointer surface);

#endif
