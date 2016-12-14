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

#include <stdio.h>
#include <string.h>
/* for XkbKeycodeToKeysym */
#include <X11/XKBlib.h>
#include <gst/video/navigation.h>
#include "gstimxvideooverlay.h"
#include "gstimxxoverlay.h"

#define RGB888TORGB565(rgb)\
    ((((rgb)<<8)>>27<<11)|(((rgb)<<18)>>26<<5)|(((rgb)<<27)>>27))

GST_DEBUG_CATEGORY_STATIC (imxxoverlay_debug);
#define GST_CAT_DEFAULT imxxoverlay_debug

void
gst_x_video_overlay_interface_init (GstVideoOverlayInterface * iface)
{
  GST_DEBUG_CATEGORY_INIT (imxxoverlay_debug, "imxxoverlay", 0,
      "IMX X video overlay interface debugging");
}

/* This function get the relative coordinates of a window */
static void
gst_x_video_overlay_get_window_rect (ImxVideoOverlay * imxxoverlay,
    GstVideoRectangle * rect)
{
  ImxXOverlay *xoverlay = (ImxXOverlay *)imxxoverlay->private;
  if (xoverlay && xoverlay->disp && imxxoverlay->video_win) {
    XWindowAttributes attr;
    g_mutex_lock (&xoverlay->mutex);
    XGetWindowAttributes (xoverlay->disp, imxxoverlay->video_win, &attr);
    rect->x = 0;
    rect->y = 0;
    rect->w = attr.width;
    rect->h = attr.height;
    g_mutex_unlock (&xoverlay->mutex);
  }
}

/* This function should be called with mutex held */
static void
update_video_win (ImxVideoOverlay * imxxoverlay)
{
  ImxXOverlay *xoverlay = (ImxXOverlay *)imxxoverlay->private;
  if (!xoverlay || !xoverlay->disp || !imxxoverlay->video_win)
    return;

  Window win = imxxoverlay->video_win;
  Display *dpy = xoverlay->disp;

  /* We need the absolute coordination of the video render area to update the
   * video position, call the parent object provided callback to update the
   * new actual video geometry.
   */
  if (imxxoverlay->running) {
    GstVideoRectangle v_rect = {0};
    XWindowAttributes attr, root_attr;
    Window w;
    gint x = 0, y = 0;
    gint rw, rh;

    XGetWindowAttributes (dpy, win, &attr);
    GST_DEBUG ("%lu: %d:%d:%d:%d", win, attr.x, attr.y, attr.width, attr.height);

    XGetWindowAttributes (dpy, attr.root, &root_attr);
    XTranslateCoordinates (dpy, win, attr.root, 0, 0, &x, &y, &w);

    v_rect.x = x + imxxoverlay->render_rect.x;
    v_rect.y = y + imxxoverlay->render_rect.y;
    if (imxxoverlay->render_rect.w &&
        (imxxoverlay->render_rect.x + imxxoverlay->render_rect.w) < attr.width)
      v_rect.w = imxxoverlay->render_rect.w;
    else
      v_rect.w = attr.width - imxxoverlay->render_rect.x;

    if (imxxoverlay->render_rect.h &&
        (imxxoverlay->render_rect.y + imxxoverlay->render_rect.h) < attr.height)
      v_rect.h = imxxoverlay->render_rect.h;
    else
      v_rect.h = attr.height - imxxoverlay->render_rect.y;

    GST_DEBUG("x:y:w:h, %d:%d:%d:%d", v_rect.x, v_rect.y, v_rect.w, v_rect.h);

    if (v_rect.w <= 0 || v_rect.h <= 0) {
      GST_WARNING("Wrong window or video geometry!");
      return;
    }

    /* Set the color key and fill remain area with black if render area is
     * small than the video window. */
    GC gc = DefaultGC (dpy, DefaultScreen (dpy));
    XSetForeground (dpy, gc, BlackPixel(dpy, DefaultScreen (dpy)));
    XSetBackground (dpy, gc, BlackPixel(dpy, DefaultScreen (dpy)));
    if (imxxoverlay->render_rect.y)
      XFillRectangle (dpy, win, gc, 0, 0, attr.width, imxxoverlay->render_rect.y);
    if (imxxoverlay->render_rect.x)
      XFillRectangle (dpy, win, gc, 0, imxxoverlay->render_rect.y,
        imxxoverlay->render_rect.x, (attr.height - imxxoverlay->render_rect.y));
    rw = attr.width - imxxoverlay->render_rect.x - v_rect.w;
    rh = attr.height - imxxoverlay->render_rect.y - v_rect.h;

    if (rw)
      XFillRectangle (dpy, win, gc, imxxoverlay->render_rect.x + v_rect.w,
          imxxoverlay->render_rect.y, rw, v_rect.h);
    if (rh)
      XFillRectangle (dpy, win, gc, imxxoverlay->render_rect.x,
          imxxoverlay->render_rect.y + v_rect.h,
          attr.width - imxxoverlay->render_rect.x, rh);

    XSetForeground (dpy, gc, RGB888TORGB565(imxxoverlay->colorkey));
    //XClearWindow (dpy, win);
    XFillRectangle (dpy, win, gc, imxxoverlay->render_rect.x,
                    imxxoverlay->render_rect.y, v_rect.w, v_rect.h);
    XSync (dpy, FALSE);

    if (imxxoverlay->update_video_geo)
      imxxoverlay->update_video_geo(imxxoverlay->parent, v_rect);
  }
}

