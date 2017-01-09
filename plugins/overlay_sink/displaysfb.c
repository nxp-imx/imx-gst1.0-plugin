/*
 * Copyright (c) 2013-2016, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2017 NXP
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
#ifndef USE_FB_API
#include <linux/mxcfb.h>
#endif

#include "gstimxcommon.h"
#include "displays.h"
#include "gstsutils.h"
#include "gstimxv4l2.h"

#define RGB888TORGB565(rgb)\
    ((((rgb)<<8)>>27<<11)|(((rgb)<<18)>>26<<5)|(((rgb)<<27)>>27))

#define RGB565TOCOLORKEY(rgb)                              \
      ( ((rgb & 0xf800)<<8)  |  ((rgb & 0xe000)<<3)  |     \
        ((rgb & 0x07e0)<<5)  |  ((rgb & 0x0600)>>1)  |     \
        ((rgb & 0x001f)<<3)  |  ((rgb & 0x001c)>>2)  )

GST_DEBUG_CATEGORY_EXTERN (overlay_sink_debug);
#define GST_CAT_DEFAULT overlay_sink_debug

#define KEY_DEVICE "device"
#define KEY_BG_DEVICE "bg_device"
#define KEY_FMT "fmt"
#define KEY_WIDTH "width"
#define KEY_HEIGHT "height"
#define KEY_COLOR_KEY "color_key"
#define KEY_ALPHA "alpha"

#define DEFAULTW (320)
#define DEFAULTH (240)

#define DISPLAY_NUM_BUFFERS (3)

#define PAGE_SHIFT      12
#ifndef PAGE_SIZE
#define PAGE_SIZE       (1 << PAGE_SHIFT)
#endif
#define ROUNDUP2PAGESIZE(x)  (((x) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))

// handle for FB display
typedef struct {
  gchar *name;
  gchar *device;
  gchar *bg_device;
  gint fmt;
  gint w;
  gint h;
  gint s;
  guint alpha;
  gboolean enable_color_key;
  guint color_key;

  int fd;
  void *paddr[DISPLAY_NUM_BUFFERS];
  gint panidx;
  void *vaddr;
  gint fb_size;
} DisplayHandle;

static guint string_to_fmt (char *value)
{
  guint fmt, a, b, c, d;

  a = value[0];
  b = value[1];
  c = value[2];
  d = value[3];

  fmt = (((a) << 0) | ((b) << 8) | ((c) << 16) | ((d) << 24));

  return fmt;
}

static gint get_display_resolution (gchar *device, gint *w, gint *h, gint *s)
{
  struct fb_var_screeninfo fb_var;
  struct fb_fix_screeninfo fb_fix;

  int fd = open (device, O_RDWR, 0);
  if (fd < 0) {
    GST_ERROR ("Can't open %s.", device);
    return -1;
  }

  if (ioctl (fd, FBIOGET_VSCREENINFO, &fb_var) < 0) {
    GST_ERROR ("FBIOGET_VSCREENINFO from %s failed.", device);
    close (fd);
    return -1;
  }

  if (ioctl(fd, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
    GST_ERROR ("FBIOGET_FSCREENINFO from %s failed.", device);
    close (fd);
    return -1;
  }


  *w = fb_var.xres;
  *h = fb_var.yres;
  *s = fb_fix.line_length;

  close (fd);

  return 0;
}

static void set_display_alpha (gchar *device, gint alpha)
{
#ifndef USE_FB_API
  struct mxcfb_gbl_alpha galpha;
  int fd = open (device, O_RDWR, 0);
  if (fd) {
    galpha.alpha = alpha;
    galpha.enable = 1;
    GST_DEBUG ("set global alpha to (%d) for display (%s)", alpha, device);
    ioctl(fd, MXCFB_SET_GBL_ALPHA, &galpha);
    close (fd);
  }
#endif
  return;
}

static void set_display_color_key (gchar *device, gboolean enable, gint color_key)
{
#ifndef USE_FB_API
  struct mxcfb_color_key colorKey;
  struct fb_var_screeninfo fbVar;

  int fd = open (device, O_RDWR, 0);
  if (fd) {
    if (ioctl(fd, FBIOGET_VSCREENINFO, &fbVar) < 0) {
      GST_ERROR("get vscreen info failed.\n");
    } else {
      if (fbVar.bits_per_pixel == 16) {
        colorKey.color_key = RGB565TOCOLORKEY(RGB888TORGB565(color_key));
        GST_DEBUG("%08X:%08X\n", colorKey.color_key, color_key);
      } else if (fbVar.bits_per_pixel == 24 || fbVar.bits_per_pixel == 32) {
        colorKey.color_key = color_key;
      }
    }

    if (enable) {
      colorKey.enable = 1; 
    GST_DEBUG ("set colorKey to (%x) for display (%s)", colorKey.color_key, device);
    } else {
      colorKey.enable = 0; 
      GST_DEBUG ("disable colorKey for display (%s)", device);
    }
    ioctl (fd, MXCFB_SET_CLR_KEY, &colorKey);
    close (fd);
  }
#endif
}

gint scan_displays(gpointer **phandle, gint *pcount)
{
  GstsutilsEntry *entry = NULL;
  CHIP_CODE chipcode = imx_chip_code();
  switch (chipcode) {
    case CC_MX8:
      entry = gstsutils_init_entry ("/usr/share/imx_8dv_display_config");
    break;
    case CC_MX7ULP:
      entry = gstsutils_init_entry ("/usr/share/imx_7ulp_display_config");
    break;
    default:
    break;
  }

  gint group_count;
  gint i;
  gint count = 0;

  if(entry == NULL) {
    GST_ERROR ("scan display configuration file failed.");
    return -1;
  }

  group_count = gstsutils_get_group_count (entry);
  GST_DEBUG ("osink config group count (%d)", group_count);
  for (i=1; i<=group_count; i++) {
    GstsutilsGroup *group;
    DisplayHandle *hdisplay;
    gchar *name = NULL;
    gchar *device, *bg_device;
    gchar *fmt, *w, *h, *color_key, *alpha;

    bg_device = color_key = w = h = NULL;

    if(!gstsutils_get_group_by_index(entry, i, &group)) {
      GST_ERROR ("gstsutils_get_group_by_index failed.\n");
      continue;
    }

    name = gstsutils_get_group_name (group); 
    if (!name)
      name = "unknown";

    if(!gstsutils_get_value_by_key(group, KEY_DEVICE, &device)) {
      GST_ERROR ("can find display %s device.", name);
      continue;
    }
    if(!gstsutils_get_value_by_key(group, KEY_BG_DEVICE, &bg_device)) {
      GST_DEBUG ("No background device for %s.", name);
    }
    if(!gstsutils_get_value_by_key(group, KEY_FMT, &fmt)) {
      fmt = g_strdup("RGBP"); //default rgb565
    }
    if(!gstsutils_get_value_by_key(group, KEY_WIDTH, &w) && !bg_device) {
      GST_ERROR ("can't find display %s width.", name);
      continue;
    }
    if(!gstsutils_get_value_by_key(group, KEY_HEIGHT, &h) && !bg_device) {
      GST_ERROR ("can't find display %s height.", name);
      continue;
    }

    if(!gstsutils_get_value_by_key(group, KEY_ALPHA, &alpha))
      alpha = g_strdup("0");

    gstsutils_get_value_by_key(group, KEY_COLOR_KEY, &color_key);

    hdisplay = g_slice_alloc (sizeof(DisplayHandle));
    if (!hdisplay) {
      GST_ERROR ("allocate DisplayHandle for display %s failed.", name);
      return -1;
    }
    memset (hdisplay, 0, sizeof (DisplayHandle));

    hdisplay->name = name;
    hdisplay->device = device;
    hdisplay->bg_device = bg_device;
    hdisplay->fmt = string_to_fmt (fmt);
    hdisplay->alpha = atoi (alpha);
    g_free(fmt);
    g_free(alpha);
    if (color_key) {
      hdisplay->enable_color_key = TRUE;
      hdisplay->color_key = atoi (color_key);
      g_free(color_key);
    }
    else {
      hdisplay->enable_color_key = FALSE;
    }

    if (bg_device && (!w || !h)) {
      if (get_display_resolution (bg_device, &hdisplay->w, &hdisplay->h,
            &hdisplay->s) < 0) {
        hdisplay->w = DEFAULTW;
        hdisplay->h = DEFAULTH;
      }
    }
    else {
      hdisplay->w = atoi (w);
      hdisplay->h = atoi (h);
      g_free(w);
      g_free(h);
    }

    GST_DEBUG ("display(%s), device(%s), fmt(%d), res(%dx%d), stride(%d), ckey(%x), alpha(%d)",
        hdisplay->name, hdisplay->device, hdisplay->fmt, hdisplay->w, hdisplay->h, 
        hdisplay->s, hdisplay->color_key, hdisplay->alpha);

    phandle[count] = hdisplay;
    count ++;
    if (count >= MAX_DISPLAY)
      break;
  }

  gstsutils_deinit_entry (entry);
  *pcount = count;

  return 0;
}

void free_display (gpointer display)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;

  if (hdisplay) {
    if (hdisplay->name)
      g_free (hdisplay->name);
    if (hdisplay->device)
      g_free (hdisplay->device);
    if (hdisplay->bg_device)
      g_free (hdisplay->bg_device);
    g_slice_free1 (sizeof(DisplayHandle), hdisplay);
  }
}

gchar *get_display_name(gpointer display)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;

  return hdisplay->name;
}

gint get_display_format(gpointer display)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;

  return hdisplay->fmt;
}

gint get_display_res(gpointer display, gint *width, gint *height, gint *stride)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;

  *width = hdisplay->w;
  *height = hdisplay->h;
  *stride = hdisplay->s;

  return 0;
}


// fb display implementation
gint init_display (gpointer display)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;

  hdisplay->fd = open (hdisplay->device, O_RDWR, 0);
  if (hdisplay->fd <= 0) {
    GST_ERROR ("failed to open device %s", hdisplay->device);
    return -1;
  }

  struct fb_var_screeninfo fb_var;
  if (ioctl(hdisplay->fd, FBIOGET_VSCREENINFO, &fb_var) < 0) {
    return -1;
  }

  fb_var.xoffset = 0;
  fb_var.xres = hdisplay->w;
  fb_var.xres_virtual = hdisplay->w;
  fb_var.yoffset = 0;
  fb_var.yres = hdisplay->h;
  fb_var.yres_virtual = hdisplay->h * DISPLAY_NUM_BUFFERS;
  fb_var.activate |= FB_ACTIVATE_FORCE;

  /* FIXME:imx7ulp use grayscale field for format setting, the below
   * setting cannot take effect in display driver on 7ulp, the format
   * is still default ARGB. The hdisplay->fmt will be BGRA for g2d
   * output to workaround the issue that format ARGB of g2d is not
   * consistent with the display driver of 7ulp */
  fb_var.nonstd = hdisplay->fmt;

  if (hdisplay->fmt == GST_MAKE_FOURCC('R', 'G', 'B', 'x')
      || hdisplay->fmt == GST_MAKE_FOURCC('B', 'G', 'R', 'x')
      || hdisplay->fmt == GST_MAKE_FOURCC('B', 'G', 'R', 'A')
      || hdisplay->fmt == GST_MAKE_FOURCC('A', 'R', 'G', 'B')) {
    fb_var.bits_per_pixel = 32;
  } else if (hdisplay->fmt == GST_MAKE_FOURCC('R', 'G', 'B', 'P')) {
    fb_var.bits_per_pixel = 16;
  } else {
    GST_ERROR ("Unsupported display format, check the display config file.");
    return -1;
  }

  if (ioctl(hdisplay->fd, FBIOPUT_VSCREENINFO, &fb_var) < 0) {
    return -1;
  }

  struct fb_fix_screeninfo fb_fix;
  if (ioctl(hdisplay->fd, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
    return -1;
  }

  int i;
  for (i = 0; i < DISPLAY_NUM_BUFFERS; i++) {
    hdisplay->paddr[i] = (void *) (fb_fix.smem_start + fb_var.yres * fb_fix.line_length * i);
  }
  hdisplay->panidx = 0;


  hdisplay->fb_size = ROUNDUP2PAGESIZE (fb_fix.line_length * fb_var.yres_virtual);
  hdisplay->vaddr = mmap(0, hdisplay->fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, hdisplay->fd, 0);
  if (hdisplay->vaddr <= 0) {
    GST_ERROR ("Get framebuffer vaddr failed.");
    return -1;
  }

  set_display_alpha (hdisplay->bg_device, hdisplay->alpha);
  set_display_color_key (hdisplay->bg_device, hdisplay->enable_color_key, hdisplay->color_key);

  clear_display (display);

  ioctl(hdisplay->fd, FBIOBLANK, FB_BLANK_UNBLANK);

  return 0;
}

