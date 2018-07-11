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

/**
 * SECTION:element-imxcompositor
 *
 * imxompositor can composite a group of video streams with different format
 * into one video frame. videos can be overlapped or alpha blent if the
 * corresponding hardware support. videos will be performed colorspace
 * conversion and scaling as well as rotation. For each of the requested
 * sink pads it will compare the incoming geometry and framerate to define the
 * output parameters. Indeed output video frames will have the geometry of the
 * biggest incoming video stream and the framerate of the fastest incoming one.
 *
 * Individual parameters for each input stream can be configured on the
 * #GstImxCompositorPad.
 * <itemizedlist>
 * <listitem>
 * "xpos": The x-coordinate position of the top-left corner of the picture
 * (#gint)
 * </listitem>
 * <listitem>
 * "ypos": The y-coordinate position of the top-left corner of the picture
 * (#gint)
 * </listitem>
 * <listitem>
 * "width": The width of the picture; the input will be scaled if necessary
 * (#gint)
 * </listitem>
 * <listitem>
 * "height": The height of the picture; the input will be scaled if necessary
 * (#gint)
 * </listitem>
 * <listitem>
 * "alpha": The transparency of the picture; between 0.0 and 1.0. This feature
 * depends on the underlying hardware, if hardware don't support, then alpha
 * will be ignored
 * (#gdouble)
 * </listitem>
 * <listitem>
 * "rotate": The rotation of the picture in the composition
 * (#guint)
 * </listitem>
 * <listitem>
 * "keep-ratio": Keep the aspect ratio of the picture after resize
 * (#gboolean)
 * </listitem>
 * <listitem>
 * "zorder": The z-order position of the picture in the composition
 * (#guint)
 * </listitem>
 * </itemizedlist>
 *
 * <refsect2>
 * <title>Sample pipelines</title>
 * |[
 * gst-launch-1.0 imxcompositor_pxp background=0x0000FFFF name=comp
 *   sink_0::alpha=0.6
 *   sink_1::alpha=0.8 sink_1::xpos=100 sink_1::ypos=120
 *   sink_2::xpos=320 sink_2::ypos=240 ! imxv4l2sink
 *   videotestsrc ! video/x-raw,format=RGB16,width=320,height=240 ! comp.sink_0
 *   videotestsrc ! video/x-raw,format=RGB16,width=800, height=600 ! comp.sink_1
 *   videotestsrc ! video/x-raw, width=1280, height=720 ! comp.sink_2
 * ]| A pipeline to demonstrate imxcompositor_pxp
 * This should show a color bar 320x240 with alpha 0.6
 * showing the yellow background. showing a color bar 800x600 at (100,120) with
 * alpha 0.8 overlapped with previous one. showing a color bar 1280x720 at
 * (320,240) with alpha 1.0 overlapped with previous one.
 * |[
 * gst-launch-1.0 imxcompositor_ipu background=0x0000FFFF name=comp
 *   sink_0::xpos=0 sink_0::ypos=0
 *   sink_1::xpos=96 sink_1::ypos=96 sink_1::width=800 sink_1::height=400
 *   sink_2::xpos=400 sink_2::ypos=300 sink_2::width=400 sink_2::height=300
 *   ! video/x-raw,format=RGBx,width=1024,height=768 ! overlaysink
 *   videotestsrc ! video/x-raw,format=RGB16,width=320,height=240 ! comp.sink_0
 *   videotestsrc ! video/x-raw,format=RGB16,width=1080,height=720 ! comp.sink_1
 *   videotestsrc ! video/x-raw, width=800, height=400 ! comp.sink_2
 * ]|A pipeline using imxcompositor_ipu to composite videos with scaling and
 * blending
 * |[
 * gst-launch-1.0 imxcompositor_g2d background=0x00FFFFFF name=comp
 *   sink_0::alpha=0.6 sink_0::xpos=50 sink_0::ypos=50 sink_0::width=1280
 *   sink_0::height=720
 *   sink_1::alpha=0.8 sink_1::xpos=300 sink_1::ypos=400 sink_1::width=800
 *   sink_1::height=540 sink_1::rotate=1
 *   sink_2::alpha=0.7 sink_2::xpos=800 sink_2::ypos=500 sink_2::width=640
 *   sink_2::height=480
 *   sink_3::xpos=900 sink_3::ypos=200 sink_3::alpha=0.5 sink_3::width=720
 *   sink_3::height=480 ! overlaysink sync=false
 *   uridecodebin uri=file://$FILE1 ! comp.sink_0
 *   uridecodebin uri=file://$FILE2 ! comp.sink_1
 *   uridecodebin uri=file://$FILE3 ! comp.sink_2
 *   videotestsrc ! video/x-raw,format=RGB16,width=1280,height=720 ! comp.sink_3
 * ]| A pipeline using imxcompositor_g2d to composite video from decoders and
 * videotestsrc by scaling, rotation and alpha blending
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gst/allocators/gstdmabuf.h>
#include <gst/allocators/gstdmabufmeta.h>
#include <libdrm/drm_fourcc.h>
#include <gst/allocators/gstallocatorphymem.h>
#include <gst/allocators/gstphymemmeta.h>
#ifdef USE_ION
#include <gst/allocators/gstionmemory.h>
#endif
#include "gstimxcompositor.h"
#include "gstimxcompositorpad.h"

//#define USE_GST_VIDEO_SAMPLE_CONVERT  //bad performance

#define IMX_COMPOSITOR_INPUT_POOL_MIN_BUFFERS   1
#define IMX_COMPOSITOR_INPUT_POOL_MAX_BUFFERS   30
#define IMX_COMPOSITOR_OUTPUT_POOL_MIN_BUFFERS   3
#define IMX_COMPOSITOR_OUTPUT_POOL_MAX_BUFFERS   30
#define IMX_COMPOSITOR_COMPOMETA_DEFAULT         FALSE

#define IMX_COMPOSITOR_CSC_LOSS_FACTOR          5 // 0 ~ 10
#define IMX_COMPOSITOR_CSC_COMPLEX_FACTOR (10 - IMX_COMPOSITOR_CSC_LOSS_FACTOR)

#define DEFAULT_IMXCOMPOSITOR_BACKGROUND        0x00000000

#define GST_IMX_COMPOSITOR_PARAMS_QDATA   \
          g_quark_from_static_string("imxcompositor-params")

#define GST_IMX_COMPOSITOR_UNREF_BUFFER(buffer) {\
    if (buffer) {                             \
      GST_LOG ("unref buffer (%p)", buffer);  \
      gst_buffer_unref(buffer);               \
      buffer = NULL;                          \
    }                                         \
  }

#define GST_IMX_COMPOSITOR_UNREF_POOL(pool)  {   \
    if (pool) {                               \
      GST_LOG ("unref pool (%p)", pool);      \
      gst_buffer_pool_set_active (pool, FALSE);\
      gst_object_unref(pool);                 \
      pool = NULL;                            \
    }                                         \
  }

GST_DEBUG_CATEGORY_STATIC (gst_imxcompositor_debug);
#define GST_CAT_DEFAULT gst_imxcompositor_debug

/* properties utility*/
enum {
  PROP_0,
#if 0
  PROP_IMXCOMPOSITOR_OUTPUT_WIDTH,
  PROP_IMXCOMPOSITOR_OUTPUT_HEIGHT,
#endif
  PROP_IMXCOMPOSITOR_BACKGROUND_ENABLE,
  PROP_IMXCOMPOSITOR_BACKGROUND_COLOR,
  PROP_IMXCOMPOSITOR_COMPOSITION_META_ENABLE
};

static GstElementClass *parent_class = NULL;

static void gst_imxcompositor_finalize (GObject * object)
{
  GstImxCompositor *imxcomp = (GstImxCompositor *)(object);
  GstStructure *config;
  GstImxCompositorClass *klass =
        (GstImxCompositorClass *) G_OBJECT_GET_CLASS (imxcomp);

  imx_video_overlay_composition_deinit(&imxcomp->video_comp);

  GST_IMX_COMPOSITOR_UNREF_POOL (imxcomp->out_pool);
  if (imxcomp->allocator) {
    gst_object_unref (imxcomp->allocator);
    imxcomp->allocator = NULL;
  }

  if (imxcomp->device) {
    imxcomp->device->close(imxcomp->device);
    if (klass->in_plugin)
      klass->in_plugin->destroy(imxcomp->device);
    imxcomp->device = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (imxcomp));
}

