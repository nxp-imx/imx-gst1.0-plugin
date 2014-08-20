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
#include "playengine.h"

#ifdef PREPARE_WINDOW_MESSAGE
static GstBusSyncReply bus_sync_cb(GstBus * bus, GstMessage * message,
                                    gpointer user_data)
{
  play_engine *engine = (play_engine *)user_data;
  if (!engine)
    return GST_BUS_PASS;

  if (!gst_is_video_overlay_prepare_window_handle_message(message))
    return GST_BUS_PASS;

#ifndef ENABLE_OVERLAY_INTERNEL_WINDOW
  if (engine->video_window_handle != 0) {
    GstVideoOverlay *overlay;
    overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC (message));
    gst_video_overlay_set_window_handle(overlay, engine->video_window_handle);
  } else {
    g_warning("No window created for video!\n");
    if (engine->error_cb)
      engine->error_cb("No window created for video!");
  }
#endif
  gst_message_unref(message);
  return GST_BUS_DROP;
}
#endif

/* Get metadata information. */
static void get_metadata_tag(const GstTagList * list, const gchar * tag,
                              gpointer data)
{
  gint count = gst_tag_list_get_tag_size(list, tag);
  gint i = 0;
  play_engine *engine = (play_engine *)data;

  if (!engine)
    return;

  imx_metadata *meta = &engine->meta;
  for (i = 0; i < count; i++) {
    const GValue *val = gst_tag_list_get_value_index (list, tag, i);
    //g_print ("\t%20s : tag of type %s\n", tag, G_VALUE_TYPE_NAME (val));
    if (G_VALUE_HOLDS_STRING (val)) {
      const gchar *str = g_value_get_string (val);
      if (str) {
        if( strncmp(gst_tag_get_nick(tag), "container format", 16) == 0 ) {
          strncpy(meta->container, str, sizeof(meta->container));
          meta->container[sizeof(meta->container) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "location", 8) == 0) {
          strncpy(meta->pathname, str, sizeof(meta->pathname));
          meta->pathname[sizeof(meta->pathname) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "title", 5) == 0) {
          strncpy(meta->title, str, sizeof(meta->title));
          meta->title[sizeof(meta->title) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "artist", 6) == 0) {
          strncpy(meta->artist, str, sizeof(meta->artist));
          meta->artist[sizeof(meta->artist) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "album", 5) == 0) {
          strncpy(meta->album, str, sizeof(meta->album));
          meta->album[sizeof(meta->album) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "date", 4) == 0) {
          strncpy(meta->year, str, sizeof(meta->year));
          meta->year[sizeof(meta->year) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "genre", 5) == 0) {
          strncpy(meta->genre, str, sizeof(meta->genre));
          meta->genre[sizeof(meta->genre) - 1] = '\0';
        }
#ifdef GET_STREAM_INFO_FROM_TAGS
        if (strncmp(gst_tag_get_nick(tag), "audio codec", 11) == 0) {
          strncpy(meta->audiocodec, str, sizeof(meta->audiocodec));
          meta->audiocodec[sizeof(meta->audiocodec) - 1] = '\0';
        }
        if (strncmp(gst_tag_get_nick(tag), "video codec", 11) == 0) {
          strncpy(meta->videocodec, str, sizeof(meta->videocodec));
          meta->videocodec[sizeof(meta->videocodec) - 1] = '\0';
        }
#endif
      }
    }
#ifdef GET_STREAM_INFO_FROM_TAGS
    else if (G_VALUE_HOLDS_UINT (val)) {
      guint value = g_value_get_uint (val);
      if (strncmp(gst_tag_get_nick(tag), "bitrate", 7) == 0) {
        meta->audiobitrate = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "image width", 11) == 0) {
        meta->width = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "image height", 12) == 0) {
        meta->height = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "frame rate", 10) == 0) {
        meta->framerate = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "video bitrate", 13) == 0) {
        meta->videobitrate = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "number of channels", 18) == 0) {
        meta->channels = value;
      }
      if (strncmp(gst_tag_get_nick(tag), "sampling frequency (Hz)", 23) == 0) {
        meta->samplerate = value;
      }
    }
