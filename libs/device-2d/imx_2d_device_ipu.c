/* GStreamer IMX IPU Device
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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/ipu.h>
#include "imx_2d_device.h"

#define IPU_DEVICE_NAME "/dev/mxc_ipu"

GST_DEBUG_CATEGORY_EXTERN (imx2ddevice_debug);
#define GST_CAT_DEFAULT imx2ddevice_debug

typedef struct _Imx2DDeviceIpu {
  gint ipu_fd;
  struct ipu_task task;
  gboolean deinterlace_enable;
  PhyMemBlock vdi;
  gboolean  new_input;
} Imx2DDeviceIpu;

typedef struct {
  GstVideoFormat gst_video_format;
  guint ipu_format;
  guint bpp;
} IpuFmtMap;

static IpuFmtMap ipu_fmts_map[] = {
    {GST_VIDEO_FORMAT_I420,   IPU_PIX_FMT_YUV420P,  12},
    {GST_VIDEO_FORMAT_NV12,   IPU_PIX_FMT_NV12,     12},
    {GST_VIDEO_FORMAT_YV12,   IPU_PIX_FMT_YVU420P,  12},
    {GST_VIDEO_FORMAT_UYVY,   IPU_PIX_FMT_UYVY,     16},
    {GST_VIDEO_FORMAT_RGB16,  IPU_PIX_FMT_RGB565,   16},
    {GST_VIDEO_FORMAT_RGBx,   IPU_PIX_FMT_RGB32,    32},
    {GST_VIDEO_FORMAT_Y42B,   IPU_PIX_FMT_YUV422P,  16},
    {GST_VIDEO_FORMAT_Y444,   IPU_PIX_FMT_YUV444P,  24},
    {GST_VIDEO_FORMAT_v308,   IPU_PIX_FMT_YUV444,   24},
    {GST_VIDEO_FORMAT_BGR,    IPU_PIX_FMT_BGR24,    24},
    {GST_VIDEO_FORMAT_RGB,    IPU_PIX_FMT_RGB24,    24},
    {GST_VIDEO_FORMAT_BGRx,   IPU_PIX_FMT_BGR32,    32},
    {GST_VIDEO_FORMAT_BGRA,   IPU_PIX_FMT_BGRA32,   32},
    {GST_VIDEO_FORMAT_RGBA,   IPU_PIX_FMT_RGBA32,   32},
    {GST_VIDEO_FORMAT_ABGR,   IPU_PIX_FMT_ABGR32,   32},

/* There is no corresponding GST Video format for those IPU formats
    {GST_VIDEO_FORMAT_,   IPU_PIX_FMT_VYU444,   32},
    {GST_VIDEO_FORMAT_,   IPU_PIX_FMT_YUYV,     32},
    {GST_VIDEO_FORMAT_,   IPU_PIX_FMT_YUV420P2, 32},
    {GST_VIDEO_FORMAT_,   IPU_PIX_FMT_YVU422P,  32},
*/

    {GST_VIDEO_FORMAT_UNKNOWN, -1,                  0}
};

static const IpuFmtMap* imx_ipu_get_format(GstVideoFormat format)
{
  const IpuFmtMap *map = ipu_fmts_map;
  while(map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN) {
    if (map->gst_video_format == format)
      return map;
    map++;
  };

  GST_ERROR ("ipu : format (%s) is not supported.",
              gst_video_format_to_string(format));

  return NULL;
}

static const guint imx_ipu_get_bpp(guint ipu_format)
{
  const IpuFmtMap *map = ipu_fmts_map;
  while(map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN) {
    if (map->ipu_format == ipu_format)
      return map->bpp;
    map++;
  };

  GST_ERROR ("ipu : format (%d) is not supported.", ipu_format);
  return 8;
}

