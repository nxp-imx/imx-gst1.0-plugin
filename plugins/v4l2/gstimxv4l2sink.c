/*
 * Copyright (c) 2013-2015, Freescale Semiconductor, Inc. All rights reserved.
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

#include <string.h>
#include <gst/video/gstvideopool.h>
#include "gstimxv4l2sink.h"
#include "gstimxv4l2allocator.h"
#include "imx_2d_device.h"
#include "gstimxvideooverlay.h"
#include <gst/allocators/gstphymemmeta.h>

#define ALIGNMENT_8 (8)
#define ALIGNMENT_2 (2)
#define ISALIGNED(a, b) (!(a & (b-1)))
#define ALIGNTO(a, b) ((a + (b-1)) & (~(b-1)))
#define MAX_BUFFER (32)

GST_DEBUG_CATEGORY (imxv4l2sink_debug);
#define GST_CAT_DEFAULT imxv4l2sink_debug

#define IMX_V4L2SINK_COMPOMETA_DEFAULT     FALSE

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_OVERLAY_TOP,
  PROP_OVERLAY_LEFT,
  PROP_OVERLAY_WIDTH,
  PROP_OVERLAY_HEIGHT,
  PROP_CROP_TOP,
  PROP_CROP_LEFT,
  PROP_CROP_WIDTH,
  PROP_CROP_HEIGHT,
  PROP_KEEP_VIDEO_RATIO,
  PROP_DEINTERLACE_ENABLE,
  PROP_DEINTERLACE_MOTION,
  PROP_CONFIG,
  PROP_COMPOSITION_META_ENABLE,
  PROP_VIDEO_DIRECTION,
};

enum {
  CONFIG_OVERLAY = 0x1,
  CONFIG_CROP = 0x2,
  CONFIG_ROTATE = 0x4
};

GST_IMPLEMENT_VIDEO_OVERLAY_METHODS (GstImxV4l2Sink, gst_imx_v4l2sink);

static gboolean v4l2sink_update_video_geo(GstElement * object, GstVideoRectangle win_rect) {
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (object);
  if (v4l2sink->overlay.left == win_rect.x && v4l2sink->overlay.top == win_rect.y &&
      v4l2sink->overlay.width == win_rect.w && v4l2sink->overlay.height == win_rect.h)
    return TRUE;

  v4l2sink->overlay.left = win_rect.x;
  v4l2sink->overlay.top = win_rect.y;
  v4l2sink->overlay.width = win_rect.w;
  v4l2sink->overlay.height = win_rect.h;
  v4l2sink->config_flag |= CONFIG_OVERLAY;
  v4l2sink->config = TRUE;
  return TRUE;
}

static void v4l2sink_config_global_alpha(GObject * object, guint alpha)
{
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (object);
  if (v4l2sink && v4l2sink->v4l2handle)
    gst_imx_v4l2out_config_alpha(v4l2sink->v4l2handle, alpha);
}

static void v4l2sink_config_color_key(GObject * object, gboolean enable, guint color_key)
{
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (object);
  if (v4l2sink && v4l2sink->v4l2handle)
    gst_imx_v4l2out_config_color_key(v4l2sink->v4l2handle, enable, color_key);
}

static void
gst_imx_v4l2sink_video_direction_interface_init (GstVideoDirectionInterface *
    iface)
{
  /* We implement the video-direction property */
}

#define gst_imx_v4l2sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstImxV4l2Sink, gst_imx_v4l2sink, GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
                       gst_imx_v4l2sink_video_overlay_interface_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_DIRECTION,
        gst_imx_v4l2sink_video_direction_interface_init));

//G_DEFINE_TYPE (GstImxV4l2Sink, gst_imx_v4l2sink, GST_TYPE_VIDEO_SINK);

