/*
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2018 NXP
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/video/gstvideopool.h>
#include <gst/allocators/gstdmabuf.h>
#include "gstosink.h"
#include "osink_object.h"
#include "gstosinkallocator.h"
#ifdef USE_ION
#include <gst/allocators/gstionmemory.h>
#endif
#include <gst/allocators/gstphymemmeta.h>
#include "gstimxvideooverlay.h"
#include "imxoverlaycompositionmeta.h"

#define ALIGNMENT (16)
#define ISALIGNED(a, b) (!(a & (b-1)))
#define ALIGNTO(a, b) ((a + (b-1)) & (~(b-1)))

GST_DEBUG_CATEGORY (overlay_sink_debug);
#define GST_CAT_DEFAULT overlay_sink_debug

enum
{
  OVERLAY_SINK_PROP_0,
  OVERLAY_SINK_PROP_COMPOSITION_META_ENABLE,
  OVERLAY_SINK_PROP_VIDEO_DIRECTION,
  OVERLAY_SINK_PROP_DISP_ON_0,
  OVERLAY_SINK_PROP_DISPWIN_X_0,
  OVERLAY_SINK_PROP_DISPWIN_Y_0,
  OVERLAY_SINK_PROP_DISPWIN_W_0,
  OVERLAY_SINK_PROP_DISPWIN_H_0,
  OVERLAY_SINK_PROP_ROTATION_0,
  OVERLAY_SINK_PROP_DISP_UPDATE_0,
  OVERLAY_SINK_PROP_KEEP_VIDEO_RATIO_0,
  OVERLAY_SINK_PROP_ZORDER_0,
  OVERLAY_SINK_PROP_DISP_MAX_0
};

#define OVERLAY_SINK_PROP_DISP_LENGTH (OVERLAY_SINK_PROP_DISP_MAX_0-OVERLAY_SINK_PROP_DISP_ON_0)
#define OVERLAY_SINK_COMPOMETA_DEFAULT     TRUE

#define DEFAULT_IMX_ROTATE_METHOD GST_IMX_ROTATION_0
#define GST_TYPE_IMX_ROTATE_METHOD (gst_imx_rotate_method_get_type())

static const GEnumValue rotate_methods[] = {
  {GST_IMX_ROTATION_0, "no rotation", "none"},
  {GST_IMX_ROTATION_90, "Rotate clockwise 90 degrees", "rotate-90"},
  {GST_IMX_ROTATION_180, "Rotate clockwise 180 degrees", "rotate-180"},
  {GST_IMX_ROTATION_270, "Rotate clockwise 270 degrees", "rotate-270"},
  {GST_IMX_ROTATION_HFLIP, "Flip horizontally", "horizontal-flip"},
  {GST_IMX_ROTATION_VFLIP, "Flip vertically", "vertically-flip"},
  {0, NULL, NULL}
};

GType
gst_imx_rotate_method_get_type()
{
  static GType rotate_method_type = 0;
  static volatile gsize once = 0;

  if (g_once_init_enter (&once)) {
    rotate_method_type = g_enum_register_static ("GstImxRotateMethod",
        rotate_methods);
    g_once_init_leave (&once, rotate_method_type);
  }

  return rotate_method_type;
}


static GstFlowReturn
gst_overlay_sink_show_frame (GstBaseSink * bsink, GstBuffer * buffer);

GST_IMPLEMENT_VIDEO_OVERLAY_METHODS (GstOverlaySink, gst_overlay_sink);

static gboolean overlay_sink_update_video_geo(GstElement * object, GstVideoRectangle win_rect) {
  GstOverlaySink *osink = GST_OVERLAY_SINK (object);
  if (osink->overlay[0].x == win_rect.x && osink->overlay[0].y == win_rect.y &&
      osink->overlay[0].w == win_rect.w && osink->overlay[0].h == win_rect.h)
    return TRUE;

  osink->overlay[0].x = win_rect.x;
  osink->overlay[0].y = win_rect.y;
  osink->overlay[0].w = win_rect.w;
  osink->overlay[0].h = win_rect.h;

  osink->config[0] = TRUE;
  if (((GstBaseSink*)osink)->eos || GST_STATE(object) == GST_STATE_PAUSED) {
    gst_overlay_sink_show_frame((GstBaseSink *)osink, osink->prv_buffer);
  }

  return TRUE;
}

static void overlay_sink_config_global_alpha(GObject * object, guint alpha)
{
  GstOverlaySink *osink = GST_OVERLAY_SINK (object);
  if (osink && osink->osink_obj)
    osink_object_set_global_alpha(osink->osink_obj, 0, alpha);
}

static void overlay_sink_config_color_key(GObject * object, gboolean enable, guint color_key)
{
  GstOverlaySink *osink = GST_OVERLAY_SINK (object);
  if (osink && osink->osink_obj)
    osink_object_set_color_key(osink->osink_obj, 0, enable, color_key);
}

static void
gst_overlay_sink_video_direction_interface_init (GstVideoDirectionInterface *
    iface)
{
  /* We implement the video-direction property */
}

#define gst_overlay_sink_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE (GstOverlaySink, gst_overlay_sink, GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_overlay_sink_video_overlay_interface_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_DIRECTION,
        gst_overlay_sink_video_direction_interface_init));

//G_DEFINE_TYPE (GstOverlaySink, gst_overlay_sink, GST_TYPE_VIDEO_SINK);

