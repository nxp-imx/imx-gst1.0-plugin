/* i.Mx GStreamer fbdev plugin
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <gst/allocators/gstdmabuf.h>
#include <gst/allocators/gstallocatorphymem.h>
#include <gst/allocators/gstphysmemory.h>
#ifdef USE_ION
#include <gst/allocators/gstionmemory.h>
#endif

#include "gstimxfbdevsink.h"
#include "gstimxcommon.h"
#include "gstimx.h"
#include "gstimxvideooverlay.h"

GST_DEBUG_CATEGORY (imx_fbdevsink_debug);
#define GST_CAT_DEFAULT imx_fbdevsink_debug

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_ROTATE_METHOD,
  PROP_VIDEO_DIRECTION,
  PROP_FORCE_ASPECT_RATIO
};

static GstFlowReturn gst_imx_fbdevsink_show_frame (GstVideoSink * videosink,
    GstBuffer * buff);

static gboolean gst_imx_fbdevsink_start (GstBaseSink * bsink);
static gboolean gst_imx_fbdevsink_stop (GstBaseSink * bsink);

static GstCaps *gst_imx_fbdevsink_getcaps (GstBaseSink * bsink, GstCaps * filter);
static gboolean gst_imx_fbdevsink_setcaps (GstBaseSink * bsink, GstCaps * caps);

static void gst_imx_fbdevsink_finalize (GObject * object);
static void gst_imx_fbdevsink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_imx_fbdevsink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_imx_fbdevsink_change_state (GstElement * element,
    GstStateChange transition);

/* FIXME: when pan display support, change this marco */
#define DISPLAY_NUM_BUFFERS (1)

/* FIXME: how to support 10 bit */
#define VIDEO_CAPS "{BGRA, NV12, YVYU, UYVY, VYUY}"

#define BG_DEVICE "/dev/fb0"
#define ISALIGNED(a, b) (!(a & (b-1)))
#define ALIGNTO(a, b) ((a + (b-1)) & (~(b-1)))
#define ALIGNMENT (8)
#define MAX_BUFFERS 30
#define MIN_BUFFERS 3

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (VIDEO_CAPS))
    );

GST_IMPLEMENT_VIDEO_OVERLAY_METHODS (GstImxFBDEVSink, gst_imx_fbdevsink);

static gboolean
imx_fbdevsink_update_video_geo(GstElement * object, GstVideoRectangle win_rect)
{
  GstImxFBDEVSink *fbdevsink = GST_IMX_FBDEVSINK (object);

  if (win_rect.w <= 0 || win_rect.h <= 0) {
    return TRUE;
  }

  GST_OBJECT_LOCK (fbdevsink);
  if (fbdevsink->video_geo.x == win_rect.x && fbdevsink->video_geo.y == win_rect.y &&
      fbdevsink->video_geo.w == win_rect.w && fbdevsink->video_geo.h == win_rect.h) {
    GST_OBJECT_UNLOCK (fbdevsink);
    return TRUE;
  }

  fbdevsink->video_geo.x = win_rect.x;
  fbdevsink->video_geo.y = win_rect.y;
  fbdevsink->video_geo.w = win_rect.w;
  fbdevsink->video_geo.h = win_rect.h;

  GST_INFO_OBJECT(fbdevsink, "resize to (%d - %d) * (%d x %d)",
      fbdevsink->video_geo.x, fbdevsink->video_geo.y,
      fbdevsink->video_geo.w, fbdevsink->video_geo.h);

  fbdevsink->need_reconfigure = TRUE;

  GST_OBJECT_UNLOCK (fbdevsink);

  if (((GstBaseSink*)fbdevsink)->eos || GST_STATE(object) == GST_STATE_PAUSED) {
    gst_imx_fbdevsink_show_frame((GstVideoSink *)fbdevsink, fbdevsink->last_buffer);
  }

  return TRUE;
}

static void
imx_fbdevsink_config_global_alpha(GObject * object, guint alpha)
{
  /* FIXME: nothing to do here */
}

static void
imx_fbdevsink_config_color_key(GObject * object, gboolean enable, guint color_key)
{
  /* FIXME: nothing to do here */
}

