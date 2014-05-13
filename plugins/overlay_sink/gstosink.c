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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/video/gstvideopool.h>
#include "gstosink.h"
#include "gstosinkallocator.h"

GST_DEBUG_CATEGORY (overlay_sink_debug);
#define GST_CAT_DEFAULT overlay_sink_debug

enum
{
  OVERLAY_SINK_PROP_0,
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

#define gst_overlay_sink_parent_class parent_class
G_DEFINE_TYPE (GstOverlaySink, gst_overlay_sink, GST_TYPE_VIDEO_SINK);

static gint
gst_overlay_sink_output_config (GstOverlaySink *sink, gint idx) 
{
  sink->surface_info.dst.left = sink->overlay[idx].x;
  sink->surface_info.dst.top = sink->overlay[idx].y;
  sink->surface_info.dst.right = sink->overlay[idx].w + sink->surface_info.dst.left;
  sink->surface_info.dst.bottom = sink->overlay[idx].h + sink->surface_info.dst.top;
  sink->surface_info.rot = sink->overlay[idx].rot;
  sink->surface_info.keep_ratio = sink->overlay[idx].keep_ratio;
  sink->surface_info.zorder = sink->overlay[idx].zorder;

  return 0;
}

static void
gst_overlay_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOverlaySink *sink = GST_OVERLAY_SINK (object);
  guint idx, prop; 

  GST_DEBUG_OBJECT (sink, "set_property (%d).", prop_id);

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
      sink->overlay[idx].rot = g_value_get_int (value);
      break;
    case OVERLAY_SINK_PROP_DISP_UPDATE_0:
      sink->config[idx] = g_value_get_boolean (value);
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
      g_value_set_int (value, sink->overlay[idx].rot);
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
      {
        gint i;
        gboolean display_enabled = FALSE;

        sink->osink_obj = osink_object_new ();
        if (!sink->osink_obj) {
          GST_ERROR_OBJECT (sink, "create osink object failed.");
          return GST_STATE_CHANGE_FAILURE;
        }

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
          osink_object_free (sink->osink_obj);
          sink->osink_obj = NULL;
          return GST_STATE_CHANGE_FAILURE;
        }

        sink->frame_showed = 0;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      {
        gint i;

        if (sink->prv_buffer) {
          gst_buffer_unref (sink->prv_buffer);
          sink->prv_buffer = NULL;
        }

        for (i=0; i<sink->disp_count; i++) {
          if (sink->hoverlay[i]) {
            GstClockTime run_time = gst_element_get_start_time (GST_ELEMENT (sink));
            if (run_time > 0) {
              gint64 blited = osink_object_get_overlay_showed_frames (sink->osink_obj, sink->hoverlay[i]);
              g_print ("Total showed frames (%lld), display %s blited (%lld), playing for (%"GST_TIME_FORMAT"), fps (%.3f).\n",
                  sink->frame_showed, sink->disp_info[i].name, blited, GST_TIME_ARGS (run_time),
                  (gfloat)GST_SECOND * blited / run_time);
            }
            osink_object_destroy_overlay (sink->osink_obj, sink->hoverlay[i]);
            sink->hoverlay[i] = NULL;
          }
        }

        if (sink->pool) {
          gst_buffer_pool_set_active (sink->pool, FALSE);
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
          osink_object_free (sink->osink_obj);

      }
      break;
    default:
      break;
  }

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

  sink->allocator = gst_osink_allocator_new (sink);
  if (!sink->allocator) {
    GST_ERROR_OBJECT (sink, "New osink allocator failed.\n");
    return -1;
  }
  GST_DEBUG_OBJECT (sink, "create allocator(%p).", sink->allocator);

  structure = gst_buffer_pool_get_config (sink->pool);
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

  if (sink->pool) {
    gst_buffer_pool_set_active (sink->pool, FALSE);
    gst_object_unref (sink->pool);
    sink->pool = NULL;
  }


  if (gst_overlay_sink_setup_buffer_pool (sink, caps) < 0) {
    GST_ERROR_OBJECT (sink, "setup buffer pool failed.");
    return FALSE;
  }

  sink->pool_alignment_checked = FALSE;

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

  for (i=0; i<sink->disp_count; i++) {
    if (sink->disp_on[i]) {
      gst_overlay_sink_output_config (sink, i);
      if (sink->hoverlay[i] == NULL) {
        sink->hoverlay[i] = osink_object_create_overlay (sink->osink_obj, i, &sink->surface_info);
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

  return TRUE;
}

static gboolean
gst_overlay_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstOverlaySink *sink = GST_OVERLAY_SINK (bsink);
  GstBufferPool *pool = sink->pool;
  GstAllocator *allocator = sink->allocator;
  guint size = 0;
  GstCaps *caps;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL) {
    GST_ERROR_OBJECT (sink, "no caps specified.");
    return FALSE;
  }

  GST_DEBUG_OBJECT (sink, "propose_allocation, pool(%p).", pool);

  if (pool != NULL) {
    GstCaps *pcaps;
    GstStructure *config;

    /* we had a pool, check caps */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);
#if 0 //FIXME:
    if (!gst_caps_is_equal (caps, pcaps)) {
      GST_ERROR_OBJECT (sink, "different caps in propose, pool with caps %" GST_PTR_FORMAT, pcaps);
      GST_ERROR_OBJECT (sink, "upstream caps %" GST_PTR_FORMAT, caps);
      gst_structure_free (config);
      return FALSE;
    }
#endif
    gst_structure_free (config);
  }