#endif
    else {
      continue;
    }
  }
}

/* Extract some metadata from the streams and print it on the screen */
static void analyze_streams (gpointer data)
{
  gint i;
  GstTagList *tags;
  GstCaps *caps;
  GstStructure *st;
  GstPad *pad;
  gchar *str;
  gint ntracks = 0;
  play_engine *engine = (play_engine *)data;

  /* Read some properties */
  g_object_get (engine->bin, "n-video", &engine->meta.n_video, NULL);
  g_object_get (engine->bin, "n-audio", &engine->meta.n_audio, NULL);
  g_object_get (engine->bin, "n-text", &engine->meta.n_subtitle, NULL);

  ntracks = engine->meta.n_video;
  if (ntracks > MAX_VIDEO_TRACK_COUNT)
    ntracks = MAX_VIDEO_TRACK_COUNT;

  for (i = 0; i < ntracks; i++) {
    tags = NULL;
    pad = NULL;
    /* Retrieve the stream's video tags */
    g_signal_emit_by_name (engine->bin, "get-video-pad", i, &pad);
    if (pad) {
      caps = gst_pad_get_current_caps (pad);
      st = gst_caps_get_structure (caps, 0);
      gst_structure_get_int (st, "width", &engine->meta.video_info[i].width);
      gst_structure_get_int (st, "height", &engine->meta.video_info[i].height);
      gst_structure_get_fraction (st, "framerate",
          &engine->meta.video_info[i].framerate_numerator,
          &engine->meta.video_info[i].framerate_denominator);
      gst_caps_unref (caps);
      gst_object_unref (pad);
    }

    g_signal_emit_by_name (engine->bin, "get-video-tags", i, &tags);
    if (tags) {
      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &str);
      strncpy (engine->meta.video_info[i].codec_type, str ? str : "unknown",
               METADATA_ITEM_SIZE_SMALL);
      engine->meta.video_info[i].codec_type[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);

      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str);
      strncpy (engine->meta.video_info[i].language, str ? str : "unknown",
               METADATA_ITEM_SIZE_SMALL);
      engine->meta.video_info[i].language[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);

      gst_tag_list_get_uint (tags, GST_TAG_BITRATE,
                             &engine->meta.video_info[i].bitrate);

      gst_tag_list_free (tags);
    }
  }

  ntracks = engine->meta.n_audio;
  if (ntracks > MAX_AUDIO_TRACK_COUNT)
    ntracks = MAX_AUDIO_TRACK_COUNT;

  for (i = 0; i < ntracks; i++) {
    tags = NULL;
    pad = NULL;
    /* Retrieve the stream's audio tags */
    g_signal_emit_by_name (engine->bin, "get-audio-pad", i, &pad);
    if (pad) {
      caps = gst_pad_get_current_caps (pad);
      st = gst_caps_get_structure (caps, 0);
      gst_structure_get_int (st, "rate",
                             &engine->meta.audio_info[i].samplerate);
      gst_structure_get_int (st, "channels",
                             &engine->meta.audio_info[i].channels);
      gst_caps_unref (caps);
      gst_object_unref (pad);
    }

    g_signal_emit_by_name (engine->bin, "get-audio-tags", i, &tags);
    if (tags) {
      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &str);
      strncpy (engine->meta.audio_info[i].codec_type, str ? str : "unknown",
               METADATA_ITEM_SIZE_SMALL);
      engine->meta.audio_info[i].codec_type[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);

      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str);
      strncpy(engine->meta.audio_info[i].language, str ? str : "unknown",
              METADATA_ITEM_SIZE_SMALL);
      engine->meta.audio_info[i].language[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);
      gst_tag_list_get_uint (tags, GST_TAG_BITRATE,
                             &engine->meta.audio_info[i].bitrate);

      gst_tag_list_free (tags);
    }
  }

  ntracks = engine->meta.n_subtitle;
  if (ntracks > MAX_SUBTITLE_TRACK_COUNT)
    ntracks = MAX_SUBTITLE_TRACK_COUNT;
  for (i = 0; i < ntracks; i++) {
    tags = NULL;
    /* Retrieve the stream's subtitle tags */
    g_signal_emit_by_name (engine->bin, "get-text-tags", i, &tags);
    if (tags) {
      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_SUBTITLE_CODEC, &str);
      strncpy (engine->meta.subtitle_info[i].codec_type, str ? str : "unknown",
               METADATA_ITEM_SIZE_SMALL);
      engine->meta.subtitle_info[i].codec_type[METADATA_ITEM_SIZE_SMALL-1]='\0';
      g_free (str);

      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str);
      strncpy(engine->meta.subtitle_info[i].language, str ? str : "unknown",
              METADATA_ITEM_SIZE_SMALL);
      engine->meta.subtitle_info[i].language[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);

      gst_tag_list_free (tags);
    }
  }

  g_object_get (engine->bin, "current-video", &engine->cur_video, NULL);
  g_object_get (engine->bin, "current-audio", &engine->cur_audio, NULL);
  g_object_get (engine->bin, "current-text", &engine->cur_subtitle, NULL);