static void
gst_imx_v4l2sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (object);

  GST_DEBUG_OBJECT (v4l2sink, "set_property (%d).", prop_id);

  switch (prop_id) {
    case PROP_DEVICE:
      g_free (v4l2sink->device);
      v4l2sink->device = g_value_dup_string (value);
      break;
    case PROP_OVERLAY_TOP:
      v4l2sink->overlay.top = g_value_get_int (value);
      v4l2sink->config_flag |= CONFIG_OVERLAY;
      break;
    case PROP_OVERLAY_LEFT:
      v4l2sink->overlay.left = g_value_get_int (value);
      v4l2sink->config_flag |= CONFIG_OVERLAY;
      break;
    case PROP_OVERLAY_WIDTH:
      v4l2sink->overlay.width = g_value_get_uint (value);
      v4l2sink->config_flag |= CONFIG_OVERLAY;
      break;
    case PROP_OVERLAY_HEIGHT:
      v4l2sink->overlay.height = g_value_get_uint (value);
      v4l2sink->config_flag |= CONFIG_OVERLAY;
      break;
    case PROP_CROP_TOP:
      v4l2sink->crop.top = g_value_get_uint (value);
      v4l2sink->config_flag |= CONFIG_CROP;
      break;
    case PROP_CROP_LEFT:
      v4l2sink->crop.left = g_value_get_uint (value);
      v4l2sink->config_flag |= CONFIG_CROP;
      break;
    case PROP_CROP_WIDTH:
      v4l2sink->crop.width = g_value_get_uint (value);
      v4l2sink->config_flag |= CONFIG_CROP;
      break;
    case PROP_CROP_HEIGHT:
      v4l2sink->crop.height = g_value_get_uint (value);
      v4l2sink->config_flag |= CONFIG_CROP;
      break;
    case PROP_VIDEO_DIRECTION:
      v4l2sink->rotate = g_value_get_enum (value);
      v4l2sink->config_flag |= CONFIG_ROTATE;
      break;
    case PROP_KEEP_VIDEO_RATIO:
      v4l2sink->keep_video_ratio = g_value_get_boolean (value);
      v4l2sink->config_flag |= CONFIG_OVERLAY;
      break;
    case PROP_DEINTERLACE_ENABLE:
      v4l2sink->do_deinterlace = g_value_get_boolean (value);
      break;
    case PROP_DEINTERLACE_MOTION:
      v4l2sink->deinterlace_motion = g_value_get_uint (value);
      break;
    case PROP_CONFIG:
      v4l2sink->config = g_value_get_boolean (value);
      break;
    case PROP_COMPOSITION_META_ENABLE:
      v4l2sink->composition_meta_enable = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_imx_v4l2sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, v4l2sink->device);
      break;
    case PROP_OVERLAY_TOP:
      g_value_set_int (value, v4l2sink->overlay.top);
      break;
    case PROP_OVERLAY_LEFT:
      g_value_set_int (value, v4l2sink->overlay.left);
      break;
    case PROP_OVERLAY_WIDTH:
      g_value_set_uint (value, v4l2sink->overlay.width);
      break;
    case PROP_OVERLAY_HEIGHT:
      g_value_set_uint (value, v4l2sink->overlay.height);
      break;
    case PROP_CROP_TOP:
      g_value_set_uint (value, v4l2sink->crop.top);
      break;
    case PROP_CROP_LEFT:
      g_value_set_uint (value, v4l2sink->crop.left);
      break;
    case PROP_CROP_WIDTH:
      g_value_set_uint (value, v4l2sink->crop.width);
      break;
    case PROP_CROP_HEIGHT:
      g_value_set_uint (value, v4l2sink->crop.height);
      break;
    case PROP_VIDEO_DIRECTION:
      g_value_set_enum (value, v4l2sink->rotate);
      break;
    case PROP_KEEP_VIDEO_RATIO:
      g_value_set_boolean (value, v4l2sink->keep_video_ratio);
      break;
    case PROP_DEINTERLACE_ENABLE:
      g_value_set_boolean (value, v4l2sink->do_deinterlace);
      break;
    case PROP_DEINTERLACE_MOTION:
      g_value_set_uint (value, v4l2sink->deinterlace_motion);
      break;
    case PROP_CONFIG:
      g_value_set_boolean (value, v4l2sink->config);
      break;
    case PROP_COMPOSITION_META_ENABLE:
      g_value_set_boolean(value, v4l2sink->composition_meta_enable);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_imx_v4l2sink_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (element);

  GST_DEBUG_OBJECT (v4l2sink, "%d -> %d",
      GST_STATE_TRANSITION_CURRENT (transition),
      GST_STATE_TRANSITION_NEXT (transition));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      {
        guint w,h;

        memset (&v4l2sink->crop, 0, sizeof(IMXV4l2Rect));
        memset (&v4l2sink->video_align, 0, sizeof(GstVideoAlignment));

        v4l2sink->v4l2handle = gst_imx_v4l2_open_device (v4l2sink->device, V4L2_BUF_TYPE_VIDEO_OUTPUT);
        if (!v4l2sink->v4l2handle) {
          return GST_STATE_CHANGE_FAILURE;
        }

        gst_imx_v4l2out_get_res (v4l2sink->v4l2handle, &w, &h);
        if (v4l2sink->overlay.width == 0) {
          v4l2sink->overlay.width = w;
          if (v4l2sink->overlay.left > 0)
            v4l2sink->overlay.width = w - v4l2sink->overlay.left;
        }

        if (v4l2sink->overlay.height == 0) {
          v4l2sink->overlay.height = h;
          if (v4l2sink->overlay.top > 0)
            v4l2sink->overlay.height= h - v4l2sink->overlay.top;
        }

        v4l2sink->config_flag |= CONFIG_OVERLAY;
        //need to configure rotate as PXP will store the previous rotate status
        v4l2sink->config_flag |= CONFIG_ROTATE;

        if ((v4l2sink->config_flag & CONFIG_OVERLAY)
            || (v4l2sink->config_flag & CONFIG_CROP)
            || (v4l2sink->config_flag & CONFIG_ROTATE)) {
          v4l2sink->config = TRUE;
        }
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (v4l2sink->do_deinterlace) {
        gst_imx_v4l2_config_deinterlace (v4l2sink->v4l2handle, 
            v4l2sink->do_deinterlace, v4l2sink->deinterlace_motion);
      }

      v4l2sink->min_buffers =
          gst_imx_v4l2_get_min_buffer_num(v4l2sink->v4l2handle,
                                          V4L2_BUF_TYPE_VIDEO_OUTPUT);
      gst_imx_video_overlay_start (v4l2sink->imxoverlay);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      v4l2sink->run_time = gst_element_get_start_time (GST_ELEMENT (v4l2sink));
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (v4l2sink->v4l2handle) {
        gst_imx_video_overlay_stop (v4l2sink->imxoverlay);

        if (gst_imx_v4l2_reset_device (v4l2sink->v4l2handle) < 0) {
          return GST_STATE_CHANGE_FAILURE;
        }
        if (v4l2sink->pool) {
          // only deactivate pool if pool activated by own, up stream element
          // may still using it
          if (v4l2sink->pool_activated) {
            gst_buffer_pool_set_active (v4l2sink->pool, FALSE);
            v4l2sink->pool_activated = FALSE;
          }
          gst_object_unref (v4l2sink->pool);
          v4l2sink->pool = NULL;
        }
        if (v4l2sink->allocator) {
          gst_object_unref (v4l2sink->allocator);
          v4l2sink->allocator = NULL;
        }
        g_hash_table_remove_all (v4l2sink->v4l2buffer2buffer_table);
      }
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (v4l2sink->v4l2handle) {
        if (gst_imx_v4l2_close_device (v4l2sink->v4l2handle) < 0 ) {
          return GST_STATE_CHANGE_FAILURE;
        }
        v4l2sink->v4l2handle = NULL;
      }

      {
        if (v4l2sink->run_time > 0) {
          g_print ("Total showed frames (%lld), playing for (%"GST_TIME_FORMAT"), fps (%.3f).\n",
              v4l2sink->frame_showed, GST_TIME_ARGS (v4l2sink->run_time),
              (gfloat)GST_SECOND * v4l2sink->frame_showed / v4l2sink->run_time);
        }
        v4l2sink->frame_showed = 0;
        v4l2sink->run_time = 0;
      }

      break;
    default:
      break;
  }

  return ret;
}

