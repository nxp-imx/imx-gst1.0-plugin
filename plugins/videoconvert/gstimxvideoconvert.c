/* GStreamer IMX video convert plugin
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2017-2020 NXP
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

#include <gst/video/video.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/allocators/gstdmabufmeta.h>
#include <libdrm/drm_fourcc.h>
#include <gst/allocators/gstallocatorphymem.h>
#ifdef USE_DMABUFHEAPS
#include <gst/allocators/gstdmabufheaps.h>
#endif
#ifdef USE_ION
#include <gst/allocators/gstionmemory.h>
#endif
#include <gst/allocators/gstphymemmeta.h>
#include "gstimxvideoconvert.h"

#define IMX_VCT_IN_POOL_MAX_BUFFERS   30

#define GST_IMX_VCT_PARAMS_QDATA   g_quark_from_static_string("imxvct-params")

#define GST_IMX_VIDEO_ROTATION_DEFAULT      IMX_2D_ROTATION_0
#define GST_IMX_VIDEO_DEINTERLACE_DEFAULT   IMX_2D_DEINTERLACE_NONE
#define GST_IMX_VIDEO_COMPOMETA_DEFAULT              FALSE
#define GST_IMX_VIDEO_COMPOMETA_IN_PLACE_DEFAULT     FALSE
#define GST_IMX_VIDEO_VIDEOCROP_META_DEFAULT         FALSE
#define GST_IMX_VIDEO_WARP_DEFAULT                   FALSE
#define GST_IMX_VIDEO_WARP_MAP_DEFAULT               IMX_2D_WARP_MAP_NULL
#define GST_IMX_VIDEO_KEEP_RATIO_DEFAULT             FALSE

#define DEFAULT_IMXVIDEOCONVERT_BACKGROUND   0x00000000

#define GST_IMX_CONVERT_UNREF_BUFFER(buffer) {\
    if (buffer) {                             \
      GST_LOG ("unref buffer (%p)", buffer);  \
      gst_buffer_unref(buffer);               \
      buffer = NULL;                          \
    }                                         \
  }

#define GST_IMX_CONVERT_UNREF_POOL(pool)  {   \
    if (pool) {                               \
      GST_LOG ("unref pool (%p)", pool);      \
      gst_buffer_pool_set_active (pool, FALSE);\
      gst_object_unref(pool);                 \
      pool = NULL;                            \
    }                                         \
  }

/* properties utility*/
enum {
  PROP_0,
  PROP_OUTPUT_ROTATE,
  PROP_DEINTERLACE_MODE,
  PROP_COMPOSITION_META_ENABLE,
  PROP_COMPOSITION_META_IN_PLACE,
  PROP_VIDEOCROP_META_ENABLE,
  PROP_VIDEO_WARP_ENABLE,
  PROP_VIDEO_WARP_COORD_FILE,
  PROP_VIDEO_WARP_EXTRA_CONTROLS,
  PROP_KEEP_RATIO
};

static GstElementClass *parent_class = NULL;

GST_DEBUG_CATEGORY (imxvideoconvert_debug);
#define GST_CAT_DEFAULT imxvideoconvert_debug

GType gst_imx_video_convert_rotation_get_type(void) {
  static GType gst_imx_video_convert_rotation_type = 0;

  if (!gst_imx_video_convert_rotation_type) {
    static GEnumValue rotation_values[] = {
      {IMX_2D_ROTATION_0, "No rotation", "none"},
      {IMX_2D_ROTATION_90, "Rotate 90 degrees", "rotate-90"},
      {IMX_2D_ROTATION_180, "Rotate 180 degrees", "rotate-180"},
      {IMX_2D_ROTATION_270, "Rotate 270 degrees", "rotate-270"},
      {IMX_2D_ROTATION_HFLIP, "Flip horizontally", "horizontal-flip"},
      {IMX_2D_ROTATION_VFLIP, "Flip vertically", "vertical-flip"},
      {0, NULL, NULL },
    };

    gst_imx_video_convert_rotation_type =
        g_enum_register_static("ImxVideoConvertRotationMode", rotation_values);
  }

  return gst_imx_video_convert_rotation_type;
}

GType gst_imx_video_convert_deinterlace_get_type(void) {
  static GType gst_imx_video_convert_deinterlace_type = 0;

  if (!gst_imx_video_convert_deinterlace_type) {
    static GEnumValue deinterlace_values[] = {
      { IMX_2D_DEINTERLACE_NONE, "No deinterlace", "none" },
      { IMX_2D_DEINTERLACE_LOW_MOTION,
          "low-motion deinterlace", "low-motion" },
      { IMX_2D_DEINTERLACE_MID_MOTION,
          "midium-motion deinterlace", "mid-motion" },
      { IMX_2D_DEINTERLACE_HIGH_MOTION,
          "high-motion deinterlace", "high-motion" },
      { 0, NULL, NULL },
    };

    gst_imx_video_convert_deinterlace_type =
        g_enum_register_static("ImxVideoConvertDeinterlaceMode",
                                deinterlace_values);
  }

  return gst_imx_video_convert_deinterlace_type;
}

static void gst_imx_video_convert_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *) (object);
  Imx2DDevice *device = imxvct->device;

  GST_DEBUG_OBJECT (imxvct, "set_property (%d).", prop_id);

  if (!device)
    return;

  switch (prop_id) {
    case PROP_OUTPUT_ROTATE:
      imxvct->rotate = g_value_get_enum (value);
      break;
    case PROP_DEINTERLACE_MODE:
      imxvct->deinterlace = g_value_get_enum (value);
      break;
    case PROP_COMPOSITION_META_ENABLE:
      imxvct->composition_meta_enable = g_value_get_boolean(value);
      break;
    case PROP_COMPOSITION_META_IN_PLACE:
      imxvct->in_place = g_value_get_boolean(value);
      break;
    case PROP_VIDEOCROP_META_ENABLE:
      imxvct->videocrop_meta_enable = g_value_get_boolean(value);
      break;
    case PROP_VIDEO_WARP_ENABLE:
      imxvct->video_warp.enable = g_value_get_boolean(value);
      device->config_warp_info (device, &imxvct->video_warp);
      break;
    case PROP_VIDEO_WARP_COORD_FILE:
      const gchar * location = g_value_get_string (value);
      if (location != NULL) {
        if (imxvct->video_warp.filename)
          g_free (imxvct->video_warp.filename);
        imxvct->video_warp.filename = g_strdup (location);

        if (imx_2d_device_read_warp_coordinates_file (device,
            imxvct->video_warp.filename, &imxvct->video_warp)) {
          GST_DEBUG_OBJECT (imxvct, "probe coordinate file success");
          device->config_warp_info (device, &imxvct->video_warp);
        }
      }
      break;
    case PROP_KEEP_RATIO:
      imxvct->keep_ratio = g_value_get_boolean(value);
      break;
    case PROP_VIDEO_WARP_EXTRA_CONTROLS:
      const GstStructure *config = gst_value_get_structure (value);
      if (imxvct->video_warp.extra_controls)
        gst_structure_free (imxvct->video_warp.extra_controls);

      imxvct->video_warp.extra_controls = config ? gst_structure_copy (config) : NULL;
      imx_2d_device_set_warp_controls (config, &imxvct->video_warp);
      device->config_warp_info (device, &imxvct->video_warp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  //TODO if property changed, it may affect the passthrough, so we need
  // reconfig the pipeline, send a reconfig event for caps re-negotiation.
}

static void gst_imx_video_convert_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *) (object);
  Imx2DDevice *device = imxvct->device;

  if (!device)
    return;

  switch (prop_id) {
    case PROP_OUTPUT_ROTATE:
      g_value_set_enum (value, imxvct->rotate);
      break;
    case PROP_DEINTERLACE_MODE:
      g_value_set_enum (value, imxvct->deinterlace);
      break;
    case PROP_COMPOSITION_META_ENABLE:
      g_value_set_boolean(value, imxvct->composition_meta_enable);
      break;
    case PROP_COMPOSITION_META_IN_PLACE:
      g_value_set_boolean(value, imxvct->in_place);
      break;
    case PROP_VIDEOCROP_META_ENABLE:
      g_value_set_boolean(value, imxvct->videocrop_meta_enable);
      break;
    case PROP_VIDEO_WARP_ENABLE:
      g_value_set_boolean(value, imxvct->video_warp.enable);
      break;
    case PROP_VIDEO_WARP_COORD_FILE:
      g_value_set_string (value, imxvct->video_warp.filename);
      break;
    case PROP_KEEP_RATIO:
      g_value_set_boolean(value, imxvct->keep_ratio);
      break;
    case PROP_VIDEO_WARP_EXTRA_CONTROLS:
      gst_value_set_structure (value, imxvct->video_warp.extra_controls);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void gst_imx_video_convert_finalize (GObject * object)
{
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *) (object);
  GstImxVideoConvertClass *klass =
        (GstImxVideoConvertClass *) G_OBJECT_GET_CLASS (imxvct);

  GST_INFO_OBJECT (imxvct, "imx_video_convert_result, frames:%" G_GUINT64_FORMAT
      " time:%" G_GUINT64_FORMAT " fps:%.3f\n",
      imxvct->total_frames, imxvct->total_time,
      (imxvct->total_time != 0) ? ((gfloat)1000000* imxvct->total_frames / imxvct->total_time): 0);

  imx_video_overlay_composition_deinit(&imxvct->video_comp);

  GST_IMX_CONVERT_UNREF_BUFFER (imxvct->in_buf);
  GST_IMX_CONVERT_UNREF_POOL (imxvct->in_pool);
  GST_IMX_CONVERT_UNREF_POOL (imxvct->self_out_pool);
  if (imxvct->allocator) {
    gst_object_unref (imxvct->allocator);
    imxvct->allocator = NULL;
  }

  if (imxvct->video_warp.extra_controls)
    gst_structure_free (imxvct->video_warp.extra_controls);

  if (imxvct->video_warp.coordinates_mem.size) {
    imxvct->device->free_mem (imxvct->device, &imxvct->video_warp.coordinates_mem);
  }
  g_free (imxvct->video_warp.filename);

  if (imxvct->device) {
    imxvct->device->close(imxvct->device);
    if (klass->in_plugin)
      klass->in_plugin->destroy(imxvct->device);
    imxvct->device = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (imxvct));
}

static gboolean
imx_video_convert_src_event(GstBaseTransform *transform, GstEvent *event)
{
  gdouble a;
  GstStructure *structure;
  GstVideoFilter *filter = GST_VIDEO_FILTER_CAST(transform);

  GST_TRACE("%s event", GST_EVENT_TYPE_NAME(event));

  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_NAVIGATION:
      if ((filter->in_info.width != filter->out_info.width) ||
          (filter->in_info.height != filter->out_info.height)) {
        event =
            GST_EVENT(gst_mini_object_make_writable(GST_MINI_OBJECT(event)));

        structure = (GstStructure *)gst_event_get_structure(event);
        if (gst_structure_get_double(structure, "pointer_x", &a)) {
          gst_structure_set(
            structure, "pointer_x", G_TYPE_DOUBLE,
            a * filter->in_info.width / filter->out_info.width,
            NULL
          );
        }

        if (gst_structure_get_double(structure, "pointer_y", &a)) {
          gst_structure_set(
            structure, "pointer_y", G_TYPE_DOUBLE,
            a * filter->in_info.height / filter->out_info.height,
            NULL
          );
        }
      }
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS(parent_class)->src_event(transform, event);
}

static GstCaps *imx_video_convert_add_drm_caps(GstCaps *caps, GList* list)
{
  gint i, caps_size;
  GstCapsFeatures *has_f;
  GstStructure *st;
  gboolean has_drm_caps = FALSE;
  GstCaps *new_caps = NULL;
  GValue dma_drm_fmts = G_VALUE_INIT;
  GValue gval = G_VALUE_INIT;
  gchar *drm_fmt_str;

  /* Check whether drm caps is added already */
  if (caps && !gst_caps_is_empty(caps)) {
    caps_size = gst_caps_get_size(caps);

    for (i = 0; i < caps_size; i++) {
      has_f = gst_caps_get_features(caps, i);
      st = gst_caps_get_structure(caps, i);

      if (has_f && gst_caps_features_contains(has_f,
          GST_CAPS_FEATURE_MEMORY_DMABUF)) {
        has_drm_caps = TRUE;
        break;
      }

      if (!g_strcmp0 (gst_structure_get_string (st, "format"), "DMA_DRM")) {
        GstCapsFeatures *f =
          gst_caps_features_new_static_str (GST_CAPS_FEATURE_MEMORY_DMABUF, NULL);
        gst_caps_set_features (caps, i, f);
        has_drm_caps = TRUE;
        break;
      }
    }
  }

  if (!has_drm_caps) {
    g_value_init (&dma_drm_fmts, GST_TYPE_LIST);
    g_value_init (&gval, G_TYPE_STRING);

    for (i = 0; i < g_list_length (list); i++) {
      GstVideoFormat fmt = (GstVideoFormat)g_list_nth_data(list, i);
      guint64 modifier;
      guint32 drm_fourcc;

      drm_fourcc = gst_video_dma_drm_format_from_gst_format (fmt, &modifier);
      if (drm_fourcc == DRM_FORMAT_INVALID)
        continue;

      drm_fmt_str =
          gst_video_dma_drm_fourcc_to_string (drm_fourcc, modifier);
      if (!drm_fmt_str)
        continue;

      g_value_set_string (&gval, drm_fmt_str);
      gst_value_list_append_value (&dma_drm_fmts, &gval);
    }

    new_caps = gst_caps_from_string ("video/x-raw(memory:DMABuf)");
    gst_caps_set_simple (new_caps, "format", G_TYPE_STRING, "DMA_DRM", NULL);
    if (gst_value_list_get_size (&dma_drm_fmts) > 0) {
      gst_caps_set_value (new_caps, "drm-format", &dma_drm_fmts);
    }
    g_value_unset (&dma_drm_fmts);
    g_value_unset (&gval);

    if (!gst_caps_is_subset(new_caps, caps)) {
      gst_caps_append (new_caps, caps);
      return new_caps;
    } else {
      gst_caps_unref(new_caps);
    }
  }

  return caps;
}