/* should call with GST_OBJECT_LOCK */
static gboolean
gst_imx_fbdevsink_output_config (GstImxFBDEVSink *fbdevsink)
{
  GstVideoRectangle src = {0, };
  GstVideoRectangle dst = {0, };
  GstVideoRectangle result = {0, };

  if (fbdevsink->keep_ratio) {
    dst.w = fbdevsink->video_geo.w;
    dst.h = fbdevsink->video_geo.h;

    if (fbdevsink->method == GST_VIDEO_ORIENTATION_90R
        || fbdevsink->method == GST_VIDEO_ORIENTATION_90L) {
      src.h = fbdevsink->width;
      src.w = fbdevsink->height;
    } else {
      src.w = fbdevsink->width;
      src.h = fbdevsink->height;
    }
    
    gst_video_sink_center_rect (src, dst, &result, TRUE);

    result.x += fbdevsink->video_geo.x;
    result.y += fbdevsink->video_geo.y;
  } else {
    result = fbdevsink->video_geo;
  }

  GST_INFO_OBJECT(fbdevsink, "keep_ratio %s output (%d - %d) * (%d x %d)",
      fbdevsink->keep_ratio ? "TRUE" : "FALSE",
      result.x, result.y, result.w, result.h);

#if 0
  fbdevsink->varinfo.reserved [0] = result.x;
  fbdevsink->varinfo.reserved [1] = result.y;
  fbdevsink->varinfo.reserved [2] = result.w;
  fbdevsink->varinfo.reserved [3] = result.h;

  /* FIXME: add rotate configure when api ready */
  if (ioctl (fbdevsink->fd, FBIOPUT_VSCREENINFO, &fbdevsink->varinfo) < 0) {
    GST_DEBUG_OBJECT(fbdevsink, "put var failed");
    return FALSE;
  }
#endif

  fbdevsink->need_reconfigure = FALSE;

  return TRUE;
}

static gboolean
gst_imx_fbdevsink_cropmeta_changed_and_set (GstVideoCropMeta *src, GstVideoCropMeta *dest)
{
  if (!src || !dest)
    return FALSE;

  if (src->x != dest->x || src->y != dest->y || src->width != dest->width || src->height != dest->height) {
    dest->x = src->x;
    dest->y = src->y;
    dest->width = src->width;
    dest->height = src->height;
    return TRUE;
  }

  return FALSE;
}

static void
gst_imx_fbdevsink_video_direction_interface_init (GstVideoDirectionInterface *
    iface)
{
  /* We implement the video-direction property */
}

#define parent_class gst_imx_fbdevsink_parent_class
G_DEFINE_TYPE_WITH_CODE (GstImxFBDEVSink, gst_imx_fbdevsink, GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_imx_fbdevsink_video_overlay_interface_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_DIRECTION,
        gst_imx_fbdevsink_video_direction_interface_init));

static void
gst_imx_fbdevsink_init (GstImxFBDEVSink * fbdevsink)
{
  fbdevsink->last_buffer = NULL;
  fbdevsink->pool = NULL;
  fbdevsink->allocator = NULL;
  fbdevsink->keep_ratio = TRUE;
  fbdevsink->imxoverlay = gst_imx_video_overlay_init ((GstElement *)fbdevsink,
                                              imx_fbdevsink_update_video_geo,
                                              imx_fbdevsink_config_color_key,
                                              imx_fbdevsink_config_global_alpha);
}

static GstCaps *
gst_imx_fbdevsink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstImxFBDEVSink *fbdevsink;
  GstCaps *caps;

  fbdevsink = GST_IMX_FBDEVSINK (bsink);

  caps = gst_static_pad_template_get_caps (&sink_template);

  if (filter != NULL) {
    GstCaps *icaps;

    icaps = gst_caps_intersect (caps, filter);
    gst_caps_unref (caps);
    caps = icaps;
  }

  return caps;
}

static void
_dump_varinfo (GstImxFBDEVSink *fbdevsink, struct fb_var_screeninfo * varinfo)
{
  GST_DEBUG_OBJECT(fbdevsink, "xres %d", varinfo->xres);
  GST_DEBUG_OBJECT(fbdevsink, "yres %d", varinfo->yres);
  GST_DEBUG_OBJECT(fbdevsink, "xres_v %d", varinfo->xres_virtual);
  GST_DEBUG_OBJECT(fbdevsink, "yres_v %d", varinfo->yres_virtual);
  GST_DEBUG_OBJECT(fbdevsink, "xoffset %d", varinfo->xoffset);
  GST_DEBUG_OBJECT(fbdevsink, "yoffset %d", varinfo->yoffset);
  GST_DEBUG_OBJECT(fbdevsink, "bits_per_pixel %d", varinfo->bits_per_pixel);
  GST_DEBUG_OBJECT(fbdevsink, "grayscale %d", varinfo->grayscale);
}