static guint
gst_imx_v4l2_special_fmt (GstCaps *caps)
{
  return 0;
}

static gint
gst_imx_v4l2sink_configure_input (GstImxV4l2Sink *v4l2sink)
{
  IMXV4l2Rect crop;
  guint w,h;

  w = v4l2sink->w + v4l2sink->video_align.padding_left + v4l2sink->video_align.padding_right;
  h = v4l2sink->h + v4l2sink->video_align.padding_top + v4l2sink->video_align.padding_bottom;

  crop.left = v4l2sink->video_align.padding_left + v4l2sink->crop.left + v4l2sink->cropmeta.x;
  crop.top = v4l2sink->video_align.padding_top +  v4l2sink->crop.top + v4l2sink->cropmeta.y;
  crop.width = MIN (v4l2sink->cropmeta.width, v4l2sink->crop.width);
  crop.height = MIN (v4l2sink->cropmeta.height, v4l2sink->crop.height);

  GST_DEBUG_OBJECT (v4l2sink, "crop: (%d,%d) -> (%d, %d)", 
      v4l2sink->crop.left, v4l2sink->crop.top, v4l2sink->crop.width, v4l2sink->crop.height);
  GST_DEBUG_OBJECT (v4l2sink, "cropmeta: (%d,%d) -> (%d, %d)", 
      v4l2sink->cropmeta.x, v4l2sink->cropmeta.y, v4l2sink->cropmeta.width, v4l2sink->cropmeta.height);
  GST_DEBUG_OBJECT (v4l2sink, "padding: (%d,%d), (%d, %d)", 
      v4l2sink->video_align.padding_left, v4l2sink->video_align.padding_top, 
      v4l2sink->video_align.padding_right, v4l2sink->video_align.padding_bottom);

  return gst_imx_v4l2out_config_input (v4l2sink->v4l2handle, v4l2sink->v4l2fmt, w, h, &crop);
}

static gint
gst_imx_v4l2sink_configure_rotate (GstImxV4l2Sink *v4l2sink)
{
  gint ret = -1;

  if (v4l2sink->rotate < GST_IMX_ROTATION_HFLIP) {
    ret = gst_imx_v4l2_config_rotate (v4l2sink->v4l2handle, v4l2sink->rotate * 90);
  } else {
    guint flip = v4l2sink->rotate > GST_IMX_ROTATION_HFLIP ? V4L2_CID_VFLIP : V4L2_CID_HFLIP;
    ret = gst_imx_v4l2_config_flip (v4l2sink->v4l2handle, flip);
  }

  return ret;
}