  gst_query_add_allocation_pool (query, pool, size, sink->min_buffers, sink->max_buffers);
  gst_query_add_allocation_param (query, allocator, NULL);

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);

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

  if (sink->pool && !sink->pool_alignment_checked) {
    GstStructure *config;
    config = gst_buffer_pool_get_config (sink->pool);

    // check if has alignment option setted.
    memset (&sink->video_align, 0, sizeof(GstVideoAlignment));
    if (gst_buffer_pool_config_has_option (config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
      GstVideoInfo info;
      GstCaps *caps;
      gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL);
      gst_video_info_from_caps (&info, caps);
      gst_buffer_pool_config_get_video_alignment (config, &sink->video_align);
      gst_video_info_align (&info, &sink->video_align);
      GST_DEBUG_OBJECT (sink, "pool has alignment (%d, %d) , (%d, %d)", 
          sink->video_align.padding_left, sink->video_align.padding_top,
          sink->video_align.padding_right, sink->video_align.padding_bottom);
    }
    sink->pool_alignment_checked = TRUE;
  }

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
gst_overlay_sink_get_surface_buffer (GstBuffer *gstbuffer, SurfaceBuffer *surface_buffer)
{
  PhyMemBlock * memblk;

  if (!gstbuffer || ! surface_buffer)
    return -1;

  memblk = gst_buffer_query_phymem_block (gstbuffer);
  if (!memblk) {
    GST_ERROR ("Can't get physical memory block from gstbuffer (%p).", gstbuffer);
    return -1;
  }

  surface_buffer->size = memblk->size;
  surface_buffer->vaddr = memblk->vaddr;
  surface_buffer->paddr = memblk->paddr;

  return 0;
}

static GstFlowReturn
gst_overlay_sink_show_frame (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstOverlaySink *sink = GST_OVERLAY_SINK (bsink);
  gboolean not_overlay_buffer = FALSE;
  GstVideoCropMeta *cropmeta = NULL;
  GstVideoFrameFlags flags = GST_VIDEO_FRAME_FLAG_NONE;
  SurfaceBuffer surface_buffer;
  gint i;

  if (!gst_buffer_is_phymem (buffer)) {
    // check if physical buffer
    GstBuffer *buffer2 = NULL;
    GstCaps *caps = gst_pad_get_current_caps (GST_VIDEO_SINK_PAD (sink));
    GstVideoInfo info;
    GstVideoFrame frame1, frame2;

    GST_DEBUG_OBJECT (sink, "not physical buffer.");

    if (!sink->pool && gst_overlay_sink_setup_buffer_pool (sink, caps) < 0)
      return GST_FLOW_ERROR;
    if (gst_buffer_pool_set_active (sink->pool, TRUE) != TRUE) {
      GST_ERROR_OBJECT (sink, "active pool(%p) failed.", sink->pool);
      return GST_FLOW_ERROR;
    }
    gst_buffer_pool_acquire_buffer (sink->pool, &buffer2, NULL);
    if (!buffer2) {
      GST_ERROR_OBJECT (sink, "acquire buffer from pool(%p) failed.", sink->pool);
      return GST_FLOW_ERROR;
    }

    gst_video_info_from_caps (&info, caps);
    gst_video_frame_map (&frame1, &info, buffer, GST_MAP_READ);
    gst_video_frame_map (&frame2, &info, buffer2, GST_MAP_WRITE);

    gst_video_frame_copy (&frame2, &frame1);

    gst_video_frame_unmap (&frame1);
    gst_video_frame_unmap (&frame2);
    gst_caps_unref (caps);

    buffer = buffer2;
  }
  else
    gst_buffer_ref (buffer);

  if (gst_overlay_sink_get_surface_buffer (buffer, &surface_buffer) < 0) {
    GST_ERROR_OBJECT (sink, "Can't get surface buffer from gst buffer (%p).", buffer);
    if (sink->prv_buffer)
      gst_buffer_unref (sink->prv_buffer);
    sink->prv_buffer = buffer;
    return GST_FLOW_OK;
  }

  GST_DEBUG_OBJECT (sink, "show gstbuffer (%p), surface_buffer vaddr (%p) paddr (%p).",
      buffer, surface_buffer.vaddr, surface_buffer.paddr);

  cropmeta = gst_buffer_get_video_crop_meta (buffer);
  if (!sink->pool_alignment_checked
      || (cropmeta && gst_overlay_sink_incrop_changed_and_set (cropmeta, &sink->cropmeta))) {
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

  if (sink->prv_buffer)
    gst_buffer_unref (sink->prv_buffer);

  sink->prv_buffer = buffer;

  sink->frame_showed ++;

  return GST_FLOW_OK;
}

static void
gst_overlay_sink_finalize (GstOverlaySink * overlay_sink)
{
  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (overlay_sink));
}