#ifdef DEFER_VIDEO_WINDOW_UPDATE
static gboolean
update_video_win_with_lock (gpointer data)
{
  ImxVideoOverlay * imxxoverlay = (ImxVideoOverlay *)(data);
  ImxXOverlay *xoverlay = (ImxXOverlay *)imxxoverlay->private;
  g_mutex_lock (&xoverlay->mutex);
  update_video_win(imxxoverlay);
  g_mutex_unlock (&xoverlay->mutex);

  /* ONCE */
  return FALSE;
}
#endif

static void
gst_x_video_overlay_update_video_win(ImxVideoOverlay * imxxoverlay)
{
  GST_DEBUG ("invoked");
  ImxXOverlay *xoverlay = (ImxXOverlay *)imxxoverlay->private;
  if (xoverlay) {
#ifdef DEFER_VIDEO_WINDOW_UPDATE
    if (xoverlay->defer_update_id)
      g_source_remove(xoverlay->defer_update_id);

    xoverlay->defer_update_id =
        g_timeout_add (VIDEO_WIN_UPDATE_DEFER_TIME, update_video_win_with_lock, imxxoverlay);
#else
    g_mutex_lock (&xoverlay->mutex);
    update_video_win(imxxoverlay);
    g_mutex_unlock (&xoverlay->mutex);
#endif
  }
}


