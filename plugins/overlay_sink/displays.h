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

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "osink_common.h"

gint scan_displays (gpointer **phandle, gint *pcount);
void free_display (gpointer handle);
gchar *get_display_name(gpointer display);
gint get_display_format(gpointer display);
gint get_display_res (gpointer display, gint *width, gint *height, gint *stride);
gint init_display (gpointer display);
void deinit_display (gpointer display);
gint clear_display (gpointer display);
gint get_next_display_buffer (gpointer display, SurfaceBuffer *buffer);
gint flip_display_buffer (gpointer display, SurfaceBuffer *buffer);
void set_global_alpha(gpointer display, gint alpha);
void set_color_key(gpointer display, gboolean enable, guint colorkey);

#endif