static gint imx_ipu_open(Imx2DDevice *device)
{
  if (!device)
    return -1;

  gint fd = open(IPU_DEVICE_NAME, O_RDWR, 0);
  if (fd < 0) {
    GST_ERROR("could not open %s: %s", IPU_DEVICE_NAME, strerror(errno));
    return -1;
  }

  Imx2DDeviceIpu *ipu = g_slice_alloc(sizeof(Imx2DDeviceIpu));
  if (!ipu) {
    GST_ERROR("allocate ipu structure failed\n");
    close(fd);
    return -1;
  }

  memset(ipu, 0, sizeof(Imx2DDeviceIpu));
  ipu->ipu_fd = fd;
  ipu->task.priority = 0;
  ipu->task.timeout = 1000;

  device->priv = (gpointer)ipu;

  return 0;
}

static gint imx_ipu_close(Imx2DDevice *device)
{
  if (!device)
    return -1;

  if (device) {
    Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);
    if (ipu) {
      if (ipu->vdi.vaddr) {
        dma_addr_t mem = (dma_addr_t)(ipu->vdi.paddr);
        munmap(ipu->vdi.vaddr, ipu->vdi.size);
        if (ioctl(ipu->ipu_fd, IPU_FREE, &mem) < 0) {
          GST_ERROR("IPU could not free vdibuf memory at 0x%x: %s", mem,
              strerror(errno));
        }
        ipu->vdi.vaddr = NULL;
        ipu->vdi.paddr = NULL;
        ipu->vdi.size = 0;
      }
      close(ipu->ipu_fd);
      g_slice_free1(sizeof(Imx2DDeviceIpu), ipu);
      device->priv = NULL;
    }
  }

  return 0;
}

static gint
imx_ipu_alloc_mem(Imx2DDevice *device, PhyMemBlock *memblk)
{
  dma_addr_t mem;

  if (!device || !device->priv || !memblk)
    return -1;

  memblk->size = PAGE_ALIGN(memblk->size);
  Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);
  mem = (dma_addr_t)(memblk->size);
  if (ioctl(ipu->ipu_fd, IPU_ALLOC, &mem) < 0) {
    GST_ERROR("IPU allocate %u bytes memory failed: %s",
        memblk->size, strerror(errno));
    return -1;
  }

  memblk->paddr = (guchar *)mem;
  memblk->vaddr = mmap(0, memblk->size, PROT_READ|PROT_WRITE, MAP_SHARED,
                       ipu->ipu_fd, (dma_addr_t)(memblk->paddr));
  GST_DEBUG("IPU allocated memory (%p)", memblk->paddr);

  return 0;
}

static gint imx_ipu_free_mem(Imx2DDevice *device, PhyMemBlock *memblk)
{
  dma_addr_t mem;

  if (!device || !device->priv || !memblk)
    return -1;

  Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);

  GST_DEBUG("IPU free memory (%p)", memblk->paddr);
  mem = (dma_addr_t)(memblk->paddr);
  munmap(memblk->vaddr, memblk->size);

  if (ioctl(ipu->ipu_fd, IPU_FREE, &mem) < 0) {
    GST_ERROR("IPU could not free memory at 0x%x: %s", mem, strerror(errno));
    return -1;
  }

  memblk->paddr = NULL;
  memblk->vaddr = NULL;
  memblk->size = 0;

  return 0;
}

static gint imx_ipu_copy_mem(Imx2DDevice* device, PhyMemBlock *dst_mem,
                             PhyMemBlock *src_mem, guint offset, guint size)
{
  if (!device || !device->priv || !src_mem || !dst_mem)
    return -1;

  if (size > src_mem->size - offset)
    size = src_mem->size - offset;
  memcpy(dst_mem->vaddr, src_mem->vaddr+offset, size);

  GST_DEBUG ("IPU copy from vaddr (%p), paddr (%p), size (%d) to "
      "vaddr (%p), paddr (%p), size (%d)",
      src_mem->vaddr, src_mem->paddr, src_mem->size,
      dst_mem->vaddr, dst_mem->paddr, dst_mem->size);

  return 0;
}

