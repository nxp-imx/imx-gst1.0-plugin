/* GStreamer IMX PXP Device
 * Copyright (c) 2015, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2017-2018 NXP
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

#include <fcntl.h>
#include <sys/ioctl.h>
#include "pxp_lib.h"
#include "imx_2d_device.h"

GST_DEBUG_CATEGORY_EXTERN (imx2ddevice_debug);
#define GST_CAT_DEFAULT imx2ddevice_debug

/* current PXP hardware has some problem on some video format when performing
 * CSC. undefine following macro to disable those formats for now.
 * if next PXP module solved those problem, we should enable them again
 */
//#define   ENABLE_ALL_PXP_FORMATS

#define PXP_WAIT_COMPLETE_TIME    3
#define ENABLE_PXP_ALPHA_OVERLAY
#define PXP_OVERLAY_TMP_BUF_SIZE_INIT       (1280*720*2)
#define PXP_OVERLAY_RGB_TMP_BUF_SIZE_INIT   (1280*720*2)

typedef struct _Imx2DDevicePxp {
  gint capabilities;
  struct pxp_config_data config;
  pxp_chan_handle_t pxp_chan;
  gboolean first_frame_done;
#ifdef ENABLE_PXP_ALPHA_OVERLAY
  PhyMemBlock ov_temp;
  PhyMemBlock dummy;
  PhyMemBlock rgb_temp;
  guint background;
#endif
} Imx2DDevicePxp;

typedef struct {
  GstVideoFormat gst_video_format;
  guint pxp_format;
  guint bpp;
} PxpFmtMap;

static PxpFmtMap pxp_in_fmts_map[] = {
    {GST_VIDEO_FORMAT_BGRx,   PXP_PIX_FMT_RGB32,    32},
    {GST_VIDEO_FORMAT_RGB16,  PXP_PIX_FMT_RGB565,   16},
    {GST_VIDEO_FORMAT_RGB15,  PXP_PIX_FMT_RGB555,   16},

    {GST_VIDEO_FORMAT_I420,   PXP_PIX_FMT_YUV420P,  12},
    {GST_VIDEO_FORMAT_YV12,   PXP_PIX_FMT_YVU420P,  12},
    {GST_VIDEO_FORMAT_Y42B,   PXP_PIX_FMT_YUV422P,  16},
    {GST_VIDEO_FORMAT_UYVY,   PXP_PIX_FMT_UYVY,     16},
    {GST_VIDEO_FORMAT_YUY2,   PXP_PIX_FMT_YUYV,     16},
    {GST_VIDEO_FORMAT_YVYU,   PXP_PIX_FMT_YVYU,     16},
    {GST_VIDEO_FORMAT_NV12,   PXP_PIX_FMT_NV12,     12},
    {GST_VIDEO_FORMAT_NV21,   PXP_PIX_FMT_NV21,     12},
    {GST_VIDEO_FORMAT_NV16,   PXP_PIX_FMT_NV16,     16},
#ifdef ENABLE_ALL_PXP_FORMATS
    {GST_VIDEO_FORMAT_BGRA,   PXP_PIX_FMT_BGRA32,   32},
    {GST_VIDEO_FORMAT_AYUV,   PXP_PIX_FMT_VUY444,   32},
    {GST_VIDEO_FORMAT_GRAY8,  PXP_PIX_FMT_GREY,     8},
#endif

    /* There is no corresponding GST Video format for those PXP input formats
    PXP_PIX_FMT_GY04
    PXP_PIX_FMT_YVU422P
    PXP_PIX_FMT_VYUY
    PXP_PIX_FMT_NV61
     */

    {GST_VIDEO_FORMAT_UNKNOWN, -1,          0}
};

static PxpFmtMap pxp_out_fmts_map[] = {
    {GST_VIDEO_FORMAT_BGRx,   PXP_PIX_FMT_RGB32,    32},
    {GST_VIDEO_FORMAT_BGRA,   PXP_PIX_FMT_BGRA32,   32},
    {GST_VIDEO_FORMAT_BGR,    PXP_PIX_FMT_RGB24,    24},
    {GST_VIDEO_FORMAT_RGB16,  PXP_PIX_FMT_RGB565,   16},
    {GST_VIDEO_FORMAT_GRAY8,  PXP_PIX_FMT_GREY,     8},
#ifdef ENABLE_ALL_PXP_FORMATS
    {GST_VIDEO_FORMAT_RGB15,  PXP_PIX_FMT_RGB555,   16},
    {GST_VIDEO_FORMAT_UYVY,   PXP_PIX_FMT_UYVY,     16},
    {GST_VIDEO_FORMAT_NV12,   PXP_PIX_FMT_NV12,     12},
    {GST_VIDEO_FORMAT_NV21,   PXP_PIX_FMT_NV21,     12},
    {GST_VIDEO_FORMAT_NV16,   PXP_PIX_FMT_NV16,     16},
#endif

    /* There is no corresponding GST Video format for those PXP output formats
    PXP_PIX_FMT_GY04
    PXP_PIX_FMT_VYUY
    PXP_PIX_FMT_NV61
     */

    {GST_VIDEO_FORMAT_UNKNOWN, -1,          0}
};

