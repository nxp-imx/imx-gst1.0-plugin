/*
 * Copyright (c) 2014-2015, Freescale Semiconductor, Inc. All rights reserved.
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
#include "gstimxv4l2src.h"
#include "gstimxv4l2allocator.h"

GST_DEBUG_CATEGORY (imxv4l2src_debug);
#define GST_CAT_DEFAULT imxv4l2src_debug

#define DEFAULT_DEVICE "/dev/video0"
#define DEFAULT_FRAME_PLUS 3
#define DEFAULT_USE_V4L2SRC_MEMORY TRUE
#define DEFAULT_FRAMES_IN_V4L2_CAPTURE 3

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_USE_V4L2SRC_MEMORY,
  PROP_FRAME_PLUS,
};

#define gst_imx_v4l2src_parent_class parent_class
G_DEFINE_TYPE (GstImxV4l2Src, gst_imx_v4l2src, GST_TYPE_PUSH_SRC);

static void
gst_imx_v4l2src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstImxV4l2Src *v4l2src = GST_IMX_V4L2SRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, v4l2src->device);
      break;
    case PROP_USE_V4L2SRC_MEMORY:
      g_value_set_boolean (value, v4l2src->use_v4l2_memory);
      break;
    case PROP_FRAME_PLUS:
      g_value_set_uint (value, v4l2src->frame_plus);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_imx_v4l2src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstImxV4l2Src *v4l2src = GST_IMX_V4L2SRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_free (v4l2src->device);
      v4l2src->device = g_value_dup_string (value);
      break;
    case PROP_USE_V4L2SRC_MEMORY:
      v4l2src->use_v4l2_memory = g_value_get_boolean (value);
      break;
    case PROP_FRAME_PLUS:
      v4l2src->frame_plus = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

}

static gboolean
gst_imx_v4l2src_start (GstBaseSrc * src)
{
  GstImxV4l2Src *v4l2src = GST_IMX_V4L2SRC (src);

  GST_INFO_OBJECT (v4l2src, "open device: %s", v4l2src->device);
  v4l2src->v4l2handle = gst_imx_v4l2_open_device (v4l2src->device, \
      V4L2_BUF_TYPE_VIDEO_CAPTURE);
  if (!v4l2src->v4l2handle) {
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_imx_v4l2src_stop (GstBaseSrc * src)
{
  GstImxV4l2Src *v4l2src = GST_IMX_V4L2SRC (src);

  if (v4l2src->v4l2handle) {
    if (gst_imx_v4l2_reset_device (v4l2src->v4l2handle) < 0) {
      return FALSE;
    }
    if (v4l2src->pool) {
      gst_object_unref (v4l2src->pool);
      v4l2src->pool = NULL;
    }
    if (v4l2src->allocator) {
      v4l2src->allocator = NULL;
    }
  }

  if (v4l2src->v4l2handle) {
    if (gst_imx_v4l2_close_device (v4l2src->v4l2handle) < 0 ) {
      return FALSE;
    }
    v4l2src->v4l2handle = NULL;
  }

  if (v4l2src->probed_caps) {
    gst_caps_unref (v4l2src->probed_caps);
    v4l2src->probed_caps = NULL;
  }

  if (v4l2src->old_caps) {
    gst_caps_unref (v4l2src->old_caps);
    v4l2src->old_caps = NULL;
  }

  if (v4l2src->gstbuffer_in_v4l2) {
    g_list_free (v4l2src->gstbuffer_in_v4l2);
    v4l2src->gstbuffer_in_v4l2 = NULL;
  }
 
  return TRUE;
}

static GstCaps *
gst_imx_v4l2src_get_device_caps (GstBaseSrc * src)
{
  GstImxV4l2Src *v4l2src;
  GstCaps *caps = NULL;

  v4l2src = GST_IMX_V4L2SRC (src);

  if (v4l2src->v4l2handle == NULL) {
    return gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (v4l2src));
  }

  if (v4l2src->probed_caps)
    return gst_caps_ref (v4l2src->probed_caps);

  caps = gst_imx_v4l2_get_caps (v4l2src->v4l2handle);
  if(!caps) {
    GST_WARNING_OBJECT (v4l2src, "Can't get caps from device.");
  }

  v4l2src->probed_caps = gst_caps_ref (caps);

  GST_INFO_OBJECT (v4l2src, "probed caps: %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstCaps *
gst_imx_v4l2src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstCaps *caps = NULL;

  caps = gst_imx_v4l2src_get_device_caps (src);
  if (caps && filter) {
      GstCaps *intersection;

      intersection =
          gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (caps);
      caps = intersection;
  }

  return caps;
}

static GstCaps *
gst_imx_v4l2src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static guint
gst_imx_v4l2_special_fmt (GstCaps *caps)
{
  const GstStructure *structure;
  const char *format;

  structure = gst_caps_get_structure (caps, 0);

  if (gst_structure_has_name (structure, "video/x-bayer")) {
    format = gst_structure_get_string (structure, "format");
    if (g_str_equal (format, "bggr")) {
      return V4L2_PIX_FMT_SBGGR8;
    } else if (g_str_equal (format, "gbrg")) {
      return V4L2_PIX_FMT_SGBRG8;
    } else if (g_str_equal (format, "grbg")) {
      return V4L2_PIX_FMT_SGRBG8;
    } else if (g_str_equal (format, "rggb")) {
      return V4L2_PIX_FMT_SRGGB8;
    }
  }

  return 0;
}

static gboolean
gst_imx_v4l2src_reset (GstImxV4l2Src * v4l2src)
{
  if (v4l2src->pool) {
    gst_object_unref (v4l2src->pool);
    v4l2src->pool = NULL;
    gst_imx_v4l2_reset_device (v4l2src->v4l2handle);
  }

  if (v4l2src->gstbuffer_in_v4l2) {
    g_list_foreach (v4l2src->gstbuffer_in_v4l2, (GFunc) gst_memory_unref, NULL);
    g_list_free (v4l2src->gstbuffer_in_v4l2);
    v4l2src->gstbuffer_in_v4l2 = NULL;
  }

  GST_DEBUG_OBJECT (v4l2src, "gstbuffer_in_v4l2 list free\n");
  v4l2src->stream_on = FALSE;
  v4l2src->actual_buf_cnt = 0;
  v4l2src->use_my_allocator = FALSE;

  return TRUE;
}

static gboolean
gst_imx_v4l2src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstImxV4l2Src *v4l2src;
  GstVideoInfo info;
  guint v4l2fmt;

  v4l2src = GST_IMX_V4L2SRC (src);

  if (v4l2src->old_caps) {
    if (gst_caps_is_equal (v4l2src->old_caps, caps))
      return TRUE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (v4l2src, "invalid caps.");
    return FALSE;
  }

  GST_DEBUG_OBJECT (v4l2src, "set caps %" GST_PTR_FORMAT, caps);

  v4l2fmt = gst_imx_v4l2_fmt_gst2v4l2 (GST_VIDEO_INFO_FORMAT (&info));
  if (!v4l2fmt) {
    v4l2fmt = gst_imx_v4l2_special_fmt (caps);
  }

  v4l2src->v4l2fmt = v4l2fmt;
  v4l2src->w = GST_VIDEO_INFO_WIDTH (&info);
  v4l2src->h = GST_VIDEO_INFO_HEIGHT (&info);
  v4l2src->fps_n = GST_VIDEO_INFO_FPS_N (&info);
  v4l2src->fps_d = GST_VIDEO_INFO_FPS_D (&info);

  if (v4l2src->fps_n <= 0 || v4l2src->fps_d <= 0) {
    GST_ERROR_OBJECT (v4l2src, "invalid fps.");
    return FALSE;
  }
  v4l2src->duration = gst_util_uint64_scale_int (GST_SECOND, v4l2src->fps_d, \
      v4l2src->fps_n);

  if (!gst_imx_v4l2src_reset(v4l2src)) {
    GST_ERROR_OBJECT (v4l2src, "gst_imx_v4l2src_reset failed.");
    return FALSE;
  }

  if (v4l2src->old_caps) {
    gst_caps_unref (v4l2src->old_caps);
    v4l2src->old_caps = NULL;
  }
  v4l2src->old_caps = gst_caps_copy (caps);

  return TRUE;
}

static gboolean
gst_imx_v4l2src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstImxV4l2Src *v4l2src;
  gboolean res = FALSE;

  v4l2src = GST_IMX_V4L2SRC (bsrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      GstClockTime min_latency, max_latency;
      guint32 fps_n, fps_d;
      guint num_buffers = 0;

      if (v4l2src->v4l2handle == NULL) {
        GST_WARNING_OBJECT (v4l2src,
            "Can't give latency since device isn't open !");
        goto done;
      }

      fps_n = v4l2src->fps_n;
      fps_d = v4l2src->fps_d;

      if (fps_n <= 0 || fps_d <= 0) {
        GST_WARNING_OBJECT (v4l2src,
            "Can't give latency since framerate isn't fixated !");
        goto done;
      }

      min_latency = gst_util_uint64_scale_int (GST_SECOND, fps_d, fps_n);

      num_buffers = v4l2src->actual_buf_cnt;

      if (num_buffers == 0)
        max_latency = -1;
      else
        max_latency = num_buffers * min_latency;

      GST_DEBUG_OBJECT (v4l2src,
          "report latency min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
          GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

      gst_query_set_latency (query, TRUE, min_latency, max_latency);

      res = TRUE;
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
      break;
  }

done:

  return res;
}

static gint
gst_imx_v4l2src_config (GstImxV4l2Src *v4l2src)
{
  guint w,h;

  w = v4l2src->w + v4l2src->video_align.padding_left + v4l2src->video_align.padding_right;
  h = v4l2src->h + v4l2src->video_align.padding_top + v4l2src->video_align.padding_bottom;

  GST_DEBUG_OBJECT (v4l2src, "padding: (%d,%d), (%d, %d)", 
      v4l2src->video_align.padding_left, v4l2src->video_align.padding_top, 
      v4l2src->video_align.padding_right, v4l2src->video_align.padding_bottom);

  return gst_imx_v4l2capture_config (v4l2src->v4l2handle, v4l2src->v4l2fmt, w, h, \
      v4l2src->fps_n, v4l2src->fps_d);
}

static gint
gst_imx_v4l2_allocator_cb (gpointer user_data, gint *count)
{
  GstImxV4l2Src *v4l2src = GST_IMX_V4L2SRC (user_data);
  guint min, max;

  if (!v4l2src->pool)
    v4l2src->pool = gst_base_src_get_buffer_pool (GST_BASE_SRC (v4l2src));
  if (v4l2src->pool) {
    GstStructure *config;
    config = gst_buffer_pool_get_config (v4l2src->pool);

    // check if has alignment option setted.
    // if yes, need to recheck the pool params for reconfigure v4l2 devicec.
    memset (&v4l2src->video_align, 0, sizeof(GstVideoAlignment));
    if (gst_buffer_pool_config_has_option (config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
      gst_buffer_pool_config_get_video_alignment (config, &v4l2src->video_align);
      GST_DEBUG_OBJECT (v4l2src, "pool has alignment (%d, %d) , (%d, %d)", 
          v4l2src->video_align.padding_left, v4l2src->video_align.padding_top,
          v4l2src->video_align.padding_right, v4l2src->video_align.padding_bottom);
    }

    gst_buffer_pool_config_get_params (config, NULL, NULL, &min, &max);
    GST_DEBUG_OBJECT (v4l2src, "need allocate %d buffers.\n", max);
    gst_structure_free(config);

    if (gst_imx_v4l2src_config (v4l2src) < 0) {
      GST_ERROR_OBJECT (v4l2src, "camera configuration failed.\n");
      g_printf ("capture device: %s probed caps: %" GST_PTR_FORMAT, v4l2src->device, \
          v4l2src->probed_caps);
      g_printf ("Please config accepted caps!\n");
      return -1;
    }

    if (v4l2src->use_my_allocator) {
      if (gst_imx_v4l2_set_buffer_count (v4l2src->v4l2handle, max, V4L2_MEMORY_MMAP) < 0)
        return -1;
    } else {
      if (gst_imx_v4l2_set_buffer_count (v4l2src->v4l2handle, max, V4L2_MEMORY_USERPTR) < 0)
        return -1;
    }

    *count = max;
  }
  else {
    GST_ERROR_OBJECT (v4l2src, "no pool to get buffer count.\n");
    return -1;
  }

  return 0;
}

static gboolean
gst_imx_v4l2src_decide_allocation (GstBaseSrc * bsrc, GstQuery * query)
{
  GstImxV4l2Src *v4l2src = GST_IMX_V4L2SRC (bsrc);
  IMXV4l2AllocatorContext context;
  GstCaps *outcaps;
  GstBufferPool *pool = NULL;
  guint size, min, max;
  GstAllocator *allocator = NULL;
  GstAllocationParams params;
  GstStructure *config;
  gboolean update_pool, update_allocator;
  GstVideoInfo vinfo;
  const GstStructure *structure;

  if (v4l2src->pool){
    gst_query_parse_allocation (query, &outcaps, NULL);
    gst_video_info_init (&vinfo);
    gst_video_info_from_caps (&vinfo, outcaps);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_set_nth_allocation_pool (query, 0, v4l2src->pool, vinfo.size, v4l2src->actual_buf_cnt, v4l2src->actual_buf_cnt);
  } else {
    gst_query_add_allocation_pool (query, v4l2src->pool, vinfo.size, v4l2src->actual_buf_cnt, v4l2src->actual_buf_cnt);
  }
    return TRUE;
  }

  v4l2src->use_my_allocator = FALSE;

  gst_query_parse_allocation (query, &outcaps, NULL);
  gst_video_info_init (&vinfo);
  gst_video_info_from_caps (&vinfo, outcaps);

  /* we got configuration from our peer or the decide_allocation method,
   * parse them */
  if (gst_query_get_n_allocation_params (query) > 0) {
    /* try the allocator */
    gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
    update_allocator = TRUE;
  } else {
    allocator = NULL;
    gst_allocation_params_init (&params);
    update_allocator = FALSE;
  }

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    size = MAX (size, vinfo.size);
    update_pool = TRUE;
  } else {
    pool = NULL;
    size = vinfo.size;
    min = max = 0;

    update_pool = FALSE;
  }

  if (allocator == NULL \
      || !GST_IS_ALLOCATOR_PHYMEM (allocator) \
      || v4l2src->use_v4l2_memory == TRUE) {
    /* no allocator or isn't physical memory allocator. VPU need continus
     * physical memory. use VPU memory allocator. */
    if (allocator) {
      GST_DEBUG_OBJECT (v4l2src, "unref proposaled allocator.\n");
      gst_object_unref (allocator);
    }
    GST_INFO_OBJECT (v4l2src, "using v4l2 source allocator.\n");

    context.v4l2_handle = v4l2src->v4l2handle;
    context.user_data = (gpointer) v4l2src;
    context.callback = gst_imx_v4l2_allocator_cb;
    allocator = v4l2src->allocator = gst_imx_v4l2_allocator_new (&context);
    if (!v4l2src->allocator) {
      GST_ERROR_OBJECT (v4l2src, "New v4l2 allocator failed.\n");
      return FALSE;
    }
    v4l2src->use_my_allocator = TRUE;
  }

  if (pool == NULL || v4l2src->use_v4l2_memory == TRUE) {
    GstVideoInfo info;
    if (pool) {
      gst_object_unref (pool);
    }
    /* no pool, we can make our own */
    GST_DEBUG_OBJECT (v4l2src, "no pool, making new pool");

    structure = gst_caps_get_structure (v4l2src->old_caps, 0);

    if (!gst_video_info_from_caps (&info, v4l2src->old_caps)) {
      GST_ERROR_OBJECT (v4l2src, "invalid caps.");
      return FALSE;
    }
    size = GST_VIDEO_INFO_SIZE (&info);

    if (gst_structure_has_name (structure, "video/x-bayer")) {
      size = GST_ROUND_UP_4 (v4l2src->w) * v4l2src->h;
      pool = gst_buffer_pool_new ();
    } else
      pool = gst_video_buffer_pool_new ();
  }
  v4l2src->pool = gst_object_ref (pool);

  max = min += DEFAULT_FRAMES_IN_V4L2_CAPTURE \
        + v4l2src->frame_plus;
  if (min > 10)
    max = min = 10;
  v4l2src->actual_buf_cnt = min;

  /* now configure */
  config = gst_buffer_pool_get_config (pool);

  if (!gst_buffer_pool_config_has_option (config, \
        GST_BUFFER_POOL_OPTION_VIDEO_META)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }

  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_set_allocator (config, allocator, &params);
  gst_buffer_pool_set_config (pool, config);

  if (update_allocator)
    gst_query_set_nth_allocation_param (query, 0, allocator, &params);
  else
    gst_query_add_allocation_param (query, allocator, &params);
  if (allocator)
    gst_object_unref (allocator);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  if (pool)
    gst_object_unref (pool);

  return TRUE;
}