static gint
gst_overlay_sink_output_config (GstOverlaySink *sink, gint idx) 
{
  if (sink->overlay[idx].rot != 0) {
    if (sink->overlay[idx].x < 0 || sink->overlay[idx].y < 0
        || (sink->overlay[idx].x + sink->overlay[idx].w) > sink->disp_info[idx].width
        || (sink->overlay[idx].y + sink->overlay[idx].h) > sink->disp_info[idx].height) {
      g_print ("not support video out of screen if orientation is not landscape.\n");
      memcpy(&sink->overlay[idx], &sink->pre_overlay_info[idx], sizeof(OverlayInfo));
      return -1;
    }
  }

  sink->surface_info.dst.left = sink->overlay[idx].x;
  sink->surface_info.dst.top = sink->overlay[idx].y;
  sink->surface_info.dst.right = sink->overlay[idx].w + sink->surface_info.dst.left;
  sink->surface_info.dst.bottom = sink->overlay[idx].h + sink->surface_info.dst.top;
  sink->surface_info.rot = sink->overlay[idx].rot;
  sink->surface_info.keep_ratio = sink->overlay[idx].keep_ratio;
  sink->surface_info.zorder = sink->overlay[idx].zorder;

  memcpy(&sink->pre_overlay_info[idx], &sink->overlay[idx], sizeof(OverlayInfo));
  return 0;
}

static void
gst_overlay_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOverlaySink *sink = GST_OVERLAY_SINK (object);
  guint idx, prop; 
  gint val;

  GST_DEBUG_OBJECT (sink, "set_property (%d).", prop_id);

  if (prop_id == OVERLAY_SINK_PROP_COMPOSITION_META_ENABLE) {
    sink->composition_meta_enable = g_value_get_boolean(value);
    return;
  }
  if (prop_id == OVERLAY_SINK_PROP_VIDEO_DIRECTION) {
    sink->overlay[0].rot = g_value_get_enum (value);
    return;
  }
  idx = (prop_id - OVERLAY_SINK_PROP_DISP_ON_0) / OVERLAY_SINK_PROP_DISP_LENGTH;
  prop = prop_id - idx * OVERLAY_SINK_PROP_DISP_LENGTH;
  switch (prop) {
    case OVERLAY_SINK_PROP_DISP_ON_0:
      sink->disp_on[idx] = g_value_get_boolean (value);
      break;
    case OVERLAY_SINK_PROP_DISPWIN_X_0:
      sink->overlay[idx].x = g_value_get_int (value);
      break;
    case OVERLAY_SINK_PROP_DISPWIN_Y_0:
      sink->overlay[idx].y = g_value_get_int (value);
      break;
    case OVERLAY_SINK_PROP_DISPWIN_W_0:
      sink->overlay[idx].w = g_value_get_int (value);
      break;
    case OVERLAY_SINK_PROP_DISPWIN_H_0:
      sink->overlay[idx].h = g_value_get_int (value);
      break;
    case OVERLAY_SINK_PROP_ROTATION_0:
      sink->overlay[idx].rot = g_value_get_enum (value);
      break;
    case OVERLAY_SINK_PROP_DISP_UPDATE_0:
      sink->config[idx] = g_value_get_boolean (value);
      if (sink->config[idx] &&
          (((GstBaseSink*)sink)->eos || GST_STATE(sink) == GST_STATE_PAUSED)) {
        gst_overlay_sink_show_frame((GstBaseSink *)sink, sink->prv_buffer);
        sink->config[idx] = FALSE;
      }
      break;
    case OVERLAY_SINK_PROP_KEEP_VIDEO_RATIO_0:
      sink->overlay[idx].keep_ratio = g_value_get_boolean (value);
      break;
    case OVERLAY_SINK_PROP_ZORDER_0:
      sink->overlay[idx].zorder = g_value_get_int (value);
      break;
    default:
      break;
  }

  return;
}

static void
gst_overlay_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOverlaySink *sink = GST_OVERLAY_SINK (object);
  guint idx, prop; 

  GST_DEBUG_OBJECT (sink, "get_property (%d).", prop_id);

  if (prop_id == OVERLAY_SINK_PROP_COMPOSITION_META_ENABLE) {
    g_value_set_boolean(value, sink->composition_meta_enable);
    return;
  }
  if (prop_id == OVERLAY_SINK_PROP_VIDEO_DIRECTION) {
     g_value_set_enum (value, sink->overlay[0].rot);
     return;
  }
  idx = (prop_id - OVERLAY_SINK_PROP_DISP_ON_0) / OVERLAY_SINK_PROP_DISP_LENGTH;
  prop = prop_id - idx * OVERLAY_SINK_PROP_DISP_LENGTH;
  switch (prop) {
    case OVERLAY_SINK_PROP_DISP_ON_0:
      g_value_set_boolean (value, sink->disp_on[idx]);
      break;
    case OVERLAY_SINK_PROP_DISPWIN_X_0:
      g_value_set_int (value, sink->overlay[idx].x);
      break;
    case OVERLAY_SINK_PROP_DISPWIN_Y_0:
      g_value_set_int (value, sink->overlay[idx].y);
      break;
    case OVERLAY_SINK_PROP_DISPWIN_W_0:
      g_value_set_int (value, sink->overlay[idx].w);
      break;
    case OVERLAY_SINK_PROP_DISPWIN_H_0:
      g_value_set_int (value, sink->overlay[idx].h);
      break;
    case OVERLAY_SINK_PROP_ROTATION_0:
      g_value_set_enum (value, sink->overlay[idx].rot);
      break;
    case OVERLAY_SINK_PROP_DISP_UPDATE_0:
      g_value_set_boolean (value, sink->config[idx]);
      break;
    case OVERLAY_SINK_PROP_KEEP_VIDEO_RATIO_0:
      g_value_set_boolean (value, sink->overlay[idx].keep_ratio);
      break;
    case OVERLAY_SINK_PROP_ZORDER_0:
      g_value_set_int (value, sink->overlay[idx].zorder);
      break;
    default:
      break;
  }

  return;
}

