/* GStreamer IMX openCL Device
 * Copyright 2023 NXP
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include "imx_opencl_converter.h"
#include "imx_2d_device.h"

GST_DEBUG_CATEGORY_EXTERN (imx2ddevice_debug);
#define GST_CAT_DEFAULT imx2ddevice_debug

typedef struct {
  gint capabilities;
  void *ocl_handle;
  OCL_FORMAT src_fmt;
  OCL_FORMAT dst_fmt;
  OCL_BUFFER src_buf;
  OCL_BUFFER dst_buf;
  OCL_MEMORY_TYPE mem_type;
  OCL_ALIGN_FLAG align_flag;
} Imx2DDeviceOcl;

typedef struct {
  GstVideoFormat gst_video_format;
  OCL_PIXEL_FORMAT ocl_pixel_format;
  guint bpp;
} OclFmtMap;

static OclFmtMap ocl_fmts_map[] = {
    {GST_VIDEO_FORMAT_RGBx,   OCL_FORMAT_RGBX8888, 32},
    {GST_VIDEO_FORMAT_RGBA,   OCL_FORMAT_RGBA8888, 32},
    {GST_VIDEO_FORMAT_RGB,    OCL_FORMAT_RGB888,   24},
    {GST_VIDEO_FORMAT_NV12,   OCL_FORMAT_NV12,     12},
    {GST_VIDEO_FORMAT_YUY2,   OCL_FORMAT_YUYV,     16},
    {GST_VIDEO_FORMAT_I420,   OCL_FORMAT_I420,     12},
    {GST_VIDEO_FORMAT_RGB16,  OCL_FORMAT_RGB565,   16},
    {GST_VIDEO_FORMAT_BGRA,   OCL_FORMAT_BGRA8888, 32},
    {GST_VIDEO_FORMAT_YV12,   OCL_FORMAT_YV12,     12},
    {GST_VIDEO_FORMAT_NV12_8L128, OCL_FORMAT_NV12_TILED, 12},
    {GST_VIDEO_FORMAT_NV12_10BE_8L128, OCL_FORMAT_NV15_TILED, 15},
    {GST_VIDEO_FORMAT_UNKNOWN, -1, 0}
};

static const OclFmtMap * imx_ocl_get_format_map (GstVideoFormat format)
{
  const OclFmtMap *map = ocl_fmts_map;

  while (map->bpp > 0) {
    if (map->gst_video_format == format)
      return map;
    map++;
  };

  GST_INFO ("ocl : format (%s) is not supported.",
              gst_video_format_to_string(format));

  return NULL;
}

static gint imx_ocl_open (Imx2DDevice *device)
{
  if (!device)
    return -1;

  Imx2DDeviceOcl *ocl = g_slice_alloc (sizeof(Imx2DDeviceOcl));
  if (!ocl) {
    GST_ERROR ("allocate ocl structure failed\n");
    return -1;
  }

  memset (ocl, 0, sizeof (Imx2DDeviceOcl));
  device->priv = (gpointer) ocl;
  if (OCL_Open (OCL_OPEN_FLAG_DEFAULT, &ocl->ocl_handle) || ocl->ocl_handle == NULL) {
    GST_ERROR ("%s Failed to open ocl device.",__FUNCTION__);
    g_slice_free1 (sizeof(Imx2DDeviceOcl), ocl);
    device->priv = NULL;
    return -1;
  }

  /* Configure OCL memory type */
  if (IS_IMX95()) {
    ocl->mem_type = OCL_MEM_TYPE_DEVICE;
  } else {
    ocl->mem_type = OCL_MEM_TYPE_GPU;
  }

  return 0;
}

static gint imx_ocl_close (Imx2DDevice *device)
{
  if (!device)
    return -1;

  if (device) {
    Imx2DDeviceOcl *ocl = (Imx2DDeviceOcl *) (device->priv);
    if (ocl) {
      OCL_Close (ocl->ocl_handle);
      g_slice_free1(sizeof(Imx2DDeviceOcl), ocl);
    }
    device->priv = NULL;
  }
  return 0;
}


static gint
imx_ocl_alloc_mem(Imx2DDevice *device, PhyMemBlock *memblk)
{
  GST_ERROR ("don't support allocate memory");
  return -1;
}

