/**
*  Copyright (c) 2014-2015, Freescale Semiconductor Inc.,
*  All Rights Reserved.
*
*  The following programs are the sole property of Freescale Semiconductor Inc.,
*  and contain its proprietary and confidential information.
*
*/

// for fullscreen setting
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/app/gstappsink.h>
#include "playengine.h"
#include "../../libs/gstimxcommon.h"

#define PLAYENGINE_DEFAULT_TIME_OUT 20 //default state change wait time :20s
#define FB_DEIVCE "/dev/fb0"

typedef struct _PlayEngineData
{
  GstElement *pipeline;
  GstElement *bin;
  GstElement *video_sink;
  GstElement *audio_sink;
  GstElement *text_sink;
  GstElement *visual_plugin;

  imx_metadata meta;
  imx_image_info thumbnail;

  gint64 duration;
  double play_rate;
  gint cur_video;
  gint cur_audio;
  gint cur_subtitle;

  gboolean bmute;
  gdouble volume;
  gint timeout_second;
  gboolean stop_wait;

  DisplayArea area;
  gint rotation;
#ifdef PREPARE_WINDOW_MESSAGE
  guintptr video_window_handle;
#endif

  void *UserCustomData;
  PlayEngineEventHandler app_callback;

  GMainContext *g_main_context;
  GMainLoop *g_main_loop;
  GThread *g_main_loop_thread;
  GMutex mutex;
} PlayEngineData;

#ifdef PREPARE_WINDOW_MESSAGE
static GstBusSyncReply 
bus_sync_cb(GstBus * bus,
            GstMessage * message,
            gpointer user_data)
{
  PlayEngine *engine = (PlayEngine *)user_data;
  PlayEngineData *engine_data = NULL;
  if (!engine || !engine->priv)
    return GST_BUS_PASS;

  engine_data = (PlayEngineData *)engine->priv;

  if (!gst_is_video_overlay_prepare_window_handle_message(message))
    return GST_BUS_PASS;

#ifndef ENABLE_OVERLAY_INTERNEL_WINDOW
  if (engine_data->video_window_handle != 0) {
    g_message("get window message");
    GstVideoOverlay *overlay;
    overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC (message));
    gst_video_overlay_set_window_handle(overlay, engine_data->video_window_handle);
  } else {
    g_warning("No window created for video!\n");
  }
#endif
  gst_message_unref(message);
  return GST_BUS_DROP;
}
#endif

/* return a new allocate string, need be freed after use */
static gchar * 
filename2uri(const gchar* fn)
{
  char * tmp;
  if (strstr(fn, "://")){
      tmp = g_strdup_printf("%s", fn);
  }else if( fn[0] == '/' ){
      tmp = g_strdup_printf("file://%s", fn);
  }
  else
  {
      gchar* pwd = getenv("PWD");
      tmp = g_strdup_printf("file://%s/%s", pwd, fn);
  }

  return tmp;
}

