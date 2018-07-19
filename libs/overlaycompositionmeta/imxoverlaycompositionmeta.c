/* Process video overlay composition meta by IMX 2D devices
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

/* This libs enable processing video overlay composition for video overlay
 * composition meta from the input buffer, this feature will composite all the
 * buffers from composition meta and the input buffer into one output buffer
 * the alpha supporting is depends on the underlying blending hardware,
 * if the underlying hardware don't support alpha blending, then the alpha
 * channel in buffers will be ignored.
 */

#include <gst/video/video-overlay-composition.h>
#include <gst/allocators/gstdmabuf.h>
#include "imxoverlaycompositionmeta.h"
#include <gst/allocators/gstphymemmeta.h>
#ifdef USE_ION
#include <gst/allocators/gstionmemory.h>
#endif

GST_DEBUG_CATEGORY_STATIC(overlay_composition_meta);
#define GST_CAT_DEFAULT overlay_composition_meta

static gboolean
is_input_fmt_support(Imx2DDevice *device, GstVideoFormat fmt)
{
  GList *list = device->get_supported_in_fmts(device);
  GList *l;
  for (l=list; l; l=l->next) {
    if (fmt == (GstVideoFormat)l->data) {
      g_list_free(list);
      return TRUE;
    }
  }
  g_list_free(list);
  return FALSE;
}

static GstVideoFormat
find_best_input_rgb_format(Imx2DDevice *device)
{
  GList *list = device->get_supported_in_fmts(device);
  GList *l;
  GstVideoFormat fmt = GST_VIDEO_FORMAT_UNKNOWN;

  for (l=list; l; l=l->next) {
    GstVideoFormat fmt = (GstVideoFormat)l->data;
      if (GST_VIDEO_FORMAT_xRGB == fmt) {
        break;
      } else if (GST_VIDEO_FORMAT_RGBA == fmt) {
        break;
      } else if (GST_VIDEO_FORMAT_RGBx == fmt) {
        break;
      } else if (GST_VIDEO_FORMAT_ABGR == fmt) {
        break;
      } else if (GST_VIDEO_FORMAT_BGRA == fmt) {
        break;
      } else if (GST_VIDEO_FORMAT_xBGR == fmt) {
        break;
      } else if (GST_VIDEO_FORMAT_BGRx == fmt) {
        break;
      } else if (GST_VIDEO_FORMAT_RGB16 == fmt) {
        break;
      } else {
        fmt = GST_VIDEO_FORMAT_UNKNOWN;
      }
  }

  g_list_free(list);
  return fmt;
}

static gint overlay_composition_buffer_convert(
                              gchar *in_vaddr, gchar *out_vaddr,
                              GstVideoFormat in_fmt, GstVideoFormat out_fmt,
                              guint width, guint height, guint pitch)
{
  gint line=0;
  gint y=0;
  gchar *in_p, *out_p;

  if (in_fmt == GST_VIDEO_FORMAT_ARGB || in_fmt == GST_VIDEO_FORMAT_xRGB) {
    switch (out_fmt) {
    case GST_VIDEO_FORMAT_xRGB:
      for (line=0; line<height; line++)
        memcpy((out_vaddr + pitch*line*4), (in_vaddr + pitch*line*4), width*4);
      break;
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
      for (line=0; line<height; line++) {
        out_p = out_vaddr + pitch*line*4;
        in_p = in_vaddr + pitch*line*4;
        for (y=0; y<width; y+=4) {
          *(out_p + y) = *(in_p + y + 3);
          *(out_p + y + 1) = *(in_p + y);
          *(out_p + y + 2) = *(in_p + y + 1);
          *(out_p + y + 3) = *(in_p + y + 2);
        }
      }
      break;
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_xBGR:
      for (line=0; line<height; line++) {
        out_p = out_vaddr + pitch*line*4;
        in_p = in_vaddr + pitch*line*4;
        for (y=0; y<width; y+=4) {
          *(out_p + y) = *(in_p + y + 2);
          *(out_p + y + 1) = *(in_p + y + 1);
          *(out_p + y + 2) = *(in_p + y);
          *(out_p + y + 3) = *(in_p + y + 3);
        }
      }
      break;
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      for (line=0; line<height; line++) {
        out_p = out_vaddr + pitch*line*4;
        in_p = in_vaddr + pitch*line*4;
        for (y=0; y<width; y+=4) {
          *(out_p + y) = *(in_p + y + 3);
          *(out_p + y + 1) = *(in_p + y + 2);
          *(out_p + y + 2) = *(in_p + y + 1);
          *(out_p + y + 3) = *(in_p + y);
        }
      }
      break;
    case GST_VIDEO_FORMAT_RGB16:
      for (line=0; line<height; line++) {
        out_p = out_vaddr + pitch*line*2;
        in_p = in_vaddr + pitch*line*4;
        for (y=0; y<width; y+=2) {
          gchar B = *(in_p + y*2);
          gchar G = *(in_p + y*2 + 1);
          gchar R = *(in_p + y*2 + 2);

          *(out_p + y) = ((G<<3) & 0xE0) | (R>>3);
          *(out_p + y + 1) = (B & 0xF8) | (G>>5);
        }
      }
      break;
    default:
      GST_WARNING("convert overlay to format %s not support",
                        gst_video_format_to_string(out_fmt));
      return -1;
    }
  } else {
    GST_WARNING("convert overlay format from %s not support",
                  gst_video_format_to_string(in_fmt));
    return -1;
  }

  return 0;
}