static gint imx_ipu_frame_copy(Imx2DDevice *device,
                               PhyMemBlock *from, PhyMemBlock *to)
{
  if (!device || !device->priv || !from || !to)
    return -1;

  memcpy(to->vaddr, from->vaddr, from->size);
  GST_LOG("IPU frame memory (%p)->(%p)", from->paddr, to->paddr);

  return 0;
}

static gint imx_ipu_config_input(Imx2DDevice *device, Imx2DVideoInfo* in_info)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);

  const IpuFmtMap *from_map = imx_ipu_get_format(in_info->fmt);
  if (!from_map)
    return -1;

  ipu->task.input.width = in_info->w;
  ipu->task.input.height = in_info->h;
  ipu->task.input.format = from_map->ipu_format;
  ipu->task.input.crop.pos.x = 0;
  ipu->task.input.crop.pos.y = 0;
  ipu->task.input.crop.w = in_info->w;
  ipu->task.input.crop.h = in_info->h;
  ipu->task.input.deinterlace.enable = ipu->deinterlace_enable;
  ipu->new_input = TRUE;

  GST_TRACE("input format = %s", gst_video_format_to_string(in_info->fmt));
  return 0;
}

static gint imx_ipu_config_output(Imx2DDevice *device, Imx2DVideoInfo* out_info)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);

  const IpuFmtMap *to_map = imx_ipu_get_format(out_info->fmt);
  if (!to_map)
    return -1;

  ipu->task.output.width = out_info->w;
  ipu->task.output.height = out_info->h;
  ipu->task.output.format = to_map->ipu_format;
  ipu->task.output.crop.pos.x = 0;
  ipu->task.output.crop.pos.y = 0;
  ipu->task.output.crop.w = out_info->w;
  ipu->task.output.crop.h = out_info->h;
  GST_TRACE("output format = %s", gst_video_format_to_string(out_info->fmt));

  return 0;
}