static const PxpFmtMap * imx_pxp_get_format(GstVideoFormat format,
                                            PxpFmtMap *map)
{
  while(map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN) {
    if (map->gst_video_format == format)
      return map;
    map++;
  };

  GST_ERROR ("pxp : format (%s) is not supported.",
              gst_video_format_to_string(format));

  return NULL;
}

static gint imx_pxp_open(Imx2DDevice *device)
{
  if (!device)
    return -1;

  gint ret = pxp_init();
  if (ret < 0) {
    GST_ERROR("pxp init failed (%d)", ret);
    return -1;
  }

  pxp_chan_handle_t pxp_chan;
  ret = pxp_request_channel(&pxp_chan);
  if (ret < 0) {
    pxp_uninit();
    GST_ERROR("pxp request channel failed (%d)", ret);
    return -1;
  }

  Imx2DDevicePxp *pxp = g_slice_alloc(sizeof(Imx2DDevicePxp));
  if (!pxp) {
    pxp_release_channel(&pxp_chan);
    pxp_uninit();
    GST_ERROR("allocate pxp structure failed");
    return -1;
  }

  memset(pxp, 0, sizeof (Imx2DDevicePxp));
  memcpy(&pxp->pxp_chan, &pxp_chan, sizeof(pxp_chan_handle_t));

  device->priv = (gpointer)pxp;

  GST_DEBUG("requested pxp chan handle %d", pxp->pxp_chan.handle);
  return 0;
}

static gint
imx_pxp_alloc_mem(Imx2DDevice *device, PhyMemBlock *memblk)
{
  if (!device || !device->priv || !memblk)
    return -1;

  struct pxp_mem_desc *mem = g_slice_alloc(sizeof(struct pxp_mem_desc));
  if (!mem)
    return -1;

  memset(mem, 0, sizeof (struct pxp_mem_desc));
  mem->size = memblk->size;

  gint ret = pxp_get_mem (mem);
  if (ret < 0) {
    GST_ERROR("PXP allocate %u bytes memory failed: %s",
              memblk->size, strerror(errno));
    return -1;
  }

  memblk->vaddr = (guchar*) mem->virt_uaddr;
  memblk->paddr = (guchar*) mem->phys_addr;
  memblk->user_data = (gpointer) mem;
  GST_DEBUG("PXP allocated memory (%p)", memblk->paddr);

  return 0;
}

static gint imx_pxp_free_mem(Imx2DDevice *device, PhyMemBlock *memblk)
{
  if (!device || !device->priv || !memblk)
    return -1;

  if (memblk->vaddr == NULL)
    return 0;

  GST_DEBUG("PXP free memory (%p)", memblk->paddr);
  gint ret = pxp_put_mem ((struct pxp_mem_desc*)(memblk->user_data));
  memblk->user_data = NULL;
  memblk->vaddr = NULL;
  memblk->paddr = NULL;
  memblk->size = 0;

  return ret;
}

static gint imx_pxp_close(Imx2DDevice *device)
{
  if (!device)
    return -1;

  if (device) {
    Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
    if (pxp) {
      imx_pxp_free_mem(device, &pxp->ov_temp);
      imx_pxp_free_mem(device, &pxp->dummy);
      imx_pxp_free_mem(device, &pxp->rgb_temp);
      pxp_release_channel(&pxp->pxp_chan);
      pxp_uninit();
      g_slice_free1(sizeof(Imx2DDevicePxp), pxp);
    }
    device->priv = NULL;
  }

  return 0;
}

static gint imx_pxp_copy_mem(Imx2DDevice* device, PhyMemBlock *dst_mem,
                             PhyMemBlock *src_mem, guint offset, guint size)
{
  if (!device || !device->priv || !src_mem || !dst_mem)
    return -1;

  if (size > src_mem->size - offset)
    size = src_mem->size - offset;

  struct pxp_mem_desc *mem = g_slice_alloc(sizeof(struct pxp_mem_desc));
  if (!mem)
    return -1;

  memset(mem, 0, sizeof (struct pxp_mem_desc));
  mem->size = size;

  gint ret = pxp_get_mem (mem);
  if (ret < 0) {
    GST_ERROR("PXP allocate %u bytes memory failed: %s", size, strerror(errno));
    return -1;
  }

  dst_mem->vaddr = (guchar*) mem->virt_uaddr;
  dst_mem->paddr = (guchar*) mem->phys_addr;
  dst_mem->user_data = (gpointer) mem;

  memcpy(dst_mem->vaddr, src_mem->vaddr+offset, size);

  GST_DEBUG ("PXP copy from vaddr (%p), paddr (%p), size (%d) to "
      "vaddr (%p), paddr (%p), size (%d)",
      src_mem->vaddr, src_mem->paddr, src_mem->size,
      dst_mem->vaddr, dst_mem->paddr, dst_mem->size);

  return 0;
}

static gint imx_pxp_frame_copy(Imx2DDevice *device,
                               PhyMemBlock *from, PhyMemBlock *to)
{
  if (!device || !device->priv || !from || !to)
    return -1;

  memcpy(to->vaddr, from->vaddr, (from->size > to->size) ? to->size:from->size);
  GST_LOG("PXP frame memory (%p)->(%p)", from->paddr, to->paddr);

  return 0;
}