static gboolean
gst_imx_fbdevsink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstImxFBDEVSink *fbdevsink;
  GstStructure *structure;
  const GValue *fps;

  fbdevsink = GST_IMX_FBDEVSINK (bsink);

  GST_DEBUG_OBJECT (fbdevsink, "set caps %" GST_PTR_FORMAT, caps);
  
  gst_video_info_from_caps(&fbdevsink->vinfo, caps);

  fbdevsink->width = GST_VIDEO_INFO_WIDTH (&fbdevsink->vinfo);
  fbdevsink->height = GST_VIDEO_INFO_HEIGHT (&fbdevsink->vinfo);

  fbdevsink->vfmt = GST_VIDEO_INFO_FORMAT (&fbdevsink->vinfo);
  
  /* get the variable screen info */
  if (ioctl (fbdevsink->fd, FBIOGET_VSCREENINFO, &fbdevsink->stored) < 0)
    return FALSE;

  fbdevsink->varinfo = fbdevsink->stored;
  fbdevsink->var_stored = TRUE;

  GST_DEBUG_OBJECT (fbdevsink, "start to configure display");
  /* FIXME: remove this condition when fb0 can be used */
  if (strcmp (fbdevsink->device, "/dev/fb0") != 0) {
    fbdevsink->varinfo.xoffset = 0;
    fbdevsink->varinfo.xres = fbdevsink->width;
    fbdevsink->varinfo.xres_virtual = fbdevsink->width;
    fbdevsink->varinfo.yoffset = 0;
    fbdevsink->varinfo.yres = fbdevsink->height;
    fbdevsink->varinfo.yres_virtual = fbdevsink->height * DISPLAY_NUM_BUFFERS;
    fbdevsink->varinfo.activate |= FB_ACTIVATE_FORCE;
    fbdevsink->varinfo.grayscale = gst_video_format_to_fourcc (fbdevsink->vfmt); 

    GST_DEBUG_OBJECT (fbdevsink, "resolution (%d x %d) format %s",
        fbdevsink->width, fbdevsink->height,
        gst_video_format_to_string(fbdevsink->vfmt));

    if (ioctl (fbdevsink->fd, FBIOPUT_VSCREENINFO, &fbdevsink->varinfo) < 0) {
      GST_DEBUG_OBJECT(fbdevsink, "put var failed");
      return FALSE;
    }
    GST_DEBUG_OBJECT (fbdevsink, "configure display successfully");
  }
  
  /* get the fixed screen info */
  if (ioctl (fbdevsink->fd, FBIOGET_FSCREENINFO, &fbdevsink->fixinfo) < 0)
    return FALSE;

  if (ioctl (fbdevsink->fd, FBIOGET_VSCREENINFO, &fbdevsink->varinfo) < 0)
    return FALSE;

  _dump_varinfo (fbdevsink, &fbdevsink->varinfo);

  if (fbdevsink->video_geo.w == 0) {
    fbdevsink->video_geo.w = fbdevsink->display_width;
    fbdevsink->video_geo.h = fbdevsink->display_height;
  }
  
  return TRUE;
}