gboolean imx_video_overlay_composition_is_out_fmt_support(Imx2DDevice *device,
                                                          GstVideoFormat fmt)
{
  GList *list = device->get_supported_out_fmts(device);
  GList *l;
  for (l=list; l; l=l->next) {
    if (fmt == (GstVideoFormat)l->data) {
      g_list_free(list);
      return TRUE;
    }
  }
  g_list_free(list);
  return FALSE;
}

void imx_video_overlay_composition_init(GstImxVideoOverlayComposition *vcomp,
                                        Imx2DDevice *device)
{
  static gint debug_init = 0;
  if (debug_init == 0) {
    GST_DEBUG_CATEGORY_INIT (overlay_composition_meta, "overlaycompometa", 0,
                           "Freescale video overlay composition meta");
    debug_init = 1;
  }

  vcomp->device = device;
  vcomp->allocator = NULL;
  vcomp->tmp_buf = NULL;
  vcomp->tmp_buf_size = 0;
}

void imx_video_overlay_composition_deinit(GstImxVideoOverlayComposition *vcomp)
{
  if (vcomp) {
    if (vcomp->tmp_buf)
      gst_buffer_unref(vcomp->tmp_buf);
    if (vcomp->allocator)
      gst_object_unref (vcomp->allocator);

    vcomp->tmp_buf = NULL;
    vcomp->allocator = NULL;
    vcomp->device = NULL;
  }
}

gboolean imx_video_overlay_composition_has_meta(GstBuffer *in)
{
  gpointer state = NULL;
  GstMeta *meta;
  GstVideoOverlayCompositionMeta *compmeta;

  while ((meta = gst_buffer_iterate_meta (in, &state))) {
    if (meta->info->api == GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE) {
      compmeta = (GstVideoOverlayCompositionMeta*)meta;
      if (GST_IS_VIDEO_OVERLAY_COMPOSITION (compmeta->overlay)) {
        return TRUE;
      }
    }
  }

  return FALSE;
}

void imx_video_overlay_composition_add_caps(GstCaps *caps)
{
  gint i;
  GstCapsFeatures *f, *has_f;
  GstStructure *s;
  GstCaps *new_caps;

  if (caps && !gst_caps_is_empty(caps)) {
    guint num = gst_caps_get_size(caps);
    for (i=0; i<num; i++) {
      has_f = gst_caps_get_features(caps, i);
      if (!has_f || !gst_caps_features_contains(has_f,
          GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY)
          || !gst_caps_features_contains(has_f,
              GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION))
      {
        s = gst_structure_copy(gst_caps_get_structure(caps, i));
        f = gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY,
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, NULL);
        new_caps = gst_caps_new_empty();
        gst_caps_append_structure(new_caps, s);
        gst_caps_set_features(new_caps, 0, f);

        if (!gst_caps_is_subset(new_caps, caps)) {
          gst_caps_append (caps, new_caps);
        } else {
          gst_caps_unref(new_caps);
        }
      }
    }
  }
}