static GstFlowReturn
gst_imx_v4l2src_register_buffer (GstImxV4l2Src * v4l2src)
{
  GstFlowReturn ret = GST_FLOW_OK;
  PhyMemBlock *memblk;
  GstBuffer * buffer;
  gint i;

  for (i = 0; i < v4l2src->actual_buf_cnt; i ++) {
    ret = gst_buffer_pool_acquire_buffer (v4l2src->pool, &buffer, NULL);
    if (ret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (v4l2src, "gst_buffer_pool_acquire_buffer failed.");
      return ret;
    }

    memblk = gst_buffer_query_phymem_block(buffer);
    if (!memblk) {
      GST_ERROR_OBJECT (v4l2src, "Can't get physical memory block from gstbuffer.\n");
      return GST_FLOW_ERROR;
    }

    if (gst_imx_v4l2_register_buffer (v4l2src->v4l2handle, memblk) < 0) {
      GST_ERROR_OBJECT (v4l2src, "register buffer failed.");
      return GST_FLOW_ERROR;
    }

    gst_buffer_unref (buffer);
  }

  return ret;
}
 
static GstFlowReturn
gst_imx_v4l2src_acquire_buffer (GstImxV4l2Src * v4l2src, GstBuffer ** buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoFrameFlags flags = GST_VIDEO_FRAME_FLAG_NONE;
  GstVideoMeta *vmeta;
  gint buffer_count;

  if (v4l2src->stream_on == FALSE) {
    if (v4l2src->use_my_allocator == FALSE) {
      if (gst_imx_v4l2_allocator_cb (v4l2src, &buffer_count) < 0) {
        GST_ERROR_OBJECT (v4l2src, "gst_imx_v4l2_allocator_cb failed.");
        return GST_FLOW_ERROR;
      }
      ret = gst_imx_v4l2src_register_buffer (v4l2src);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT (v4l2src, "gst_imx_v4l2_register_buffer failed.");
        return ret;
      }
    } else {
    }
    v4l2src->stream_on = TRUE;
  }

  while (g_list_length (v4l2src->gstbuffer_in_v4l2) \
      < DEFAULT_FRAMES_IN_V4L2_CAPTURE) {
    GstBuffer * buffer;
    ret = gst_buffer_pool_acquire_buffer (v4l2src->pool, &buffer, NULL);
    if (ret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (v4l2src, "gst_buffer_pool_acquire_buffer failed.");
      return ret;
    }
    if (gst_imx_v4l2_queue_gstbuffer (v4l2src->v4l2handle, buffer, flags) < 0) {
      GST_ERROR_OBJECT (v4l2src, "Queue buffer %p failed.", buffer);
      return GST_FLOW_ERROR;
    }
    v4l2src->gstbuffer_in_v4l2 = g_list_append ( \
        v4l2src->gstbuffer_in_v4l2, buffer);
  }
 
  if (gst_imx_v4l2_dequeue_gstbuffer (v4l2src->v4l2handle, buf, &flags) < 0) {
    GST_ERROR_OBJECT (v4l2src, "Dequeue buffer failed.");
    return GST_FLOW_ERROR;
  }
  v4l2src->gstbuffer_in_v4l2 = g_list_remove ( \
      v4l2src->gstbuffer_in_v4l2, *buf);

  vmeta = gst_buffer_get_video_meta (*buf);
  /* If the buffer pool didn't add the meta already
   * we add it ourselves here */
  if (!vmeta) {
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, v4l2src->old_caps)) {
      GST_ERROR_OBJECT (v4l2src, "invalid caps.");
      return GST_FLOW_ERROR;
    }

    vmeta = gst_buffer_add_video_meta (*buf, \
        GST_VIDEO_FRAME_FLAG_NONE, \
        GST_VIDEO_INFO_FORMAT (&info), \
        v4l2src->w, \
        v4l2src->h);
  }

  vmeta->flags = flags;
  GST_DEBUG_OBJECT(v4l2src, "field type: %d\n", flags);

  return ret;
}