#ifdef PRINT_STREAM_INFO
  /* print the streams info */
  g_print ("%d video stream(s), %d audio stream(s), %d text stream(s)\n",
      engine->meta.n_video, engine->meta.n_audio,
      engine->meta.n_subtitle);

  ntracks = engine->meta.n_video;
  if (ntracks > MAX_VIDEO_TRACK_COUNT)
    ntracks = MAX_VIDEO_TRACK_COUNT;
  for (i = 0; i < ntracks; i++) {
    g_print ("\n");
    g_print ("video stream %d:\n", i);
    g_print ("  codec: %s\n", engine->meta.video_info[i].codec_type);
    g_print ("  language: %s\n", engine->meta.video_info[i].language);
    g_print ("  resolution: %d x %d \n", engine->meta.video_info[i].width,
        engine->meta.video_info[i].height);
    g_print ("  framerate: %d/%d\n",
        engine->meta.video_info[i].framerate_numerator,
        engine->meta.video_info[i].framerate_denominator);
    g_print ("  bitrate: %d\n", engine->meta.video_info[i].bitrate);
  }

  ntracks = engine->meta.n_audio;
  if (ntracks > MAX_AUDIO_TRACK_COUNT)
    ntracks = MAX_AUDIO_TRACK_COUNT;
  for (i = 0; i < ntracks; i++) {
    g_print ("\n");
    g_print ("audio stream %d:\n", i);
    g_print ("  codec: %s\n", engine->meta.audio_info[i].codec_type);
    g_print ("  language: %s\n", engine->meta.audio_info[i].language);
    g_print ("  channels: %d\n", engine->meta.audio_info[i].channels);
    g_print ("  sample rate: %d\n", engine->meta.audio_info[i].samplerate);
    g_print ("  bitrate: %d\n", engine->meta.audio_info[i].bitrate);
  }

  ntracks = engine->meta.n_subtitle;
  if (ntracks > MAX_SUBTITLE_TRACK_COUNT)
    ntracks = MAX_SUBTITLE_TRACK_COUNT;
  for (i = 0; i < ntracks; i++) {
    g_print ("\n");
    g_print ("subtitle stream %d:\n", i);
    g_print ("  codec: %s\n", engine->meta.subtitle_info[i].codec_type);
    g_print ("  language: %s\n", engine->meta.subtitle_info[i].language);
  }

  g_print ("\n");
  g_print ("Currently playing video %d, audio %d and subtitle %d\n",
            engine->cur_video, engine->cur_audio, engine->cur_subtitle);
