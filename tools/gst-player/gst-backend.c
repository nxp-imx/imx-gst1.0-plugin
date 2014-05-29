/*
 * Copyright (C) 2009-2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/*
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include "gst-backend.h"

//#define PREPARE_WINDOW_MESSAGE
//#define ENABLE_OVERLAY_INTERNEL_WINDOW
//#define VIDEO_SINK_V4L2SINK

static GstElement *pipeline;
static GstElement *bin;
static GstElement* video_sink;
static backend_metadata metadata;
extern void gtk_main_quit(void);
static void get_metadata_tag(const GstTagList * list, const gchar * tag,
          gpointer data);

#ifdef PREPARE_WINDOW_MESSAGE
static guintptr video_window_handle = 0;

static GstBusSyncReply bus_sync_handler(GstBus * bus, GstMessage * message,
    gpointer user_data) {
  // ignore anything but 'prepare-window-handle' element messages
  if (!gst_is_video_overlay_prepare_window_handle_message(message))
    return GST_BUS_PASS;
#ifndef ENABLE_OVERLAY_INTERNEL_WINDOW
  if (video_window_handle != 0) {
    GstVideoOverlay *overlay;
    overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC (message));
    gst_video_overlay_set_window_handle(overlay, video_window_handle);
  } else {
    g_warning("No window created for video!\n");
  }
#endif
  gst_message_unref(message);
  return GST_BUS_DROP;
}
#endif

static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer data) {
  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_EOS: {
    g_print("end-of-stream\n");
    //gtk_main_quit();
    break;
  }
  case GST_MESSAGE_ERROR: {
    gchar *debug;
    GError *err;

    gst_message_parse_error(msg, &err, &debug);
    g_free(debug);

    g_warning("Error: %s\n", err->message);
    g_error_free(err);
    //gtk_main_quit();
    break;
  }
  case GST_MESSAGE_TAG: {
      GstTagList *tags;
      gst_message_parse_tag(msg, &tags);
      gst_tag_list_foreach(tags, get_metadata_tag, data);
      gst_tag_list_free(tags);
      break;
  }
  default:
    break;
  }

  return TRUE;
}

void backend_init(int *argc, char **argv[]) {
  guint major, minor, micro, nano;

  gst_init(argc, argv);
  gst_version (&major, &minor, &micro, &nano);

  g_print ("This program is linked against GStreamer %d.%d.%d\n",
            major, minor, micro);

  pipeline = gst_pipeline_new("gst-player");

  bin = gst_element_factory_make("playbin", "bin");
#ifdef VIDEO_SINK_V4L2SINK
  video_sink = gst_element_factory_make ("imxv4l2sink", "videosink");
#else
  video_sink = gst_element_factory_make ("overlaysink", "videosink");
#endif

  g_object_set(G_OBJECT(bin), "video-sink", video_sink, NULL);
  gst_bin_add(GST_BIN(pipeline), bin);

  {
    GstBus *bus;
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_watch(bus, bus_cb, NULL);
#ifdef PREPARE_WINDOW_MESSAGE
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler) bus_sync_handler,
                              NULL, NULL);
#endif
    gst_object_unref(bus);
  }

}

void backend_set_window_id(guintptr handle) {
#ifndef PREPARE_WINDOW_MESSAGE
  if (GST_IS_VIDEO_OVERLAY (video_sink))
  {
    g_print("videosink is overlay\n");
    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY(video_sink), handle);
  }
  else {
    g_print("videosink is not overlay\n");
  }
#else
  video_window_handle = handle;
#endif
}

void backend_play(void) {
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void backend_stop(void) {
    gst_element_set_state(pipeline, GST_STATE_NULL);
}

void backend_pause(void) {
  gst_element_set_state(pipeline, GST_STATE_PAUSED);
}

void backend_resume(void) {
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void backend_reset(void) {
  gst_element_seek(pipeline, 1.0, GST_FORMAT_TIME,
      (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
      GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
}

void backend_seek(gint value) {
  GstEvent *seek_event = NULL;
  gint64 duration = backend_query_duration();
  gint64 cur = backend_query_position();
  //g_print("dur, cur: %lld:%lld\n", duration, cur);
  cur += value * GST_SECOND;
  seek_event = gst_event_new_seek(1.0, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                                  GST_SEEK_TYPE_SET, cur,
                                  GST_SEEK_TYPE_SET, duration);
  gst_element_send_event(pipeline, seek_event);
}

void backend_seek_absolute(guint64 value) {
  GstEvent *seek_event = NULL;
  gint64 duration = backend_query_duration();
  //g_print("dur, cur: %lld:%lld\n", duration, cur);

  seek_event = gst_event_new_seek(1.0, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                                  GST_SEEK_TYPE_SET, value,
                                  GST_SEEK_TYPE_SET, duration);
  gst_element_send_event(pipeline, seek_event);
}

guint64 backend_query_position(void) {
  gint64 cur;

  if (!gst_element_query_position(pipeline, GST_FORMAT_TIME, &cur))
    return GST_CLOCK_TIME_NONE;

  return (guint64) cur;
}

guint64 backend_query_duration(void) {
  //static gint64 duration = 0;
  gint64 duration = 0;
  //if (duration <=0 ) {
    if (!gst_element_query_duration(pipeline, GST_FORMAT_TIME, &duration))
      return GST_CLOCK_TIME_NONE;
  //}
  return (guint64) duration;
}

void backend_deinit(void) {
  if (pipeline) {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    pipeline = NULL;
  }
}

void backend_set_render_rect(gint x, gint y, gint w, gint h) {
  if (GST_IS_VIDEO_OVERLAY(video_sink)) {
    gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(video_sink),
                                            x, y, w, h);
  }
}

void backend_video_expose(void)
{
  if (GST_IS_VIDEO_OVERLAY(video_sink)) {
    gst_video_overlay_expose(GST_VIDEO_OVERLAY(video_sink));
  }
}

/* Get metadata information. */
static void get_metadata_tag(const GstTagList * list, const gchar * tag,
          gpointer data)
{
  gint count = gst_tag_list_get_tag_size(list, tag);
  gint i = 0;

  for (i = 0; i < count; i++) {
    const GValue *val = gst_tag_list_get_value_index (list, tag, i);
    //g_print ("\t%20s : tag of type %s\n", tag, G_VALUE_TYPE_NAME (val));
    if (G_VALUE_HOLDS_STRING (val)) {
      const gchar *str = g_value_get_string (val);
      if (str) {
        if (strncmp(gst_tag_get_nick(tag), "location", 8) == 0) {
          strncpy(metadata.pathname, str, sizeof(metadata.pathname));
          metadata.pathname[sizeof(metadata.pathname) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "title", 5) == 0) {
          strncpy(metadata.title, str, sizeof(metadata.title));
          metadata.title[sizeof(metadata.title) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "artist", 6) == 0) {
          strncpy(metadata.artist, str, sizeof(metadata.artist));
          metadata.artist[sizeof(metadata.artist) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "album", 5) == 0) {
          strncpy(metadata.album, str, sizeof(metadata.album));
          metadata.album[sizeof(metadata.album) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "date", 4) == 0) {
          strncpy(metadata.year, str, sizeof(metadata.year));
          metadata.year[sizeof(metadata.year) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "genre", 5) == 0) {
          strncpy(metadata.genre, str, sizeof(metadata.genre));
          metadata.genre[sizeof(metadata.genre) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "audio codec", 11) == 0) {
          strncpy(metadata.audiocodec, str, sizeof(metadata.audiocodec));
          metadata.audiocodec[sizeof(metadata.audiocodec) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "video codec", 11) == 0) {
          strncpy(metadata.videocodec, str, sizeof(metadata.videocodec));
          metadata.videocodec[sizeof(metadata.videocodec) - 1] = '\0';
        }
      }
    } else if (G_VALUE_HOLDS_UINT (val)) {
      guint value = g_value_get_uint (val);
      if (strncmp(gst_tag_get_nick(tag), "bitrate", 7) == 0) {
        metadata.audiobitrate = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "image width", 11) == 0) {
        metadata.width = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "image height", 12) == 0) {
        metadata.height = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "frame rate", 10) == 0) {
        metadata.framerate = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "video bitrate", 13) == 0) {
        metadata.videobitrate = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "number of channels", 18) == 0) {
        metadata.channels = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "sampling frequency (Hz)", 23) == 0) {
        metadata.samplerate = value;
      }
    } else {
      continue;
    }
  }
}

backend_metadata * backend_get_metadata(void)
{
  return &metadata;
}

gulong backend_get_color_key(void)
{
  gulong key = 0;
  const gchar *colorkey = g_getenv ("COLORKEY");
  if (colorkey) {
    if (strlen(colorkey) > 1)
      key = strtol(colorkey, NULL, 16);
  }
  return key;
}

static void filename2uri(gchar* uri, gchar* filename, guint size)
{
    if (strstr(filename, "://")){
        snprintf(uri, size, "%s", filename);
    }else if( filename[0] == '/' ){
        snprintf(uri, size, "file://%s", filename);
    }
    else
    {
      gchar* pwd = getenv("PWD");
        snprintf(uri, size, "file://%s/%s", pwd, filename);
    }
}

void backend_set_file_location(gchar *filename)
{
  gchar uri[256] = {0};
  filename2uri(uri, filename, sizeof(uri) - 1);
  g_object_set(G_OBJECT(bin), "uri", uri, NULL);
}

gboolean backend_is_playing(void)
{
  GstState current;
  gst_element_get_state(bin, &current, NULL, GST_SECOND);
  return (current == GST_STATE_PLAYING);
}