static gint imx_pxp_config_input(Imx2DDevice *device, Imx2DVideoInfo* in_info)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
  const PxpFmtMap *in_map = imx_pxp_get_format(in_info->fmt, pxp_in_fmts_map);
  if (!in_map)
    return -1;

  pxp->config.proc_data.srect.left = 0;
  pxp->config.proc_data.srect.top = 0;
  pxp->config.proc_data.srect.width = in_info->w;
  pxp->config.proc_data.srect.height = in_info->h;

  /* set S0 parameters */
  pxp->config.s0_param.pixel_fmt = in_map->pxp_format;
  pxp->config.s0_param.width = in_info->w;
  pxp->config.s0_param.height = in_info->h;
  pxp->config.s0_param.stride = in_info->w;

  GST_TRACE("input format = %s", gst_video_format_to_string(in_info->fmt));

  return 0;
}

static gint imx_pxp_config_output(Imx2DDevice *device, Imx2DVideoInfo* out_info)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
  const PxpFmtMap *out_map = imx_pxp_get_format(out_info->fmt,pxp_out_fmts_map);
  if (!out_map)
    return -1;

  pxp->config.proc_data.drect.left = 0;
  pxp->config.proc_data.drect.top = 0;
  pxp->config.proc_data.drect.width = out_info->w;
  pxp->config.proc_data.drect.height = out_info->h;

  /* set Output channel parameters */
  pxp->config.out_param.pixel_fmt = out_map->pxp_format;
  pxp->config.out_param.width = out_info->w;
  pxp->config.out_param.height = out_info->h;
  pxp->config.out_param.stride = out_info->w;

  GST_TRACE("output format = %s", gst_video_format_to_string(out_info->fmt));

  return 0;
}

static gint imx_pxp_do_channel(Imx2DDevicePxp *pxp)
{
  gint ret = 0;
  ret = pxp_config_channel(&pxp->pxp_chan, &pxp->config);
  if (ret < 0) {
    GST_ERROR("pxp config channel fail (%d)", ret);
    return -1;
  }

  ret = pxp_start_channel(&pxp->pxp_chan);
  if (ret < 0) {
    GST_ERROR("pxp start channel fail (%d)", ret);
    return -1;
  }

  ret = pxp_wait_for_completion(&pxp->pxp_chan, 3);
  if (ret < 0) {
    GST_ERROR("pxp wait for completion fail (%d)", ret);
    return -1;
  }

  return ret;
}

static gint imx_pxp_convert(Imx2DDevice *device,
                            Imx2DFrame *dst, Imx2DFrame *src)
{
  gint ret = 0;

  if (!device || !device->priv || !dst || !src || !dst->mem || !src->mem)
    return -1;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
  memset(&pxp->config.ol_param[0], 0, sizeof(struct pxp_layer_param));

  // Set input crop
  pxp->config.proc_data.srect.left = src->crop.x;
  pxp->config.proc_data.srect.top = src->crop.y;
  pxp->config.proc_data.srect.width =
      MIN(src->crop.w, pxp->config.proc_data.srect.width);
  pxp->config.proc_data.srect.height =
      MIN(src->crop.h, pxp->config.proc_data.srect.height);

  pxp->config.s0_param.paddr = (dma_addr_t)src->mem->paddr;

  GST_TRACE ("pxp src : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.s0_param.width, pxp->config.s0_param.height,
      pxp->config.s0_param.stride,
      pxp->config.proc_data.srect.left, pxp->config.proc_data.srect.top,
      pxp->config.proc_data.srect.width, pxp->config.proc_data.srect.height,
      pxp->config.s0_param.pixel_fmt);

  // Set output crop
  pxp->config.proc_data.drect.left = dst->crop.x;
  pxp->config.proc_data.drect.top = dst->crop.y;
  pxp->config.proc_data.drect.width =
      MIN(dst->crop.w, pxp->config.proc_data.drect.width);
  pxp->config.proc_data.drect.height =
      MIN(dst->crop.h, pxp->config.proc_data.drect.height);

  pxp->config.out_param.paddr = (dma_addr_t)dst->mem->paddr;

  GST_TRACE ("pxp dest : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.out_param.width, pxp->config.out_param.height,
      pxp->config.out_param.stride,
      pxp->config.proc_data.drect.left, pxp->config.proc_data.drect.top,
      pxp->config.proc_data.drect.width, pxp->config.proc_data.drect.height,
      pxp->config.out_param.pixel_fmt);

  // Final conversion
  return imx_pxp_do_channel(pxp);
}