#endif

}

static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
  play_engine *engine = (play_engine *)data;
  switch (GST_MESSAGE_TYPE(msg)) {
  case GST_MESSAGE_EOS: {
    g_print("end-of-stream\n");
    if (engine->eos_cb)
      engine->eos_cb(engine->player);
    break;
  }
  case GST_MESSAGE_ERROR: {
    gchar *debug;
    GError *err;

    gst_message_parse_error(msg, &err, &debug);
    g_free(debug);

    g_warning("Error: %s\n", err->message);

    if (engine->error_cb)
      engine->error_cb(engine->player, err->message);
    g_error_free(err);
    break;
  }
  case GST_MESSAGE_TAG: {
      GstTagList *tags;
      gst_message_parse_tag(msg, &tags);
      gst_tag_list_foreach(tags, get_metadata_tag, data);
      gst_tag_list_free(tags);
      break;
  }
  case GST_MESSAGE_STATE_CHANGED: {
    GstState old_st, new_st, pending_st;
    gst_message_parse_state_changed (msg, &old_st, &new_st, &pending_st);
    if (GST_MESSAGE_SRC (msg) == GST_OBJECT (engine->bin)) {
      if (old_st == GST_STATE_READY && new_st == GST_STATE_PAUSED) {
        /* Once we are in the playing state, analyze the streams */
        analyze_streams (data);
      }

      /* inform application state changed */
      //g_print("old: %d, new: %d, pending: %d\n",old_st, new_st, pending_st);
      if (engine->state_change_cb)
        engine->state_change_cb(engine->player, old_st, new_st, pending_st);
    }
  }
  break;

  default:
    break;
  }

  return TRUE;
}

static gint64 playengine_get_position(play_engine *engine)
{
  gint64 cur = GST_CLOCK_TIME_NONE;

  if (engine) {
    if (!gst_element_query_position(engine->pipeline, GST_FORMAT_TIME, &cur))
      return GST_CLOCK_TIME_NONE;
  }

  return cur;
}

static gint64 playengine_get_duration(play_engine *engine)
{
  gint64 dur = GST_CLOCK_TIME_NONE;

  if (engine) {
    gst_element_query_duration(engine->pipeline, GST_FORMAT_TIME, &dur);
    engine->duration = dur;
  }

  return dur;
}

static void playengine_set_file(play_engine *engine, gchar *filename)
{
  gchar uri[256] = {0};
  if (engine && filename) {
    if (strstr(filename, "://")) {
        snprintf(uri, 255, "%s", filename);
    } else if( filename[0] == '/' ) {
        snprintf(uri, 255, "file://%s", filename);
    } else {
      gchar* pwd = getenv("PWD");
        snprintf(uri, 255, "file://%s/%s", pwd, filename);
    }

    g_object_set(G_OBJECT(engine->bin), "uri", uri, NULL);
  }
}

static void playengine_play(play_engine *engine)
{
  if (engine) {
    gst_element_set_state(engine->pipeline, GST_STATE_PLAYING);
    engine->play_rate = 1.0;
  }
}

static void playengine_stop(play_engine *engine)
{
  if (engine)
    gst_element_set_state(engine->pipeline, GST_STATE_NULL);
}

static void playengine_pause(play_engine *engine)
{
  if (engine)
    gst_element_set_state(engine->pipeline, GST_STATE_PAUSED);
}

static void playengine_seek(play_engine *engine, gint64 value,
                            gboolean accurate)
{
  GstEvent *seek = NULL;
  GstSeekFlags flag = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT;

  if (engine) {
    engine->duration = playengine_get_duration(engine);

    if (accurate)
      flag |= GST_SEEK_FLAG_ACCURATE;

    seek = gst_event_new_seek(1.0, GST_FORMAT_TIME, flag, GST_SEEK_TYPE_SET,
                              value, GST_SEEK_TYPE_SET, engine->duration);
    gst_element_send_event(engine->pipeline, seek);
    engine->play_rate = 1.0;
  }
}