static gint imx_ipu_do_convert(Imx2DDevice *device,
                                PhyMemBlock *from, PhyMemBlock *to,
                                Imx2DInterlaceType interlace_type,
                                Imx2DCrop incrop, Imx2DCrop outcrop)
{
  if (!device || !device->priv || !from || !to)
    return -1;

  Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);

  // Set input
  ipu->task.input.paddr = (dma_addr_t)(from->paddr);
  ipu->task.input.crop.pos.x = GST_ROUND_UP_8(incrop.x);
  ipu->task.input.crop.pos.y = GST_ROUND_UP_8(incrop.y);
  ipu->task.input.crop.w = GST_ROUND_DOWN_8((MIN(incrop.w,
      (ipu->task.input.width - ipu->task.input.crop.pos.x))));
  ipu->task.input.crop.h = GST_ROUND_DOWN_8(MIN(incrop.h,
      (ipu->task.input.height - ipu->task.input.crop.pos.y)));

  if (ipu->deinterlace_enable) {
    switch (interlace_type) {
    case IMX_2D_INTERLACE_INTERLEAVED:
      ipu->task.input.deinterlace.enable = TRUE;
      break;
    case IMX_2D_INTERLACE_FIELDS:
      GST_FIXME("2-fields deinterlacing not supported yet");
      ipu->task.input.deinterlace.enable = FALSE;
      break;
    default:
      ipu->task.input.deinterlace.enable = FALSE;
      break;
    }
  }

  GST_TRACE ("ipu input : %dx%d(%d,%d->%d,%d), format=%d, deinterlace=%s"
      " deinterlace-mode=%d", ipu->task.input.width, ipu->task.input.height,
      ipu->task.input.crop.pos.x, ipu->task.input.crop.pos.y,
      ipu->task.input.crop.w, ipu->task.input.crop.h,
      ipu->task.input.format,
      (ipu->task.input.deinterlace.enable ? "Yes" : "No"),
          ipu->task.input.deinterlace.motion);

  // Set output
  ipu->task.output.paddr = (dma_addr_t)(to->paddr);
  ipu->task.output.crop.pos.x = GST_ROUND_DOWN_8(outcrop.x);
  ipu->task.output.crop.pos.y = GST_ROUND_DOWN_8(outcrop.y);
  ipu->task.output.crop.w = GST_ROUND_UP_8(MIN(outcrop.w,
      (ipu->task.output.width - ipu->task.output.crop.pos.x)));
  ipu->task.output.crop.h = GST_ROUND_UP_8(MIN(outcrop.h,
      (ipu->task.output.height - ipu->task.output.crop.pos.y)));

  GST_TRACE ("ipu output : %dx%d(%d,%d->%d,%d), format=%d",
      ipu->task.output.width, ipu->task.output.height,
      ipu->task.output.crop.pos.x, ipu->task.output.crop.pos.y,
      ipu->task.output.crop.w, ipu->task.output.crop.h,
      ipu->task.output.format);

  // Check parameters
  gint cnt = 100;
  gboolean check_end = FALSE;
  gint ret = IPU_CHECK_ERR_INPUT_CROP;
  while(ret != IPU_CHECK_OK && ret > IPU_CHECK_ERR_MIN) {
    ret = ioctl((ipu->ipu_fd), IPU_CHECK_TASK, &(ipu->task));
    GST_TRACE ("IPU CHECK TASK ret=%d", ret);

    switch(ret) {
      case IPU_CHECK_OK:
        check_end = TRUE;
        break;
      case IPU_CHECK_ERR_SPLIT_INPUTW_OVER:
        ipu->task.input.crop.w -= 8;
        cnt--;
        break;
      case IPU_CHECK_ERR_SPLIT_INPUTH_OVER:
        ipu->task.input.crop.h -= 8;
        cnt--;
        break;
      case IPU_CHECK_ERR_SPLIT_OUTPUTW_OVER:
        ipu->task.output.crop.w -= 8;
        cnt--;
        break;
      case IPU_CHECK_ERR_SPLIT_OUTPUTH_OVER:
        ipu->task.output.crop.h -= 8;
        cnt--;
        break;
      case IPU_CHECK_ERR_SPLIT_WITH_ROT:
        if (ipu->task.output.rotate != IPU_ROTATE_NONE) {
          g_print("out of size range for rotation! (%d,%d):%d\n",
              ipu->task.output.width, ipu->task.output.height,
              ipu->task.output.rotate);
        }
        check_end = TRUE;
        break;
      case IPU_CHECK_ERR_PROC_NO_NEED:
        GST_INFO ("shouldn't be here, but copy frame directly anyway");
        imx_ipu_frame_copy(device, from, to);
        return 0;
      default:
        check_end = TRUE;
        break;
    }

    if (check_end || cnt <= 0)
      break;
  }

  if (ipu->task.input.deinterlace.enable &&
      ipu->task.input.deinterlace.motion != HIGH_MOTION && ipu->new_input) {
    if (ipu->vdi.vaddr) {
      dma_addr_t mem = (dma_addr_t)(ipu->vdi.paddr);
      munmap(ipu->vdi.vaddr, ipu->vdi.size);
      if (ioctl(ipu->ipu_fd, IPU_FREE, &mem) < 0) {
        GST_ERROR("IPU could not free vdibuf memory at 0x%x: %s", mem,
            strerror(errno));
      }
      ipu->vdi.vaddr = NULL;
      ipu->vdi.paddr = NULL;
      ipu->vdi.size = 0;
    }

    ipu->vdi.size = ipu->task.input.width * ipu->task.input.height *
                    (imx_ipu_get_bpp(ipu->task.input.format) / 8);
    ipu->task.input.paddr_n = ipu->vdi.size;
    if (ioctl(ipu->ipu_fd, IPU_ALLOC, &ipu->task.input.paddr_n) < 0) {
      GST_ERROR ("ioctl IPU_ALLOC failed");
      return -1;
    }

    ipu->vdi.vaddr = mmap(0, ipu->vdi.size, PROT_READ | PROT_WRITE, MAP_SHARED,
                  ipu->ipu_fd, ipu->task.input.paddr_n);
    if (!ipu->vdi.vaddr) {
      GST_ERROR ("mmap vdibuf failed");
      return -1;
    }
    ipu->vdi.paddr = (guchar *)(ipu->task.input.paddr_n);

    memcpy(ipu->vdi.vaddr, from->vaddr, ipu->vdi.size);
  }

  // Final conversion
  if (ioctl(ipu->ipu_fd, IPU_QUEUE_TASK, &(ipu->task)) < 0) {
    GST_ERROR("queuing IPU task failed: %s", strerror(errno));
    return -1;
  }

  if (ipu->task.input.deinterlace.enable &&
      ipu->task.input.deinterlace.motion != HIGH_MOTION) {
    memcpy(ipu->vdi.vaddr, from->vaddr, ipu->vdi.size);
  }
  ipu->new_input = FALSE;

  return 0;
}

