/* GStreamer IMX PXP Device
 * Copyright (c) 2015, Freescale Semiconductor, Inc. All rights reserved.
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

#define PXP_WAIT_COMPLETE_TIME    3

typedef struct _Imx2DDevicePxp {
  gint capabilities;
  struct pxp_config_data config;
  pxp_chan_handle_t pxp_chan;
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
    {GST_VIDEO_FORMAT_GRAY8,  PXP_PIX_FMT_GREY,     8},
    {GST_VIDEO_FORMAT_AYUV,   PXP_PIX_FMT_VUY444,   32},
    {GST_VIDEO_FORMAT_Y42B,   PXP_PIX_FMT_YUV422P,  16},
    {GST_VIDEO_FORMAT_UYVY,   PXP_PIX_FMT_UYVY,     16},
    {GST_VIDEO_FORMAT_YUY2,   PXP_PIX_FMT_YUYV,     16},
    {GST_VIDEO_FORMAT_YVYU,   PXP_PIX_FMT_YVYU,     16},
    {GST_VIDEO_FORMAT_NV12,   PXP_PIX_FMT_NV12,     12},
    {GST_VIDEO_FORMAT_NV21,   PXP_PIX_FMT_NV21,     12},
    {GST_VIDEO_FORMAT_NV16,   PXP_PIX_FMT_NV16,     16},

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
// not support    {GST_VIDEO_FORMAT_RGB15,  PXP_PIX_FMT_RGB555,   16},

    {GST_VIDEO_FORMAT_GRAY8,  PXP_PIX_FMT_GREY,     8},
    {GST_VIDEO_FORMAT_UYVY,   PXP_PIX_FMT_UYVY,     16},
    {GST_VIDEO_FORMAT_NV12,   PXP_PIX_FMT_NV12,     12},
    {GST_VIDEO_FORMAT_NV21,   PXP_PIX_FMT_NV21,     12},
    {GST_VIDEO_FORMAT_NV16,   PXP_PIX_FMT_NV16,     16},

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

  GST_ERROR ("pxp : format (%x) is not supported.",
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

/* not necessary, pxp already memset to 0
  pxp->config.proc_data.scaling = 0;
  pxp->config.proc_data.bgcolor = 0;
  pxp->config.proc_data.overlay_state = 0;
  pxp->config.proc_data.lut_transform = PXP_LUT_NONE;

  //Initialize OL parameters, No overlay will be used for PxP operation
  gint i;
  for (i=0; i < 8; i++) {
    pxp->config.ol_param[i].combine_enable = false;
    pxp->config.ol_param[i].width = 0;
    pxp->config.ol_param[i].height = 0;
    pxp->config.ol_param[i].pixel_fmt = PXP_PIX_FMT_RGB565;
    pxp->config.ol_param[i].color_key_enable = false;
    pxp->config.ol_param[i].color_key = -1;
    pxp->config.ol_param[i].global_alpha_enable = false;
    pxp->config.ol_param[i].global_alpha = 0;
    pxp->config.ol_param[i].local_alpha_enable = false;
  }
*/

  device->priv = (gpointer)pxp;

  GST_DEBUG("requested pxp chan handle %d", pxp->pxp_chan.handle);
  return 0;
}

static gint imx_pxp_close(Imx2DDevice *device)
{
  if (!device)
    return -1;

  if (device) {
    Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);
    if (pxp) {
      pxp_release_channel(&pxp->pxp_chan);
      pxp_uninit();
      g_slice_free1(sizeof(Imx2DDevicePxp), pxp);
    }
    device->priv = NULL;
  }
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

  GST_DEBUG("PXP free memory (%p)", memblk->paddr);
  gint ret = pxp_put_mem ((struct pxp_mem_desc*)(memblk->user_data));
  memblk->user_data = NULL;
  memblk->vaddr = NULL;
  memblk->paddr = NULL;
  memblk->size = 0;

  return ret;
}

static gint imx_pxp_copy_mem(Imx2DDevice* device, PhyMemBlock *dst_mem,
                             PhyMemBlock *src_mem, guint offset, guint size)
{
  if (!device || !device->priv || !src_mem || !dst_mem)
    return -1;

  if (size > src_mem->size - offset)
    size = src_mem->size - offset;
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

  memcpy(to->vaddr, from->vaddr, from->size);
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
  //if (pxp->config.proc_data.rotate % 180)
  //  pxp->config.out_param.stride = out_info->h;

  GST_TRACE("output format = %s", gst_video_format_to_string(out_info->fmt));

  return 0;
}

static gint imx_pxp_do_convert(Imx2DDevice *device,
                                PhyMemBlock *from, PhyMemBlock *to,
                                Imx2DInterlaceType interlace_type,
                                Imx2DCrop incrop, Imx2DCrop outcrop)
{
  gint ret = 0;

  if (!device || !device->priv || !from || !to)
    return -1;

  Imx2DDevicePxp *pxp = (Imx2DDevicePxp *) (device->priv);

  // Set input crop
  pxp->config.proc_data.srect.left = incrop.x;
  pxp->config.proc_data.srect.top = incrop.y;
  pxp->config.proc_data.srect.width =
      MIN(incrop.w, pxp->config.proc_data.srect.width);
  pxp->config.proc_data.srect.height =
      MIN(incrop.h, pxp->config.proc_data.srect.height);

  pxp->config.s0_param.paddr = (dma_addr_t)from->paddr;

  GST_TRACE ("pxp src : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.s0_param.width, pxp->config.s0_param.height,
      pxp->config.s0_param.stride,
      pxp->config.proc_data.srect.left, pxp->config.proc_data.srect.top,
      pxp->config.proc_data.srect.width, pxp->config.proc_data.srect.height,
      pxp->config.s0_param.pixel_fmt);

  // Set output crop
  pxp->config.proc_data.drect.left = outcrop.x;
  pxp->config.proc_data.drect.top = outcrop.y;
  pxp->config.proc_data.drect.width =
      MIN(outcrop.w, pxp->config.proc_data.drect.width);
  pxp->config.proc_data.drect.height =
      MIN(outcrop.h, pxp->config.proc_data.drect.height);

  pxp->config.out_param.paddr = (dma_addr_t)to->paddr;

  GST_TRACE ("pxp dest : %dx%d,%d(%d,%d-%d,%d), format=%x",
      pxp->config.out_param.width, pxp->config.out_param.height,
      pxp->config.out_param.stride,
      pxp->config.proc_data.drect.left, pxp->config.proc_data.drect.top,
      pxp->config.proc_data.drect.width, pxp->config.proc_data.drect.height,
      pxp->config.out_param.pixel_fmt);

  // Final conversion
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

  capabilities = IMX_2D_DEVICE_CAP_SCALE|IMX_2D_DEVICE_CAP_CSC \
                      |IMX_2D_DEVICE_CAP_ROTATE | IMX_2D_DEVICE_CAP_ALPHA
                      |IMX_2D_DEVICE_CAP_OVERLAY;
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
  device->do_convert          = imx_pxp_do_convert;
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
