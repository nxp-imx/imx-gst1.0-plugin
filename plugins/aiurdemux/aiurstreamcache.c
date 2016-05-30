

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
 * Copyright (C) 2010-2011, 2014-2015 Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/*
 * Module Name:    mfw_gst_streaming_cache.c
 *
 * Description:    Implementation for streamed based demuxer srcpad cache.
 *
 * Portability:    This code is written for Linux OS.
 */

/*
 * Changelog:
 *
 */

GST_DEBUG_CATEGORY_EXTERN (aiurdemux_debug);
#define GST_CAT_DEFAULT aiurdemux_debug

#include "aiurstreamcache.h"

#define WAIT_COND_TIMEOUT(cond, mutex, timeout) \
    do{\
        gint64 end_time;\
        end_time = g_get_monotonic_time () + timeout;\
        g_cond_wait_until((cond),(mutex),end_time);\
    }while(0)


#define READ_ADDR(cache)\
    ((cache)->start+(cache)->offset)

#define AVAIL_BYTES(cache)\
    (gst_adapter_available((cache)->adapter)-(cache)->offset)

#define CHECK_PRESERVE(cache)\
    do {\
        if ((cache)->offset>(cache)->threshold_pre){\
            guint64 flush = ((cache)->offset-(cache)->threshold_pre);\
            gst_adapter_flush((cache)->adapter, flush);\
            (cache)->offset =(cache)->threshold_pre;\
            (cache)->start+=flush;\
            g_cond_signal(&(cache)->consume_cond);\
        }\
    }while(0)

#define READ_BYTES(cache, buffer, readbytes)\
    do {\
        if (buffer){\
            gst_adapter_copy((cache)->adapter, (buffer), (cache)->offset, (readbytes));\
        }\
        (cache)->offset+=(readbytes);\
        CHECK_PRESERVE(cache);\
    }while(0)

GST_DEFINE_MINI_OBJECT_TYPE (GstAiurStreamCache, gst_aiur_stream_cache);
GType aiur_stream_cache_type = 0;

static GTimeVal timeout = { 1, 0 };

void
gst_aiur_stream_cache_finalize (GstAiurStreamCache * cache)
{

  if (cache->pad) {
    gst_object_unref (GST_OBJECT_CAST (cache->pad));
    cache->pad = NULL;
  }

  if (cache->adapter) {
    gst_adapter_clear (cache->adapter);
    gst_object_unref (cache->adapter);
    cache->adapter = NULL;
  }

  g_cond_clear (&cache->produce_cond);
  g_cond_clear (&cache->consume_cond);

  g_mutex_clear (&cache->mutex);

}

void
gst_aiur_stream_cache_close (GstAiurStreamCache * cache)
{

  if (cache) {
    cache->closed = TRUE;
  }
}

void
gst_aiur_stream_cache_open (GstAiurStreamCache * cache)
{
  if (cache) {
    cache->closed = FALSE;
  }
}



GstAiurStreamCache *
gst_aiur_stream_cache_new (guint64 threshold_max, guint64 threshold_pre,
    void *context)
{
  GstAiurStreamCache *cache = g_new0 (GstAiurStreamCache, 1);

  gst_mini_object_init (GST_MINI_OBJECT_CAST (cache), 0, aiur_stream_cache_type,
      (GstMiniObjectCopyFunction) NULL,
      (GstMiniObjectDisposeFunction) NULL,
      (GstMiniObjectFreeFunction) gst_aiur_stream_cache_finalize);

  cache->pad = NULL;

  cache->adapter = gst_adapter_new ();
  g_mutex_init (&cache->mutex);
  g_cond_init (&cache->consume_cond);
  g_cond_init (&cache->produce_cond);

  cache->threshold_max = threshold_max;
  cache->threshold_pre = threshold_pre;

  cache->start = 0;
  cache->offset = 0;
  cache->ignore_size = 0;

  cache->eos = FALSE;
  cache->seeking = FALSE;
  cache->closed = FALSE;

  cache->context = context;

  return cache;
}