static void
gst_imxcompositor_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstImxCompositor *imxcomp = (GstImxCompositor *) (object);

  switch (prop_id) {
    case PROP_IMXCOMPOSITOR_BACKGROUND_ENABLE:
      g_value_set_boolean(value, imxcomp->background_enable);
      break;
    case PROP_IMXCOMPOSITOR_BACKGROUND_COLOR:
      g_value_set_uint (value, imxcomp->background);
      break;
    case PROP_IMXCOMPOSITOR_COMPOSITION_META_ENABLE:
      g_value_set_boolean(value, imxcomp->composition_meta_enable);
      break;
#if 0
    case PROP_IMXCOMPOSITOR_OUTPUT_WIDTH:
      g_value_set_uint (value, imxcomp->width);
      break;
    case PROP_IMXCOMPOSITOR_OUTPUT_HEIGHT:
      g_value_set_uint (value, imxcomp->height);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_imxcompositor_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstImxCompositor *imxcomp = (GstImxCompositor *) (object);

  switch (prop_id) {
    case PROP_IMXCOMPOSITOR_BACKGROUND_ENABLE:
      imxcomp->background_enable = g_value_get_boolean(value);
      break;
    case PROP_IMXCOMPOSITOR_BACKGROUND_COLOR:
      imxcomp->background = g_value_get_uint (value);
      break;
    case PROP_IMXCOMPOSITOR_COMPOSITION_META_ENABLE:
      imxcomp->composition_meta_enable = g_value_get_boolean(value);
      break;
#if 0
    case PROP_IMXCOMPOSITOR_OUTPUT_WIDTH:
      imxcomp->width = g_value_get_uint (value);
      break;
    case PROP_IMXCOMPOSITOR_OUTPUT_HEIGHT:
      imxcomp->height = g_value_get_uint (value);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_imxcompositor_set_pool_alignment(GstCaps *caps, GstBufferPool *pool)
{
  GstVideoInfo info;
  GstVideoAlignment alignment;
  GstStructure *config = gst_buffer_pool_get_config(pool);
  gst_video_info_from_caps (&info, caps);

  memset (&alignment, 0, sizeof (GstVideoAlignment));

  gint w = GST_VIDEO_INFO_WIDTH (&info);
  gint h = GST_VIDEO_INFO_HEIGHT (&info);
  if (!ISALIGNED (w, ALIGNMENT) || !ISALIGNED (h, ALIGNMENT)) {
    alignment.padding_right = ALIGNTO (w, ALIGNMENT) - w;
    alignment.padding_bottom = ALIGNTO (h, ALIGNMENT) - h;
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

static GstBufferPool*
gst_imxcompositor_create_bufferpool(GstImxCompositor *imxcomp,
                    GstCaps *caps, guint size, guint min, guint max)
{
  GstBufferPool *pool;
  GstStructure *config;
  pool = gst_video_buffer_pool_new ();
  if (pool) {
    if (!imxcomp->allocator) {
#ifdef USE_ION
      imxcomp->allocator = gst_ion_allocator_obtain ();
#endif
    }

    if (!imxcomp->allocator)
      imxcomp->allocator =
          gst_imx_2d_device_allocator_new((gpointer)(imxcomp->device));

    if (!imxcomp->allocator) {
      GST_ERROR ("new imx compositor allocator failed.");
      gst_buffer_pool_set_active (pool, FALSE);
      gst_object_unref (pool);
      return NULL;
    }

    config = gst_buffer_pool_get_config(pool);
    gst_buffer_pool_config_set_params(config, caps, size, min, max);
    gst_buffer_pool_config_set_allocator(config, imxcomp->allocator, NULL);
    gst_buffer_pool_config_add_option(config,GST_BUFFER_POOL_OPTION_VIDEO_META);
    if (!gst_buffer_pool_set_config(pool, config)) {
      GST_ERROR ("set buffer pool config failed.");
      gst_buffer_pool_set_active (pool, FALSE);
      gst_object_unref (pool);
      return NULL;
    }
  }

  gst_imxcompositor_set_pool_alignment(caps, pool);

  GST_LOG ("created a buffer pool (%p).", pool);
  return pool;
}

static gboolean
gst_imxcompositor_sink_query (GstAggregator * agg, GstAggregatorPad * bpad,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstImxCompositor *imxcomp = (GstImxCompositor *) (agg);

  GST_TRACE ("QUERY %" GST_PTR_FORMAT, query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
    {
      GstImxCompositorPad *imxcompo_pad = GST_IMXCOMPOSITOR_PAD (bpad);
      GstCaps *pool_caps = NULL;
      GstStructure *config = NULL;
      GstCaps *caps;
      gboolean need_pool = FALSE;
      GstBufferPool *pool = NULL;
      GstVideoInfo info;
      guint size = 0;

      gst_query_parse_allocation (query, &caps, &need_pool);
      if (caps == NULL) {
        GST_WARNING_OBJECT (bpad, "no caps specified");
        return FALSE;
      }

      GST_DEBUG_OBJECT(bpad, "query allocation, caps: %" GST_PTR_FORMAT, caps);

      // proposal allocation pool
      if (imxcompo_pad->sink_pool) {
        config = gst_buffer_pool_get_config (imxcompo_pad->sink_pool);
        gst_buffer_pool_config_get_params(config, &pool_caps, &size, NULL, NULL);
        if (gst_caps_is_equal(pool_caps, caps)) {
          pool = imxcompo_pad->sink_pool;
          need_pool = FALSE;
        }
      }

      if (need_pool) {
        if (!gst_video_info_from_caps (&info, caps))
          return FALSE;

        size = GST_VIDEO_INFO_SIZE (&info);
        pool = gst_imxcompositor_create_bufferpool(imxcomp, caps, size,
            IMX_COMPOSITOR_INPUT_POOL_MIN_BUFFERS,
            IMX_COMPOSITOR_INPUT_POOL_MAX_BUFFERS);
        if (pool) {
          GST_IMX_COMPOSITOR_UNREF_POOL(imxcompo_pad->sink_pool);
          imxcompo_pad->sink_pool = pool;
          imxcompo_pad->sink_pool_update = TRUE;
        }
      }

      if (pool) {
        GST_DEBUG_OBJECT (bpad, "propose_allocation, pool(%p).", pool);
        GstStructure *config = gst_buffer_pool_get_config (pool);
        gst_buffer_pool_config_get_params (config, &caps, &size, NULL, NULL);
        gst_structure_free (config);

        gst_query_add_allocation_pool (query, pool, size,
            IMX_COMPOSITOR_INPUT_POOL_MIN_BUFFERS,
            IMX_COMPOSITOR_INPUT_POOL_MAX_BUFFERS);
        gst_query_add_allocation_param (query, imxcomp->allocator, NULL);
      }

      gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, 0);
      gst_query_add_allocation_meta(query,GST_VIDEO_CROP_META_API_TYPE, NULL);
      if (imxcomp->composition_meta_enable)
        imx_video_overlay_composition_add_query_meta (query);

      GST_DEBUG_OBJECT (bpad, "ALLOCATION ret %p, %"
                        GST_PTR_FORMAT, imxcompo_pad->sink_pool, query);
      ret = TRUE;
      break;
    }
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;
      GstCaps *srccaps;
      GstCaps *sink_template;

      gst_query_parse_caps (query, &filter);

      GstCaps *filtered_caps;
      GstStructure *s;
      gint i, n;

      srccaps = gst_pad_get_current_caps (GST_PAD (agg->srcpad));
      if (srccaps == NULL)
        srccaps = gst_pad_get_pad_template_caps (GST_PAD (agg->srcpad));

      srccaps = gst_caps_make_writable (srccaps);

      n = gst_caps_get_size (srccaps);
      for (i = 0; i < n; i++) {
        s = gst_caps_get_structure (srccaps, i);
        gst_structure_set (s, "width", GST_TYPE_INT_RANGE, 64, G_MAXINT32,
            "height", GST_TYPE_INT_RANGE, 64, G_MAXINT32,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT32, 1, NULL);
        gst_structure_remove_fields (s, "colorimetry", "chroma-site", "format",
            "pixel-aspect-ratio", NULL);
      }

      sink_template = gst_pad_get_pad_template_caps (GST_PAD (bpad));
      filtered_caps = gst_caps_intersect(srccaps, sink_template);

      GST_LOG_OBJECT(bpad, "srccaps: %" GST_PTR_FORMAT, srccaps);
      GST_LOG_OBJECT(bpad, "sink_template: %" GST_PTR_FORMAT, sink_template);
      GST_LOG_OBJECT(bpad, "filtered_caps: %" GST_PTR_FORMAT, filtered_caps);

      if (imxcomp->composition_meta_enable)
        imx_video_overlay_composition_add_caps(filtered_caps);
      else
        imx_video_overlay_composition_remove_caps(filtered_caps);

      if (filter)
        caps = gst_caps_intersect (filtered_caps, filter);
      else
        caps = filtered_caps;

      GST_LOG_OBJECT(bpad, "query sink caps: %" GST_PTR_FORMAT, caps);
      gst_query_set_caps_result (query, caps);
      if (filter)
        gst_caps_unref (filtered_caps);
      gst_caps_unref (srccaps);
      gst_caps_unref (sink_template);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    case GST_QUERY_ACCEPT_CAPS:
    {
      GstCaps *caps;
      gst_query_parse_accept_caps (query, &caps);
      GstCaps *accepted_caps;
      GstCaps *sink_template;
      GstCaps *srccaps;
      gint i, n;
      GstStructure *s;

      GST_DEBUG_OBJECT (bpad, "%" GST_PTR_FORMAT, caps);

      srccaps = gst_pad_get_current_caps (GST_PAD (agg->srcpad));
      if (srccaps == NULL)
        srccaps = gst_pad_get_pad_template_caps (GST_PAD (agg->srcpad));

      srccaps = gst_caps_make_writable (srccaps);

      GST_LOG_OBJECT (bpad, "sink caps %" GST_PTR_FORMAT, srccaps);

      n = gst_caps_get_size (srccaps);
      for (i = 0; i < n; i++) {
        s = gst_caps_get_structure (srccaps, i);
        gst_structure_set (s, "width", GST_TYPE_INT_RANGE, 64, G_MAXINT32,
            "height", GST_TYPE_INT_RANGE, 64, G_MAXINT32,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT32, 1, NULL);

        gst_structure_remove_fields (s, "colorimetry", "chroma-site", "format",
            "pixel-aspect-ratio", NULL);
      }

      sink_template = gst_pad_get_pad_template_caps (GST_PAD (bpad));
      accepted_caps = gst_caps_intersect(srccaps, sink_template);
      gst_caps_unref(srccaps);

      ret = gst_caps_can_intersect (caps, accepted_caps);
      GST_DEBUG_OBJECT (bpad, "%saccepted caps %" GST_PTR_FORMAT,
          (ret ? "" : "not "), caps);
      gst_caps_unref (accepted_caps);
      gst_caps_unref (sink_template);
      gst_query_set_accept_caps_result (query, ret);
      ret = TRUE;
      break;
    }
    default:
      ret = GST_AGGREGATOR_CLASS (parent_class)->sink_query (agg, bpad, query);
      break;
  }

  return ret;
}

static gint get_format_csc_loss(GstVideoFormat in_name, GstVideoFormat out_name)
{
#define SCORE_FORMAT_CHANGE       1
#define SCORE_COLORSPACE_LOSS     2     /* RGB <-> YUV */
#define SCORE_ALPHA_LOSS          4     /* lose the alpha channel */
#define SCORE_DEPTH_LOSS          8     /* change bit depth */
#define SCORE_CHROMA_W_LOSS       4     /* vertical sub-sample */
#define SCORE_CHROMA_H_LOSS       8     /* horizontal sub-sample */
#define SCORE_COLOR_LOSS         16     /* convert to GRAY */
#define COLORSPACE_MASK (GST_VIDEO_FORMAT_FLAG_YUV | \
                         GST_VIDEO_FORMAT_FLAG_RGB | GST_VIDEO_FORMAT_FLAG_GRAY)
#define LOSS_MAX  (SCORE_FORMAT_CHANGE + SCORE_COLORSPACE_LOSS + \
    SCORE_ALPHA_LOSS + SCORE_DEPTH_LOSS + SCORE_CHROMA_W_LOSS + \
    SCORE_CHROMA_H_LOSS + SCORE_COLOR_LOSS)

  gint loss = LOSS_MAX;
  GstVideoFormatFlags in_flags, out_flags;
  const GstVideoFormatInfo *in_info = gst_video_format_get_info(in_name);
  const GstVideoFormatInfo *out_info = gst_video_format_get_info(out_name);

  if (!in_info || !out_info)
    return loss;

  if (in_info == out_info)
    return 0;

  loss = SCORE_FORMAT_CHANGE;

  in_flags = GST_VIDEO_FORMAT_INFO_FLAGS (in_info);
  out_flags = GST_VIDEO_FORMAT_INFO_FLAGS (out_info);

  if ((out_flags & COLORSPACE_MASK) != (in_flags & COLORSPACE_MASK)) {
    loss += SCORE_COLORSPACE_LOSS;
    if (out_flags & GST_VIDEO_FORMAT_FLAG_GRAY)
      loss += SCORE_COLOR_LOSS;
  }

  if ((in_flags & GST_VIDEO_FORMAT_FLAG_ALPHA) &&
      !(out_flags & GST_VIDEO_FORMAT_FLAG_ALPHA))
    loss += SCORE_ALPHA_LOSS;

  if ((out_flags & GST_VIDEO_FORMAT_FLAG_YUV)
      && (in_flags & GST_VIDEO_FORMAT_FLAG_YUV)) {
    if ((in_info->h_sub[1]) < (out_info->h_sub[1]))
      loss += SCORE_CHROMA_H_LOSS;
    if ((in_info->w_sub[1]) < (out_info->w_sub[1]))
      loss += SCORE_CHROMA_W_LOSS;
  }

  if ((in_info->bits) > (out_info->bits))
    loss += SCORE_DEPTH_LOSS;

  GST_LOG("%s -> %s, loss = %d", GST_VIDEO_FORMAT_INFO_NAME(in_info),
                  GST_VIDEO_FORMAT_INFO_NAME(out_info), loss);
  return loss;
}

static gint get_format_csc_complexity(GstVideoFormat in_name,
                                      GstVideoFormat out_name)
{
#define COMPLEX_FORMAT_CHANGE       1
#define COMPLEX_DEPTH_CHANGE        2
#define COMPLEX_ALPHA_CHANGE        2
#define COMPLEX_CHROMA_W_CHANGE     4
#define COMPLEX_CHROMA_H_CHANGE     4
#define COMPLEX_COLORSPACE_CHANGE   8     /* RGB <-> YUV */
#define COMPLEX_COLOR_CHANGE        2     /* RGB/YUV <-> GRAY */
#define COMPLEX_MAX (COMPLEX_FORMAT_CHANGE + COMPLEX_DEPTH_CHANGE +\
    COMPLEX_ALPHA_CHANGE + COMPLEX_CHROMA_W_CHANGE + COMPLEX_CHROMA_H_CHANGE +\
    COMPLEX_COLORSPACE_CHANGE + COMPLEX_COLOR_CHANGE)

  gint complex = COMPLEX_MAX;
  GstVideoFormatFlags in_flags, out_flags;
  const GstVideoFormatInfo *in_info = gst_video_format_get_info(in_name);
  const GstVideoFormatInfo *out_info = gst_video_format_get_info(out_name);

  if (!in_info || !out_info)
    return complex;

  if (in_info == out_info)
    return 0;

  complex = COMPLEX_FORMAT_CHANGE;
  in_flags = GST_VIDEO_FORMAT_INFO_FLAGS (in_info);
  out_flags = GST_VIDEO_FORMAT_INFO_FLAGS (out_info);

  if ((out_flags & (GST_VIDEO_FORMAT_FLAG_YUV|GST_VIDEO_FORMAT_FLAG_RGB))
      != (in_flags & (GST_VIDEO_FORMAT_FLAG_YUV|GST_VIDEO_FORMAT_FLAG_RGB)))
    complex += COMPLEX_COLORSPACE_CHANGE;

  if ((out_flags & GST_VIDEO_FORMAT_FLAG_GRAY)
      != (in_flags & GST_VIDEO_FORMAT_FLAG_GRAY)) {
      complex += COMPLEX_COLOR_CHANGE;
      if ((in_flags & GST_VIDEO_FORMAT_FLAG_RGB)
          || (out_flags & GST_VIDEO_FORMAT_FLAG_RGB))
        complex += COMPLEX_COLOR_CHANGE;
  }

  if ((out_flags & GST_VIDEO_FORMAT_FLAG_ALPHA)
      != (in_flags & GST_VIDEO_FORMAT_FLAG_ALPHA))
    complex += COMPLEX_ALPHA_CHANGE;

  if ((out_flags & GST_VIDEO_FORMAT_FLAG_YUV)
      && (in_flags & GST_VIDEO_FORMAT_FLAG_YUV)) {
    if ((in_info->h_sub[1]) != (out_info->h_sub[1]))
      complex += COMPLEX_CHROMA_H_CHANGE;
    if ((in_info->w_sub[1]) != (out_info->w_sub[1]))
      complex += COMPLEX_CHROMA_W_CHANGE;
  }

  if ((in_info->bits) != (out_info->bits))
    complex += COMPLEX_DEPTH_CHANGE;

  GST_LOG("%s -> %s, complex = %d", GST_VIDEO_FORMAT_INFO_NAME(in_info),
                  GST_VIDEO_FORMAT_INFO_NAME(out_info), complex);
  return complex;
}

