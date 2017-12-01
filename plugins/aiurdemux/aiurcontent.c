/*
 * Copyright (c) 2013-2015, Freescale Semiconductor, Inc. All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "aiurcontent.h"

GST_DEBUG_CATEGORY_EXTERN (aiurdemux_debug);
#define GST_CAT_DEFAULT aiurdemux_debug

typedef struct{
  const gchar *protocol;
  guint32 flags;
}AiurdemuxProtocalEntry;

static AiurdemuxProtocalEntry aiurdemux_protocol_table[] = {
  {"file",0},
  {"http",FILE_FLAG_READ_IN_SEQUENCE},
  {"rtsp",FILE_FLAG_READ_IN_SEQUENCE},
  {"udp",FILE_FLAG_READ_IN_SEQUENCE},
  {"rtp",FILE_FLAG_READ_IN_SEQUENCE},
  {"mtp-track",FILE_FLAG_READ_IN_SEQUENCE},
  {"mms",FILE_FLAG_NON_SEEKABLE|FILE_FLAG_READ_IN_SEQUENCE},
  {"rtmp",FILE_FLAG_NON_SEEKABLE|FILE_FLAG_READ_IN_SEQUENCE},
  {NULL,FILE_FLAG_NON_SEEKABLE|FILE_FLAG_READ_IN_SEQUENCE},
};

typedef struct
{
  gint64 length;
  gint64 offset;
  gboolean seekable;
  void *cache;
} AiurDemuxContentDesc;

struct _AiurContent
{
    gchar *uri;
    gint64 length;
    gboolean seekable;

    gboolean random_access;
    guint32 flags;
    gint64 duration;

    gchar * index_file;
    GstPad *sinkpad;
    GstAiurStreamCache *stream_cache;
};

/* memory callbacks */
void *
aiurcontent_callback_malloc (uint32 size)
{
  void *memory;

  if (size == 0) {
    size = 4;
    GST_WARNING ("Try mallo 0 size buffer, maybe a core parser bug!");
  }

  memory = g_try_malloc (size);
  return memory;
}


void *
aiurcontent_callback_calloc (uint32 numElements, uint32 size)
{
  void *memory;

  if (size == 0) {
    size = 4;
    GST_WARNING ("Try callo 0 size buffer, maybe a core parser bug!");
  }

  memory = g_try_malloc (numElements * size);

  if (memory) {
    memset (memory, 0, numElements * size);
  }

  return memory;
}


void *
aiurcontent_callback_realloc (void *ptr, uint32 size)
{
  void *memory;

  if (size == 0) {
    size = 4;
    GST_WARNING ("Try realloc 0 size buffer, maybe a core parser bug!");
  }

  memory = g_try_realloc (ptr, size);

  return memory;
}


void
aiurcontent_callback_free (void *ptr)
{
  if (ptr) {
    g_free (ptr);
  } else {
    GST_WARNING ("Try free NULL buffer, maybe a core parser bug!");
  }
}

/* pull mode stream callbacks */
FslFileHandle
aiurcontent_callback_open_pull (const uint8 * fileName, const uint8 * mode,
    void *context)
{
  AiurContent *pContent = (AiurContent *) context;
  AiurDemuxContentDesc *content;

  content = g_new0 (AiurDemuxContentDesc, 1);
  if (content) {

    content->length = pContent->length;
    content->seekable = pContent->seekable;
    content->offset = 0;
    content->cache = NULL;

  }

  return content;
}


int32
aiurcontent_callback_close_pull (FslFileHandle handle, void *context)
{
  if (handle) {

    g_free (handle);
  }
  return 0;
}


uint32
aiurcontent_callback_read_pull (FslFileHandle handle, void *buffer, uint32 size,
    void *context)
{
  GstBuffer *gstbuffer = NULL;
  AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
  AiurContent *pContent = (AiurContent *) context;
  GstFlowReturn ret;
  gint32 read_size = 0;
  GstMapInfo map;
  if ((content == NULL) || (size == 0))
    return 0;

  ret = gst_pad_pull_range (pContent->sinkpad, content->offset,
      size, &gstbuffer);

  if (ret == GST_FLOW_OK) {
    gst_buffer_map (gstbuffer, &map, GST_MAP_READ);
    read_size = map.size;
    content->offset += read_size;
    memcpy (buffer, map.data, read_size);
    gst_buffer_unmap (gstbuffer, &map);
    gst_buffer_unref (gstbuffer);
  } else {
    GST_WARNING ("gst_pad_pull_range failed ret = %d", ret);
  }

  return read_size;
}