void imx_video_overlay_composition_remove_caps(GstCaps *caps)
{
  gint i;
  GstCapsFeatures *has_f = NULL;

  if (caps && !gst_caps_is_empty(caps)) {
    guint num = gst_caps_get_size(caps);
    for (i=num-1; i>=0; i--) {
      has_f = gst_caps_get_features(caps, i);
      if (has_f && gst_caps_features_contains(has_f,
          GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY)
          && gst_caps_features_contains(has_f,
              GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION))
      {
        gst_caps_remove_structure(caps, i);
      }
    }
  }
}

void imx_video_overlay_composition_add_query_meta(GstQuery *query)
{
  gst_query_add_allocation_meta (query,
                           GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL);
}

void imx_video_overlay_composition_copy_meta(GstBuffer *dst, GstBuffer *src,
                                             guint in_width, guint in_height,
                                             guint out_width, guint out_height)
{
  gpointer state = NULL;
  GstMeta *meta;
  GstVideoOverlayCompositionMeta *compmeta;
  GstVideoOverlayComposition *comp_copy;

  while ((meta = gst_buffer_iterate_meta (src, &state))) {
    if (meta->info->api == GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE) {
      compmeta = (GstVideoOverlayCompositionMeta*)meta;
      if (GST_IS_VIDEO_OVERLAY_COMPOSITION (compmeta->overlay)) {
        comp_copy = gst_video_overlay_composition_copy(compmeta->overlay);
        if (comp_copy) {
          if ((in_width != out_width || in_height != out_height) &&
              (in_width && out_width && in_height && out_height)) {
            guint num = gst_video_overlay_composition_n_rectangles (comp_copy);
            guint n;
            GstVideoOverlayRectangle *rect;
            gint render_x, render_y;
            guint render_w, render_h;

            for (n = 0; n < num; n++) {
              rect = gst_video_overlay_composition_get_rectangle(comp_copy, n);
              gst_video_overlay_rectangle_get_render_rectangle (rect,
                  &render_x, &render_y, &render_w, &render_h);

              render_x = render_x * out_width / in_width;
              render_w = render_w * out_width / in_width;
              render_y = render_y * out_height / in_height;
              render_h = render_h * out_height / in_height;
              gst_video_overlay_rectangle_set_render_rectangle(rect,
                  render_x, render_y, render_w, render_h);
            }
          }

          gst_buffer_add_video_overlay_composition_meta(dst, comp_copy);
          gst_video_overlay_composition_unref (comp_copy);
        }
      }
    }
  }
}

gint imx_video_overlay_composition_remove_meta(GstBuffer *buffer)
{
  gint ret = 0;
  GstVideoOverlayCompositionMeta *compmeta;

  if (gst_buffer_is_writable(buffer)) {
    while(compmeta = gst_buffer_get_video_overlay_composition_meta(buffer))
      gst_buffer_remove_video_overlay_composition_meta(buffer, compmeta);
  } else {
    GST_WARNING("remove video composition meta failed: buffer not writable\n");
    ret = -1;
  }

  return ret;
}

