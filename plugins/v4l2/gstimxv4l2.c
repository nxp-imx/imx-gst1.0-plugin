/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
 #include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>

#include <linux/fb.h>
#include <linux/mxcfb.h>
#include <linux/ipu.h>
#include <linux/mxc_v4l2.h>

#include <gst/video/gstvideosink.h>
#include "gstimxv4l2.h"

GST_DEBUG_CATEGORY_EXTERN (imxv4l2_debug);
#define GST_CAT_DEFAULT imxv4l2_debug



#define MAX_BUFFER (32)
#define UPALIGNTO8(a) ((a + 7) & (~7))
#define DOWNALIGNTO8(a) ((a) & (~7))

#define INVISIBLE_IN (0x1)
#define INVISIBLE_OUT (0x2)

typedef struct {
  struct v4l2_buffer v4l2buffer;
  GstBuffer *gstbuffer;
} IMXV4l2BufferPair;

typedef struct {
  gchar *device;
  gint type;
  int v4l2_fd;
  guint disp_w;
  guint disp_h;
  int device_map_id;
  gboolean streamon;
  gint invisible;
  gint streamon_count;
  gint queued_count;
  guint in_fmt;
  guint in_w;
  guint in_h;
  IMXV4l2Rect in_crop;
  gboolean do_deinterlace;
  gint buffer_count;
  gint allocated;
  IMXV4l2BufferPair buffer_pair[MAX_BUFFER];
  gint rotate;
} IMXV4l2Handle;

typedef struct {
  const gchar * caps_str;
  guint v4l2fmt;
  GstVideoFormat gstfmt;
  guint bits_per_pixel;
  guint flags;
} IMXV4l2FmtMap;

static IMXV4l2FmtMap g_imxv4l2fmt_maps[] = {
  {GST_VIDEO_CAPS_MAKE("I420"), V4L2_PIX_FMT_YUV420, GST_VIDEO_FORMAT_I420, 12, 0},
  {GST_VIDEO_CAPS_MAKE("YV12"), V4L2_PIX_FMT_YVU420, GST_VIDEO_FORMAT_YV12, 12, 0},
  {GST_VIDEO_CAPS_MAKE("NV12"), V4L2_PIX_FMT_NV12, GST_VIDEO_FORMAT_NV12, 12, 0},
  {GST_VIDEO_CAPS_MAKE("Y42B"), V4L2_PIX_FMT_YUV422P, GST_VIDEO_FORMAT_Y42B, 16, 0},
  {GST_VIDEO_CAPS_MAKE("Y444"), IPU_PIX_FMT_YUV444P, GST_VIDEO_FORMAT_Y444, 24, 0},
  {GST_VIDEO_CAPS_MAKE("TNVP"), IPU_PIX_FMT_TILED_NV12, GST_VIDEO_FORMAT_UNKNOWN, 12, 0},
  {GST_VIDEO_CAPS_MAKE("TNVF"), IPU_PIX_FMT_TILED_NV12F, GST_VIDEO_FORMAT_UNKNOWN, 12, 0},
  {GST_VIDEO_CAPS_MAKE("UYVY"), V4L2_PIX_FMT_UYVY, GST_VIDEO_FORMAT_UYVY, 16, 0},
  {GST_VIDEO_CAPS_MAKE("YUY2"), V4L2_PIX_FMT_YUYV, GST_VIDEO_FORMAT_YUY2, 16, 0},
  {GST_VIDEO_CAPS_MAKE("RGBx"), V4L2_PIX_FMT_RGB32, GST_VIDEO_FORMAT_RGBx, 32, 0},
  {GST_VIDEO_CAPS_MAKE("BGRx"), V4L2_PIX_FMT_BGR32, GST_VIDEO_FORMAT_BGRx, 32, 0},
  {GST_VIDEO_CAPS_MAKE("RGB"), V4L2_PIX_FMT_RGB24, GST_VIDEO_FORMAT_RGB, 24, 0},
  {GST_VIDEO_CAPS_MAKE("BGR"), V4L2_PIX_FMT_BGR24, GST_VIDEO_FORMAT_RGB, 24, 0},
  {GST_VIDEO_CAPS_MAKE("RGB16"), V4L2_PIX_FMT_RGB565, GST_VIDEO_FORMAT_RGB16, 16, 0},
};

