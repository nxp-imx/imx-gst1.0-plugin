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

#ifndef __GST_IMX_X_OVERLAY_H__
#define __GST_IMX_X_OVERLAY_H__

#include <X11/X.h>
#include <X11/Xlib.h>
#include <gst/gst.h>

/*
 * the X window will get the expose event first, then the outside window
 * system (like GTK) will get expose signal, and it will flush the background
 * color of the window again, this may cause the filled color key be erased,
 * and the video will be covered by the background color. So, we defer the
 * expose processing to let the outside window system process first, then
 * update the X window. It is the best practice for the application to set
 * the video window background color as the color key color to avoid the
 * color flickering
 * Enable following definition to enable the deferring of update
 */
//#define DEFER_VIDEO_WINDOW_UPDATE

#ifdef DEFER_VIDEO_WINDOW_UPDATE
#define VIDEO_WIN_UPDATE_DEFER_TIME  100  //miliseconds
#endif

typedef struct _ImxXOverlay {
  Display *disp;          /* X display */
  GMutex mutex;
#ifdef DEFER_VIDEO_WINDOW_UPDATE
  guint defer_update_id;  /* g_timeout_add id of deferring update */
#endif
} ImxXOverlay;

#endif /* __GST_IMX_X_OVERLAY_H__ */