static GstVideoFormat find_best_src_format(GstAggregator *vagg, GstCaps *o_caps)
{
#define COMPLEX_ROTATE_FACTOR   1
#define COMPLEX_SCALE_FACTOR    1

  GList *l;
  gint factor_min = G_MAXINT32;
  GstVideoFormat best_fmt = GST_VIDEO_FORMAT_UNKNOWN;

  if (!(GST_ELEMENT (vagg)->sinkpads)) {
    GST_DEBUG("no sink pad yet");
    return best_fmt;
  }

  if (!o_caps || gst_caps_is_empty (o_caps)) {
    return best_fmt;
  }

  GstCaps *caps = gst_caps_normalize(o_caps);
  GST_DEBUG ("gst_caps_normalize caps: %" GST_PTR_FORMAT, caps);

  GST_OBJECT_LOCK (vagg);
  gint n = gst_caps_get_size (caps) - 1;
  for (; n >= 0; n--) {
    GstStructure *s = gst_caps_get_structure (caps, n);
    const gchar *fmt = gst_structure_get_string(s, "format");
    GstVideoFormat o_fmt = gst_video_format_from_string(fmt);
    gint factor = 0;
    GstVideoFormat i_fmt;

    for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
      GstVideoAggregatorPad *vaggpad = l->data;
      GstImxCompositorPad *pad = GST_IMXCOMPOSITOR_PAD (vaggpad);

      gint width = GST_VIDEO_INFO_WIDTH (&vaggpad->info);
      gint height = GST_VIDEO_INFO_HEIGHT (&vaggpad->info);
      if (vaggpad->info.finfo)
        i_fmt = GST_VIDEO_INFO_FORMAT(&vaggpad->info);
      else
        continue;

      if (i_fmt == GST_VIDEO_FORMAT_UNKNOWN)
        continue;

      gint resol = width * height;
      gint complex = 0;
      gint loss = 0;

      if (resol == 0)
        continue;

      if ((pad->width && pad->width != width)
          || (pad->height && pad->height != height))
        complex += resol * COMPLEX_SCALE_FACTOR;
      if (pad->rotate != IMX_2D_ROTATION_0)
        complex += resol * COMPLEX_ROTATE_FACTOR;

      complex += resol * get_format_csc_complexity(i_fmt, o_fmt);
      loss = resol * get_format_csc_loss(i_fmt, o_fmt);
      factor += IMX_COMPOSITOR_CSC_LOSS_FACTOR * loss;
      factor += IMX_COMPOSITOR_CSC_COMPLEX_FACTOR * complex;
    }
    GST_LOG("fmt %s factor %d", fmt, factor);
    if (factor < factor_min) {
      best_fmt = o_fmt;
      factor_min = factor;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  return best_fmt;
}

static gboolean
gst_imxcompositor_src_query (GstAggregator * agg, GstQuery * query)
{
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps, *tmp;
      GstStructure *s;
      gint n;

      GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);

      gst_query_parse_caps (query, &filter);

      if (GST_VIDEO_INFO_FORMAT (&vagg->info) != GST_VIDEO_FORMAT_UNKNOWN)
        caps = gst_video_info_to_caps (&vagg->info);
      else
        caps = gst_pad_get_pad_template_caps (agg->srcpad);

      caps = gst_caps_make_writable (caps);
      n = gst_caps_get_size (caps) - 1;
      for (; n >= 0; n--) {
        s = gst_caps_get_structure (caps, n);
        gst_structure_set (s, "width", GST_TYPE_INT_RANGE, 64, G_MAXINT32,
            "height", GST_TYPE_INT_RANGE, 64, G_MAXINT32, NULL);
        if (GST_VIDEO_INFO_FPS_D (&vagg->info) != 0) {
          gst_structure_set (s,
              "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT32, 1, NULL);
        }
      }

      if (filter) {
        tmp = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(caps);
        caps = tmp;
      }

      gst_query_set_caps_result (query, caps);
      GST_DEBUG ("query src caps: %" GST_PTR_FORMAT, caps);
      gst_caps_unref (caps);

      res = TRUE;
      break;
    }
    default:
      res = GST_AGGREGATOR_CLASS (parent_class)->src_query (agg, query);
      break;
  }

  return res;
}