static GstFlowReturn
gst_imx_v4l2src_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstImxV4l2Src *v4l2src = GST_IMX_V4L2SRC (src);
  GstFlowReturn ret;
  GstClock *clock;
  GstClockTime abs_time, base_time, timestamp, duration;
  GstClockTime delay;
  GstBuffer *buffer;

  ret = gst_imx_v4l2src_acquire_buffer (v4l2src, buf);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    GST_DEBUG_OBJECT (v4l2src, "error processing buffer %d (%s)", ret,
        gst_flow_get_name (ret));
    return ret;
  }
  buffer = *buf;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = v4l2src->duration;

  GST_OBJECT_LOCK (v4l2src);
  if ((clock = GST_ELEMENT_CLOCK (v4l2src))) {
    base_time = GST_ELEMENT (v4l2src)->base_time;
    gst_object_ref (clock);
  } else {
    base_time = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (v4l2src);

  if (clock) {
    abs_time = gst_clock_get_time (clock);
    gst_object_unref (clock);
  } else {
    abs_time = GST_CLOCK_TIME_NONE;
  }

  if (!GST_CLOCK_TIME_IS_VALID (v4l2src->base_time_org)) {
    v4l2src->base_time_org = base_time;
  }

  GST_DEBUG_OBJECT (v4l2src, "base_time: %" GST_TIME_FORMAT " abs_time: %" 
      GST_TIME_FORMAT, GST_TIME_ARGS (base_time), GST_TIME_ARGS (abs_time));

  if (timestamp != GST_CLOCK_TIME_NONE) {
    struct timespec now;
    GstClockTime gstnow;

    clock_gettime (CLOCK_MONOTONIC, &now);
    gstnow = GST_TIMESPEC_TO_TIME (now);

    if (gstnow < timestamp && (timestamp - gstnow) > (10 * GST_SECOND)) {
      GTimeVal now;

      g_get_current_time (&now);
      gstnow = GST_TIMEVAL_TO_TIME (now);
    }

    if (gstnow > timestamp) {
      delay = gstnow - timestamp;
    } else {
      delay = 0;
    }

    GST_DEBUG_OBJECT (v4l2src, "ts: %" GST_TIME_FORMAT " now %" GST_TIME_FORMAT
        " delay %" GST_TIME_FORMAT, GST_TIME_ARGS (timestamp),
        GST_TIME_ARGS (gstnow), GST_TIME_ARGS (delay));
  } else {
    if (GST_CLOCK_TIME_IS_VALID (duration))
      delay = duration;
    else
      delay = 0;
  }

  if (G_LIKELY (abs_time != GST_CLOCK_TIME_NONE)) {
    /* workaround for base time will change when image capture. */
    timestamp = abs_time - v4l2src->base_time_org;
    if (timestamp > delay)
      timestamp -= delay;
    else
      timestamp = 0;
  } else {
    timestamp = GST_CLOCK_TIME_NONE;
  }

  GST_DEBUG_OBJECT (v4l2src, "timestamp: %" GST_TIME_FORMAT " duration: %" GST_TIME_FORMAT
      , GST_TIME_ARGS (timestamp), GST_TIME_ARGS (duration));

  GST_BUFFER_TIMESTAMP (buffer) = timestamp;
  GST_BUFFER_PTS (buffer) = timestamp;
  GST_BUFFER_DTS (buffer) = timestamp;
  GST_BUFFER_DURATION (buffer) = duration;

  return ret;
}