GstCaps *
gst_imx_v4l2_get_device_caps (gint type)
{
  struct v4l2_fmtdesc fmtdesc;
  char * devname;
  int fd;
  gint i;
  GstCaps *caps = NULL;

  fmtdesc.type = type;
  if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    devname = (char*)"/dev/video17";
  }
  else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    devname = (char*)"/dev/video0";
  }
  else {
    GST_ERROR ("UNKNOWN v4l2 type %d\n", type);
    return NULL;
  }

  fd = open(devname, O_RDWR | O_NONBLOCK, 0);
  if (fd <= 0) {
    GST_ERROR ("Can't open %s\n", devname);
    return NULL;
  }

  fmtdesc.index = 0;
  while (!(ioctl (fd, VIDIOC_ENUM_FMT, &fmtdesc))) {
    for (i=0; i<sizeof(g_imxv4l2fmt_maps)/sizeof(IMXV4l2FmtMap); i++) {
      if (fmtdesc.pixelformat == g_imxv4l2fmt_maps[i].v4l2fmt) {
        if (!caps)
          caps = gst_caps_new_empty ();
        if (caps) {
          GstStructure * structure = gst_structure_from_string(g_imxv4l2fmt_maps[i].caps_str, NULL);
          gst_caps_append_structure (caps, structure);
        }
        break;
      }
    }
    fmtdesc.index ++;
  };

  close (fd);

  return caps;
}

guint
gst_imx_v4l2_fmt_gst2v4l2 (GstVideoFormat gstfmt)
{
  guint v4l2fmt = 0;
  int i;

  for(i=0; i<sizeof(g_imxv4l2fmt_maps)/sizeof(IMXV4l2FmtMap); i++) {
    if (gstfmt == g_imxv4l2fmt_maps[i].gstfmt) {
      v4l2fmt = g_imxv4l2fmt_maps[i].v4l2fmt;
      break;
    }
  }

  return v4l2fmt;
}

guint
gst_imx_v4l2_get_bits_per_pixel (guint v4l2fmt)
{
  guint bits_per_pixel = 0;
  int i;
  for(i=0; i<sizeof(g_imxv4l2fmt_maps)/sizeof(IMXV4l2FmtMap); i++) {
    if (v4l2fmt == g_imxv4l2fmt_maps[i].v4l2fmt) {
      bits_per_pixel = g_imxv4l2fmt_maps[i].bits_per_pixel;
      break;
    }
  }

  return bits_per_pixel;
}

typedef struct {
  const gchar *name;
  gboolean bg;
  const gchar *bg_fb_name;
} IMXV4l2DeviceMap;

static IMXV4l2DeviceMap g_device_maps[] = {
  {"/dev/video16", TRUE, "/dev/fb0"},
  {"/dev/video17", FALSE, "/dev/fb0"},
  {"/dev/video18", TRUE, "/dev/fb2"},
  {"/dev/video19", FALSE, "/dev/fb2"},
  {"/dev/video20", TRUE, "/dev/fb4"}
};