static gint
gst_imx_v4l2_allocator_cb (gpointer user_data, gint *count)
{
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (user_data);
  guint min, max;

  if (v4l2sink->pool) {
   GstStructure *config;
    config = gst_buffer_pool_get_config (v4l2sink->pool);

    // check if has alignment option setted.
    // if yes, need to recheck the pool params for reconfigure v4l2 devicec.
    if (gst_buffer_pool_config_has_option (config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
      memset (&v4l2sink->video_align, 0, sizeof(GstVideoAlignment));
      gst_buffer_pool_config_get_video_alignment (config, &v4l2sink->video_align);
      GST_DEBUG_OBJECT (v4l2sink, "pool has alignment (%d, %d) , (%d, %d)", 
          v4l2sink->video_align.padding_left, v4l2sink->video_align.padding_top,
          v4l2sink->video_align.padding_right, v4l2sink->video_align.padding_bottom);
    }

    gst_buffer_pool_config_get_params (config, NULL, NULL, &min, &max);
    GST_DEBUG_OBJECT (v4l2sink, "need allocate %d buffers.\n", max);
    g_print ("v4l2sink need allocate %d buffers.\n", max);
    gst_structure_free(config);

    if (gst_imx_v4l2sink_configure_input (v4l2sink) < 0)
      return -1;

    if (v4l2sink->use_userptr_mode) {
      if (gst_imx_v4l2_set_buffer_count (v4l2sink->v4l2handle, MAX_BUFFER, V4L2_MEMORY_USERPTR) < 0)
        return -1;
    } else {
      if (gst_imx_v4l2_set_buffer_count (v4l2sink->v4l2handle, max, V4L2_MEMORY_MMAP) < 0)
        return -1;
    }

    *count = max;
  }
  else {
    GST_ERROR_OBJECT (v4l2sink, "no pool to get buffer count.\n");
    return -1;
  }

  return 0;
}

static gboolean
gst_imx_v4l2sink_buffer_pool_is_ok (GstBufferPool * pool, GstCaps * newcaps,
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
gst_imx_v4l2sink_setup_buffer_pool (GstImxV4l2Sink *v4l2sink, GstCaps *caps)
{
  GstStructure *structure;
  gint size;
  GstVideoInfo info;
  IMXV4l2AllocatorContext context;

  v4l2sink->pool = gst_video_buffer_pool_new ();
  if (!v4l2sink->pool) {
    GST_ERROR_OBJECT (v4l2sink, "New video buffer pool failed.\n");
    return -1;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (v4l2sink, "invalid caps.");
    return -1;
  }

  GST_DEBUG_OBJECT (v4l2sink, "create buffer pool(%p).", v4l2sink->pool);

  context.v4l2_handle = v4l2sink->v4l2handle;
  context.user_data = (gpointer) v4l2sink;
  context.callback = gst_imx_v4l2_allocator_cb;
  if (!v4l2sink->allocator)
    v4l2sink->allocator = gst_imx_v4l2_allocator_new (&context);
  if (!v4l2sink->allocator) {
    GST_ERROR_OBJECT (v4l2sink, "New v4l2 allocator failed.\n");
    return -1;
  }

  structure = gst_buffer_pool_get_config (v4l2sink->pool);

  gst_buffer_pool_config_add_option (structure, GST_BUFFER_POOL_OPTION_VIDEO_META);

  // buffer alignment configuration
  gint w = GST_VIDEO_INFO_WIDTH (&info);
  gint h = GST_VIDEO_INFO_HEIGHT (&info);
  if (!ISALIGNED (w, ALIGNMENT_8) || !ISALIGNED (h, ALIGNMENT_2)) {
    GstVideoAlignment alignment;

    memset (&alignment, 0, sizeof (GstVideoAlignment));
    alignment.padding_right = ALIGNTO (w, ALIGNMENT_8) - w;
    alignment.padding_bottom = ALIGNTO (h, ALIGNMENT_2) - h;

    GST_DEBUG ("align buffer pool, w(%d) h(%d), padding_right (%d), padding_bottom (%d)",
        w, h, alignment.padding_right, alignment.padding_bottom);

    gst_buffer_pool_config_add_option (structure, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    gst_buffer_pool_config_set_video_alignment (structure, &alignment);
  }

  size = (ALIGNTO (w, ALIGNMENT_8)) * (ALIGNTO (h, ALIGNMENT_2)) * gst_imx_v4l2_get_bits_per_pixel (v4l2sink->v4l2fmt) / 8;
  gst_buffer_pool_config_set_params (structure, caps, size, v4l2sink->min_buffers, v4l2sink->min_buffers);
  gst_buffer_pool_config_set_allocator (structure, v4l2sink->allocator, NULL);
  if (!gst_buffer_pool_set_config (v4l2sink->pool, structure)) {
    GST_ERROR_OBJECT (v4l2sink, "set buffer pool failed.\n");
    return -1;
  }

  g_hash_table_remove_all (v4l2sink->v4l2buffer2buffer_table);

  return 0;
}

static gboolean
gst_imx_v4l2sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (bsink);
  GstVideoInfo info;
  guint v4l2fmt;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (v4l2sink, "invalid caps.");
    return FALSE;
  }

  GST_DEBUG_OBJECT (v4l2sink, "set caps %" GST_PTR_FORMAT, caps);

  v4l2fmt = gst_imx_v4l2_fmt_gst2v4l2 (GST_VIDEO_INFO_FORMAT (&info));
  if (!v4l2fmt) {
    v4l2fmt = gst_imx_v4l2_special_fmt (caps);
  }

  v4l2sink->v4l2fmt = v4l2fmt;
  v4l2sink->w = GST_VIDEO_INFO_WIDTH (&info);
  v4l2sink->h = GST_VIDEO_INFO_HEIGHT (&info);
  v4l2sink->cropmeta.x = 0;
  v4l2sink->cropmeta.y = 0;
  v4l2sink->cropmeta.width = v4l2sink->w;
  v4l2sink->cropmeta.height = v4l2sink->h;

  if (v4l2sink->crop.width <= 0) {
    v4l2sink->crop.width = v4l2sink->w;
    if (v4l2sink->crop.left > 0)
      v4l2sink->crop.width -= v4l2sink->crop.left;
  }

  if (v4l2sink->crop.height <= 0) {
    v4l2sink->crop.height = v4l2sink->h;
    if (v4l2sink->crop.top > 0)
      v4l2sink->crop.height -= v4l2sink->crop.top;
  }

  gst_imx_video_overlay_prepare_window_handle (v4l2sink->imxoverlay, TRUE);

  return TRUE;
}

static gboolean
gst_imx_v4l2sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (bsink);
  guint size = 0;
  GstCaps *caps;
  gboolean need_pool;
  GstVideoInfo info;

  gst_query_parse_allocation (query, &caps, &need_pool);
  if (caps == NULL) {
    GST_ERROR_OBJECT (v4l2sink, "no caps specified.");
    return FALSE;
  }

  if (!gst_video_info_from_caps(&info, caps)) {
    GST_ERROR_OBJECT (v4l2sink, "can't get info from caps.");
    return FALSE;
  }

  if (need_pool) {
    guint v4l2fmt = gst_imx_v4l2_fmt_gst2v4l2 (GST_VIDEO_INFO_FORMAT (&info));
    if (!v4l2fmt) {
      v4l2fmt = gst_imx_v4l2_special_fmt (caps);
    }

    v4l2sink->v4l2fmt = v4l2fmt;
    v4l2sink->w = GST_VIDEO_INFO_WIDTH (&info);
    v4l2sink->h = GST_VIDEO_INFO_HEIGHT (&info);
    v4l2sink->cropmeta.x = 0;
    v4l2sink->cropmeta.y = 0;
    v4l2sink->cropmeta.width = v4l2sink->w;
    v4l2sink->cropmeta.height = v4l2sink->h;

    if (v4l2sink->crop.width <= 0) {
      v4l2sink->crop.width = v4l2sink->w;
      if (v4l2sink->crop.left > 0)
        v4l2sink->crop.width -= v4l2sink->crop.left;
    }

    if (v4l2sink->crop.height <= 0) {
      v4l2sink->crop.height = v4l2sink->h;
      if (v4l2sink->crop.top > 0)
        v4l2sink->crop.height -= v4l2sink->crop.top;
    }

    if (v4l2sink->pool) {
      if (!gst_imx_v4l2sink_buffer_pool_is_ok(v4l2sink->pool, caps, info.size)){
        gst_imx_v4l2_reset_device (v4l2sink->v4l2handle);
        gst_buffer_pool_set_active (v4l2sink->pool, FALSE);
        gst_object_unref (v4l2sink->pool);
        v4l2sink->pool = NULL;
      }
    }

    if (!v4l2sink->pool) {
      if (gst_imx_v4l2sink_setup_buffer_pool (v4l2sink, caps) < 0)
        return FALSE;
    }

    GST_DEBUG_OBJECT (v4l2sink, "propose_allocation, pool(%p).",v4l2sink->pool);

    GstCaps *pcaps;
    GstStructure *config;

    config = gst_buffer_pool_get_config (v4l2sink->pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);
    gst_structure_free (config);

    /* we need at least 3 buffers to operate */
    gst_query_add_allocation_pool (query, v4l2sink->pool, size, v4l2sink->min_buffers, v4l2sink->min_buffers);
    gst_query_add_allocation_param (query, v4l2sink->allocator, NULL);
  }

  /* we also support various metadata */
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);
  if (v4l2sink->composition_meta_enable && v4l2sink->blend_dev) {
    if (imx_video_overlay_composition_is_out_fmt_support(v4l2sink->blend_dev,
                                                         info.finfo->format))
      imx_video_overlay_composition_add_query_meta (query);
    else
      g_print("!!!device don't support %s format, can't do in-place "
          "blending, will try software blending!!!\n",
                gst_video_format_to_string(info.finfo->format));
  }

  return TRUE;
}

