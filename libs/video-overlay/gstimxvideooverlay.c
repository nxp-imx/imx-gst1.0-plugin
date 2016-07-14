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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gstimxvideooverlay.h"

#ifdef USE_X11
extern void gst_x_video_overlay_init(ImxVideoOverlay * imxxoverlay);
extern void gst_x_video_overlay_deinit(ImxVideoOverlay * imxxoverlay);
extern void gst_x_video_overlay_interface_init (GstVideoOverlayInterface * iface);
#endif

GST_DEBUG_CATEGORY_STATIC (imxvideooverlay_debug);
#define GST_CAT_DEFAULT imxvideooverlay_debug

void
gst_imx_video_overlay_interface_init (GstVideoOverlayInterface * iface)
{
  GST_DEBUG_CATEGORY_INIT (imxvideooverlay_debug, "imxvideooverlay", 0,
      "IMX General video overlay interface debugging");
#ifdef USE_X11
  gst_x_video_overlay_interface_init (iface);
#else
#endif
}

ImxVideoOverlay *
gst_imx_video_overlay_init(GstElement *element,
    VideoUpdateCallback update_video_geometry,
    ColorkeySetCallback set_color_key,
    AlphaSetCallback set_alpha)
{
  ImxVideoOverlay *overlay = NULL;

  if (!element) {
    GST_ERROR("NULL element parameter");
    return NULL;
  }

  overlay = g_new0(ImxVideoOverlay, 1);
  memset(overlay, 0, sizeof (ImxVideoOverlay));
  overlay->parent = element;

#ifdef USE_X11
  gst_x_video_overlay_init(overlay);
#else
#endif

  overlay->set_color_key = set_color_key;
  overlay->set_global_alpha = set_alpha;
  overlay->update_video_geo = update_video_geometry;

  if (!update_video_geometry)
    GST_ERROR("Parent and video geometry function not applied");

  if (!set_color_key)
    GST_ERROR("Color key setting callback NULL, video overlay may not work");

  if (!set_alpha)
    GST_ERROR("Alpha setting callback NULL, video overlay may not work");

  const gchar *colorkey = (gchar *)getenv ("COLORKEY");
  if (colorkey && (strlen(colorkey) > 1)) {
    overlay->colorkey = strtol(colorkey, NULL, 16);
  } else {
    overlay->colorkey = DEFAULT_COLORKEY;
    gchar str[10] = {0};
    sprintf(str, "%08x", overlay->colorkey);
    setenv("COLORKEY", str, TRUE);
    GST_INFO("set color key:%s\n", str);
  }

  return overlay;
}

void
gst_imx_video_overlay_finalize(ImxVideoOverlay * imxxoverlay)
{
  GST_DEBUG ("event_id %d", imxxoverlay->event_id);

  if (imxxoverlay) {
#ifdef USE_X11
    gst_x_video_overlay_deinit(imxxoverlay);
#else
#endif

    if (imxxoverlay->event_id)
      g_source_remove (imxxoverlay->event_id);

    imxxoverlay->set_color_key = NULL;
    imxxoverlay->set_global_alpha = NULL;
    imxxoverlay->update_video_geo = NULL;
    g_free(imxxoverlay);
  }
}

void
gst_imx_video_overlay_start (ImxVideoOverlay * imxxoverlay)
{
  GST_DEBUG ("START");

  if (imxxoverlay) {
    imxxoverlay->running = TRUE;

    if (imxxoverlay->update_win_geo && imxxoverlay->video_win) {
      if (imxxoverlay->set_global_alpha)
        imxxoverlay->set_global_alpha(imxxoverlay->parent, 255);

      if (imxxoverlay->set_color_key)
        imxxoverlay->set_color_key (imxxoverlay->parent, TRUE, imxxoverlay->colorkey);

      imxxoverlay->update_win_geo (imxxoverlay);
    }
  }
}

void
gst_imx_video_overlay_stop (ImxVideoOverlay * imxxoverlay)
{
  GST_DEBUG ("STOP");
  if (imxxoverlay) {
    imxxoverlay->running = FALSE;
  }
}