static gboolean
gst_x_video_overlay_event_refresh (gpointer data)
{
  ImxVideoOverlay * xo = (ImxVideoOverlay *)(data);
  XEvent e;
  ImxXOverlay *xoverlay = (ImxXOverlay *)xo->private;

  if (!xoverlay)
    return FALSE;

  if (!xo->running)  //Not start yet
    return TRUE;

  GST_LOG ("event refresh\n");
  g_mutex_lock (&xoverlay->mutex);

  /* If the element supports navigation, collect the relevant input
   * events and push them upstream as navigation events */
  if (GST_IS_NAVIGATION (xo->parent)) {
    gboolean pointer_moved = FALSE;

    /* We get all pointer motion events, only the last position is interesting*/
    while (XCheckWindowEvent(xoverlay->disp, xo->video_win, PointerMotionMask,
        &e)) {
      if (MotionNotify == e.type)
        pointer_moved = TRUE;
    }

    if (pointer_moved) {
      GST_DEBUG("pointer moved over window at %d,%d", e.xmotion.x, e.xmotion.y);
      g_mutex_unlock (&xoverlay->mutex);
      gst_navigation_send_mouse_event (GST_NAVIGATION (xo->parent),
          "mouse-move", 0, e.xbutton.x, e.xbutton.y);
      g_mutex_lock (&xoverlay->mutex);
    }

    /* We get all events on our window to throw them upstream */
    while (XCheckWindowEvent (xoverlay->disp, xo->video_win,
      KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask, &e)){
      KeySym keysym;
      const char *key_str = NULL;

      g_mutex_unlock (&xoverlay->mutex);

      switch (e.type) {
      case ButtonPress:
        GST_DEBUG ("button %d pressed over window at %d,%d",
                    e.xbutton.button, e.xbutton.x, e.xbutton.y);
        gst_navigation_send_mouse_event (GST_NAVIGATION (xo->parent),
                "mouse-button-press", e.xbutton.button,
                e.xbutton.x, e.xbutton.y);
        break;
      case ButtonRelease:
        GST_DEBUG ("button %d released over window at %d,%d",
                    e.xbutton.button, e.xbutton.x, e.xbutton.y);
        gst_navigation_send_mouse_event (GST_NAVIGATION (xo->parent),
                "mouse-button-release", e.xbutton.button,
                e.xbutton.x, e.xbutton.y);
        break;
      case KeyPress:
      case KeyRelease:
        g_mutex_lock (&xoverlay->mutex);
        keysym = XkbKeycodeToKeysym (xoverlay->disp, e.xkey.keycode, 0, 0);
        if (keysym != NoSymbol) {
          key_str = XKeysymToString (keysym);
        } else {
          key_str = "unknown";
        }
        g_mutex_unlock (&xoverlay->mutex);
        GST_DEBUG ("key %d pressed over window at %d,%d (%s)",
                e.xkey.keycode, e.xkey.x, e.xkey.y, key_str);
        gst_navigation_send_key_event (GST_NAVIGATION (xo->parent),
                e.type == KeyPress ? "key-press" : "key-release", key_str);
        break;
      default:
        GST_DEBUG ("unhandled X event (%d)", e.type);
      }

      g_mutex_lock (&xoverlay->mutex);
    }
  }

  /* Handle ConfigureNotify */
  while (XCheckWindowEvent(xoverlay->disp, xo->video_win,
          ExposureMask |StructureNotifyMask, &e)) {
    switch (e.type) {
    case ConfigureNotify:
      GST_DEBUG ("ConfiureNotify event");
#ifdef DEFER_VIDEO_WINDOW_UPDATE
      if (xoverlay->defer_update_id)
        g_source_remove(xoverlay->defer_update_id);

      xoverlay->defer_update_id =
                g_timeout_add (VIDEO_WIN_UPDATE_DEFER_TIME, update_video_win_with_lock, xo);
#else
      update_video_win(xo);
#endif
      break;
    case Expose:
      if (e.xexpose.count > 0)
        break; //ignore repeated Expose event
      GST_DEBUG ("Expose event");
#ifdef DEFER_VIDEO_WINDOW_UPDATE
      if (xoverlay->defer_update_id)
        g_source_remove(xoverlay->defer_update_id);

      xoverlay->defer_update_id =
                g_timeout_add (VIDEO_WIN_UPDATE_DEFER_TIME, update_video_win_with_lock, xo);
#else
      update_video_win(xo);
#endif
      break;

    default:
      GST_DEBUG ("unknown X event (%d)", e.type);
    }
  }

  g_mutex_unlock (&xoverlay->mutex);

  /* repeat */
  return TRUE;
}

static gulong
gst_x_video_overlay_create_window (ImxVideoOverlay * imxxoverlay)
{
  GST_DEBUG ("creating window");
  Window win = 0;
#ifdef ENABLE_PREPARE_WINDOW_INTERFACE
  int width, height;
  ImxXOverlay *xoverlay = (ImxXOverlay *)imxxoverlay->private;

  if (!xoverlay)
    return 0;

  g_mutex_lock (&xoverlay->mutex);
  width = DisplayWidth(xoverlay->disp, DefaultScreen(xoverlay->disp));
  height = DisplayHeight(xoverlay->disp, DefaultScreen(xoverlay->disp));

  win = XCreateSimpleWindow (xoverlay->disp,
        DefaultRootWindow (xoverlay->disp), 0, 0, width, height, 0, 0,
        BlackPixel (xoverlay->disp, DefaultScreen (xoverlay->disp)));

  GST_DEBUG ("win=%lu", win);

  /* We have to do that to prevent X from redrawing the background on
   * ConfigureNotify. This takes away flickering of video when resizing. */
  XSetWindowBackgroundPixmap (xoverlay->disp, win, None);

  XMapRaised (xoverlay->disp, win);
  XSync (xoverlay->disp, FALSE);

  g_mutex_unlock (&xoverlay->mutex);
#endif
  return win;
}