static void
gst_imx_v4l2output_set_default_res (IMXV4l2Handle *handle)
{
  struct fb_var_screeninfo fb_var;
  IMXV4l2Rect rect;
  int i;

  for (i=0; i<sizeof(g_device_maps)/sizeof(IMXV4l2DeviceMap); i++) {
    if (!strcmp (handle->device, g_device_maps[i].name)) {
      handle->device_map_id = i;
      break;
    }
  }

  if(!g_device_maps[handle->device_map_id].bg) {
    int fd = open (g_device_maps[handle->device_map_id].bg_fb_name, O_RDWR, 0);
    if (fd < 0) {
      GST_ERROR ("Can't open %s.\n", g_device_maps[handle->device_map_id].bg_fb_name);
      return;
    }

    if (ioctl (fd, FBIOGET_VSCREENINFO, &fb_var) >= 0) {
      handle->disp_w = fb_var.xres;
      handle->disp_h = fb_var.yres;
    }
    else {
#define DEFAULTW (320)
#define DEFAULTH (240)
      GST_ERROR ("Can't get display resolution, use default (%dx%d).\n", DEFAULTW, DEFAULTH);
      handle->disp_w = DEFAULTW;
      handle->disp_h = DEFAULTH;
    }

    {
      //set gblobal alpha to 0 to show video
      struct mxcfb_gbl_alpha galpha;
      galpha.alpha = 0; //overlay transparent
      galpha.enable = 1;
      if (ioctl(fd, MXCFB_SET_GBL_ALPHA, &galpha) < 0)
        GST_ERROR ("Set %s global alpha failed.", g_device_maps[handle->device_map_id].bg_fb_name);
    }

    close (fd);

    rect.left = 0;
    rect.top = 0;
    rect.width = handle->disp_w;
    rect.height = handle->disp_h;
    gst_imx_v4l2out_config_output (handle, &rect, TRUE);
  }

  return;
}

gpointer gst_imx_v4l2_open_device (gchar *device, int type)
{
  int fd;
  IMXV4l2Handle *handle = NULL;

  fd = open(device, O_RDWR | O_NONBLOCK, 0); if (fd < 0) {
    GST_ERROR ("Can't open %s.\n", device);
    return NULL;
  }

  handle = (IMXV4l2Handle*) g_slice_alloc (sizeof(IMXV4l2Handle));
  if (!handle) {
    GST_ERROR ("allocate for IMXV4l2Handle failed.\n");
    close (fd);
    return NULL;
  }
  memset (handle, 0, sizeof(IMXV4l2Handle));

  handle->v4l2_fd = fd; 
  handle->device = device;
  handle->type = type;
  handle->streamon = FALSE;

  if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    gst_imx_v4l2output_set_default_res (handle);
    handle->streamon_count = 1;
  }

  return (gpointer) handle;
}

gint gst_imx_v4l2_reset_device (gpointer v4l2handle)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;

  if (handle && handle->v4l2_fd && handle->streamon) {
    gint i;
    if (ioctl (handle->v4l2_fd, VIDIOC_STREAMOFF, &handle->type) < 0) {
      GST_ERROR ("stream off failed\n");
      return -1;
    }
    handle->streamon = FALSE;

    GST_DEBUG ("V4l2 device hold (%d) buffers when reset.", handle->queued_count);

    for (i=0; i<handle->buffer_count; i++) {
      GST_DEBUG ("unref v4l held gstbuffer(%p).", handle->buffer_pair[i].gstbuffer);
      if (handle->buffer_pair[i].gstbuffer) {
        gst_buffer_unref (handle->buffer_pair[i].gstbuffer);
        handle->buffer_pair[i].gstbuffer = NULL;
      }
    }

    handle->queued_count = 0;

    GST_DEBUG ("V4L2 device is STREAMOFF.");
  }

  return 0;
}

gint gst_imx_v4l2_close_device (gpointer v4l2handle)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;

  if (handle && handle->v4l2_fd) {
    close (handle->v4l2_fd);
    handle->v4l2_fd = 0;
  }

  g_slice_free1 (sizeof(IMXV4l2Handle), handle);

  return 0;
}

gint gst_imx_v4l2out_get_res (gpointer v4l2handle, guint *w, guint *h)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;

  if (handle && handle->v4l2_fd) {
    *w = handle->disp_w;
    *h = handle->disp_h;
  }

  return 0;
}