void
gst_aiur_stream_cache_attach_pad (GstAiurStreamCache * cache, GstPad * pad)
{
  if (cache) {
    g_mutex_lock (&cache->mutex);
    if (cache->pad) {
      gst_object_unref (GST_OBJECT_CAST (cache->pad));
      cache->pad = NULL;
    }

    if (pad) {
      cache->pad = gst_object_ref (GST_OBJECT_CAST (pad));

    }
    g_mutex_unlock (&cache->mutex);
  }
}

gint64
gst_aiur_stream_cache_availiable_bytes (GstAiurStreamCache * cache)
{
  gint64 avail = -1;

  if (cache) {
    g_mutex_lock (&cache->mutex);
    avail = AVAIL_BYTES (cache);
    g_mutex_unlock (&cache->mutex);
  }

  return avail;
}


void
gst_aiur_stream_cache_set_segment (GstAiurStreamCache * cache, guint64 start,
    guint64 stop)
{
  if (cache) {
    g_mutex_lock (&cache->mutex);

    cache->seeking = FALSE;
    cache->start = start;
    cache->offset = 0;
    cache->ignore_size = 0;
    gst_adapter_clear (cache->adapter);
    cache->eos = FALSE;

    g_cond_signal (&cache->consume_cond);

    g_mutex_unlock (&cache->mutex);
  }
}


void
gst_aiur_stream_cache_add_buffer (GstAiurStreamCache * cache,
    GstBuffer * buffer)
{
  guint64 size;
  gint trycnt = 0;
  if ((cache == NULL) || (buffer == NULL))
    goto bail;

  g_mutex_lock (&cache->mutex);

  size = gst_buffer_get_size (buffer);

  if ((cache->seeking) || (size == 0)) {
    g_mutex_unlock (&cache->mutex);
    goto bail;
  }

  if (cache->ignore_size) {
    /* drop part or total buffer */
    if (cache->ignore_size >= size) {
      cache->ignore_size -= size;
      g_mutex_unlock (&cache->mutex);
      goto bail;
    } else {
      GstMapInfo map;
      GstBuffer * newBuffer;
      guint8 *inbuf = NULL;
      gst_buffer_map (buffer, &map, GST_MAP_READ);
      size = map.size;
      inbuf = map.data;
      gst_buffer_unmap(buffer,&map);

      newBuffer = gst_buffer_new_and_alloc (size - cache->ignore_size);
      gst_buffer_fill(newBuffer,0,(guint8 *)inbuf+cache->ignore_size,size - cache->ignore_size);
      cache->ignore_size = 0;

      gst_adapter_push (cache->adapter, newBuffer);
      newBuffer = NULL;
      if (buffer) {
        gst_buffer_unref (buffer);
      }
    }

  }else{
    gst_adapter_push (cache->adapter, buffer);
  }
  g_cond_signal (&cache->produce_cond);

  buffer = NULL;

  if (cache->threshold_max) {
#if 0
    if (cache->threshold_max < size + cache->threshold_pre) {
      cache->threshold_max = size + cache->threshold_pre;
    }
#endif

    while ((gst_adapter_available (cache->adapter) > cache->threshold_max)
        && (cache->closed == FALSE)) {
      if (((++trycnt) & 0x1f) == 0x0) {
        GST_WARNING ("wait push try %d SIZE %d %lld", trycnt,
            gst_adapter_available (cache->adapter), cache->threshold_max);
      }
      WAIT_COND_TIMEOUT (&cache->consume_cond, &cache->mutex, 1000000);
    }

    if (cache->seeking) {
      g_mutex_unlock (&cache->mutex);
      goto bail;
    }
  }


  g_mutex_unlock (&cache->mutex);

  return;

bail:
  if (buffer) {
    gst_buffer_unref (buffer);
  }
}

void
gst_aiur_stream_cache_seteos (GstAiurStreamCache * cache, gboolean eos)
{
  if (cache) {
    g_mutex_lock (&cache->mutex);
    cache->eos = eos;
    g_cond_signal (&cache->produce_cond);
    g_mutex_unlock (&cache->mutex);
  }
}