static PlayEngineResult
get_fullscreen_size(gint32 * pfullscreen_width,
                    gint32 * pfullscreen_height)
{
    struct fb_var_screeninfo scrinfo;
    gint32 fb;

    if((fb = open(FB_DEIVCE, O_RDWR, 0)) < 0)
    {
	g_warning("Unable to open %s %d\n", FB_DEIVCE, fb);
        fb = 0;
        return PLAYENGINE_FAILURE;
    }

    if (ioctl(fb, FBIOGET_VSCREENINFO, &scrinfo) < 0)
    {
        g_warning("Get var of fb0 failed\n");
        close(fb);
        return PLAYENGINE_FAILURE;
    }
    *pfullscreen_width = scrinfo.xres;
    *pfullscreen_height = scrinfo.yres;
    close(fb);

    return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
malloc_image( imx_image_info *image,
              gsize size)
{
  imx_image_info *image_info = image;

  if( !image_info )
    return PLAYENGINE_ERROR_BAD_PARAM;

  if (image_info->image) {
      g_free(image_info->image);
      image_info->image = NULL;
      image_info->size = 0;        
  }
  
  image_info->image = (guint8 *)g_malloc(size);
  if(!image_info->image)
    return PLAYENGINE_FAILURE;

  image_info->size = size;

  return PLAYENGINE_SUCCESS;
}

static GstElement *
get_video_sink(PlayEngine *engine)
{
  GstElement* auto_video_sink = NULL;
  GstElement* actual_video_sink = NULL;
  if(!engine || !engine->priv)
  {
    g_warning("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return NULL;
  }

  PlayEngineData *engine_data = (PlayEngineData *)engine->priv;

  g_object_get(G_OBJECT(engine_data->bin), "video-sink", &auto_video_sink, NULL);
  if( NULL == auto_video_sink )
  {
    g_warning("%s(): Can not find auto_video-sink\n", __FUNCTION__);
    return NULL;
  }

  GValue item = { 0, };
  GstIterator *it = gst_bin_iterate_sinks((GstBin*)auto_video_sink);
  if (gst_iterator_next (it, &item) != GST_ITERATOR_OK)
  {
    g_warning("%s(): gst_iterator_next failed\n", __FUNCTION__);
    gst_iterator_free (it);
    return NULL;
  }

  actual_video_sink = g_value_get_object (&item);
  g_value_unset (&item);
  gst_iterator_free (it);
  if( NULL == actual_video_sink )
  {
    g_warning("%s(): Can not find actual_video-sink\n", __FUNCTION__);
    return NULL;
  }
  g_object_unref (auto_video_sink);
  return actual_video_sink;
}

static PlayEngineResult
wait_for_state_change(PlayEngine *engine,
                      GstState pending_st,
                      gint timeout_second)
{
  GTimeVal tfthen, tfnow;
  GstClockTimeDiff diff;
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;
  GstState current;
  guint32 timeescap = 0;
  if(!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  PlayEngineData *engine_data = (PlayEngineData *)engine->priv;

  g_get_current_time(&tfthen);

  while(1)
  {
    /* gst_get_element_state will block one second if the state can not be return immediately */
    if (gst_element_get_state(engine_data->pipeline, &current, NULL, GST_SECOND)!=GST_STATE_CHANGE_FAILURE){
      g_get_current_time(&tfnow);
      diff = GST_TIMEVAL_TO_TIME(tfnow) - GST_TIMEVAL_TO_TIME(tfthen);
      diff /= (1000 * 1000);
      timeescap = (unsigned int) diff;

      if( pending_st == current )
      {
          break;
      }
      else
      {
          if ((timeout_second) && (timeescap > (timeout_second * 1000)))
          {
              g_warning( "\n%s(): Time out in state transferring from %s to %s",
                  __FUNCTION__,
                  gst_element_state_get_name (current),
                  gst_element_state_get_name (pending_st) );
              return PLAYENGINE_FAILURE;
          }

          if(engine_data->stop_wait)
          {
              return PLAYENGINE_FAILURE;
          }
      }
      usleep(400000);
      g_message("Wait status change from %s to %s ",
                gst_element_state_get_name (current),
                gst_element_state_get_name (pending_st));

    }else
    {
        g_warning("state change failed from %s to %s ",
                gst_element_state_get_name (current),
                gst_element_state_get_name (pending_st));
        return PLAYENGINE_FAILURE;
    }
  }

  return PLAYENGINE_SUCCESS;
}

/* Get metadata information. */
static PlayEngineResult 
get_metadata_tag( const GstTagList * list,
                  const gchar * tag,
                  gpointer data )
{
  gint count = gst_tag_list_get_tag_size(list, tag);
  gint i = 0;
  PlayEngine *engine = (PlayEngine *)data;
  PlayEngineData *engine_data = NULL;

  if (!engine || !engine->priv)
    return PLAYENGINE_ERROR_BAD_PARAM;

  engine_data = (PlayEngineData *)engine->priv;
  imx_metadata *meta = &engine_data->meta;
  for (i = 0; i < count; i++) {
    gchar * str = NULL;

    if(gst_tag_get_type(tag) == G_TYPE_STRING)
    {
      if(!gst_tag_list_get_string_index(list,tag,i,&str))
      {
        g_assert_not_reached();
      }
      if(str == NULL)
      {
        g_warning("tag list get string pointer return NULL");
        return PLAYENGINE_FAILURE;
      }
    }else if(gst_tag_get_type(tag) == GST_TYPE_BUFFER)
    {
      GstBuffer *img;
      img = gst_value_get_buffer(gst_tag_list_get_value_index(list, tag, i));
      if(img)
      {
        gchar *caps_str;
        GstMapInfo minfo;
        gst_buffer_map (img, &minfo, GST_MAP_READ);
        caps_str = g_strdup("unknown");
        str = g_strdup_printf("buffer of %u bytes, type: %s", minfo.size, caps_str);
        if( NULL != caps_str )
        {
          g_free(caps_str);
        }
        gst_buffer_unmap (img, &minfo);
      }
      else
      {
        str = g_strdup("NULL buffer");
      }
    }
    else
    {
      str = g_strdup_value_contents(gst_tag_list_get_value_index(list, tag, i));
    }

    if (i == 0) {
      if (strncmp(gst_tag_get_nick(tag), "datetime", 8) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        GstDateTime *dt = g_value_get_boxed(val);
        gchar *dt_str = gst_date_time_to_iso8601_string(dt);
        strncpy(meta->year, dt_str, sizeof(meta->year));
        meta->year[sizeof(meta->year) - 1] = '\0';
        g_free(dt_str);
      } else if (strncmp(gst_tag_get_nick(tag), "date", 4) == 0) {
        strncpy(meta->year, str, sizeof(meta->year));
        meta->year[sizeof(meta->year) - 1] = '\0';
      }else if( strncmp(gst_tag_get_nick(tag), "container format", 16) == 0 ) {
        strncpy(meta->container, str, sizeof(meta->container));
        meta->container[sizeof(meta->container) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "location", 8) == 0) {
        strncpy(meta->pathname, str, sizeof(meta->pathname));
        meta->pathname[sizeof(meta->pathname) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "title", 5) == 0) {
        strncpy(meta->title, str, sizeof(meta->title));
        meta->title[sizeof(meta->title) - 1] = '\0';
      }else if( strncmp(gst_tag_get_nick(tag), "album artist", 12) == 0 ) {
        strncpy(meta->albumartist, str, sizeof(meta->albumartist));
        meta->albumartist[sizeof(meta->albumartist) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "album", 5) == 0) {
        strncpy(meta->album, str, sizeof(meta->album));
        meta->album[sizeof(meta->album) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "artist", 6) == 0) {
        strncpy(meta->artist, str, sizeof(meta->artist));
        meta->artist[sizeof(meta->artist) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "genre", 5) == 0) {
        strncpy(meta->genre, str, sizeof(meta->genre));
        meta->genre[sizeof(meta->genre) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "composer", 8) == 0) {
        strncpy(meta->composer, str, sizeof(meta->composer));
        meta->composer[sizeof(meta->composer) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "copyright", 9) == 0) {
        strncpy(meta->copyright, str, sizeof(meta->copyright));
        meta->copyright[sizeof(meta->copyright) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "description", 11) == 0) {
        strncpy(meta->description, str, sizeof(meta->description));
        meta->description[sizeof(meta->description) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "performer", 9) == 0) {
        strncpy(meta->performer, str, sizeof(meta->performer));
        meta->performer[sizeof(meta->performer) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "keywords", 8) == 0) {
        strncpy(meta->keywords, str, sizeof(meta->keywords));
        meta->keywords[sizeof(meta->keywords) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "comment", 7) == 0) {
        strncpy(meta->comment, str, sizeof(meta->comment));
        meta->comment[sizeof(meta->comment) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "application name", 16) == 0) {
        strncpy(meta->tool, str, sizeof(meta->tool));
        meta->tool[sizeof(meta->tool) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "geo location latitude", 21) == 0) {
        strncpy(meta->location_latitude, str, sizeof(meta->location_latitude));
        meta->location_latitude[sizeof(meta->location_latitude) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "geo location longitude", 22) == 0) {
        strncpy(meta->location_longtitude, str, sizeof(meta->location_longtitude));
        meta->location_longtitude[sizeof(meta->location_longtitude) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "track count", 11) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->track_count = g_value_get_uint(val);
      }else if (strncmp(gst_tag_get_nick(tag), "track number", 11) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->track_number = g_value_get_uint(val);
      }else if (strncmp(gst_tag_get_nick(tag), "disc number", 10) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->disc_number = g_value_get_uint(val);
      }else if (strncmp(gst_tag_get_nick(tag), "user rating", 11) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->rating = g_value_get_uint(val);
      }
#ifdef GET_STREAM_INFO_FROM_TAGS
      else if (strncmp(gst_tag_get_nick(tag), "audio codec", 11) == 0) {
        strncpy(meta->audiocodec, str, sizeof(meta->audiocodec));
        meta->audiocodec[sizeof(meta->audiocodec) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "video codec", 11) == 0) {
        strncpy(meta->videocodec, str, sizeof(meta->videocodec));
        meta->videocodec[sizeof(meta->videocodec) - 1] = '\0';
      }else if (strncmp(gst_tag_get_nick(tag), "bitrate", 7) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->audiobitrate = g_value_get_uint(val);
      }else if (strncmp(gst_tag_get_nick(tag), "image width", 11) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->width =  g_value_get_uint(val);
      }else if (strncmp(gst_tag_get_nick(tag), "image height", 12) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->height = g_value_get_uint(val);
      }else if (strncmp(gst_tag_get_nick(tag), "frame rate", 10) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->framerate = g_value_get_uint(val);
      }else if (strncmp(gst_tag_get_nick(tag), "video bitrate", 13) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->videobitrate = g_value_get_uint(val);
      }else if (strncmp(gst_tag_get_nick(tag), "number of channels", 18) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->channels = g_value_get_uint(val);
      }else if (strncmp(gst_tag_get_nick(tag), "sampling frequency (Hz)", 23) == 0) {
        const GValue *val;
        val = gst_tag_list_get_value_index(list, tag, i);
        meta->samplerate = g_value_get_uint(val);
      }
#endif
    }
    else {
      continue;
    }
    if(str)
      g_free(str);
  }

  return PLAYENGINE_SUCCESS; 
}

/* Extract some metadata from the streams and print it on the screen */
static PlayEngineResult 
analyze_streams (gpointer data)
{
  gint i;
  GstTagList *tags;
  GstCaps *caps;
  GstStructure *st;
  GstPad *pad;
  gchar *str;
  gint ntracks = 0;
  PlayEngineData *engine_data = (PlayEngineData *)data;

  if (!engine_data)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  /* Read some properties */
  g_object_get (engine_data->bin, "n-video", &engine_data->meta.n_video, NULL);
  g_object_get (engine_data->bin, "n-audio", &engine_data->meta.n_audio, NULL);
  g_object_get (engine_data->bin, "n-text", &engine_data->meta.n_subtitle, NULL);

  ntracks = engine_data->meta.n_video;
  if (ntracks > MAX_VIDEO_TRACK_COUNT)
    ntracks = MAX_VIDEO_TRACK_COUNT;

  for (i = 0; i < ntracks; i++) {
    tags = NULL;
    pad = NULL;
    /* Retrieve the stream's video tags */
    g_signal_emit_by_name (engine_data->bin, "get-video-pad", i, &pad);
    if (pad) {
      caps = gst_pad_get_current_caps (pad);
      if (caps) {
        st = gst_caps_get_structure (caps, 0);
        gst_structure_get_int (st, "width", &engine_data->meta.video_info[i].width);
        gst_structure_get_int (st, "height", &engine_data->meta.video_info[i].height);
        gst_structure_get_fraction (st, "framerate",
            &engine_data->meta.video_info[i].framerate_numerator,
            &engine_data->meta.video_info[i].framerate_denominator);
        gst_caps_unref (caps);
      }
      gst_object_unref (pad);
    }

    g_signal_emit_by_name (engine_data->bin, "get-video-tags", i, &tags);
    if (tags) {
      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &str);
      strncpy (engine_data->meta.video_info[i].codec_type, str ? str : "unknown",
               METADATA_ITEM_SIZE_SMALL);
      engine_data->meta.video_info[i].codec_type[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);

      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str);
      strncpy (engine_data->meta.video_info[i].language, str ? str : "unknown",
               METADATA_ITEM_SIZE_SMALL);
      engine_data->meta.video_info[i].language[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);

      gst_tag_list_get_uint (tags, GST_TAG_BITRATE,
                             &engine_data->meta.video_info[i].bitrate);

      gst_tag_list_free (tags);
    }
  }

  ntracks = engine_data->meta.n_audio;
  if (ntracks > MAX_AUDIO_TRACK_COUNT)
    ntracks = MAX_AUDIO_TRACK_COUNT;

  for (i = 0; i < ntracks; i++) {
    tags = NULL;
    pad = NULL;
    /* Retrieve the stream's audio tags */
    g_signal_emit_by_name (engine_data->bin, "get-audio-pad", i, &pad);
    if (pad) {
      caps = gst_pad_get_current_caps (pad);
      if (caps) {
        st = gst_caps_get_structure (caps, 0);
        gst_structure_get_int (st, "rate",
                               &engine_data->meta.audio_info[i].samplerate);
        gst_structure_get_int (st, "channels",
                               &engine_data->meta.audio_info[i].channels);
        gst_caps_unref (caps);
      }
      gst_object_unref (pad);
    }

    g_signal_emit_by_name (engine_data->bin, "get-audio-tags", i, &tags);
    if (tags) {
      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &str);
      strncpy (engine_data->meta.audio_info[i].codec_type, str ? str : "unknown",
               METADATA_ITEM_SIZE_SMALL);
      engine_data->meta.audio_info[i].codec_type[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);

      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str);
      strncpy(engine_data->meta.audio_info[i].language, str ? str : "unknown",
              METADATA_ITEM_SIZE_SMALL);
      engine_data->meta.audio_info[i].language[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);
      gst_tag_list_get_uint (tags, GST_TAG_BITRATE,
                             &engine_data->meta.audio_info[i].bitrate);
      
      /* get album art info */
      GstSample *sample = NULL;
      GstBuffer *buffer = NULL;
      GstMapInfo map;
      gboolean ok;
      engine_data->meta.album_art_info.size = 0;
      ok = gst_tag_list_get_sample(tags, GST_TAG_IMAGE, &sample);
      if (!ok)
          ok = gst_tag_list_get_sample(tags, GST_TAG_PREVIEW_IMAGE, &sample);
      if (ok) {
          caps = gst_sample_get_caps(sample);
          if (caps) {
            st = gst_caps_get_structure(caps, 0);
            gst_structure_get_int(st, "width",
                                &engine_data->meta.album_art_info.width);
            gst_structure_get_int(st, "height",
                                &engine_data->meta.album_art_info.height);
          }
          buffer = gst_sample_get_buffer(sample);
          gst_buffer_map(buffer, &map, GST_MAP_READ);          
          //g_error ("album art size is: %d\n", map.size);
          malloc_image(&engine_data->meta.album_art_info, map.size);
          memcpy(engine_data->meta.album_art_info.image, map.data, map.size);

          gst_buffer_unmap(buffer, &map);
          gst_sample_unref(sample);
        }
      
      gst_tag_list_free (tags);
    }
  }

  ntracks = engine_data->meta.n_subtitle;
  if (ntracks > MAX_SUBTITLE_TRACK_COUNT)
    ntracks = MAX_SUBTITLE_TRACK_COUNT;
  for (i = 0; i < ntracks; i++) {
    tags = NULL;
    /* Retrieve the stream's subtitle tags */
    g_signal_emit_by_name (engine_data->bin, "get-text-tags", i, &tags);
    if (tags) {
      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_SUBTITLE_CODEC, &str);
      strncpy (engine_data->meta.subtitle_info[i].codec_type, str ? str : "unknown",
               METADATA_ITEM_SIZE_SMALL);
      engine_data->meta.subtitle_info[i].codec_type[METADATA_ITEM_SIZE_SMALL-1]='\0';
      g_free (str);

      str = NULL;
      gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str);
      strncpy(engine_data->meta.subtitle_info[i].language, str ? str : "unknown",
              METADATA_ITEM_SIZE_SMALL);
      engine_data->meta.subtitle_info[i].language[METADATA_ITEM_SIZE_SMALL-1] = '\0';
      g_free (str);

      gst_tag_list_free (tags);
    }
  }

  g_object_get (engine_data->bin, "current-video", &engine_data->cur_video, NULL);
  g_object_get (engine_data->bin, "current-audio", &engine_data->cur_audio, NULL);
  g_object_get (engine_data->bin, "current-text", &engine_data->cur_subtitle, NULL);

  return PLAYENGINE_SUCCESS;
}