static gint imx_ipu_set_rotate(Imx2DDevice *device, Imx2DRotationMode rot)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);
  gint ipu_rotate = IPU_ROTATE_NONE;
  switch (rot) {
  case IMX_2D_ROTATION_0:      ipu_rotate = IPU_ROTATE_NONE;       break;
  case IMX_2D_ROTATION_90:     ipu_rotate = IPU_ROTATE_90_RIGHT;   break;
  case IMX_2D_ROTATION_180:    ipu_rotate = IPU_ROTATE_180;        break;
  case IMX_2D_ROTATION_270:    ipu_rotate = IPU_ROTATE_90_LEFT;    break;
  case IMX_2D_ROTATION_HFLIP:  ipu_rotate = IPU_ROTATE_HORIZ_FLIP; break;
  case IMX_2D_ROTATION_VFLIP:  ipu_rotate = IPU_ROTATE_VERT_FLIP;  break;
  default:                     ipu_rotate = IPU_ROTATE_NONE;       break;
  }
  ipu->task.output.rotate = ipu_rotate;

  return 0;
}

static gint imx_ipu_set_deinterlace (Imx2DDevice *device,
                                     Imx2DDeinterlaceMode deint_mode)
{
  if (!device || !device->priv)
    return -1;

  Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);
  switch (deint_mode) {
  case IMX_2D_DEINTERLACE_NONE:
    ipu->deinterlace_enable = FALSE;
    break;
  case IMX_2D_DEINTERLACE_LOW_MOTION:
    ipu->deinterlace_enable = TRUE;
    ipu->task.input.deinterlace.motion = LOW_MOTION;
    break;
  case IMX_2D_DEINTERLACE_MID_MOTION:
    ipu->deinterlace_enable = TRUE;
    ipu->task.input.deinterlace.motion = MED_MOTION;
    break;
  case IMX_2D_DEINTERLACE_HIGH_MOTION:
    ipu->deinterlace_enable = TRUE;
    ipu->task.input.deinterlace.motion = HIGH_MOTION;
    break;
  default:
    ipu->deinterlace_enable = FALSE;
    break;
  }

  return 0;
}

static Imx2DRotationMode imx_ipu_get_rotate (Imx2DDevice* device)
{
  if (!device || !device->priv)
    return 0;

  Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);
  Imx2DRotationMode rot = IMX_2D_ROTATION_0;
  switch (ipu->task.output.rotate) {
  case IPU_ROTATE_NONE:       rot = IMX_2D_ROTATION_0;     break;
  case IPU_ROTATE_90_RIGHT:   rot = IMX_2D_ROTATION_90;    break;
  case IPU_ROTATE_180:        rot = IMX_2D_ROTATION_180;   break;
  case IPU_ROTATE_90_LEFT:    rot = IMX_2D_ROTATION_270;   break;
  case IPU_ROTATE_HORIZ_FLIP: rot = IMX_2D_ROTATION_HFLIP; break;
  case IPU_ROTATE_VERT_FLIP:  rot = IMX_2D_ROTATION_VFLIP; break;
  default:                    rot = IMX_2D_ROTATION_0;     break;
  }

  return rot;
}