int32
aiurcontent_callback_seek_pull (FslFileHandle handle, int64 offset,
    int32 whence, void *context)
{
  AiurDemuxContentDesc *content;
  int64 newoffset;
  int32 ret = 0;

  if (handle == NULL)
    return -1;

  content = (AiurDemuxContentDesc *) handle;
  newoffset = content->offset;

  switch (whence) {
    case SEEK_SET:
      newoffset = offset;
      break;

    case SEEK_CUR:
      newoffset += offset;
      break;

    case SEEK_END:
      newoffset = content->length + offset;
      break;

    default:
      return -1;
      break;
  }

  if ((newoffset < 0) || ((content->length > 0)
          && (newoffset > content->length))) {
    GST_ERROR ("Failed to seek. Target (%lld) exceeds the file range (%lld)",
        newoffset, content->length);
    ret = -1;
  } else {
    content->offset = newoffset;
  }

  return ret;
}


int64
aiurcontent_callback_tell_pull (FslFileHandle handle, void *context)
{
  AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;

  if (content == NULL)
    return 0;
  return content->offset;
}


int64
aiurcontent_callback_availiable_bytes_pull (FslFileHandle handle,
    int64 bytesRequested, void *context)
{
  return bytesRequested;
}

int64
aiurcontent_callback_size_pull (FslFileHandle handle, void *context)
{
  AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;

  if (content == NULL)
    return 0;

  return content->length;
}

uint32
aiurcontent_callback_getflag_pull (FslFileHandle handle, void *context)
{
    AiurContent *pContent = (AiurContent *) context;

    if(!pContent){
        return 0;
    }

    return pContent->flags;
}
/* push mode stream callbacks */
FslFileHandle
aiurcontent_callback_open_push (const uint8 * fileName, const uint8 * mode,
    void *context)
{
  AiurContent *pContent = (AiurContent *) context;
  AiurDemuxContentDesc *content;

  content = g_new0 (AiurDemuxContentDesc, 1);
  if (content) {
    content->cache =
        gst_mini_object_ref (GST_MINI_OBJECT_CAST (pContent->stream_cache));
    content->length = pContent->length;
    content->seekable = pContent->seekable;
    content->offset = 0;

  }

  return content;
}


int32
aiurcontent_callback_close_push (FslFileHandle handle, void *context)
{
  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    if (content->cache) {
      gst_mini_object_unref (GST_MINI_OBJECT_CAST (content->cache));
      content->cache = NULL;
    }
    g_free (handle);
  }
  return 0;
}


uint32
aiurcontent_callback_read_push (FslFileHandle handle, void *buffer, uint32 size,
    void *context)
{

  uint32 ret = 0;
  gint64 readsize = 0;

  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    if (size == 0)
      return ret;

    if (content->offset != gst_aiur_stream_cache_get_position (content->cache)) {
      gst_aiur_stream_cache_seek (content->cache, content->offset);
    }
    readsize =
        gst_aiur_stream_cache_read (content->cache, (guint64) size, buffer);
    if (readsize >= 0) {
      ret = readsize;
      content->offset += readsize;
    }

  }

  return ret;
}


int32
aiurcontent_callback_seek_push (FslFileHandle handle, int64 offset,
    int32 whence, void *context)
{

  GST_DEBUG ("seek to %lld", offset);

  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    int64 newoffset = content->offset;
    switch (whence) {
      case SEEK_SET:
        newoffset = offset;
        break;

      case SEEK_CUR:
        newoffset += offset;
        break;

      case SEEK_END:
        newoffset = content->length + offset;
        break;

      default:
        return -1;
        break;
    }

    if ((newoffset < 0) || ((content->length > 0)
            && (newoffset > content->length))) {
      GST_ERROR ("Failed to seek. Target (%lld) exceeds the file range (%lld)",
          newoffset, content->length);
      return -1;
    } else {
      content->offset = newoffset;
    }
  }



  return 0;
}


int64
aiurcontent_callback_size_push (FslFileHandle handle, void *context)
{

  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    return content->length;
  }

  return -1;


}


int64
aiurcontent_callback_tell_push (FslFileHandle handle, void *context)
{
  if (handle) {
    AiurDemuxContentDesc *content = (AiurDemuxContentDesc *) handle;
    return content->offset;
  }

  return -1;
}


int64
aiurcontent_callback_availiable_bytes_push (FslFileHandle handle,
    int64 bytesRequested, void *context)
{
  return bytesRequested;
}

uint32
aiurcontent_callback_getflag_push (FslFileHandle handle, void *context)
{

    AiurContent *pContent = (AiurContent *) context;

    if(!pContent){
        return 0;
    }

    return pContent->flags;

}