static GstCaps* imx_video_convert_transform_caps(GstBaseTransform *transform,
                     GstPadDirection direction, GstCaps *caps, GstCaps *filter)
{
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *) (transform);
  GstCaps *tmp, *tmp2, *result, *drm_caps;
  GstStructure *st;
  gint i, n;
  GstCapsFeatures *f;

  GST_DEBUG("transform caps: %" GST_PTR_FORMAT, caps);
  GST_DEBUG("filter: %" GST_PTR_FORMAT, filter);
  GST_DEBUG("direction: %d", direction);

  /* Get all possible caps that we can transform to */
  /* copies the given caps */
  tmp = gst_caps_new_empty();
  n = gst_caps_get_size(caps);

  for (i = 0; i < n; i++) {
    st = gst_caps_get_structure(caps, i);

    if ((i > 0) && gst_caps_is_subset_structure(tmp, st))
      continue;

    st = gst_structure_copy(st);

    /* NV12 8x8 to YUY2 works on DPU */
    if (HAS_DPU()) {
      gst_structure_set(st, "width", GST_TYPE_INT_RANGE, 8, G_MAXINT32,
          "height", GST_TYPE_INT_RANGE, 8, G_MAXINT32, NULL);
    } else {
      if (imxvct->device->device_type == IMX_2D_DEVICE_G2D ||
          imxvct->device->device_type == IMX_2D_DEVICE_PXP)
        gst_structure_set(st, "width", GST_TYPE_INT_RANGE, 16, G_MAXINT32,
            "height", GST_TYPE_INT_RANGE, 16, G_MAXINT32, NULL);
      else
        gst_structure_set(st, "width", GST_TYPE_INT_RANGE, 64, G_MAXINT32,
            "height", GST_TYPE_INT_RANGE, 64, G_MAXINT32, NULL);
    }

    if (!g_strcmp0 (gst_structure_get_string (st, "format"), "DMA_DRM")) {
      gst_structure_remove_fields(st, "drm-format", NULL);
    } else {
      gst_structure_remove_fields(st, "format", NULL);
    }

    /* Remove colorimetry field to support colorimetry conversion
     * if the input and output are different.
     */
    if (gst_structure_has_field (st, "colorimetry"))
      gst_structure_remove_field (st, "colorimetry");

    /* if pixel aspect ratio, make a range of it*/
    if (gst_structure_has_field(st, "pixel-aspect-ratio")) {
      gst_structure_set(st, "pixel-aspect-ratio",
          GST_TYPE_FRACTION_RANGE, 1, G_MAXINT32, G_MAXINT32, 1, NULL);
    }

    /* Check and append DRM feature if needed */
    if (!g_strcmp0 (gst_structure_get_string (st, "format"), "DMA_DRM")) {
      drm_caps = gst_caps_new_empty();
      gst_caps_append_structure(drm_caps, st);
      f = gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_DMABUF, NULL);
      gst_caps_set_features(drm_caps, 0, f);
      gst_caps_append (tmp, drm_caps);
    } else {
      gst_caps_append_structure(tmp, st);
    }
  }
  imx_video_overlay_composition_add_caps (tmp);

  GST_DEBUG("transformed: %" GST_PTR_FORMAT, tmp);

  if (filter) {
    tmp2 = gst_caps_intersect_full(filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref(tmp);
    tmp = tmp2;
  }

  result = tmp;

  GST_DEBUG("return caps: %" GST_PTR_FORMAT, result);

  return result;
}

#ifdef COMPARE_CONVERT_LOSS
/* calculate how much loss a conversion would be */
/* This loss calculation comes from gstvideoconvert.c of base plugins */
static gint get_format_conversion_loss(GstBaseTransform * base,
                                       GstVideoFormat in_name,
                                       GstVideoFormat out_name)
{
#define SCORE_FORMAT_CHANGE       1
#define SCORE_DEPTH_CHANGE        1
#define SCORE_ALPHA_CHANGE        1
#define SCORE_CHROMA_W_CHANGE     1
#define SCORE_CHROMA_H_CHANGE     1
#define SCORE_PALETTE_CHANGE      1

#define SCORE_COLORSPACE_LOSS     2     /* RGB <-> YUV */
#define SCORE_DEPTH_LOSS          4     /* change bit depth */
#define SCORE_ALPHA_LOSS          8     /* lose the alpha channel */
#define SCORE_CHROMA_W_LOSS      16     /* vertical sub-sample */
#define SCORE_CHROMA_H_LOSS      32     /* horizontal sub-sample */
#define SCORE_PALETTE_LOSS       64     /* convert to palette format */
#define SCORE_COLOR_LOSS        128     /* convert to GRAY */

#define COLORSPACE_MASK (GST_VIDEO_FORMAT_FLAG_YUV | \
                         GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_GRAY)
#define ALPHA_MASK      (GST_VIDEO_FORMAT_FLAG_ALPHA)
#define PALETTE_MASK    (GST_VIDEO_FORMAT_FLAG_PALETTE)

  gint loss = G_MAXINT32;
  GstVideoFormatFlags in_flags, out_flags;
  const GstVideoFormatInfo *in_info = gst_video_format_get_info(in_name);
  const GstVideoFormatInfo *out_info = gst_video_format_get_info(out_name);
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(base);
  Imx2DDevice *device = imxvct->device;

  if (!in_info || !out_info)
    return G_MAXINT32;

  in_flags = GST_VIDEO_FORMAT_INFO_FLAGS (in_info);
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_LE;
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_COMPLEX;
  in_flags &= ~GST_VIDEO_FORMAT_FLAG_UNPACK;

  out_flags = GST_VIDEO_FORMAT_INFO_FLAGS (out_info);
  out_flags &= ~GST_VIDEO_FORMAT_FLAG_LE;
  out_flags &= ~GST_VIDEO_FORMAT_FLAG_COMPLEX;
  out_flags &= ~GST_VIDEO_FORMAT_FLAG_UNPACK;

  /* when enable compistion meta, need limite output format to RGB */
  if ((out_flags & COLORSPACE_MASK) == GST_VIDEO_FORMAT_FLAG_YUV
      && imxvct->composition_meta_enable) {
    return G_MAXINT32;
  }

  /* accept input format immediately without loss */
  if (in_info == out_info) {
    GST_LOG("same format %s", GST_VIDEO_FORMAT_INFO_NAME(in_info));
    return 0;
  }

  loss = SCORE_FORMAT_CHANGE;

  /* Reduce the priority of NV12 output on 8q platform */
  if (IS_IMX8Q() && device->device_type == IMX_2D_DEVICE_G2D) {
    if (out_name == GST_VIDEO_FORMAT_NV12)
      loss = G_MAXINT32 - 1;
    goto done;
  }

  if ((out_flags & PALETTE_MASK) != (in_flags & PALETTE_MASK)) {
    loss += SCORE_PALETTE_CHANGE;
    if (out_flags & PALETTE_MASK)
      loss += SCORE_PALETTE_LOSS;
  }

  if ((out_flags & COLORSPACE_MASK) != (in_flags & COLORSPACE_MASK)) {
    loss += SCORE_COLORSPACE_LOSS;
    if (out_flags & GST_VIDEO_FORMAT_FLAG_GRAY)
      loss += SCORE_COLOR_LOSS;
  }

  if ((out_flags & ALPHA_MASK) != (in_flags & ALPHA_MASK)) {
    loss += SCORE_ALPHA_CHANGE;
    if (in_flags & ALPHA_MASK)
      loss += SCORE_ALPHA_LOSS;
  }

  if ((in_info->h_sub[1]) != (out_info->h_sub[1])) {
    loss += SCORE_CHROMA_H_CHANGE;
    if ((in_info->h_sub[1]) < (out_info->h_sub[1]))
      loss += SCORE_CHROMA_H_LOSS;
  }
  if ((in_info->w_sub[1]) != (out_info->w_sub[1])) {
    loss += SCORE_CHROMA_W_CHANGE;
    if ((in_info->w_sub[1]) < (out_info->w_sub[1]))
      loss += SCORE_CHROMA_W_LOSS;
  }

  if ((in_info->bits) != (out_info->bits)) {
    loss += SCORE_DEPTH_CHANGE;
    if ((in_info->bits) > (out_info->bits))
      loss += SCORE_DEPTH_LOSS;
  }

done:
  GST_LOG("%s -> %s, loss = %d", GST_VIDEO_FORMAT_INFO_NAME(in_info),
                  GST_VIDEO_FORMAT_INFO_NAME(out_info), loss);
  return loss;
}
#endif

static GstCaps* imx_video_convert_caps_from_fmt_list(GList* list)
{
  gint i;
  GstCaps *caps = NULL;

  for (i=0; i<g_list_length (list); i++) {
    GstVideoFormat fmt = (GstVideoFormat)g_list_nth_data(list, i);

    if (caps) {
      GstCaps *newcaps = gst_caps_new_simple("video/x-raw",
          "format", G_TYPE_STRING, gst_video_format_to_string(fmt), NULL);
      gst_caps_append (caps, newcaps);
    } else {
      caps = gst_caps_new_simple("video/x-raw",
          "format", G_TYPE_STRING, gst_video_format_to_string(fmt), NULL);
    }
  }

  caps = gst_caps_simplify(caps);

  imx_video_overlay_composition_add_caps(caps);
  caps = imx_video_convert_add_drm_caps (caps, list);

  return caps;
}

static gboolean imx_video_convert_check_format_conversion (GstBaseTransform *transform,
    GstCaps *in_caps, GstCaps *out_caps)
{
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(transform);
  Imx2DDevice *device = imxvct->device;
  gboolean is_support = TRUE;

  if ((device->device_type == IMX_2D_DEVICE_OCL)
      || ((device->device_type == IMX_2D_DEVICE_G2D) && HAS_DPU())) {
    if (!device->check_conversion (device, in_caps, out_caps)) {
      GST_DEBUG_OBJECT (imxvct, "Current device can't support conversion");
      is_support = FALSE;
      goto done;
    }

    if (!gst_caps_is_fixed (out_caps)) {
      out_caps = gst_caps_fixate (out_caps);
      GST_DEBUG("fixated out caps to %" GST_PTR_FORMAT, out_caps);
      if (!gst_caps_is_fixed (out_caps)) {
        GST_DEBUG("Not fixed caps: %" GST_PTR_FORMAT, out_caps);
        is_support = FALSE;
        goto done;
      }
    }

    if (!gst_pad_peer_query_accept_caps (GST_BASE_TRANSFORM_SRC_PAD (transform), out_caps)) {
      GST_DEBUG_OBJECT (imxvct, "Downstream can't accept the caps %" GST_PTR_FORMAT, out_caps);
      is_support = FALSE;
    }
  }

done:
  return is_support;
}

static gboolean imx_video_convert_set_format_name (GstStructure *structure, const gchar *fmt_name, guint64 modifier)
{
  guint32 fourcc;
  const gchar *drm_fmt_name;
  GstVideoFormat format;

  if (!structure || !structure)
    return FALSE;

  if (!g_strcmp0 (gst_structure_get_string (structure, "format"), "DMA_DRM")) {
    if (!g_strcmp0 (fmt_name, "UNKNOWN")) {
       gst_structure_set(structure, "drm-format", G_TYPE_STRING, fmt_name, NULL);
    } else {
      format = gst_video_format_from_string (fmt_name);
      /* Check and convert the format if it is not DRM format */
      if (format != GST_VIDEO_FORMAT_UNKNOWN) {
        fourcc = gst_video_dma_drm_fourcc_from_format (format);
        if (fourcc == DRM_FORMAT_INVALID)
          return FALSE;

        drm_fmt_name = gst_video_dma_drm_fourcc_to_string (fourcc, modifier);
        if (!drm_fmt_name)
          return FALSE;

        gst_structure_set(structure, "drm-format", G_TYPE_STRING, drm_fmt_name, NULL);
      } else {
        gst_structure_set(structure, "drm-format", G_TYPE_STRING, fmt_name, NULL);
      }
    }
  } else {
    gst_structure_set(structure, "format", G_TYPE_STRING, fmt_name, NULL);
  }

  return TRUE;
}