static gboolean
gst_imxcompositor_negotiated_caps (GstVideoAggregator * vagg, GstCaps * caps)
{
  GstImxCompositor *imxcomp = (GstImxCompositor *) (vagg);
  GstQuery *query;
  gboolean result = TRUE;
  GstStructure *config = NULL;
  guint size, num, min = 0, max = 0;
  GstAggregator *agg = GST_AGGREGATOR (imxcomp);

  GST_DEBUG("negotiated caps: %" GST_PTR_FORMAT, caps);

  if (imxcomp->self_out_pool) {
    GstCaps *pool_caps;
    config = gst_buffer_pool_get_config (imxcomp->self_out_pool);
    gst_buffer_pool_config_get_params(config, &pool_caps, &size, &min, &max);
    if (gst_caps_is_equal(pool_caps, caps)) {
      gst_structure_free (config);
      imxcomp->out_pool = imxcomp->self_out_pool;
      return TRUE;
    }
    gst_structure_free (config);
  }

  /* find a pool for the negotiated caps now */
  GST_DEBUG_OBJECT (imxcomp, "doing allocation query");
  query = gst_query_new_allocation (caps, TRUE);
  if (!gst_pad_peer_query (agg->srcpad, query)) {
    // nothing, just print
    GST_DEBUG_OBJECT (imxcomp, "peer ALLOCATION query failed");
  }

  GstBufferPool *pool = NULL;
  GstVideoInfo vinfo;
  GstAllocator *allocator = NULL;

  gst_video_info_init(&vinfo);
  gst_video_info_from_caps(&vinfo, caps);
  size = vinfo.size;

  num = gst_query_get_n_allocation_pools(query);
  GST_DEBUG_OBJECT(imxcomp, "number of allocation pools: %d", num);

  /* if downstream element provided buffer pool with phy buffers */
  if (num > 0) {
    guint i = 0;
    while (i < num ) {
      gst_query_parse_nth_allocation_pool(query, i, &pool, &size, &min, &max);
      if (pool) {
        config = gst_buffer_pool_get_config(pool);
        gst_buffer_pool_config_get_allocator(config, &allocator, NULL);

        if (allocator && GST_IS_ALLOCATOR_PHYMEM(allocator)) {
          gst_imxcompositor_set_pool_alignment(caps, pool);
          if (min < IMX_COMPOSITOR_OUTPUT_POOL_MIN_BUFFERS)
            min = IMX_COMPOSITOR_OUTPUT_POOL_MIN_BUFFERS;
          max = IMX_COMPOSITOR_OUTPUT_POOL_MAX_BUFFERS;
          gst_buffer_pool_config_set_params (config, caps, size, min, max);
          gst_buffer_pool_set_config (pool, config);
          GST_IMX_COMPOSITOR_UNREF_POOL (imxcomp->out_pool);
          imxcomp->out_pool = pool;
          gst_query_unref (query);
          imxcomp->negotiated = TRUE;
          imxcomp->out_pool_update = TRUE;
          return TRUE;
        } else {
          GST_LOG_OBJECT (imxcomp, "no phy allocator in output pool (%p)",pool);
        }

        if (config) {
          gst_structure_free (config);
          config = NULL;
        }

        if (allocator) {
          gst_object_unref (allocator);
          allocator = NULL;
        }

        gst_object_unref (pool);
      }
      i++;
    }
  }
  gst_query_unref (query);

  size = PAGE_ALIGN(MAX(size, vinfo.size));

  /* downstream doesn't provide a pool or the pool has no ability to allocate
   * physical memory buffers, we need create new pool */
  GST_DEBUG_OBJECT(imxcomp, "creating new output pool");
  pool = gst_imxcompositor_create_bufferpool(imxcomp, caps, size,
      IMX_COMPOSITOR_OUTPUT_POOL_MIN_BUFFERS,
      IMX_COMPOSITOR_OUTPUT_POOL_MAX_BUFFERS);
  if (pool) {
    if (imxcomp->self_out_pool != imxcomp->out_pool) {
      GST_IMX_COMPOSITOR_UNREF_POOL(imxcomp->self_out_pool);
      GST_IMX_COMPOSITOR_UNREF_POOL (imxcomp->out_pool);
    } else {
      GST_IMX_COMPOSITOR_UNREF_POOL(imxcomp->self_out_pool);
    }
    imxcomp->self_out_pool = pool;
    imxcomp->out_pool = pool;
    gst_buffer_pool_set_active(pool, TRUE);
    GST_DEBUG_OBJECT(imxcomp, "pool config:  outcaps: %" GST_PTR_FORMAT "  "
        "size: %u  min buffers: %u  max buffers: %u", caps, size,
        IMX_COMPOSITOR_OUTPUT_POOL_MIN_BUFFERS,
        IMX_COMPOSITOR_OUTPUT_POOL_MAX_BUFFERS);
    imxcomp->negotiated = TRUE;
    imxcomp->out_pool_update = TRUE;
  } else {
    GST_WARNING_OBJECT (imxcomp, "Failed to decide allocation");
    imxcomp->negotiated = FALSE;
  }

  return imxcomp->negotiated;
}