static gint imx_ocl_free_mem(Imx2DDevice *device, PhyMemBlock *memblk)
{
  GST_ERROR ("don't support free memory");
  return -1;
}

static gint imx_ocl_copy_mem(Imx2DDevice* device, PhyMemBlock *dst_mem,
                             PhyMemBlock *src_mem, guint offset, guint size)
{
  GST_ERROR ("don't support copy memory");
  return -1;
}

static gint imx_ocl_frame_copy(Imx2DDevice *device,
                               PhyMemBlock *from, PhyMemBlock *to)
{
   GST_ERROR ("don't support frame copy");
  return -1;
}

static gint imx_ocl_update_colorimetry (OCL_FORMAT *format,
                                    Imx2DColorimetry colorimetry)
{
  if (!format)
    return -1;

  switch (colorimetry.range) {
    case IMX_2D_COLOR_RANGE_LIMITED:
      format->range = OCL_RANGE_LIMITED;
      break;
    case IMX_2D_COLOR_RANGE_FULL:
      format->range = OCL_RANGE_FULL;
      break;
    default:
      break;
  }

  switch (colorimetry.matrix) {
    case IMX_2D_COLOR_MATRIX_DEFAULT:
      format->colorspace = OCL_COLORSPACE_DEFAULT;
      break;
    case IMX_2D_COLOR_MATRIX_BT601_625:
      format->colorspace = OCL_COLORSPACE_BT601_625;
      break;
    case IMX_2D_COLOR_MATRIX_BT601_525:
      format->colorspace = OCL_COLORSPACE_BT601_525;
      break;
    case IMX_2D_COLOR_MATRIX_BT709:
      format->colorspace = OCL_COLORSPACE_BT709;
      break;
    case IMX_2D_COLOR_MATRIX_BT2020:
      format->colorspace = OCL_COLORSPACE_BT2020;
      break;
    default:
      break;
  }

  return 0;
}

static gint imx_ocl_config_input(Imx2DDevice *device, Imx2DVideoInfo* in_info)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDeviceOcl *ocl = (Imx2DDeviceOcl *) (device->priv);
  const OclFmtMap *in_map = imx_ocl_get_format_map(in_info->fmt);
  if (!in_map)
    return -1;

  ocl->src_fmt.format = in_map->ocl_pixel_format;
  ocl->src_fmt.width  = in_info->w;
  ocl->src_fmt.height = in_info->h;
  ocl->src_fmt.stride = in_info->w;
  ocl->src_fmt.sliceheight = in_info->h;
  ocl->src_fmt.right  = in_info->w;
  ocl->src_fmt.bottom = in_info->h;
  ocl->src_fmt.left   = 0;
  ocl->src_fmt.top    = 0;
  ocl->src_fmt.range  = OCL_RANGE_LIMITED;
  ocl->src_fmt.colorspace  = OCL_COLORSPACE_DEFAULT;

  if (in_info->tile_type == IMX_2D_TILE_AMHPION) {
    ocl->src_fmt.stride = in_info->stride / (in_map->bpp/8);
    #define SRC_WIDTH_ALIGN 256
    ocl->src_fmt.stride = (ocl->src_fmt.stride + SRC_WIDTH_ALIGN - 1) & (~(SRC_WIDTH_ALIGN - 1));
    GST_TRACE("IMX_2D_TILE_AMHPION, update stride to %d", ocl->src_fmt.stride);
  }

  GST_TRACE("input format: %s", gst_video_format_to_string(in_info->fmt));
  return 0;
}

static gint imx_ocl_config_output(Imx2DDevice *device, Imx2DVideoInfo* out_info)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDeviceOcl *ocl = (Imx2DDeviceOcl *) (device->priv);
  const OclFmtMap *out_map = imx_ocl_get_format_map(out_info->fmt);
  if (!out_map)
    return -1;

  ocl->dst_fmt.format = out_map->ocl_pixel_format;
  ocl->dst_fmt.width  = out_info->w;
  ocl->dst_fmt.height = out_info->h;
  ocl->dst_fmt.stride = out_info->w;
  ocl->dst_fmt.sliceheight = out_info->h;
  ocl->dst_fmt.right  = out_info->w;
  ocl->dst_fmt.bottom = out_info->h;
  ocl->dst_fmt.left   = 0;
  ocl->dst_fmt.top    = 0;
  ocl->dst_fmt.range  = OCL_RANGE_DEFAULT;
  ocl->dst_fmt.colorspace  = OCL_COLORSPACE_DEFAULT;

  GST_TRACE("output format: %s", gst_video_format_to_string(out_info->fmt));
  return 0;
}