static gint imx_pxp_blend_without_alpha(Imx2DDevice *device,
                                        Imx2DFrame *dst, Imx2DFrame *src)
{
  gint ret = 0;

  if (!device || !device->priv || !dst || !src || !dst->mem || !src->mem)
    return -1;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
  memset(&pxp->config.ol_param[0], 0, sizeof(struct pxp_layer_param));

  if (pxp->first_frame_done == FALSE) {
    pxp->config.proc_data.drect.left = dst->crop.x;
    pxp->config.proc_data.drect.top = dst->crop.y;
    pxp->config.proc_data.drect.width =
        MIN(dst->crop.w, pxp->config.proc_data.drect.width);
    pxp->config.proc_data.drect.height =
        MIN(dst->crop.h, pxp->config.proc_data.drect.height);
    pxp->config.out_param.paddr = (dma_addr_t)dst->mem->paddr;
    pxp->first_frame_done = TRUE;
  } else {
    pxp->config.proc_data.drect.left = 0;
    pxp->config.proc_data.drect.top = 0;
    pxp->config.proc_data.drect.width = dst->crop.w;
    pxp->config.proc_data.drect.height = dst->crop.h;
    pxp->config.out_param.paddr = (dma_addr_t)dst->mem->paddr +
        (dst->crop.y * dst->info.stride +
            dst->crop.x*(dst->info.stride/dst->info.w));

    pxp->config.out_param.width = pxp->config.proc_data.drect.width;
    pxp->config.out_param.height = pxp->config.proc_data.drect.height;
  }

  pxp->config.proc_data.srect.left = src->crop.x;
  pxp->config.proc_data.srect.top = src->crop.y;
  pxp->config.proc_data.srect.width = MIN(src->crop.w, src->info.w);
  pxp->config.proc_data.srect.height = MIN(src->crop.h, src->info.h);
  pxp->config.s0_param.paddr = (dma_addr_t)src->mem->paddr;

  GST_TRACE ("pxp dest : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.out_param.width, pxp->config.out_param.height,
      pxp->config.out_param.stride,
      pxp->config.proc_data.drect.left, pxp->config.proc_data.drect.top,
      pxp->config.proc_data.drect.width, pxp->config.proc_data.drect.height,
      pxp->config.out_param.pixel_fmt);

  GST_TRACE ("pxp s0 : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.s0_param.width, pxp->config.s0_param.height,
      pxp->config.s0_param.stride,
      pxp->config.proc_data.srect.left, pxp->config.proc_data.srect.top,
      pxp->config.proc_data.srect.width, pxp->config.proc_data.srect.height,
      pxp->config.s0_param.pixel_fmt);

  return imx_pxp_do_channel(pxp);
}

