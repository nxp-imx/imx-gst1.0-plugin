/*
 * Copyright (c) 2013-2014, Freescale Semiconductor, Inc. All rights reserved.
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

#include <string.h>
#include <gst/video/gstvideosink.h>
#include "compositor.h"

GST_DEBUG_CATEGORY_EXTERN (overlay_sink_debug);
#define GST_CAT_DEFAULT overlay_sink_debug

#define RECTA_SMALLER_RECTB(a,b) \
  (((a)->left >= (b)->left) && ((a)->right <= (b)->right) \
   && ((a)->top >= (b)->top) && ((a)->bottom <= (b)->bottom))

#define RECTA_EQUAL_RECTB(a,b) \
  (((a)->left == (b)->left) && ((a)->right == (b)->right) \
   && ((a)->top == (b)->top) && ((a)->bottom == (b)->bottom))

#define RECTA_NOT_OVERLAY_RECTB(a,b) \
  (((a)->right <= (b)->left) || ((a)->left >= (b)->right) \
   || ((a)->bottom <= (b)->top) || ((a)->top >= (b)->bottom))

#define COMPOSITOR_WAIT_SURFACE(h) (g_cond_wait (&((h)->cond), &((h)->lock)))
#define COMPOSITOR_POST_SURFACE(h) (g_cond_signal (&((h)->cond)))

typedef struct _Surface Surface;
struct _Surface{
  SurfaceInfo info;
  SurfaceBuffer buffer;
  gboolean update;
  gboolean single;
  gboolean hide;
  gpointer hdevice_surface;
  gint64 blited_frames;

  Surface *prev;
  Surface *next;
};

typedef struct {
  gpointer hdevice;
  CompositorDstBufferCb dst_buffer_cb;
  GThread *thread;
  GCond cond;
  GMutex lock;
  gboolean running;

  Surface *head;
  Surface *tail;
  PhyMemBlock prvdst;
} CompositorHandle;

static void
compositor_insert_surface (CompositorHandle *hcompositor, Surface *hsurface)
{
  Surface *list;

  if (hcompositor->head == NULL && hcompositor->tail == NULL) {
    hcompositor->head = hcompositor->tail = hsurface;
    return;
  }

  list = hcompositor->head;
  while (list) {
    if (list->info.zorder <= hsurface->info.zorder) {
      list = list->next;
      continue;
    }
    else {
      break;
    }
  }

  if (list == hcompositor->head) {
    //need add to the head
    hsurface->next = hcompositor->head;
    hcompositor->head->prev = hsurface;
    hcompositor->head = hsurface;
    return;
  }

  if (!list) {
    hsurface->prev = hcompositor->tail;
    hcompositor->tail->next = hsurface;
    hcompositor->tail = hsurface;
    return;
  }

  hsurface->prev = list->prev;
  hsurface->next = list;
  list->prev->next = hsurface;
  list->prev = hsurface;

  return;
}

static void
compositor_check_surface_draw_area (CompositorHandle *hcompositor, Surface *hsurface)
{
  Surface *list;
  DestRect *srect, *lrect;
  gboolean upper = TRUE;

  srect = &hsurface->info.dst;
  list = hcompositor->tail;
  while (list) {
    if (hsurface == list) {
      list = list->prev;
      upper = FALSE;
      continue;
    }

    lrect = &list->info.dst;
    if (hsurface->single) {
      if (!RECTA_NOT_OVERLAY_RECTB (srect, lrect)) {
        hsurface->single = FALSE;
      }
      else {
        list = list->prev;
        continue;
      }
    }

    if (!hsurface->hide) {
      if (upper && RECTA_SMALLER_RECTB (srect, lrect)) {
        hsurface->hide = TRUE;
        break;
      }
    }

    list = list->prev;
  }

  return;
}

static void
compositor_check_list_draw_area (CompositorHandle *hcompositor)
{
  Surface *list;
  gint i = 0;

  list = hcompositor->head;
  while (list) {
    GST_DEBUG ("surface list node%d %p", i++, list);
    list->single = TRUE;
    list->hide = FALSE;
    list->update = TRUE;
    compositor_check_surface_draw_area (hcompositor, list);
    list = list->next;
  }

  GST_DEBUG ("surface list head (%p), tail (%p)", hcompositor->head, hcompositor->tail);

  return;
}

static void
compositor_check_keep_ratio (CompositorHandle *hcompositor, Surface *hsurface, SurfaceInfo *surface_info)
{
  GstVideoRectangle src, dest, result;
  gboolean brotate = (surface_info->rot == 90 || surface_info->rot == 270) ? TRUE : FALSE; 

  src.x = src.y = 0;
  src.w = surface_info->src.right - surface_info->src.left;
  src.h = surface_info->src.bottom - surface_info->src.top;
  dest.x = dest.y = 0;
  dest.w = surface_info->dst.right - surface_info->dst.left;
  dest.h = surface_info->dst.bottom - surface_info->dst.top;
  if (brotate) {
    gint tmp = dest.w;
    dest.w = dest.h;
    dest.h = tmp;
  }

  gst_video_sink_center_rect (src, dest, &result, TRUE);

  if (brotate) {
    hsurface->info.dst.left = result.y + surface_info->dst.left;
    hsurface->info.dst.top = result.x + surface_info->dst.top;
    hsurface->info.dst.right = hsurface->info.dst.left + result.h;
    hsurface->info.dst.bottom = hsurface->info.dst.top + result.w;
  }
  else {
    hsurface->info.dst.left = result.x + surface_info->dst.left;
    hsurface->info.dst.top = result.y + surface_info->dst.top;
    hsurface->info.dst.right = hsurface->info.dst.left + result.w;
    hsurface->info.dst.bottom = hsurface->info.dst.top + result.h;
  }

  return;
}

static gpointer compositor_do_compositing_surface_list (CompositorHandle *hcompositor)
{
  gpointer hdevice;
  Surface *surface, *list;
  SurfaceBuffer dstbuf;
  gboolean upper;

  if (hcompositor->dst_buffer_cb.get_dst_buffer (hcompositor->dst_buffer_cb.context, &dstbuf) < 0) {
    GST_ERROR ("compositor dst buffer is invalid vaddr(%p), paddr(%p)", dstbuf.vaddr, dstbuf.paddr);
    return;
  }

  if (hcompositor->head != hcompositor->tail) {
    // need to back copy dest buffer for 2 more surfaces
    if (hcompositor->prvdst.vaddr)
      compositor_device_copy (hcompositor->hdevice, &dstbuf, &hcompositor->prvdst);
  }

  upper = FALSE;
  list = hcompositor->head;
  while (list) {
    surface = list;
    list = list->next;

    if (surface->hide) {
      GST_DEBUG ("surface is hide");
      continue;
    }

    if (surface->single && !surface->update) {
      GST_DEBUG ("Surface is single, but not updated.");
      continue;
    }

    if (surface->update && !surface->single) {
      upper = TRUE;
    }

    if (surface->update || upper) {
      GST_DEBUG ("blit surface (%p)", surface);
      if (compositor_device_blit_surface (hcompositor->hdevice, surface->hdevice_surface, 
            &surface->buffer, &dstbuf) < 0) {
        GST_ERROR ("composite surface (%p) failed.", surface);
        break;
      }
    }

    surface->blited_frames ++;
    surface->update = FALSE;
  }

  if (hcompositor->dst_buffer_cb.flip_dst_buffer (hcompositor->dst_buffer_cb.context, &dstbuf) < 0) {
    GST_ERROR ("compositor flit dst buffer failed.");
  }

  memcpy (&hcompositor->prvdst, &dstbuf, sizeof (SurfaceBuffer));

  return;
}

static gpointer compositor_compositing_thread (gpointer compositor)
{
  CompositorHandle *hcompositor = (CompositorHandle*) compositor;

  while (hcompositor->running) {
    COMPOSITOR_WAIT_SURFACE (hcompositor);
    compositor_do_compositing_surface_list (hcompositor);
  }

  GST_DEBUG ("compositor thread exit");

  return;
}


// global functions

gpointer create_compositor(gpointer device, CompositorDstBufferCb *pcallback)
{
  CompositorHandle *hcompositor = NULL;

  hcompositor = g_slice_alloc (sizeof(CompositorHandle));
  if (!hcompositor) {
    GST_ERROR ("allocate memory for CompositorHandle failed.");
    return NULL;
  }

  memset (hcompositor, 0, sizeof(CompositorHandle));
  hcompositor->hdevice = device;
  hcompositor->dst_buffer_cb.context = pcallback->context;
  hcompositor->dst_buffer_cb.get_dst_buffer = pcallback->get_dst_buffer;
  hcompositor->dst_buffer_cb.flip_dst_buffer = pcallback->flip_dst_buffer;
  g_mutex_init (&hcompositor->lock);
  g_cond_init (&hcompositor->cond);
  hcompositor->running = TRUE;
  hcompositor->thread = g_thread_new ("compositor thread", compositor_compositing_thread, hcompositor);
  if (!hcompositor->thread) {
    GST_ERROR ("Create compositor thread failed.");
    destroy_compositor (hcompositor);
    return NULL;
  }

  return hcompositor;
}

void destroy_compositor(gpointer compositor)
{
  CompositorHandle *hcompositor = (CompositorHandle*) compositor;

  if (!hcompositor) {
    GST_ERROR ("invalid parameter.");
    return;
  }

  if (hcompositor->thread) {
    hcompositor->running = FALSE;
    COMPOSITOR_POST_SURFACE (hcompositor);
    g_thread_join (hcompositor->thread);
    hcompositor->thread = NULL;
  }
  g_mutex_clear (&hcompositor->lock);
  g_cond_clear (&hcompositor->cond);
  g_slice_free1 (sizeof(CompositorHandle), hcompositor);

  return;
}

gpointer compositor_add_surface (gpointer compositor, SurfaceInfo *surface_info)
{
  CompositorHandle *hcompositor = (CompositorHandle*) compositor;
  Surface *hsurface = NULL;

  if (!hcompositor) {
    GST_ERROR ("invalid parameter.");
    return NULL;
  }
  
  hsurface = (Surface *) g_slice_alloc (sizeof(Surface));
  if (!hsurface) {
    GST_ERROR ("allocate memory for Surface failed.");
    return NULL;
  }

  memset (hsurface, 0, sizeof(Surface));
  memcpy (&hsurface->info, surface_info, sizeof(SurfaceInfo));
  hsurface->hdevice_surface = compositor_device_create_surface (hcompositor->hdevice, surface_info);
  if (!hsurface->hdevice_surface) {
    GST_ERROR ("compositor_device_create_surface failed.");
    g_slice_free1 (sizeof(Surface), hsurface);
    return NULL;
  }

  GST_DEBUG ("add surface %p", hsurface);
  compositor_insert_surface (hcompositor, hsurface);
  compositor_config_surface (hcompositor, hsurface, surface_info);

  return hsurface;
}

gint compositor_remove_surface (gpointer compositor, gpointer surface)
{
  CompositorHandle *hcompositor = (CompositorHandle*) compositor;
  Surface *hsurface = (Surface *) surface;

  if (!hcompositor || !hsurface) {
    GST_ERROR ("invalid parameter.");
    return -1;
  }

  if (hsurface == hcompositor->head && hsurface == hcompositor->tail) {
    hcompositor->head = hcompositor->tail = NULL;
  }
  else if (hsurface == hcompositor->head) {
    hcompositor->head = hsurface->next;
    hcompositor->head->prev = NULL;
    if (hcompositor->head->next == NULL) {
      hcompositor->tail = hcompositor->head;
    }
  }
  else if (hsurface == hcompositor->tail) {
    hcompositor->tail = hsurface->prev;
    hcompositor->tail->next = NULL;
    if (hcompositor->tail->prev == NULL) {
      hcompositor->head = hcompositor->tail;
    }
  }
  else {
    hsurface->prev->next = hsurface->next;
    hsurface->next->prev = hsurface->prev;
  }

  compositor_device_destroy_surface (hcompositor->hdevice, hsurface->hdevice_surface);
  g_slice_free1 (sizeof(Surface), hsurface);

  return 0;
}


gint compositor_config_surface (gpointer compositor, gpointer surface, SurfaceInfo *surface_info)
{
  CompositorHandle *hcompositor = (CompositorHandle*) compositor;
  Surface *hsurface = (Surface *) surface;
  DestRect prect;

  memcpy (&prect, &hsurface->info.dst, sizeof(DestRect));
  memcpy (&hsurface->info, surface_info, sizeof(SurfaceInfo));
  if (surface_info->keep_ratio) {
    compositor_check_keep_ratio (hcompositor, hsurface, surface_info);
  }

  if (!RECTA_EQUAL_RECTB (&prect, &hsurface->info.dst)) {
    compositor_check_list_draw_area (hcompositor);
  }

  return compositor_device_update_surface_info (hcompositor->hdevice, &hsurface->info, hsurface->hdevice_surface);
}

gboolean compositor_check_need_clear_display (gpointer compositor)
{
  CompositorHandle *hcompositor = (CompositorHandle*) compositor;

  return TRUE; 
}

gint compositor_update_surface (gpointer compositor, gpointer surface, SurfaceBuffer *buffer)
{
  CompositorHandle *hcompositor = (CompositorHandle*) compositor;
  Surface *hsurface = (Surface *) surface;

  if (!hcompositor || !hsurface || !buffer) {
    GST_ERROR ("invalid parameter.");
    return -1;
  }

  GST_DEBUG ("update surface (%p), buffer vaddr (%p) paddr (%p)", 
      hsurface, buffer->vaddr, buffer->paddr);

  memcpy (&hsurface->buffer, buffer, sizeof(SurfaceBuffer));
  hsurface->update = TRUE;

  if (hcompositor->head == hcompositor->tail)
    compositor_do_compositing_surface_list (hcompositor);
  else
    COMPOSITOR_POST_SURFACE (hcompositor);

  return 0;
}

gint64 compositor_get_surface_showed_frames (gpointer compositor, gpointer surface)
{
  CompositorHandle *hcompositor = (CompositorHandle*) compositor;
  Surface *hsurface = (Surface *) surface;

  if (!hcompositor || !hsurface) {
    GST_ERROR ("invalid parameter.");
    return -1;
  }

  return hsurface->blited_frames;
}

