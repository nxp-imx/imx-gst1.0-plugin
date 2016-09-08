/*
 * Copyright (c) 2013-2016, Freescale Semiconductor, Inc. All rights reserved.
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

#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <signal.h>

#include "displays.h"
#include "osink_object.h"
#include "imx_2d_device.h"
#include "compositor.h"

GST_DEBUG_CATEGORY_EXTERN (overlay_sink_debug);
#define GST_CAT_DEFAULT overlay_sink_debug

typedef struct {
  gint refcount;
  gint display_count;
  gboolean display_enabled[MAX_DISPLAY];
  DisplayInfo disp_info[MAX_DISPLAY];
  gpointer hdisplay[MAX_DISPLAY];
  Imx2DDevice * hdevice[MAX_DISPLAY];
  gpointer hcompositor[MAX_DISPLAY];
} OSinkHandle;

typedef struct {
  gint display_idx;
  gpointer hsurface;
} OSinkOverlay;

#if 0

#define OSINK_LOCK_NAME "imx_osink_lock"
#define OSINK_SHMEM_NAME "imx_osink_shmem"
#define OSINK_SHMEM_SIZE (3 * sizeof (OSinkHandle))

#define LOCK(lock) \
  do {\
    sem_wait((lock));\
  }while(0)
#define TRY_LOCK(lock) \
  do {\
    sem_trywait((lock));\
  }while(0)
#define UNLOCK(lock) \
  do {\
    sem_post((lock));\
  }while(0)

#define GET_LOCK(val) \
  do { \
    if (NULL == get_lock()) { \
      GST_ERROR ("can't get lock!!!"); \
      return val; \
    } \
  } while(0)

#define OSINK_MAKE_HANDLE(val) \
  do { \
    handle = (OSinkHandle*) osink_handle; \
    if (!handle) { \
      GST_ERROR ("osink object handle is NULL."); \
      return val; \
    } \
  } while(0)

#define OSINK_MAKE_OVERLAY(val) \
  do { \
    hoverlay = (OSinkOverlay*) overlay; \
    if (!hoverlay) { \
      GST_ERROR ("overlay handle is NULL."); \
      return val; \
    } \
  } while(0)

#define CHECK_DISPLAY_INDEX(val) \
  do {  \
    if (display_idx >= handle->display_count) { \
      GST_ERROR ("display index in wrong, max (%d), request (%d).", handle->display_count, display_idx); \
      return val; \
    } \
  } while(0)

static sem_t *glock = NULL;
static OSinkHandle *gosink = NULL;

static sem_t *get_lock()
{
  sem_t *lock = NULL;

  if(!glock) {
    int oflag = O_CREAT;
    umask (0);
    glock = sem_open (OSINK_LOCK_NAME, oflag, 0666, 1);
    if (SEM_FAILED == glock) {
      GST_ERROR ("Can not get lock %s!", OSINK_LOCK_NAME);
      glock = NULL;
    }
  }

  return glock;
}

static void destroy_osink_object()
{
  gint i;
  struct stat shmStat;

  for (i=0; i<gosink->display_count; i++) {
    if (gosink->hdisplay[i]) {
      deinit_display (gosink->hdisplay[i]);
      free_display (gosink->hdisplay[i]);
      gosink->hdisplay[i] = NULL;
    }
    if (gosink->hcompositor[i]) {
      destroy_compositor (gosink->hcompositor[i]);
      gosink->hcompositor[i] = NULL;
    }
    if (gosink->hdevice[i]) {
      comositor_device_close (gosink->hdevice[i]);
      gosink->hdevice[i] = NULL;
    }
    gosink->display_enabled[i] = FALSE;
  }

  //free share memory;
  if (munmap (gosink, OSINK_SHMEM_SIZE) < 0) {
    GST_ERROR ("munmap share memory failed.");
  }

  if (shm_unlink (OSINK_SHMEM_NAME) < 0) {
    GST_ERROR ("unlink share memory failed.");
  }

  gosink = NULL;

  return;
}

static OSinkHandle *create_osink_object()
{
  OSinkHandle *handle = NULL;
  struct stat shmStat;
  static int shmid;
  int ret;

  int oflag = O_CREAT | O_RDWR;
  shmid = shm_open (OSINK_SHMEM_NAME, oflag, 0666);
  if (shmid == -1) {
    GST_ERROR ("Can not get share memory %s!", OSINK_SHMEM_NAME);
    return NULL;
  }

  ret = ftruncate (shmid, (off_t) OSINK_SHMEM_SIZE);
  if (ret < 0) {
    GST_ERROR ("ftruncate shm failed.");
    close(shmid);
    return NULL;
  }

  fstat (shmid, &shmStat);

  handle = (OSinkHandle*) mmap (NULL, shmStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
  if ((handle == NULL) || (handle == MAP_FAILED)) {
    GST_ERROR ("can not mmap share memory %d!", shmid);
    close(shmid);
    return NULL;
  }

  GST_DEBUG ("osink handle refcount (%d)", handle->refcount);

  if (handle->refcount == 0) {
    gint i;
    if (scan_displays (&handle->hdisplay, &handle->display_count) < 0) {
      GST_ERROR ("scan displays failed.");
      destroy_osink_object();
      return NULL;
    }

    for (i=0; i<handle->display_count; i++) {
      handle->display_enabled[i] = FALSE;
      handle->disp_info[i].name = get_display_name (handle->hdisplay[i]);
      handle->disp_info[i].fmt = get_display_format (handle->hdisplay[i]);
      get_display_res (handle->hdisplay[i], &handle->disp_info[i].width, &handle->disp_info[i].height);
    }
  }

  return handle;
}

#endif


// pthread lock

#define LOCK(lock) \
  do {\
    g_mutex_lock((lock));\
  }while(0)
#define UNLOCK(lock) \
  do {\
    g_mutex_unlock((lock));\
  }while(0)

#define GET_LOCK(val) \
  do { \
    if (NULL == get_lock()) { \
      GST_ERROR ("can't get lock!!!"); \
      return val; \
    } \
  } while(0)

#define OSINK_MAKE_HANDLE(val) \
  do { \
    handle = (OSinkHandle*) osink_handle; \
    if (!handle) { \
      GST_ERROR ("osink object handle is NULL."); \
      return val; \
    } \
  } while(0)

#define OSINK_MAKE_OVERLAY(val) \
  do { \
    hoverlay = (OSinkOverlay*) overlay; \
    if (!hoverlay) { \
      GST_ERROR ("overlay handle is NULL."); \
      return val; \
    } \
  } while(0)

#define CHECK_DISPLAY_INDEX(val) \
  do {  \
    if (display_idx >= handle->display_count) { \
      GST_ERROR ("display index in wrong, max (%d), request (%d).", handle->display_count, display_idx); \
      return val; \
    } \
  } while(0)

static GMutex *glock = NULL;
static OSinkHandle *gosink = NULL;

static GMutex *get_lock()
{
  GMutex *lock = NULL;

  if(!glock) {
    glock = g_slice_alloc (sizeof (GMutex));
    if (!glock)
      return NULL;

    g_mutex_init (glock);
  }

  return glock;
}

static void destroy_osink_object()
{
  gint i;
  struct stat shmStat;

  if (!gosink)
    return;

  for (i=0; i<gosink->display_count; i++) {
    if (gosink->hcompositor[i]) {
      destroy_compositor (gosink->hcompositor[i]);
      gosink->hcompositor[i] = NULL;
    }
    if (gosink->hdevice[i]) {
      gosink->hdevice[i]->close(gosink->hdevice[i]);
      imx_2d_device_destroy(gosink->hdevice[i]);
      gosink->hdevice[i] = NULL;
    }
    if (gosink->hdisplay[i]) {
      deinit_display (gosink->hdisplay[i]);
      free_display (gosink->hdisplay[i]);
      gosink->hdisplay[i] = NULL;
    }
    gosink->display_enabled[i] = FALSE;
  }

  g_slice_free1 (sizeof (OSinkHandle), gosink);
  gosink = NULL;

  return;
}

static OSinkHandle *create_osink_object()
{
  OSinkHandle *handle;
  gint i;

  handle = g_slice_alloc (sizeof (OSinkHandle));
  if (!handle) {
    GST_ERROR ("allocate memory for OSinkHandle failed.");
    return NULL;
  }

  memset (handle, 0, sizeof (OSinkHandle));

  if (scan_displays (&handle->hdisplay, &handle->display_count) < 0) {
    GST_ERROR ("scan displays failed.");
    destroy_osink_object();
    return NULL;
  }

  for (i=0; i<handle->display_count; i++) {
    handle->display_enabled[i] = FALSE;
    handle->disp_info[i].name = get_display_name (handle->hdisplay[i]);
    handle->disp_info[i].fmt = get_display_format (handle->hdisplay[i]);
    get_display_res (handle->hdisplay[i], &handle->disp_info[i].width,
        &handle->disp_info[i].height, &handle->disp_info[i].stride);
  }

  return handle;
}

static int osink_object_get_compositor_dst_buffer (gpointer context, SurfaceBuffer *buffer)
{
  gpointer hdisplay = context;

  if (get_next_display_buffer (hdisplay, buffer) < 0) {
    GST_ERROR ("get next display buffer failed.");
    return -1;
  }

  return 0;
}

static int osink_object_flip_compositor_dst_buffer (gpointer context, SurfaceBuffer *buffer)
{
  gpointer hdisplay = context;

  if (flip_display_buffer (hdisplay, buffer) < 0) {
    GST_ERROR ("flip display buffer failed.");
    return -1;
  }

  return 0;
}


//global functions

gpointer osink_object_new ()
{
  GET_LOCK (NULL);

  LOCK (glock);
  if (gosink == NULL) {
    gosink = create_osink_object();
  }
  if (gosink)
    gosink->refcount ++;
  UNLOCK (glock);

  return (gpointer) gosink;
}

void osink_object_ref (gpointer osink_handle)
{
  if (!osink_handle || osink_handle != gosink) {
    GST_ERROR ("osink handle is not correct, current (%p), expected (%p).", osink_handle, gosink);
    return;
  }

  GET_LOCK ();
  LOCK (glock);
  gosink->refcount ++;
  UNLOCK (glock);

  return;
}

void osink_object_unref (gpointer osink_handle)
{
  if (!osink_handle || osink_handle != gosink) {
    GST_ERROR ("osink handle is not correct, current (%p), expected (%p).", osink_handle, gosink);
    return;
  }

  GET_LOCK ();

  LOCK (glock);
  gosink->refcount --;
  if (gosink->refcount == 0) {
    destroy_osink_object ();
    UNLOCK (glock);
    g_mutex_clear (glock);
    g_slice_free1 (sizeof (GMutex), glock);
    glock = NULL;
    GST_DEBUG ("osink object is freed.");
    return;
  }

  UNLOCK (glock);

  return;
}


int osink_object_get_display_count (gpointer osink_handle)
{
  OSinkHandle *handle;
  OSINK_MAKE_HANDLE (-1);

  return handle->display_count;
}

int osink_object_get_display_info (gpointer osink_handle, DisplayInfo *info, gint display_idx)
{
  OSinkHandle *handle;
  OSINK_MAKE_HANDLE (-1);
  CHECK_DISPLAY_INDEX (-1);

  memcpy (info, &handle->disp_info[display_idx], sizeof (DisplayInfo));

  return 0;
}

int osink_object_enable_display (gpointer osink_handle, gint display_idx)
{
  OSinkHandle *handle;
  CompositorDstBufferCb dstbufcb;
  OSINK_MAKE_HANDLE (-1);
  CHECK_DISPLAY_INDEX (-1);
  GET_LOCK (-1);

  GST_DEBUG ("Enable diaplay (%s).", handle->disp_info[display_idx].name);

  LOCK (glock);
  if (handle->display_enabled[display_idx] == FALSE) {
    gint fmt = handle->disp_info[display_idx].fmt;
    GstVideoFormat gst_fmt;
    if (fmt == GST_MAKE_FOURCC('R', 'G', 'B', 'P'))
      gst_fmt = GST_VIDEO_FORMAT_RGB16;
    else if (fmt == GST_MAKE_FOURCC('R', 'G', 'B', 'x')) {
      if (HAS_PXP())
        gst_fmt = GST_VIDEO_FORMAT_BGRx;
      else
        gst_fmt = GST_VIDEO_FORMAT_RGBx;
    } else if (fmt == GST_MAKE_FOURCC('B', 'G', 'R', 'x')) {
      gst_fmt = GST_VIDEO_FORMAT_BGRx;
    } else if (fmt == GST_MAKE_FOURCC('B', 'G', 'R', 'A')) {
      gst_fmt = GST_VIDEO_FORMAT_BGRA;
    } else if (fmt == GST_MAKE_FOURCC('A', 'R', 'G', 'B')) {
      gst_fmt = GST_VIDEO_FORMAT_ARGB;
    } else {
      UNLOCK (glock);
      return -1;
    }

    if (init_display (handle->hdisplay[display_idx]) < 0) {
      GST_ERROR ("init display (%s) failed.", handle->disp_info[display_idx].name);
      UNLOCK (glock);
      return -1;
    }

    Imx2DDevice *dev = imx_2d_device_create(IMX_2D_DEVICE_TYPE_USED);
    if (dev == NULL) {
      GST_ERROR ("create device failed.");
      deinit_display (handle->hdisplay[display_idx]);
      UNLOCK (glock);
      return -1;
    }

    if (dev->open(dev) < 0) {
      GST_ERROR ("create device %d failed.", IMX_2D_DEVICE_TYPE_USED);
      deinit_display (handle->hdisplay[display_idx]);
      imx_2d_device_destroy(dev);
      UNLOCK (glock);
      return -1;
    }

    Imx2DVideoInfo out_info;
    out_info.fmt = gst_fmt;
    out_info.w = handle->disp_info[display_idx].width;
    out_info.h = handle->disp_info[display_idx].height;
    out_info.stride = handle->disp_info[display_idx].stride;
    dev->config_output(dev, &out_info);


    dstbufcb.context = handle->hdisplay[display_idx];
    dstbufcb.get_dst_buffer = osink_object_get_compositor_dst_buffer;
    dstbufcb.flip_dst_buffer = osink_object_flip_compositor_dst_buffer;
    handle->hcompositor[display_idx] = create_compositor (dev, &dstbufcb);
    if (handle->hcompositor[display_idx] == NULL) {
      GST_ERROR ("create compositor for display (%s) failed.", handle->disp_info[display_idx].name);
      deinit_display (handle->hdisplay[display_idx]);
      dev->close(dev);
      imx_2d_device_destroy(dev);
      UNLOCK (glock);
      return -1;
    }

    handle->hdevice[display_idx] = dev;
    handle->display_enabled[display_idx] = TRUE;
  }

  UNLOCK (glock);

  return 0;
}

gpointer osink_object_create_overlay (gpointer osink_handle, guint display_idx, SurfaceInfo *surface_info)
{
  OSinkHandle *handle;
  OSinkOverlay *hoverlay;

  OSINK_MAKE_HANDLE (NULL);
  CHECK_DISPLAY_INDEX (NULL);

  GST_DEBUG ("create overlay, format (%x), source (%d,%d), crop (%d,%d) --> (%d,%d), dest (%d,%d) --> (%d,%d), rot (%d), zorder (%d)",
      surface_info->fmt, surface_info->src.width, surface_info->src.height,
      surface_info->src.left, surface_info->src.top, surface_info->src.right, surface_info->src.bottom,
      surface_info->dst.left, surface_info->dst.top, surface_info->dst.right, surface_info->dst.bottom,
      surface_info->rot, surface_info->zorder);

  hoverlay = g_slice_alloc (sizeof(OSinkOverlay));
  if (!hoverlay) {
    GST_ERROR ("allocate memory for OSinkOverlay failed.");
    return NULL;
  }

  hoverlay->display_idx = display_idx;

  GET_LOCK (NULL);
  LOCK (glock);
  hoverlay->hsurface = compositor_add_surface (handle->hcompositor[display_idx], surface_info);
  UNLOCK (glock);

  if (!hoverlay->hsurface) {
    GST_ERROR ("compositor_add_surface failed.");
    g_slice_free1 (sizeof(OSinkOverlay), hoverlay);
    return NULL;
  }

  GST_DEBUG ("create overlay (%p).", hoverlay);

  return hoverlay;
}

void osink_object_destroy_overlay (gpointer osink_handle, gpointer overlay)
{
  OSinkHandle *handle;
  OSinkOverlay *hoverlay;
  OSINK_MAKE_HANDLE ();
  OSINK_MAKE_OVERLAY ();
  GET_LOCK ();

  GST_DEBUG ("destroy overlay (%p).", hoverlay);

  LOCK (glock);
  compositor_remove_surface (handle->hcompositor[hoverlay->display_idx], hoverlay->hsurface);
  g_slice_free1 (sizeof(OSinkOverlay), hoverlay);
  UNLOCK (glock);

  return;
}

int osink_object_config_overlay (gpointer osink_handle, gpointer overlay, SurfaceInfo *surface_info)
{
  gint ret;
  OSinkHandle *handle;
  OSinkOverlay *hoverlay;

  OSINK_MAKE_HANDLE (-1);
  OSINK_MAKE_OVERLAY (-1);
  GET_LOCK (-1);

  GST_DEBUG ("config overlay (%p), format (%x), source (%d,%d), crop (%d,%d) --> (%d,%d), dest (%d,%d) --> (%d,%d), rot (%d)",
      hoverlay, surface_info->fmt, surface_info->src.width, surface_info->src.height,
      surface_info->src.left, surface_info->src.top, surface_info->src.right, surface_info->src.bottom,
      surface_info->dst.left, surface_info->dst.top, surface_info->dst.right, surface_info->dst.bottom,
      surface_info->rot);

  LOCK (glock);
  ret = compositor_config_surface (handle->hcompositor[hoverlay->display_idx], hoverlay->hsurface, surface_info);
  if (compositor_check_need_clear_display (handle->hcompositor[hoverlay->display_idx])) {
    clear_display (handle->hdisplay[hoverlay->display_idx]);
  }
  UNLOCK (glock);

  return ret;
}

int osink_object_update_overlay (gpointer osink_handle, gpointer overlay, SurfaceBuffer *buffer)
{
  gint ret;
  OSinkHandle *handle;
  OSinkOverlay *hoverlay;

  OSINK_MAKE_HANDLE (-1);
  OSINK_MAKE_OVERLAY (-1);
  GET_LOCK (-1);

  LOCK (glock);
  if (compositor_update_surface (handle->hcompositor[hoverlay->display_idx], hoverlay->hsurface, buffer) < 0) {
    GST_ERROR ("compositor update surface (%p) failed.", hoverlay);
    UNLOCK (glock);
    return -1;
  }

  UNLOCK (glock);

  return 0;
}

gint64 osink_object_get_overlay_showed_frames (gpointer osink_handle, gpointer overlay)
{
  OSinkHandle *handle;
  OSinkOverlay *hoverlay;

  OSINK_MAKE_HANDLE (-1);
  OSINK_MAKE_OVERLAY (-1);

  return compositor_get_surface_showed_frames (handle->hcompositor[hoverlay->display_idx], hoverlay->hsurface);
}

int osink_object_allocate_memory (gpointer osink_handle, PhyMemBlock *memblk)
{
  gint ret = -1;
  OSinkHandle *handle;
  OSINK_MAKE_HANDLE (-1);
  GET_LOCK (-1);

  LOCK (glock);

  guint dev_num = 0;
  for (; dev_num < MAX_DISPLAY; dev_num++) {
    if (handle->hdevice[dev_num])
      break;
  }
  if ((dev_num < MAX_DISPLAY) && handle->hdevice[dev_num])
    ret = handle->hdevice[dev_num]->alloc_mem(handle->hdevice[dev_num], memblk);

  UNLOCK (glock);

  if (ret >= 0)
    osink_object_ref (osink_handle);

  GST_DEBUG ("allocate memory, vaddr (%p), paddr (%p).", memblk->vaddr, memblk->paddr);

  return ret;
}

int osink_object_free_memory (gpointer osink_handle, PhyMemBlock *memblk)
{
  gint ret = -1;
  OSinkHandle *handle;
  OSINK_MAKE_HANDLE (-1);
  GET_LOCK (-1);

  GST_DEBUG ("free memory, vaddr (%p), paddr (%p).", memblk->vaddr, memblk->paddr);

  LOCK (glock);

  guint dev_num = 0;
  for (; dev_num < MAX_DISPLAY; dev_num++) {
    if (handle->hdevice[dev_num])
      break;
  }
  if ((dev_num < MAX_DISPLAY) && handle->hdevice[dev_num])
    ret = handle->hdevice[dev_num]->free_mem(handle->hdevice[dev_num], memblk);

  UNLOCK (glock);

  osink_object_unref (osink_handle);

  return ret ;
}

int osink_object_copy_memory (gpointer osink_handle, PhyMemBlock *dst_mem,
    PhyMemBlock *src_mem, guint offset, guint size)
{
  gint ret = -1;
  OSinkHandle *handle;
  OSINK_MAKE_HANDLE (-1);
  GET_LOCK (-1);

  LOCK (glock);
  if (handle->hdevice[0])
    ret = handle->hdevice[0]->copy_mem(handle->hdevice[0],
                                       dst_mem, src_mem, offset, size);
  UNLOCK (glock);

  if (ret >= 0)
    osink_object_ref (osink_handle);

  GST_DEBUG ("copy memory, vaddr (%p), paddr (%p), size (%d) "
      "to memory vaddr (%p), paddr (%p), size (%d)",
      src_mem->vaddr, src_mem->paddr, src_mem->size,
      dst_mem->vaddr, dst_mem->paddr, dst_mem->size);

  return ret ;
}

void osink_object_set_global_alpha(gpointer osink_handle, gint display_idx, gint alpha)
{
  OSinkHandle *handle;
  OSINK_MAKE_HANDLE ();
  GET_LOCK ();

  GST_DEBUG ("set global alpha, (%d), (%d).", display_idx, alpha);
  LOCK (glock);
  set_global_alpha(handle->hdisplay[display_idx], alpha);
  UNLOCK (glock);
}

void osink_object_set_color_key(gpointer osink_handle, gint display_idx, gboolean enable, guint colorkey)
{
  OSinkHandle *handle;
  OSINK_MAKE_HANDLE ();
  GET_LOCK ();

  GST_DEBUG ("set global alpha, (%d), (%d). (%08x)", display_idx, enable, colorkey);
  LOCK (glock);
  set_color_key(handle->hdisplay[display_idx], enable, colorkey);
  UNLOCK (glock);
}