static gint imx_pxp_overlay(Imx2DDevice *device,
                            Imx2DFrame *dst, Imx2DFrame *src)
{
  guint orig_dst_w;
  guint orig_dst_h;
  guint orig_dst_s;
  guint orig_dst_fmt;
  guint orig_src_fmt;
  guint BPP = 4;
  const PxpFmtMap *fmt_map = NULL;

  if (!device || !device->priv || !dst || !src || !dst->mem || !src->mem)
    return -1;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
  memset(&pxp->config.ol_param[0], 0, sizeof(struct pxp_layer_param));

  orig_src_fmt = pxp->config.s0_param.pixel_fmt;
  orig_dst_fmt = pxp->config.out_param.pixel_fmt;
  orig_dst_w = dst->info.w;
  orig_dst_h = dst->info.h;
  orig_dst_s = dst->info.w;

  if (pxp->ov_temp.vaddr == NULL) {
    pxp->ov_temp.size = PXP_OVERLAY_TMP_BUF_SIZE_INIT;
    if (imx_pxp_alloc_mem(device, &pxp->ov_temp) < 0)
      return -1;
  }

  fmt_map = imx_pxp_get_format(dst->info.fmt, pxp_out_fmts_map);
  if (fmt_map)
    BPP = fmt_map->bpp/8 + (fmt_map->bpp%8 ? 1 : 0);

  if (pxp->ov_temp.vaddr
      && pxp->ov_temp.size < (dst->crop.w * dst->crop.h * BPP)) {
    imx_pxp_free_mem(device, &pxp->ov_temp);
    pxp->ov_temp.size = dst->crop.w * dst->crop.h * BPP;
    GST_LOG ("reallocte memory %d, BPP=%d", pxp->ov_temp.size, BPP);
    if (imx_pxp_alloc_mem(device, &pxp->ov_temp) < 0)
      return -1;
  }

  if (pxp->ov_temp.vaddr == NULL)
    return -1;

  if (pxp->first_frame_done == FALSE) {
    //pxp background was filled along with output, if the first frame isn't done
    //we need fill the background before we can apply alpha blending on the
    //background.
    //output a small dummy area with the color of background to let pxp fill all
    //output frame with background color.
    if (pxp->dummy.vaddr == NULL) {
      pxp->dummy.size = 16 * 16 * 4;
      if (imx_pxp_alloc_mem(device, &pxp->dummy) < 0)
        return -1;
    }

    gchar R,G,B,A;
    R = pxp->background & 0x000000FF;
    G = (pxp->background & 0x0000FF00) >> 8;
    B = (pxp->background & 0x00FF0000) >> 16;
    A = (pxp->background & 0xFF000000) >> 24;

    gchar *p = pxp->dummy.vaddr;
    gint i;
    for (i = 0; i < 16*16; i++) {
      p[4 * i + 0] = B;
      p[4 * i + 1] = G;
      p[4 * i + 2] = R;
      p[4 * i + 3] = A;
    }

    pxp->config.proc_data.srect.left = 0;
    pxp->config.proc_data.srect.top = 0;
    pxp->config.proc_data.srect.width = 16;
    pxp->config.proc_data.srect.height = 16;
    pxp->config.s0_param.width = 16;
    pxp->config.s0_param.height = 16;
    pxp->config.s0_param.stride = 16;
    pxp->config.s0_param.pixel_fmt = PXP_PIX_FMT_RGB32;
    pxp->config.s0_param.paddr = (dma_addr_t)pxp->dummy.paddr;

    pxp->config.proc_data.drect.left = 0;
    pxp->config.proc_data.drect.top = 0;
    pxp->config.proc_data.drect.width = 16;
    pxp->config.proc_data.drect.height = 16;
    pxp->config.out_param.paddr = (dma_addr_t)dst->mem->paddr;

    imx_pxp_do_channel(pxp);
    pxp->first_frame_done = TRUE;
  }

  // get the original overlapped destination area to tmep buffer
  pxp->config.s0_param.paddr = (dma_addr_t)dst->mem->paddr;
  pxp->config.s0_param.pixel_fmt = orig_dst_fmt;
  pxp->config.s0_param.width = orig_dst_w;
  pxp->config.s0_param.height = orig_dst_h;
  pxp->config.s0_param.stride = orig_dst_s;
  pxp->config.proc_data.srect.left = dst->crop.x;
  pxp->config.proc_data.srect.top = dst->crop.y;
  pxp->config.proc_data.srect.width = MIN(dst->crop.w, orig_dst_w-dst->crop.x);
  pxp->config.proc_data.srect.height = MIN(dst->crop.h, orig_dst_h-dst->crop.y);

  GST_TRACE ("pxp temp src : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.s0_param.width, pxp->config.s0_param.height,
      pxp->config.s0_param.stride,
      pxp->config.proc_data.srect.left, pxp->config.proc_data.srect.top,
      pxp->config.proc_data.srect.width, pxp->config.proc_data.srect.height,
      pxp->config.s0_param.pixel_fmt);

  pxp->config.out_param.paddr = (dma_addr_t)pxp->ov_temp.paddr;
  pxp->config.proc_data.drect.left = 0;
  pxp->config.proc_data.drect.top = 0;
  pxp->config.proc_data.drect.width = pxp->config.proc_data.srect.width;
  pxp->config.proc_data.drect.height = pxp->config.proc_data.srect.height;
  pxp->config.out_param.pixel_fmt = orig_dst_fmt;
  pxp->config.out_param.width = dst->crop.w;
  pxp->config.out_param.height = dst->crop.h;
  pxp->config.out_param.stride = dst->crop.w;

  GST_TRACE ("pxp temp dest : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.out_param.width, pxp->config.out_param.height,
      pxp->config.out_param.stride,
      pxp->config.proc_data.drect.left, pxp->config.proc_data.drect.top,
      pxp->config.proc_data.drect.width, pxp->config.proc_data.drect.height,
      pxp->config.out_param.pixel_fmt);

  if (imx_pxp_do_channel(pxp) < 0) {
    GST_ERROR("pxp overlay copy temp dst buffer failed");
    return -1;
  }

  if (orig_src_fmt == PXP_PIX_FMT_RGB32 || orig_src_fmt == PXP_PIX_FMT_BGRA32 ||
      orig_src_fmt == PXP_PIX_FMT_RGB565 || orig_src_fmt == PXP_PIX_FMT_RGB555){
    //overlay don't support resize, resize to s0 size before blending
    if (dst->crop.w != src->crop.w || dst->crop.h != src->crop.h) {
      if (pxp->rgb_temp.vaddr == NULL) {
        pxp->rgb_temp.size = PXP_OVERLAY_RGB_TMP_BUF_SIZE_INIT;
        if (imx_pxp_alloc_mem(device, &pxp->rgb_temp) < 0)
          return -1;
      }

      guint BPP = 2;
      if (orig_src_fmt == PXP_PIX_FMT_RGB32
          || orig_src_fmt == PXP_PIX_FMT_BGRA32)
        BPP = 4;

      if (pxp->rgb_temp.vaddr
          && pxp->rgb_temp.size < (orig_dst_w * orig_dst_h * BPP)) {
        imx_pxp_free_mem(device, &pxp->rgb_temp);
        pxp->rgb_temp.size = orig_dst_w * orig_dst_h * BPP;
        GST_LOG ("reallocte memory %d", pxp->rgb_temp.size);
        if (imx_pxp_alloc_mem(device, &pxp->rgb_temp) < 0)
          return -1;
      }

      pxp->config.s0_param.paddr = (dma_addr_t)src->mem->paddr;
      pxp->config.s0_param.pixel_fmt = orig_src_fmt;
      pxp->config.s0_param.width = src->info.w;
      pxp->config.s0_param.height = src->info.h;
      pxp->config.s0_param.stride = src->info.w;
      pxp->config.proc_data.srect.left = src->crop.x;
      pxp->config.proc_data.srect.top = src->crop.y;
      pxp->config.proc_data.srect.width = MIN(src->crop.w, src->info.w);
      pxp->config.proc_data.srect.height = MIN(src->crop.h, src->info.h);

      GST_TRACE ("pxp rgb resize src : %dx%d,%d(%d,%d-%d,%d), format=%x",
          pxp->config.s0_param.width, pxp->config.s0_param.height,
          pxp->config.s0_param.stride,
          pxp->config.proc_data.srect.left, pxp->config.proc_data.srect.top,
          pxp->config.proc_data.srect.width, pxp->config.proc_data.srect.height,
          pxp->config.s0_param.pixel_fmt);

      pxp->config.out_param.paddr = (dma_addr_t)pxp->rgb_temp.paddr;
      pxp->config.proc_data.drect.left = 0;
      pxp->config.proc_data.drect.top = 0;
      pxp->config.proc_data.drect.width = MIN(dst->crop.w, orig_dst_w-dst->crop.x);
      pxp->config.proc_data.drect.height = MIN(dst->crop.h, orig_dst_h-dst->crop.y);
      pxp->config.out_param.pixel_fmt = orig_src_fmt;
      pxp->config.out_param.width = dst->crop.w;
      pxp->config.out_param.height = dst->crop.h;
      pxp->config.out_param.stride = dst->crop.w;

      GST_TRACE ("pxp rgb resize dest : %dx%d,%d(%d,%d-%d,%d), format=%x",
          pxp->config.out_param.width, pxp->config.out_param.height,
          pxp->config.out_param.stride,
          pxp->config.proc_data.drect.left, pxp->config.proc_data.drect.top,
          pxp->config.proc_data.drect.width, pxp->config.proc_data.drect.height,
          pxp->config.out_param.pixel_fmt);

      if (imx_pxp_do_channel(pxp) < 0) {
        GST_ERROR("pxp overlay copy temp dst buffer failed");
        return -1;
      }

      pxp->config.ol_param[0].left = 0;
      pxp->config.ol_param[0].top = 0;
      pxp->config.ol_param[0].stride = dst->crop.w;
      pxp->config.ol_param[0].paddr = (dma_addr_t)pxp->rgb_temp.paddr;
    } else {
      pxp->config.ol_param[0].left = src->crop.x;
      pxp->config.ol_param[0].top = src->crop.y;
      pxp->config.ol_param[0].stride = src->info.w;
      pxp->config.ol_param[0].paddr = (dma_addr_t)src->mem->paddr;
    }

    pxp->config.ol_param[0].pixel_fmt = orig_src_fmt;
  } else {
    //overlay don't support YUV color space. need convert src to RGB space first
    if (pxp->rgb_temp.vaddr == NULL) {
      pxp->rgb_temp.size = PXP_OVERLAY_RGB_TMP_BUF_SIZE_INIT;
      if (imx_pxp_alloc_mem(device, &pxp->rgb_temp) < 0)
        return -1;
    }

    if (pxp->rgb_temp.vaddr
        && pxp->rgb_temp.size < (dst->crop.w * dst->crop.h * 2)) {
      imx_pxp_free_mem(device, &pxp->rgb_temp);
      pxp->rgb_temp.size = dst->crop.w * dst->crop.h * 2;
      GST_LOG ("reallocte memory %d", pxp->rgb_temp.size);
      if (imx_pxp_alloc_mem(device, &pxp->rgb_temp) < 0)
        return -1;
    }

    pxp->config.s0_param.paddr = (dma_addr_t)src->mem->paddr;
    pxp->config.s0_param.pixel_fmt = orig_src_fmt;
    pxp->config.s0_param.width = src->info.w;
    pxp->config.s0_param.height = src->info.h;
    pxp->config.s0_param.stride = src->info.w;
    pxp->config.proc_data.srect.left = src->crop.x;
    pxp->config.proc_data.srect.top = src->crop.y;
    pxp->config.proc_data.srect.width = MIN(src->crop.w, src->info.w);
    pxp->config.proc_data.srect.height = MIN(src->crop.h, src->info.h);

    GST_TRACE ("pxp rgb temp src : %dx%d,%d(%d,%d-%d,%d), format=%x",
        pxp->config.s0_param.width, pxp->config.s0_param.height,
        pxp->config.s0_param.stride,
        pxp->config.proc_data.srect.left, pxp->config.proc_data.srect.top,
        pxp->config.proc_data.srect.width, pxp->config.proc_data.srect.height,
        pxp->config.s0_param.pixel_fmt);

    pxp->config.out_param.paddr = (dma_addr_t)pxp->rgb_temp.paddr;
    pxp->config.proc_data.drect.left = 0;
    pxp->config.proc_data.drect.top = 0;
    pxp->config.proc_data.drect.width = MIN(dst->crop.w, orig_dst_w-dst->crop.x);
    pxp->config.proc_data.drect.height = MIN(dst->crop.h, orig_dst_h-dst->crop.y);
    pxp->config.out_param.pixel_fmt = PXP_PIX_FMT_RGB565;
    pxp->config.out_param.width = dst->crop.w;
    pxp->config.out_param.height = dst->crop.h;
    pxp->config.out_param.stride = dst->crop.w;

    GST_TRACE ("pxp rgb temp dest : %dx%d,%d(%d,%d-%d,%d), format=%x",
        pxp->config.out_param.width, pxp->config.out_param.height,
        pxp->config.out_param.stride,
        pxp->config.proc_data.drect.left, pxp->config.proc_data.drect.top,
        pxp->config.proc_data.drect.width, pxp->config.proc_data.drect.height,
        pxp->config.out_param.pixel_fmt);

    if (imx_pxp_do_channel(pxp) < 0) {
      GST_ERROR("pxp overlay copy temp dst buffer failed");
      return -1;
    }

    pxp->config.ol_param[0].left = 0;
    pxp->config.ol_param[0].top = 0;
    pxp->config.ol_param[0].stride = pxp->config.proc_data.drect.width;
    pxp->config.ol_param[0].pixel_fmt = PXP_PIX_FMT_RGB565;
    pxp->config.ol_param[0].paddr = (dma_addr_t)pxp->rgb_temp.paddr;
  }

  pxp->config.ol_param[0].global_alpha_enable = TRUE;
  pxp->config.ol_param[0].global_alpha = src->alpha;
  pxp->config.ol_param[0].local_alpha_enable = TRUE;
  pxp->config.ol_param[0].global_override = FALSE;
  pxp->config.ol_param[0].width = dst->crop.w;
  pxp->config.ol_param[0].height = dst->crop.h;
  pxp->config.ol_param[0].combine_enable = TRUE;

  GST_TRACE ("pxp overlay : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.ol_param[0].width, pxp->config.ol_param[0].height,
      pxp->config.ol_param[0].stride, pxp->config.ol_param[0].left,
      pxp->config.ol_param[0].top, pxp->config.ol_param[0].width,
      pxp->config.ol_param[0].height, pxp->config.ol_param[0].pixel_fmt);

  pxp->config.s0_param.width = dst->crop.w;
  pxp->config.s0_param.height = dst->crop.h;
  pxp->config.s0_param.stride = pxp->config.s0_param.width;
  pxp->config.s0_param.pixel_fmt = orig_dst_fmt;
  pxp->config.proc_data.srect.left = 0;
  pxp->config.proc_data.srect.top = 0;
  pxp->config.proc_data.srect.width = dst->crop.w;
  pxp->config.proc_data.srect.height = dst->crop.h;
  pxp->config.s0_param.paddr = (dma_addr_t)pxp->ov_temp.paddr;

  GST_TRACE ("pxp src : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.s0_param.width, pxp->config.s0_param.height,
      pxp->config.s0_param.stride,
      pxp->config.proc_data.srect.left, pxp->config.proc_data.srect.top,
      pxp->config.proc_data.srect.width, pxp->config.proc_data.srect.height,
      pxp->config.s0_param.pixel_fmt);

  pxp->config.out_param.pixel_fmt = orig_dst_fmt;
  pxp->config.out_param.stride = orig_dst_s;
  pxp->config.proc_data.drect.left = 0;
  pxp->config.proc_data.drect.top = 0;
  pxp->config.proc_data.drect.width = dst->crop.w;
  pxp->config.proc_data.drect.height = dst->crop.h;
  pxp->config.out_param.paddr = (dma_addr_t)dst->mem->paddr +
                        (dst->crop.y * dst->info.stride + dst->crop.x * BPP);
  pxp->config.out_param.width = pxp->config.proc_data.drect.width;
  pxp->config.out_param.height = pxp->config.proc_data.drect.height;

  GST_TRACE ("pxp dest : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.out_param.width, pxp->config.out_param.height,
      pxp->config.out_param.stride,
      pxp->config.proc_data.drect.left, pxp->config.proc_data.drect.top,
      pxp->config.proc_data.drect.width, pxp->config.proc_data.drect.height,
      pxp->config.out_param.pixel_fmt);

  return imx_pxp_do_channel(pxp);
}