static void playengine_set_play_rate(play_engine *engine, double rate)
{
  GstEvent *set_playback_rate_event = NULL;
  gint64 cur;
  GstQuery* query;

  if(rate > 16.0 || rate < -16.0) {
    g_print("Invalid rate=%f, should be between [-16.0, 16.0]!\n", rate);
    return;
  }

  query = gst_query_new_position(GST_FORMAT_TIME);
  if( gst_element_query(engine->bin, query) ) {
    gst_query_parse_position(query,NULL,&cur);
    //g_print("current_position = %"GST_TIME_FORMAT"\n", GST_TIME_ARGS (cur));
  } else {
    g_print ("current_postion query failed...\n");
  }

  gst_element_query_duration(engine->bin, GST_FORMAT_TIME, &(engine->duration));

  if( rate >= 0.0 ) {
    set_playback_rate_event = gst_event_new_seek(rate, GST_FORMAT_TIME,
                GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                GST_SEEK_TYPE_SET, cur, GST_SEEK_TYPE_SET, engine->duration);
  } else {
    set_playback_rate_event = gst_event_new_seek(rate, GST_FORMAT_TIME,
                GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, cur);
  }

  if(gst_element_send_event(engine->bin, set_playback_rate_event) == FALSE) {
    g_print("Send setting playback rate event failed!\n");
    if (engine->error_cb)
      engine->error_cb(engine->player,
                       "Send setting playback rate event failed!");
    return;
  }

  engine->play_rate = rate;
}

static double playengine_get_play_rate(play_engine *engine)
{
  return engine->play_rate;
}

static void playengine_set_rotate(play_engine *engine, gint rotation)
{
  g_object_set(G_OBJECT(engine->video_sink), "rotate", rotation, NULL);
  g_object_set(G_OBJECT(engine->video_sink), "reconfig", 1, NULL);
}

static PlayEngineState playengine_get_state(play_engine *engine)
{
  PlayEngineState state = PLAYENGINE_UNKNOWN;
  GstState gst_state = GST_STATE_VOID_PENDING;
  if (engine) {
    gst_element_get_state(engine->pipeline, &gst_state, NULL, GST_SECOND);
    if (GST_STATE_NULL == gst_state || GST_STATE_READY == gst_state)
      state = PLAYENGINE_STOPPED;
    else if (GST_STATE_PAUSED == gst_state)
      state = PLAYENGINE_PAUSED;
    else if (GST_STATE_PLAYING == gst_state)
      state = PLAYENGINE_PLAYING;
  }

  return state;
}

static void playengine_set_force_ratio (play_engine *engine, gboolean force)
{
  if (engine)
    g_object_set(G_OBJECT(engine->bin), "force-aspect-ratio", force, NULL);
}

static void playengine_get_metadata(play_engine *engine, imx_metadata *meta)
{
  if (engine && meta)
    memcpy(meta, &engine->meta, sizeof (imx_metadata));
}

static gint playengine_get_subtitle_num(play_engine *engine)
{
  if (engine && engine->meta.n_subtitle == 0)
    g_object_get (G_OBJECT(engine->bin), "n-text",
                  &engine->meta.n_subtitle, NULL);

  return engine->meta.n_subtitle;
}

static gint playengine_get_audio_num(play_engine *engine)
{
  if (engine && engine->meta.n_audio == 0)
    g_object_get (G_OBJECT(engine->bin), "n-audio",
                  &engine->meta.n_audio, NULL);

  return engine->meta.n_audio;
}

static gint playengine_get_video_num(play_engine *engine)
{
  if (engine && engine->meta.n_video == 0)
    g_object_get (G_OBJECT(engine->bin), "n-video",
                  &engine->meta.n_video, NULL);

  return engine->meta.n_video;
}

