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

#ifndef __GST_IMX_VIDEO_OVERLAY_H__
#define __GST_IMX_VIDEO_OVERLAY_H__

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>  /* for GstVideoRectange */
#include <gst/video/videooverlay.h>

#define RGB888(r,g,b) ((((guint32)(r))<<16)|(((guint32)(g))<<8)|(((guint32)(b))))
#define DEFAULT_COLORKEY RGB888(1, 2, 3)

/*
 * uncomment following definition to enable gst video overlay prepare window
 * interface, this interface will post a window prepare message to application,
 * a application can create a UI window and set the window id to video overlay
 * when it receive this message. if the application doesn't provide a window,
 * then the video overlay will create a internal window to show the video
 * within it. If this interface disabled, video overlay will do nothing if
 * application doesn't provide a window. and the video will show as if there is
 * no any graphic system.
 */
//#define ENABLE_PREPARE_WINDOW_INTERFACE

/*
 * The window message polling interval
 */
#define EVENT_REFRESH_INTERVAL  45    /* miliseconds */

typedef gboolean  (*VideoUpdateCallback) (GstElement *object, GstVideoRectangle win_rect);
typedef void (*ColorkeySetCallback) (GObject *object, gboolean enable, guint key);
typedef void (*AlphaSetCallback) (GObject *object, guint alpha);

typedef struct _ImxVideoOverlay {
  void *parent;           /* pointer to the object who use this interface*/
  gint colorkey;        /* The color key for FB (RGB888)*/
  gulong video_win;      /* ID of window where video display */
  gint event_id;          /* event refresh */
  gulong internal_win;    /* ID of window created internally */
  gboolean running;       /* is pipeline running */
  GstVideoRectangle render_rect; /* region for render video in video window */
  void *private;          /* pointer to concrete overlay structure */

  void (*update_win_geo)(struct _ImxVideoOverlay * imxxoverlay);
  gboolean (*event_polling)(gpointer data);
  gulong (*create_win)(struct _ImxVideoOverlay * imxxoverlay);
  void (*destroy_win)(struct _ImxVideoOverlay * imxxoverlay);
  void (*get_win_rect)(struct _ImxVideoOverlay * imxxoverlay, GstVideoRectangle * rect);
  void (*get_render_rect)(struct _ImxVideoOverlay * imxxoverlay, GstVideoRectangle * rect);
  void (*handle_events)(struct _ImxVideoOverlay * imxxoverlay, gboolean handle_events);

  // callback function
  VideoUpdateCallback update_video_geo; /* callback to update video geometry */
  ColorkeySetCallback set_color_key; /* callback for set the color key */
  AlphaSetCallback set_global_alpha; /* callback for set the global alpha */
} ImxVideoOverlay;

ImxVideoOverlay * gst_imx_video_overlay_init(GstElement *element,
                                    VideoUpdateCallback update_video_geometry,
                                    ColorkeySetCallback set_color_key,
                                    AlphaSetCallback set_alpha);

void gst_imx_video_overlay_finalize(ImxVideoOverlay * imxxoverlay);
void gst_imx_video_overlay_start (ImxVideoOverlay * imxxoverlay);
void gst_imx_video_overlay_stop (ImxVideoOverlay * imxxoverlay);
void gst_imx_video_overlay_prepare_window_handle (ImxVideoOverlay * imxxoverlay,
                                                  gboolean required);
void gst_imx_video_overlay_interface_init (GstVideoOverlayInterface * iface);
void gst_imx_video_overlay_set_window_handle (ImxVideoOverlay *imxxoverlay,
                                              gulong id);
void gst_imx_video_overlay_expose (ImxVideoOverlay * imxxoverlay);
void gst_imx_video_overlay_handle_events (ImxVideoOverlay * imxxoverlay,
                                          gboolean handle_events);
gboolean gst_imx_video_overlay_set_render_rectangle (
                                ImxVideoOverlay * imxxoverlay,
                                gint x, gint y, gint width, gint height);

#define GST_IMPLEMENT_VIDEO_OVERLAY_METHODS(Type, interface_as_function)      \
                                                                              \
static void                                                                   \
interface_as_function ## _video_overlay_set_window_handle (GstVideoOverlay * overlay,\
                                                           guintptr id)       \
{                                                                             \
  Type *this = (Type*) overlay;                                               \
  gst_imx_video_overlay_set_window_handle (this->imxoverlay, id);             \
}                                                                             \
                                                                              \
static void                                                                   \
interface_as_function ## _video_overlay_expose (GstVideoOverlay * overlay)    \
{                                                                             \
  Type *this = (Type*) overlay;                                               \
  gst_imx_video_overlay_expose (this->imxoverlay);                            \
}                                                                             \
                                                                              \
static void                                                                   \
interface_as_function ## _video_overlay_handle_events (GstVideoOverlay * overlay,\
                                                   gboolean handle_events)    \
{                                                                             \
  Type *this = (Type*) overlay;                                               \
  gst_imx_video_overlay_handle_events (this->imxoverlay, handle_events);        \
}                                                                             \
                                                                              \
static void                                                                   \
interface_as_function ## _video_overlay_set_render_rectangle (GstVideoOverlay * overlay,\
                                  gint x, gint y, gint width, gint height)    \
{                                                                             \
  Type *this = (Type*) overlay;                                               \
  gst_imx_video_overlay_set_render_rectangle (this->imxoverlay, x, y, width, height);\
}                                                                             \
                                                                              \
static void                                                                   \
interface_as_function ## _video_overlay_interface_init (GstVideoOverlayInterface * iface)\
{                                                                             \
  iface->set_window_handle = interface_as_function ## _video_overlay_set_window_handle;\
  iface->expose = interface_as_function ## _video_overlay_expose;             \
  iface->set_render_rectangle = interface_as_function ## _video_overlay_set_render_rectangle;\
  iface->handle_events = interface_as_function ## _video_overlay_handle_events;\
                                                                              \
  gst_imx_video_overlay_interface_init (iface);                               \
}                                                                             \

#endif /* __GST_IMX_VIDEO_OVERLAY_H__ */
