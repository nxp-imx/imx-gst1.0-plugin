/* GStreamer IMX IPU Video Processing
 * Copyright (c) 2014, Freescale Semiconductor, Inc. All rights reserved.
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
#include "gstallocatorphymem.h"
#include "videoprocessdevice.h"

#define IPU_DEVICE_NAME "/dev/mxc_ipu"

GST_DEBUG_CATEGORY_EXTERN (imxvideoconvert_debug);
#define GST_CAT_DEFAULT imxvideoconvert_debug

typedef struct _ImxVpDeviceIpu {
  gint ipu_fd;
  struct ipu_task task;
  gboolean deinterlace;
  GstVideoInterlaceMode interlace_mode;
  PhyMemBlock vdi;
  gboolean  first_frame;
  GstVideoAlignment valign;
} ImxVpDeviceIpu;

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

static gint imx_ipu_open(ImxVideoProcessDevice *device)
{
  if (!device)
    return -1;

  gint fd = open(IPU_DEVICE_NAME, O_RDWR, 0);
  if (fd < 0) {
    GST_ERROR("could not open %s: %s", IPU_DEVICE_NAME, strerror(errno));
    return -1;
  }

  ImxVpDeviceIpu *ipu = g_slice_alloc(sizeof(ImxVpDeviceIpu));
  if (!ipu) {
    GST_ERROR("allocate ipu structure failed\n");
    close(fd);
    return -1;
  }

  memset(ipu, 0, sizeof(ImxVpDeviceIpu));
  ipu->ipu_fd = fd;
  ipu->task.priority = 0;
  ipu->task.timeout = 1000;

  device->priv = (gpointer)ipu;

  return 0;
}

static gint imx_ipu_close(ImxVideoProcessDevice *device)
{
  if (!device)
    return -1;

  if (device) {
    ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);
    if (ipu) {
      close(ipu->ipu_fd);
      g_slice_free1(sizeof(ImxVpDeviceIpu), ipu);
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
      device->priv = NULL;
    }
  }

  return 0;
}

static gint
imx_ipu_alloc_mem(ImxVideoProcessDevice *device, PhyMemBlock *memblk)
{
  dma_addr_t mem;

  if (!device || !device->priv || !memblk)
    return -1;

  memblk->size = PAGE_ALIGN(memblk->size);
  ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);
  mem = (dma_addr_t)(memblk->size);
  if (ioctl(ipu->ipu_fd, IPU_ALLOC, &mem) < 0) {
    GST_ERROR("IPU allocate %u bytes memory failed: %s",
        memblk->size, strerror(errno));
    return -1;
  }

  memblk->paddr = (guint8 *)mem;
  memblk->vaddr = mmap(0, memblk->size, PROT_READ|PROT_WRITE, MAP_SHARED,
                       ipu->ipu_fd, (dma_addr_t)(memblk->paddr));

  return 0;
}

static gint imx_ipu_free_mem(ImxVideoProcessDevice *device, PhyMemBlock *memblk)
{
  dma_addr_t mem;

  if (!device || !device->priv || !memblk)
    return -1;

  ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);

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

static gint imx_ipu_frame_copy(ImxVideoProcessDevice *device,
                               PhyMemBlock *from, PhyMemBlock *to)
{
  if (!device || !device->priv || !from || !to)
    return -1;

  memcpy(to->vaddr, from->vaddr, from->size);

  return 0;
}

static gint imx_ipu_config_input(ImxVideoProcessDevice *device,
                                  GstVideoInfo* in_info,
                                  GstVideoAlignment *valign)
{
  if (!device || !device->priv)
    return -1;

  ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);
  const IpuFmtMap *from_map = imx_ipu_get_format(GST_VIDEO_INFO_FORMAT(in_info));
  if (!from_map)
    return -1;

  if (valign)
    memcpy(&ipu->valign, valign, sizeof(GstVideoAlignment));

  ipu->task.input.width =
      ipu->valign.padding_left + in_info->width + ipu->valign.padding_right;
  ipu->task.input.height =
      ipu->valign.padding_top + in_info->height + ipu->valign.padding_bottom;
  ipu->task.input.format = from_map->ipu_format;
  ipu->task.input.crop.pos.x = ipu->valign.padding_left;
  ipu->task.input.crop.pos.y = ipu->valign.padding_top;
  ipu->task.input.crop.w = in_info->width;
  ipu->task.input.crop.h = in_info->height;

  ipu->first_frame = FALSE;
  GST_TRACE("input format = %s",
      gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(in_info)));

  ipu->task.input.deinterlace.enable = FALSE;
  if (ipu->deinterlace) {
    ipu->interlace_mode = in_info->interlace_mode;
    switch (in_info->interlace_mode) {
      case GST_VIDEO_INTERLACE_MODE_INTERLEAVED:
        GST_TRACE("input stream is interleaved");
        ipu->task.input.deinterlace.enable = TRUE;
        break;
      case GST_VIDEO_INTERLACE_MODE_MIXED:
        ipu->task.input.deinterlace.enable = TRUE;
        GST_TRACE("input stream is mixed");
        break;
      case GST_VIDEO_INTERLACE_MODE_PROGRESSIVE:
        GST_TRACE("input stream is progressive");
        break;
      case GST_VIDEO_INTERLACE_MODE_FIELDS:
        GST_FIXME("2-fields deinterlacing not supported yet");
        break;
      default:
        break;
    }
  }

  if (ipu->task.input.deinterlace.enable &&
      ipu->task.input.deinterlace.motion != HIGH_MOTION) {
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

    ipu->vdi.size =
        ipu->task.input.width * ipu->task.input.height * from_map->bpp / 8;
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
    ipu->vdi.paddr = (guint8 *)(ipu->task.input.paddr_n);
  }

  return 0;
}

static gint imx_ipu_config_output(ImxVideoProcessDevice *device,
                                  GstVideoInfo* out_info)
{
  if (!device || !device->priv)
    return -1;

  ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);

  const IpuFmtMap *to_map = imx_ipu_get_format(GST_VIDEO_INFO_FORMAT(out_info));
  if (!to_map)
    return -1;

  ipu->task.output.width = ALIGNTO (out_info->width, ALIGNMENT);
  ipu->task.output.height = out_info->height;
  ipu->task.output.format = to_map->ipu_format;
  ipu->task.output.crop.pos.x = 0;
  ipu->task.output.crop.pos.y = 0;
  ipu->task.output.crop.w = out_info->width;
  ipu->task.output.crop.h = out_info->height;
  GST_TRACE("output format = %s",
      gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(out_info)));

  return 0;
}

static gint imx_ipu_do_convert(ImxVideoProcessDevice *device,
                                GstBuffer *from, GstBuffer *to)
{
  if (!device || !device->priv || !from || !to)
    return -1;

  ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);
  GstVideoCropMeta *in_crop = NULL, *out_crop = NULL;

  // Set input
  PhyMemBlock *from_memblk = gst_buffer_query_phymem_block (from);
  if (!from_memblk)
    return -1;
  ipu->task.input.paddr = (dma_addr_t)(from_memblk->paddr);

  in_crop = gst_buffer_get_video_crop_meta(from);
  if (in_crop != NULL) {
    GST_LOG ("input crop meta: (%d, %d, %d, %d).", in_crop->x, in_crop->y,
        in_crop->width, in_crop->height);
    if ((in_crop->x >= ipu->task.input.width)
        || (in_crop->y >= ipu->task.input.height))
      return -1;

    ipu->task.input.crop.pos.x =
        GST_ROUND_UP_8(ipu->valign.padding_left + in_crop->x);
    ipu->task.input.crop.pos.y =
        GST_ROUND_UP_8(ipu->valign.padding_top + in_crop->y);
    ipu->task.input.crop.w = MIN(in_crop->width,
        (ipu->task.input.width - ipu->task.input.crop.pos.x));
    ipu->task.input.crop.h = MIN(in_crop->height,
        (ipu->task.input.height - ipu->task.input.crop.pos.y));
  }

  if (ipu->deinterlace) {
    if (ipu->interlace_mode == GST_VIDEO_INTERLACE_MODE_MIXED) {
      GstVideoMeta *video_meta = gst_buffer_get_video_meta(from);
      if (video_meta != NULL) {
        if (video_meta->flags & GST_VIDEO_FRAME_FLAG_INTERLACED) {
          GST_TRACE("frame has video metadata and INTERLACED flag");
          ipu->task.input.deinterlace.enable = TRUE;
        } else {
          GST_TRACE("frame has video metadata but no INTERLACED flag");
          ipu->task.input.deinterlace.enable = FALSE;
        }
      } else {
        GST_TRACE("frame has no video metadata");
        ipu->task.input.deinterlace.enable = FALSE;
      }
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
  PhyMemBlock *to_memblk = gst_buffer_query_phymem_block (to);
  if (!to_memblk)
    return -1;
  ipu->task.output.paddr = (dma_addr_t)(to_memblk->paddr);

  out_crop = gst_buffer_get_video_crop_meta(to);
  if (out_crop != NULL) {
    GST_LOG ("input crop meta: (%d, %d, %d, %d).", out_crop->x, out_crop->y,
        out_crop->width, out_crop->height);
    if ((out_crop->x >= ipu->task.output.width)
        || (out_crop->y >= ipu->task.output.height))
      return -1;

    ipu->task.output.crop.pos.x = GST_ROUND_UP_8(out_crop->x);
    ipu->task.output.crop.pos.y = GST_ROUND_UP_8(out_crop->y);
    ipu->task.output.crop.w = MIN(out_crop->width,
        (ipu->task.output.width - ipu->task.output.crop.pos.x));
    ipu->task.output.crop.h = MIN(out_crop->height,
        (ipu->task.output.height - ipu->task.output.crop.pos.y));
  }

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
        imx_ipu_frame_copy(device, from_memblk, to_memblk);
        return 0;
      default:
        check_end = TRUE;
        break;
    }

    if (check_end || cnt <= 0)
      break;
  }

  if (ipu->task.input.deinterlace.enable &&
      ipu->task.input.deinterlace.motion != HIGH_MOTION &&
      ipu->first_frame == FALSE) {
    memcpy(ipu->vdi.vaddr, from_memblk->vaddr, ipu->vdi.size);
  }

  // Final conversion
  if (ioctl(ipu->ipu_fd, IPU_QUEUE_TASK, &(ipu->task)) < 0) {
    GST_ERROR("queuing IPU task failed: %s", strerror(errno));
    return -1;
  }

  if (ipu->task.input.deinterlace.enable &&
      ipu->task.input.deinterlace.motion != HIGH_MOTION) {
    memcpy(ipu->vdi.vaddr, from_memblk->vaddr, ipu->vdi.size);
  }
  ipu->first_frame = TRUE;

  return 0;
}

static gint imx_ipu_set_rotate(ImxVideoProcessDevice *device,
                               ImxVideoRotationMode rot)
{
  if (!device || !device->priv)
    return -1;

  ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);
  gint ipu_rotate = IPU_ROTATE_NONE;
  switch (rot) {
  case IMX_VIDEO_ROTATION_0:      ipu_rotate = IPU_ROTATE_NONE;       break;
  case IMX_VIDEO_ROTATION_90:     ipu_rotate = IPU_ROTATE_90_RIGHT;   break;
  case IMX_VIDEO_ROTATION_180:    ipu_rotate = IPU_ROTATE_180;        break;
  case IMX_VIDEO_ROTATION_270:    ipu_rotate = IPU_ROTATE_90_LEFT;    break;
  case IMX_VIDEO_ROTATION_HFLIP:  ipu_rotate = IPU_ROTATE_HORIZ_FLIP; break;
  case IMX_VIDEO_ROTATION_VFLIP:  ipu_rotate = IPU_ROTATE_VERT_FLIP;  break;
  default:                        ipu_rotate = IPU_ROTATE_NONE;       break;
  }
  ipu->task.output.rotate = ipu_rotate;

  return 0;
}

static gint imx_ipu_set_deinterlace (ImxVideoProcessDevice *device,
                                     ImxVideoDeinterlaceMode deint_mode)
{
  if (!device || !device->priv)
    return -1;

  ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);
  switch (deint_mode) {
  case IMX_VIDEO_DEINTERLACE_NONE:
    ipu->deinterlace = FALSE;
    break;
  case IMX_VIDEO_DEINTERLACE_LOW_MOTION:
    ipu->deinterlace = TRUE;
    ipu->task.input.deinterlace.motion = LOW_MOTION;
    break;
  case IMX_VIDEO_DEINTERLACE_MID_MOTION:
    ipu->deinterlace = TRUE;
    ipu->task.input.deinterlace.motion = MED_MOTION;
    break;
  case IMX_VIDEO_DEINTERLACE_HIGH_MOTION:
    ipu->deinterlace = TRUE;
    ipu->task.input.deinterlace.motion = HIGH_MOTION;
    break;
  default:
    ipu->deinterlace = FALSE;
    break;
  }

  return 0;
}

static ImxVideoRotationMode imx_ipu_get_rotate (ImxVideoProcessDevice* device)
{
  if (!device || !device->priv)
    return 0;

  ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);
  ImxVideoRotationMode rot = IMX_VIDEO_ROTATION_0;
  switch (ipu->task.output.rotate) {
  case IPU_ROTATE_NONE:       rot = IMX_VIDEO_ROTATION_0;     break;
  case IPU_ROTATE_90_RIGHT:   rot = IMX_VIDEO_ROTATION_90;    break;
  case IPU_ROTATE_180:        rot = IMX_VIDEO_ROTATION_180;   break;
  case IPU_ROTATE_90_LEFT:    rot = IMX_VIDEO_ROTATION_270;   break;
  case IPU_ROTATE_HORIZ_FLIP: rot = IMX_VIDEO_ROTATION_HFLIP; break;
  case IPU_ROTATE_VERT_FLIP:  rot = IMX_VIDEO_ROTATION_VFLIP; break;
  default:                    rot = IMX_VIDEO_ROTATION_0;     break;
  }

  return rot;
}

static ImxVideoDeinterlaceMode imx_ipu_get_deinterlace (
                                              ImxVideoProcessDevice* device)
{
  if (!device || !device->priv)
    return 0;

  ImxVpDeviceIpu *ipu = (ImxVpDeviceIpu *) (device->priv);
  ImxVideoDeinterlaceMode deint_mode = IMX_VIDEO_DEINTERLACE_NONE;
  if (ipu->deinterlace) {
    switch (ipu->task.input.deinterlace.motion) {
    case LOW_MOTION:  deint_mode = IMX_VIDEO_DEINTERLACE_LOW_MOTION;  break;
    case MED_MOTION:  deint_mode = IMX_VIDEO_DEINTERLACE_MID_MOTION;  break;
    case HIGH_MOTION: deint_mode = IMX_VIDEO_DEINTERLACE_HIGH_MOTION; break;
    default:          deint_mode = IMX_VIDEO_DEINTERLACE_NONE;        break;
    }
  }

  return deint_mode;
}

static gint imx_ipu_get_capabilities(void)
{
  return IMX_VP_DEVICE_CAP_ALL;
}

static GList* imx_ipu_get_supported_in_fmts(void)
{
  GList* list = NULL;
  const IpuFmtMap *map = ipu_fmts_map;
  while (map->gst_video_format != GST_VIDEO_FORMAT_UNKNOWN) {
    list = g_list_append(list, (gpointer)(map->gst_video_format));
    map++;
  }

  return list;
}

static GList* imx_ipu_get_supported_out_fmts(void)
{
  return imx_ipu_get_supported_in_fmts();
}

ImxVideoProcessDevice * imx_ipu_create(void)
{
  ImxVideoProcessDevice * device = g_slice_alloc(sizeof(ImxVideoProcessDevice));
  if (!device) {
    GST_ERROR("allocate device structure failed\n");
    return NULL;
  }

  device->device_type = IMX_VP_DEVICE_IPU;
  device->priv = NULL;

  device->open                = imx_ipu_open;
  device->close               = imx_ipu_close;
  device->alloc_mem           = imx_ipu_alloc_mem;
  device->free_mem            = imx_ipu_free_mem;
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

gint imx_ipu_destroy(ImxVideoProcessDevice *device)
{
  if (!device)
    return -1;

  g_slice_free1(sizeof(ImxVideoProcessDevice), device);

  return 0;
}