gint64
gst_aiur_stream_cache_get_position (GstAiurStreamCache * cache)
{

  gint64 pos = -1;
  if (cache) {

    g_mutex_lock (&cache->mutex);
    pos = READ_ADDR (cache);
    g_mutex_unlock (&cache->mutex);
  }
  return pos;
}


gint
gst_aiur_stream_cache_seek (GstAiurStreamCache * cache, guint64 addr)
{
  gboolean ret;

  gint r = 0;


  int isfail = 0;
  if (cache == NULL) {
    return -1;
  }

tryseek:
  g_mutex_lock (&cache->mutex);


  if (addr < cache->start) {    /* left */
    GST_DEBUG ("Flush cache, backward seek addr %lld, cachestart %lld, offset %lld",
        addr, cache->start, cache->offset);
    isfail = 1;
    goto trysendseek;
  } else if (addr <= cache->start + gst_adapter_available (cache->adapter)) {
    if (addr != READ_ADDR (cache)) {
      cache->offset = addr - cache->start;
      CHECK_PRESERVE (cache);
    }

  } else if ((addr > (cache->start + gst_adapter_available (cache->adapter))) && ((addr < cache->start + 2000000) || (isfail))) {       /* right */
    cache->ignore_size =
        addr - cache->start - gst_adapter_available (cache->adapter);

    cache->start = addr;
    cache->offset = 0;
    gst_adapter_clear (cache->adapter);
    g_cond_signal (&cache->consume_cond);
  } else {
    goto trysendseek;
  }
  g_mutex_unlock (&cache->mutex);
  return 0;
#if 1
trysendseek:

  GST_INFO ("stream cache try seek to %lld", addr);

  gst_adapter_clear (cache->adapter);

  cache->offset = 0;
  cache->start = addr;
  cache->ignore_size = 0;


  cache->seeking = TRUE;
  cache->eos = FALSE;
  g_mutex_unlock (&cache->mutex);
  ret =
      gst_pad_push_event (cache->pad, gst_event_new_seek ((gdouble) 1,
          GST_FORMAT_BYTES, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET,
          (gint64) addr, GST_SEEK_TYPE_NONE, (gint64) (-1)));
  g_cond_signal (&cache->consume_cond);


  if (ret == FALSE) {
    if (isfail == 0) {
      isfail = 1;
      goto tryseek;
    }
    r = -1;
  }
  return r;
#endif
}




gint64
gst_aiur_stream_cache_read (GstAiurStreamCache * cache, guint64 size,
    char *buffer)
{
  gint64 readsize = -1;
  gint retrycnt = 0;
  if (cache == NULL) {
    return readsize;
  }

try_read:

  if (cache->closed == TRUE) {
    return readsize;
  }

  g_mutex_lock (&cache->mutex);

  if (cache->seeking == TRUE)
    goto not_enough_bytes;

  if ((cache->threshold_max)
      && (cache->threshold_max < size + cache->threshold_pre)) {
    cache->threshold_max = size + cache->threshold_pre;
    /* enlarge maxsize means consumed */
    g_cond_signal (&cache->consume_cond);
  }

  if (size > AVAIL_BYTES (cache)) {
    if (cache->eos) {           /* not enough bytes when eos */
      readsize = AVAIL_BYTES (cache);
      if (readsize) {
        READ_BYTES (cache, buffer, readsize);
      }
      goto beach;
    }
    goto not_enough_bytes;
  }

  readsize = size;
  READ_BYTES (cache, buffer, readsize);

  goto beach;


not_enough_bytes:
  //g_print("not enough %lld, try %d\n", size, retrycnt++);
  WAIT_COND_TIMEOUT (&cache->produce_cond, &cache->mutex, 1000000);
  g_mutex_unlock (&cache->mutex);

  goto try_read;


beach:
  g_mutex_unlock (&cache->mutex);
  return readsize;
}