static Imx2DDeinterlaceMode imx_ipu_get_deinterlace (Imx2DDevice* device)
{
  if (!device || !device->priv)
    return 0;

  Imx2DDeviceIpu *ipu = (Imx2DDeviceIpu *) (device->priv);
  Imx2DDeinterlaceMode deint_mode = IMX_2D_DEINTERLACE_NONE;
  if (ipu->deinterlace_enable) {
    switch (ipu->task.input.deinterlace.motion) {
    case LOW_MOTION:  deint_mode = IMX_2D_DEINTERLACE_LOW_MOTION;  break;
    case MED_MOTION:  deint_mode = IMX_2D_DEINTERLACE_MID_MOTION;  break;
    case HIGH_MOTION: deint_mode = IMX_2D_DEINTERLACE_HIGH_MOTION; break;
    default:          deint_mode = IMX_2D_DEINTERLACE_NONE;        break;
    }
  }

  return deint_mode;
}

static gint imx_ipu_get_capabilities(Imx2DDevice* device)
{
  return IMX_2D_DEVICE_CAP_CSC | IMX_2D_DEVICE_CAP_DEINTERLACE
          | IMX_2D_DEVICE_CAP_ROTATE | IMX_2D_DEVICE_CAP_SCALE
          | IMX_2D_DEVICE_CAP_ALPHA | IMX_2D_DEVICE_CAP_OVERLAY;
}

static GList* imx_ipu_get_supported_in_fmts(Imx2DDevice* device)
{
  GList* list = NULL;
  const IpuFmtMap *map = ipu_fmts_map;
  while (map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN) {
    list = g_list_append(list, (gpointer)(map->gst_video_format));
    map++;
  }

  return list;
}

static GList* imx_ipu_get_supported_out_fmts(Imx2DDevice* device)
{
  return imx_ipu_get_supported_in_fmts(device);
}

Imx2DDevice * imx_ipu_create(Imx2DDeviceType  device_type)
{
  Imx2DDevice * device = g_slice_alloc(sizeof(Imx2DDevice));
  if (!device) {
    GST_ERROR("allocate device structure failed\n");
    return NULL;
  }

  device->device_type = device_type;
  device->priv = NULL;

  device->open                = imx_ipu_open;
  device->close               = imx_ipu_close;
  device->alloc_mem           = imx_ipu_alloc_mem;
  device->free_mem            = imx_ipu_free_mem;
  device->copy_mem            = imx_ipu_copy_mem;
  device->frame_copy          = imx_ipu_frame_copy;
  device->config_input        = imx_ipu_config_input;
  device->config_output       = imx_ipu_config_output;
  device->do_convert          = imx_ipu_do_convert;
  device->set_rotate          = imx_ipu_set_rotate;
  device->set_deinterlace     = imx_ipu_set_deinterlace;
  device->get_rotate          = imx_ipu_get_rotate;
  device->get_deinterlace     = imx_ipu_get_deinterlace;
  device->get_capabilities    = imx_ipu_get_capabilities;
  device->get_supported_in_fmts  = imx_ipu_get_supported_in_fmts;
  device->get_supported_out_fmts = imx_ipu_get_supported_out_fmts;

  return device;
}

gint imx_ipu_destroy(Imx2DDevice *device)
{
  if (!device)
    return -1;

  g_slice_free1(sizeof(Imx2DDevice), device);

  return 0;
}

gboolean imx_ipu_is_exist (void)
{
  return HAS_IPU();
}