static GstStateChangeReturn
gst_overlay_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstOverlaySink *sink = GST_OVERLAY_SINK (element);

  GST_DEBUG_OBJECT (sink, "%d -> %d",
      GST_STATE_TRANSITION_CURRENT (transition),
      GST_STATE_TRANSITION_NEXT (transition));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        sink->osink_obj = osink_object_new ();
        if (!sink->osink_obj) {
          GST_ERROR_OBJECT (sink, "create osink object failed.");
          return GST_STATE_CHANGE_FAILURE;

        }
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        {
          gint i;
          gboolean display_enabled = FALSE;

          //put enable display in READY->PAUSE as playbin will change videosink
          //to ready state even for pure audio playback, this will cause overlaysink
          //show black screen in enable_display call
          sink->disp_count = osink_object_get_display_count (sink->osink_obj);
          for (i=0; i<sink->disp_count; i++) {
            osink_object_get_display_info (sink->osink_obj, &sink->disp_info[i], i);
            if (sink->disp_on[i]) {
              if (osink_object_enable_display (sink->osink_obj, i) < 0) {
                GST_ERROR_OBJECT (sink, "enable display %s failed.", sink->disp_info[i].name);
                sink->disp_on[i] = FALSE;
                continue;
              }

              display_enabled = TRUE;

              if (sink->overlay[i].w == 0) {
                if (sink->overlay[i].x > 0)
                  sink->overlay[i].w = sink->disp_info[i].width - sink->overlay[i].x;
                else
                  sink->overlay[i].w = sink->disp_info[i].width;
              }

              if (sink->overlay[i].h == 0) {
                if (sink->overlay[i].y > 0)
                  sink->overlay[i].h = sink->disp_info[i].height - sink->overlay[i].h;
                else
                  sink->overlay[i].h = sink->disp_info[i].height;
              }
            }
          }

          if (!display_enabled) {
            GST_ERROR_OBJECT (sink, "No display enabled.");
            osink_object_unref (sink->osink_obj);
            sink->osink_obj = NULL;
            return GST_STATE_CHANGE_FAILURE;
          }

          sink->frame_showed = 0;
          sink->run_time = 0;

          gst_imx_video_overlay_start (sink->imxoverlay);
        }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      sink->run_time = gst_element_get_start_time (GST_ELEMENT (sink));
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      {
        gint i;

        gst_imx_video_overlay_stop (sink->imxoverlay);

        if (sink->prv_buffer) {
          gst_buffer_unref (sink->prv_buffer);
          sink->prv_buffer = NULL;
        }

        for (i=0; i<sink->disp_count; i++) {
          if (sink->hoverlay[i]) {
            if (sink->run_time > 0) {
              gint64 blited = osink_object_get_overlay_showed_frames (sink->osink_obj, sink->hoverlay[i]);
              g_print ("Total showed frames (%lld), display %s blited (%lld), playing for (%"GST_TIME_FORMAT"), fps (%.3f).\n",
                  sink->frame_showed, sink->disp_info[i].name, blited, GST_TIME_ARGS (sink->run_time),
                  (gfloat)GST_SECOND * blited / sink->run_time);
            }
            osink_object_destroy_overlay (sink->osink_obj, sink->hoverlay[i]);
            sink->hoverlay[i] = NULL;
          }
        }

        if (sink->pool) {
          // only deactivate pool if pool activated by own, up stream element
          // may still using it
          if (sink->pool_activated) {
            gst_buffer_pool_set_active (sink->pool, FALSE);
            sink->pool_activated = FALSE;
          }
          gst_object_unref (sink->pool);
          sink->pool = NULL;
        }

        if (sink->allocator) {
          gst_object_unref (sink->allocator);
          sink->allocator = NULL;
        }
      }
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      {
        if (sink->osink_obj)
          osink_object_unref (sink->osink_obj);
        sink->osink_obj = NULL;
        sink->frame_showed = 0;
        sink->run_time = 0;
      }
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_overlay_sink_buffer_pool_is_ok (GstBufferPool * pool, GstCaps * newcaps,
    gint size)
{
  GstCaps *oldcaps;
  GstStructure *config;
  guint bsize;
  gboolean ret;

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, &oldcaps, &bsize, NULL, NULL);
  ret = (size <= bsize) && gst_caps_is_equal (newcaps, oldcaps);
  gst_structure_free (config);

  return ret;
}