static gboolean is_format_has_alpha(guint pxp_format) {
  return (pxp_format == PXP_PIX_FMT_BGRA32 || pxp_format == PXP_PIX_FMT_VUY444);
}

static gint imx_pxp_blend(Imx2DDevice *device, Imx2DFrame *dst, Imx2DFrame *src)
{
  if (!device || !device->priv || !dst || !src || !dst->mem || !src->mem)
    return -1;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);

  if (src->alpha < 0xFF
      || is_format_has_alpha(pxp->config.s0_param.pixel_fmt)) {
    return imx_pxp_overlay(device, dst, src);
  } else {
    return imx_pxp_blend_without_alpha(device, dst, src);
  }
}

static gint imx_pxp_blend_finish(Imx2DDevice *device)
{
  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
  pxp->first_frame_done = FALSE;
  return 0;
}

static gint imx_pxp_fill_color(Imx2DDevice *device, Imx2DFrame *dst,
                                guint RGBA8888)
{
  if (!device || !device->priv)
    return -1;
  guint bgcolor;

  gchar R,G,B,A,Y,U,V;
  gdouble y,u,v;

  R = RGBA8888 & 0x000000FF;
  G = (RGBA8888 & 0x0000FF00) >> 8;
  B = (RGBA8888 & 0x00FF0000) >> 16;
  A = (RGBA8888 & 0xFF000000) >> 24;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);

  if (dst->info.fmt == GST_VIDEO_FORMAT_BGRx
      || dst->info.fmt == GST_VIDEO_FORMAT_RGB16
      || dst->info.fmt == GST_VIDEO_FORMAT_BGRA
      || dst->info.fmt == GST_VIDEO_FORMAT_BGR) {
    bgcolor = (A << 24)| (R << 16) | (G << 8) | B;
  } else {
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

    bgcolor = (A << 24) | (Y << 16) | (U << 8) | V;
  }

  pxp->config.proc_data.bgcolor = bgcolor;
  pxp->background = RGBA8888;

  return 0;
}