static void
gst_imx_v4l2src_finalize (GstImxV4l2Src * v4l2src)
{
  if (v4l2src->device)
    g_free (v4l2src->device);

  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (v4l2src));
}

static void
gst_imx_v4l2src_install_properties (GObjectClass *gobject_class)
{
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device", "Device location",
        DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_USE_V4L2SRC_MEMORY,
      g_param_spec_boolean ("use-v4l2src-memory", "Force use V4L2 src memory",
        "Force allocate video frame buffer by V4L2 capture", 
          DEFAULT_USE_V4L2SRC_MEMORY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FRAME_PLUS,
      g_param_spec_uint ("frame-plus", "addtionlal frames",
        "set number of addtional frames for smoothly recording", 
          0, 16, DEFAULT_FRAME_PLUS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  return;
}

static GstCaps*
gst_imx_v4l2src_default_caps ()
{
  GstCaps *caps = gst_caps_new_empty ();
  GstStructure *structure = gst_structure_from_string(GST_VIDEO_CAPS_MAKE("I420"), NULL);
  gst_caps_append_structure (caps, structure);

  return caps;
}

static GstCaps*
gst_imx_v4l2src_get_all_caps ()
{
  GstCaps *caps = gst_imx_v4l2_get_device_caps (V4L2_BUF_TYPE_VIDEO_CAPTURE);
  if(!caps) {
    g_printf ("Can't get caps from capture device, use the default setting.\n");
    g_printf ("Perhaps haven't capture device.\n");
    caps = gst_imx_v4l2src_default_caps ();
  }

  return caps;
}

//type functions

static void
gst_imx_v4l2src_class_init (GstImxV4l2SrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseSrcClass *basesrc_class;
  GstPushSrcClass *pushsrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  basesrc_class = GST_BASE_SRC_CLASS (klass);
  pushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->finalize = (GObjectFinalizeFunc) gst_imx_v4l2src_finalize;
  gobject_class->set_property = gst_imx_v4l2src_set_property;
  gobject_class->get_property = gst_imx_v4l2src_get_property;

  gst_imx_v4l2src_install_properties (gobject_class);

  gst_element_class_add_pad_template (element_class, \
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, \
        gst_imx_v4l2src_get_all_caps ()));

  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_imx_v4l2src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_imx_v4l2src_stop);
  basesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_imx_v4l2src_get_caps);
  basesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_imx_v4l2src_fixate);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_imx_v4l2src_set_caps);
  basesrc_class->query = GST_DEBUG_FUNCPTR (gst_imx_v4l2src_query);
  basesrc_class->decide_allocation = \
      GST_DEBUG_FUNCPTR (gst_imx_v4l2src_decide_allocation);
  pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_imx_v4l2src_create);

  gst_element_class_set_static_metadata (element_class, \
      "IMX Video (video4linux2) Source", "Source/Video", \
      "Capture frames from IMX SoC video4linux2 device", IMX_GST_PLUGIN_AUTHOR);

  GST_DEBUG_CATEGORY_INIT (imxv4l2src_debug, "imxv4l2src", 0, "Freescale IMX V4L2 source element");
}

static void
gst_imx_v4l2src_init (GstImxV4l2Src * v4l2src)
{
  v4l2src->device = g_strdup (DEFAULT_DEVICE);
  v4l2src->frame_plus = DEFAULT_FRAME_PLUS;
  v4l2src->v4l2handle = NULL;
  v4l2src->probed_caps = NULL;
  v4l2src->old_caps = NULL;
  v4l2src->pool = NULL;
  v4l2src->allocator = NULL;
  v4l2src->gstbuffer_in_v4l2 = NULL;
  v4l2src->actual_buf_cnt = 0;
  v4l2src->duration = 0;
  v4l2src->stream_on = FALSE;
  v4l2src->use_my_allocator = FALSE;
  v4l2src->use_v4l2_memory = DEFAULT_USE_V4L2SRC_MEMORY;
  v4l2src->base_time_org = GST_CLOCK_TIME_NONE;

  gst_base_src_set_format (GST_BASE_SRC (v4l2src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (v4l2src), TRUE);

  g_print("====== IMXV4L2SRC: %s build on %s %s. ======\n",  (VERSION),__DATE__,__TIME__);

}