static gint imx_ocl_set_plane (void *handle, OCL_BUFFER *buf, OCL_FORMAT *ocl_format, guint8 *paddr, Imx2DFrame *frame)
{
  guint i = 0;
  OCL_FORMAT_PLANE_INFO plane_info;

  memset(&plane_info, 0, sizeof(OCL_FORMAT_PLANE_INFO));
  plane_info.ocl_format = ocl_format;
  if (OCL_SUCCESS != OCL_GetParam (handle, OCL_PARAM_INDEX_FORMAT_PLANE_INFO, &plane_info)) {
    return -1;
  }

  if (plane_info.plane_num < 1) {
    return -1;
  }

  buf->plane_num = plane_info.plane_num;
  buf->planes[i].size = plane_info.plane_size[i];
  buf->planes[i].paddr = (long long) paddr;
  buf->planes[i].fd = frame->fd[0];
  buf->planes[i].offset = 0;
  GST_TRACE ("ocl : plane num: %d , planes[%d].size: 0x%x, planes[%d].paddr: %p, planes[%d].fd: 0x%x",
    buf->plane_num, i, buf->planes[i].size, i, (guint8 *)buf->planes[i].paddr, i, buf->planes[i].fd);
  i++;
  if (plane_info.plane_num <= 1) {
    return 0;
  }

  while (i < plane_info.plane_num) {
    buf->planes[i].offset = buf->planes[i-1].offset + buf->planes[i-1].size;
    buf->planes[i].size = plane_info.plane_size[i];
    buf->planes[i].paddr = (long long) (buf->planes[i-1].paddr + buf->planes[i-1].size);
    if (frame->fd[i] >= 0)
      buf->planes[i].fd = frame->fd[i];
    else
      buf->planes[i].fd = frame->fd[0];
    GST_TRACE ("ocl : plane num: %d , planes[%d].size: 0x%x, planes[%d].paddr: %p, planes[%d].fd: 0x%x",
    buf->plane_num, i, buf->planes[i].size, i, (guint8 *)buf->planes[i].paddr, i, buf->planes[i].fd);
    i++;
  }

  return 0;
}