static gint
gst_overlay_sink_setup_buffer_pool (GstOverlaySink *sink, GstCaps *caps)
{
  GstStructure *structure;
  GstVideoInfo info;

  if (sink->pool) {
    GST_DEBUG_OBJECT (sink, "already have a pool (%p).", sink->pool);
    return 0;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (sink, "invalid caps.");
    return -1;
  }

  sink->pool = gst_video_buffer_pool_new ();
  if (!sink->pool) {
    GST_ERROR_OBJECT (sink, "New video buffer pool failed.\n");
    return -1;
  }
  GST_DEBUG_OBJECT (sink, "create buffer pool(%p).", sink->pool);

  if (!sink->allocator) {
#ifdef USE_ION
    sink->allocator = gst_ion_allocator_obtain ();
#endif
    if (!sink->allocator) {
      sink->allocator = gst_osink_allocator_new (sink->osink_obj);
    }
    if (!sink->allocator) {
      GST_ERROR_OBJECT (sink, "New osink allocator failed.\n");
      return -1;
    }
    GST_DEBUG_OBJECT (sink, "create allocator(%p).", sink->allocator);
  }

  structure = gst_buffer_pool_get_config (sink->pool);

  // buffer alignment configuration
  gint w = GST_VIDEO_INFO_WIDTH (&info);
  gint h = GST_VIDEO_INFO_HEIGHT (&info);
  if (!ISALIGNED (w, ALIGNMENT) || !ISALIGNED (h, ALIGNMENT)) {
    GstVideoAlignment alignment;

    memset (&alignment, 0, sizeof (GstVideoAlignment));
    alignment.padding_right = ALIGNTO (w, ALIGNMENT) - w;
    alignment.padding_bottom = ALIGNTO (h, ALIGNMENT) - h;

    GST_DEBUG ("align buffer pool, w(%d) h(%d), padding_right (%d), padding_bottom (%d)",
        w, h, alignment.padding_right, alignment.padding_bottom);

    gst_buffer_pool_config_add_option (structure, GST_BUFFER_POOL_OPTION_VIDEO_META);
    gst_buffer_pool_config_add_option (structure, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    gst_buffer_pool_config_set_video_alignment (structure, &alignment);
  }

  gst_buffer_pool_config_set_params (structure, caps, info.size, sink->min_buffers, sink->max_buffers);
  gst_buffer_pool_config_set_allocator (structure, sink->allocator, NULL);
  if (!gst_buffer_pool_set_config (sink->pool, structure)) {
    GST_ERROR_OBJECT (sink, "set buffer pool failed.\n");
    return -1;
  }

  return 0;
}

static gboolean
gst_overlay_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstOverlaySink *sink = GST_OVERLAY_SINK (bsink);
  GstVideoInfo info;
  gint i, w, h;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (sink, "invalid caps.");
    return FALSE;
  }

  GST_DEBUG_OBJECT (sink, "set caps %" GST_PTR_FORMAT, caps);

  w = GST_VIDEO_INFO_WIDTH (&info);
  h = GST_VIDEO_INFO_HEIGHT (&info);

  sink->w = w;
  sink->h = h;

  sink->cropmeta.x = 0;
  sink->cropmeta.y = 0;
  sink->cropmeta.width = w;
  sink->cropmeta.height = h;

  sink->surface_info.fmt = GST_VIDEO_INFO_FORMAT (&info);
  sink->surface_info.src.left = 0;
  sink->surface_info.src.top = 0;
  sink->surface_info.src.right = w;
  sink->surface_info.src.bottom = h;
  sink->surface_info.src.width = w;
  sink->surface_info.src.height = h;
  sink->surface_info.alpha = 0xFF;

  /* one video frame which allocate by VPU may arrived before propose_allocation.
   * need check alignment for the video frame. */
  sink->pool_alignment_checked = FALSE;

  for (i=0; i<sink->disp_count; i++) {
    if (sink->disp_on[i]) {
      gst_overlay_sink_output_config (sink, i);
      if (sink->hoverlay[i] == NULL) {
        sink->hoverlay[i] = (gpointer)osink_object_create_overlay (sink->osink_obj, i, &sink->surface_info);
        if (!sink->hoverlay[i]) {
          GST_ERROR_OBJECT (sink, "create overlay for display %s failed.", sink->disp_info[i].name);
        }
      }
      else {
        if (osink_object_config_overlay (sink->osink_obj, sink->hoverlay[i], &sink->surface_info) < 0 ) {
          GST_ERROR_OBJECT (sink, "configure overlay for display %s failed.", sink->disp_info[i].name);
          osink_object_destroy_overlay (sink->osink_obj, sink->hoverlay[i]);
          sink->hoverlay[i] = NULL;
        }
      }
    }
  }

  gst_imx_video_overlay_prepare_window_handle (sink->imxoverlay, TRUE);

  return TRUE;
}

static gboolean
gst_overlay_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstOverlaySink *sink = GST_OVERLAY_SINK (bsink);
  guint size = 0;
  GstCaps *caps;
  gboolean need_pool;
  GstCaps *pcaps;
  GstStructure *config;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (need_pool) {
    if (caps == NULL) {
      GST_ERROR_OBJECT (sink, "no caps specified.");
      return FALSE;
    }

    GST_DEBUG_OBJECT (sink, "prosal set caps %" GST_PTR_FORMAT, caps);

    if (sink->pool) {
      // check caps, if caps not change, reuse previous pool
      config = gst_buffer_pool_get_config (sink->pool);
      gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

      if (!gst_caps_is_equal (pcaps, caps)) {
        if (sink->prv_buffer) {
          gst_buffer_unref (sink->prv_buffer);
          sink->prv_buffer = NULL;
        }

        gst_buffer_pool_set_active (sink->pool, FALSE);
        gst_object_unref (sink->pool);
        sink->pool = NULL;
      }
      gst_structure_free (config);
    }

    if (!sink->pool) {
      if (gst_overlay_sink_setup_buffer_pool (sink, caps) < 0) {
        GST_ERROR_OBJECT (sink, "setup buffer pool failed.");
        return FALSE;
      }

      sink->pool_alignment_checked = FALSE;
      sink->no_phy_buffer = FALSE;
    }

    if (sink->pool) {
      config = gst_buffer_pool_get_config (sink->pool);
      gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);
      gst_structure_free (config);

      GST_DEBUG_OBJECT (sink, "propose_allocation, pool(%p).", sink->pool);

      gst_query_add_allocation_pool (query, sink->pool, size, sink->min_buffers, sink->max_buffers);
      gst_query_add_allocation_param (query, sink->allocator, NULL);
    } else {
      return FALSE;
    }
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);
  if (sink->composition_meta_enable)
    imx_video_overlay_composition_add_query_meta (query);

  return TRUE;
}