/* buffer callbacks */
uint8 *
aiurcontent_callback_request_buffer (uint32 stream_idx, uint32 * size,
    void **bufContext, void *parserContext)
{
  uint8 *buffer = NULL;
  GstBuffer *gstbuf = NULL;
  GstMapInfo map;

  AiurContent *pContent = (AiurContent *) parserContext;
  //AiurDemuxStream *stream = aiurdemux_trackidx_to_stream (demux, stream_idx);

  if((*size) >= 100000000){
      GST_ERROR("request buffer failed!!!");
    }

  if (*size == 0) {
    GST_WARNING
        ("Stream[%02d] request zero size buffer, maybe a core parser bug!",
        stream_idx);
    *size = AIURDEMUX_MIN_OUTPUT_BUFFER_SIZE;
  }

  if (TRUE) {
    gstbuf = gst_buffer_new_and_alloc (*size);
    *bufContext = gstbuf;
  } else {
    GST_ERROR ("Unknown stream number %d.", stream_idx);
  }

  if (gstbuf) {
    gst_buffer_map(gstbuf, &map, GST_MAP_READ);
    buffer = map.data;
    gst_buffer_unmap (gstbuf, &map);
    *bufContext = gstbuf;
  }

  return buffer;
}


void
aiurcontent_callback_release_buffer (uint32 stream_idx, uint8 * pBuffer,
    void *bufContext, void *parserContext)
{
  GstBuffer *gstbuf = (GstBuffer *) bufContext;
  if (gstbuf) {
    gst_buffer_unref (gstbuf);
  }
}
static gchar* aiurcontent_generate_idx_file(AiurContent * pContent,char * prefix)
{
    gchar *location = NULL, *buf = NULL;
    gchar * protocal = NULL;
    if(!pContent || !pContent->uri)
        goto bail;

    location = g_strdup (pContent->uri);
    protocal = gst_uri_get_protocol (location);
    if (strcmp(protocal, "file")) {
      goto bail;
    }

    buf = gst_uri_get_location (location);

    if (buf) {
      g_free (location);
      location = buf;
      while (*buf != '\0') {
        if (*buf == '/') {
          *buf = '.';
        }
        buf++;
      }

      buf = g_strdup_printf ("%s/%s.%s", prefix, location, "aidx");
    }

bail:
  if(location)
    g_free(location);
  if(protocal)
    g_free(protocal);
  return buf;

}

static void aiurcontent_query_content_info (AiurContent *pContent)
{
  GstQuery *q;
  GstFormat fmt;
  gchar *prefix;

  GstPad *pad = pContent->sinkpad;

  gst_object_ref (GST_OBJECT_CAST (pad));

  q = gst_query_new_uri ();
  if (gst_pad_peer_query (pad, q)) {
    gchar *uri;
    gst_query_parse_uri (q, &uri);
    if (uri) {
      pContent->uri = g_uri_unescape_string (uri, NULL);
      g_free (uri);
    }
  }
  gst_query_unref (q);

  q = gst_query_new_seeking (GST_FORMAT_BYTES);
  if (gst_pad_peer_query (pad, q)) {
    gst_query_parse_seeking (q, &fmt, &(pContent->seekable), NULL,
        NULL);
  }
  gst_query_unref (q);

  pContent->length = -1;
  q = gst_query_new_duration (GST_FORMAT_BYTES);
  if (gst_pad_peer_query (pad, q)) {
    gst_query_parse_duration (q, &fmt, &(pContent->length));
  }
  gst_query_unref (q);

  gst_object_unref (GST_OBJECT_CAST (pad));

  prefix = (gchar *)getenv ("HOME");

  if (prefix == NULL)
    prefix = "";

  prefix = g_strdup_printf ("%s/.aiur", prefix);


  pContent->index_file = aiurcontent_generate_idx_file(pContent,prefix);


    if (pContent->index_file) {
      umask (0);
      if (mkdir (prefix, 0777))
        GST_DEBUG("can not mkdir %s ", prefix);
    }
  g_free (prefix);

}
static void aiurcontent_set_flag (AiurContent *pContent)
{
  int i = 0;
  AiurdemuxProtocalEntry * protocol = NULL;

  gchar *uri = pContent->uri;
  gchar *uri_protocal = NULL;
  if(uri)
    uri_protocal = gst_uri_get_protocol (uri);

  pContent->flags = 0;

  if(uri_protocal){
  for(i = 0; i < sizeof(aiurdemux_protocol_table)/sizeof(AiurdemuxProtocalEntry); i++){
    protocol = &aiurdemux_protocol_table[i];
    if (protocol->protocol != NULL && (strcmp (uri_protocal, protocol->protocol) == 0)){
        pContent->flags = protocol->flags;
        break;
      }
    }
  }else{
    pContent->flags = FILE_FLAG_NON_SEEKABLE|FILE_FLAG_READ_IN_SEQUENCE;
  }

  if(!pContent->seekable)
      pContent->flags |= FILE_FLAG_NON_SEEKABLE;

  if(pContent->flags & FILE_FLAG_READ_IN_SEQUENCE){
      pContent->random_access= FALSE;
  }else{
      pContent->random_access = TRUE;
  }

  if(uri_protocal){
    g_free(uri_protocal);
  }
}