static void
gst_overlay_sink_install_properties (GObjectClass *gobject_class)
{
  gpointer osink_obj = osink_object_new ();
  gint display_count = 0;
  gint prop, i;
  gchar *prop_name;
  gboolean defaul_value = FALSE;

  if (!osink_obj)
    return;

  display_count = osink_object_get_display_count (osink_obj);
  prop = OVERLAY_SINK_PROP_DISP_ON_0;

  for (i = 0; i < display_count; i++) {
    prop = i * OVERLAY_SINK_PROP_DISP_LENGTH + OVERLAY_SINK_PROP_DISP_ON_0;
    gchar *name;
    gint max_w, max_h;
    DisplayInfo info;

    if (osink_object_get_display_info (osink_obj, &info, i) < 0) {
      name = "UNKNOWN";
      max_w = max_h = G_MAXINT;
    }
    else {
      name = info.name;
      max_w = info.width;
      max_h = info.height;
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
          0, max_w, 0, G_PARAM_READWRITE));
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
          0, max_h, 0, G_PARAM_READWRITE));
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
          0, max_w, max_w, G_PARAM_READWRITE));
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
          0, max_h, max_h, G_PARAM_READWRITE));
    g_free (prop_name);
    prop++;

    if (i == 0)
      prop_name = g_strdup_printf ("rotate");
    else
      prop_name = g_strdup_printf ("rotate-%d", i);
    g_object_class_install_property (gobject_class, prop,
        g_param_spec_int (prop_name,
          prop_name,
          "get/set the rotation of the video", 0, G_MAXINT, 0, G_PARAM_READWRITE));
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

  osink_object_free (osink_obj);

  return;
}

static GstCaps*
gst_overlay_sink_get_static_caps ()
{
  GstCaps *caps;
  gint i;

#define CAPS_NUM 8
  gchar *caps_str[] = {
    (gchar*)GST_VIDEO_CAPS_MAKE("I420"),
    (gchar*)GST_VIDEO_CAPS_MAKE("NV12"),
    (gchar*)GST_VIDEO_CAPS_MAKE("YV12"),
    (gchar*)GST_VIDEO_CAPS_MAKE("UYVY"),
    (gchar*)GST_VIDEO_CAPS_MAKE("YUY2"),
    (gchar*)GST_VIDEO_CAPS_MAKE("RGB16"),
    (gchar*)GST_VIDEO_CAPS_MAKE("RGBx")
  };

  /* make a list of all available caps */
  caps = gst_caps_new_empty ();
  for(i=0; i<CAPS_NUM; i++) {
    GstStructure *structure = gst_structure_from_string(caps_str[i], NULL);
    gst_caps_append_structure (caps, structure);
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

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  basesink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->finalize = (GObjectFinalizeFunc) gst_overlay_sink_finalize;
  gobject_class->set_property = gst_overlay_sink_set_property;
  gobject_class->get_property = gst_overlay_sink_get_property;

  element_class->change_state = gst_overlay_sink_change_state;

  gst_overlay_sink_install_properties (gobject_class);

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_overlay_sink_get_static_caps ()));

  //FIXME:basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_overlay_sink_get_caps);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_overlay_sink_set_caps);
  basesink_class->propose_allocation = GST_DEBUG_FUNCPTR (gst_overlay_sink_propose_allocation);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_overlay_sink_show_frame);

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
  overlay_sink->min_buffers = 2;
  overlay_sink->max_buffers = 30;
  overlay_sink->prv_buffer = NULL;
  memset (&overlay_sink->surface_info, 0, sizeof (SurfaceInfo));

  for (i=0; i<MAX_DISPLAY; i++) {
    overlay_sink->hoverlay[i] = NULL;
    overlay_sink->disp_on[i] = FALSE;
    overlay_sink->config[MAX_DISPLAY] = FALSE;
    overlay_sink->overlay[i].keep_ratio = TRUE;
    overlay_sink->overlay[i].zorder = 0;
  }

  overlay_sink->disp_on[0] = TRUE;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (overlay_sink_debug, "overlaysink", 0, "Freescale IMX video overlay(compositor) sink element");

  if (!gst_element_register (plugin, "overlaysink", IMX_GST_PLUGIN_RANK,
        GST_TYPE_OVERLAY_SINK))
    return FALSE;

  return TRUE;
}

IMX_GST_PLUGIN_DEFINE (overlaysink, "IMX SoC video compositing sink", plugin_init);