static GstFlowReturn
gst_imxcompositor_get_output_buffer (GstVideoAggregator * vagg,
    GstBuffer ** outbuf)
{
  GstImxCompositor *imxcomp = (GstImxCompositor *) (vagg);

  if (!gst_buffer_pool_set_active (imxcomp->out_pool, TRUE)) {
    GST_ELEMENT_ERROR (imxcomp, RESOURCE, SETTINGS,
        ("failed to activate bufferpool"), ("failed to activate bufferpool"));
    return GST_FLOW_ERROR;
  }

  GstFlowReturn ret =
      gst_buffer_pool_acquire_buffer (imxcomp->out_pool, outbuf, NULL);

  GST_LOG_OBJECT(imxcomp, "Get out buffer %p from pool %p",
      *outbuf, imxcomp->out_pool);
  return ret;
}

static void
gst_imxcompositor_find_best_format (
    GstVideoAggregator * vagg, GstCaps * downstream_caps,
    GstVideoInfo * best_info, gboolean * at_least_one_alpha)
{
#if 0
  GstVideoInfo tmp_info;

  GstVideoFormat best_fmt =
      find_best_src_format(GST_AGGREGATOR(vagg), downstream_caps);

  gst_video_info_set_format (&tmp_info, best_fmt, best_info->width,
      best_info->height);
  tmp_info.par_n = best_info->par_n;
  tmp_info.par_d = best_info->par_d;
  tmp_info.fps_n = best_info->fps_n;
  tmp_info.fps_d = best_info->fps_d;
  tmp_info.flags = best_info->flags;
  tmp_info.interlace_mode = best_info->interlace_mode;
  *best_info = tmp_info;
#endif
}

static GstCaps *
gst_imxcompositor_update_caps (GstVideoAggregator * vagg, GstCaps * caps,
    GstCaps * filter)
{
  GList *l;
  gint best_width = -1, best_height = -1;
  gdouble best_fps = -1, cur_fps;
  gint best_fps_n = -1, best_fps_d = -1;
  GstVideoInfo info;
  GstCaps *ret = caps;
  GstAggregator *agg = (GstAggregator*)vagg;

  if (!gst_caps_is_fixed (caps)) {
    GstCaps *tmp = gst_caps_fixate (gst_caps_ref (caps));
    gst_video_info_from_caps (&info, tmp);
    gst_caps_unref (tmp);
  } else
    gst_video_info_from_caps (&info, caps);

  GstCaps *downstream_caps = gst_pad_get_allowed_caps (agg->srcpad);
  if (!downstream_caps || gst_caps_is_empty (downstream_caps)) {
    GST_INFO_OBJECT (vagg, "No downstream caps found %"
        GST_PTR_FORMAT, downstream_caps);
    if (downstream_caps)
      gst_caps_unref (downstream_caps);
    return ret;
  }

  GstVideoFormat best_fmt = find_best_src_format(agg, downstream_caps);
  gst_caps_unref (downstream_caps);

/*
  GstCaps *src_caps, *peer_caps;
  src_caps = gst_pad_get_pad_template_caps (agg->srcpad);
  peer_caps = gst_pad_peer_query_caps(agg->srcpad, NULL);
  src_caps = gst_caps_intersect_full(peer_caps, src_caps, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (peer_caps);

  GstVideoFormat best_fmt = find_best_src_format(agg, src_caps);
  gst_caps_unref (src_caps);
*/

  if (best_fmt != GST_VIDEO_FORMAT_UNKNOWN) {
    gst_video_info_set_format (&info, best_fmt, GST_VIDEO_INFO_WIDTH(&info),
                                GST_VIDEO_INFO_HEIGHT(&info));
    GST_DEBUG("Set best format %s", gst_video_format_to_string(best_fmt));
  }

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *vaggpad = l->data;
    GstImxCompositorPad *pad = GST_IMXCOMPOSITOR_PAD (vaggpad);
    gint this_width, this_height;
    gint fps_n, fps_d;
    gint width, height;

    fps_n = GST_VIDEO_INFO_FPS_N (&vaggpad->info);
    fps_d = GST_VIDEO_INFO_FPS_D (&vaggpad->info);
    gst_imxcompositor_pad_get_output_size(vagg, pad, &width, &height);

    if (width == 0 || height == 0)
      continue;

    if (pad->width)
      width = pad->width;
    if (pad->height)
      height = pad->height;

    this_width = width + MAX (pad->xpos, 0);
    this_height = height + MAX (pad->ypos, 0);

    if (best_width < this_width)
      best_width = this_width;
    if (best_height < this_height)
      best_height = this_height;

    if (fps_d == 0)
      cur_fps = 0.0;
    else
      gst_util_fraction_to_double (fps_n, fps_d, &cur_fps);

    if (best_fps < cur_fps) {
      best_fps = cur_fps;
      best_fps_n = fps_n;
      best_fps_d = fps_d;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  if (best_fps_n <= 0 || best_fps_d <= 0 || best_fps == 0.0) {
    best_fps_n = 25;
    best_fps_d = 1;
    best_fps = 25.0;
  }

  if (best_width > 0 && best_height > 0 && best_fps > 0) {
    gst_video_info_set_format (&info, GST_VIDEO_INFO_FORMAT (&info),
        best_width, best_height);
    info.fps_n = best_fps_n;
    info.fps_d = best_fps_d;
    ret = gst_video_info_to_caps (&info);
    GST_DEBUG("update output info %dx%d", best_width, best_height);

    gst_caps_set_simple (ret, "pixel-aspect-ratio", GST_TYPE_FRACTION_RANGE,
        1, G_MAXINT32, G_MAXINT32, 1, NULL);
  }

  vagg->info = info;

  return ret;
}

static gint gst_imxcompositor_config_dst(GstImxCompositor *imxcomp,
    GstBuffer * outbuf, Imx2DFrame *dst)
{
  GstVideoFrame out_frame;
  GstPhyMemMeta *phymemmeta = NULL;
  guint i, n_mem;

  if (!(gst_buffer_is_phymem(outbuf)
        || gst_is_dmabuf_memory (gst_buffer_peek_memory (outbuf, 0)))) {
    GST_ERROR ("out buffer is not phy memory or DMA Buf");
    return -1;
  }

  if (!gst_video_frame_map (&out_frame, &((GstVideoAggregator*)imxcomp)->info,
      outbuf, GST_MAP_WRITE|GST_VIDEO_FRAME_MAP_FLAG_NO_REF)) {
    GST_WARNING_OBJECT (imxcomp, "Could not map output buffer");
    return -1;
  }

  if (imxcomp->out_pool_update) {
    if (imxcomp->out_pool) {
      GstStructure *config = gst_buffer_pool_get_config (imxcomp->out_pool);
      memset (&imxcomp->out_align, 0, sizeof(GstVideoAlignment));

      if (gst_buffer_pool_config_has_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
        gst_buffer_pool_config_get_video_alignment (config,&imxcomp->out_align);
        GST_DEBUG ("output pool has alignment (%d, %d) , (%d, %d)",
            imxcomp->out_align.padding_left, imxcomp->out_align.padding_top,
            imxcomp->out_align.padding_right,imxcomp->out_align.padding_bottom);
      }
      gst_structure_free (config);
    }

    /* set physical memory padding info */
    if (imxcomp->self_out_pool
        && gst_buffer_is_writable (out_frame.buffer)) {
      phymemmeta = GST_PHY_MEM_META_ADD (out_frame.buffer);
      phymemmeta->x_padding = imxcomp->out_align.padding_right;
      phymemmeta->y_padding = imxcomp->out_align.padding_bottom;
      GST_DEBUG_OBJECT (imxcomp, "out physical memory meta x_padding: %d "
          "y_padding: %d", phymemmeta->x_padding, phymemmeta->y_padding);
    }

    imxcomp->out_pool_update = FALSE;
  }

  dst->info.fmt = GST_VIDEO_INFO_FORMAT(&(out_frame.info));
  dst->info.w = out_frame.info.width +
      imxcomp->out_align.padding_left + imxcomp->out_align.padding_right;
  dst->info.h = out_frame.info.height +
      imxcomp->out_align.padding_top + imxcomp->out_align.padding_bottom;
  dst->info.stride = out_frame.info.stride[0];

  GST_LOG ("Output: %s, %dx%d",
      GST_VIDEO_FORMAT_INFO_NAME(out_frame.info.finfo),
      out_frame.info.width, out_frame.info.height);

  if (imxcomp->device->config_output(imxcomp->device, &dst->info) < 0) {
    GST_ERROR ("config output failed");
    gst_video_frame_unmap (&out_frame);
    return -1;
  }

  if (gst_is_dmabuf_memory (gst_buffer_peek_memory (out_frame.buffer, 0))) {
    n_mem = gst_buffer_n_memory (out_frame.buffer);
    for (i = 0; i < n_mem; i++)
      dst->fd[i] = gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (out_frame.buffer, i));
  } else 
    dst->mem = gst_buffer_query_phymem_block (out_frame.buffer);
  dst->alpha = 0xFF; //TODO how to use destination alpha?
  dst->rotate = IMX_2D_ROTATION_0;
  dst->interlace_type = IMX_2D_INTERLACE_PROGRESSIVE;
  dst->crop.x = 0;
  dst->crop.y = 0;
  dst->crop.w = out_frame.info.width;
  dst->crop.h = out_frame.info.height;

  GstVideoCropMeta *out_crop = NULL;
  out_crop = gst_buffer_get_video_crop_meta(out_frame.buffer);
  if (out_crop != NULL) {
    GST_LOG ("output crop meta: (%d, %d, %d, %d)", out_crop->x, out_crop->y,
        out_crop->width, out_crop->height);
    if ((out_crop->x >= out_frame.info.width)
        || (out_crop->y >= out_frame.info.height)) {
      gst_video_frame_unmap (&out_frame);
      return -1;
    }

    dst->crop.x += out_crop->x;
    dst->crop.y += out_crop->y;
    dst->crop.w = MIN(out_crop->width, (out_frame.info.width - out_crop->x));
    dst->crop.h = MIN(out_crop->height,(out_frame.info.height-out_crop->y));
  }

  gst_video_frame_unmap (&out_frame);
  return 0;
}