static gboolean 
bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
  PlayEngine *engine = (PlayEngine *)data;
  PlayEngineData *engine_data = NULL;

  if (!engine || !engine->priv)
  {
    return FALSE;
  }

  engine_data = (PlayEngineData *)engine->priv;

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:{
      g_message("end-of-stream");
      if (engine_data->app_callback)
        (*(engine_data->app_callback))(engine_data->UserCustomData,
                                        EVENT_ID_EOS, NULL);
    }
    break;

    case GST_MESSAGE_ERROR:{
      gchar *debug = NULL;
      GError *err = NULL;

      gst_message_parse_error(msg, &err, &debug);

      if (debug)
        g_message("Debug: %s\n", debug);
      g_free(debug);

      if (engine_data->app_callback)
        if(err)
          (*(engine_data->app_callback))(engine_data->UserCustomData,
                                          EVENT_ID_ERROR, err->message);
        else
          (*(engine_data->app_callback))(engine_data->UserCustomData,
                                          EVENT_ID_ERROR, NULL);
      g_error_free(err);
    }
    break;

    case GST_MESSAGE_TAG:{
        GstTagList *tags;
        gst_message_parse_tag(msg, &tags);
        gst_tag_list_foreach(tags, get_metadata_tag, data);
        gst_tag_list_free(tags);
    }
    break;

    case GST_MESSAGE_STATE_CHANGED:{
      PlayEngineState state;
      gst_message_parse_state_changed (msg, &state.old_st, &state.new_st,
                                                          &state.pending_st);
      if (GST_MESSAGE_SRC (msg) == GST_OBJECT (engine_data->bin)) {
        /* inform application state changed */
        if (engine_data->app_callback)
          (*(engine_data->app_callback))(engine_data->UserCustomData, 
                                          EVENT_ID_STATE_CHANGE,
                                          &state);
      }
    }
    break;

    case GST_MESSAGE_ELEMENT:{
      gchar * msgstr;
      if (msgstr = gst_structure_to_string(gst_message_get_structure (msg))){
        g_message("get GST_MESSAGE_ELEMENT %s\n", msgstr);
        g_free(msgstr);
      }
    }
    break;

    case GST_MESSAGE_BUFFERING:{
      gint percent = 0;
      gst_message_parse_buffering(msg, &percent);
      if (engine_data->app_callback)
        (*(engine_data->app_callback))(engine_data->UserCustomData,
                                        EVENT_ID_BUFFERING, &percent);
    }
    break;

    default:
    break;
  }

  return TRUE;
}