static gint
gst_imx_v4l2out_do_config_input (IMXV4l2Handle *handle, guint fmt, guint w, guint h, IMXV4l2Rect *crop)
{
  struct v4l2_format v4l2fmt;
  struct v4l2_rect icrop;

  if (gst_imx_v4l2_reset_device ((gpointer)handle) < 0)
    return -1;

  GST_DEBUG ("config in, fmt(%x), res(%dx%d), crop((%d,%d) -> (%d,%d))",
      fmt, w, h, crop->left, crop->top, crop->width, crop->height);

  //align to 8 pixel for IPU limitation
  crop->left = UPALIGNTO8 (crop->left);
  crop->top = UPALIGNTO8 (crop->top);
  crop->width = DOWNALIGNTO8 (crop->width); 
  crop->height = DOWNALIGNTO8 (crop->height);

  if (crop->width <=0 || crop->height <= 0) {
    return 1;
  }

  memset(&v4l2fmt, 0, sizeof(struct v4l2_format));
  v4l2fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  v4l2fmt.fmt.pix.width = handle->in_w = w;
  v4l2fmt.fmt.pix.height = handle->in_h = h;
  v4l2fmt.fmt.pix.pixelformat = handle->in_fmt = fmt;
  icrop.left = crop->left;
  icrop.top = crop->top;
  icrop.width = crop->width;
  icrop.height = crop->height;
  v4l2fmt.fmt.pix.priv = (unsigned int)&icrop;

  if (ioctl(handle->v4l2_fd, VIDIOC_S_FMT, &v4l2fmt) < 0) {
    GST_ERROR ("Set format failed.");
    return -1;
  }

  if (ioctl(handle->v4l2_fd, VIDIOC_G_FMT, &v4l2fmt) < 0) {
    GST_ERROR ("Get format failed.");
    return -1;
  }

  return 0;
}

gint gst_imx_v4l2out_config_input (gpointer v4l2handle, guint fmt, guint w, guint h, IMXV4l2Rect *crop)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;
  gint ret;

  memcpy (&handle->in_crop, crop, sizeof(IMXV4l2Rect));
  ret = gst_imx_v4l2out_do_config_input (handle, fmt, w, h, crop);
  if (ret == 1) {
    GST_DEBUG ("Video is invisible as all input are cropped.");
    handle->invisible |= INVISIBLE_IN;
    return 0;
  }

  handle->invisible &= ~INVISIBLE_IN;

  return ret;
}

static gboolean
gst_imx_v4l2out_calc_crop (IMXV4l2Handle *handle, 
    IMXV4l2Rect *rect_out, GstVideoRectangle *result, IMXV4l2Rect *rect_crop)
{
  gdouble ratio;
  gboolean need_crop = FALSE;

  ratio = (gdouble)handle->in_crop.width / (gdouble)result->w;
  if ((rect_out->left + result->x) < 0) {
    rect_crop->left = ABS(rect_out->left + result->x) * ratio;
    rect_crop->width = MIN(handle->in_crop.width - rect_crop->left, handle->disp_w * ratio);
    rect_out->width = result->w + (rect_out->left + result->x);
    rect_out->left = 0;
    need_crop = TRUE;
  }
  else if ((rect_out->left + result->x + result->w) > handle->disp_w) {
    rect_crop->left = 0;
    rect_crop->width = (rect_out->left + result->x + result->w - handle->disp_w) * ratio;
    rect_out->left = rect_out->left + result->x;
    rect_out->width = result->w;
    need_crop = TRUE;
  }
  else {
    rect_crop->left = 0;
    rect_crop->width = handle->in_crop.width;
    rect_out->left += result->x;
    rect_out->width = result->w;
  }

  if ((rect_out->top + result->y) < 0) {
    rect_crop->top = ABS(rect_out->top + result->y) * ratio;
    rect_crop->height = MIN(handle->in_crop.height - rect_crop->top, handle->disp_h * ratio);
    rect_out->height = result->h + (rect_out->top + result->y);
    rect_out->top = 0;
    need_crop = TRUE;
  }
  else if ((rect_out->top + result->y + result->h) > handle->disp_h) {
    rect_crop->top = 0;
    rect_crop->height = (rect_out->top + result->y + result->h - handle->disp_h) * ratio;
    rect_out->top = rect_out->top + result->y;
    rect_out->height = result->h;
    need_crop = TRUE;
  }
  else {
    rect_crop->top = 0;
    rect_crop->height = handle->in_crop.height;
    rect_out->top += result->y;
    rect_out->height = result->h;
  } 

  return need_crop;
}