static gint gst_imxcompositor_config_src(GstImxCompositor *imxcomp,
    GstImxCompositorPad *pad, Imx2DFrame *src)
{
  GstVideoAggregatorPad *ppad = (GstVideoAggregatorPad *)pad;
  guint i, n_mem;
  GstDmabufMeta *dmabuf_meta;
  gint64 drm_modifier = 0;

  src->info.fmt = GST_VIDEO_INFO_FORMAT(&(ppad->aggregated_frame->info));
  src->info.w = ppad->aggregated_frame->info.width +
                pad->align.padding_left + pad->align.padding_right;
  src->info.h = ppad->aggregated_frame->info.height +
                pad->align.padding_top + pad->align.padding_bottom;
  src->info.stride = ppad->aggregated_frame->info.stride[0];

  dmabuf_meta = gst_buffer_get_dmabuf_meta (ppad->aggregated_frame->buffer);
  if (dmabuf_meta)
    drm_modifier = dmabuf_meta->drm_modifier;

  GST_INFO_OBJECT (pad, "buffer modifier type %d", drm_modifier);

  if (drm_modifier == DRM_FORMAT_MOD_AMPHION_TILED)
    src->info.tile_type = IMX_2D_TILE_AMHPION;
  else
    src->info.tile_type = IMX_2D_TILE_NULL;

  GST_LOG_OBJECT (pad, "Input: %s, %dx%d(%d), crop(%d,%d,%d,%d)",
      GST_VIDEO_FORMAT_INFO_NAME(ppad->aggregated_frame->info.finfo),
      src->info.w, src->info.h, src->info.stride,
      pad->src_crop.x, pad->src_crop.y, pad->src_crop.w, pad->src_crop.h);

  if (imxcomp->device->config_input(imxcomp->device, &src->info) < 0) {
    GST_ERROR_OBJECT (pad, "config input failed");
    return -1;
  }

  if (gst_is_dmabuf_memory (gst_buffer_peek_memory (ppad->aggregated_frame->buffer, 0))) {
    n_mem = gst_buffer_n_memory (ppad->aggregated_frame->buffer);
    for (i = 0; i < n_mem; i++)
      src->fd[i] = gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (ppad->aggregated_frame->buffer, i));
  } else 
    src->mem = gst_buffer_query_phymem_block (ppad->aggregated_frame->buffer);
  src->alpha = (gint)(pad->alpha * 255);
  src->rotate = pad->rotate;
  src->interlace_type = IMX_2D_INTERLACE_PROGRESSIVE;
  src->crop.x = pad->src_crop.x;
  src->crop.y = pad->src_crop.y;
  src->crop.w = pad->src_crop.w;
  src->crop.h = pad->src_crop.h;

  return 0;
}