void deinit_display (gpointer display)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;

  // FIXME: set alpha to 0 to workaround for can't close overlay.
  if(hdisplay->fmt == GST_MAKE_FOURCC('A', 'R', 'G', 'B')) {
    memset (hdisplay->vaddr, 0, hdisplay->fb_size);
  }

  if (hdisplay->fd) {
    /* Don't close screen if the background device and foreground device are the same */
    if (hdisplay->bg_device && strcmp(hdisplay->device, hdisplay->bg_device) != 0) {
      ioctl(hdisplay->fd, FBIOBLANK, FB_BLANK_NORMAL);
    }
    munmap(hdisplay->vaddr, hdisplay->fb_size);
    close (hdisplay->fd);
    hdisplay->fd = 0;
  }

  return;
}

gint clear_display (gpointer display)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;
  gint32 *framebuffer = hdisplay->vaddr;
  gint32 alpha_pixel = 0;
  gint i;

  if(hdisplay->fmt == GST_MAKE_FOURCC('A', 'R', 'G', 'B')) {
    alpha_pixel = 0x000000FF;
  } else if (hdisplay->fmt == GST_MAKE_FOURCC('B', 'G', 'R', 'A')) {
    alpha_pixel = 0xFF000000;
  }

  for (i=0; i<hdisplay->fb_size / 4; i++)
    framebuffer[i] = alpha_pixel;

  return 0;
}