static gboolean
gst_imx_fbdevsink_setup_buffer_pool (GstImxFBDEVSink *fbdevsink, GstCaps *caps)
{
  GstStructure *structure;
  GstVideoInfo info;

  if (fbdevsink->pool) {
    GST_DEBUG_OBJECT (fbdevsink, "already have a pool (%p)", fbdevsink->pool);
    return TRUE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (fbdevsink, "invalid caps.");
    return FALSE;
  }

  fbdevsink->pool = gst_video_buffer_pool_new ();
  if (!fbdevsink->pool) {
    GST_ERROR_OBJECT (fbdevsink, "New video buffer pool failed");
    return FALSE;
  }
  GST_DEBUG_OBJECT (fbdevsink, "create buffer pool (%p)", fbdevsink->pool);

  if (!fbdevsink->allocator) {
#ifdef USE_ION
    fbdevsink->allocator = gst_ion_allocator_obtain ();
#endif
    if (!fbdevsink->allocator) {
      GST_ERROR_OBJECT (fbdevsink, "New allocator failed");
      return FALSE;
    }
    GST_DEBUG_OBJECT (fbdevsink, "create allocator (%p)", fbdevsink->allocator);
  }

  structure = gst_buffer_pool_get_config (fbdevsink->pool);

  /* buffer alignment configuration */
  if (!ISALIGNED (fbdevsink->width, ALIGNMENT) || !ISALIGNED (fbdevsink->height, ALIGNMENT)) {
    GstVideoAlignment alignment;

    memset (&alignment, 0, sizeof (GstVideoAlignment));
    alignment.padding_right = ALIGNTO (fbdevsink->width, ALIGNMENT) - fbdevsink->width;
    alignment.padding_bottom = ALIGNTO (fbdevsink->height, ALIGNMENT) - fbdevsink->height;

    GST_DEBUG_OBJECT (fbdevsink, "align buffer pool, w(%d) h(%d), padding_right (%d), padding_bottom (%d)",
        fbdevsink->width, fbdevsink->height, alignment.padding_right, alignment.padding_bottom);

    gst_buffer_pool_config_add_option (structure, GST_BUFFER_POOL_OPTION_VIDEO_META);
    gst_buffer_pool_config_add_option (structure, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    gst_buffer_pool_config_set_video_alignment (structure, &alignment);
  }

  gst_buffer_pool_config_set_params (structure, caps, info.size, MIN_BUFFERS, MAX_BUFFERS);
  gst_buffer_pool_config_set_allocator (structure, fbdevsink->allocator, NULL);
  if (!gst_buffer_pool_set_config (fbdevsink->pool, structure)) {
    GST_ERROR_OBJECT (fbdevsink, "set buffer pool failed");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_imx_fbdevsink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstImxFBDEVSink *fbdevsink = GST_IMX_FBDEVSINK (bsink);
  guint size = 0;
  GstCaps *caps;
  gboolean need_pool;
  GstCaps *pcaps;
  GstStructure *config;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (need_pool) {
    if (caps == NULL) {
      GST_ERROR_OBJECT (fbdevsink, "no caps specified");
      return FALSE;
    }

    GST_DEBUG_OBJECT (fbdevsink, "prosal set caps %" GST_PTR_FORMAT, caps);

    if (fbdevsink->pool) {
      // check caps, if caps not change, reuse previous pool
      config = gst_buffer_pool_get_config (fbdevsink->pool);
      gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

      if (!gst_caps_is_equal (pcaps, caps)) {
        if (fbdevsink->last_buffer) {
          gst_buffer_unref (fbdevsink->last_buffer);
          fbdevsink->last_buffer = NULL;
        }

        gst_buffer_pool_set_active (fbdevsink->pool, FALSE);
        gst_object_unref (fbdevsink->pool);
        fbdevsink->pool = NULL;
      }
      gst_structure_free (config);
    }

    if (!fbdevsink->pool) {
      if (gst_imx_fbdevsink_setup_buffer_pool (fbdevsink, caps) < 0) {
        GST_ERROR_OBJECT (fbdevsink, "setup buffer pool failed");
        return FALSE;
      }
    }

    if (fbdevsink->pool) {
      config = gst_buffer_pool_get_config (fbdevsink->pool);
      gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);
      gst_structure_free (config);

      GST_INFO_OBJECT (fbdevsink, "propose_allocation, pool (%p)", fbdevsink->pool);

      gst_query_add_allocation_pool (query, fbdevsink->pool, size, MIN_BUFFERS, MAX_BUFFERS);
      gst_query_add_allocation_param (query, fbdevsink->allocator, NULL);
    } else {
      return FALSE;
    }
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);
  /* FIXME: add allocation meta for modifier format */
#if 0
  guint64 drm_modifier = 1;
  gst_query_add_allocation_dmabuf_meta (query, drm_modifier);
#endif
  return TRUE;
}

static GstFlowReturn
gst_imx_fbdevsink_show_frame (GstVideoSink * videosink, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstImxFBDEVSink *fbdevsink;
  GstMapInfo map;
  gint i, fb_size, n_mem;
  GstMemory *in_mem;
  GstVideoCropMeta *cropmeta = NULL;
  guintptr phys_addr = 0;;
  gint dma_fd;

  fbdevsink = GST_IMX_FBDEVSINK (videosink);
 
  if (!buf){
    GST_ERROR_OBJECT (fbdevsink, "invalid video buffer");
    return GST_FLOW_ERROR;
  }
  
  GST_DEBUG_OBJECT (fbdevsink, "buffer type phy %d dma %d",
      gst_buffer_is_phymem (buf),
      gst_is_dmabuf_memory (gst_buffer_peek_memory (buf, 0)));

  gst_buffer_ref (buf);

  /* FIXME: add cropmeta configure to display */
  cropmeta = gst_buffer_get_video_crop_meta (buf);
  if (gst_imx_fbdevsink_cropmeta_changed_and_set (cropmeta, &fbdevsink->cropmeta)){
  }

  in_mem = gst_buffer_peek_memory (buf, 0);
  if (gst_buffer_is_phymem (buf)) { 
    phys_addr = gst_phys_memory_get_phys_addr (in_mem);
  } else if (gst_is_dmabuf_memory (in_mem)){
    dma_fd = gst_dmabuf_memory_get_fd (in_mem);
    phys_addr = phy_addr_from_fd (dma_fd);
  } else { /* allocate dmabuf from buffer pool */
    GstBuffer *temp = NULL;
    GstVideoFrame frame1, frame2;
      
    GST_DEBUG_OBJECT(fbdevsink, "do buffer copy");

    gst_video_frame_map (&frame1, &fbdevsink->vinfo, buf, GST_MAP_READ);

    GstCaps *new_caps = gst_video_info_to_caps(&frame1.info);
    gst_video_info_from_caps(&fbdevsink->vinfo, new_caps); //update the size info

    if (!fbdevsink->pool) {
      gst_imx_fbdevsink_setup_buffer_pool (fbdevsink, new_caps);
      gst_caps_unref (new_caps);

      if (!fbdevsink->pool)
        return GST_FLOW_ERROR;
      
      GST_DEBUG_OBJECT(fbdevsink, "creating buffer pool %p", fbdevsink->pool);
    } else {
      gst_caps_unref (new_caps);
    }

    if (!gst_buffer_pool_is_active (fbdevsink->pool)) {
      if (gst_buffer_pool_set_active (fbdevsink->pool, TRUE) != TRUE) {
        GST_ERROR_OBJECT (fbdevsink, "active pool (%p) failed", fbdevsink->pool);
        return GST_FLOW_ERROR;
      }
    }

    gst_buffer_pool_acquire_buffer (fbdevsink->pool, &temp, NULL);
    if (!temp) {
      GST_ERROR_OBJECT (fbdevsink, "acquire buffer from pool failed");
      return GST_FLOW_ERROR;
    }

    gst_video_frame_map (&frame2, &fbdevsink->vinfo, temp, GST_MAP_WRITE);
    gst_video_frame_copy (&frame2, &frame1);
    gst_video_frame_unmap (&frame1);
    gst_video_frame_unmap (&frame2);

    GstVideoMeta *meta = gst_buffer_get_video_meta(buf);
    if (meta) {
      gst_buffer_add_video_meta(temp, meta->flags,
                                meta->format, meta->width, meta->height);
    }

    gst_buffer_unref (buf);
    buf = temp;
    in_mem = gst_buffer_peek_memory (buf, 0);
    dma_fd = gst_dmabuf_memory_get_fd (in_mem);
    phys_addr = phy_addr_from_fd (dma_fd);
  }

  /* config video geo and direction */
  GST_OBJECT_LOCK (fbdevsink);
  if (fbdevsink->need_reconfigure)
    gst_imx_fbdevsink_output_config (fbdevsink);
  GST_OBJECT_UNLOCK (fbdevsink);

  GST_DEBUG_OBJECT (fbdevsink, "pan display buffer paddr %x", phys_addr);
  if (phys_addr) {
    fbdevsink->varinfo.reserved[0] = phys_addr;
    if (ioctl(fbdevsink->fd, FBIOPAN_DISPLAY, &fbdevsink->varinfo) < 0) {
      GST_ERROR_OBJECT (fbdevsink, "pan display failed");
    }
  }
  GST_DEBUG_OBJECT (fbdevsink, "pan display successfully");

  if (fbdevsink->frame_showed == 0) {
    if (strcmp (fbdevsink->device, "/dev/fb0") != 0) {
      ioctl(fbdevsink->fd, FBIOBLANK, FB_BLANK_UNBLANK);
      fbdevsink->unblanked = TRUE;
    }
  }

  if (fbdevsink->last_buffer)
    gst_buffer_unref (fbdevsink->last_buffer);

  fbdevsink->last_buffer = buf;
  fbdevsink->frame_showed ++;

  return GST_FLOW_OK;
}

static gboolean
gst_imx_fbdevsink_start (GstBaseSink * bsink)
{
  GstImxFBDEVSink *fbdevsink;
  int fd;
  struct fb_var_screeninfo scrinfo;

  fbdevsink = GST_IMX_FBDEVSINK (bsink);

  fd = open (BG_DEVICE, O_RDWR);
  if (fd < 0) {
    GST_ERROR_OBJECT (fbdevsink, "can not open background device %s", BG_DEVICE);
    return FALSE;
  }

  if (ioctl (fd, FBIOGET_VSCREENINFO, &scrinfo) < 0) {
    GST_ERROR_OBJECT ("Get var of %s failed", BG_DEVICE);
    close (fd);
    return FALSE;
  }

  fbdevsink->display_width = scrinfo.xres;
  fbdevsink->display_height = scrinfo.yres;
  close (fd);

  /* default to use fb1 */
  if (!fbdevsink->device) {
    fbdevsink->device = g_strdup ("/dev/fb1");
  }

  fbdevsink->fd = open (fbdevsink->device, O_RDWR);

  if (fbdevsink->fd == -1) {
    GST_ERROR_OBJECT (fbdevsink, "can not open video device %s", fbdevsink->device);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_imx_fbdevsink_stop (GstBaseSink * bsink)
{
  GstImxFBDEVSink *fbdevsink;

  fbdevsink = GST_IMX_FBDEVSINK (bsink);

  if (strcmp (fbdevsink->device, "/dev/fb0") != 0 && fbdevsink->unblanked)
    ioctl(fbdevsink->fd, FBIOBLANK, FB_BLANK_NORMAL);

  if (fbdevsink->var_stored) {
    if (ioctl (fbdevsink->fd, FBIOPUT_VSCREENINFO, &fbdevsink->stored) < 0) {
      GST_DEBUG("put var failed");
      return FALSE;
    }
  }
  
  if (close (fbdevsink->fd))
    return FALSE;

  return TRUE;
}

static void
gst_imx_fbdevsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstImxFBDEVSink *fbdevsink;

  fbdevsink = GST_IMX_FBDEVSINK (object);

  switch (prop_id) {
    case PROP_DEVICE:{
      g_free (fbdevsink->device);
      fbdevsink->device = g_value_dup_string (value);
      break;
    }
    case PROP_ROTATE_METHOD:
    case PROP_VIDEO_DIRECTION:{
      GST_OBJECT_LOCK (fbdevsink);
      fbdevsink->method = g_value_get_enum (value);
      fbdevsink->need_reconfigure = TRUE;
      GST_OBJECT_UNLOCK (fbdevsink);
      break;
    }
    case PROP_FORCE_ASPECT_RATIO:{
      fbdevsink->keep_ratio = g_value_get_boolean (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_imx_fbdevsink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstImxFBDEVSink *fbdevsink;

  fbdevsink = GST_IMX_FBDEVSINK (object);

  switch (prop_id) {
    case PROP_DEVICE:{
      g_value_set_string (value, fbdevsink->device);
      break;
    }
    case PROP_ROTATE_METHOD:
    case PROP_VIDEO_DIRECTION:{
      GST_OBJECT_LOCK (fbdevsink);
      g_value_set_enum (value, fbdevsink->method);
      GST_OBJECT_UNLOCK (fbdevsink);
      break;
    }
    case PROP_FORCE_ASPECT_RATIO:{
      g_value_set_boolean (value, fbdevsink->keep_ratio);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_imx_fbdevsink_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  g_return_val_if_fail (GST_IS_IMX_FBDEVSINK (element), GST_STATE_CHANGE_FAILURE);

  GstImxFBDEVSink *fbdevsink = GST_IMX_FBDEVSINK (element);

  GST_DEBUG_OBJECT (fbdevsink, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      fbdevsink->frame_showed = 0;
      fbdevsink->run_time = 0;
      fbdevsink->need_reconfigure = TRUE;
      fbdevsink->var_stored = FALSE;
      fbdevsink->unblanked = FALSE;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      fbdevsink->run_time = gst_element_get_start_time (element);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      {
        if (fbdevsink->run_time > 0) {
          g_print ("Total showed frames (%lld), device %s, playing for (%"GST_TIME_FORMAT"), fps (%.3f).\n",
              fbdevsink->frame_showed, fbdevsink->device, GST_TIME_ARGS (fbdevsink->run_time),
              (gfloat)GST_SECOND * fbdevsink->frame_showed / fbdevsink->run_time);
        }
        
        fbdevsink->frame_showed = 0;
        fbdevsink->run_time = 0;

        if (fbdevsink->last_buffer)
          gst_buffer_unref (fbdevsink->last_buffer);
        fbdevsink->last_buffer = NULL;

        if (fbdevsink->pool) {
          if (gst_buffer_pool_is_active (fbdevsink->pool)) {
            gst_buffer_pool_set_active (fbdevsink->pool, FALSE);
          }
          gst_object_unref (fbdevsink->pool);
          fbdevsink->pool = NULL;
        }

        if (fbdevsink->allocator) {
          gst_object_unref (fbdevsink->allocator);
          fbdevsink->allocator = NULL;
        }
      }
      break;
    default:
      break;
  }
  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (HAS_DCSS()) {
    GST_DEBUG_CATEGORY_INIT (imx_fbdevsink_debug, "imxfbdevsink", 0, "Freescale IMX framebuffer video sink element");
    if (!gst_element_register (plugin, "imxfbdevsink", GST_RANK_NONE,
            GST_TYPE_IMX_FBDEVSINK))
      return FALSE;
  } else {
    return FALSE;
  }

  return TRUE;
}

IMX_GST_PLUGIN_DEFINE (imxfbdevsink, "IMX framebuffer device videosink", plugin_init);

static void
gst_imx_fbdevsink_class_init (GstImxFBDEVSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *basesink_class;
  GstVideoSinkClass *videosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  basesink_class = (GstBaseSinkClass *) klass;
  videosink_class = (GstVideoSinkClass *) klass;

  gobject_class->set_property = gst_imx_fbdevsink_set_property;
  gobject_class->get_property = gst_imx_fbdevsink_get_property;
  gobject_class->finalize = gst_imx_fbdevsink_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_imx_fbdevsink_change_state);

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DEVICE,
      g_param_spec_string ("device", "device",
          "The framebuffer device eg: /dev/fb1", NULL, G_PARAM_READWRITE));

  /* this property is for compatible with gplay rotate interface */
  g_object_class_install_property (gobject_class, PROP_ROTATE_METHOD,
      g_param_spec_enum ("rotate-method",
        "rotate method",
        "get/set the rotation of the video",
        GST_TYPE_IMX_ROTATE_METHOD,
        DEFAULT_IMX_ROTATE_METHOD,
        G_PARAM_READWRITE));
  g_object_class_override_property (gobject_class, PROP_VIDEO_DIRECTION,
      "video-direction");

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
        "Force aspect ratio",
        "When enabled, scaling will respect original aspect ratio",
        TRUE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_imx_fbdevsink_setcaps);
  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_imx_fbdevsink_getcaps);
  basesink_class->propose_allocation = GST_DEBUG_FUNCPTR (gst_imx_fbdevsink_propose_allocation);
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_imx_fbdevsink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_imx_fbdevsink_stop);

  videosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_imx_fbdevsink_show_frame);

  gst_element_class_set_static_metadata (gstelement_class, "IMX fbdev video sink",
      "Sink/Video", "Linux framebuffer videosink for i.Mx",
      IMX_GST_PLUGIN_AUTHOR);

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
}

static void
gst_imx_fbdevsink_finalize (GObject * object)
{
  GstImxFBDEVSink *fbdevsink = GST_IMX_FBDEVSINK (object);
  
  if (fbdevsink->imxoverlay) {
    gst_imx_video_overlay_finalize (fbdevsink->imxoverlay);
    fbdevsink->imxoverlay = NULL;
  }

  g_free (fbdevsink->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}