static gint imx_ocl_convert (Imx2DDevice *device, Imx2DFrame *dst, Imx2DFrame *src)
{
  gint ret = 0;
  unsigned long paddr = 0;

  if (!device || !device->priv || !dst || !src || !dst->mem || !src->mem)
    return -1;

  Imx2DDeviceOcl *ocl = (Imx2DDeviceOcl *) (device->priv);

  GST_DEBUG ("src paddr fd vaddr: %p %d %p dst paddr fd vaddr: %p %d %p",
      src->mem->paddr, src->fd[0], src->mem->vaddr, dst->mem->paddr,
      dst->fd[0], dst->mem->vaddr);

  /* Check source frame physical address */
  if (!src->mem->paddr) {
    if (src->fd[0] >= 0) {
      paddr = phy_addr_from_fd (src->fd[0]);
    } else if (src->mem->vaddr) {
      paddr = phy_addr_from_vaddr (src->mem->vaddr, PAGE_ALIGN(src->mem->size));
    } else {
      GST_ERROR ("Invalid parameters.");
      ret = -1;
      goto err;
    }

    if (paddr) {
      src->mem->paddr = (guint8 *)paddr;
    } else {
      GST_ERROR ("Can't get physical address.");
      ret = -1;
      goto err;
    }
  }

  /* Check destination frame physical address */
  if (!dst->mem->paddr) {
    paddr = phy_addr_from_fd (dst->fd[0]);
    if (paddr) {
      dst->mem->paddr = (guint8 *) paddr;
    } else {
      GST_ERROR ("Can't get physical address.");
      ret = -1;
      goto err;
    }
  }
  GST_DEBUG ("src paddr: %p dst paddr: %p", src->mem->paddr, dst->mem->paddr);

  /* Update input buffer */
  ocl->src_fmt.left = src->crop.x;
  ocl->src_fmt.top = src->crop.y;
  ocl->src_fmt.right = src->crop.x + MIN(src->crop.w,  ocl->src_fmt.width - src->crop.x);
  ocl->src_fmt.bottom = src->crop.y + MIN(src->crop.h, ocl->src_fmt.height - src->crop.y);
  ocl->src_fmt.alpha = src->alpha;

  if (ocl->src_fmt.left >= ocl->src_fmt.width || ocl->src_fmt.top >= ocl->src_fmt.height ||
      ocl->src_fmt.right <= 0 || ocl->src_fmt.bottom <= 0) {
    GST_WARNING("input crop outside of source");
    ret = -1;
    goto err;
  }

  if (ocl->src_fmt.left < 0)
    ocl->src_fmt.left = 0;
  if (ocl->src_fmt.top < 0)
    ocl->src_fmt.top = 0;
  if (ocl->src_fmt.right > ocl->src_fmt.width)
    ocl->src_fmt.right = ocl->src_fmt.width;
  if (ocl->src_fmt.bottom > ocl->src_fmt.height)
    ocl->src_fmt.bottom = ocl->src_fmt.height;
  imx_ocl_set_plane (ocl->ocl_handle, &ocl->src_buf, &ocl->src_fmt, src->mem->paddr, src);
  /* Update input OCL buffer type */
  ocl->src_buf.mem_type = ocl->mem_type;

  if (src->fd[1] >= 0) {
    if (!src->mem->user_data) {
      src->mem->user_data = (gpointer *) phy_addr_from_fd (src->fd[1]);
    }
    if (src->mem->user_data)
      ocl->src_buf.planes[1].paddr = (long long) src->mem->user_data;
   }

  switch (src->interlace_type) {
    case IMX_2D_INTERLACE_INTERLEAVED:
      ocl->src_fmt.interlace = 1;
      break;
    default:
      ocl->src_fmt.interlace = 0;
      break;
  }

  GST_TRACE ("ocl src : %dx%d,%d(%d,%d-%d,%d), stride: %d, alpha: %d, format: %d",
      ocl->src_fmt.width, ocl->src_fmt.height, ocl->src_fmt.stride, ocl->src_fmt.left,
      ocl->src_fmt.top, ocl->src_fmt.right, ocl->src_fmt.bottom, ocl->src_fmt.stride,
      ocl->src_fmt.alpha, ocl->src_fmt.format);

  /* Update output buffer */
  ocl->dst_fmt.alpha = dst->alpha;
  ocl->dst_fmt.left = dst->crop.x;
  ocl->dst_fmt.top = dst->crop.y;
  ocl->dst_fmt.right = dst->crop.x + dst->crop.w;
  ocl->dst_fmt.bottom = dst->crop.y + dst->crop.h;

  if (ocl->dst_fmt.left >= ocl->dst_fmt.width || ocl->dst_fmt.top >= ocl->dst_fmt.height ||
      ocl->dst_fmt.right <= 0 || ocl->dst_fmt.bottom <= 0) {
    GST_WARNING("output crop outside of destination");
    ret = -1;
    goto err;
  }

  if (ocl->dst_fmt.left < 0)
    ocl->dst_fmt.left = 0;
  if (ocl->dst_fmt.top < 0)
    ocl->dst_fmt.top = 0;
  if (ocl->dst_fmt.right > ocl->dst_fmt.width)
    ocl->dst_fmt.right = ocl->dst_fmt.width;
  if (ocl->dst_fmt.bottom > ocl->dst_fmt.height)
    ocl->dst_fmt.bottom = ocl->dst_fmt.height;

  /* adjust incrop size by outcrop size and output resolution */
  guint src_w, src_h, dst_w, dst_h, org_src_left, org_src_top;
  src_w = ocl->src_fmt.right - ocl->src_fmt.left;
  src_h = ocl->src_fmt.bottom - ocl->src_fmt.top;
  dst_w = dst->crop.w;
  dst_h = dst->crop.h;
  org_src_left = ocl->src_fmt.left;
  org_src_top = ocl->src_fmt.top;
  ocl->src_fmt.left = org_src_left + (ocl->dst_fmt.left-dst->crop.x) * src_w / dst_w;
  ocl->src_fmt.top = org_src_top + (ocl->dst_fmt.top-dst->crop.y) * src_h / dst_h;
  ocl->src_fmt.right = org_src_left + (ocl->dst_fmt.right-dst->crop.x) * src_w / dst_w;
  ocl->src_fmt.bottom = org_src_top + (ocl->dst_fmt.bottom-dst->crop.y) * src_h / dst_h;
  GST_TRACE ("update ocl src :left:%d, top:%d, right:%d, bootm:%d",
      ocl->src_fmt.left, ocl->src_fmt.top, ocl->src_fmt.right, ocl->src_fmt.bottom);

  GST_TRACE ("ocl dest : %dx%d,%d(%d,%d-%d,%d), stride: %d, alpha: %d, format: %d",
      ocl->dst_fmt.width, ocl->dst_fmt.height,ocl->dst_fmt.stride, ocl->dst_fmt.left,
      ocl->dst_fmt.top, ocl->dst_fmt.right, ocl->dst_fmt.bottom, ocl->dst_fmt.stride,
      ocl->dst_fmt.alpha, ocl->dst_fmt.format);

  imx_ocl_set_plane (ocl->ocl_handle, &ocl->dst_buf, &ocl->dst_fmt, dst->mem->paddr, dst);
  /* Update output OCL buffer type */
  ocl->dst_buf.mem_type = ocl->mem_type;

  /* Check destination fd[1] */
  if (dst->fd[1] >= 0) {
    if (phy_addr_from_fd (dst->fd[1]))
      ocl->dst_buf.planes[1].paddr = (long long) phy_addr_from_fd (dst->fd[1]);
  }

  /* Update range and colorspace */
  if (ocl->src_fmt.format <= OCL_FORMAT_BGRA8888) {
    imx_ocl_update_colorimetry (&ocl->dst_fmt, dst->info.colorimetry);
     GST_TRACE("output range: %d, colorspace: %d ",
      ocl->dst_fmt.range, ocl->dst_fmt.colorspace);
  } else if (ocl->src_fmt.format >= OCL_FORMAT_P010) {
    imx_ocl_update_colorimetry (&ocl->src_fmt, src->info.colorimetry);
    GST_TRACE("input range: %d, colorspace: %d ",
      ocl->src_fmt.range, ocl->src_fmt.colorspace);
  }

  OCL_SetParam(ocl->ocl_handle, OCL_PARAM_INDEX_INPUT_FORMAT, &ocl->src_fmt);
  OCL_SetParam(ocl->ocl_handle, OCL_PARAM_INDEX_OUTPUT_FORMAT, &ocl->dst_fmt);

  ret = OCL_Convert(ocl->ocl_handle, &ocl->src_buf, &ocl->dst_buf);

  if (ocl->align_flag == OCL_ALIGN_FLAG_DOWNSCALE
      && ocl->dst_fmt.format == OCL_FORMAT_RGB888) {
    gst_buffer_resize (dst->outbuf, 0, dst->crop.w * dst->crop.h * 3);
    GST_TRACE("resize buffer: w:%d, h:%d",dst->crop.w, dst->crop.h);
  }

err:

  GST_TRACE ("finish\n");
  return ret;
}