static PlayEngineResult
playengine_get_position(PlayEngineHandle handle,
                        gint64 *position)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && position && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    if (!gst_element_query_position(engine_data->pipeline, GST_FORMAT_TIME, position))
    {
      *position = 0;
      return PLAYENGINE_FAILURE;
    }
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_duration(PlayEngineHandle handle,
                        gint64 *duration)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && duration && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    if(!gst_element_query_duration(engine_data->pipeline, GST_FORMAT_TIME, duration))
    {
      *duration = 0;
      return PLAYENGINE_FAILURE;
    }
    engine_data->duration = *duration;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_set_file(PlayEngineHandle handle,const gchar *filename)
{
  gchar *uri = NULL;
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && filename && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    uri = filename2uri (filename);
    g_object_set(G_OBJECT(engine_data->bin), "uri", uri, NULL);
    g_free(uri);
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_play(PlayEngineHandle handle)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData * engine_data = NULL;
  GstState current;
  GstStateChangeReturn result;

  if (engine && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    g_mutex_lock(&engine_data->mutex);
    result = gst_element_set_state(engine_data->pipeline, GST_STATE_PLAYING);

    if(result == GST_STATE_CHANGE_FAILURE)
    {
      g_mutex_unlock(&engine_data->mutex);
      g_warning("Gstreamer state change failed : play.\n");
      return PLAYENGINE_FAILURE;
    }else{
      if(wait_for_state_change(engine, GST_STATE_PLAYING, engine_data->timeout_second) 
          != PLAYENGINE_SUCCESS)
      {
        g_mutex_unlock(&engine_data->mutex);
        return PLAYENGINE_FAILURE;
      }
    }

    g_mutex_unlock(&engine_data->mutex);
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_stop(PlayEngineHandle handle)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  GstStateChangeReturn result;

  if (engine && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    g_mutex_lock(&engine_data->mutex);
    result = gst_element_set_state(engine_data->pipeline, GST_STATE_NULL);

    if(result == GST_STATE_CHANGE_FAILURE)
    {
      g_mutex_unlock(&engine_data->mutex);
      g_warning("Gstreamer state change failed : stop.\n");
      return PLAYENGINE_FAILURE;
    }else{
      if(wait_for_state_change(engine, GST_STATE_NULL, engine_data->timeout_second) 
          != PLAYENGINE_SUCCESS)
      {
        g_mutex_unlock(&engine_data->mutex);
        return PLAYENGINE_FAILURE;
      }
    }
    
    engine_data->play_rate = 1.0;
    g_mutex_unlock(&engine_data->mutex);
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_pause(PlayEngineHandle handle)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  GstStateChangeReturn result;

  if (engine && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    g_mutex_lock(&engine_data->mutex);
    result = gst_element_set_state(engine_data->pipeline, GST_STATE_PAUSED);

    if(result == GST_STATE_CHANGE_FAILURE)
    {
      g_mutex_unlock(&engine_data->mutex);
      g_warning("Gstreamer state change failed : pause.\n");
      return PLAYENGINE_FAILURE;
    }else{
      if(wait_for_state_change(engine, GST_STATE_PAUSED, engine_data->timeout_second) 
          != PLAYENGINE_SUCCESS)
      {
        g_mutex_unlock(&engine_data->mutex);
        return PLAYENGINE_FAILURE;
      }
    }
    g_mutex_unlock(&engine_data->mutex);
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_seek(PlayEngineHandle handle,
                guint64 value,
                gboolean accurate)
{
  GstEvent *seek = NULL;
  GstSeekFlags flag = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT;
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  guint64 seekpos;

  if (engine && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    if(engine_data->duration == 0)
      playengine_get_duration(engine, &engine_data->duration);

    seekpos = value*GST_SECOND;
    if(seekpos > engine_data->duration)
    {
        g_warning("Seek Failed: Invalid seek position=%u(s)!\n", value);
        return PLAYENGINE_ERROR_BAD_PARAM;
    }

    g_message("seeking: %"GST_TIME_FORMAT"/%"GST_TIME_FORMAT"",
                        GST_TIME_ARGS(seekpos),
                        GST_TIME_ARGS(engine_data->duration));
    if (accurate)
      flag |= GST_SEEK_FLAG_ACCURATE;

    seek = gst_event_new_seek(1.0, GST_FORMAT_TIME, flag, GST_SEEK_TYPE_SET,
                              seekpos, GST_SEEK_TYPE_SET, engine_data->duration);
    if(!gst_element_send_event(engine_data->bin, seek)){
      g_warning("Send seek Failed\n");
      return PLAYENGINE_FAILURE;
    }
    engine_data->play_rate = 1.0;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }
  return PLAYENGINE_SUCCESS;
}

#define EPSINON 0.00001
static PlayEngineResult 
playengine_set_play_rate(PlayEngineHandle handle,
                          double rate)
{
  GstEvent *set_playback_rate_event = NULL;
  gint64 cur = 0;
  GstQuery* query;
  gint32 try_cnt = 20;
  PlayEngineData * engine_data = NULL;
  PlayEngine *engine = (PlayEngine *)handle;

  if(rate > 16.0 || rate < -16.0 || (rate >= -EPSINON && rate <= EPSINON)) 
  {
    g_print("Invalid rate=%lf, should be between [-16.0, 16.0] and not be 0.0!\n", rate);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }
  
  if (!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  query = gst_query_new_position(GST_FORMAT_TIME);

  while (try_cnt--) {
    if( gst_element_query(engine_data->bin, query) ) {
      gst_query_parse_position(query,NULL,&cur);
      g_message("current_position = %"GST_TIME_FORMAT"", GST_TIME_ARGS (cur));
      break;
    } else {
      g_warning ("current_postion query failed...\n");
      return PLAYENGINE_FAILURE;
    }
  }

  if (try_cnt <= 0) {
    g_warning ("can't get current position, set play rate failed\n");
    return PLAYENGINE_FAILURE;
  }

  gst_element_query_duration(engine_data->bin, GST_FORMAT_TIME, &(engine_data->duration));

  if( rate >= 0.0 ) {
    set_playback_rate_event = gst_event_new_seek(rate, GST_FORMAT_TIME,
                GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                GST_SEEK_TYPE_SET, cur, GST_SEEK_TYPE_SET, engine_data->duration);
  } else {
    set_playback_rate_event = gst_event_new_seek(rate, GST_FORMAT_TIME,
                GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, cur);
  }

  if(gst_element_send_event(engine_data->bin, set_playback_rate_event) == FALSE) {
    g_warning("Send setting playback rate event failed!\n");
    return PLAYENGINE_FAILURE;
  }
  engine_data->play_rate = rate;

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_get_play_rate(PlayEngineHandle handle,
                          gdouble *playback_rate)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && playback_rate && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    *playback_rate = engine_data->play_rate;
    return PLAYENGINE_SUCCESS;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }
}

static PlayEngineResult 
playengine_set_rotate(PlayEngineHandle handle,
                      gint rotation)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  GstElement* actual_video_sink = NULL;
  GstObjectClass * gstobjclass = NULL;

  if (engine && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    actual_video_sink = get_video_sink (engine);
    if (!actual_video_sink) {
      g_warning("Can't get video sink.\n");
      return PLAYENGINE_FAILURE;
    }
    /* check if the element has "rotate" and "reconfig" property */
    gstobjclass = G_OBJECT_GET_CLASS(G_OBJECT(actual_video_sink));
    if(g_object_class_find_property (gstobjclass,"rotate")
      && g_object_class_find_property (gstobjclass,"reconfig"))
    {
      g_object_set(G_OBJECT(actual_video_sink), "rotate", rotation/90, NULL);
      g_object_set(G_OBJECT(actual_video_sink), "reconfig", 1, NULL);
      engine_data->rotation = rotation;
    } else if (g_object_class_find_property (gstobjclass,"rotate-method"))
    {
      g_object_set(G_OBJECT(actual_video_sink), "rotate-method", rotation/90, NULL);
      engine_data->rotation = rotation;
    }
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_rotate(PlayEngineHandle handle,
                      gint *rotation)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  GstElement * actual_video_sink = NULL;
  GstObjectClass * gstobjclass = NULL;

  if (engine && rotation && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    actual_video_sink = get_video_sink (engine);

    if (!actual_video_sink) {
      g_warning("Can't get video sink.\n");
      return PLAYENGINE_FAILURE;
    }

    /* check if the element has "rotate" property */
    gstobjclass = G_OBJECT_GET_CLASS(G_OBJECT(actual_video_sink));
    if(g_object_class_find_property (gstobjclass,"rotate"))
    {
      g_object_get(G_OBJECT(actual_video_sink), "rotate", rotation, NULL);
      *rotation = *rotation * 90;
    } else if (g_object_class_find_property (gstobjclass,"rotate-method"))
    {
      g_object_get(G_OBJECT(actual_video_sink), "rotate-method", rotation, NULL);
      *rotation = *rotation * 90;
    } else {
      *rotation = engine_data->rotation;
    }
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_state(PlayEngineHandle handle,
                      GstState *state)
{
  PlayEngineData *engine_data = NULL;
  PlayEngine *engine = (PlayEngine *)handle;
  GstStateChangeReturn result;

  if (engine && state && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    result = gst_element_get_state(engine_data->pipeline, state, NULL, GST_SECOND);
    if (result == GST_STATE_CHANGE_FAILURE)
      return PLAYENGINE_FAILURE;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_state_change_timeout(PlayEngineHandle handle,
                                    gint timeout_second)
{
  PlayEngineData *engine_data = NULL;
  PlayEngine *engine = (PlayEngine *)handle;

  if (engine && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    engine_data->timeout_second = timeout_second;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_stop_wait_state_change(PlayEngineHandle handle)
{
  PlayEngineData *engine_data = NULL;
  PlayEngine *engine = (PlayEngine *)handle;

  if (engine && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    engine_data->stop_wait = TRUE;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_set_force_ratio (PlayEngineHandle handle,
                            gboolean force)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    g_object_set(G_OBJECT(engine_data->bin), "force-aspect-ratio", force, NULL);
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_get_metadata(PlayEngineHandle handle,
                        imx_metadata *meta)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && meta && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    analyze_streams(engine_data);
    memcpy(meta, &engine_data->meta, sizeof (imx_metadata));
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_subtitle_num(PlayEngineHandle handle, gint *subtitle_num)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && subtitle_num && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    if(engine_data->meta.n_subtitle == 0)
      g_object_get (G_OBJECT(engine_data->bin), "n-text",
                    &engine_data->meta.n_subtitle, NULL);
    *subtitle_num = engine_data->meta.n_subtitle;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_get_audio_num(PlayEngineHandle handle, gint *audio_num)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && audio_num && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    if(engine_data->meta.n_audio == 0)
      g_object_get (G_OBJECT(engine_data->bin), "n-audio",
                    &engine_data->meta.n_audio, NULL);
    *audio_num = engine_data->meta.n_audio;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_get_video_num(PlayEngineHandle handle, gint *video_num)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && video_num && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    if(engine_data->meta.n_video == 0)
      g_object_get (G_OBJECT(engine_data->bin), "n-video",
                    &engine_data->meta.n_video, NULL);
    *video_num = engine_data->meta.n_video;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_get_current_audio_no(PlayEngineHandle handle, gint *cur_audio)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && cur_audio && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    *cur_audio = engine_data->cur_audio;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_get_current_video_no(PlayEngineHandle handle, gint *cur_video)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && cur_video && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    *cur_video = engine_data->cur_video;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_get_current_subtitle_no(PlayEngineHandle handle, gint *cur_subtitle)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && cur_subtitle && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    *cur_subtitle = engine_data->cur_subtitle;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_get_video_thumbnail(PlayEngineHandle handle,
                              gint seconds,
                              imx_image_info *thumbnail)
{
  GstElement *pipeline = NULL;
  GstStructure *st = NULL;
  GstSample *sample = NULL;
  GstCaps *caps = NULL;
  GstBuffer *buffer = NULL;
  GstMapInfo map;
  GstStateChangeReturn ret;
  gint64 position, duration;
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  gint *width = NULL;
  gint *height = NULL;
  PlayEngineResult retVal;
  
  if (engine && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;

    pipeline = engine_data->pipeline;
    width = &engine_data->thumbnail.width;
    height = &engine_data->thumbnail.height;

    gst_element_set_state(pipeline, GST_STATE_PAUSED);

    ret = gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_warning ("Failed to pause file when get video thumbnail\n");
        thumbnail = NULL;
        retVal = PLAYENGINE_FAILURE;
        goto fail;
    }

    g_object_get (engine_data->bin, "n-video", &engine_data->meta.n_video, NULL);
    if (engine_data->meta.n_video == 0) {
        g_warning("No video tracks found.\n");
        thumbnail = NULL;
        retVal = PLAYENGINE_FAILURE;
        goto fail;
    }
    
    gst_element_query_duration(pipeline, GST_FORMAT_TIME, &duration);
    if (duration <= 0) {
        g_warning("cannot get the duration for seek");
        thumbnail = NULL;
        retVal = PLAYENGINE_FAILURE;
        goto fail;
    }

    if (duration < seconds * GST_SECOND)
        position = duration / 100;
    else
        position = seconds * GST_SECOND;

    gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
            GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_FLUSH, position);
    
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    ret = gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_warning("Failed to pause file when get video thumbnail\n");
        thumbnail = NULL;
        retVal = PLAYENGINE_FAILURE;
        goto fail;
    }

    caps = gst_caps_new_simple("video/x-raw",
                               "format", G_TYPE_STRING, "RGB",
                               "width", G_TYPE_INT, 720,
                               "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
                               NULL);

    g_signal_emit_by_name(engine_data->bin, "convert-sample", caps, &sample);
    gst_caps_unref(caps);
    if (!sample) {
        g_warning("Failed to retrieve or convert video frame");
        thumbnail = NULL;
        retVal = PLAYENGINE_FAILURE;
        goto fail;
    }
    
    caps = gst_sample_get_caps(sample);
    if (!caps) {
        g_warning("No caps get, could not take the thumbnail");
        thumbnail = NULL;
        retVal = PLAYENGINE_FAILURE;
        goto fail;
    }
    st = gst_caps_get_structure(caps, 0);
    
    gst_structure_get_int(st, "width", width);
    gst_structure_get_int(st, "height", height);
    if (*width <= 0 || *height <= 0) {
        g_warning("height and width of image are less than 0");
        thumbnail = NULL;
        retVal = PLAYENGINE_FAILURE;
        goto fail;
    }

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &map, GST_MAP_READ);
    malloc_image(&engine_data->thumbnail, map.size);
    memcpy(engine_data->thumbnail.image, map.data, map.size);

    gst_buffer_unmap(buffer, &map);
    gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
        GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_FLUSH, 0);

    memcpy(thumbnail, &engine_data->thumbnail, sizeof(imx_image_info));
    retVal = PLAYENGINE_SUCCESS;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    retVal = PLAYENGINE_ERROR_BAD_PARAM;
  }

fail:
  if (sample)
     gst_sample_unref(sample);
  return retVal;
}

static PlayEngineResult 
playengine_select_subtitle(PlayEngineHandle handle,
                          gint text_no)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && engine->priv) 
  {
    engine_data = (PlayEngineData *)engine->priv;
    GstElement *element =
        gst_bin_get_by_name (GST_BIN (engine_data->bin), "suboverlay");
    if (element) 
    {
      if (0 > text_no) 
      {
        //silent text
        g_object_set(G_OBJECT(element), "silent", TRUE, NULL);
      }else 
      {
        g_object_set(G_OBJECT(element), "silent", FALSE, NULL);
        if (text_no != engine_data->cur_subtitle) 
        {
          if (text_no < engine_data->meta.n_subtitle) 
          {
            g_object_set( engine_data->bin, "current-text", text_no, NULL );
            g_object_get (engine_data->bin, "current-text", &engine_data->cur_subtitle,
                          NULL);
          }else 
          {
            g_warning("subtitle number out of range %d, %d\n", text_no,
                    engine_data->meta.n_subtitle);
            return PLAYENGINE_ERROR_BAD_PARAM;
          }
        }
      }
    }
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_select_audio(PlayEngineHandle handle,
                        gint audio_no)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    if(audio_no != engine_data->cur_audio)
    {
      if (audio_no < engine_data->meta.n_audio)
      {
        g_object_set( engine_data->bin, "current-audio", audio_no, NULL );
        g_object_get( engine_data->bin, "current-audio", &engine_data->cur_audio, NULL );
      }else
      {
        g_warning("audio number out of range %d, %d\n", audio_no,
                engine_data->meta.n_audio);
        return PLAYENGINE_ERROR_BAD_PARAM;
      }
    }
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_select_video(PlayEngineHandle handle, 
                        gint video_no)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    if(video_no != engine_data->cur_video)
    {
      if (video_no < engine_data->meta.n_video)
      {
        g_object_set( engine_data->bin, "current-video", video_no, NULL );
        g_object_get( engine_data->bin, "current-video", &engine_data->cur_video, NULL );
      }else
      {
        g_warning("video number out of range %d, %d\n", video_no,
                engine_data->meta.n_video);
        return PLAYENGINE_ERROR_BAD_PARAM;
      }
    }
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

/*
 * return: -1 : No text got, 0 : EOS, 1 : got a text
 */
static PlayEngineResult
playengine_get_subtitle_text(PlayEngineHandle handle,
                            gchar *text, guint32 len,
                            guint64 *duration,
                            guint64 *pts)
{
  PlayEngineResult ret = PLAYENGINE_FAILURE;
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && text && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    if (GST_IS_APP_SINK(engine_data->text_sink)) {
      GstSample *sample =
          gst_app_sink_pull_sample(GST_APP_SINK(engine_data->text_sink));
      //g_error("%s(): %d, sample=%p\n", __FUNCTION__, __LINE__, sample);
      if (sample) {
        GstBuffer *buf = gst_sample_get_buffer(sample);
        if (buf) {
          GstMapInfo mapinfo;
          gst_buffer_map(buf, &mapinfo, GST_MAP_READ);
          if (len > mapinfo.size)
            len = mapinfo.size;
          memcpy(text, mapinfo.data, len);
          //g_error("%s:%s\n", __FUNCTION__, text);

          if (duration)
            *duration = buf->duration;
          if (pts)
            *pts = buf->pts;
          gst_buffer_unmap(buf, &mapinfo);
          ret = PLAYENGINE_SUCCESS;
        }
        gst_sample_unref(sample);
      }

      if (gst_app_sink_is_eos (GST_APP_SINK(engine_data->text_sink))) {
        ret = PLAYENGINE_FAILURE;
      }
    } else {
      //what about other text sink?
      g_warning("unsupported text sink\n");
      ret = PLAYENGINE_ERROR_NOT_SUPPORT;
    }
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    ret = PLAYENGINE_ERROR_BAD_PARAM;
  }

  return ret;
}

static PlayEngineResult 
playengine_set_text_sink(PlayEngineHandle handle,
                        const gchar *sink_name)
{
  GstStateChangeReturn ret;
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && sink_name && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;

    gst_element_set_state(engine_data->pipeline, GST_STATE_NULL);

    if(wait_for_state_change(engine, GST_STATE_NULL, engine_data->timeout_second) 
        != PLAYENGINE_SUCCESS)
    {
      g_warning("failed to stop of pipeline while setting text sink\n");
      return PLAYENGINE_FAILURE;
    }

    engine_data->text_sink = gst_parse_launch(sink_name, NULL);
    if (engine_data->text_sink){
      g_object_set(G_OBJECT(engine_data->bin), "text-sink", engine_data->text_sink, NULL);
      if (GST_IS_APP_SINK(engine_data->text_sink)) {
        g_message("text sink set to appsink");
        gst_app_sink_set_max_buffers(GST_APP_SINK(engine_data->text_sink),
                                     MAX_SUBTITLE_BUF_COUNT);
        gst_app_sink_set_drop (GST_APP_SINK(engine_data->text_sink), TRUE);
      }
      return PLAYENGINE_SUCCESS;
    }
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_FAILURE;
}

static PlayEngineResult
playengine_set_audio_sink(PlayEngineHandle handle,
                          const gchar *sink_name)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  GstElement *audio_sink = NULL;
  
  if(!engine || !engine->priv || !sink_name || !strlen(sink_name))
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;

  audio_sink = gst_parse_bin_from_description(sink_name, TRUE, NULL);
  if(audio_sink == NULL)
  {
    g_error("Error: not support %s\n", sink_name);
    return PLAYENGINE_FAILURE;
  }
  engine_data->audio_sink = audio_sink;
  g_object_set(G_OBJECT(engine_data->bin), "audio-sink", audio_sink, NULL);

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_video_sink(PlayEngineHandle handle,
                          const gchar *sink_name)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  GstElement *video_sink = NULL;
  
  if(!engine || !engine->priv || !sink_name || !strlen(sink_name))
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;

  video_sink = gst_parse_bin_from_description(sink_name, TRUE, NULL);
  if(video_sink == NULL)
  {
    g_error("Error: not support %s\n", sink_name);
    return PLAYENGINE_FAILURE;
  }
  engine_data->video_sink = video_sink;
  g_object_set(G_OBJECT(engine_data->bin), "video-sink", video_sink, NULL);

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_video_sink_element(PlayEngineHandle handle,
                          GstElement* sink)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if(!engine || !engine->priv || !sink)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }
  g_print("set sink element %p\n",sink);

  engine_data = (PlayEngineData *)engine->priv;

  engine_data->video_sink = sink;
  g_object_set(G_OBJECT(engine_data->bin), "video-sink", sink, NULL);

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_visual(PlayEngineHandle handle,
                      const gchar *visual_name)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  GstElement *visual_plugin = NULL;
  
  if(!engine || !engine->priv || !visual_name || !strlen(visual_name))
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;

  visual_plugin = gst_parse_launch(visual_name, NULL);
  if(visual_plugin == NULL)
  {
    g_error("Error: not support %s\n", visual_name);
    return PLAYENGINE_FAILURE;
  }
  engine_data->visual_plugin = visual_plugin;
  g_object_set(G_OBJECT(engine_data->bin), "vis-plugin", visual_plugin, NULL);

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_subtitle_uri(PlayEngineHandle handle,
                            const gchar *filename)
{
  gchar *uri_buffer = NULL;
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    if(filename)
    {
      uri_buffer = filename2uri (filename);
      g_object_set(G_OBJECT(engine_data->bin), "suburi", uri_buffer, NULL);
      g_message("%s(): suburi=%s\n", __FUNCTION__, uri_buffer);
      g_free(uri_buffer);
    }else
    {
      g_object_set(G_OBJECT(engine_data->bin), "suburi", NULL, NULL);
    }
    return PLAYENGINE_SUCCESS;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_FAILURE;
}

static PlayEngineResult 
playengine_set_volume(PlayEngineHandle handle, 
                      gdouble volume)
{
  GValue value  = {0};
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  if (engine && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    if( volume >= 0.0 && volume <= 1.0 ) {
      g_value_init(&value, G_TYPE_DOUBLE);
      g_value_set_double(&value, volume);
      g_object_set_property(G_OBJECT(engine_data->bin), "volume", &value);
      g_value_set_double(&value, 0.0);
      g_object_get_property(G_OBJECT(engine_data->bin), "volume", &value);
      engine_data->volume = g_value_get_double(&value);
    } else {
      g_warning("Volume out of range %f", volume);
      return PLAYENGINE_ERROR_BAD_PARAM;
    }
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_volume(PlayEngineHandle handle,
                      gdouble *volume)
{
  GValue value  = {0};
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData * engine_data = NULL;

  g_value_init(&value, G_TYPE_DOUBLE);
  if (engine && volume && engine->priv)
  {
    engine_data = (PlayEngineData *)engine->priv;
    g_object_get_property(G_OBJECT(engine_data->bin), "volume", &value);
    *volume = g_value_get_double(&value);
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_set_mute(PlayEngineHandle handle,
                    gboolean mute)
{
  GValue value  = {0};
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  gdouble tmp_volume;

  if (engine && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;
    g_value_init(&value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&value, mute);
    g_object_set_property(G_OBJECT(engine_data->bin), "mute", &value);

    engine_data->bmute = mute;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_mute(PlayEngineHandle handle,
                    gboolean *mute_stat)
{
  PlayEngine *engine = (PlayEngine *)handle;
  if(engine && mute_stat && engine->priv)
  {
    PlayEngineData *engine_data = (PlayEngineData *)engine->priv;
    *mute_stat = engine_data->bmute;
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}


static PlayEngineResult 
playengine_get_seekable(PlayEngineHandle handle,
                        gboolean *seekable)
{
  GstQuery *query;
  gboolean res;
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;

  if (engine && seekable && engine->priv) {
    *seekable = FALSE;
    engine_data = (PlayEngineData *)engine->priv;
    query = gst_query_new_seeking (GST_FORMAT_TIME);
    if (gst_element_query (engine_data->bin, query)) {
      gst_query_parse_seeking (query, NULL, &res, NULL, NULL);
      if (res)
        *seekable = TRUE;
    }
    gst_query_unref (query);
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_set_window(PlayEngineHandle handle,
                      guintptr winhandle)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  GstElement * actual_video_sink = NULL;

  if (engine && engine->priv) {
    engine_data = (PlayEngineData *)engine->priv;

    actual_video_sink = get_video_sink (engine);
    if (!actual_video_sink) {
      g_warning("Can't get video sink.\n");
      return PLAYENGINE_FAILURE;
    }
#ifndef PREPARE_WINDOW_MESSAGE
    if (GST_IS_VIDEO_OVERLAY (actual_video_sink)) {
      g_message("videosink is overlay");
      gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(actual_video_sink),
                                          winhandle);
    } else {
      g_message("videosink is not overlay");
    }
#else
    engine_data->video_window_handle = winhandle;
#endif
  }else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_set_render_rect(PlayEngineHandle handle,
                          DisplayArea area)
{
  PlayEngine *engine = (PlayEngine *)handle;
  gboolean result;
  GstElement * actual_video_sink = NULL;

  if (!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  PlayEngineData *engine_data = (PlayEngineData *)engine->priv;

  actual_video_sink = get_video_sink (engine);
  if (!actual_video_sink) {
    g_warning("Can't get video sink.\n");
    return PLAYENGINE_FAILURE;
  }

  if (GST_IS_VIDEO_OVERLAY(actual_video_sink)) {
    result = gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(actual_video_sink)
                                           ,area.offsetx, area.offsety, area.width, area.height);
    if(result == FALSE)
    {
      g_warning("Failed to set render rectangle.");
      return PLAYENGINE_FAILURE;
    }

    engine_data->area.offsetx = area.offsetx;
    engine_data->area.offsety = area.offsety;
    engine_data->area.width = area.width;
    engine_data->area.height = area.height;

  }else
  {
    g_warning("%s video sink do not support video overlay interface\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_fullscreen(PlayEngineHandle handle)
{
  PlayEngine *engine = (PlayEngine *)handle;
  gint32 fb;
  gint32 width = 0;
  gint32 height = 0;
  DisplayArea area = {0};

  if (!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  PlayEngineData *engine_data = (PlayEngineData *)engine->priv;
  if(get_fullscreen_size(&width, &height) != PLAYENGINE_SUCCESS)
  {
    g_warning("Can not get fullscreen size\n");
    return PLAYENGINE_FAILURE;
  }
  area.width = width;
  area.height = height;
  area.offsetx = 0;
  area.offsety = 0;

  if(playengine_set_render_rect(handle,area) != PLAYENGINE_SUCCESS)
  {
    g_warning("Can not set full screen\n");
    return PLAYENGINE_FAILURE;
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_expose_video(PlayEngineHandle handle)
{
  PlayEngine *engine = (PlayEngine *)handle;
  GstElement *actual_video_sink = NULL;

  if (!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  PlayEngineData *engine_data = (PlayEngineData *)engine->priv;

  actual_video_sink = get_video_sink (engine);
  if (!actual_video_sink) {
    g_warning("Can't get video sink.\n");
    return PLAYENGINE_FAILURE;
  }

  if (engine_data && GST_IS_VIDEO_OVERLAY(actual_video_sink))
    gst_video_overlay_expose(GST_VIDEO_OVERLAY(actual_video_sink));
  else
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_display_area(PlayEngineHandle handle,
                            DisplayArea *area)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  GstElement *actual_video_sink = NULL;
  GstObjectClass * gstobjclass = NULL;

  if (!engine || !area || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;

  actual_video_sink = get_video_sink (engine);
  if (!actual_video_sink) {
    g_warning("Can't get video sink.\n");
    return PLAYENGINE_FAILURE;
  }

  /* check if the element has display area property. This is not a comment
   * solution, because there is no display area get API in videooverlay 
   * interface. Will submit a new bug to upstream about this question*/
  gstobjclass = G_OBJECT_GET_CLASS(G_OBJECT(actual_video_sink));
  if( g_object_class_find_property (gstobjclass,"overlay-left")
    &&g_object_class_find_property (gstobjclass,"overlay-top")
    &&g_object_class_find_property (gstobjclass,"overlay-width")
    &&g_object_class_find_property (gstobjclass,"overlay-height")
    )
  {
    g_object_get(G_OBJECT(actual_video_sink), "overlay-left", &area->offsetx, NULL);
    g_object_get(G_OBJECT(actual_video_sink), "overlay-top", &area->offsety, NULL);
    g_object_get(G_OBJECT(actual_video_sink), "overlay-width", &area->width, NULL);
    g_object_get(G_OBJECT(actual_video_sink), "overlay-height", &area->height, NULL);
  }else{
     memcpy(area, &engine_data->area, sizeof(DisplayArea));
  }
  return PLAYENGINE_SUCCESS;
}

void 
g_main_loop_thread_fun(gpointer data)
{
  if (!data)
  {
    g_error("main loop thread start failed\n");
    return;
  }

  PlayEngineData *engine_data = (PlayEngineData *)data;

  if (engine_data && engine_data->g_main_loop)
    g_main_loop_run (engine_data->g_main_loop);

  return;
}

static PlayEngineResult
reg_event_handler(PlayEngineHandle handle,
                  void *context,
                  PlayEngineEventHandler handler)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  if( !engine || !engine->priv || !context || !handler )
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;

  engine_data->UserCustomData = context;
  engine_data->app_callback = handler;

  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult 
playengine_set_subtitle_font (PlayEngineHandle handle,
                              gchar *font_desc)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv || !font_desc)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
    return PLAYENGINE_ERROR_BAD_PARAM;

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");//textoverlay
  if (element) {
    g_object_set(G_OBJECT(element), "font-desc", font_desc, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_subtitle_color (PlayEngineHandle handle,
                              guint argb)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay"); //textoverlay
  if (element) {
    g_object_set(G_OBJECT(element), "color", argb, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_subtitle_outline_color (PlayEngineHandle handle,
                                        guint argb)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");
  if (element) {
    g_object_set(G_OBJECT(element), "outline-color", argb, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_subtitle_shaded_background (PlayEngineHandle handle,
                                          gboolean enable)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");
  if (element) {
    g_object_set(G_OBJECT(element), "shaded-background", enable, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_subtitle_halignment (PlayEngineHandle handle,
                                    SubtitleHAlign mode,
                                    gdouble xpos)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");
  if (element) {
    g_object_set(G_OBJECT(element), "halignment", mode, NULL);
    if (SUBTITLE_H_ALIGN_POSITION == mode)
      g_object_set(G_OBJECT(element), "xpos", xpos, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_set_subtitle_valignment (PlayEngineHandle handle,
                                    SubtitleVAlign mode,
                                    gdouble ypos)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }
  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");
  if (element) {
    g_object_set(G_OBJECT(element), "valignment", mode, NULL);
    if (SUBTITLE_V_ALIGN_POSITION == mode)
      g_object_set(G_OBJECT(element), "ypos", ypos, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_subtitle_font (PlayEngineHandle handle,
                              gchar *font_desc)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv || !font_desc)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");
  if (element) {
    g_object_get(G_OBJECT(element), "font-desc", font_desc, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_subtitle_color (PlayEngineHandle handle,
                                guint *argb)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv || !argb)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");

  if (element) {
    g_object_get(G_OBJECT(element), "color", argb, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_subtitle_outline_color (PlayEngineHandle handle,
                                        guint *argb)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv || !argb)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");
  if (element) {
    g_object_get(G_OBJECT(element), "outline-color", argb, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_subtitle_shaded_background (PlayEngineHandle handle,
                                            gboolean *enable)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv || !enable)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");
  if (element) {
    g_object_get(G_OBJECT(element), "shaded-background", enable, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_subtitle_halignment (PlayEngineHandle handle,
                                    SubtitleHAlign *mode,
                                    gdouble *xpos)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv || !mode)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");
  if (element) {
    g_object_get(G_OBJECT(element), "halignment", mode, NULL);
    if (SUBTITLE_H_ALIGN_POSITION == *mode && xpos)
      g_object_get(G_OBJECT(element), "xpos", xpos, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

static PlayEngineResult
playengine_get_subtitle_valignment (PlayEngineHandle handle,
                                    SubtitleVAlign *mode,
                                    gdouble *ypos)
{
  PlayEngine *engine = (PlayEngine *)handle;
  PlayEngineData *engine_data = NULL;
  
  if (!engine || !engine->priv || !mode)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  engine_data = (PlayEngineData *)engine->priv;
  if (!engine_data->bin)
  {
    g_error("Error:%s Invalid pointer is used\n", __FUNCTION__);
    return PLAYENGINE_ERROR_BAD_PARAM;
  }

  GstElement *element =
      gst_bin_get_by_name (GST_BIN (engine_data->bin), "overlay");
  if (element) {
    g_object_get(G_OBJECT(element), "valignment", mode, NULL);
    if (SUBTITLE_V_ALIGN_POSITION == *mode && ypos)
      g_object_get(G_OBJECT(element), "ypos", ypos, NULL);
  }
  return PLAYENGINE_SUCCESS;
}

PlayEngine * 
play_engine_create()
{
  PlayEngine *engine = NULL;
  PlayEngineData *engine_data = NULL;

  guint major, minor, micro, nano;
  GMainContext *ctx;

  //require the memory need by PlayEngine
  engine = (PlayEngine *)malloc(sizeof(PlayEngine));
  if(!engine)
  {
    g_warning("malloc play engine failed\n");
    return NULL;
  }else
  {
    memset (engine, 0, sizeof(PlayEngine));
  }

  //require the memory need by PlayEngineData
  engine_data = (PlayEngineData *)malloc(sizeof(PlayEngineData));
  if(!engine_data)
  {
    g_warning("malloc play engine data failed\n");
    free (engine);
    return NULL;
  }else
  {
    memset (engine_data, 0, sizeof(PlayEngineData));
  }

  gst_init(NULL, NULL);
  gst_version (&major, &minor, &micro, &nano);

  g_message ("GStreamer version %d.%d.%d", major, minor, micro);

  engine_data->pipeline = gst_pipeline_new("gst-player");
  engine_data->bin = gst_element_factory_make("playbin", "bin");

  if(!engine_data->pipeline || !engine_data->bin)
  {
    g_warning("%s create pipeline failed", __FUNCTION__);
    if(engine_data->pipeline)
      gst_object_unref(GST_OBJECT(engine_data->pipeline));
    if(engine_data->bin)
      gst_object_unref(GST_OBJECT(engine_data->bin));

    free (engine_data);
    free (engine);
    return NULL;
  }
  gst_bin_add(GST_BIN(engine_data->pipeline), engine_data->bin);
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(engine_data->pipeline));
#ifdef PREPARE_WINDOW_MESSAGE
  gst_bus_set_sync_handler(bus, (GstBusSyncHandler)bus_sync_cb, engine, NULL);
#endif

  g_mutex_init(&engine_data->mutex);

  engine_data->g_main_context = ctx = g_main_context_new ();
  engine_data->g_main_loop = g_main_loop_new (ctx, FALSE);
  g_main_context_push_thread_default (ctx);

  gst_bus_add_watch(bus, bus_cb, engine);
  gst_object_unref(bus);

  g_main_context_pop_thread_default (ctx);
  engine_data->g_main_loop_thread = g_thread_new("main_loop_thread",
                      (GThreadFunc)g_main_loop_thread_fun, engine_data);

  engine_data->area.offsetx = 0;
  engine_data->area.offsety = 0;
  if(get_fullscreen_size(&engine_data->area.width, &engine_data->area.height) != PLAYENGINE_SUCCESS)
  {
    g_warning("Can not get fullscreen size\n");
    return NULL;
  }

  engine_data->bmute = FALSE;
  engine_data->volume = 1.0;
  engine_data->play_rate = 1.0;
  engine_data->timeout_second = PLAYENGINE_DEFAULT_TIME_OUT;//default 20s
  engine_data->stop_wait = FALSE;
  engine_data->rotation = 0;
  /* meta data should be initialize here but not in play function */
  memset(&engine_data->meta, 0 , sizeof(imx_metadata));

  engine->set_file = playengine_set_file;
  engine->play = playengine_play;
  engine->stop = playengine_stop;
  engine->pause = playengine_pause;
  engine->seek = playengine_seek;
  engine->set_play_rate = playengine_set_play_rate;
  engine->get_play_rate = playengine_get_play_rate;
  engine->set_rotate = playengine_set_rotate;
  engine->get_rotate = playengine_get_rotate;
  engine->force_ratio = playengine_set_force_ratio;
  engine->get_state = playengine_get_state;
  engine->set_state_change_timeout = playengine_set_state_change_timeout;
  engine->stop_wait_state_change = playengine_stop_wait_state_change;
  engine->get_position = playengine_get_position;
  engine->get_duration = playengine_get_duration;
  engine->get_metadata = playengine_get_metadata;
  engine->get_subtitle_num = playengine_get_subtitle_num;
  engine->get_audio_num = playengine_get_audio_num;
  engine->get_video_num = playengine_get_video_num;
  engine->get_cur_subtitle_no = playengine_get_current_subtitle_no;
  engine->get_cur_audio_no = playengine_get_current_audio_no;
  engine->get_cur_video_no = playengine_get_current_video_no;
  engine->get_video_thumbnail = playengine_get_video_thumbnail;
  engine->set_volume = playengine_set_volume;
  engine->get_volume = playengine_get_volume;
  engine->set_mute = playengine_set_mute;
  engine->get_mute = playengine_get_mute;
  engine->get_seekable = playengine_get_seekable;
  engine->set_window = playengine_set_window;
  engine->set_render_rect = playengine_set_render_rect;
  engine->set_fullscreen = playengine_set_fullscreen;
  engine->expose_video = playengine_expose_video;
  engine->get_display_area = playengine_get_display_area;
  engine->select_audio = playengine_select_audio;
  engine->select_video = playengine_select_video;
  engine->select_subtitle = playengine_select_subtitle;
  engine->get_subtitle_text = playengine_get_subtitle_text;
  engine->set_subtitle_uri = playengine_set_subtitle_uri;
  engine->set_text_sink = playengine_set_text_sink;
  engine->set_audio_sink = playengine_set_audio_sink;
  engine->set_video_sink = playengine_set_video_sink;
  engine->set_video_sink_element = playengine_set_video_sink_element;
  engine->set_visual = playengine_set_visual;
  engine->set_subtitle_font = playengine_set_subtitle_font;
  engine->set_subtitle_color = playengine_set_subtitle_color;
  engine->set_subtitle_outline_color = playengine_set_subtitle_outline_color;
  engine->set_subtitle_shaded_background = playengine_set_subtitle_shaded_background;
  engine->set_subtitle_halignment = playengine_set_subtitle_halignment;
  engine->set_subtitle_valignment = playengine_set_subtitle_valignment;
  engine->get_subtitle_font = playengine_get_subtitle_font;
  engine->get_subtitle_color = playengine_get_subtitle_color;
  engine->get_subtitle_outline_color = playengine_get_subtitle_outline_color;
  engine->get_subtitle_shaded_background = playengine_get_subtitle_shaded_background;
  engine->get_subtitle_halignment = playengine_get_subtitle_halignment;
  engine->get_subtitle_valignment = playengine_get_subtitle_valignment;
  engine->reg_event_handler = reg_event_handler;
  engine->priv = engine_data;
  
  return engine;
}

void 
play_engine_destroy( PlayEngine *engine )
{
  if (engine)
  {
    PlayEngineData *engine_data = (PlayEngineData *) engine->priv;

    gst_element_set_state(engine_data->pipeline, GST_STATE_NULL);
    if (engine_data->g_main_loop)
    {
      g_main_loop_quit (engine_data->g_main_loop);
      g_main_loop_unref (engine_data->g_main_loop);
    }
    if (engine_data->g_main_context)
    {
      g_main_context_unref (engine_data->g_main_context);
    }
    if (engine_data->g_main_loop_thread)
    {
      g_thread_join (engine_data->g_main_loop_thread);
    }
    if(engine_data->thumbnail.image)
    {
      g_free(engine_data->thumbnail.image);
    }

    g_mutex_clear(&engine_data->mutex);

    gst_object_unref(GST_OBJECT(engine_data->pipeline));

    free( engine->priv );
    free( engine );
    g_message("playengine destroyed");
  }
}

gboolean
play_engine_checkfeature(FeatureType type)
{
  gboolean ret = FALSE;

  switch(type){
    case PLAYENGINE_G2D:
      ret = HAS_G2D();
      break;
    case PLAYENGINE_G3D:
      ret = HAS_G3D();
      break;
    case PLAYENGINE_PXP:
      ret = HAS_PXP();
      break;
    case PLAYENGINE_IPU:
      ret = HAS_IPU();
      break;
    case PLAYENGINE_VPU:
      ret = HAS_VPU();
      break;
    default:
      ret = FALSE;
      break;
  }

  return ret;
}