int aiurcontent_new(AiurContent **pContent)
{

    if(pContent == NULL)
        return 1;

    *pContent = g_new0 (AiurContent, 1);

    if(*pContent){
        memset(*pContent,0, sizeof(AiurContent));
    }

    return 0;

}
void aiurcontent_release(AiurContent *pContent)
{
    if(pContent == NULL)
        return;

    if(pContent->uri)
        g_free (pContent->uri);

    if(pContent->index_file)
        g_free (pContent->index_file);

    if(pContent)
        g_free(pContent);
}

int aiurcontent_get_pullfile_callback(AiurContent * pContent,FslFileStream *file_cbks)
{

    if(!pContent || file_cbks == NULL)
        return -1;

    file_cbks->Open = aiurcontent_callback_open_pull;
    file_cbks->Read = aiurcontent_callback_read_pull;
    file_cbks->Seek = aiurcontent_callback_seek_pull;
    file_cbks->Tell = aiurcontent_callback_tell_pull;
    file_cbks->Size = aiurcontent_callback_size_pull;
    file_cbks->Close = aiurcontent_callback_close_pull;
    file_cbks->CheckAvailableBytes = aiurcontent_callback_availiable_bytes_pull;
    file_cbks->GetFlag = aiurcontent_callback_getflag_pull;

    return 0;
}
int aiurcontent_get_pushfile_callback(AiurContent * pContent,FslFileStream *file_cbks)
{
    if(!pContent || file_cbks == NULL)
        return -1;

    file_cbks->Open = aiurcontent_callback_open_push;
    file_cbks->Read = aiurcontent_callback_read_push;
    file_cbks->Seek = aiurcontent_callback_seek_push;
    file_cbks->Tell = aiurcontent_callback_tell_push;
    file_cbks->Size = aiurcontent_callback_size_push;
    file_cbks->Close = aiurcontent_callback_close_push;

    file_cbks->CheckAvailableBytes = aiurcontent_callback_availiable_bytes_push;
    file_cbks->GetFlag = aiurcontent_callback_getflag_push;

    return 0;

}

int aiurcontent_get_memory_callback(AiurContent * pContent,ParserMemoryOps *mem_cbks)
{
    if(!pContent || mem_cbks == NULL)
        return -1;

    mem_cbks->Calloc = aiurcontent_callback_calloc;
    mem_cbks->Malloc = aiurcontent_callback_malloc;
    mem_cbks->Free = aiurcontent_callback_free;
    mem_cbks->ReAlloc = aiurcontent_callback_realloc;
    return 0;
}
int aiurcontent_get_buffer_callback(AiurContent * pContent,ParserOutputBufferOps *buffer_cbks)
{
    if(!pContent || buffer_cbks == NULL)
        return -1;

    buffer_cbks->RequestBuffer = aiurcontent_callback_request_buffer;
    buffer_cbks->ReleaseBuffer = aiurcontent_callback_release_buffer;

    return 0;
}
int aiurcontent_init(AiurContent * pContent,GstPad *sinkpad,GstAiurStreamCache *stream_cache)
{
    if(!pContent || sinkpad == NULL || stream_cache == NULL)
        return -1;

    pContent->sinkpad = sinkpad;
    pContent->stream_cache = stream_cache;

    aiurcontent_query_content_info (pContent);

    aiurcontent_set_flag(pContent);


    return 0;
}
gboolean aiurcontent_is_live(AiurContent * pContent)
{
    gboolean isLive = FALSE;

    if((pContent->random_access == FALSE) &&
        (pContent->seekable == FALSE)){
        isLive = TRUE;
    }

    return isLive;
}
gboolean aiurcontent_is_seelable(AiurContent * pContent)
{
    if(!pContent)
        return FALSE;

    return pContent->seekable;
}
gboolean aiurcontent_is_random_access(AiurContent * pContent)
{
    if(!pContent)
        return FALSE;

    return pContent->random_access;
}
gchar* aiurcontent_get_url(AiurContent * pContent)
{
    if(!pContent)
        return NULL;

    if(pContent->uri)
        return pContent->uri;
    else
        return NULL;
}
gchar* aiurcontent_get_index_file(AiurContent * pContent)
{
    if(!pContent)
        return NULL;

    if(pContent->index_file)
        return pContent->index_file;
    else
        return NULL;

}