static gint imx_ocl_set_rotate (Imx2DDevice *device, Imx2DRotationMode rot)
{
  GST_TRACE ("set rotate: %d", rot);
  return 0;
}

static gint imx_ocl_set_deinterlace (Imx2DDevice *device,
                                    Imx2DDeinterlaceMode mode)
{
  GST_TRACE ("set deinterlace mode: %d", mode);
  return 0;
}

static Imx2DRotationMode imx_ocl_get_rotate (Imx2DDevice* device)
{
  return IMX_2D_ROTATION_0;
}

static Imx2DDeinterlaceMode imx_ocl_get_deinterlace (Imx2DDevice* device)
{
  return IMX_2D_DEINTERLACE_NONE;
}

static gint imx_ocl_get_capabilities (Imx2DDevice* device)
{
  gint capabilities = IMX_2D_DEVICE_CAP_CSC;

  return capabilities;
}

static GList* imx_ocl_get_supported_fmts (OCL_PORT port)
{
  int i = 0;
  int fmt_num = 0;
  OCL_PIXEL_FORMAT *p_fmt;
  const OclFmtMap *map = NULL;
  GList* list = NULL;

  GST_INFO ("get all supported formats: port %d ", port);
  OCL_QuerySupportFormat (port, &fmt_num, &p_fmt);
  while (i < fmt_num) {
    map = ocl_fmts_map;
    while (map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN) {
      if (map->ocl_pixel_format == *(p_fmt + i)) {
        list = g_list_append(list, (gpointer)map->gst_video_format);
        break;
      }
      map++;
    }
    i++;
  }

  if ((!IS_AMPHION()) && port == OCL_PORT_TYPE_INPUT) {
    /* The two formats are supported only for amphion VPU */
    GstVideoFormat ignore_list[2] = {GST_VIDEO_FORMAT_NV12_8L128,
      GST_VIDEO_FORMAT_NV12_10BE_8L128};
    list = g_list_remove (list, (gpointer)ignore_list[0]);
    list = g_list_remove (list, (gpointer)ignore_list[1]);
  }

  return list;
}