void
gst_imx_video_overlay_set_window_handle (ImxVideoOverlay *imxxoverlay,
                                         gulong id)
{
  GST_DEBUG ("winid %lu", id);
  if (!imxxoverlay || !imxxoverlay->parent ||
      !GST_IS_VIDEO_OVERLAY(imxxoverlay->parent)) {
    GST_ERROR ("Parent object is not video overlay");
    return;
  }

  if (imxxoverlay->video_win != id) {
    if (imxxoverlay->internal_win && id != imxxoverlay->internal_win) {
      if (imxxoverlay->destroy_win)
        imxxoverlay->destroy_win(imxxoverlay);
    }
    imxxoverlay->video_win = id;
    GST_DEBUG ("Setting XID to %lu", id);
  }

  if (id != 0) {
    if (imxxoverlay->set_global_alpha)
      imxxoverlay->set_global_alpha(imxxoverlay->parent, 255);

    if (imxxoverlay->set_color_key)
      imxxoverlay->set_color_key (imxxoverlay->parent, TRUE, imxxoverlay->colorkey);

    if (imxxoverlay->update_win_geo)
      imxxoverlay->update_win_geo (imxxoverlay);

    /* handle the X event always, we need events to update video geometry */
    if (imxxoverlay->handle_events)
      imxxoverlay->handle_events(imxxoverlay, TRUE);

    if (imxxoverlay->event_polling) {
      if (imxxoverlay->event_id)
        g_source_remove (imxxoverlay->event_id);

      imxxoverlay->event_id = g_timeout_add (EVENT_REFRESH_INTERVAL,
                                 imxxoverlay->event_polling, imxxoverlay);
      GST_DEBUG ("event_id %d", imxxoverlay->event_id);
    }
  } else {
    if (imxxoverlay->set_global_alpha)
      imxxoverlay->set_global_alpha(imxxoverlay->parent, 0);

    if (imxxoverlay->set_color_key)
      imxxoverlay->set_color_key (imxxoverlay->parent, FALSE, 0);
  }
}

/*
 * required: TRUE if display is required (ie. TRUE for v4l2sink, but
 *           FALSE for any other element with optional overlay capabilities)
 */
void
gst_imx_video_overlay_prepare_window_handle (ImxVideoOverlay * imxxoverlay,
    gboolean required)
{
  GST_DEBUG ("video-win %lu", imxxoverlay->video_win);
#ifdef ENABLE_PREPARE_WINDOW_INTERFACE
  if (!imxxoverlay || !imxxoverlay->parent ||
      !GST_IS_VIDEO_OVERLAY(imxxoverlay->parent))
    return;

  gst_video_overlay_prepare_window_handle(GST_VIDEO_OVERLAY(imxxoverlay->parent));

  if (required && !imxxoverlay->video_win && imxxoverlay->create_win) {
    /* video_overlay is supported, but we don't have a window.. so create one */
    imxxoverlay->internal_win = imxxoverlay->create_win(imxxoverlay);
    gst_imx_video_overlay_set_window_handle (imxxoverlay, imxxoverlay->internal_win);
  }
#endif
}

void
gst_imx_video_overlay_handle_events (ImxVideoOverlay * imxxoverlay,
                                     gboolean handle_events)
{
  GST_DEBUG ("handle events:%s", handle_events ? "TRUE" : "FALSE");

  if (!imxxoverlay || !imxxoverlay->video_win)
    return;

  if (imxxoverlay->handle_events)
    imxxoverlay->handle_events(imxxoverlay, handle_events);
}

void
gst_imx_video_overlay_expose (ImxVideoOverlay * imxxoverlay)
{
  GST_DEBUG ("EXPOSE");
  if (!imxxoverlay || !imxxoverlay->parent ||
      !GST_IS_VIDEO_OVERLAY(imxxoverlay->parent))
    return;

  if (imxxoverlay->video_win) {
    if (imxxoverlay->update_win_geo)
      imxxoverlay->update_win_geo (imxxoverlay);
  } else if (imxxoverlay->update_video_geo) {
    // no window applied
    imxxoverlay->update_video_geo(imxxoverlay->parent,imxxoverlay->render_rect);
  }
}

gboolean
gst_imx_video_overlay_set_render_rectangle (ImxVideoOverlay * imxxoverlay,
                                gint x, gint y, gint width, gint height) {
  GstVideoRectangle rect = {0};
  gint w, h;
  GST_DEBUG ("SET Render Rect : %d:%d:%d:%d", x, y, width, height);

  if (!imxxoverlay || !imxxoverlay->parent ||
      !GST_IS_VIDEO_OVERLAY(imxxoverlay->parent))
    return FALSE;

  imxxoverlay->render_rect.w = width;
  imxxoverlay->render_rect.h = height;
  imxxoverlay->render_rect.x = x;
  imxxoverlay->render_rect.y = y;

  return TRUE;
}