gint gst_imx_v4l2out_config_output (gpointer v4l2handle, IMXV4l2Rect *overlay, gboolean keep_video_ratio)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  IMXV4l2Rect *rect = &crop.c;;
  GstVideoRectangle src, dest, result;
  gboolean brotate; 

  memset(&cropcap, 0, sizeof(cropcap));
  cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  if (ioctl(handle->v4l2_fd, VIDIOC_CROPCAP, &cropcap) < 0) {
    GST_ERROR ("Get crop capability failed.");
    return -1;
  }


  memcpy (rect, overlay, sizeof(IMXV4l2Rect));

  GST_DEBUG ("config output, (%d, %d) -> (%d, %d)",
      rect->left, rect->top, rect->width, rect->height);

  brotate = (handle->rotate == 90 || handle->rotate == 270) ? TRUE : FALSE;

  //keep video ratio with display
  src.x = src.y = 0;
  src.w = handle->in_crop.width;
  src.h = handle->in_crop.height;
  dest.x = dest.y = 0;
  if (brotate) {
    dest.w = rect->height;
    dest.h = rect->width;
  }
  else {
    dest.w = rect->width;
    dest.h = rect->height;
  }

  if (keep_video_ratio)
    gst_video_sink_center_rect (src, dest, &result, TRUE);
  else
    memcpy (&result, &dest, sizeof(GstVideoRectangle));

  GST_DEBUG ("src, (%d, %d) -> (%d, %d).", src.x, src.y, src.w, src.h);
  GST_DEBUG ("dest, (%d, %d) -> (%d, %d).", dest.x, dest.y, dest.w, dest.h);
  GST_DEBUG ("result, (%d, %d) -> (%d, %d).", result.x, result.y, result.w, result.h);

  if (handle->rotate == 0) {
    // only support video out of screen in landscape mode
    IMXV4l2Rect rect_crop;
    if (gst_imx_v4l2out_calc_crop (handle, rect, &result, &rect_crop)) {
      gint ret = gst_imx_v4l2out_do_config_input (handle, handle->in_fmt, handle->in_w, handle->in_h, &rect_crop);
      if (ret == 1) {
        handle->invisible |= INVISIBLE_OUT;
        GST_DEBUG ("Video is invisible as out of display.");
        return 0;
      }
      else if (ret < 0)
        return ret;
      else
        handle->invisible &= ~INVISIBLE_OUT;
    }
  }
  else {
    if (brotate) {
      rect->left += result.y;
      rect->top += result.x;
      rect->width = result.h;
      rect->height = result.w;
    }
    else {
      rect->left += result.x;
      rect->top += result.y;
      rect->width = result.w;
      rect->height = result.h;
    }
  }

  if (rect->left >= handle->disp_w || rect->top >= handle->disp_h) {
    handle->invisible |= INVISIBLE_OUT;
    GST_DEBUG ("Video is invisible as out of display.");
    return 1;
  }
  handle->invisible &= ~INVISIBLE_OUT;

  GST_DEBUG ("rect, (%d, %d) -> (%d, %d).", rect->left, rect->top, rect->width, rect->height);

  crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  if (ioctl(handle->v4l2_fd, VIDIOC_S_CROP, &crop) < 0) {
    GST_ERROR ("Set crop failed.");
    return -1;
  }

  return 0;
}

gint gst_imx_v4l2_config_rotate (gpointer v4l2handle, gint rotate)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;
  struct v4l2_control ctrl;

  GST_DEBUG ("set rotation to (%d).", rotate);

  ctrl.id = V4L2_CID_ROTATE;
  ctrl.value = rotate;
  if (ioctl(handle->v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
    GST_ERROR ("Set ctrl rotate failed.");
    return -1;
  }
  handle->rotate = rotate;

  return 0;
}