static gboolean
gst_overlay_sink_incrop_changed_and_set (GstVideoCropMeta *src, GstVideoCropMeta *dest)
{
  if (src->x != dest->x || src->y != dest->y || src->width != dest->width || src->height != dest->height) {
    dest->x = src->x;
    dest->y = src->y;
    dest->width = src->width;
    dest->height = src->height;
    return TRUE;
  }

  return FALSE;
}

static gint
gst_overlay_sink_input_config (GstOverlaySink *sink) 
{
  gint i;

  GST_DEBUG_OBJECT (sink, "cropmeta (%d, %d) --> (%d, %d)", 
      sink->cropmeta.x, sink->cropmeta.y, sink->cropmeta.width, sink->cropmeta.height);

  sink->surface_info.src.width = sink->video_align.padding_left + sink->w + sink->video_align.padding_right;
  sink->surface_info.src.height = sink->video_align.padding_top + sink->h + sink->video_align.padding_bottom;
  sink->surface_info.src.left = sink->video_align.padding_left + sink->cropmeta.x;
  sink->surface_info.src.top = sink->video_align.padding_top + sink->cropmeta.y;
  sink->surface_info.src.right = sink->surface_info.src.left + MIN (sink->w, sink->cropmeta.width);
  sink->surface_info.src.bottom = sink->surface_info.src.top + MIN (sink->h, sink->cropmeta.height);

  for (i=0; i<sink->disp_count; i++) {
    sink->config[i] = TRUE;
  }

  return 0;
}

static gint
gst_overlay_sink_check_alignment (GstOverlaySink *sink, GstBuffer *buffer)
{
  if (!sink->pool_alignment_checked) {
    /* one video frame which allocate by VPU will arrived video sink when video 
     * track selection. But pool still active and the pool is used for previous
     * video track. So check video physical meta first, if no, check pool. */
    GstPhyMemMeta *phymemmeta = NULL;
    memset (&sink->video_align, 0, sizeof(GstVideoAlignment));

    phymemmeta = GST_PHY_MEM_META_GET (buffer);
    if (phymemmeta) {
      sink->video_align.padding_right = phymemmeta->x_padding;
      sink->video_align.padding_bottom = phymemmeta->y_padding;
      GST_DEBUG_OBJECT (sink, "physical memory meta x_padding: %d y_padding: %d",
          phymemmeta->x_padding, phymemmeta->y_padding);
    } else {
      if (sink->pool && gst_buffer_pool_is_active (sink->pool)) {
        GstStructure *config;
        config = gst_buffer_pool_get_config (sink->pool);

        if (gst_buffer_pool_config_has_option (config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
          gst_buffer_pool_config_get_video_alignment (config, &sink->video_align);
          GST_DEBUG_OBJECT (sink, "pool has alignment (%d, %d) , (%d, %d)",
              sink->video_align.padding_left, sink->video_align.padding_top,
              sink->video_align.padding_right, sink->video_align.padding_bottom);
        } else {
          GstCaps *caps = NULL;
          GstVideoInfo info;
          gst_buffer_pool_config_get_params(config, &caps, NULL, NULL, NULL);
          if (gst_video_info_from_caps(&info, caps)) {
            if (info.width > sink->w)
              sink->video_align.padding_right = info.width - sink->w;
            if (info.height > sink->h)
              sink->video_align.padding_bottom = info.height - sink->h;
          }
        }
        gst_structure_free (config);
      } else {
        GstVideoMeta *meta = gst_buffer_get_video_meta(buffer);
        if (meta) {
          if (meta->width > sink->w)
            sink->video_align.padding_right = meta->width - sink->w;
          switch (meta->format) {
            case GST_VIDEO_FORMAT_YUY2:
            case GST_VIDEO_FORMAT_YVYU:
            case GST_VIDEO_FORMAT_UYVY:
              if (meta->stride[0]/2 > sink->w)
                sink->video_align.padding_right = meta->stride[0]/2 - sink->w;
              break;
            default:
              GST_WARNING_OBJECT (sink, "Add more format to check stride.");
              break;
          }
          if (meta->height > sink->h)
            sink->video_align.padding_bottom = meta->height - sink->h;
          GST_DEBUG_OBJECT(sink, "video align right %d, bottom %d",
            sink->video_align.padding_right, sink->video_align.padding_bottom);
        }
      }
    }

    sink->pool_alignment_checked = TRUE;
    gst_overlay_sink_input_config (sink);
  }

  return 0;
}

static gint
gst_overlay_sink_get_surface_buffer (GstBuffer *gstbuffer, SurfaceBuffer *surface_buffer)
{
  PhyMemBlock * memblk;
  guint i, n_mem;

  if (!gstbuffer || ! surface_buffer)
    return -1;

  if (gst_is_dmabuf_memory (gst_buffer_peek_memory (gstbuffer, 0))) {
    memset (&surface_buffer->mem, 0, sizeof(PhyMemBlock));
    n_mem = gst_buffer_n_memory (gstbuffer);
    for (i = 0; i < n_mem; i++)
      surface_buffer->fd[i] = gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (gstbuffer, i));
  } else if (gst_buffer_is_phymem (gstbuffer)) {
    memblk = gst_buffer_query_phymem_block (gstbuffer);
    if (!memblk) {
      GST_ERROR ("Can't get physical memory block from gstbuffer (%p).", gstbuffer);
      return -1;
    }

    surface_buffer->mem.size = memblk->size;
    surface_buffer->mem.vaddr = memblk->vaddr;
    surface_buffer->mem.paddr = memblk->paddr;
    surface_buffer->buf = NULL;
  } else if (GST_MEMORY_IS_PHYSICALLY_CONTIGUOUS(gst_buffer_peek_memory (gstbuffer, 0))) {
    GstMapInfo minfo;
    gst_buffer_map (gstbuffer, &minfo, GST_MAP_READ);
    memset (&surface_buffer->mem, 0, sizeof(PhyMemBlock));
    surface_buffer->mem.vaddr = minfo.data;
    surface_buffer->mem.size = minfo.size;
    gst_buffer_unmap (gstbuffer, &minfo);
  } else {
    GST_ERROR ("Shouldn't be here");
    return -1;
  }

  return 0;
}