static gint playengine_get_current_audio_no(play_engine *engine)
{
  return engine->cur_audio;
}

static gint playengine_get_current_video_no(play_engine *engine)
{
  return engine->cur_video;
}

static gint playengine_get_current_subtitle_no(play_engine *engine)
{
  return engine->cur_subtitle;
}

static void playengine_select_subtitle(play_engine *engine, gint text_no)
{
  if (engine && text_no != engine->cur_subtitle) {
    if (text_no < engine->meta.n_subtitle) {
      g_object_set( engine->bin, "current-text", text_no, NULL );
      g_object_get (engine->bin, "current-text", &engine->cur_subtitle, NULL);
    } else {
      g_print("subtitle number out of range %d, %d\n", text_no,
              engine->meta.n_subtitle);
    }
  }
}

static void playengine_select_audio(play_engine *engine, gint audio_no)
{
  if (engine && audio_no != engine->cur_audio) {
    if (audio_no < engine->meta.n_audio) {
      g_object_set( engine->bin, "current-audio", audio_no, NULL );
      g_object_get( engine->bin, "current-audio", &engine->cur_audio, NULL );
    } else {
      g_print("audio number out of range %d, %d\n", audio_no,
              engine->meta.n_audio);
    }
  }
}

static void playengine_select_video(play_engine *engine, gint video_no)
{
  if (engine && video_no != engine->cur_video) {
    if (video_no < engine->meta.n_video) {
      g_object_set( engine->bin, "current-video", video_no, NULL );
      g_object_get( engine->bin, "current-video", &engine->cur_video, NULL );
    } else {
      g_print("video number out of range %d, %d\n", video_no,
              engine->meta.n_video);
    }
  }
}

static gboolean playengine_get_subtitle_text(play_engine *engine, gchar *text)
{
  //TODO text-sink??
  return 1;
}

static void playengine_set_volume(play_engine *engine, gdouble volume)
{
  GValue value  = {0};
  if (engine) {
    if( volume >= 0.0 && volume <= 1.0 ) {
      g_value_init(&value, G_TYPE_DOUBLE);
      g_value_set_double(&value, volume);
      g_object_set_property(G_OBJECT(engine->bin), "volume", &value);
    } else {
      g_print("Volume out of range %f\n", volume);
    }
  }
}

static gdouble playengine_get_volume(play_engine *engine)
{
  GValue value  = {0};

  g_value_init(&value, G_TYPE_DOUBLE);
  if (engine)
    g_object_get_property(G_OBJECT(engine->bin), "volume", &value);

  return g_value_get_double(&value);
}

static void playengine_set_mute(play_engine *engine, gboolean mute)
{
  GValue value  = {0};
  if (engine) {
    g_value_init(&value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&value, mute);
    g_object_set_property(G_OBJECT(engine->bin), "mute", &value);
  }
}

static gboolean playengine_get_seekable(play_engine *engine)
{
  GstQuery *query;
  gboolean res;
  gboolean seekable = FALSE;

  if (engine) {
    query = gst_query_new_seeking (GST_FORMAT_TIME);
    if (gst_element_query (engine->bin, query)) {
      gst_query_parse_seeking (query, NULL, &res, NULL, NULL);
      if (res)
        seekable = TRUE;
    }
    gst_query_unref (query);
  }

  return seekable;
}

static void playengine_set_window(play_engine *engine, guintptr handle)
{
  if (engine) {
#ifndef PREPARE_WINDOW_MESSAGE
    if (GST_IS_VIDEO_OVERLAY (engine->video_sink)) {
      g_print("videosink is overlay\n");
      gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(engine->video_sink),
                                          handle);
    } else {
      g_print("videosink is not overlay\n");
    }
#else
    engine->video_window_handle = handle;
#endif
  }
}