gint gst_imx_v4l2_config_deinterlace (gpointer v4l2handle, gboolean do_deinterlace, guint motion)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;
  struct v4l2_control ctrl;

  if (do_deinterlace) {
    ctrl.id = V4L2_CID_MXC_MOTION;
    ctrl.value = motion;
    if (ioctl(handle->v4l2_fd, VIDIOC_S_CTRL, &ctrl) < 0) {
      GST_ERROR("Set ctrl motion failed\n");
      return -1;
    }
  }

  handle->do_deinterlace = do_deinterlace;

  return 0;
}

gint gst_imx_v4l2_set_buffer_count (gpointer v4l2handle, guint count)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;
  struct v4l2_requestbuffers buf_req;

  GST_DEBUG ("requeset for (%d) buffers.", count);

  memset(&buf_req, 0, sizeof(buf_req));

  buf_req.type = handle->type;
  buf_req.count = count;
  buf_req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(handle->v4l2_fd, VIDIOC_REQBUFS, &buf_req) < 0) {
    GST_ERROR("Request %d buffers failed\n", count);
    return -1;
  }
  handle->buffer_count = count;

  return 0;
}

gint gst_imx_v4l2_allocate_buffer (gpointer v4l2handle, PhyMemBlock *memblk)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;
  struct v4l2_buffer *v4l2buf;

  if (handle->allocated >= handle->buffer_count) {
    GST_ERROR ("No more v4l2 buffer for allocating.\n");
    return -1;
  }

  v4l2buf = &handle->buffer_pair[handle->allocated].v4l2buffer;
  memset (v4l2buf, 0, sizeof(struct v4l2_buffer));
  v4l2buf->type = handle->type;
  v4l2buf->memory = V4L2_MEMORY_MMAP;;
  v4l2buf->index = handle->allocated;

  if (ioctl(handle->v4l2_fd, VIDIOC_QUERYBUF, v4l2buf) < 0) {
    GST_ERROR ("VIDIOC_QUERYBUF error.");
    return -1;
  }

  handle->allocated ++;
  memblk->size = v4l2buf->length;
  memblk->vaddr = mmap (NULL, v4l2buf->length, PROT_READ | PROT_WRITE, MAP_SHARED, handle->v4l2_fd, v4l2buf->m.offset);
  if (!memblk->vaddr) {
    GST_ERROR ("mmap v4lbuffer address failed\n");
    return -1;
  }

  //FIXME: need to query twice to get the physical address
  if (ioctl(handle->v4l2_fd, VIDIOC_QUERYBUF, v4l2buf) < 0) {
    GST_ERROR ("VIDIOC_QUERYBUF for physical address failed.");
    return -1;
  }
  memblk->paddr = (guint8*) v4l2buf->m.offset;
  memblk->user_data = (gpointer) v4l2buf;

  GST_DEBUG ("Allocated v4l2buffer(%p), index(%d).", v4l2buf, handle->allocated - 1);

  return 0;
}

gint gst_imx_v4l2_free_buffer (gpointer v4l2handle, PhyMemBlock *memblk)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;
  struct v4l2_buffer *v4l2buf;

  if (memblk->vaddr)
    munmap(memblk->vaddr, memblk->size);
  handle->allocated --;
  if (handle->allocated < 0) {
    GST_ERROR ("freed buffer more than allocated.");
    handle->allocated = 0;
  }

  v4l2buf =  (struct v4l2_buffer*) memblk->user_data;
  GST_DEBUG ("Free v4l2buffer(%p), index(%d).", v4l2buf, v4l2buf->index);
  memset (v4l2buf, 0, sizeof(struct v4l2_buffer));

  return 0;
}