static gboolean
gst_imx_v4l2sink_incrop_changed_and_set (GstVideoCropMeta *src, GstVideoCropMeta *dest)
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

static gboolean
remove_list_item (gpointer key, gpointer value, gpointer user_data)
{
  return value != user_data;
}

static GstFlowReturn
gst_imx_v4l2sink_show_frame (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (bsink);
  gboolean not_v4l2buffer = FALSE;
  GstVideoCropMeta *cropmeta = NULL;
  GstVideoMeta *vmeta = NULL;
  GstVideoFrameFlags flags = GST_VIDEO_FRAME_FLAG_NONE;
  GstMemory *mem = NULL;
  GstBuffer *v4l2_buffer = NULL, *in_buffer = NULL;

  cropmeta = gst_buffer_get_video_crop_meta (buffer);
  vmeta = gst_buffer_get_video_meta (buffer);

  // check if v4l2sink allocated buffer
  mem = gst_buffer_get_memory (buffer, 0);
  if (mem && mem->allocator != v4l2sink->allocator)
    not_v4l2buffer = TRUE;
  gst_memory_unref (mem);

  GstVideoInfo info;
  GstCaps *caps = gst_pad_get_current_caps (GST_VIDEO_SINK_PAD (v4l2sink));
  gst_video_info_from_caps (&info, caps);

  if (not_v4l2buffer) {
    GstVideoFrame frame1, frame2;

    GST_DEBUG_OBJECT (v4l2sink, "not v4l2 allocated buffer.");

    v4l2sink->use_userptr_mode = FALSE;
    if (gst_buffer_is_phymem (buffer) && HAS_IPU()) {
      v4l2sink->use_userptr_mode = TRUE;
      if (!v4l2sink->pool) {
        if (gst_imx_v4l2sink_setup_buffer_pool (v4l2sink, caps) < 0) {
          GST_ERROR ("create replace buffer pool failed");
          gst_caps_unref (caps);
          return GST_FLOW_ERROR;
        }
      }
    } else {
      gst_video_frame_map (&frame2, &info, buffer, GST_MAP_READ);
      GstCaps *new_caps = gst_video_info_to_caps(&frame2.info);
      gst_video_info_from_caps(&info, new_caps); //update the size info

      if (v4l2sink->pool) {
        if (!gst_imx_v4l2sink_buffer_pool_is_ok(v4l2sink->pool, new_caps,
            info.size)) {
          gst_imx_v4l2_reset_device (v4l2sink->v4l2handle);
          gst_buffer_pool_set_active (v4l2sink->pool, FALSE);
          gst_object_unref(v4l2sink->pool);
          v4l2sink->pool = NULL;
        }
      }

      if (!v4l2sink->pool) {
        if (gst_imx_v4l2sink_setup_buffer_pool (v4l2sink, new_caps) < 0) {
          GST_ERROR ("create copy buffer pool failed");
          gst_caps_unref (new_caps);
          gst_caps_unref (caps);
          return GST_FLOW_ERROR;
        }
        GST_DEBUG_OBJECT(v4l2sink, "creating new input pool");
      }
      gst_caps_unref (new_caps);
    }

    GstPhyMemMeta *phymemmeta = NULL;
    if (v4l2sink->pool_activated == FALSE) {
      phymemmeta = GST_PHY_MEM_META_GET (buffer);
      if (phymemmeta) {
        v4l2sink->video_align.padding_right = phymemmeta->x_padding;
        v4l2sink->video_align.padding_bottom = phymemmeta->y_padding;
        GST_DEBUG_OBJECT (v4l2sink,
            "physical memory meta x_padding: %d y_padding: %d",
            phymemmeta->x_padding, phymemmeta->y_padding);

        GstStructure *config = gst_buffer_pool_get_config (v4l2sink->pool);
        if (!gst_buffer_pool_config_has_option (config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
          gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
          gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
        }
        gint size = (v4l2sink->w + phymemmeta->x_padding) * (v4l2sink->h + phymemmeta->y_padding)
            * gst_imx_v4l2_get_bits_per_pixel (v4l2sink->v4l2fmt) / 8;
        gst_buffer_pool_config_set_video_alignment (config, &v4l2sink->video_align);
        gst_buffer_pool_config_set_params (config, caps, size, v4l2sink->min_buffers, v4l2sink->min_buffers);
        gst_buffer_pool_set_config (v4l2sink->pool, config);
      } else {
        if (vmeta) {
          if (vmeta->width > v4l2sink->w)
            v4l2sink->video_align.padding_right = vmeta->width - v4l2sink->w;
          if (vmeta->height > v4l2sink->h)
            v4l2sink->video_align.padding_bottom = vmeta->height - v4l2sink->h;
          GST_DEBUG_OBJECT(v4l2sink, "video align right %d, bottom %d",
              v4l2sink->video_align.padding_right, v4l2sink->video_align.padding_bottom);
          v4l2sink->config = TRUE;
          v4l2sink->config_flag |= CONFIG_CROP;
        }
      }
    }

    if (gst_buffer_pool_set_active (v4l2sink->pool, TRUE) != TRUE) {
      GST_ERROR_OBJECT (v4l2sink, "active pool(%p) failed.", v4l2sink->pool);
      gst_caps_unref (caps);
      return GST_FLOW_ERROR;
    }

    gst_buffer_pool_acquire_buffer (v4l2sink->pool, &v4l2_buffer, NULL);
    if (!v4l2_buffer) {
      GST_ERROR_OBJECT (v4l2sink, "acquire buffer from pool(%p) failed.", v4l2sink->pool);
      gst_caps_unref (caps);
      return GST_FLOW_ERROR;
    }

    v4l2sink->pool_activated = TRUE;

    if (v4l2sink->use_userptr_mode) {
      PhyMemBlock *memblk1, *memblk2;

      GST_DEBUG_OBJECT (v4l2sink, "not v4l2 allocated buffer. put physical address");
      memblk1 = gst_buffer_query_phymem_block (v4l2_buffer);
      memblk2 = gst_buffer_query_phymem_block (buffer);
      memblk1->vaddr = memblk2->vaddr;
      memblk1->paddr = memblk2->paddr;
      memblk1->size = memblk2->size;
      in_buffer = buffer;
      g_hash_table_replace (v4l2sink->v4l2buffer2buffer_table, 
          (gpointer)(memblk1->vaddr), (gpointer)(buffer));
    } else {
      gst_video_frame_map (&frame1, &info, v4l2_buffer, GST_MAP_WRITE);
      gst_video_frame_copy (&frame1, &frame2);
      gst_video_frame_unmap (&frame1);
      gst_video_frame_unmap (&frame2);
    }

    if (v4l2sink->composition_meta_enable
        && imx_video_overlay_composition_has_meta(buffer)) {
      imx_video_overlay_composition_copy_meta(v4l2_buffer, buffer,
          info.width, info.height, info.width, info.height);
    }

    buffer = v4l2_buffer;
  }

  gst_caps_unref (caps);

  if (cropmeta && gst_imx_v4l2sink_incrop_changed_and_set (cropmeta, &v4l2sink->cropmeta)) {
    v4l2sink->config = TRUE;
    v4l2sink->config_flag |= CONFIG_CROP;
  }

  if (v4l2sink->config) {
    if (v4l2sink->config_flag & CONFIG_CROP) {
      if (gst_imx_v4l2sink_configure_input (v4l2sink) < 0) {
        GST_ERROR_OBJECT (v4l2sink, "configure input failed.");
        return GST_FLOW_ERROR;
      }
      v4l2sink->config_flag &= ~CONFIG_CROP;
      if (v4l2sink->keep_video_ratio) {
        //need to recalculate output as input resolution changed
        v4l2sink->config_flag |= CONFIG_OVERLAY;
      }
    }

    if (v4l2sink->config_flag & CONFIG_ROTATE) {
      if (gst_imx_v4l2sink_configure_rotate (v4l2sink) < 0) {
        GST_WARNING_OBJECT (v4l2sink, "configure rotate failed.");
        v4l2sink->rotate = v4l2sink->prev_rotate;
      } else {
        v4l2sink->prev_rotate = v4l2sink->rotate;
        if (v4l2sink->keep_video_ratio) {
          //need to recalculate output as rotation changed
          v4l2sink->config_flag |= CONFIG_OVERLAY;
        }
      }
      v4l2sink->config_flag &= ~CONFIG_ROTATE;
    }

    if (v4l2sink->config_flag & CONFIG_OVERLAY) {
      gint ret = gst_imx_v4l2out_config_output (v4l2sink->v4l2handle,
          &v4l2sink->overlay, v4l2sink->keep_video_ratio);
      if (ret < 0) {
        GST_WARNING_OBJECT (v4l2sink, "configure output failed.");
        memcpy(&v4l2sink->overlay, &v4l2sink->prev_overlay, sizeof(IMXV4l2Rect));
      } else {
        memcpy(&v4l2sink->prev_overlay, &v4l2sink->overlay, sizeof(IMXV4l2Rect));
      }
      if (ret == 1) {
        v4l2sink->invisible = TRUE;
      } else {
        if (ret == 2) {
          g_hash_table_foreach_remove (v4l2sink->v4l2buffer2buffer_table,
              remove_list_item, in_buffer);
        }
        v4l2sink->invisible = FALSE;
      }
      v4l2sink->config_flag &= ~CONFIG_OVERLAY;
    }

    v4l2sink->config = FALSE;
  }

  if (v4l2sink->composition_meta_enable) {
    if (imx_video_overlay_composition_has_meta(buffer)) {

      VideoCompositionVideoInfo in_v, out_v;
      memset (&in_v, 0, sizeof(VideoCompositionVideoInfo));
      memset (&out_v, 0, sizeof(VideoCompositionVideoInfo));
      in_v.buf = buffer;
      in_v.fmt = out_v.fmt = info.finfo->format;
      in_v.width = out_v.width = info.width;
      in_v.height = out_v.height = info.height;
      in_v.stride = out_v.stride = info.stride[0];
      in_v.crop_x = out_v.crop_x = v4l2sink->crop.left;
      in_v.crop_y = out_v.crop_y = v4l2sink->crop.top;
      in_v.crop_w = out_v.crop_w = v4l2sink->crop.width;
      in_v.crop_h = out_v.crop_h = v4l2sink->crop.height;
      in_v.rotate = out_v.rotate = IMX_2D_ROTATION_0;

      out_v.mem = gst_buffer_query_phymem_block(buffer);
      memcpy(&out_v.align, &(v4l2sink->video_align), sizeof(GstVideoAlignment));

      gint cnt = imx_video_overlay_composition_composite(&v4l2sink->video_comp,
                                                         &in_v, &out_v, TRUE);

      if (cnt >= 0)
        GST_DEBUG ("processed %d video overlay composition buffers", cnt);
      else
        GST_WARNING ("video overlay composition meta handling failed");
    }
  }

  if (!not_v4l2buffer)
    gst_buffer_ref (buffer);

  if (vmeta)
    flags = vmeta->flags;

  if (gst_imx_v4l2_queue_gstbuffer (v4l2sink->v4l2handle, buffer, flags) < 0) {
    GST_ERROR_OBJECT (v4l2sink, "Queue buffer %p failed.", buffer);
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  v4l2sink->frame_showed ++;

  v4l2_buffer = NULL;
  if (gst_imx_v4l2_dequeue_gstbuffer (v4l2sink->v4l2handle, &v4l2_buffer, &flags) < 0) {
    GST_ERROR_OBJECT (v4l2sink, "Dequeue buffer failed.");
    return GST_FLOW_ERROR;
  }

  if (v4l2sink->use_userptr_mode) {
    if (in_buffer)
      gst_buffer_ref (in_buffer);
    if (v4l2_buffer) {
      PhyMemBlock *memblk;
      memblk = gst_buffer_query_phymem_block (v4l2_buffer);
      g_hash_table_remove (v4l2sink->v4l2buffer2buffer_table, memblk->vaddr);
      GST_DEBUG_OBJECT (v4l2sink, "Reserved count: %d\n.", 
          g_hash_table_size (v4l2sink->v4l2buffer2buffer_table));
    }
    if (v4l2sink->invisible)
      g_hash_table_remove_all (v4l2sink->v4l2buffer2buffer_table);
  }
  if (v4l2_buffer)
    gst_buffer_unref (v4l2_buffer);

  return GST_FLOW_OK;
}

static void
gst_imx_v4l2sink_finalize (GstImxV4l2Sink * v4l2sink)
{
  if (v4l2sink->v4l2buffer2buffer_table != NULL) {
    g_hash_table_destroy (v4l2sink->v4l2buffer2buffer_table);
    v4l2sink->v4l2buffer2buffer_table = NULL;
  }

  if (v4l2sink->device)
    g_free (v4l2sink->device);

  if (v4l2sink->imxoverlay) {
    gst_imx_video_overlay_finalize (v4l2sink->imxoverlay);
    v4l2sink->imxoverlay = NULL;
  }

  if (v4l2sink->blend_dev) {
    imx_video_overlay_composition_deinit(&v4l2sink->video_comp);
    v4l2sink->blend_dev->close(v4l2sink->blend_dev);
    imx_2d_device_destroy(v4l2sink->blend_dev);
  }

  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (v4l2sink));
}

static void
gst_imx_v4l2sink_install_properties (GObjectClass *gobject_class)
{
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device", "Device location",
        gst_imx_v4l2_get_default_device_name(V4L2_BUF_TYPE_VIDEO_OUTPUT), G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_OVERLAY_TOP,
      g_param_spec_int ("overlay-top", "Overlay top",
        "The topmost (y) coordinate of the video overlay; top left corner of screen is 0,0",
        G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_OVERLAY_LEFT,
      g_param_spec_int ("overlay-left", "Overlay left",
        "The leftmost (x) coordinate of the video overlay; top left corner of screen is 0,0",
        G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_OVERLAY_WIDTH,
      g_param_spec_uint ("overlay-width", "Overlay width",
        "The width of the video overlay; default is equal to screen width",
        0, G_MAXUINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_OVERLAY_HEIGHT,
      g_param_spec_uint ("overlay-height", "Overlay height",
        "The height of the video overlay; default is equal to screen height",
        0, G_MAXUINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CROP_TOP,
      g_param_spec_uint ("crop-top", "Crop top",
        "The topmost (y) coordinate of the video crop; top left corner of image is 0,0",
        0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_CROP_LEFT,
      g_param_spec_uint ("crop-left", "Crop left",
        "The leftmost (x) coordinate of the video crop; top left corner of image is 0,0",
        0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_CROP_WIDTH,
      g_param_spec_uint ("crop-width", "Crop width",
        "The width of the video crop; default is equal to negotiated image width",
        0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_CROP_HEIGHT,
      g_param_spec_uint ("crop-height", "Crop height",
        "The height of the video crop; default is equal to negotiated image height",
        0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_KEEP_VIDEO_RATIO,
      g_param_spec_boolean ("force-aspect-ratio", "Force aspect ratio",
        "When enabled, scaling will respect original aspect ratio",
        FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CONFIG,
      g_param_spec_boolean ("reconfig", "Reconfig",
        "Change V4L2 configuration while running; overlay position/size/rotation changed.",
        FALSE, G_PARAM_READWRITE));

  if (gst_imx_v4l2_support_deinterlace (V4L2_BUF_TYPE_VIDEO_OUTPUT)) {
    g_object_class_install_property (gobject_class,PROP_DEINTERLACE_ENABLE,
        g_param_spec_boolean ("deinterlace", "deinterlace",
          "set deinterlace enabled; can't be configed on fly",
          TRUE, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_DEINTERLACE_MOTION,
        g_param_spec_uint ("motion",
          "set deinterlace motion; can't be configed on fly",
          "The interlace motion setting: 0 - low motion, 1 - medium motion, 2 - high motion.",
          0, 2, 2, G_PARAM_READWRITE));
  }

  g_object_class_install_property (gobject_class, PROP_COMPOSITION_META_ENABLE,
      g_param_spec_boolean("composition-meta-enable", "Enable composition meta",
        "Enable overlay composition meta processing",
        TRUE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_override_property (gobject_class, PROP_VIDEO_DIRECTION,
      "video-direction");

  return;
}

static GstCaps*
gst_imx_v4l2sink_default_caps ()
{
  GstCaps *caps;
  gint i;

#define CAPS_NUM 8
  gchar *caps_str[] = {
    (gchar*)GST_VIDEO_CAPS_MAKE("I420"),
    (gchar*)GST_VIDEO_CAPS_MAKE("NV12"),
    (gchar*)GST_VIDEO_CAPS_MAKE("YV12"),
    (gchar*)GST_VIDEO_CAPS_MAKE("Y42B"),
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

static GstCaps*
gst_imx_v4l2sink_get_all_caps ()
{
  GstCaps *caps = NULL;
  caps = gst_imx_v4l2_get_device_caps (V4L2_BUF_TYPE_VIDEO_OUTPUT);
  if(!caps) {
    GST_WARNING ("Can't get caps from default device, use the default setting.");
    caps = gst_imx_v4l2sink_default_caps ();
  }

  caps = gst_caps_simplify(caps);

  imx_video_overlay_composition_add_caps(caps);

  return caps;
}

static GstCaps *gst_imx_v4l2sink_get_caps (GstBaseSink *sink, GstCaps* filter)
{
  GstImxV4l2Sink *v4l2sink = GST_IMX_V4L2SINK (sink);
  GstCaps *tmp;

  GstCaps *temp_caps = gst_pad_get_pad_template_caps(sink->sinkpad);
  GstCaps *caps = gst_caps_copy(temp_caps);
  gst_caps_unref(temp_caps);
  if (!v4l2sink->composition_meta_enable)
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
gst_imx_v4l2sink_class_init (GstImxV4l2SinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseSinkClass *basesink_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  basesink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->finalize = (GObjectFinalizeFunc) gst_imx_v4l2sink_finalize;
  gobject_class->set_property = gst_imx_v4l2sink_set_property;
  gobject_class->get_property = gst_imx_v4l2sink_get_property;

  element_class->change_state = gst_imx_v4l2sink_change_state;

  gst_imx_v4l2sink_install_properties (gobject_class);

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        gst_imx_v4l2sink_get_all_caps ()));

  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_imx_v4l2sink_get_caps);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_imx_v4l2sink_set_caps);
  basesink_class->propose_allocation =
    GST_DEBUG_FUNCPTR (gst_imx_v4l2sink_propose_allocation);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_imx_v4l2sink_show_frame);

  gst_element_class_set_static_metadata (element_class,
      "IMX Video (video4linux2) Sink", "Sink/Video",
      "Displays frames on IMX SoC video4linux2 device", IMX_GST_PLUGIN_AUTHOR);

  GST_DEBUG_CATEGORY_INIT (imxv4l2sink_debug, "imxv4l2sink", 0, "Freescale IMX V4L2 sink element");
}

static void
gst_imx_v4l2sink_init (GstImxV4l2Sink * v4l2sink)
{
  v4l2sink->device = g_strdup (gst_imx_v4l2_get_default_device_name(V4L2_BUF_TYPE_VIDEO_OUTPUT));
  v4l2sink->rotate = 0;
  v4l2sink->prev_rotate = 0;
  v4l2sink->do_deinterlace = TRUE;  /* enable deinterlace by default */
  v4l2sink->deinterlace_motion = 2; /* high motion by default */
  v4l2sink->config = FALSE;
  v4l2sink->config_flag = 0;
  v4l2sink->v4l2handle = NULL;
  v4l2sink->pool = NULL;
  v4l2sink->allocator = NULL;
  memset (&v4l2sink->overlay, 0, sizeof(IMXV4l2Rect));
  memset (&v4l2sink->prev_overlay, 0, sizeof(IMXV4l2Rect));
  memset (&v4l2sink->crop, 0, sizeof(IMXV4l2Rect));
  v4l2sink->keep_video_ratio = FALSE;
  v4l2sink->frame_showed = 0;
  v4l2sink->run_time = 0;
  v4l2sink->min_buffers = 0;
  v4l2sink->pool_activated = FALSE;
  v4l2sink->use_userptr_mode = FALSE;
  v4l2sink->invisible = FALSE;
  v4l2sink->v4l2buffer2buffer_table = g_hash_table_new_full (NULL, NULL, NULL, 
      (GDestroyNotify) gst_buffer_unref);

  v4l2sink->imxoverlay = gst_imx_video_overlay_init ((GstElement *)v4l2sink,
                                              v4l2sink_update_video_geo,
                                              v4l2sink_config_color_key,
                                              v4l2sink_config_global_alpha);

  v4l2sink->composition_meta_enable = IMX_V4L2SINK_COMPOMETA_DEFAULT;
  v4l2sink->blend_dev = NULL;
  if (HAS_IPU())
    v4l2sink->blend_dev = imx_2d_device_create(IMX_2D_DEVICE_IPU);
  else if (HAS_PXP())
    v4l2sink->blend_dev = imx_2d_device_create(IMX_2D_DEVICE_PXP);
/*
  else if (HAS_G2D())  G2D don't support YUV color space
    v4l2sink->blend_dev = imx_2d_device_create(IMX_2D_DEVICE_G2D);
*/

  if (v4l2sink->blend_dev) {
    v4l2sink->blend_dev->open(v4l2sink->blend_dev);
    imx_video_overlay_composition_init(&v4l2sink->video_comp,v4l2sink->blend_dev);
  }

  g_print("====== IMXV4L2SINK: %s build on %s %s. ======\n",  (VERSION),__DATE__,__TIME__);

}

