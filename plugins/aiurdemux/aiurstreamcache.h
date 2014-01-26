/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 */



/*
 * Module Name:    aiurdemux.h
 *
 * Description:    Head file of unified parser gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */

#ifndef __AIURSTREAMCACHE_H__
#define __AIURSTREAMCACHE_H__
#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#define AIUR_STREAM_CACHE_SIZE 200000
#define AIUR_STREAM_CACHE_SIZE_MAX (AIUR_STREAM_CACHE_SIZE+10)

#if 0
#define GST_TYPE_AIURSTREAMCACHE \
  (gst_aiur_stream_cache_get_type())
#define GST_AIURSTREAMCACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AIURSTREAMCACHE,GstAiurStreamCache))
#define GST_AIURSTREAMCACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AIURSTREAMCACHE,GstAiurStreamCacheClass))
#define GST_IS_AIURSTREAMCACHE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AIURSTREAMCACHE))
#define GST_IS_AIURSTREAMCACHE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AIURSTREAMCACHE))

#define GST_AIURSTREAMCACHE_CAST(obj) ((GstAiurStreamCache *)(obj))
#endif
typedef struct _GstAiurStreamCache GstAiurStreamCache;
//typedef struct _GstAiurStreamCacheClass GstAiurStreamCacheClass;

struct _GstAiurStreamCache
{
  GstMiniObject mini_object;

  GstPad *pad;
  GstAdapter *adapter;
  GMutex mutex;
  GCond consume_cond;
  GCond produce_cond;

  guint64 start;
  guint64 offset;

  guint64 threshold_max;        /* threshold for cache max-size */
  guint64 threshold_pre;        /* threshold for cache max-size */

  guint64 ignore_size;

  gboolean eos;
  gboolean seeking;
  gboolean closed;

  void *context;
};





void gst_aiur_stream_cache_finalize (GstAiurStreamCache * cache);


void gst_aiur_stream_cache_close (GstAiurStreamCache * cache);


void gst_aiur_stream_cache_open (GstAiurStreamCache * cache);


GstAiurStreamCache *gst_aiur_stream_cache_new (guint64 threshold_max,
    guint64 threshold_pre, void *context);


void
gst_aiur_stream_cache_attach_pad (GstAiurStreamCache * cache, GstPad * pad);


gint64 gst_aiur_stream_cache_availiable_bytes (GstAiurStreamCache * cache);



void
gst_aiur_stream_cache_set_segment (GstAiurStreamCache * cache, guint64 start,
    guint64 stop);


void
gst_aiur_stream_cache_add_buffer (GstAiurStreamCache * cache,
    GstBuffer * buffer);


void gst_aiur_stream_cache_seteos (GstAiurStreamCache * cache, gboolean eos);


gint64 gst_aiur_stream_cache_get_position (GstAiurStreamCache * cache);


gint gst_aiur_stream_cache_seek (GstAiurStreamCache * cache, guint64 addr);


gint64
gst_aiur_stream_cache_read (GstAiurStreamCache * cache, guint64 size,
    char *buffer);

















#endif