gint gst_imx_v4l2_queue_buffer (gpointer v4l2handle, GstBuffer *buffer, GstVideoFrameFlags flags)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;
  struct v4l2_buffer *v4l2buf;
  PhyMemBlock *memblk;
  gint index;
  struct timeval queuetime;

  if (handle->invisible) {
    gst_buffer_unref (buffer);
    return 0;
  }

  memblk = gst_buffer_query_phymem_block(buffer);
  if (!memblk) {
    GST_ERROR ("Can't get physical memory block from gstbuffer.\n");
    return -1;
  }

  v4l2buf = (struct v4l2_buffer *) memblk->user_data;
  index = v4l2buf->index;
  if (handle->buffer_pair[index].gstbuffer)
    GST_ERROR ("gstbuffer(%p) not dequeued yet but queued again, index(%d).", buffer, index);
  handle->buffer_pair[index].gstbuffer = buffer;

  v4l2buf->field = V4L2_FIELD_NONE;
  if ((flags & GST_VIDEO_FRAME_FLAG_INTERLACED) && handle->do_deinterlace) {
    if (flags & GST_VIDEO_FRAME_FLAG_TFF)
      v4l2buf->field = V4L2_FIELD_INTERLACED_TB;
    else
      v4l2buf->field = V4L2_FIELD_INTERLACED_BT;
  }

  if (flags & GST_VIDEO_FRAME_FLAG_ONEFIELD) {
    if (flags & GST_VIDEO_FRAME_FLAG_TFF)
      v4l2buf->field = V4L2_FIELD_TOP;
    else
      v4l2buf->field = V4L2_FIELD_BOTTOM;
  }

  /*display immediately */
  gettimeofday (&queuetime, NULL);
  v4l2buf->timestamp = queuetime;

  GST_DEBUG ("queue gstbuffer(%p), flags(%x), v4lbuffer(%p), index(%d).",
      buffer, flags, v4l2buf, index);

  if (ioctl (handle->v4l2_fd, VIDIOC_QBUF, v4l2buf) < 0) {
    GST_ERROR ("queue v4l2 buffer failed.\n");
    handle->buffer_pair[index].gstbuffer = NULL;
    return -1;
  }

  handle->queued_count ++;

  if (!handle->streamon && handle->queued_count >= handle->streamon_count) {
    if (ioctl (handle->v4l2_fd, VIDIOC_STREAMON, &handle->type) < 0) {
      GST_ERROR ("Stream on V4L2 device failed.\n");
      return -1;
    }
    handle->streamon = TRUE;
    GST_DEBUG ("V4L2 device is STREAMON.");
  }

  return 0;
}

#define V4L2_HOLDED_BUFFERS (2)
#define TRY_TIMEOUT (500000) //500ms
#define TRY_INTERVAL (10000) //10ms
#define MAX_TRY_CNT (TRY_TIMEOUT/TRY_INTERVAL)
gint gst_imx_v4l2_dequeue_buffer (gpointer v4l2handle, GstBuffer **buffer)
{
  IMXV4l2Handle *handle = (IMXV4l2Handle*)v4l2handle;
  struct v4l2_buffer v4l2buf;
  gint trycnt = 0;

  if (handle->queued_count <= V4L2_HOLDED_BUFFERS)
    return 0;

  if (handle->invisible) {
    return 0;
  }

  memset (&v4l2buf, 0, sizeof(v4l2buf));
  v4l2buf.type = handle->type;
  v4l2buf.memory = V4L2_MEMORY_MMAP;

  while (ioctl (handle->v4l2_fd, VIDIOC_DQBUF, &v4l2buf) < 0) {
    trycnt ++;
    if(trycnt >= MAX_TRY_CNT) {
      GST_ERROR ("Dequeue buffer from v4l2 device failed.");
      return -1;
    }

    usleep (TRY_INTERVAL);
  }

  *buffer = handle->buffer_pair[v4l2buf.index].gstbuffer;
  handle->buffer_pair[v4l2buf.index].gstbuffer = NULL;

  GST_DEBUG ("dequeue gstbuffer(%p), v4l2buffer index(%d).", *buffer, v4l2buf.index);

  return 0;
}