static GstFlowReturn
gst_overlay_sink_show_frame (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstOverlaySink *sink = GST_OVERLAY_SINK (bsink);
  gboolean not_overlay_buffer = FALSE;
  GstVideoCropMeta *cropmeta = NULL;
  GstVideoFrameFlags flags = GST_VIDEO_FRAME_FLAG_NONE;
  SurfaceBuffer surface_buffer = {0};
  gint i;
  GstVideoInfo info;
  GstCaps *caps = gst_pad_get_current_caps (GST_VIDEO_SINK_PAD (sink));
  gst_video_info_from_caps (&info, caps);

  if(!buffer)
  {
    GST_ERROR_OBJECT (sink, "Invalid buffer pointer.");
    gst_caps_unref (caps);
    return GST_FLOW_ERROR;
  }

  cropmeta = gst_buffer_get_video_crop_meta (buffer);
  surface_buffer.fd[0] = -1;

  if (!(gst_buffer_is_phymem (buffer)
        || gst_is_dmabuf_memory (gst_buffer_peek_memory (buffer, 0))
        || GST_MEMORY_IS_PHYSICALLY_CONTIGUOUS(gst_buffer_peek_memory (buffer, 0)))) {
    GST_DEBUG ("copy input frame to physical continues memory");
    // check if physical continues buffer
    GstBuffer *buffer2 = NULL;
    GstVideoFrame frame1, frame2;

    GST_DEBUG_OBJECT (sink, "not physical buffer.");

    gst_video_frame_map (&frame1, &info, buffer, GST_MAP_READ);

    GstCaps *new_caps = gst_video_info_to_caps(&frame1.info);
    gst_video_info_from_caps(&info, new_caps); //update the size info

    if (!sink->pool ||
        !gst_overlay_sink_buffer_pool_is_ok(sink->pool, new_caps,info.size))
    {
      if (sink->pool) {
        gst_object_unref(sink->pool);
        sink->pool = NULL;
      }
      gst_overlay_sink_setup_buffer_pool (sink, new_caps);
      GST_DEBUG_OBJECT(sink, "creating new input pool");
      gst_caps_unref (new_caps);

      if (!sink->pool) {
        gst_caps_unref (caps);
        return GST_FLOW_ERROR;
      }
    } else {
      gst_caps_unref (new_caps);
    }

    if (gst_buffer_pool_set_active (sink->pool, TRUE) != TRUE) {
      GST_ERROR_OBJECT (sink, "active pool(%p) failed.", sink->pool);
      gst_caps_unref (caps);
      return GST_FLOW_ERROR;
    }

    sink->pool_activated = TRUE;
    gst_buffer_pool_acquire_buffer (sink->pool, &buffer2, NULL);
    if (!buffer2) {
      GST_ERROR_OBJECT (sink, "acquire buffer from pool(%p) failed.", sink->pool);
      gst_caps_unref (caps);
      return GST_FLOW_ERROR;
    }

    gst_video_frame_map (&frame2, &info, buffer2, GST_MAP_WRITE);
    gst_video_frame_copy (&frame2, &frame1);
    gst_video_frame_unmap (&frame1);
    gst_video_frame_unmap (&frame2);

    GstVideoMeta *meta = gst_buffer_get_video_meta(buffer);
    if (meta) {
      gst_buffer_add_video_meta(buffer2, meta->flags,
                                meta->format, meta->width, meta->height);
    }

    if (sink->composition_meta_enable
        && imx_video_overlay_composition_has_meta(buffer)) {
      imx_video_overlay_composition_copy_meta(buffer2, buffer,
          info.width, info.height, info.width, info.height);
    }

    buffer = buffer2;
    sink->no_phy_buffer = TRUE;
  } else {
    gst_buffer_ref (buffer);
    sink->no_phy_buffer = FALSE;
  }

  gst_caps_unref (caps);

  if (gst_overlay_sink_get_surface_buffer (buffer, &surface_buffer) < 0) {
    GST_ERROR_OBJECT (sink, "Can't get surface buffer from gst buffer (%p).", buffer);
    if (sink->prv_buffer)
      gst_buffer_unref (sink->prv_buffer);
    sink->prv_buffer = buffer;
    return GST_FLOW_OK;
  }

  if (sink->composition_meta_enable
      && imx_video_overlay_composition_has_meta(buffer)) {
    surface_buffer.buf = buffer;
  } else {
    surface_buffer.buf = NULL;
  }

  GST_DEBUG_OBJECT (sink, "show gstbuffer (%p), surface_buffer vaddr (%p) paddr (%p).",
      buffer, surface_buffer.mem.vaddr, surface_buffer.mem.paddr);

  gst_overlay_sink_check_alignment (sink, buffer);

  if ((cropmeta && gst_overlay_sink_incrop_changed_and_set (cropmeta, &sink->cropmeta))) {
    gst_overlay_sink_input_config (sink);
  }

  for (i=0; i<sink->disp_count; i++) {
    if (sink->disp_on[i]) {
      if (sink->config[i]) {
        GST_DEBUG_OBJECT (sink, "config display %s for surface updated.", sink->disp_info[i].name);
        gst_overlay_sink_output_config (sink, i);
        osink_object_config_overlay (sink->osink_obj, sink->hoverlay[i], &sink->surface_info);
        sink->config[i] = FALSE;
      }

      if (osink_object_update_overlay (sink->osink_obj, sink->hoverlay[i], &surface_buffer) < 0) {
        GST_ERROR_OBJECT (sink, "update overlay buffer (%p) for display (%s) failed.", buffer, sink->disp_info[i].name);
      }
    }
  }

  if (sink->no_phy_buffer && sink->prv_buffer && sink->composition_meta_enable
      && imx_video_overlay_composition_has_meta(sink->prv_buffer)) {
    imx_video_overlay_composition_remove_meta(sink->prv_buffer);
  }
  if (sink->prv_buffer)
    gst_buffer_unref (sink->prv_buffer);

  sink->prv_buffer = buffer;

  sink->frame_showed ++;

  return GST_FLOW_OK;
}