static guint imx_video_convert_fixate_format_caps(GstBaseTransform *transform,
                                            GstCaps *caps, GstCaps *othercaps)
{
  GstStructure *outs;
  GstStructure *tests;
  const GValue *format;
  GstVideoFormat out_fmt = GST_VIDEO_FORMAT_UNKNOWN;
  const GstVideoFormatInfo *out_info = NULL;
  const gchar *fmt_name;
  GstStructure *ins;
  const gchar *in_interlace;
  gboolean interlace = FALSE;
  GstCaps *new_caps;
  const GValue *color = NULL;
  guint32 fourcc;
  guint64 modifier;
  gboolean is_drm_format = FALSE;
  const GValue *max_framerate = NULL;

  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(transform);
  Imx2DDevice *device = imxvct->device;

  //the input caps should fixed alreay, and only have caps0
  ins = gst_caps_get_structure(caps, 0);
  outs = gst_caps_get_structure(othercaps, 0);

  in_interlace = gst_structure_get_string(ins, "interlace-mode");
  if (in_interlace && (g_strcmp0(in_interlace, "interleaved") == 0
                       || g_strcmp0(in_interlace, "mixed") == 0)) {
    interlace = TRUE;
  }

  /* if rotate or deinterlace enabled & interleaved input,
   * then passthrough is not possible, we need limit the othercaps
   * with device conversion limitation
   */
  if (imxvct->rotate != IMX_2D_ROTATION_0 ||
      (imxvct->deinterlace != IMX_2D_DEINTERLACE_NONE && interlace)) {
    GList* list = device->get_supported_out_fmts(device);
    GstCaps *out_caps = imx_video_convert_caps_from_fmt_list(list);
    g_list_free(list);

    new_caps = gst_caps_intersect_full(othercaps, out_caps,
                                       GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref(out_caps);
  } else {
    new_caps = gst_caps_copy(othercaps);
  }

#ifdef COMPARE_CONVERT_LOSS
  GstVideoFormat in_fmt;
  gint min_loss = G_MAXINT32;
  gint loss;
  guint i, j;
  const gchar *drm_fmt_name;
  GstCaps *select_caps;
  GstStructure *select_st;

  if (!g_strcmp0 (gst_structure_get_string (ins, "format"), "DMA_DRM")) {
    drm_fmt_name = gst_structure_get_string(ins, "drm-format");
    fourcc = gst_video_dma_drm_fourcc_from_string (drm_fmt_name, &modifier);
    in_fmt = gst_video_dma_drm_fourcc_to_format (fourcc);
    if (in_fmt == GST_VIDEO_FORMAT_UNKNOWN)
      fmt_name = NULL;
    else
      fmt_name = gst_video_format_to_string(in_fmt);
  } else {
    fmt_name = gst_structure_get_string(ins, "format");
  }
  if (!fmt_name) {
    gst_caps_unref(new_caps);
    return -1;
  }

  GST_LOG("source format : %s", fmt_name);

  in_fmt = gst_video_format_from_string(fmt_name);
  gboolean have_colorimetry = gst_structure_has_field (ins, "colorimetry");
  gboolean have_max_framerate = gst_structure_has_field (ins, "max-framerate");

  for (i = 0; i < gst_caps_get_size(new_caps); i++) {
    tests = gst_caps_get_structure(new_caps, i);

    /* Check whether it's DMA DRM format */
    if (!g_strcmp0 (gst_structure_get_string (tests, "format"), "DMA_DRM")) {
      format = gst_structure_get_value (tests, "drm-format");
      is_drm_format = TRUE;
    } else {
      format = gst_structure_get_value(tests, "format");
      is_drm_format = FALSE;
    }
    if (!format) {
      gst_caps_unref(new_caps);
      return -1;
    }

    if (GST_VALUE_HOLDS_LIST(format)) {
      for (j = 0; j < gst_value_list_get_size(format); j++) {
        /* Create caps to check the conversion */
        select_caps = gst_caps_copy_nth(new_caps, i);
        select_st = gst_caps_get_structure(select_caps, 0);

        const GValue *val = gst_value_list_get_value(format, j);
        if (G_VALUE_HOLDS_STRING(val)) {
          if (is_drm_format) {
            gst_structure_set(select_st, "drm-format", G_TYPE_STRING, g_value_get_string (val), NULL);
            fourcc = gst_video_dma_drm_fourcc_from_string (g_value_get_string (val), &modifier);
            out_fmt = gst_video_dma_drm_fourcc_to_format (fourcc);
          } else {
            gst_structure_set(select_st, "format", G_TYPE_STRING, g_value_get_string (val), NULL);
            out_fmt = gst_video_format_from_string(g_value_get_string(val));
          }
          loss = get_format_conversion_loss(transform, in_fmt, out_fmt);

          /* Append the colorimetry field if needed */
          if (!gst_structure_has_field(tests, "colorimetry") && have_colorimetry) {
            gst_structure_set_value(tests, "colorimetry",
                gst_structure_get_value (ins, "colorimetry"));
          }

          /* Append the max-framerate field if needed */
          if (!gst_structure_has_field(tests, "max-framerate") && have_max_framerate) {
            gst_structure_set_value(tests, "max-framerate",
                gst_structure_get_value (ins, "max-framerate"));
          }

          /* Need check if current device and the downstream can accept this caps
           * because some devices can only support the specified conversion */
          if (!imx_video_convert_check_format_conversion (transform, caps, select_caps)) {
            gst_caps_unref(select_caps);
            continue;
          } else {
            gst_caps_unref(select_caps);
          }

          if (loss < min_loss) {
            out_info = gst_video_format_get_info(out_fmt);
            min_loss = loss;
            if (gst_structure_has_field(tests, "colorimetry"))
              color = gst_structure_get_value(tests, "colorimetry");
            if (gst_structure_has_field(tests, "max-framerate"))
              max_framerate = gst_structure_get_value(tests, "max-framerate");
          }

          if (min_loss == 0)
            break;
        }
      }
    } else if (G_VALUE_HOLDS_STRING(format)) {
      /* Create caps to check conversion if needed */
      select_caps = gst_caps_copy_nth(new_caps, i);
      select_st = gst_caps_get_structure(select_caps, 0);

      if (is_drm_format) {
        gst_structure_set(select_st, "drm-format", G_TYPE_STRING, g_value_get_string (format), NULL);
        fourcc = gst_video_dma_drm_fourcc_from_string (g_value_get_string (format), &modifier);
        out_fmt = gst_video_dma_drm_fourcc_to_format (fourcc);
      } else {
        gst_structure_set(select_st, "format", G_TYPE_STRING, g_value_get_string (format), NULL);
        out_fmt = gst_video_format_from_string(g_value_get_string(format));
      }
      loss = get_format_conversion_loss(transform, in_fmt, out_fmt);

      /* Append the colorimetry field if needed */
      if (!gst_structure_has_field(tests, "colorimetry") && have_colorimetry) {
        gst_structure_set_value(tests, "colorimetry",
            gst_structure_get_value (ins, "colorimetry"));
      }

      /* Append the max-framerate field if needed */
      if (!gst_structure_has_field(tests, "max-framerate") && have_max_framerate) {
        gst_structure_set_value(tests, "max-framerate",
            gst_structure_get_value (ins, "max-framerate"));
      }

      /* Need check if current device and the downstream can accept this caps
       * because some devices can only support the specified conversion */
      if (!imx_video_convert_check_format_conversion (transform, caps, select_caps)) {
        gst_caps_unref(select_caps);
        continue;
      } else {
        gst_caps_unref(select_caps);
      }

      if (loss < min_loss) {
        out_info = gst_video_format_get_info(out_fmt);
        min_loss = loss;
        if (gst_structure_has_field(tests, "colorimetry"))
          color = gst_structure_get_value(tests, "colorimetry");
        if (gst_structure_has_field(tests, "max-framerate"))
          max_framerate = gst_structure_get_value(tests, "max-framerate");
      }
    }

    if (min_loss == 0)
      break;
  }
#else
  if (gst_video_is_dma_drm_caps (new_caps)) {
    format =
    gst_structure_get_value(gst_caps_get_structure(new_caps, 0), "drm-format");
    if (format & GST_VALUE_HOLDS_LIST(format)) {
      format = gst_value_list_get_value(format, 0);
    }

    fourcc = gst_video_dma_drm_fourcc_from_string (g_value_get_string (format), &modifier);
    out_fmt = gst_video_dma_drm_fourcc_to_format (fourcc);
    out_info = gst_video_format_get_info(out_fmt);
  } else {
    format =
      gst_structure_get_value(gst_caps_get_structure(new_caps, 0), "format");
    if (format) {
      if (GST_VALUE_HOLDS_LIST(format)) {
        format = gst_value_list_get_value(format, 0);
      }
      out_fmt = gst_video_format_from_string(g_value_get_string(format));
      out_info = gst_video_format_get_info(out_fmt);
    }
  }
#endif

  if (out_info) {
    /* respect the colormetry of selected output conversion format,
     * this is to avoid negotiation fail between converter and v4l2enc
     * when outs format is YUV(RGB) but actual output format is RGB(YUV) */
    if ((gst_structure_get_value(outs, "colorimetry") && color)
        || (have_colorimetry && color))
      gst_structure_set_value(outs, "colorimetry", color);

    if ((gst_structure_get_value(outs, "max-framerate") && max_framerate)
        || (have_max_framerate && max_framerate))
      gst_structure_set_value(outs, "max-framerate", max_framerate);
    gst_caps_unref(new_caps);

    fmt_name = GST_VIDEO_FORMAT_INFO_NAME(out_info);
    imx_video_convert_set_format_name (outs, fmt_name, modifier);
    GST_LOG("out format %s", fmt_name);
    return 0;
  } else {
    gst_caps_unref(new_caps);
    imx_video_convert_set_format_name (outs, "UNKNOWN", modifier);
    GST_LOG("out format not match");
    return -1;
  }
}

static GstCaps* imx_video_convert_fixate_caps(GstBaseTransform *transform,
    GstPadDirection direction, GstCaps *caps, GstCaps *othercaps)
{
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(transform);
  GstStructure *ins, *outs;
  GValue const *from_par, *to_par;
  GValue fpar = { 0, }, tpar = { 0, };

  g_return_val_if_fail(gst_caps_is_fixed (caps), othercaps);

  othercaps = gst_caps_make_writable(othercaps);

  GST_DEBUG("fixate othercaps: %" GST_PTR_FORMAT, othercaps);
  GST_DEBUG("based on caps: %" GST_PTR_FORMAT, caps);
  GST_DEBUG("direction: %d", direction);

  ins = gst_caps_get_structure(caps, 0);
  outs = gst_caps_get_structure(othercaps, 0);

  from_par = gst_structure_get_value(ins, "pixel-aspect-ratio");
  to_par = gst_structure_get_value(outs, "pixel-aspect-ratio");

  /* If no par info, then set some assuming value  */
  if (!from_par || !to_par) {
    if (direction == GST_PAD_SINK) {
      if (!from_par) {
        g_value_init(&fpar, GST_TYPE_FRACTION);
        gst_value_set_fraction(&fpar, 1, 1);
        from_par = &fpar;
      }
      if (!to_par) {
        g_value_init(&tpar, GST_TYPE_FRACTION_RANGE);
        gst_value_set_fraction_range_full(&tpar, 1, G_MAXINT32, G_MAXINT32, 1);
        to_par = &tpar;
      }
    } else {
      if (!to_par) {
        g_value_init(&tpar, GST_TYPE_FRACTION);
        gst_value_set_fraction(&tpar, 1, 1);
        to_par = &tpar;
        gst_structure_set(outs, "pixel-aspect-ratio",
                          GST_TYPE_FRACTION, 1, 1, NULL);
      }
      if (!from_par) {
        g_value_init(&fpar, GST_TYPE_FRACTION);
        gst_value_set_fraction (&fpar, 1, 1);
        from_par = &fpar;
      }
    }
  }

  /* from_par should be fixed now */
  gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;
  gint w = 0, h = 0;
  gint from_dar_n, from_dar_d;
  gint num, den;
  GstStructure *tmp;
  gint set_w, set_h, set_par_n, set_par_d;

  from_par_n = gst_value_get_fraction_numerator(from_par);
  from_par_d = gst_value_get_fraction_denominator(from_par);

  gst_structure_get_int(ins, "width", &from_w);
  gst_structure_get_int(ins, "height", &from_h);

  gst_structure_get_int(outs, "width", &w);
  gst_structure_get_int(outs, "height", &h);

  /* if both width and height are already fixed, we can do nothing */
  if (w && h) {
    guint dar_n, dar_d;
    GST_DEBUG("dimensions already set to %dx%d", w, h);

    if (!gst_value_is_fixed(to_par)) {
      /* If set keep_ratio=true and downstream doesn't specify pixel-aspect-ratio,
       * default use 1/1. This refers to the videoaggregator(imxcompositor) which
       * always sets pixel-aspect-ratio=1/1 for output caps. */
      if (imxvct->keep_ratio) {
        if (gst_structure_has_field(outs, "pixel-aspect-ratio")) {
          gst_structure_fixate_field_nearest_fraction(outs,
                                        "pixel-aspect-ratio", 1, 1);
        } else {
          gst_structure_set(outs, "pixel-aspect-ratio",
                            GST_TYPE_FRACTION, 1, 1, NULL);
        }
      } else if (gst_video_calculate_display_ratio(&dar_n, &dar_d,
          from_w, from_h, from_par_n, from_par_d, w, h)) {
        GST_DEBUG("fixating to_par to %d/%d", dar_n, dar_d);

        if (gst_structure_has_field(outs, "pixel-aspect-ratio")) {
          gst_structure_fixate_field_nearest_fraction(outs,
                                        "pixel-aspect-ratio", dar_n, dar_d);
        } else if (dar_n != dar_d) {
          gst_structure_set(outs, "pixel-aspect-ratio",
                            GST_TYPE_FRACTION, dar_n, dar_d, NULL);
        }
      }
    }

    goto done;
  }

  /* Calculate input DAR */
  gst_util_fraction_multiply(from_w, from_h, from_par_n, from_par_d,
                              &from_dar_n, &from_dar_d);
  GST_LOG("Input DAR is %d/%d", from_dar_n, from_dar_d);

  /* If either width or height are fixed, choose a height or width and PAR */
  if (h) {
    GST_DEBUG("height is fixed (%d)", h);

    /* If the PAR is fixed, choose the width that match DAR */
    if (gst_value_is_fixed(to_par)) {
      to_par_n = gst_value_get_fraction_numerator(to_par);
      to_par_d = gst_value_get_fraction_denominator(to_par);
      GST_DEBUG("PAR is fixed %d/%d", to_par_n, to_par_d);

      gst_util_fraction_multiply(from_dar_n, from_dar_d,
                                 to_par_d, to_par_n, &num, &den);
      w = (guint) gst_util_uint64_scale_int(h, num, den);
      gst_structure_fixate_field_nearest_int(outs, "width", w);
    } else {
      /* The PAR is not fixed, Check if we can keep the input width */
      tmp = gst_structure_copy(outs);
      gst_structure_fixate_field_nearest_int(tmp, "width", from_w);
      gst_structure_get_int(tmp, "width", &set_w);
      gst_util_fraction_multiply(from_dar_n, from_dar_d, h, set_w,
                                 &to_par_n, &to_par_d);

      if (!gst_structure_has_field(tmp, "pixel-aspect-ratio"))
        gst_structure_set_value(tmp, "pixel-aspect-ratio", to_par);

      gst_structure_fixate_field_nearest_fraction(tmp, "pixel-aspect-ratio",
                                                  to_par_n, to_par_d);
      gst_structure_get_fraction(tmp, "pixel-aspect-ratio",
                                  &set_par_n, &set_par_d);
      gst_structure_free(tmp);

      /* Check if the adjusted PAR is accepted */
      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        if (gst_structure_has_field(outs, "pixel-aspect-ratio")
            || set_par_n != set_par_d) {
          gst_structure_set(outs, "width", G_TYPE_INT, set_w,
           "pixel-aspect-ratio", GST_TYPE_FRACTION, set_par_n, set_par_d, NULL);
        }
      } else {
        /* scale the width to the new PAR and check if the adjusted width is
         * accepted. If all that fails we can't keep the DAR */
        gst_util_fraction_multiply(from_dar_n, from_dar_d, set_par_d, set_par_n,
                                  &num, &den);

        w = (guint) gst_util_uint64_scale_int(h, num, den);
        gst_structure_fixate_field_nearest_int(outs, "width", w);
        if (gst_structure_has_field(outs, "pixel-aspect-ratio")
            || set_par_n != set_par_d) {
          gst_structure_set(outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
                            set_par_n, set_par_d, NULL);
        }
      }
    }
  } else if (w) {
    GST_DEBUG("width is fixed (%d)", w);

    /* If the PAR is fixed, choose the height that match the DAR */
    if (gst_value_is_fixed(to_par)) {
      to_par_n = gst_value_get_fraction_numerator(to_par);
      to_par_d = gst_value_get_fraction_denominator(to_par);
      GST_DEBUG("PAR is fixed %d/%d", to_par_n, to_par_d);

      gst_util_fraction_multiply(from_dar_n, from_dar_d, to_par_d, to_par_n,
                                 &num, &den);
      h = (guint) gst_util_uint64_scale_int(w, den, num);
      gst_structure_fixate_field_nearest_int(outs, "height", h);
    } else {
      /* Check if we can keep the input height */
      tmp = gst_structure_copy(outs);
      gst_structure_fixate_field_nearest_int(tmp, "height", from_h);
      gst_structure_get_int(tmp, "height", &set_h);
      gst_util_fraction_multiply(from_dar_n, from_dar_d, set_h, w,
                                 &to_par_n, &to_par_d);

      if (!gst_structure_has_field(tmp, "pixel-aspect-ratio"))
        gst_structure_set_value(tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction(tmp, "pixel-aspect-ratio",
                                                  to_par_n, to_par_d);
      gst_structure_get_fraction(tmp, "pixel-aspect-ratio",
                                 &set_par_n, &set_par_d);
      gst_structure_free(tmp);

      /* Check if the adjusted PAR is accepted */
      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        if (gst_structure_has_field(outs, "pixel-aspect-ratio")
            || set_par_n != set_par_d) {
          gst_structure_set(outs, "height", G_TYPE_INT, set_h,
           "pixel-aspect-ratio", GST_TYPE_FRACTION, set_par_n, set_par_d, NULL);
        }
      } else {
        /* scale the height to the new PAR and check if the adjusted width
         * is accepted. If all that fails we can't keep the DAR */
        gst_util_fraction_multiply(from_dar_n, from_dar_d, set_par_d, set_par_n,
                                    &num, &den);

        h = (guint) gst_util_uint64_scale_int(w, den, num);
        gst_structure_fixate_field_nearest_int(outs, "height", h);
        if (gst_structure_has_field(outs, "pixel-aspect-ratio")
            || set_par_n != set_par_d) {
          gst_structure_set(outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        }
      }
    }
  } else {
    /* both h and w not fixed */
    if (gst_value_is_fixed(to_par)) {
      gint f_h, f_w;
      to_par_n = gst_value_get_fraction_numerator(to_par);
      to_par_d = gst_value_get_fraction_denominator(to_par);

      /* Calculate scale factor for the PAR change */
      gst_util_fraction_multiply(from_dar_n, from_dar_d, to_par_n, to_par_d,
                                 &num, &den);

      /* Try to keep the input height (because of interlacing) */
      tmp = gst_structure_copy(outs);
      gst_structure_fixate_field_nearest_int(tmp, "height", from_h);
      gst_structure_get_int(tmp, "height", &set_h);
      w = (guint) gst_util_uint64_scale_int(set_h, num, den);
      gst_structure_fixate_field_nearest_int(tmp, "width", w);
      gst_structure_get_int(tmp, "width", &set_w);
      gst_structure_free(tmp);

      if (set_w == w) {
        gst_structure_set(outs, "width", G_TYPE_INT, set_w,
                          "height", G_TYPE_INT, set_h, NULL);
      } else {
        f_h = set_h;
        f_w = set_w;

        /* If the former failed, try to keep the input width at least */
        tmp = gst_structure_copy(outs);
        gst_structure_fixate_field_nearest_int(tmp, "width", from_w);
        gst_structure_get_int(tmp, "width", &set_w);
        h = (guint) gst_util_uint64_scale_int(set_w, den, num);
        gst_structure_fixate_field_nearest_int(tmp, "height", h);
        gst_structure_get_int(tmp, "height", &set_h);
        gst_structure_free(tmp);

        if (set_h == h)
          gst_structure_set(outs, "width", G_TYPE_INT, set_w,
                            "height", G_TYPE_INT, set_h, NULL);
        else
          gst_structure_set(outs, "width", G_TYPE_INT, f_w,
                            "height", G_TYPE_INT, f_h, NULL);
      }
    } else {
      gint tmp2;
      /* width, height and PAR are not fixed but passthrough is not possible */
      /* try to keep the height and width as good as possible and scale PAR */
      tmp = gst_structure_copy(outs);
      gst_structure_fixate_field_nearest_int(tmp, "height", from_h);
      gst_structure_get_int(tmp, "height", &set_h);
      gst_structure_fixate_field_nearest_int(tmp, "width", from_w);
      gst_structure_get_int(tmp, "width", &set_w);

      gst_util_fraction_multiply(from_dar_n, from_dar_d, set_h, set_w,
                                 &to_par_n, &to_par_d);

      if (!gst_structure_has_field(tmp, "pixel-aspect-ratio"))
        gst_structure_set_value(tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction(tmp, "pixel-aspect-ratio",
                                                  to_par_n, to_par_d);
      gst_structure_get_fraction(tmp, "pixel-aspect-ratio",
                                 &set_par_n, &set_par_d);
      gst_structure_free(tmp);

      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        gst_structure_set(outs, "width", G_TYPE_INT, set_w,
                                "height", G_TYPE_INT, set_h, NULL);
        if (gst_structure_has_field(outs, "pixel-aspect-ratio")
            || set_par_n != set_par_d) {
          gst_structure_set(outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
                            set_par_n, set_par_d, NULL);
        }
      } else {
        /* Otherwise try to scale width to keep the DAR with the set
         * PAR and height */
        gst_util_fraction_multiply(from_dar_n, from_dar_d, set_par_d, set_par_n,
                                   &num, &den);

        w = (guint) gst_util_uint64_scale_int(set_h, num, den);
        tmp = gst_structure_copy(outs);
        gst_structure_fixate_field_nearest_int(tmp, "width", w);
        gst_structure_get_int(tmp, "width", &tmp2);
        gst_structure_free(tmp);

        if (tmp2 == w) {
          gst_structure_set(outs, "width", G_TYPE_INT, tmp2,
                                  "height", G_TYPE_INT, set_h, NULL);
          if (gst_structure_has_field(outs, "pixel-aspect-ratio")
              || set_par_n != set_par_d) {
            gst_structure_set(outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
                              set_par_n, set_par_d, NULL);
          }
        } else {
          /* then try the same with the height */
          h = (guint) gst_util_uint64_scale_int(set_w, den, num);
          tmp = gst_structure_copy(outs);
          gst_structure_fixate_field_nearest_int(tmp, "height", h);
          gst_structure_get_int(tmp, "height", &tmp2);
          gst_structure_free(tmp);

          if (tmp2 == h) {
            gst_structure_set(outs, "width", G_TYPE_INT, set_w,
                                    "height", G_TYPE_INT, tmp2, NULL);
            if (gst_structure_has_field(outs, "pixel-aspect-ratio")
                || set_par_n != set_par_d) {
              gst_structure_set(outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
                                set_par_n, set_par_d, NULL);
            }
          } else {
            /* Don't keep the DAR, take the nearest values from the first try */
            gst_structure_set(outs, "width", G_TYPE_INT, set_w,
                                    "height", G_TYPE_INT, set_h, NULL);
            if (gst_structure_has_field(outs, "pixel-aspect-ratio")
                || set_par_n != set_par_d) {
              gst_structure_set(outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
                                set_par_n, set_par_d, NULL);
            }
          }
        }
      }
    }
  }

done:
  if (from_par == &fpar)
    g_value_unset(&fpar);
  if (to_par == &tpar)
    g_value_unset(&tpar);

  imx_video_convert_fixate_format_caps(transform, caps, othercaps);
  othercaps = gst_caps_fixate (othercaps);

  GST_DEBUG("fixated othercaps to %" GST_PTR_FORMAT, othercaps);

  return othercaps;
}

static gboolean
gst_imx_video_convert_filter_meta (GstBaseTransform * trans, GstQuery * query,
    GType api, const GstStructure * params)
{
  /* propose all metadata upstream */
  return TRUE;
}

static void
imx_video_convert_set_pool_alignment(GstImxVideoConvert *imxvct, GstCaps *caps, GstBufferPool *pool, gboolean is_output)
{
  GstVideoInfo info;
  GstVideoAlignment pool_alignment, alignment;
  Imx2DAlignInfo align_info;
  gint w, h, padding_w, padding_h;
  GstStructure *config = gst_buffer_pool_get_config(pool);
  Imx2DDevice *device = imxvct->device;
  GstVideoFilter *filter = GST_VIDEO_FILTER_CAST(imxvct);
  GstVideoInfoDmaDrm drm_info;

  if (gst_video_is_dma_drm_caps (caps)) {
    if (gst_video_info_dma_drm_from_caps (&drm_info, caps))
      gst_video_info_dma_drm_to_video_info (&drm_info, &info);
  } else {
    gst_video_info_from_caps (&info, caps);
  }

  memset (&alignment, 0, sizeof (GstVideoAlignment));
  memset (&pool_alignment, 0, sizeof (GstVideoAlignment));

  gst_buffer_pool_config_get_video_alignment (config, &pool_alignment);

  w = GST_VIDEO_INFO_WIDTH (&info);
  h = GST_VIDEO_INFO_HEIGHT (&info);
  padding_w = w + pool_alignment.padding_right;
  padding_h = h + pool_alignment.padding_bottom;

  align_info.is_output = is_output;
  if (device->get_alignment
      && device->get_alignment (device, &filter->in_info, &filter->out_info, &align_info)
      && align_info.is_apply) {
    /* Check alignment parameters */
    if (!align_info.width_align || !align_info.height_align) {
      align_info.width_align = ALIGNMENT;
      align_info.height_align = ALIGNMENT;
    }

    if (!ISALIGNED (padding_w, align_info.width_align)
        || !ISALIGNED (padding_h, align_info.height_align)) {
      alignment.padding_right = SIZE_ALIGN (padding_w, align_info.width_align) - w;
      alignment.padding_bottom = SIZE_ALIGN (padding_h, align_info.height_align) - h;
    } else {
      alignment.padding_right = pool_alignment.padding_right;
      alignment.padding_bottom = pool_alignment.padding_bottom;
    }
  } else {
    if (!ISALIGNED (padding_w, ALIGNMENT) || !ISALIGNED (padding_h, ALIGNMENT)) {
      alignment.padding_right = ALIGNTO (padding_w, ALIGNMENT) - w;
      alignment.padding_bottom = ALIGNTO (padding_h, ALIGNMENT) - h;
    } else {
      alignment.padding_right = pool_alignment.padding_right;
      alignment.padding_bottom = pool_alignment.padding_bottom;
    }
    GST_DEBUG_OBJECT (imxvct, "Set padding info by default");
  }

  GST_DEBUG ("pool(%p), [%d, %d]:padding_right (%d), padding_bottom (%d)",
      pool, w, h, alignment.padding_right, alignment.padding_bottom);

  if (!gst_buffer_pool_config_has_option (config, \
        GST_BUFFER_POOL_OPTION_VIDEO_META)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }
  if (!gst_buffer_pool_config_has_option (config,
            GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  }

  gst_buffer_pool_config_set_video_alignment (config, &alignment);
  gst_buffer_pool_set_config(pool, config);
}

static gboolean
imx_video_convert_buffer_pool_is_ok (GstBufferPool * pool, GstCaps * newcaps,
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

static GstBufferPool*
gst_imx_video_convert_create_bufferpool(GstImxVideoConvert *imxvct,
                    GstCaps *caps, guint size, guint min, guint max, gboolean is_output)
{
  GstBufferPool *pool;
  GstStructure *config;

  pool = gst_video_buffer_pool_new ();
  if (pool) {
    if (!imxvct->allocator) {
#ifdef USE_DMABUFHEAPS
      imxvct->allocator = gst_dmabufheaps_allocator_obtain ();
#endif
#ifdef USE_ION
      if (!imxvct->allocator) {
        imxvct->allocator = gst_ion_allocator_obtain ();
      }
#endif
    }

    if (!imxvct->allocator)
      imxvct->allocator =
          gst_imx_2d_device_allocator_new((gpointer)(imxvct->device));

    if (!imxvct->allocator) {
      GST_ERROR ("new imx video convert allocator failed.");
      gst_buffer_pool_set_active (pool, FALSE);
      gst_object_unref (pool);
      return NULL;
    }

    config = gst_buffer_pool_get_config(pool);
    gst_buffer_pool_config_set_params(config, caps, size, min, max);
    gst_buffer_pool_config_set_allocator(config, imxvct->allocator, NULL);
    gst_buffer_pool_config_add_option(config,
                                      GST_BUFFER_POOL_OPTION_VIDEO_META);
    if (!gst_buffer_pool_set_config(pool, config)) {
      GST_ERROR ("set buffer pool config failed.");
      gst_buffer_pool_set_active (pool, FALSE);
      gst_object_unref (pool);
      return NULL;
    }
  }

  imx_video_convert_set_pool_alignment(imxvct, caps, pool, is_output);

  GST_LOG ("created a buffer pool (%p).", pool);
  return pool;
}

static gboolean
imx_video_convert_propose_allocation(GstBaseTransform *transform,
                                      GstQuery *decide_query, GstQuery *query)
{
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(transform);
  GstBufferPool *pool;
  GstVideoInfo info;
  guint size = 0;
  GstCaps *caps;
  gboolean need_pool;

  /* passthrough, we're done */
  if (decide_query == NULL) {
    GST_DEBUG ("doing passthrough query");
    if (imxvct->composition_meta_enable && imxvct->in_place) {
      imx_video_overlay_composition_add_query_meta (query);
    }
    return gst_pad_peer_query (transform->srcpad, query);
  } else {
    guint i, n_metas;
    /* non-passthrough, copy all metadata, decide_query does not contain the
     * metadata anymore that depends on the buffer memory */
    n_metas = gst_query_get_n_allocation_metas (decide_query);
    for (i = 0; i < n_metas; i++) {
      GType api;
      const GstStructure *params;
      api = gst_query_parse_nth_allocation_meta (decide_query, i, &params);
      gst_query_add_allocation_meta (query, api, params);
    }
  }

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (need_pool) {
    if (caps == NULL) {
      GST_ERROR_OBJECT (imxvct, "no caps specified.");
      return FALSE;
    }

    GstVideoInfoDmaDrm drm_info;
    if (gst_video_is_dma_drm_caps (caps)) {
      if (!gst_video_info_dma_drm_from_caps (&drm_info, caps))
        return FALSE;
      if (!gst_video_info_dma_drm_to_video_info (&drm_info, &info))
        return FALSE;
    } else {
      if (!gst_video_info_from_caps (&info, caps))
      return FALSE;
    }

    size = GST_VIDEO_INFO_SIZE (&info);
    GST_IMX_CONVERT_UNREF_BUFFER (imxvct->in_buf);
    GST_IMX_CONVERT_UNREF_POOL(imxvct->in_pool);
    GST_DEBUG_OBJECT(imxvct, "creating new input pool");
    pool = gst_imx_video_convert_create_bufferpool(imxvct, caps, size, 1,
                                                   IMX_VCT_IN_POOL_MAX_BUFFERS, FALSE);
    imxvct->in_pool = pool;
    imxvct->pool_config_update = TRUE;

    if (pool) {
      GST_DEBUG_OBJECT (imxvct, "propose_allocation, pool(%p).", pool);
      GstStructure *config = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_get_params (config, &caps, &size, NULL, NULL);
      gst_structure_free (config);

      gst_query_add_allocation_pool (query, pool, size, 1,
                                     IMX_VCT_IN_POOL_MAX_BUFFERS);
      gst_query_add_allocation_param (query, imxvct->allocator, NULL);
    } else {
      return FALSE;
    }
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);

  if (imxvct->composition_meta_enable)
    imx_video_overlay_composition_add_query_meta (query);

  return TRUE;
}

static gboolean imx_video_convert_decide_allocation(GstBaseTransform *transform,
                                                     GstQuery *query)
{
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(transform);
  GstCaps *outcaps;
  GstBufferPool *pool = NULL;
  guint size, num, min = 0, max = 0;
  GstStructure *config = NULL;
  GstVideoInfo vinfo;
  gboolean new_pool = TRUE;
  GstAllocator *allocator = NULL;
  Imx2DAlignInfo align_info;
  GstVideoFilter *filter = GST_VIDEO_FILTER_CAST(transform);
  GstVideoInfoDmaDrm drm_info;

  gst_query_parse_allocation(query, &outcaps, NULL);
  gst_video_info_init(&vinfo);

  if (gst_video_is_dma_drm_caps (outcaps)) {
    if (gst_video_info_dma_drm_from_caps (&drm_info, outcaps))
      gst_video_info_dma_drm_to_video_info (&drm_info, &vinfo);
  } else {
    gst_video_info_from_caps(&vinfo, outcaps);
  }
  num = gst_query_get_n_allocation_pools(query);
  size = vinfo.size;

  GST_DEBUG_OBJECT(imxvct, "number of allocation pools: %d", num);

  /* if downstream element provided buffer pool with phy buffers */
  if (num > 0) {
    guint i = 0;
    for (; i < num; ++i) {
      gst_query_parse_nth_allocation_pool(query, i, &pool, &size, &min, &max);
      if (pool) {
        config = gst_buffer_pool_get_config(pool);
        gst_buffer_pool_config_get_allocator(config, &allocator, NULL);
        if (allocator
          && (GST_IS_DMABUFHEAPS_ALLOCATOR(allocator)
          || GST_IS_ALLOCATOR_PHYMEM(allocator))) {
          size = MAX(size, vinfo.size);
          new_pool = FALSE;
          break;
        } else {
          GST_DEBUG_OBJECT (imxvct, "no phy or dma allocator in output pool (%p)", pool);
        }

        if (config) {
          gst_structure_free (config);
          config = NULL;
        }

        allocator = NULL;
        gst_object_unref (pool);
      }
    }
  }

  size = MAX(size, vinfo.size);

  align_info.is_output = TRUE;
  if (imxvct->device->get_alignment
      && imxvct->device->get_alignment (imxvct->device, &filter->in_info, &filter->out_info, &align_info)
      && align_info.is_apply
      && align_info.size_align) {
    size = SIZE_ALIGN(size, align_info.size_align);
  } else {
    size = PAGE_ALIGN(size);
    GST_DEBUG_OBJECT (imxvct, "Set size alignment by default");
  }

  if (max == 0) {
    if (min < 3)
      max = min = 3;
    else
      max = min;
  }

  /* downstream doesn't provide a pool or the pool has no ability to allocate
   * physical memory buffers, we need create new pool */
  if (new_pool) {
    GST_IMX_CONVERT_UNREF_POOL(imxvct->self_out_pool);
    GST_DEBUG_OBJECT(imxvct, "creating new output pool");
    pool = gst_imx_video_convert_create_bufferpool(imxvct, outcaps, size,
                                                   min, max, TRUE);
    imxvct->self_out_pool = pool;
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_set_active(pool, TRUE);
  } else {
    // check the requirement of output alignment
    imx_video_convert_set_pool_alignment(imxvct, outcaps, pool, TRUE);
  }

  imxvct->out_pool = pool;
  gst_buffer_pool_config_get_params (config, &outcaps, &size, &min, &max);

  GST_DEBUG_OBJECT(imxvct, "pool config:  outcaps: %" GST_PTR_FORMAT "  "
      "size: %u  min buffers: %u  max buffers: %u", outcaps, size, min, max);
  gst_structure_free (config);

  if (pool) {
    if (num > 0)
      gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
    else
      gst_query_add_allocation_pool(query, pool, size, min, max);

    if (!new_pool)
      gst_object_unref (pool);
  }

  return TRUE;
}

static gboolean imx_video_convert_set_info(GstVideoFilter *filter,
                                    GstCaps *in, GstVideoInfo *in_info,
                                    GstCaps *out, GstVideoInfo *out_info)
{
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(filter);
  Imx2DDevice *device = imxvct->device;
  GstStructure *ins, *outs;
  const gchar *from_interlace;
  GstVideoInfoDmaDrm drm_info;

  if (!device)
    return FALSE;

  gst_video_info_dma_drm_init (&drm_info);
  if (gst_video_is_dma_drm_caps (in)) {
    if (gst_video_info_dma_drm_from_caps (&drm_info, in))
      gst_video_info_dma_drm_to_video_info (&drm_info, in_info);
  }

  gst_video_info_dma_drm_init (&drm_info);
  if (gst_video_is_dma_drm_caps (out)) {
    if (gst_video_info_dma_drm_from_caps (&drm_info, out))
      gst_video_info_dma_drm_to_video_info (&drm_info, out_info);
  }

  ins = gst_caps_get_structure(in, 0);
  outs = gst_caps_get_structure(out, 0);

  /* if interlaced and we enabled deinterlacing, make it progressive */
  from_interlace = gst_structure_get_string(ins, "interlace-mode");
  if (from_interlace &&
      (g_strcmp0(from_interlace, "interleaved") == 0
          || g_strcmp0(from_interlace, "mixed") == 0)) {
    if (IMX_2D_DEINTERLACE_NONE != imxvct->deinterlace) {
      gst_structure_set(outs,
          "interlace-mode", G_TYPE_STRING, "progressive", NULL);
      gst_base_transform_set_passthrough((GstBaseTransform*)filter, FALSE);
    }
  }

  if (IMX_2D_ROTATION_0 != imxvct->rotate)
    gst_base_transform_set_passthrough((GstBaseTransform*)filter, FALSE);

/* can't remove since caps fixed
  if (gst_structure_get_string(outs, "colorimetry")) {
    GST_DEBUG("try to remove colorimetry");
    gst_structure_remove_fields(outs,"colorimetry", NULL);
  }

  if (gst_structure_get_string(outs, "chroma-site")) {
    GST_DEBUG("try to remove chroma-site");
    gst_structure_remove_fields(outs,"chroma-site", NULL);
  }
*/

  if (!imxvct->composition_meta_enable || imxvct->in_place) {
    //if src and sink caps only has video overlay composition feature difference
    //then force to work in pass through mode.
    GstCapsFeatures *in_f, *out_f;
    in_f = gst_caps_get_features(in, 0);
    out_f = gst_caps_get_features(out, 0);
    if (in_f && !gst_caps_features_is_equal(in_f, out_f)) {
      GstCapsFeatures *f = gst_caps_features_new(
          GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, NULL);
      GstCaps *copy_out = gst_caps_copy(out);
      gst_caps_set_features(copy_out, 0, f);
      if (gst_caps_is_equal(in, copy_out)) {
        gst_base_transform_set_passthrough((GstBaseTransform*)filter, TRUE);
      }
      gst_caps_unref(copy_out);
    }
  }

  /* FIXME: find a way to detect videocrop meta */
  if (imxvct->videocrop_meta_enable)
    gst_base_transform_set_passthrough((GstBaseTransform*)filter, FALSE);

  if (imxvct->video_warp.enable)
    gst_base_transform_set_passthrough((GstBaseTransform*)filter, FALSE);

  imxvct->pool_config_update = TRUE;

  GST_DEBUG ("set info from %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, in, out);

  return TRUE;
}

static gboolean
imx_video_convert_accept_caps (GstBaseTransform * transform,
    GstPadDirection direction, GstCaps * caps)
{
  GstPad *pad;
  gboolean ret = TRUE;
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(transform);
  Imx2DDevice *device = imxvct->device;

  GST_DEBUG_OBJECT (transform, "accept caps %" GST_PTR_FORMAT, caps);
  /* Check whether the current format conversion
   * is supported for OpenCL-based 2d device or
   * g2d device which has DPU */
  if ((device->device_type == IMX_2D_DEVICE_OCL)
      || ((device->device_type == IMX_2D_DEVICE_G2D) && HAS_DPU())) {
    pad =
      (direction ==
      GST_PAD_SINK) ? GST_BASE_TRANSFORM_SRC_PAD (transform) :
      GST_BASE_TRANSFORM_SINK_PAD (transform);

    GstQuery *query = gst_query_new_caps (NULL);
    if (gst_pad_peer_query (pad, query)) {
      GstCaps *rescaps = NULL;

      gst_query_parse_caps_result (query, &rescaps);
      if (!rescaps || gst_caps_is_any (rescaps) || gst_caps_is_empty (rescaps)) {
        GST_DEBUG_OBJECT (transform, "can't provide caps");
      } else {
        if (direction == GST_PAD_SINK) {
          ret = device->check_conversion (device, caps, rescaps);
        } else {
          ret = device->check_conversion (device, rescaps, caps);
        }
        GST_DEBUG_OBJECT (transform, "query caps %" GST_PTR_FORMAT
            ", accept-caps result: %d", rescaps, ret);
        if (!ret) {
          gst_query_unref (query);
          return ret;
        }
      }
    }
    gst_query_unref (query);
  }

  return GST_BASE_TRANSFORM_CLASS(parent_class)->accept_caps(transform, direction, caps);
}

static guint8 *
_get_cached_phyaddr (GstMemory * mem)
{
    return gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
              g_quark_from_static_string ("phyaddr"));
}

static void
_set_cached_phyaddr (GstMemory * mem, guint8 * phyadd)
{
  return gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
                g_quark_from_static_string ("phyaddr"), phyadd, NULL);
}

static GstFlowReturn imx_video_convert_transform_frame(GstVideoFilter *filter,
    GstVideoFrame *in, GstVideoFrame *out)
{
    return GST_FLOW_OK;
}

static void imx_video_convert_update_colorimetry (GstVideoInfo * info, Imx2DVideoInfo * imx_info)
{
  if (!info || !imx_info)
    return;

  Imx2DColorRangeType imx_color_range = IMX_2D_COLOR_RANGE_DEFAULT;
  Imx2DColorMatrixType imx_color_matrix = IMX_2D_COLOR_MATRIX_DEFAULT;

  switch (info->colorimetry.range) {
    case GST_VIDEO_COLOR_RANGE_0_255:
      imx_color_range = IMX_2D_COLOR_RANGE_FULL;
      break;
    case GST_VIDEO_COLOR_RANGE_16_235:
      imx_color_range = IMX_2D_COLOR_RANGE_LIMITED;
      break;
    default:
      break;
  }

  switch (info->colorimetry.matrix) {
    case GST_VIDEO_COLOR_MATRIX_BT709:
      imx_color_matrix = IMX_2D_COLOR_MATRIX_BT709;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT601:
      imx_color_matrix = IMX_2D_COLOR_MATRIX_BT601_625;
      break;
    default:
      break;
  }

  imx_info->colorimetry.range = imx_color_range;
  imx_info->colorimetry.matrix = imx_color_matrix;
}

static gboolean
imx_video_convert_check_src_buffer_alignment(GstImxVideoConvert *imxvct, GstBuffer *inbuf, GstVideoMeta *video_meta)
{
  Imx2DDevice *device = imxvct->device;
  GstVideoFilter *filter = GST_VIDEO_FILTER_CAST(imxvct);
  gint w, h;
  Imx2DAlignInfo align_info;
  gsize size;

  /* Check whether alignment does not needs to be handled
   * or the input buffer is acquired from its own buffer pool
   */
  if (!device->get_alignment
      || (imxvct->in_pool && imxvct->in_pool == inbuf->pool)) {
    GST_DEBUG_OBJECT(imxvct, "No need check buffer alignment");
    return TRUE;
  }

  /* Get input alignemnt information */
  align_info.is_output = FALSE;
  if (device->get_alignment (device, &filter->in_info,
      &filter->out_info, &align_info)) {
    if (!align_info.width_align
        || !align_info.height_align
        || !align_info.size_align) {
      GST_DEBUG_OBJECT(imxvct, "No need check because of invalid align (w,h,size): %d,%d,%d",
          align_info.width_align,
          align_info.height_align,
          align_info.size_align);
      return TRUE;
    }

    w = GST_VIDEO_INFO_WIDTH (&filter->in_info);
    h = GST_VIDEO_INFO_HEIGHT (&filter->in_info);
    size = GST_VIDEO_INFO_SIZE (&filter->in_info);
    if (video_meta) {
      w += video_meta->alignment.padding_left + video_meta->alignment.padding_right;
      h += video_meta->alignment.padding_top + video_meta->alignment.padding_bottom;
    }
    if (!ISALIGNED (w, align_info.width_align)
        || !ISALIGNED (h, align_info.height_align)
        || !ISALIGNED (size, align_info.size_align)) {
      GST_DEBUG_OBJECT(imxvct, "buffer does not meet alignment, buf: %dx%d, size: %" G_GSIZE_FORMAT
          ", align(w,h,size): %d,%d,%d", w, h, size,
          align_info.width_align,
          align_info.height_align,
          align_info.size_align);
      return FALSE;
    }
  }

  GST_DEBUG_OBJECT(imxvct, "buffer meet alignment");
  return TRUE;
}

static GstFlowReturn imx_video_convert_transform(GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVideoFilter *filter = GST_VIDEO_FILTER_CAST(trans);
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(trans);
  Imx2DDevice *device = imxvct->device;
  GstBuffer *input_buf = inbuf;
  GstPhyMemMeta *phymemmeta = NULL;
  GstCaps *caps;
  GstVideoFrame temp_in_frame;
  Imx2DFrame src = {0}, dst = {0};
  PhyMemBlock src_mem = {0}, dst_mem = {0};
  guint i, n_mem;
  GstVideoCropMeta *in_crop = NULL, *out_crop = NULL;
  GstVideoMeta *video_meta = gst_buffer_get_video_meta (inbuf);
  GstVideoInfo in_info;
  GstDmabufMeta *dmabuf_meta;
  guint64 drm_modifier = 0;

  if (!device)
    return GST_FLOW_ERROR;

  if (!(gst_buffer_is_phymem(outbuf)
        || gst_is_dmabuf_memory (gst_buffer_peek_memory (outbuf, 0)))) {
    GST_ERROR ("out buffer is not phy memory or DMA Buf");
    return GST_FLOW_ERROR;
  }

  in_info = filter->in_info;
  if (video_meta) {
    gint i;

    for (i = 0; i < video_meta->n_planes; i++) {
      in_info.offset[i] = video_meta->offset[i];
      in_info.stride[i] = video_meta->stride[i];
    }
    in_info.width = video_meta->width;
    in_info.height = video_meta->height;
  }

  /* Check if need copy input frame */
  if (!(gst_buffer_is_phymem(inbuf)
        || gst_is_dmabuf_memory (gst_buffer_peek_memory (inbuf, 0)))
        || !imx_video_convert_check_src_buffer_alignment(imxvct, inbuf, video_meta)) {
    GST_DEBUG ("copy input frame to physical continues memory");
    caps = gst_video_info_to_caps(&in_info);
    gst_video_info_from_caps(&in_info, caps); //update the size info

    if (!imxvct->in_pool ||
        !imx_video_convert_buffer_pool_is_ok(imxvct->in_pool, caps, in_info.size)) {
      GST_IMX_CONVERT_UNREF_POOL(imxvct->in_pool);
      GST_DEBUG_OBJECT(imxvct, "creating new input pool");
      imxvct->in_pool = gst_imx_video_convert_create_bufferpool(imxvct, caps,
          in_info.size, 1, IMX_VCT_IN_POOL_MAX_BUFFERS, FALSE);
    }

    gst_caps_unref (caps);

    if (imxvct->in_pool && !imxvct->in_buf) {
      gst_buffer_pool_set_active(imxvct->in_pool, TRUE);
      GstFlowReturn ret = gst_buffer_pool_acquire_buffer(imxvct->in_pool,
                                                  &(imxvct->in_buf), NULL);
      if (ret != GST_FLOW_OK)
        GST_ERROR("error acquiring input buffer: %s", gst_flow_get_name(ret));
      else
        GST_LOG ("created input buffer (%p)", imxvct->in_buf);
    }

    if (imxvct->in_buf) {
      GstVideoFrame in_frame;

      gst_video_frame_map (&in_frame, &in_info, inbuf,
        GST_MAP_READ | GST_VIDEO_FRAME_MAP_FLAG_NO_REF);

      gst_video_frame_map(&temp_in_frame, &in_info, imxvct->in_buf, GST_MAP_WRITE);
      gst_video_frame_copy(&temp_in_frame, &in_frame);
      input_buf = temp_in_frame.buffer;
      gst_video_frame_unmap(&temp_in_frame);
      gst_video_frame_unmap (&in_frame);

      if (imxvct->composition_meta_enable
              && imx_video_overlay_composition_has_meta(inbuf)) {
        imx_video_overlay_composition_remove_meta(imxvct->in_buf);
        imx_video_overlay_composition_copy_meta(imxvct->in_buf, inbuf,
            in_info.width, in_info.height, in_info.width, in_info.height);
      }
    } else {
      GST_ERROR ("Can't get input buffer");
      return GST_FLOW_ERROR;
    }
  }

  if (imxvct->video_meta_check) {
    if (video_meta) {
      if (imxvct->in_video_align.padding_left != video_meta->alignment.padding_left ||
          imxvct->in_video_align.padding_right != video_meta->alignment.padding_right ||
          imxvct->in_video_align.padding_top != video_meta->alignment.padding_top ||
          imxvct->in_video_align.padding_bottom != video_meta->alignment.padding_bottom) {
        GST_INFO_OBJECT (imxvct, "input videometa alignment changes to (%d, %d) , (%d, %d)",
            video_meta->alignment.padding_left,
            video_meta->alignment.padding_top,
            video_meta->alignment.padding_right,
            video_meta->alignment.padding_bottom);
        imxvct->in_video_align = video_meta->alignment;
      }
    } else {
      memset (&imxvct->in_video_align, 0, sizeof(GstVideoAlignment));
    }
  }

  if (imxvct->pool_config_update) {
    //alignment check
    memset (&imxvct->in_video_align, 0, sizeof(GstVideoAlignment));
    phymemmeta = GST_PHY_MEM_META_GET (input_buf);
    if (phymemmeta) {
      imxvct->in_video_align.padding_right = phymemmeta->x_padding;
      imxvct->in_video_align.padding_bottom = phymemmeta->y_padding;
      GST_DEBUG_OBJECT (imxvct, "physical memory meta x_padding: %d y_padding: %d",
          phymemmeta->x_padding, phymemmeta->y_padding);
    } else if (imxvct->in_pool && gst_buffer_pool_is_active (imxvct->in_pool) &&
        imxvct->in_pool == input_buf->pool) {
      /* Apply the alignment information only if
       * the buffer is allocated from own pool.
       */
      GstStructure *config = gst_buffer_pool_get_config (imxvct->in_pool);
      memset (&imxvct->in_video_align, 0, sizeof(GstVideoAlignment));

      if (gst_buffer_pool_config_has_option (config,
                GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
        gst_buffer_pool_config_get_video_alignment (config,
            &imxvct->in_video_align);
        GST_DEBUG ("input pool has alignment (%d, %d) , (%d, %d)",
          imxvct->in_video_align.padding_left,
          imxvct->in_video_align.padding_top,
          imxvct->in_video_align.padding_right,
          imxvct->in_video_align.padding_bottom);
      }

      gst_structure_free (config);
    } else if (video_meta){
      GST_DEBUG_OBJECT (imxvct, "videometa has alignment (%d, %d) , (%d, %d)",
          video_meta->alignment.padding_left,
          video_meta->alignment.padding_top,
          video_meta->alignment.padding_right,
          video_meta->alignment.padding_bottom);
      imxvct->in_video_align = video_meta->alignment;
      imxvct->video_meta_check = TRUE;
    }

    if (imxvct->out_pool) {
      GstStructure *config = gst_buffer_pool_get_config (imxvct->out_pool);
      memset (&imxvct->out_video_align, 0, sizeof(GstVideoAlignment));

      if (gst_buffer_pool_is_active (imxvct->out_pool)
          && gst_buffer_pool_config_has_option (config,
                GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
        gst_buffer_pool_config_get_video_alignment (config,
            &imxvct->out_video_align);
        GST_DEBUG ("output pool has alignment (%d, %d) , (%d, %d)",
          imxvct->out_video_align.padding_left,
          imxvct->out_video_align.padding_top,
          imxvct->out_video_align.padding_right,
          imxvct->out_video_align.padding_bottom);
      }

      gst_structure_free (config);
    }

    /* set physical memory padding info */
    if (imxvct->self_out_pool && gst_buffer_is_writable (outbuf)) {
      phymemmeta = GST_PHY_MEM_META_ADD (outbuf);
      phymemmeta->x_padding = imxvct->out_video_align.padding_right;
      phymemmeta->y_padding = imxvct->out_video_align.padding_bottom;
      GST_DEBUG_OBJECT (imxvct, "out physical memory meta x_padding: %d y_padding: %d",
          phymemmeta->x_padding, phymemmeta->y_padding);
    }

    imxvct->pool_config_update = FALSE;
  }

  /* Update output buffer alignment information
    * because some plugins such as imxcompositor need it */
  GstVideoMeta *out_video_meta;
  out_video_meta = gst_buffer_get_video_meta (outbuf);
  if (out_video_meta) {
    out_video_meta->alignment.padding_left = imxvct->out_video_align.padding_left;
    out_video_meta->alignment.padding_top = imxvct->out_video_align.padding_top;
    out_video_meta->alignment.padding_right = imxvct->out_video_align.padding_right;
    out_video_meta->alignment.padding_bottom = imxvct->out_video_align.padding_bottom;
    GST_DEBUG_OBJECT (imxvct, "update output buffer alignment, %ux%u (%u,%u,%u,%u)",
        out_video_meta->width, out_video_meta->height,
        out_video_meta->alignment.padding_left, out_video_meta->alignment.padding_right,
        out_video_meta->alignment.padding_top, out_video_meta->alignment.padding_bottom);
  }

  src.info.fmt = GST_VIDEO_INFO_FORMAT(&in_info);
  if (video_meta) {
    src.info.w = video_meta->width + imxvct->in_video_align.padding_left +
                imxvct->in_video_align.padding_right;
    src.info.h = video_meta->height + imxvct->in_video_align.padding_top +
                imxvct->in_video_align.padding_bottom;
    src.info.stride = video_meta->stride[0];
  } else {
    src.info.w = in_info.width + imxvct->in_video_align.padding_left +
                imxvct->in_video_align.padding_right;
    src.info.h = in_info.height + imxvct->in_video_align.padding_top +
                imxvct->in_video_align.padding_bottom;
    src.info.stride = in_info.stride[0];
  }
  if (GST_VIDEO_FORMAT_INFO_IS_TILED(in_info.finfo)) {
    gint ws = GST_VIDEO_FORMAT_INFO_TILE_STRIDE (in_info.finfo, 0);
    src.info.stride = GST_VIDEO_TILE_X_TILES(src.info.stride) * ws;
  }

  dmabuf_meta = gst_buffer_get_dmabuf_meta (inbuf);
  if (dmabuf_meta) {
    drm_modifier = dmabuf_meta->drm_modifier;
    dmabuf_meta->drm_modifier = 0;
  }

  dmabuf_meta = gst_buffer_get_dmabuf_meta (outbuf);
  if (dmabuf_meta) {
    dmabuf_meta->drm_modifier = 0;
  }

  GST_TRACE_OBJECT (imxvct, "buffer modifier type %" G_GUINT64_FORMAT, drm_modifier);

  switch (GST_VIDEO_FORMAT_INFO_FORMAT(in_info.finfo)) {
  case GST_VIDEO_FORMAT_NV12_8L128:
  case GST_VIDEO_FORMAT_NV12_10BE_8L128:
    src.info.tile_type = IMX_2D_TILE_AMHPION;
    break;
  default:
    break;
  }
  if (drm_modifier == DRM_FORMAT_MOD_AMPHION_TILED)
    src.info.tile_type = IMX_2D_TILE_AMHPION;

  if (device->device_type == IMX_2D_DEVICE_OCL) {
    imx_video_convert_update_colorimetry (&filter->in_info, &src.info);
    imx_video_convert_update_colorimetry (&filter->out_info, &dst.info);
  }

  gint ret = device->config_input(device, &src.info);

  GST_LOG ("Input: %s, %dx%d(%d)", GST_VIDEO_FORMAT_INFO_NAME(in_info.finfo),
      src.info.w, src.info.h, src.info.stride);

  dst.info.fmt = GST_VIDEO_INFO_FORMAT(&(filter->out_info));
  dst.info.w = filter->out_info.width + imxvct->out_video_align.padding_left +
                imxvct->out_video_align.padding_right;
  dst.info.h = filter->out_info.height + imxvct->out_video_align.padding_top +
                imxvct->out_video_align.padding_bottom;
  dst.info.stride = filter->out_info.stride[0];

  ret |= device->config_output(device, &dst.info);

  GST_LOG ("Output: %s, %dx%d", GST_VIDEO_FORMAT_INFO_NAME(filter->out_info.finfo),
      filter->out_info.width, filter->out_info.height);

  if (ret != 0)
    return GST_FLOW_ERROR;

  src.fd[0] = src.fd[1] =src.fd[2] = src.fd[3] = -1;
  if (gst_is_dmabuf_memory (gst_buffer_peek_memory (input_buf, 0))) {
    src.mem = &src_mem;
    n_mem = gst_buffer_n_memory (input_buf);
    for (i = 0; i < n_mem; i++)
      src.fd[i] = gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (input_buf, i));
  } else
    src.mem = gst_buffer_query_phymem_block (input_buf);
  src.alpha = 0xFF;
  src.crop.x = 0;
  src.crop.y = 0;
  src.crop.w = in_info.width;
  src.crop.h = in_info.height;
  src.rotate = imxvct->rotate;

  in_crop = gst_buffer_get_video_crop_meta(inbuf);
  if (in_crop != NULL) {
    GST_LOG ("input crop meta: (%d, %d, %d, %d).", in_crop->x, in_crop->y,
        in_crop->width, in_crop->height);
    if ((in_crop->x >= src.info.w) || (in_crop->y >= src.info.h))
      return GST_FLOW_ERROR;

    src.crop.x += in_crop->x;
    src.crop.y += in_crop->y;
    src.crop.w = MIN(in_crop->width, in_info.width);
    src.crop.h = MIN(in_crop->height, in_info.height);
  }

  //rotate and de-interlace setting
  if (device->set_rotate(device, imxvct->rotate) < 0) {
    GST_WARNING_OBJECT (imxvct, "set rotate failed");
    return GST_FLOW_ERROR;
  }

  if (device->set_deinterlace(device, imxvct->deinterlace) < 0) {
    GST_WARNING_OBJECT (imxvct, "set deinterlace mode failed");
    return GST_FLOW_ERROR;
  }

  switch (in_info.interlace_mode) {
    case GST_VIDEO_INTERLACE_MODE_INTERLEAVED:
      GST_TRACE("input stream is interleaved");
      src.interlace_type = IMX_2D_INTERLACE_INTERLEAVED;
      break;
    case GST_VIDEO_INTERLACE_MODE_MIXED:
    {
      GST_TRACE("input stream is mixed");
      if (video_meta != NULL) {
        if (video_meta->flags & GST_VIDEO_FRAME_FLAG_INTERLACED) {
          GST_TRACE("frame has video metadata and INTERLACED flag");
          src.interlace_type = IMX_2D_INTERLACE_INTERLEAVED;
        }
      }
      break;
    }
    case GST_VIDEO_INTERLACE_MODE_PROGRESSIVE:
      GST_TRACE("input stream is progressive");
      break;
    case GST_VIDEO_INTERLACE_MODE_FIELDS:
      GST_TRACE("input stream is 2-fields");
      src.interlace_type = IMX_2D_INTERLACE_FIELDS;
      break;
    default:
      src.interlace_type = IMX_2D_INTERLACE_PROGRESSIVE;
      break;
  }
  if (GST_BUFFER_FLAG_IS_SET (input_buf, GST_VIDEO_BUFFER_FLAG_INTERLACED)) {
    src.interlace_type = IMX_2D_INTERLACE_INTERLEAVED;
    GST_BUFFER_FLAG_UNSET (input_buf, GST_VIDEO_BUFFER_FLAG_INTERLACED);
    GST_BUFFER_FLAG_UNSET (outbuf, GST_VIDEO_BUFFER_FLAG_INTERLACED);
  }

  dst.fd[0] = dst.fd[1] = dst.fd[2] = dst.fd[3] = -1;
  if (gst_is_dmabuf_memory (gst_buffer_peek_memory (outbuf, 0))) {
    dst.mem = &dst_mem;
    n_mem = gst_buffer_n_memory (outbuf);
    for (i = 0; i < n_mem; i++)
      dst.fd[i] = gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (outbuf, i));
  } else 
    dst.mem = gst_buffer_query_phymem_block (outbuf);
  dst.alpha = 0xFF;
  dst.interlace_type = IMX_2D_INTERLACE_PROGRESSIVE;

  if (imxvct->keep_ratio) {
    GstVideoRectangle s_rect, d_rect, result;
    s_rect.x = s_rect.y = 0;
    s_rect.w = src.crop.w;
    s_rect.h = src.crop.h;
    d_rect.x = d_rect.y = 0;
    d_rect.w = filter->out_info.width;
    d_rect.h = filter->out_info.height;
    if (imxvct->rotate == IMX_2D_ROTATION_90 ||
        imxvct->rotate == IMX_2D_ROTATION_270) {
      gint tmp = d_rect.w;
      d_rect.w = d_rect.h;
      d_rect.h = tmp;
    }

    gst_video_sink_center_rect (s_rect, d_rect, &result, TRUE);

    if (imxvct->rotate == IMX_2D_ROTATION_90 ||
        imxvct->rotate == IMX_2D_ROTATION_270) {
      dst.crop.x = result.y;
      dst.crop.y = result.x;
      dst.crop.w = result.h;
      dst.crop.h = result.w;
    } else {
      dst.crop.x = result.x;
      dst.crop.y = result.y;
      dst.crop.w = result.w;
      dst.crop.h = result.h;
    }
  } else {
    dst.crop.x = 0;
    dst.crop.y = 0;
    dst.crop.w = filter->out_info.width;
    dst.crop.h = filter->out_info.height;
  }

  out_crop = gst_buffer_get_video_crop_meta(outbuf);
  if (out_crop != NULL) {
    GST_LOG ("output crop meta: (%d, %d, %d, %d).", out_crop->x, out_crop->y,
        out_crop->width, out_crop->height);
    if ((out_crop->x >= dst.info.w) || (out_crop->y >= dst.info.h))
      return GST_FLOW_ERROR;

    dst.crop.x += out_crop->x;
    dst.crop.y += out_crop->y;
    dst.crop.w = MIN(out_crop->width, filter->out_info.width);
    dst.crop.h = MIN(out_crop->height, filter->out_info.height);
  } else {
      out_crop = gst_buffer_add_video_crop_meta (outbuf);
      out_crop->x = imxvct->out_video_align.padding_left;
      out_crop->y = imxvct->out_video_align.padding_top;
      out_crop->width = filter->out_info.width;
      out_crop->height = filter->out_info.height;
      GST_DEBUG_OBJECT (imxvct, "crop, %ux%u (%u,%u)",
          out_crop->width, out_crop->height, out_crop->x, out_crop->y);
  }

  if (!src.mem->paddr)
    src.mem->paddr = _get_cached_phyaddr (gst_buffer_peek_memory (input_buf, 0));
  if (!src.mem->user_data && src.fd[1] >= 0)
    src.mem->user_data = (gpointer *)_get_cached_phyaddr (gst_buffer_peek_memory (input_buf, 1));
  if (!dst.mem->paddr)
    dst.mem->paddr = _get_cached_phyaddr (gst_buffer_peek_memory (outbuf, 0));

  /* For OpenCL-based 2d device, need get outbuf and handle it in some cases */
  dst.outbuf = outbuf;

  if (imxvct->keep_ratio) {
    GstVideoFormatFlags out_flags;
    gboolean need_sw_fill = FALSE;

    out_flags = GST_VIDEO_FORMAT_INFO_FLAGS (filter->out_info.finfo);
    if (device->fill) {
      if(((out_flags & COLORSPACE_MASK) == GST_VIDEO_FORMAT_FLAG_YUV &&
          device->device_type == IMX_2D_DEVICE_G2D) ||
          device->fill (device, &dst, DEFAULT_IMXVIDEOCONVERT_BACKGROUND) < 0) {
        GST_LOG("fill color background by device failed");
        need_sw_fill = TRUE;
      }
    } else {
      GST_LOG("device has no fill interface");
      need_sw_fill = TRUE;
    }

    // set black background when need keep ratio
    if (need_sw_fill) {
      GstMapInfo map;
      gboolean need_unmap = FALSE;

      if (!dst.mem->vaddr) {
        if(gst_buffer_map (outbuf, &map, GST_MAP_WRITE)) {
          dst.mem->vaddr = map.data;
          dst.mem->size = map.size;
          need_unmap = TRUE;
          GST_LOG("map background buffer %p size %" G_GSIZE_FORMAT, dst.mem->vaddr, dst.mem->size);
        }
      }
      imx_2d_device_fill_background (&dst, DEFAULT_IMXVIDEOCONVERT_BACKGROUND);

      if (need_unmap)
        gst_buffer_unmap (outbuf, &map);
    }
  }

  gint64 start_time = g_get_monotonic_time ();
  //convert
  if (device->convert(device, &dst, &src) == 0) {
    imxvct->total_frames++;
    imxvct->total_time += g_get_monotonic_time () - start_time;
    GST_TRACE_OBJECT (imxvct, "frame conversion done, time: %" G_GUINT64_FORMAT, g_get_monotonic_time () - start_time);

    if (!_get_cached_phyaddr (gst_buffer_peek_memory (input_buf, 0)))
      _set_cached_phyaddr (gst_buffer_peek_memory (input_buf, 0), src.mem->paddr);
    if (src.fd[1] >= 0 && !_get_cached_phyaddr (gst_buffer_peek_memory (input_buf, 1)))
      _set_cached_phyaddr (gst_buffer_peek_memory (input_buf, 1), (guint8 *)src.mem->user_data);
    if (!_get_cached_phyaddr (gst_buffer_peek_memory (outbuf, 0)))
      _set_cached_phyaddr (gst_buffer_peek_memory (outbuf, 0), dst.mem->paddr);

    if (imxvct->composition_meta_enable) {
      if (imx_video_overlay_composition_has_meta(inbuf)) {
        VideoCompositionVideoInfo in_v, out_v;
        memset (&in_v, 0, sizeof(VideoCompositionVideoInfo));
        memset (&out_v, 0, sizeof(VideoCompositionVideoInfo));
        in_v.buf = inbuf;
        in_v.fmt = src.info.fmt;
        in_v.width = src.info.w;
        in_v.height = src.info.h;
        in_v.stride = src.info.stride;
        in_v.rotate = src.rotate;
        in_v.crop_x = src.crop.x;
        in_v.crop_y = src.crop.y;
        in_v.crop_w = src.crop.w;
        in_v.crop_h = src.crop.h;

        out_v.mem = dst.mem;
        out_v.fmt = dst.info.fmt;
        out_v.width = dst.info.w;
        out_v.height = dst.info.h;
        out_v.stride = dst.info.stride;
        out_v.rotate = IMX_2D_ROTATION_0;
        out_v.crop_x = dst.crop.x;
        out_v.crop_y = dst.crop.y;
        out_v.crop_w = dst.crop.w;
        out_v.crop_h = dst.crop.h;

        memcpy(&out_v.align, &(imxvct->out_video_align),
                sizeof(GstVideoAlignment));

        gint cnt = imx_video_overlay_composition_composite(&imxvct->video_comp,
                                                          &in_v, &out_v, FALSE);

        if (cnt >= 0) {
          imx_video_overlay_composition_remove_meta(outbuf);
          GST_DEBUG ("processed %d video overlay composition buffers", cnt);
        } else {
          GST_WARNING ("video overlay composition meta handling failed");
        }
      }
    } else {
      if (imx_video_overlay_composition_has_meta(inbuf) &&
          !imx_video_overlay_composition_has_meta(outbuf)) {
        imx_video_overlay_composition_copy_meta(outbuf, inbuf,
            src.crop.w, src.crop.h, dst.crop.w, dst.crop.h);
      }
    }

    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}

static GstFlowReturn
imx_video_convert_transform_frame_ip(GstVideoFilter *filter, GstVideoFrame *in)
{
    return GST_FLOW_OK;
}

static GstFlowReturn
imx_video_convert_transform_ip(GstBaseTransform * trans, GstBuffer * buf)
{
  GstVideoFilter *filter = GST_VIDEO_FILTER_CAST(trans);
  GstImxVideoConvert *imxvct = (GstImxVideoConvert *)(trans);
  GstPhyMemMeta *phymemmeta = NULL;

  if (imxvct->composition_meta_enable) {
  if (!(gst_buffer_is_phymem(buf)
        || gst_is_dmabuf_memory (gst_buffer_peek_memory (buf, 0)))) {
      gpointer state = NULL;
      GstMeta *meta;
      GstVideoOverlayCompositionMeta *compmeta;

      while ((meta = gst_buffer_iterate_meta (buf, &state))) {
        if (meta->info &&
            meta->info->api == GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE) {
          compmeta = (GstVideoOverlayCompositionMeta*)meta;
          if (GST_IS_VIDEO_OVERLAY_COMPOSITION (compmeta->overlay)) {
            GstVideoFrame in_frame;

            gst_video_frame_map (&in_frame, &filter->in_info, buf,
              GST_MAP_READ | GST_VIDEO_FRAME_MAP_FLAG_NO_REF);

            gst_video_overlay_composition_blend (compmeta->overlay, &in_frame);
            gst_video_frame_unmap (&in_frame);
          }
        }
      }

      imx_video_overlay_composition_remove_meta(buf);
      return GST_FLOW_OK;
    } else if (imx_video_overlay_composition_has_meta(buf)) {
      if (imxvct->pool_config_update) {
        if (imxvct->in_pool && gst_buffer_pool_is_active (imxvct->in_pool)) {
          GstStructure *config = gst_buffer_pool_get_config (imxvct->in_pool);
          memset (&imxvct->in_video_align, 0, sizeof(GstVideoAlignment));
          if (gst_buffer_pool_config_has_option (config,
              GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
            gst_buffer_pool_config_get_video_alignment (config,
                &imxvct->in_video_align);
          }

          gst_structure_free (config);
        } else {
          memset (&imxvct->in_video_align, 0, sizeof(GstVideoAlignment));
          phymemmeta = GST_PHY_MEM_META_GET (buf);
          if (phymemmeta) {
            imxvct->in_video_align.padding_right = phymemmeta->x_padding;
            imxvct->in_video_align.padding_bottom = phymemmeta->y_padding;
          }
        }

        imxvct->pool_config_update = FALSE;
      }

      gint crop_x = 0;
      gint crop_y = 0;
      guint crop_w = filter->in_info.width;
      guint crop_h = filter->in_info.height;

      GstVideoCropMeta *in_crop = gst_buffer_get_video_crop_meta(buf);
      if (in_crop != NULL) {
        if ((in_crop->x < filter->in_info.width) && (in_crop->y < filter->in_info.height)) {
          crop_x += in_crop->x;
          crop_y += in_crop->y;
          crop_w = MIN(in_crop->width, filter->in_info.width);
          crop_h = MIN(in_crop->height, filter->in_info.height);
        }
      }

      VideoCompositionVideoInfo in_v, out_v;
      PhyMemBlock src_mem = {0};
      guint i, n_mem;

      memset (&in_v, 0, sizeof(VideoCompositionVideoInfo));
      memset (&out_v, 0, sizeof(VideoCompositionVideoInfo));
      in_v.buf = buf;
      in_v.fmt = out_v.fmt = GST_VIDEO_INFO_FORMAT(&(filter->in_info));
      in_v.width = out_v.width = filter->in_info.width;
      in_v.height = out_v.height = filter->in_info.height;
      in_v.stride = out_v.stride = filter->in_info.stride[0];
      in_v.rotate = out_v.rotate = IMX_2D_ROTATION_0;
      in_v.crop_x = out_v.crop_x = crop_x;
      in_v.crop_y = out_v.crop_y = crop_y;
      in_v.crop_w = out_v.crop_w = crop_w;
      in_v.crop_h = out_v.crop_h = crop_h;

      if (gst_is_dmabuf_memory (gst_buffer_peek_memory (buf, 0))) {
        out_v.mem = &src_mem;
        n_mem = gst_buffer_n_memory (buf);
        for (i = 0; i < n_mem; i++)
          out_v.fd[i] = gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (buf, i));
      } else
        out_v.mem = gst_buffer_query_phymem_block (buf);
      memcpy(&out_v.align, &(imxvct->in_video_align),sizeof(GstVideoAlignment));

      gint cnt = imx_video_overlay_composition_composite(&imxvct->video_comp,
                                                         &in_v, &out_v, TRUE);

      if (cnt >= 0) {
        imx_video_overlay_composition_remove_meta(buf);
        GST_DEBUG ("processed %d video overlay composition buffers", cnt);
      } else {
        GST_WARNING ("video overlay composition meta handling failed");
      }
    } else {
      GST_DEBUG("no video overlay composition meta");
    }
  }

  return GST_FLOW_OK;
}

static void
gst_imx_video_convert_class_init (GstImxVideoConvertClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS(klass);
  GstCaps *caps;

  Imx2DDeviceInfo *in_plugin = (Imx2DDeviceInfo *)
      g_type_get_qdata (G_OBJECT_CLASS_TYPE (klass), GST_IMX_VCT_PARAMS_QDATA);
  g_assert (in_plugin != NULL);

  Imx2DDevice* dev = in_plugin->create(in_plugin->device_type);
  if (!dev)
    return;

  gchar longname[64] = {0};
  gchar desc[64] = {0};
  gint capabilities = dev->get_capabilities(dev);

  snprintf(longname, 32, "IMX %s Video Converter", in_plugin->name);
  snprintf(desc, 64, "Video CSC/Resize/Rotate%s",
      (capabilities&IMX_2D_DEVICE_CAP_DEINTERLACE) ? "/Deinterlace." : ".");
  gst_element_class_set_static_metadata (element_class, longname,
        "Filter/Converter/Video", desc, IMX_GST_PLUGIN_AUTHOR);

  GList *list = dev->get_supported_in_fmts(dev);
  caps = imx_video_convert_caps_from_fmt_list(list);
  g_list_free(list);

  if (!caps) {
    GST_ERROR ("Couldn't create caps for device '%s'", in_plugin->name);
    caps = gst_caps_new_empty_simple ("video/x-raw");
  }
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));

#ifdef PASSTHOUGH_FOR_UNSUPPORTED_OUTPUT_FORMAT
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                            gst_caps_copy(caps)));
#else
  list = dev->get_supported_out_fmts(dev);
  caps = imx_video_convert_caps_from_fmt_list(list);
  g_list_free(list);

  if (!caps) {
    GST_ERROR ("Couldn't create caps for device '%s'", in_plugin->name);
    caps = gst_caps_new_empty_simple ("video/x-raw");
  }
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
#endif
  klass->in_plugin = in_plugin;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_imx_video_convert_finalize;
  gobject_class->set_property = gst_imx_video_convert_set_property;
  gobject_class->get_property = gst_imx_video_convert_get_property;

  if (capabilities & IMX_2D_DEVICE_CAP_ROTATE) {
    g_object_class_install_property (gobject_class, PROP_OUTPUT_ROTATE,
        g_param_spec_enum("rotation", "Output rotation",
          "Rotation that shall be applied to output frames",
          gst_imx_video_convert_rotation_get_type(),
          GST_IMX_VIDEO_ROTATION_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  if (capabilities & IMX_2D_DEVICE_CAP_DEINTERLACE) {
    g_object_class_install_property (gobject_class, PROP_DEINTERLACE_MODE,
        g_param_spec_enum("deinterlace", "Deinterlace mode",
          "Deinterlacing mode to be used for incoming frames "
          "(ignored if frames are not interlaced)",
          gst_imx_video_convert_deinterlace_get_type(),
          GST_IMX_VIDEO_DEINTERLACE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  if (capabilities & IMX_2D_DEVICE_CAP_BLEND ||
      capabilities & IMX_2D_DEVICE_CAP_OVERLAY) {
    g_object_class_install_property (gobject_class, PROP_COMPOSITION_META_ENABLE,
        g_param_spec_boolean("composition-meta-enable", "Enable composition meta",
          "Enable overlay composition meta processing",
          GST_IMX_VIDEO_COMPOMETA_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
    PROP_COMPOSITION_META_IN_PLACE,
    g_param_spec_boolean("in-place", "Handle composition meta in place",
      "Handle composition meta in place in pass through mode, "
      "video overlay composition will blended onto input buffer",
      GST_IMX_VIDEO_COMPOMETA_IN_PLACE_DEFAULT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  if (in_plugin->device_type == IMX_2D_DEVICE_G2D ||
      in_plugin->device_type == IMX_2D_DEVICE_OCL) {
    g_object_class_install_property (gobject_class,
        PROP_VIDEOCROP_META_ENABLE,
        g_param_spec_boolean("videocrop-meta-enable", "process buffer's videocrop meta",
          "Enable videocrop meta processing",
          GST_IMX_VIDEO_VIDEOCROP_META_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  if (capabilities & IMX_2D_DEVICE_CAP_WARP) {
    g_object_class_install_property (gobject_class, PROP_VIDEO_WARP_ENABLE,
        g_param_spec_boolean("video-warp-enable", "video warp enable",
            "Enable video warp",
            GST_IMX_VIDEO_WARP_DEFAULT,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_VIDEO_WARP_COORD_FILE,
      g_param_spec_string ("video-warp-coord-file", "video warp coord file",
          "Video warp coordinates file location", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

    g_object_class_install_property (gobject_class, PROP_VIDEO_WARP_EXTRA_CONTROLS,
        g_param_spec_boxed ("video-warp-extra-controls", "Video warp extra controls",
            "Extra the video warp parameters",
            GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  g_object_class_install_property (gobject_class, PROP_KEEP_RATIO,
      g_param_spec_boolean ("keep-ratio", "Keep Aspect Ratio",
        "Keep the video aspect ratio after resize",
        GST_IMX_VIDEO_KEEP_RATIO_DEFAULT,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  in_plugin->destroy(dev);

  base_transform_class->src_event =
      GST_DEBUG_FUNCPTR(imx_video_convert_src_event);
  base_transform_class->transform_caps =
      GST_DEBUG_FUNCPTR(imx_video_convert_transform_caps);
  base_transform_class->fixate_caps =
      GST_DEBUG_FUNCPTR(imx_video_convert_fixate_caps);
  base_transform_class->filter_meta =
      GST_DEBUG_FUNCPTR (gst_imx_video_convert_filter_meta);
  base_transform_class->propose_allocation =
      GST_DEBUG_FUNCPTR(imx_video_convert_propose_allocation);
  base_transform_class->decide_allocation =
      GST_DEBUG_FUNCPTR(imx_video_convert_decide_allocation);
  video_filter_class->set_info =
      GST_DEBUG_FUNCPTR(imx_video_convert_set_info);
  video_filter_class->transform_frame =
       GST_DEBUG_FUNCPTR(imx_video_convert_transform_frame);
  video_filter_class->transform_frame_ip =
       GST_DEBUG_FUNCPTR(imx_video_convert_transform_frame_ip);
  base_transform_class->transform =
      GST_DEBUG_FUNCPTR(imx_video_convert_transform);
  base_transform_class->transform_ip =
      GST_DEBUG_FUNCPTR(imx_video_convert_transform_ip);
  base_transform_class->accept_caps =
    GST_DEBUG_FUNCPTR (imx_video_convert_accept_caps);

  base_transform_class->passthrough_on_same_caps = TRUE;
}

static void
gst_imx_video_convert_init (GstImxVideoConvert * imxvct)
{
  GstImxVideoConvertClass *klass =
      (GstImxVideoConvertClass *) G_OBJECT_GET_CLASS (imxvct);

  if (klass->in_plugin)
    imxvct->device = klass->in_plugin->create(klass->in_plugin->device_type);

  if (imxvct->device) {
    if (imxvct->device->open(imxvct->device) < 0) {
      GST_ERROR ("Open video process device failed.");
    } else {
      imxvct->in_buf = NULL;
      imxvct->in_pool = NULL;
      imxvct->out_pool = NULL;
      imxvct->self_out_pool = NULL;
      imxvct->pool_config_update = TRUE;
      imxvct->video_meta_check = FALSE;
      imxvct->rotate = IMX_2D_ROTATION_0;
      imxvct->deinterlace = IMX_2D_DEINTERLACE_NONE;
      imxvct->composition_meta_enable = GST_IMX_VIDEO_COMPOMETA_DEFAULT;
      imxvct->in_place = GST_IMX_VIDEO_COMPOMETA_IN_PLACE_DEFAULT;
      imxvct->videocrop_meta_enable = GST_IMX_VIDEO_VIDEOCROP_META_DEFAULT;
      imx_video_overlay_composition_init(&imxvct->video_comp, imxvct->device);
      memset (&imxvct->video_warp, 0, sizeof(Imx2DVideoWarp));
      imxvct->video_warp.enable = GST_IMX_VIDEO_WARP_DEFAULT;
      imxvct->video_warp.map_format = GST_IMX_VIDEO_WARP_MAP_DEFAULT;
      imxvct->keep_ratio = GST_IMX_VIDEO_KEEP_RATIO_DEFAULT;
      imxvct->total_time = 0;
      imxvct->total_frames = 0;
    }
  } else {
    GST_ERROR ("Create video process device failed.");
  }
}

static gboolean gst_imx_video_convert_register (GstPlugin * plugin)
{
  GTypeInfo tinfo = {
    sizeof (GstImxVideoConvertClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_imx_video_convert_class_init,
    NULL,
    NULL,
    sizeof (GstImxVideoConvert),
    0,
    (GInstanceInitFunc) gst_imx_video_convert_init,
  };

  GType type;
  gchar *t_name;

  const Imx2DDeviceInfo *in_plugin = imx_get_2d_devices();

  while (in_plugin->name) {
    GST_LOG ("Registering %s video converter", in_plugin->name);

    if (!in_plugin->is_exist()) {
      GST_WARNING("device %s not exist", in_plugin->name);
      in_plugin++;
      continue;
    }

    t_name = g_strdup_printf ("imxvideoconvert_%s", in_plugin->name);
    type = g_type_from_name (t_name);

    if (!type) {
      type = g_type_register_static (GST_TYPE_VIDEO_FILTER, t_name, &tinfo, 0);
      g_type_set_qdata (type, GST_IMX_VCT_PARAMS_QDATA, (gpointer) in_plugin);
    }

    if (!gst_element_register (plugin, t_name, IMX_GST_PLUGIN_RANK, type)) {
      GST_ERROR ("Failed to register %s", t_name);
      g_free (t_name);
      return FALSE;
    }
    g_free (t_name);

    in_plugin++;
  }

  return TRUE;
}

static gboolean plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (imxvideoconvert_debug, "imxvideoconvert", 0,
      "Freescale IMX Video Convert element");

  return gst_imx_video_convert_register (plugin);
}

IMX_GST_PLUGIN_DEFINE(imxvideoconvert, "IMX Video Convert Plugins",plugin_init);