static gint imx_pxp_set_rotate(Imx2DDevice *device, Imx2DRotationMode rot)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
  switch (rot) {
  case IMX_2D_ROTATION_0:      pxp->config.proc_data.rotate = 0;    break;
  case IMX_2D_ROTATION_90:     pxp->config.proc_data.rotate = 90;   break;
  case IMX_2D_ROTATION_180:    pxp->config.proc_data.rotate = 180;  break;
  case IMX_2D_ROTATION_270:    pxp->config.proc_data.rotate = 270;  break;
  case IMX_2D_ROTATION_HFLIP:  pxp->config.proc_data.hflip = 1;     break;
  case IMX_2D_ROTATION_VFLIP:  pxp->config.proc_data.vflip = 1;     break;
  default:                     pxp->config.proc_data.rotate = 0;    break;
  }

  return 0;
}

static gint imx_pxp_set_deinterlace(Imx2DDevice *device,
                                    Imx2DDeinterlaceMode mode)
{
  return 0;
}

static Imx2DRotationMode imx_pxp_get_rotate (Imx2DDevice* device)
{
  if (!device || !device->priv)
    return 0;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
  Imx2DRotationMode rot = IMX_2D_ROTATION_0;
  switch (pxp->config.proc_data.rotate) {
  case 0:    rot = IMX_2D_ROTATION_0;     break;
  case 90:   rot = IMX_2D_ROTATION_90;    break;
  case 180:  rot = IMX_2D_ROTATION_180;   break;
  case 270:  rot = IMX_2D_ROTATION_270;   break;
  default:   rot = IMX_2D_ROTATION_0;     break;
  }

  if (pxp->config.proc_data.hflip)
    rot = IMX_2D_ROTATION_HFLIP;
  else if (pxp->config.proc_data.vflip)
    rot = IMX_2D_ROTATION_VFLIP;

  return rot;
}