static void
gst_overlay_sink_finalize (GstOverlaySink * overlay_sink)
{
  if (overlay_sink->imxoverlay) {
    gst_imx_video_overlay_finalize (overlay_sink->imxoverlay);
    overlay_sink->imxoverlay = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (overlay_sink));
}

static void
gst_overlay_sink_install_properties (GObjectClass *gobject_class)
{
  gpointer osink_obj = (gpointer)osink_object_new ();
  gint display_count = 0;
  gint prop, i;
  gchar *prop_name;
  gboolean defaul_value = FALSE;

  if (!osink_obj)
    return;

  g_object_class_install_property (gobject_class,
      OVERLAY_SINK_PROP_COMPOSITION_META_ENABLE,
      g_param_spec_boolean("composition-meta-enable", "Enable composition meta",
        "Enable overlay composition meta processing",
        OVERLAY_SINK_COMPOMETA_DEFAULT,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  display_count = osink_object_get_display_count (osink_obj);
  prop = OVERLAY_SINK_PROP_DISP_ON_0;

  for (i = 0; i < display_count; i++) {
    prop = i * OVERLAY_SINK_PROP_DISP_LENGTH + OVERLAY_SINK_PROP_DISP_ON_0;
    gchar *name;
    DisplayInfo info;

    if (osink_object_get_display_info (osink_obj, &info, i) < 0) {
      name = "UNKNOWN";
      info.width = info.height = G_MAXINT32;
    } else {
      name = info.name;
    }
    
    prop_name = g_strdup_printf ("display-%s", name);
    if (i == 0)
      defaul_value = TRUE;
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_boolean (prop_name,
          prop_name,
          "enable/disable show video to the display", defaul_value, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    if (i == 0)
      prop_name = g_strdup_printf ("overlay-left");
    else
      prop_name = g_strdup_printf ("overlay-left-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
          prop_name,
          "get/set the left position of the video to the display",
          G_MININT32, G_MAXINT32, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    if (i == 0)
      prop_name = g_strdup_printf ("overlay-top");
    else
      prop_name = g_strdup_printf ("overlay-top-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
          prop_name,
          "get/set the right postion of the video to the display",
          G_MININT32, G_MAXINT32, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    if (i == 0)
      prop_name = g_strdup_printf ("overlay-width");
    else
      prop_name = g_strdup_printf ("overlay-width-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
          prop_name,
          "get/set the width of the video to the display",
          0, G_MAXINT32, info.width, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    if (i == 0)
      prop_name = g_strdup_printf ("overlay-height");
    else
      prop_name = g_strdup_printf ("overlay-height-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
          prop_name,
          "get/set the height of the video to the display",
          0, G_MAXINT32, info.height, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;


    if (i == 0) {
      prop_name = g_strdup_printf("video-direction");
      g_object_class_override_property (gobject_class, OVERLAY_SINK_PROP_VIDEO_DIRECTION, "video-direction");

    }
    else {
      prop_name = g_strdup_printf ("rotate-%d", i);
      g_object_class_install_property (gobject_class, prop,
        g_param_spec_enum (prop_name,
          prop_name,
          "get/set the rotation of the video", GST_TYPE_IMX_ROTATE_METHOD, DEFAULT_IMX_ROTATE_METHOD,
          G_PARAM_READWRITE));
    }
    g_free (prop_name);
    prop++;

    if (i == 0)
      prop_name = g_strdup_printf ("reconfig");
    else
      prop_name = g_strdup_printf ("reconfig-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_boolean (prop_name,
          prop_name,
          "trigger reconfig of video output x/y/w/h/rot in the fly", FALSE, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    if (i == 0)
      prop_name = g_strdup_printf ("force-aspect-ratio");
    else
      prop_name = g_strdup_printf ("force-aspect-ratio-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_boolean (prop_name, "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio",
          TRUE, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    if (i == 0)
      prop_name = g_strdup_printf ("zorder");
    else
      prop_name = g_strdup_printf ("zorder-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name, prop_name, "get/set overlay zorder",
        0, G_MAXINT, 0, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;
  }

  osink_object_unref (osink_obj);

  return;
}

static GstCaps*
gst_overlay_sink_get_static_caps ()
{
  GstCaps *caps;
  gint i;

#define CAPS_NUM 9
  gchar *caps_str[] = {
    (gchar*)GST_VIDEO_CAPS_MAKE("I420"),
    (gchar*)GST_VIDEO_CAPS_MAKE("NV12"),
    (gchar*)GST_VIDEO_CAPS_MAKE("YV12"),
    (gchar*)GST_VIDEO_CAPS_MAKE("NV16"),
    (gchar*)GST_VIDEO_CAPS_MAKE("UYVY"),
    (gchar*)GST_VIDEO_CAPS_MAKE("YUY2"),
    (gchar*)GST_VIDEO_CAPS_MAKE("RGB16"),
    (gchar*)GST_VIDEO_CAPS_MAKE("RGBA"),
    (gchar*)GST_VIDEO_CAPS_MAKE("RGBx")
  };

  /* make a list of all available caps */
  caps = gst_caps_new_empty ();
  for(i=0; i<CAPS_NUM; i++) {
    GstStructure *structure = gst_structure_from_string(caps_str[i], NULL);
    gst_caps_append_structure (caps, structure);
  }

  caps = gst_caps_simplify(caps);

  imx_video_overlay_composition_add_caps(caps);

  return caps;
}

static GstCaps *gst_overlay_sink_get_caps (GstBaseSink *sink, GstCaps* filter)
{
  GstOverlaySink *overlay_sink = GST_OVERLAY_SINK (sink);
  GstCaps *tmp;

  GstCaps *caps = gst_overlay_sink_get_static_caps();
  if (!overlay_sink->composition_meta_enable)
    imx_video_overlay_composition_remove_caps(caps);

  if (filter) {
    tmp = gst_caps_intersect (caps, filter);
    gst_caps_unref(caps);
    caps = tmp;
  }

  return caps;
}

//type functions

static void
gst_overlay_sink_class_init (GstOverlaySinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseSinkClass *basesink_class;
  GstVideoSinkClass *videosink_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  basesink_class = GST_BASE_SINK_CLASS (klass);
  videosink_class = GST_VIDEO_SINK_CLASS (klass);

  gobject_class->finalize = (GObjectFinalizeFunc) gst_overlay_sink_finalize;
  gobject_class->set_property = gst_overlay_sink_set_property;
  gobject_class->get_property = gst_overlay_sink_get_property;

  element_class->change_state = gst_overlay_sink_change_state;

  gst_overlay_sink_install_properties (gobject_class);

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_overlay_sink_get_static_caps ()));

  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_overlay_sink_get_caps);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_overlay_sink_set_caps);
  basesink_class->propose_allocation = GST_DEBUG_FUNCPTR (gst_overlay_sink_propose_allocation);
  videosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_overlay_sink_show_frame);

  gst_element_class_set_static_metadata (element_class,
      "IMX Video (video compositor) Sink", "Sink/Video",
      "Composite multiple video frames into one and display on IMX SoC", IMX_GST_PLUGIN_AUTHOR);

}

static void
gst_overlay_sink_init (GstOverlaySink * overlay_sink)
{
  gint i;

  overlay_sink->osink_obj = NULL;
  overlay_sink->pool = NULL;
  overlay_sink->allocator = NULL;
  overlay_sink->min_buffers = 3;
  overlay_sink->max_buffers = 30;
  overlay_sink->prv_buffer = NULL;
  memset (&overlay_sink->surface_info, 0, sizeof (SurfaceInfo));

  for (i=0; i<MAX_DISPLAY; i++) {
    overlay_sink->hoverlay[i] = NULL;
    overlay_sink->disp_on[i] = FALSE;
    overlay_sink->config[i] = FALSE;
    memset(&overlay_sink->overlay[i], 0, sizeof (OverlayInfo));
    overlay_sink->overlay[i].keep_ratio = TRUE;
    memset(&overlay_sink->pre_overlay_info[i], 0, sizeof (OverlayInfo));
  }

  overlay_sink->disp_on[0] = TRUE;
  overlay_sink->no_phy_buffer = FALSE;
  overlay_sink->pool_activated = FALSE;
  overlay_sink->pool_alignment_checked = FALSE;
  overlay_sink->composition_meta_enable = OVERLAY_SINK_COMPOMETA_DEFAULT;

  overlay_sink->imxoverlay = gst_imx_video_overlay_init ((GstElement *)overlay_sink,
                                              overlay_sink_update_video_geo,
                                              overlay_sink_config_color_key,
                                              overlay_sink_config_global_alpha);

  g_print("====== OVERLAYSINK: %s build on %s %s. ======\n",  (VERSION),__DATE__,__TIME__);

}


static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (overlay_sink_debug, "overlaysink", 0, "Freescale IMX video overlay(compositor) sink element");

  if (HAS_G2D()) {
    if (!gst_element_register (plugin, "overlaysink", IMX_GST_PLUGIN_RANK + 1,
          GST_TYPE_OVERLAY_SINK))
      return FALSE;

    return TRUE; 
  } else {
    return FALSE;
  }
}

IMX_GST_PLUGIN_DEFINE (overlaysink, "IMX SoC video compositing sink", plugin_init);

