/* GStreamer IMX video compositor plugin
 * Copyright (c) 2015-2016, Freescale Semiconductor, Inc. All rights reserved.
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

#include <string.h>
#include <gst/allocators/gstphymemmeta.h>
#include <gst/allocators/gstdmabuf.h>
#ifdef USE_ION
#include <gst/allocators/gstionmemory.h>
#endif
#include "gstimxcompositorpad.h"
#include "gstimxcompositor.h"
#include "imx_2d_device.h"

#define ENABLE_OBSCURED_CHECKING

#define DEFAULT_IMXCOMPOSITOR_PAD_XPOS   0
#define DEFAULT_IMXCOMPOSITOR_PAD_YPOS   0
#define DEFAULT_IMXCOMPOSITOR_PAD_WIDTH  0
#define DEFAULT_IMXCOMPOSITOR_PAD_HEIGHT 0
#define DEFAULT_IMXCOMPOSITOR_PAD_ROTATE IMX_2D_ROTATION_0
#define DEFAULT_IMXCOMPOSITOR_PAD_ALPHA  1.0
#define DEFAULT_IMXCOMPOSITOR_PAD_KEEP_RATIO  FALSE
#define SINK_TEMP_BUFFER_INIT_SIZE              (1920*1088*2)

GST_DEBUG_CATEGORY_EXTERN (gst_imxcompositor_debug);

GType gst_imx_compositor_rotation_get_type(void) {
  static GType gst_imx_compositor_rotation_type = 0;

  if (!gst_imx_compositor_rotation_type) {
    static GEnumValue rotation_values[] = {
      {IMX_2D_ROTATION_0, "No rotation", "none"},
      {IMX_2D_ROTATION_90, "Rotate 90 degrees", "rotate-90"},
      {IMX_2D_ROTATION_180, "Rotate 180 degrees", "rotate-180"},
      {IMX_2D_ROTATION_270, "Rotate 270 degrees", "rotate-270"},
      {IMX_2D_ROTATION_HFLIP, "Flip horizontally", "horizontal-flip"},
      {IMX_2D_ROTATION_VFLIP, "Flip vertically", "vertical-flip"},
      {0, NULL, NULL },
    };

    gst_imx_compositor_rotation_type =
        g_enum_register_static("ImxCompositorRotationMode", rotation_values);
  }

  return gst_imx_compositor_rotation_type;
}

enum
{
  PROP_IMXCOMPOSITOR_PAD_0,
  PROP_IMXCOMPOSITOR_PAD_XPOS,
  PROP_IMXCOMPOSITOR_PAD_YPOS,
  PROP_IMXCOMPOSITOR_PAD_WIDTH,
  PROP_IMXCOMPOSITOR_PAD_HEIGHT,
  PROP_IMXCOMPOSITOR_PAD_ALPHA,
  PROP_IMXCOMPOSITOR_PAD_ROTATE,
  PROP_IMXCOMPOSITOR_PAD_DEINTERLACE,
  PROP_IMXCOMPOSITOR_PAD_KEEP_RATIO
};

G_DEFINE_TYPE (GstImxCompositorPad, gst_imxcompositor_pad, \
                GST_TYPE_VIDEO_AGGREGATOR_PAD);

static void
gst_imxcompositor_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstImxCompositorPad *pad = GST_IMXCOMPOSITOR_PAD (object);

  switch (prop_id) {
    case PROP_IMXCOMPOSITOR_PAD_XPOS:
      g_value_set_int (value, pad->xpos);
      break;
    case PROP_IMXCOMPOSITOR_PAD_YPOS:
      g_value_set_int (value, pad->ypos);
      break;
    case PROP_IMXCOMPOSITOR_PAD_WIDTH:
      g_value_set_int (value, pad->width);
      break;
    case PROP_IMXCOMPOSITOR_PAD_HEIGHT:
      g_value_set_int (value, pad->height);
      break;
    case PROP_IMXCOMPOSITOR_PAD_ROTATE:
      g_value_set_enum (value, pad->rotate);
      break;
    case PROP_IMXCOMPOSITOR_PAD_ALPHA:
    {
      GstImxCompositor* comp = (GstImxCompositor*)gst_pad_get_parent(pad);
      if (comp->capabilities & IMX_2D_DEVICE_CAP_ALPHA) {
        g_value_set_double (value, pad->alpha);
      } else {
        g_value_set_double (value, 1.0);
        g_print("!This device don't support alpha blending!\n");
      }
    }
      break;
    case PROP_IMXCOMPOSITOR_PAD_KEEP_RATIO:
      g_value_set_boolean(value, pad->keep_ratio);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_imxcompositor_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstImxCompositorPad *pad = GST_IMXCOMPOSITOR_PAD (object);

  switch (prop_id) {
    case PROP_IMXCOMPOSITOR_PAD_XPOS:
      pad->xpos = g_value_get_int (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_YPOS:
      pad->ypos = g_value_get_int (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_ALPHA:
    {
      GstImxCompositor* comp = (GstImxCompositor*)gst_pad_get_parent(pad);
      if (comp->capabilities & IMX_2D_DEVICE_CAP_ALPHA) {
        pad->alpha = g_value_get_double (value);
      } else {
        pad->alpha = 1.0;
        g_print("!This device don't support alpha blending, "
            "pad alpha setting will be ignored!\n");
      }
      gst_object_unref(comp);
    }
      break;
    case PROP_IMXCOMPOSITOR_PAD_WIDTH:
      pad->width = g_value_get_int (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_HEIGHT:
      pad->height = g_value_get_int (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_ROTATE:
      pad->rotate = g_value_get_enum (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_KEEP_RATIO:
      pad->keep_ratio = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_imxcompositor_pad_finalize (GObject * object)
{
  GstImxCompositorPad *pad = GST_IMXCOMPOSITOR_PAD (object);

  if (pad->sink_tmp_buf) {
    gst_buffer_unref(pad->sink_tmp_buf);
    pad->sink_tmp_buf = NULL;
  }

  if (pad->sink_pool) {
    gst_buffer_pool_set_active (pad->sink_pool, FALSE);
    gst_object_unref (pad->sink_pool);
    pad->sink_pool = NULL;
  }

  G_OBJECT_CLASS (gst_imxcompositor_pad_parent_class)->finalize (object);
}

void
gst_imxcompositor_pad_get_output_size (GstVideoAggregator * vagg,
    GstImxCompositorPad * comp_pad, gint * width, gint * height)
{
  GstVideoAggregatorPad *vagg_pad = GST_VIDEO_AGGREGATOR_PAD (comp_pad);
  gint pad_width, pad_height;
  guint dar_n, dar_d;
  gint v_width, v_height;
  GstVideoCropMeta *in_crop = NULL;

  if (!vagg || !comp_pad || !width || !height)
    return;

  *width = comp_pad->width;
  *height = comp_pad->height;

  if (!vagg_pad->info.finfo || !vagg->info.finfo)
    return;

  v_width = GST_VIDEO_INFO_WIDTH (&vagg_pad->info);
  v_height = GST_VIDEO_INFO_HEIGHT (&vagg_pad->info);

  if (vagg_pad->buffer) {
    in_crop = gst_buffer_get_video_crop_meta(vagg_pad->buffer);
    if (in_crop != NULL) {
      GST_LOG_OBJECT (vagg_pad, "input crop meta: (%d, %d, %d, %d)",
          in_crop->x, in_crop->y, in_crop->width, in_crop->height);
      if ((in_crop->x >= v_width) || (in_crop->y >= v_height)) {
        *width = *height = 0;
        return;
      }

      v_width = MIN(in_crop->width, (v_width - in_crop->x));
      v_height = MIN(in_crop->height, (v_height - in_crop->y));
    }
  }

  pad_width = comp_pad->width <= 0 ? v_width : comp_pad->width;
  pad_height = comp_pad->height <= 0 ? v_height : comp_pad->height;

  gst_video_calculate_display_ratio (&dar_n, &dar_d, pad_width, pad_height,
      GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_D (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_N (&vagg->info), GST_VIDEO_INFO_PAR_D (&vagg->info)
      );

  GST_LOG_OBJECT (comp_pad, "scaling %ux%u by %u/%u (%u/%u / %u/%u)", pad_width,
      pad_height, dar_n, dar_d, GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_D (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_N (&vagg->info), GST_VIDEO_INFO_PAR_D (&vagg->info));

  if (pad_height % dar_n == 0) {
    pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
  } else if (pad_width % dar_d == 0) {
    pad_height = gst_util_uint64_scale_int (pad_width, dar_d, dar_n);
  } else {
    pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
  }

  *width = pad_width;
  *height = pad_height;
}

static gboolean
gst_imxcompositor_pad_set_info (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg G_GNUC_UNUSED,
    GstVideoInfo * current_info, GstVideoInfo * wanted_info)
{
  GstImxCompositor *comp = (GstImxCompositor *)(vagg);
  GstImxCompositorPad *cpad = (GstImxCompositorPad *)(pad);
  gchar *colorimetry, *best_colorimetry;
  const gchar *chroma, *best_chroma;
  gint width, height;

  if (!current_info->finfo)
    return TRUE;

  if (GST_VIDEO_INFO_FORMAT (current_info) == GST_VIDEO_FORMAT_UNKNOWN)
    return TRUE;

  colorimetry = gst_video_colorimetry_to_string (&(current_info->colorimetry));
  chroma = gst_video_chroma_to_string (current_info->chroma_site);

  best_colorimetry =
      gst_video_colorimetry_to_string (&(wanted_info->colorimetry));
  best_chroma = gst_video_chroma_to_string (wanted_info->chroma_site);

  gst_imxcompositor_pad_get_output_size (vagg, cpad, &width, &height);

  if (GST_VIDEO_INFO_FORMAT(wanted_info) != GST_VIDEO_INFO_FORMAT(current_info)
      || g_strcmp0 (colorimetry, best_colorimetry)
      || g_strcmp0 (chroma, best_chroma)
      || width != current_info->width || height != current_info->height) {
    GstVideoInfo tmp_info;

    gst_video_info_set_format (&tmp_info, GST_VIDEO_INFO_FORMAT (wanted_info),
        width, height);
    tmp_info.chroma_site = wanted_info->chroma_site;
    tmp_info.colorimetry = wanted_info->colorimetry;
    tmp_info.par_n = vagg->info.par_n;
    tmp_info.par_d = vagg->info.par_d;
    tmp_info.fps_n = current_info->fps_n;
    tmp_info.fps_d = current_info->fps_d;
    tmp_info.flags = current_info->flags;
    tmp_info.interlace_mode = current_info->interlace_mode;

    GST_DEBUG_OBJECT (pad, "This pad will be converted from %d to %d",
        GST_VIDEO_INFO_FORMAT (current_info),
        GST_VIDEO_INFO_FORMAT (&tmp_info));

    cpad->info = tmp_info;
  } else {
    cpad->info = *current_info;
    GST_DEBUG_OBJECT (pad, "This pad will not need conversion");
  }
  g_free (colorimetry);
  g_free (best_colorimetry);

  return TRUE;
}

static gboolean
is_rectangle_contained (GstVideoRectangle rect1, GstVideoRectangle rect2)
{
  if ((rect2.x <= rect1.x) && (rect2.y <= rect1.y) &&
      ((rect2.x + rect2.w) >= (rect1.x + rect1.w)) &&
      ((rect2.y + rect2.h) >= (rect1.y + rect1.h)))
    return TRUE;
  return FALSE;
}

static GstVideoRectangle
clamp_rectangle (GstVideoRectangle rect, gint outer_width, gint outer_height)
{
  gint x2 = rect.x + rect.w;
  gint y2 = rect.y + rect.h;
  GstVideoRectangle clamped;

  clamped.x = CLAMP (rect.x, 0, outer_width);
  clamped.y = CLAMP (rect.y, 0, outer_height);
  clamped.w = CLAMP (x2, 0, outer_width) - clamped.x;
  clamped.h = CLAMP (y2, 0, outer_height) - clamped.y;

  return clamped;
}

static gboolean
gst_imxcompositor_pad_prepare_frame (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg)
{
  GstImxCompositor *imxcomp = (GstImxCompositor *)(vagg);
  GstImxCompositorPad *cpad = (GstImxCompositorPad *)(pad);
  GstVideoFrame *frame;
  gint width, height;
  GstVideoRectangle video_area;
  GstVideoRectangle clamp;
  gint o_width, o_height;
  GstVideoCropMeta *in_crop = NULL;

  if (!pad->buffer)
    return TRUE;

  gst_imxcompositor_pad_get_output_size (vagg, cpad, &width, &height);

  if (cpad->alpha == 0.0) {
    GST_DEBUG_OBJECT (vagg, "Pad has alpha 0.0, not converting frame");
    pad->aggregated_frame = NULL;
    return TRUE;
  }

  cpad->src_crop.x = 0;
  cpad->src_crop.y = 0;
  cpad->src_crop.w = GST_VIDEO_INFO_WIDTH (&pad->info);
  cpad->src_crop.h = GST_VIDEO_INFO_HEIGHT (&pad->info);

  in_crop = gst_buffer_get_video_crop_meta(pad->buffer);
  if (in_crop != NULL) {
    GST_LOG_OBJECT (pad, "input crop meta: (%d, %d, %d, %d)",
        in_crop->x, in_crop->y, in_crop->width, in_crop->height);
    if ((in_crop->x >= cpad->src_crop.w) || (in_crop->y >= cpad->src_crop.h)) {
      pad->aggregated_frame = NULL;
      return TRUE;
    }

    cpad->src_crop.x = in_crop->x;
    cpad->src_crop.y = in_crop->y;
    cpad->src_crop.w = MIN(in_crop->width, (cpad->src_crop.w - in_crop->x));
    cpad->src_crop.h = MIN(in_crop->height, (cpad->src_crop.h - in_crop->y));
  }

  video_area.x = cpad->xpos;
  video_area.y = cpad->ypos;
  video_area.w = width;
  video_area.h = height;

  if (cpad->keep_ratio) {
    GstVideoRectangle s_rect, d_rect, result;
    s_rect.x = s_rect.y = 0;
    s_rect.w = cpad->src_crop.w;
    s_rect.h = cpad->src_crop.h;
    d_rect.x = d_rect.y = 0;
    d_rect.w = width;
    d_rect.h = height;
    if (cpad->rotate == IMX_2D_ROTATION_90 ||
        cpad->rotate == IMX_2D_ROTATION_270) {
      gint tmp = d_rect.w;
      d_rect.w = d_rect.h;
      d_rect.h = tmp;
    }

    gst_video_sink_center_rect (s_rect, d_rect, &result, TRUE);

    if (cpad->rotate == IMX_2D_ROTATION_90 ||
        cpad->rotate == IMX_2D_ROTATION_270) {
      video_area.x += result.y;
      video_area.y += result.x;
      video_area.w = result.h;
      video_area.h = result.w;
    } else {
      video_area.x += result.x;
      video_area.y += result.y;
      video_area.w = result.w;
      video_area.h = result.h;
    }
  }

  o_width = GST_VIDEO_INFO_WIDTH (&vagg->info);
  o_height = GST_VIDEO_INFO_HEIGHT (&vagg->info);
  clamp = clamp_rectangle (video_area, o_width, o_height);

  if (clamp.w == 0 || clamp.h == 0) {
    GST_DEBUG_OBJECT (vagg, "Resulting frame is zero-width or zero-height "
        "(w: %i, h: %i), skipping", clamp.w, clamp.h);
    pad->aggregated_frame = NULL;
    return TRUE;
  }

  cpad->dst_crop = clamp;
  cpad->src_crop.x = cpad->src_crop.x +
         (cpad->dst_crop.x - video_area.x) * cpad->src_crop.w / video_area.w;
  cpad->src_crop.y = cpad->src_crop.y +
         (cpad->dst_crop.y - video_area.y) * cpad->src_crop.h / video_area.h;
  cpad->src_crop.w = cpad->dst_crop.w * cpad->src_crop.w / video_area.w;
  cpad->src_crop.h = cpad->dst_crop.h * cpad->src_crop.h / video_area.h;

#ifdef ENABLE_OBSCURED_CHECKING
  gboolean frame_obscured = FALSE;
  GList *l;

  GST_OBJECT_LOCK (vagg);
  /* Check if this frame is obscured by a higher-zorder frame */
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoRectangle frame2_rect;
    GstVideoAggregatorPad *pad2 = l->data;
    GstImxCompositorPad *cpad2 = (GstImxCompositorPad *)(pad2);
    gint pad2_width, pad2_height;

    if (pad2->zorder > pad->zorder && pad2->buffer && cpad2->alpha == 1.0 &&
        !GST_VIDEO_INFO_HAS_ALPHA (&pad2->info)) {
      gst_imxcompositor_pad_get_output_size (vagg, cpad2,
                                              &pad2_width, &pad2_height);

      frame2_rect.x = cpad2->xpos;
      frame2_rect.y = cpad2->ypos;
      frame2_rect.w = pad2_width;
      frame2_rect.h = pad2_height;

      if (is_rectangle_contained (clamp, frame2_rect)) {
        frame_obscured = TRUE;
        GST_DEBUG_OBJECT (pad, "%ix%i@(%i,%i) obscured by %s %ix%i@(%i,%i) "
            "in output of size %ix%i; skipping frame", clamp.w, clamp.h,
            clamp.x, clamp.y, GST_PAD_NAME (pad2), frame2_rect.w,
            frame2_rect.h, frame2_rect.x, frame2_rect.y, o_width, o_height);
        break;
      }
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  if (frame_obscured) {
    pad->aggregated_frame = NULL;
    return TRUE;
  }
#endif

  frame = g_slice_new0 (GstVideoFrame);

#if GST_CHECK_VERSION(1, 14, 0)
  if (!gst_video_frame_map (frame, &pad->info, pad->buffer,
#else
  if (!gst_video_frame_map (frame, &pad->buffer_vinfo, pad->buffer,
#endif
          GST_MAP_READ)) {

    GST_WARNING_OBJECT (vagg, "Could not map input buffer");
    g_slice_free (GstVideoFrame, frame);
    return FALSE;
  }

  /* Check if need copy input frame */
  if (!(gst_buffer_is_phymem(pad->buffer)
        || gst_is_dmabuf_memory (gst_buffer_peek_memory (pad->buffer, 0)))) {
    GST_DEBUG_OBJECT (pad, "copy input frame to physical continues memory");
    GstVideoInfo info;
    GstCaps *caps = gst_video_info_to_caps(&frame->info);
    gst_video_info_from_caps(&info, caps); //update the size info
    gst_caps_unref(caps);

    if (!imxcomp->allocator) {
#ifdef USE_ION
      imxcomp->allocator = gst_ion_allocator_obtain ();
#endif
    }

    if (!imxcomp->allocator)
      imxcomp->allocator =
          gst_imx_2d_device_allocator_new((gpointer)(imxcomp->device));

    if (!cpad->sink_tmp_buf) {
      cpad->sink_tmp_buf = gst_buffer_new_allocate(imxcomp->allocator,
          SINK_TEMP_BUFFER_INIT_SIZE, NULL);
      cpad->sink_tmp_buf_size = SINK_TEMP_BUFFER_INIT_SIZE;
    }

    if (cpad->sink_tmp_buf && info.size > SINK_TEMP_BUFFER_INIT_SIZE) {
      if (cpad->sink_tmp_buf)
        gst_buffer_unref(cpad->sink_tmp_buf);
      cpad->sink_tmp_buf = gst_buffer_new_allocate(imxcomp->allocator,
          info.size, NULL);
      cpad->sink_tmp_buf_size = info.size;
    }

    if (cpad->sink_tmp_buf) {
      GstVideoFrame *copy_frame = g_slice_new0 (GstVideoFrame);
      gst_video_frame_map(copy_frame, &info, cpad->sink_tmp_buf, GST_MAP_WRITE);
      gst_video_frame_copy(copy_frame, frame);
      gst_video_frame_unmap (frame);
      g_slice_free (GstVideoFrame, frame);
      frame = copy_frame;

      if (imxcomp->composition_meta_enable
              && imx_video_overlay_composition_has_meta(pad->buffer)) {
        imx_video_overlay_composition_remove_meta(cpad->sink_tmp_buf);
        imx_video_overlay_composition_copy_meta(cpad->sink_tmp_buf, pad->buffer,
            frame->info.width, frame->info.height,
            frame->info.width, frame->info.height);
      }
    } else {
      GST_ERROR_OBJECT (pad,
          "Can't get input buffer,ignore this frame,continue next");
      gst_video_frame_unmap (frame);
      g_slice_free (GstVideoFrame, frame);
      pad->aggregated_frame = NULL;
      return TRUE;
    }
  }

  if (cpad->sink_pool_update) {
    memset (&cpad->align, 0, sizeof(GstVideoAlignment));
    if (cpad->sink_pool && gst_buffer_pool_is_active (cpad->sink_pool)) {
      GstStructure *config = gst_buffer_pool_get_config (cpad->sink_pool);

      if (gst_buffer_pool_config_has_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
        gst_buffer_pool_config_get_video_alignment (config, &cpad->align);
        GST_DEBUG_OBJECT (pad, "input pool has alignment (%d, %d) , (%d, %d)",
            cpad->align.padding_left, cpad->align.padding_top,
            cpad->align.padding_right, cpad->align.padding_bottom);
      }
      gst_structure_free (config);
    } else {
      GstPhyMemMeta *phymemmeta = GST_PHY_MEM_META_GET (pad->buffer);
      if (phymemmeta) {
        cpad->align.padding_right = phymemmeta->x_padding;
        cpad->align.padding_bottom = phymemmeta->y_padding;
        GST_DEBUG_OBJECT (pad, "physical memory meta x_padding: %d "
            "y_padding: %d",phymemmeta->x_padding, phymemmeta->y_padding);
      }
    }
    cpad->sink_pool_update = FALSE;
  }

  GstSegment *seg = &((GstAggregatorPad*)pad)->segment;
  GstClockTime timestamp = GST_BUFFER_TIMESTAMP (pad->buffer);
  gint64 stream_time =
      gst_segment_to_stream_time (seg, GST_FORMAT_TIME, timestamp);
  /* sync object properties on stream time */
  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (pad), stream_time);

  pad->aggregated_frame = frame;

  return TRUE;
}

static void
gst_imxcompositor_pad_clean_frame (GstVideoAggregatorPad * pad,
    GstVideoAggregator * vagg)
{
  if (pad->aggregated_frame) {
    gst_video_frame_unmap (pad->aggregated_frame);
    g_slice_free (GstVideoFrame, pad->aggregated_frame);
    pad->aggregated_frame = NULL;
  }
}

static void
gst_imxcompositor_pad_class_init (GstImxCompositorPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoAggregatorPadClass *vaggpadclass =
      (GstVideoAggregatorPadClass *) klass;

  gobject_class->set_property = gst_imxcompositor_pad_set_property;
  gobject_class->get_property = gst_imxcompositor_pad_get_property;
  gobject_class->finalize = gst_imxcompositor_pad_finalize;

  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_XPOS,
      g_param_spec_int ("xpos", "X Position", "X Position of the picture",
          G_MININT32, G_MAXINT32, DEFAULT_IMXCOMPOSITOR_PAD_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_YPOS,
      g_param_spec_int ("ypos", "Y Position", "Y Position of the picture",
          G_MININT32, G_MAXINT32, DEFAULT_IMXCOMPOSITOR_PAD_YPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_WIDTH,
      g_param_spec_int ("width", "Width", "Target width of the picture",
          G_MININT32, G_MAXINT32, DEFAULT_IMXCOMPOSITOR_PAD_WIDTH,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_HEIGHT,
      g_param_spec_int ("height", "Height", "Target height of the picture",
          G_MININT32, G_MAXINT32, DEFAULT_IMXCOMPOSITOR_PAD_HEIGHT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_ROTATE,
      g_param_spec_enum("rotate", "input rotation",
        "Rotation that shall be applied to input frames",
        gst_imx_compositor_rotation_get_type(),
        DEFAULT_IMXCOMPOSITOR_PAD_ROTATE,
        G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_ALPHA,
      g_param_spec_double ("alpha", "Alpha", "Alpha of the picture", 0.0, 1.0,
          DEFAULT_IMXCOMPOSITOR_PAD_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_IMXCOMPOSITOR_PAD_KEEP_RATIO,
      g_param_spec_boolean ("keep-ratio", "Keep Aspect Ratio",
          "Keep the video aspect ratio after resize",
          DEFAULT_IMXCOMPOSITOR_PAD_KEEP_RATIO,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  vaggpadclass->set_info = GST_DEBUG_FUNCPTR (gst_imxcompositor_pad_set_info);
  vaggpadclass->prepare_frame =
      GST_DEBUG_FUNCPTR (gst_imxcompositor_pad_prepare_frame);
  vaggpadclass->clean_frame =
      GST_DEBUG_FUNCPTR (gst_imxcompositor_pad_clean_frame);
}

static void
gst_imxcompositor_pad_init (GstImxCompositorPad * compo_pad)
{
  compo_pad->xpos = DEFAULT_IMXCOMPOSITOR_PAD_XPOS;
  compo_pad->ypos = DEFAULT_IMXCOMPOSITOR_PAD_YPOS;
  compo_pad->width = DEFAULT_IMXCOMPOSITOR_PAD_WIDTH;
  compo_pad->height = DEFAULT_IMXCOMPOSITOR_PAD_HEIGHT;
  compo_pad->rotate = DEFAULT_IMXCOMPOSITOR_PAD_ROTATE;
  compo_pad->alpha = DEFAULT_IMXCOMPOSITOR_PAD_ALPHA;
  compo_pad->keep_ratio = DEFAULT_IMXCOMPOSITOR_PAD_KEEP_RATIO;
  compo_pad->sink_pool = NULL;
  compo_pad->sink_tmp_buf = NULL;
  compo_pad->sink_tmp_buf_size = 0;
  memset(&compo_pad->align, 0, sizeof(GstVideoAlignment));
}
