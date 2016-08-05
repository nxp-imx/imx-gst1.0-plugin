/*
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef __OSINK_COMMON_H__
#define __OSINK_COMMON_H__

#include <gst/gst.h>
#include <gst/allocators/gstallocatorphymem.h>

#define MAX_DISPLAY (4)

typedef struct {
  gchar *name;
  gint fmt;
  guint width;
  guint height;
  guint stride;
} DisplayInfo;

typedef struct {
  gint left;
  gint top;
  gint right;
  gint bottom;
  gint width;
  gint height;
} SurfaceRect;

typedef struct {
  gint left;
  gint top;
  gint right;
  gint bottom;
} DestRect;

typedef struct {
  gint fmt;
  gint rot;
  gint alpha;
  gboolean keep_ratio;
  gint zorder;
  SurfaceRect src;
  DestRect dst;
} SurfaceInfo;

typedef struct {
  PhyMemBlock mem;
  int fd[4];
  GstBuffer *buf;
} SurfaceBuffer;

#endif