static void
gst_x_video_overlay_destroy_window (ImxVideoOverlay * imxxoverlay)
{
  GST_DEBUG ("internal_win %lu\n", imxxoverlay->internal_win);
#ifdef ENABLE_PREPARE_WINDOW_INTERFACE
  ImxXOverlay *xoverlay = (ImxXOverlay *)imxxoverlay->private;

  if (xoverlay && imxxoverlay->internal_win) {
    XDestroyWindow (xoverlay->disp, imxxoverlay->internal_win);
    imxxoverlay->internal_win = 0;
  }
#endif
}

static void
gst_x_video_overlay_handle_events (ImxVideoOverlay * imxxoverlay,
                                    gboolean handle_events)
{
  ImxXOverlay *xoverlay = (ImxXOverlay *)imxxoverlay->private;

  if (xoverlay) {
    g_mutex_lock (&xoverlay->mutex);
    long event_mask = ExposureMask | StructureNotifyMask;
    if (GST_IS_NAVIGATION (imxxoverlay->parent)) {
      event_mask |= PointerMotionMask |
          KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;
    }

    if (handle_events) {
      XSelectInput (xoverlay->disp, imxxoverlay->video_win, event_mask);
    } else {
      XSelectInput (xoverlay->disp, imxxoverlay->video_win, NoEventMask);
    }
    g_mutex_unlock (&xoverlay->mutex);
  }
}

void
gst_x_video_overlay_init(ImxVideoOverlay * imxxoverlay)
{
  if (imxxoverlay) {
    imxxoverlay->private = g_new0(ImxXOverlay, 1);
    imxxoverlay->update_win_geo = gst_x_video_overlay_update_video_win;
    imxxoverlay->get_win_rect = gst_x_video_overlay_get_window_rect;
    imxxoverlay->create_win = gst_x_video_overlay_create_window;
    imxxoverlay->destroy_win = gst_x_video_overlay_destroy_window;
    imxxoverlay->handle_events = gst_x_video_overlay_handle_events;
    imxxoverlay->event_polling = gst_x_video_overlay_event_refresh;

    const gchar *name = g_getenv ("DISPLAY");
    Display *dpy;

    /* Open the default display */
    if (!name) {
      GST_WARNING ("No $DISPLAY set, open :0\n");
      dpy = XOpenDisplay (":0");
    } else {
      dpy = XOpenDisplay (name);
    }

    if (!dpy) {
      GST_ERROR ("failed to open X display - no overlay");
      return;
    }

    ImxXOverlay *xoverlay = (ImxXOverlay *)imxxoverlay->private;
    xoverlay->disp = dpy;
#ifdef DEFER_VIDEO_WINDOW_UPDATE
    xoverlay->defer_update_id = 0;
#endif
    g_mutex_init (&xoverlay->mutex);
    GST_DEBUG ("done");
  }
}

void
gst_x_video_overlay_deinit(ImxVideoOverlay * imxxoverlay)
{
  ImxXOverlay *xoverlay = (ImxXOverlay *)imxxoverlay->private;
  if (imxxoverlay->video_win)
    XSelectInput (xoverlay->disp, imxxoverlay->video_win, NoEventMask);
  gst_x_video_overlay_destroy_window(imxxoverlay);
  if (xoverlay->disp)
    XCloseDisplay (xoverlay->disp);
  xoverlay->disp = NULL;
  g_mutex_clear (&xoverlay->mutex);
  g_free(xoverlay);
  imxxoverlay->private = NULL;
  GST_DEBUG ("done");
}