gint get_next_display_buffer (gpointer display, SurfaceBuffer *buffer)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;

  buffer->mem.paddr = hdisplay->paddr[hdisplay->panidx];
  buffer->mem.vaddr = hdisplay->vaddr + hdisplay->s * hdisplay->h * hdisplay->panidx;
  buffer->mem.size = hdisplay->s * hdisplay->h;

  GST_DEBUG ("get display buffer, vaddr (%p) paddr (%p)", buffer->mem.vaddr, buffer->mem.paddr);

  return 0;
}

gint flip_display_buffer (gpointer display, SurfaceBuffer *buffer)
{
  struct fb_var_screeninfo fb_var;
  DisplayHandle *hdisplay = (DisplayHandle*) display;

  ioctl (hdisplay->fd, FBIOGET_VSCREENINFO, &fb_var);
  fb_var.yoffset = fb_var.yres * hdisplay->panidx;
  ioctl (hdisplay->fd, FBIOPAN_DISPLAY, &fb_var);
  hdisplay->panidx = (hdisplay->panidx + 1) % DISPLAY_NUM_BUFFERS;

  return 0;
}

void set_global_alpha(gpointer display, gint alpha)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;
  if (hdisplay && hdisplay->bg_device)
    set_display_alpha (hdisplay->bg_device, alpha);
}

void set_color_key(gpointer display, gboolean enable, guint colorkey)
{
  DisplayHandle *hdisplay = (DisplayHandle*) display;
  if (hdisplay && hdisplay->bg_device)
    set_display_color_key (hdisplay->bg_device, enable, colorkey);
}