static Imx2DDeinterlaceMode imx_pxp_get_deinterlace (Imx2DDevice* device)
{
  return IMX_2D_DEINTERLACE_NONE;
}

static gint imx_pxp_get_capabilities (Imx2DDevice* device)
{
  void *pxp_handle = NULL;
  gint capabilities = 0;

  capabilities = IMX_2D_DEVICE_CAP_SCALE | IMX_2D_DEVICE_CAP_CSC \
                      | IMX_2D_DEVICE_CAP_ROTATE | IMX_2D_DEVICE_CAP_OVERLAY;
#ifdef ENABLE_PXP_ALPHA_OVERLAY
  capabilities |= IMX_2D_DEVICE_CAP_ALPHA;
#endif

  return capabilities;
}

static GList* imx_pxp_get_supported_in_fmts(Imx2DDevice* device)
{
  GList* list = NULL;
  const PxpFmtMap *map = pxp_in_fmts_map;

  while (map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN) {
    list = g_list_append(list, (gpointer)(map->gst_video_format));
    map++;
  }

  return list;
}

static GList* imx_pxp_get_supported_out_fmts(Imx2DDevice* device)
{
  GList* list = NULL;
  const PxpFmtMap *map = pxp_out_fmts_map;

  while (map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN) {
    list = g_list_append(list, (gpointer)(map->gst_video_format));
    map++;
  }

  return list;
}

Imx2DDevice * imx_pxp_create(Imx2DDeviceType  device_type)
{
  Imx2DDevice * device = g_slice_alloc(sizeof(Imx2DDevice));
  if (!device) {
    GST_ERROR("allocate device structure failed\n");
    return NULL;
  }

  device->device_type = device_type;
  device->priv = NULL;

  device->open                = imx_pxp_open;
  device->close               = imx_pxp_close;
  device->alloc_mem           = imx_pxp_alloc_mem;
  device->free_mem            = imx_pxp_free_mem;
  device->copy_mem            = imx_pxp_copy_mem;
  device->frame_copy          = imx_pxp_frame_copy;
  device->config_input        = imx_pxp_config_input;
  device->config_output       = imx_pxp_config_output;
  device->convert             = imx_pxp_convert;
  device->blend               = imx_pxp_blend;
  device->blend_finish        = imx_pxp_blend_finish;
  device->fill                = imx_pxp_fill_color;
  device->set_rotate          = imx_pxp_set_rotate;
  device->set_deinterlace     = imx_pxp_set_deinterlace;
  device->get_rotate          = imx_pxp_get_rotate;
  device->get_deinterlace     = imx_pxp_get_deinterlace;
  device->get_capabilities    = imx_pxp_get_capabilities;
  device->get_supported_in_fmts  = imx_pxp_get_supported_in_fmts;
  device->get_supported_out_fmts = imx_pxp_get_supported_out_fmts;

  return device;
}

gint imx_pxp_destroy(Imx2DDevice *device)
{
  if (!device)
    return -1;

  g_slice_free1(sizeof(Imx2DDevice), device);

  return 0;
}

gboolean imx_pxp_is_exist (void)
{
  return HAS_PXP();
}