static GstVideoFormat imx_ocl_get_format (GstCaps * caps)
{
  gint i, caps_size;
  GstStructure *st;
  const GValue *format;
  const gchar *fmt_name;

  caps_size = gst_caps_get_size (caps);
  for (i = 0; i < caps_size; i++) {
    st = gst_caps_get_structure(caps, i);
    if (!g_strcmp0 (gst_structure_get_string (st, "format"), "DMA_DRM")) {
      format = gst_structure_get_value (st, "drm-format");
    } else {
      format = gst_structure_get_value (st, "format");
    }

    if (!GST_VALUE_HOLDS_LIST (format) && G_VALUE_HOLDS_STRING (format)) {
      fmt_name = g_value_get_string (format);
      return gst_video_format_from_string(fmt_name);
    }
  }

  return GST_VIDEO_FORMAT_UNKNOWN;
}

static gboolean imx_ocl_check_conversion (GstCaps *input_caps, GstCaps *output_caps)
{
  OCL_PIXEL_FORMAT_GROUP *p_group;
  int fmt_num = 0;
  int i = 0;
  OCL_PIXEL_FORMAT in_pixel_format;
  OCL_PIXEL_FORMAT out_pixel_format;

  const OclFmtMap *in_map = imx_ocl_get_format_map (imx_ocl_get_format(input_caps));
  const OclFmtMap *out_map = imx_ocl_get_format_map (imx_ocl_get_format(output_caps));

  if (!in_map || !out_map) {
    GST_INFO ("No valid input or output format, input caps %" GST_PTR_FORMAT
        ", output_caps %" GST_PTR_FORMAT, input_caps, output_caps);
    return FALSE;
  }
  in_pixel_format = in_map->ocl_pixel_format;
  out_pixel_format = out_map->ocl_pixel_format;

  if (!IS_AMPHION()) {
    if (in_pixel_format == OCL_FORMAT_NV12_TILED
        || in_pixel_format == OCL_FORMAT_NV15_TILED) {
      return FALSE;
    }
  }

  OCL_QuerySupportMap (&fmt_num, &p_group);
  while (i < fmt_num) {
    if (p_group->input_format == in_pixel_format
        && p_group->output_format == out_pixel_format) {
      return TRUE;
    }
    i++;
    p_group++;
  }

  return FALSE;
}