#ifdef USE_GST_VIDEO_SAMPLE_CONVERT
static void
gst_imxcompositor_fill_background(Imx2DFrame *dst, guint RGBA8888)
{
  GstVideoInfo vinfo;
  GstCaps *from_caps, *to_caps;
  GstBuffer *from_buffer, *to_buffer;
  GstSample *from_sample, *to_sample;
  gint i;
  GstMapInfo map;

  from_buffer = gst_buffer_new_and_alloc (dst->info.stride * dst->info.h * 4);

  gst_buffer_map (from_buffer, &map, GST_MAP_WRITE);
  for (i = 0; i < dst->info.stride * dst->info.h; i++) {
    map.data[4 * i + 0] = RGBA8888 & 0x000000FF;
    map.data[4 * i + 1] = (RGBA8888 & 0x0000FF00) >> 8;
    map.data[4 * i + 2] = (RGBA8888 & 0x00FF0000) >> 16;
    map.data[4 * i + 3] = (RGBA8888 & 0xFF000000) >> 24;
  }
  gst_buffer_unmap (from_buffer, &map);

  gst_video_info_init (&vinfo);
  gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_RGBA,
      dst->info.w, dst->info.h);
  from_caps = gst_video_info_to_caps (&vinfo);
  from_sample = gst_sample_new (from_buffer, from_caps, NULL, NULL);

  gst_video_info_set_format (&vinfo, dst->info.fmt, dst->info.w, dst->info.h);
  to_caps = gst_video_info_to_caps (&vinfo);
  to_sample = gst_video_convert_sample (from_sample, to_caps, GST_SECOND, NULL);
  if (to_sample) {
    to_buffer = gst_sample_get_buffer(to_sample);
    gst_buffer_map (to_buffer, &map, GST_MAP_READ);
    memcpy(dst->mem->vaddr, map.data, map.size);
    gst_buffer_unmap(to_buffer, &map);
    gst_sample_unref (to_sample);
  }
  gst_buffer_unref (from_buffer);
  gst_caps_unref (from_caps);
  gst_sample_unref (from_sample);
  gst_caps_unref (to_caps);
}
#else
static void
gst_imxcompositor_fill_background(Imx2DFrame *dst, guint RGBA8888)
{
  gchar *p = dst->mem->vaddr;
  gint i;
  gchar R,G,B,A,Y,U,V;
  gdouble y,u,v;

  R = RGBA8888 & 0x000000FF;
  G = (RGBA8888 & 0x0000FF00) >> 8;
  B = (RGBA8888 & 0x00FF0000) >> 16;
  A = (RGBA8888 & 0xFF000000) >> 24;

  //BT.709
  y = (0.213*R + 0.715*G + 0.072*B);
  u = -0.117*R - 0.394*G + 0.511*B + 128;
  v = 0.511*R - 0.464*G - 0.047*B + 128;

  if (y > 255.0)
    Y = 255;
  else
    Y = (gchar)y;
  if (u < 0.0)
    U = 0;
  else
    U = (gchar)u;
  if (u > 255.0)
    U = 255;
  else
    U = (gchar)u;
  if (v < 0.0)
    V = 0;
  else
    V = (gchar)v;
  if (v > 255.0)
    V = 255;
  else
    V = (gchar)v;

  GST_INFO("RGBA8888 to %s\n", gst_video_format_to_string(dst->info.fmt));

  switch (dst->info.fmt) {
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_RGBA:
      for (i = 0; i < dst->mem->size/4; i++) {
        p[4 * i + 0] = R;
        p[4 * i + 1] = G;
        p[4 * i + 2] = B;
        p[4 * i + 3] = A;
      }
      break;
    case GST_VIDEO_FORMAT_BGR:
      for (i = 0; i < dst->mem->size/3; i++) {
        p[3 * i + 0] = B;
        p[3 * i + 1] = G;
        p[3 * i + 2] = R;
      }
      break;
    case GST_VIDEO_FORMAT_RGB:
      for (i = 0; i < dst->mem->size/3; i++) {
        p[3 * i + 0] = R;
        p[3 * i + 1] = G;
        p[3 * i + 2] = B;
      }
      break;
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_BGRA:
      for (i = 0; i < dst->mem->size/4; i++) {
        p[4 * i + 0] = B;
        p[4 * i + 1] = G;
        p[4 * i + 2] = R;
        p[4 * i + 3] = A;
      }
      break;
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_xBGR:
      for (i = 0; i < dst->mem->size/4; i++) {
        p[4 * i + 0] = A;
        p[4 * i + 1] = B;
        p[4 * i + 2] = G;
        p[4 * i + 3] = R;
      }
      break;
    case GST_VIDEO_FORMAT_RGB16:
      for (i = 0; i < dst->mem->size/2; i++) {
        p[2 * i + 0] = ((G<<3) & 0xE0) | (B>>3);
        p[2 * i + 1] = (R & 0xF8) | (G>>5);
      }
      break;
    case GST_VIDEO_FORMAT_BGR16:
      for (i = 0; i < dst->mem->size/2; i++) {
        p[2 * i + 0] = ((G<<3) & 0xE0) | (R>>3);
        p[2 * i + 1] = (B & 0xF8) | (G>>5);
      }
      break;
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_xRGB:
      for (i = 0; i < dst->mem->size/4; i++) {
        p[4 * i + 0] = A;
        p[4 * i + 1] = R;
        p[4 * i + 2] = G;
        p[4 * i + 3] = B;
      }
      break;
    case GST_VIDEO_FORMAT_Y444:
      memset(p, Y, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h, U, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h*2, V, dst->info.w*dst->info.h);
      break;
    case GST_VIDEO_FORMAT_I420:
      memset(p, Y, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h, U, dst->info.w*dst->info.h/4);
      memset(p+dst->info.w*dst->info.h*5/4, V, dst->info.w*dst->info.h/4);
      break;
    case GST_VIDEO_FORMAT_YV12:
      memset(p, Y, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h, V, dst->info.w*dst->info.h/4);
      memset(p+dst->info.w*dst->info.h*5/4, U, dst->info.w*dst->info.h/4);
      break;
    case GST_VIDEO_FORMAT_NV12:
      memset(p, Y, dst->info.w*dst->info.h);
      p += dst->info.w*dst->info.h;
      for (i = 0; i < dst->info.w*dst->info.h/4; i++) {
        *p++ = U;
        *p++ = V;
      }
      break;
    case GST_VIDEO_FORMAT_NV21:
      memset(p, Y, dst->info.w*dst->info.h);
      p += dst->info.w*dst->info.h;
      for (i = 0; i < dst->info.w*dst->info.h/4; i++) {
        *p++ = V;
        *p++ = U;
      }
      break;
    case GST_VIDEO_FORMAT_UYVY:
      for (i = 0; i < dst->info.w*dst->info.h/2; i++) {
        *p++ = U;
        *p++ = Y;
        *p++ = V;
        *p++ = Y;
      }
      break;
    case GST_VIDEO_FORMAT_Y42B:
      memset(p, Y, dst->info.w*dst->info.h);
      memset(p+dst->info.w*dst->info.h, U, dst->info.w*dst->info.h/2);
      memset(p+dst->info.w*dst->info.h*3/2, V, dst->info.w*dst->info.h/2);
      break;
    case GST_VIDEO_FORMAT_v308:
      for (i = 0; i < dst->info.w*dst->info.h; i++) {
        *p++ = Y;
        *p++ = U;
        *p++ = V;
      }
      break;
    case GST_VIDEO_FORMAT_GRAY8:
      memset(p, Y, dst->info.w*dst->info.h);
      break;
    case GST_VIDEO_FORMAT_NV16:
      memset(p, Y, dst->info.w*dst->info.h);
      p += dst->info.w*dst->info.h;
      for (i = 0; i < dst->info.w*dst->info.h/2; i++) {
        *p++ = U;
        *p++ = V;
      }
      break;
    default:
      GST_FIXME("Add support for %d", dst->info.fmt);
      memset(dst->mem->vaddr, 0, dst->mem->size);
      break;
  }
}
#endif

static gint imxcompositor_pad_zorder_compare (gconstpointer a, gconstpointer b)
{
  if (a && b) {
    GstVideoAggregatorPad *pad_a = GST_VIDEO_AGGREGATOR_PAD (a);
    GstVideoAggregatorPad *pad_b = GST_VIDEO_AGGREGATOR_PAD (b);
    return (pad_a->zorder - pad_b->zorder);
  } else {
    return 0;
  }
}

static GstFlowReturn
gst_imxcompositor_aggregate_frames (GstVideoAggregator * vagg,
                                    GstBuffer * outbuf)
{
  GList *l;
  GstImxCompositor *imxcomp = (GstImxCompositor *) (vagg);
  Imx2DDevice *device = imxcomp->device;
  GstFlowReturn ret;
  Imx2DFrame src = {0}, dst = {0};
  PhyMemBlock src_mem = {0}, dst_mem = {0};
  guint aggregated = 0;

  if (!device)
    return GST_FLOW_ERROR;

  dst.mem = &dst_mem;
  if (gst_imxcompositor_config_dst(imxcomp, outbuf, &dst) < 0)
    return GST_FLOW_ERROR;

  GST_OBJECT_LOCK (vagg);

  /* TODO: If the frames to be composited completely obscure the background,
   * don't bother drawing the background at all. */
  if (imxcomp->background_enable) {
    if (device->fill) {
      if(device->fill (device, &dst, imxcomp->background) < 0) {
        GST_LOG("fill color background by device failed");
        gst_imxcompositor_fill_background(&dst, imxcomp->background);
      }
    } else {
      GST_LOG("device has no fill interface");
      gst_imxcompositor_fill_background(&dst, imxcomp->background);
    }
  } else {
    //gst_imxcompositor_fill_background(&dst, DEFAULT_IMXCOMPOSITOR_BACKGROUND);
  }

  //re-order by zorder of pad
  GList *pads = g_list_copy(GST_ELEMENT (vagg)->sinkpads);
  pads = g_list_sort(pads, imxcompositor_pad_zorder_compare);

  for (l = pads; l; l = l->next) {
    GstVideoAggregatorPad *ppad = l->data;
    GstImxCompositorPad *pad = GST_IMXCOMPOSITOR_PAD (ppad);

    if (ppad->aggregated_frame != NULL) {
      src.mem = &src_mem;
      memset (src.mem, 0, sizeof(PhyMemBlock));
      if (gst_imxcompositor_config_src(imxcomp, pad, &src) < 0) {
        continue;
      }

      if (device->set_rotate(device, src.rotate) < 0) {
        GST_WARNING_OBJECT (pad, "set rotate failed");
        continue;
      }
      if (device->set_deinterlace(device, IMX_2D_DEINTERLACE_NONE) < 0) {
        GST_WARNING_OBJECT (pad, "set deinterlace mode failed");
        continue;
      }

      //update destination location and size
      dst.crop.x = pad->dst_crop.x;
      dst.crop.y = pad->dst_crop.y;
      dst.crop.w = pad->dst_crop.w;
      dst.crop.h = pad->dst_crop.h;

      if (device->blend(device, &dst, &src) < 0) {
        GST_WARNING_OBJECT (pad, "frame blend fail");
        continue;
      }

      aggregated++;

      if (imxcomp->composition_meta_enable &&
        imx_video_overlay_composition_has_meta(ppad->buffer)) {
        VideoCompositionVideoInfo in_v, out_v;
        memset (&in_v, 0, sizeof(VideoCompositionVideoInfo));
        memset (&out_v, 0, sizeof(VideoCompositionVideoInfo));
        in_v.buf = ppad->buffer;
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

        memcpy(&out_v.align, &(imxcomp->out_align), sizeof(GstVideoAlignment));

        gint cnt = imx_video_overlay_composition_composite(&imxcomp->video_comp,
                                                          &in_v, &out_v, FALSE);

        if (cnt >= 0)
          GST_DEBUG ("processed %d video overlay composition buffers", cnt);
        else
          GST_WARNING ("video overlay composition meta handling failed");
      }
    }
  }

  g_list_free(pads);

  if (imxcomp->background_enable &&
      device->device_type == IMX_2D_DEVICE_PXP && aggregated == 0) {
    /* PXP can't fill background without blending something */
    /* fill the background color by software */
    gst_imxcompositor_fill_background(&dst, imxcomp->background);
  }

  GST_LOG("Aggregated %d frames", aggregated);
  if (aggregated > 0 && device->blend_finish(device) < 0) {
    GST_ERROR ("frame blend finish fail");
  }

  GST_OBJECT_UNLOCK (vagg);

  return GST_FLOW_OK;
}