static void playengine_set_render_rect(play_engine *engine,
                                      gint x, gint y, gint w, gint h)
{
  if (engine && GST_IS_VIDEO_OVERLAY(engine->video_sink)) {
    gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(engine->video_sink)
                                           ,x, y, w, h);
  }
}

static void playengine_expose_video(play_engine *engine)
{
  if (engine && GST_IS_VIDEO_OVERLAY(engine->video_sink))
    gst_video_overlay_expose(GST_VIDEO_OVERLAY(engine->video_sink));
}

play_engine * play_engine_create(int *argc, char **argv[],
               void (*eos_cb)(void *),
               void (*error_cb)(void *, const gchar *),
               void (*state_change_cb)(void *, GstState, GstState, GstState))
{
  play_engine *engine = NULL;
  guint major, minor, micro, nano;

  engine = (play_engine *)malloc(sizeof(play_engine));
  if (!engine) {
    g_print("malloc play engine failed\n");
    return NULL;
  } else {
    memset (engine, 0, sizeof (play_engine));
  }

  gst_init(argc, argv);
  gst_version (&major, &minor, &micro, &nano);

  g_print ("GStreamer version %d.%d.%d\n", major, minor, micro);

  engine->pipeline = gst_pipeline_new("gst-player");
  engine->bin = gst_element_factory_make("playbin", "bin");

#ifdef VIDEO_SINK_V4L2SINK
  engine->video_sink = gst_element_factory_make ("imxv4l2sink", "videosink");
#else
  engine->video_sink = gst_element_factory_make ("overlaysink", "videosink");
#endif

  g_object_set(G_OBJECT(engine->bin), "video-sink", engine->video_sink, NULL);
  gst_bin_add(GST_BIN(engine->pipeline), engine->bin);

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(engine->pipeline));
  gst_bus_add_watch(bus, bus_cb, engine);
#ifdef PREPARE_WINDOW_MESSAGE
  gst_bus_set_sync_handler(bus, (GstBusSyncHandler)bus_sync_cb, engine, NULL);
#endif
  gst_object_unref(bus);

  engine->set_file = playengine_set_file;
  engine->play = playengine_play;
  engine->stop = playengine_stop;
  engine->pause = playengine_pause;
  engine->seek = playengine_seek;
  engine->set_play_rate = playengine_set_play_rate;
  engine->get_play_rate = playengine_get_play_rate;
  engine->rotate = playengine_set_rotate;
  engine->force_ratio = playengine_set_force_ratio;
  engine->get_state = playengine_get_state;
  engine->get_position = playengine_get_position;
  engine->get_duration = playengine_get_duration;
  engine->get_metadata = playengine_get_metadata;
  engine->get_subtitle_num = playengine_get_subtitle_num;
  engine->get_audio_num = playengine_get_audio_num;
  engine->get_video_num = playengine_get_video_num;
  engine->get_cur_subtitle_no = playengine_get_current_subtitle_no;
  engine->get_cur_audio_no = playengine_get_current_audio_no;
  engine->get_cur_video_no = playengine_get_current_video_no;
  engine->set_volume = playengine_set_volume;
  engine->get_volume = playengine_get_volume;
  engine->set_mute = playengine_set_mute;
  engine->get_seekable = playengine_get_seekable;
  engine->set_window = playengine_set_window;
  engine->set_render_rect = playengine_set_render_rect;
  engine->expose_video = playengine_expose_video;
  engine->select_audio = playengine_select_audio;
  engine->select_video = playengine_select_video;
  engine->select_subtitle = playengine_select_subtitle;
  engine->get_subtitle_text = playengine_get_subtitle_text;
  engine->eos_cb = eos_cb;
  engine->error_cb = error_cb;
  engine->state_change_cb = state_change_cb;
  return engine;
}

void play_engine_destroy(play_engine *engine)
{
  if (engine) {
    gst_element_set_state(engine->pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(engine->pipeline));
    free(engine);
  }
}