gint imx_video_overlay_composition_composite(
                                      GstImxVideoOverlayComposition *vcomp,
                                      VideoCompositionVideoInfo *in_v,
                                      VideoCompositionVideoInfo *out_v,
                                      gboolean config_out)
{
  GstVideoOverlayCompositionMeta *compmeta;
  gint blend_cnt = -1;
  gpointer state = NULL;
  GstMeta *meta;

  if (!vcomp || !in_v || !in_v->buf || !out_v || !out_v->mem)
    return -1;

  while ((meta = gst_buffer_iterate_meta (in_v->buf, &state))) {
    if (meta->info->api == GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE) {
      compmeta = (GstVideoOverlayCompositionMeta*)meta;
    } else {
      continue;
    }

    if (!GST_IS_VIDEO_OVERLAY_COMPOSITION (compmeta->overlay)) {
      continue;
    }

    guint n;
    GstVideoOverlayComposition *comp = compmeta->overlay;
    guint num = gst_video_overlay_composition_n_rectangles (comp);
    blend_cnt = 0;

    GST_INFO ("Blending composition %p with %u rectangles onto buffer %p",
                comp, num, in_v->buf);

    for (n = 0; n < num; n++) {
      GstVideoOverlayRectangle *rect;
      gint render_x, render_y;
      guint render_w, render_h;
      guint aligned_w, aligned_h;
      Imx2DFrame src = {0}, dst = {0};
      PhyMemBlock src_mem = {0}, dst_mem = {0};
      guint i, n_mem;
      GstVideoCropMeta *in_crop = NULL;
      GstBuffer *in_buf;

      rect = gst_video_overlay_composition_get_rectangle(comp, n);
      GstBuffer *ovbuf =
          gst_video_overlay_rectangle_get_pixels_unscaled_raw (rect,
              GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);

      if (!ovbuf)
        continue;

      GstVideoMeta *vmeta = gst_buffer_get_video_meta(ovbuf);
      if (!vmeta)
        continue;

      GST_INFO (" Get overlay buffer [%d] with format (%s).",
                  n, gst_video_format_to_string(vmeta->format));

      //check if overlay format are supported by device
      GstVideoFormat t_fmt = vmeta->format;
      if (!is_input_fmt_support(vcomp->device, t_fmt)) {
        ovbuf = gst_video_overlay_rectangle_get_pixels_unscaled_argb(rect,
                                          GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
        if (!ovbuf) {
          GST_WARNING ("convert overlay buffer [%d] format (%s) to ARGB failed",
              n, gst_video_format_to_string(vmeta->format));
          continue;
        }
        vmeta = gst_buffer_get_video_meta(ovbuf);
        t_fmt = vmeta->format;
        if (!is_input_fmt_support(vcomp->device, t_fmt)) {
          //the device don't support ARGB format, we need do a little trick.
          //convert ARGB to the proper RGB color space format the device support
          t_fmt = find_best_input_rgb_format(vcomp->device);
          if (t_fmt == GST_VIDEO_FORMAT_UNKNOWN) {
            GST_WARNING("overlay buffer [%d] format %s not support",
                n, gst_video_format_to_string(vmeta->format));
            continue;
          }

          if (vmeta->format == GST_VIDEO_FORMAT_ARGB
              && t_fmt == GST_VIDEO_FORMAT_xRGB) {
            t_fmt = vmeta->format; //treat ARGB as xRGB
          }
        }
      }

      aligned_w = ALIGNTO(vmeta->width, ALIGNMENT);
      aligned_h = ALIGNTO(vmeta->height, ALIGNMENT);

      GST_INFO ("t_fmt=%s, orgi=%s, phy=%d, w=%d, h=%d",
          gst_video_format_to_string(t_fmt),
          gst_video_format_to_string(vmeta->format),
          gst_buffer_is_phymem(ovbuf), vmeta->width, vmeta->height);

      if (t_fmt != vmeta->format || !gst_buffer_is_phymem(ovbuf)) {
        // need copy buffer
        if (!vcomp->allocator) {
#ifdef USE_ION
          vcomp->allocator = gst_ion_allocator_obtain ();
#endif
        }

        /* obtain ion allocator will fail on imx6 and 7D */
        if (!vcomp->allocator)
          vcomp->allocator =
                 gst_imx_2d_device_allocator_new((gpointer)(vcomp->device));

        if (!vcomp->allocator) {
          GST_WARNING("create allocator for overlay buffer failed\n");
          continue;
        }

        if (!vcomp->tmp_buf) {
          vcomp->tmp_buf = gst_buffer_new_allocate(vcomp->allocator,
              IMX_OVERLAY_COMPOSITION_INIT_BUFFER_SIZE, NULL);
          if (vcomp->tmp_buf)
            vcomp->tmp_buf_size = IMX_OVERLAY_COMPOSITION_INIT_BUFFER_SIZE;
          else {
            vcomp->tmp_buf_size = 0;
            GST_WARNING("allocate buffer by allocator failed, size=%d\n",
                IMX_OVERLAY_COMPOSITION_INIT_BUFFER_SIZE);
          }
        }

        if (vcomp->tmp_buf && vcomp->tmp_buf_size < aligned_w * aligned_h * 4) {
          GstBuffer *tmp_buf = gst_buffer_new_allocate(vcomp->allocator,
              (aligned_w * aligned_h * 4), NULL);
          if (tmp_buf) {
            vcomp->tmp_buf_size = (aligned_w * aligned_h * 4);
            gst_buffer_unref(vcomp->tmp_buf);
            vcomp->tmp_buf = tmp_buf;
          }
        }

        if (!vcomp->tmp_buf) {
          GST_WARNING ("!!!allocate buffer for overlay [%d] failed, "
           "this video overlay buffer size is out of hardware capability\n", n);
          continue;
        }

        if (t_fmt == vmeta->format) {
          GstVideoFrame temp_in_frame, frame;
          GstVideoInfo vinfo;
          GstVideoAlignment align = {0};
          gst_video_info_set_format(&vinfo, vmeta->format,
                                      vmeta->width, vmeta->height);

          if (!gst_video_frame_map (&frame, &vinfo, ovbuf, GST_MAP_READ)) {
            GST_WARNING ("can not map overlay buffer [%d]", n);
            continue;
          }

          align.padding_right = aligned_w - vmeta->width;
          align.padding_bottom = aligned_h - vmeta->height;
          gst_video_info_align(&vinfo, &align);

          gst_video_frame_map(&temp_in_frame, &vinfo,
              vcomp->tmp_buf, GST_MAP_WRITE);
          gst_video_frame_copy(&temp_in_frame, &frame);
          gst_video_frame_unmap(&temp_in_frame);
          gst_video_frame_unmap (&frame);
        } else {
          //convert ARGB format to target format
          GstMapInfo minfo_in, minfo_out;
          gst_buffer_map(ovbuf, &minfo_in, GST_MAP_READ);
          gst_buffer_map(vcomp->tmp_buf, &minfo_out, GST_MAP_WRITE);
          gint ret = overlay_composition_buffer_convert(
                        minfo_in.data, minfo_out.data, vmeta->format, t_fmt,
                        vmeta->width, vmeta->height, aligned_w);
          gst_buffer_unmap(ovbuf, &minfo_in);
          gst_buffer_unmap(vcomp->tmp_buf, &minfo_out);

          if (ret < 0) {
            GST_WARNING("video overlay buffer %d convert from %s to %s failed",
                n, gst_video_format_to_string(vmeta->format),
                gst_video_format_to_string(t_fmt));
            continue;
          }
        }

        in_buf = vcomp->tmp_buf;
        src.info.w = aligned_w;
        src.info.h = aligned_h;
      } else {
        GstPhyMemMeta *phymemmeta = NULL;
        phymemmeta = GST_PHY_MEM_META_GET (ovbuf);
        if (phymemmeta) {
          src.info.w = vmeta->width + phymemmeta->x_padding;
          src.info.h = vmeta->height + phymemmeta->y_padding;
        }
        in_buf = ovbuf;
      }

      gst_video_overlay_rectangle_get_render_rectangle (rect,
                                  &render_x, &render_y, &render_w, &render_h);

      GST_INFO ("rectangle %u %p: %ux%u, render %u,%u,%ux,%u, format %s -> %s,",
          n, rect, vmeta->width, vmeta->height, render_x, render_y,
          render_w, render_h,
          gst_video_format_to_string(vmeta->format),
          gst_video_format_to_string(t_fmt));

      // start blending
      src.info.fmt = t_fmt;
      src.info.stride = src.info.w;

      if (vcomp->device->config_input(vcomp->device, &src.info) < 0) {
        GST_WARNING("config input for overlay buffer [%d] failed", n);
        continue;
      }

      GST_LOG ("overlay input: %s, %dx%d", gst_video_format_to_string(t_fmt),
          src.info.w, src.info.h);

      if (gst_is_dmabuf_memory (gst_buffer_peek_memory (in_buf, 0))) {
        src.mem = &src_mem;
        n_mem = gst_buffer_n_memory (in_buf);
        for (i = 0; i < n_mem; i++)
          src.fd[i] = gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (in_buf, i));
      } else
        src.mem = gst_buffer_query_phymem_block (in_buf);
      src.alpha = 0xFF;
      src.crop.x = 0;
      src.crop.y = 0;
      src.crop.w = vmeta->width;
      src.crop.h = vmeta->height;
      src.rotate = in_v->rotate;
      src.interlace_type = IMX_2D_INTERLACE_PROGRESSIVE;

      in_crop = gst_buffer_get_video_crop_meta(ovbuf);
      if (in_crop != NULL) {
        if ((in_crop->x < in_v->width) && (in_crop->y < in_v->height)) {
          src.crop.x += in_crop->x;
          src.crop.y += in_crop->y;
          src.crop.w = MIN(in_crop->width, (vmeta->width - in_crop->x));
          src.crop.h = MIN(in_crop->height, (vmeta->height - in_crop->y));
        }
      }

      if (vcomp->device->set_rotate(vcomp->device, src.rotate) < 0) {
        GST_WARNING ("set rotate for overlay buffer [%d] failed", n);
        continue;
      }
      if (vcomp->device->set_deinterlace(vcomp->device,
                                          IMX_2D_DEINTERLACE_NONE) < 0) {
        GST_WARNING ("set deinterlace mode for overlay buffer [%d] failed", n);
        continue;
      }

      dst.mem = out_v->mem;
      dst.fd[0] = out_v->fd[0];
      dst.fd[1] = out_v->fd[1];
      dst.fd[2] = out_v->fd[2];
      dst.fd[3] = out_v->fd[3];
      dst.info.fmt = out_v->fmt;
      dst.alpha = 0xFF;
      dst.rotate = out_v->rotate;
      dst.interlace_type = IMX_2D_INTERLACE_PROGRESSIVE;

      if (render_x < 0)
        render_x = 0;
      if (render_y < 0)
        render_y = 0;
      if (render_x + render_w > in_v->crop_w)
        render_w = in_v->crop_w - render_x;
      if (render_y + render_h > in_v->crop_h)
        render_h = in_v->crop_h - render_y;

      switch(src.rotate) {
      case IMX_2D_ROTATION_90:
        dst.crop.x = out_v->crop_x +
                    (in_v->height-render_y-render_h)*out_v->crop_w/in_v->crop_h;
        dst.crop.y = out_v->crop_y + render_x*out_v->crop_h/in_v->crop_w;
        dst.crop.w = render_h * out_v->crop_w / in_v->crop_h;
        dst.crop.h = render_w * out_v->crop_h / in_v->crop_w;
        break;
      case IMX_2D_ROTATION_180:
        dst.crop.x = out_v->crop_x + render_x * out_v->crop_w / in_v->crop_w;
        dst.crop.y = out_v->crop_y +
                    (in_v->height-render_y-render_h)*out_v->crop_h/in_v->crop_h;
        dst.crop.w = render_w * out_v->crop_w / in_v->crop_w;
        dst.crop.h = render_h * out_v->crop_h / in_v->crop_h;
        break;
      case IMX_2D_ROTATION_270:
        dst.crop.x = out_v->crop_x + render_y*out_v->crop_w/in_v->crop_h;
        dst.crop.y = out_v->crop_y + render_x*out_v->crop_h/in_v->crop_w;
        dst.crop.w = render_h * out_v->crop_w / in_v->crop_h;
        dst.crop.h = render_w * out_v->crop_h / in_v->crop_w;
        break;
      default:
        dst.crop.x = out_v->crop_x + render_x * out_v->crop_w / in_v->crop_w;
        dst.crop.y = out_v->crop_y + render_y * out_v->crop_h / in_v->crop_h;
        dst.crop.w = render_w * out_v->crop_w / in_v->crop_w;
        dst.crop.h = render_h * out_v->crop_h / in_v->crop_h;
        break;
      }

      dst.info.w = out_v->width +
          out_v->align.padding_left + out_v->align.padding_right;
      dst.info.h = out_v->height +
          out_v->align.padding_top + out_v->align.padding_bottom;
      dst.info.stride = out_v->stride;

      GST_INFO ("(%d,%d,%d,%d)->(%d,%d,%d,%d) base on "
          "in(%d,%d,%d,%d),out(%d,%d,%d,%d)",
          render_x,render_y,render_w,render_h,
          dst.crop.x,dst.crop.y,dst.crop.w,dst.crop.h,
          in_v->crop_x,in_v->crop_y,in_v->crop_w,in_v->crop_h,
          out_v->crop_x,out_v->crop_y,out_v->crop_w,out_v->crop_h);

      if (config_out) {
        if (vcomp->device->config_output(vcomp->device, &dst.info) < 0) {
          GST_WARNING ("config output failed");
          continue;
        }
      }

      if (vcomp->device->blend(vcomp->device, &dst, &src) < 0) {
        GST_WARNING ("blending overlay buffer [%d] failed", n);
        continue;
      }

      blend_cnt++;
    }
  }

  GST_INFO("blended %d overlay buffers", blend_cnt);

  return blend_cnt;
}