static gboolean imx_ocl_get_alignment (Imx2DDevice* device, GstVideoInfo *in_info,
  GstVideoInfo *out_info, Imx2DAlignInfo *align_info)
{
  OCL_ALIGN_FLAG align_flag = OCL_ALIGN_FLAG_DEFAULT;
  OCL_ALIGN_INFO ocl_align;
  gboolean ret = FALSE;
  OCL_RESULT result;

  if (!device || !in_info || !out_info || !align_info)
    return FALSE;

  Imx2DDeviceOcl *ocl = (Imx2DDeviceOcl *) (device->priv);
  memset (align_info, 0, sizeof (Imx2DAlignInfo));
  const OclFmtMap *in_map = imx_ocl_get_format_map(GST_VIDEO_INFO_FORMAT(in_info));
  const OclFmtMap *out_map = imx_ocl_get_format_map(GST_VIDEO_INFO_FORMAT(out_info));
  if (!in_map || ! out_map) {
    GST_INFO ("Can't get supported format map");
    return ret;
  }

  if (GST_VIDEO_INFO_WIDTH (in_info) != GST_VIDEO_INFO_WIDTH (out_info)
      || GST_VIDEO_INFO_HEIGHT (in_info) != GST_VIDEO_INFO_HEIGHT (out_info)) {
    align_flag = OCL_ALIGN_FLAG_DOWNSCALE;
    ocl->align_flag = align_flag;
  } else {
    ocl->align_flag = align_flag;
    return ret;
  }

  result = OCL_QueryAlignmentInfo (align_flag, &ocl_align);
  if (result != OCL_SUCCESS) {
    GST_INFO ("ocl query align info, result: %d", result);
    return ret;
  } else {
    align_info->width_align = ocl_align.width_align;
    align_info->height_align = ocl_align.height_align;
    align_info->size_align = ocl_align.size_align;
    ret = TRUE;
  }

  GST_INFO ("get align, ocl fmt:%d to %d, %dx%d to %dx%d, "
      "down scale:%d, output align(w,h,size):%d,%d,%d",
      in_map->ocl_pixel_format, out_map->ocl_pixel_format,
      GST_VIDEO_INFO_WIDTH (in_info), GST_VIDEO_INFO_HEIGHT (in_info),
      GST_VIDEO_INFO_WIDTH (out_info), GST_VIDEO_INFO_HEIGHT (out_info),
      align_flag, align_info->width_align,
      align_info->height_align, align_info->size_align);
  return ret;
}

static GList* imx_ocl_get_supported_in_fmts (Imx2DDevice* device)
{
  return imx_ocl_get_supported_fmts (OCL_PORT_TYPE_INPUT);
}

static GList* imx_ocl_get_supported_out_fmts (Imx2DDevice* device)
{
  return imx_ocl_get_supported_fmts (OCL_PORT_TYPE_OUTPUT);
}

static gint imx_ocl_blend (Imx2DDevice *device, Imx2DFrame *dst, Imx2DFrame *src)
{
  return 0;
}

static gint imx_ocl_blend_finish (Imx2DDevice *device)
{
  return 0;
}

static gint imx_ocl_fill_color (Imx2DDevice *device, Imx2DFrame *dst,
                                guint RGBA8888)
{
  return 0;
}

Imx2DDevice * imx_ocl_create (Imx2DDeviceType  device_type)
{
  Imx2DDevice * device = g_slice_alloc(sizeof(Imx2DDevice));
  if (!device) {
    GST_ERROR("allocate device structure failed\n");
    return NULL;
  }

  device->device_type = device_type;
  device->priv = NULL;

  device->open                = imx_ocl_open;
  device->close               = imx_ocl_close;
  device->alloc_mem           = imx_ocl_alloc_mem;
  device->free_mem            = imx_ocl_free_mem;
  device->copy_mem            = imx_ocl_copy_mem;
  device->frame_copy          = imx_ocl_frame_copy;
  device->config_input        = imx_ocl_config_input;
  device->config_output       = imx_ocl_config_output;
  device->convert             = imx_ocl_convert;
  device->blend               = imx_ocl_blend;
  device->blend_finish        = imx_ocl_blend_finish;
  device->fill                = imx_ocl_fill_color;
  device->set_rotate          = imx_ocl_set_rotate;
  device->set_deinterlace     = imx_ocl_set_deinterlace;
  device->get_rotate          = imx_ocl_get_rotate;
  device->get_deinterlace     = imx_ocl_get_deinterlace;
  device->get_capabilities    = imx_ocl_get_capabilities;
  device->get_supported_in_fmts  = imx_ocl_get_supported_in_fmts;
  device->get_supported_out_fmts = imx_ocl_get_supported_out_fmts;
  device->check_conversion       = imx_ocl_check_conversion;
  device->get_alignment       = imx_ocl_get_alignment;

  return device;
}

gint imx_ocl_destroy (Imx2DDevice *device)
{
  if (!device)
    return -1;

  g_slice_free1 (sizeof(Imx2DDevice), device);

  return 0;
}

gboolean imx_ocl_is_exist (void)
{
  if (IS_IMX8MM()) {
    return FALSE;
  }
  return TRUE;
}