static GstCaps* imx_compositor_caps_from_fmt_list(GList* list)
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
  return caps;
}

static void
gst_imxcompositor_class_init (GstImxCompositorClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoAggregatorClass *videoaggregator_class =
      (GstVideoAggregatorClass *) klass;
  GstAggregatorClass *agg_class = (GstAggregatorClass *) klass;
  GstCaps *caps;

  parent_class = g_type_class_peek_parent (klass);
  gobject_class->finalize = gst_imxcompositor_finalize;
  gobject_class->get_property = gst_imxcompositor_get_property;
  gobject_class->set_property = gst_imxcompositor_set_property;

  Imx2DDeviceInfo *in_plugin = (Imx2DDeviceInfo *)g_type_get_qdata (
                G_OBJECT_CLASS_TYPE (klass), GST_IMX_COMPOSITOR_PARAMS_QDATA);
  g_assert (in_plugin != NULL);

  Imx2DDevice* dev = in_plugin->create(in_plugin->device_type);
  if (!dev)
    return;

  gchar longname[64] = {0};
  snprintf(longname, 32, "IMX %s Video Compositor", in_plugin->name);
  gst_element_class_set_static_metadata (gstelement_class, longname,
      "Filter/Editor/Video/ImxCompositor", "Composite multiple video streams",
      IMX_GST_PLUGIN_AUTHOR);

  GList *list = dev->get_supported_in_fmts(dev);
  caps = imx_compositor_caps_from_fmt_list(list);
  g_list_free(list);

  if (!caps) {
    GST_ERROR ("Couldn't create caps for device '%s'", in_plugin->name);
    caps = gst_caps_new_empty_simple ("video/x-raw");
  }
#if GST_CHECK_VERSION(1, 14, 0)
   gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new_with_gtype ("sink_%u", GST_PAD_SINK, GST_PAD_REQUEST, caps, GST_TYPE_IMXCOMPOSITOR_PAD));
#else
  gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new ("sink_%u", GST_PAD_SINK, GST_PAD_REQUEST, caps));
#endif

  list = dev->get_supported_out_fmts(dev);
  caps = imx_compositor_caps_from_fmt_list(list);
  g_list_free(list);

  if (!caps) {
    GST_ERROR ("Couldn't create caps for device '%s'", in_plugin->name);
    caps = gst_caps_new_empty_simple ("video/x-raw");
  }

  klass->in_plugin = in_plugin;
  in_plugin->destroy(dev);

#if GST_CHECK_VERSION(1, 14, 0)
  gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new_with_gtype ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps, GST_TYPE_AGGREGATOR_PAD));
  agg_class->negotiated_src_caps = gst_imxcompositor_negotiated_caps;
#else
  gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  agg_class->sinkpads_type = GST_TYPE_IMXCOMPOSITOR_PAD;
  videoaggregator_class->negotiated_caps = gst_imxcompositor_negotiated_caps;
#endif
  agg_class->sink_query = gst_imxcompositor_sink_query;
  agg_class->src_query = gst_imxcompositor_src_query;

  videoaggregator_class->find_best_format = gst_imxcompositor_find_best_format;
  videoaggregator_class->update_caps = gst_imxcompositor_update_caps;
  videoaggregator_class->aggregate_frames = gst_imxcompositor_aggregate_frames;
  videoaggregator_class->get_output_buffer=gst_imxcompositor_get_output_buffer;

  g_object_class_install_property (gobject_class,
      PROP_IMXCOMPOSITOR_BACKGROUND_ENABLE,
      g_param_spec_boolean ("back-enable", "Background Enable",
          "Enable Fill Background color", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_IMXCOMPOSITOR_BACKGROUND_COLOR,
      g_param_spec_uint ("background", "Background",
          "Background color (ARGB format)",
          0, 0xFFFFFFFF, DEFAULT_IMXCOMPOSITOR_BACKGROUND,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_IMXCOMPOSITOR_COMPOSITION_META_ENABLE,
      g_param_spec_boolean("composition-meta-enable", "Enable composition meta",
        "Enable overlay composition meta processing",
        IMX_COMPOSITOR_COMPOMETA_DEFAULT,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#if 0
  g_object_class_install_property (gobject_class,
      PROP_IMXCOMPOSITOR_OUTPUT_WIDTH,
      g_param_spec_uint ("output-width", "Output width",
          "Output frame width", 64, 4096, DEFAULT_IMXCOMPOSITOR_BACKGROUND,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
      PROP_IMXCOMPOSITOR_OUTPUT_HEIGHT,
      g_param_spec_uint ("output-height", "Output Height",
          "Output frame height", 64, 4096, DEFAULT_IMXCOMPOSITOR_BACKGROUND,
          G_PARAM_READWRITE));
#endif
}

static void
gst_imxcompositor_init (GstImxCompositor * imxcomp)
{
  imxcomp->background = DEFAULT_IMXCOMPOSITOR_BACKGROUND;
  imxcomp->background_enable = TRUE;
  GstImxCompositorClass *klass =
      (GstImxCompositorClass *) G_OBJECT_GET_CLASS (imxcomp);

  if (klass->in_plugin)
    imxcomp->device = klass->in_plugin->create(klass->in_plugin->device_type);

  if (imxcomp->device) {
    if (imxcomp->device->open(imxcomp->device) < 0) {
      GST_ERROR ("Open 2D device failed.");
    } else {
      imxcomp->out_pool = NULL;
      imxcomp->self_out_pool = NULL;
      imxcomp->out_pool_update = TRUE;
      imxcomp->allocator = NULL;
      imxcomp->capabilities =imxcomp->device->get_capabilities(imxcomp->device);
      memset (&imxcomp->out_align, 0, sizeof(GstVideoAlignment));
      imxcomp->composition_meta_enable = IMX_COMPOSITOR_COMPOMETA_DEFAULT;
      imx_video_overlay_composition_init(&imxcomp->video_comp, imxcomp->device);
    }
  } else {
    GST_ERROR ("Create 2D device failed.");
  }
}

static gboolean gst_imx_compositor_register (GstPlugin * plugin)
{
  GTypeInfo tinfo = {
    sizeof (GstImxCompositorClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_imxcompositor_class_init,
    NULL,
    NULL,
    sizeof (GstImxCompositor),
    0,
    (GInstanceInitFunc) gst_imxcompositor_init,
  };

  GType type;
  gchar *t_name;

  const Imx2DDeviceInfo *in_plugin = imx_get_2d_devices();

  while (in_plugin->name) {
    GST_LOG ("Registering %s compositor", in_plugin->name);

    if (!in_plugin->is_exist()) {
      GST_WARNING("device %s not exist", in_plugin->name);
      in_plugin++;
      continue;
    }

    t_name = g_strdup_printf ("imxcompositor_%s", in_plugin->name);
    type = g_type_from_name (t_name);

    if (!type) {
      type = g_type_register_static (GST_TYPE_VIDEO_AGGREGATOR,
                                      t_name, &tinfo, 0);
      g_type_set_qdata (type, GST_IMX_COMPOSITOR_PARAMS_QDATA,
                        (gpointer) in_plugin);
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

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_imxcompositor_debug, "imxcompositor", 0,
      "Freescale IMX Video Compositor");

  return gst_imx_compositor_register (plugin);
}

IMX_GST_PLUGIN_DEFINE(imxcompositor, "IMX Video Composition Plugins",
                      plugin_init);
