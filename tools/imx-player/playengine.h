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

#ifndef IMX_PLAY_ENGINE_H
#define IMX_PLAY_ENGINE_H

#include <gst/gst.h>

//#define PREPARE_WINDOW_MESSAGE
//#define ENABLE_OVERLAY_INTERNEL_WINDOW
//#define VIDEO_SINK_V4L2SINK
//#define GET_STREAM_INFO_FROM_TAGS
//#define PRINT_STREAM_INFO

extern void gtk_main_quit(void);

#define RGB888TORGB565(rgb)\
    ((((rgb)<<8)>>27<<11)|(((rgb)<<18)>>26<<5)|(((rgb)<<27)>>27))

#define RGB565TOCOLORKEY(rgb)                              \
      ( ((rgb & 0xf800)<<8)  |  ((rgb & 0xe000)<<3)  |     \
        ((rgb & 0x07e0)<<5)  |  ((rgb & 0x0600)>>1)  |     \
        ((rgb & 0x001f)<<3)  |  ((rgb & 0x001c)>>2)  )

#define METADATA_ITEM_SIZE_LARGE 256
#define METADATA_ITEM_SIZE_SMALL 64

#define MAX_AUDIO_TRACK_COUNT     8
#define MAX_VIDEO_TRACK_COUNT     8
#define MAX_SUBTITLE_TRACK_COUNT  8

typedef enum
{
  PLAYENGINE_UNKNOWN,
  PLAYENGINE_PLAYING,
  PLAYENGINE_STOPPED,
  PLAYENGINE_PAUSED,
  PLAYENGINE_SEEKING,
  PLAYENGINE_FASTFORWARD,
  PLAYENGINE_FASTREWIND,
  PLAYENGINE_INVALID
} PlayEngineState;

/* audio stream info */
typedef struct {
  gchar codec_type[METADATA_ITEM_SIZE_SMALL];
  gchar language[METADATA_ITEM_SIZE_SMALL];
  gint samplerate;
  gint channels;
  guint bitrate;
} imx_audio_info;

/* video stream info */
typedef struct {
  gchar codec_type[METADATA_ITEM_SIZE_SMALL];
  gchar language[METADATA_ITEM_SIZE_SMALL];
  gint width;
  gint height;
  gint framerate_numerator;
  gint framerate_denominator;
  guint bitrate;
} imx_video_info;

/* subtitle stream info */
typedef struct {
  gchar codec_type[METADATA_ITEM_SIZE_SMALL];
  gchar language[METADATA_ITEM_SIZE_SMALL];
} imx_subtitle_info;

typedef struct
{
    gchar container[METADATA_ITEM_SIZE_SMALL];
    gchar pathname[METADATA_ITEM_SIZE_LARGE];
    gchar title[METADATA_ITEM_SIZE_LARGE];
    gchar artist[METADATA_ITEM_SIZE_LARGE];
    gchar album[METADATA_ITEM_SIZE_LARGE];
    gchar year[METADATA_ITEM_SIZE_SMALL];
    gchar genre[METADATA_ITEM_SIZE_SMALL];
#ifdef GET_STREAM_INFO_FROM_TAGS
    gint width;
    gint height;
    gint framerate;
    gint videobitrate;
    gchar videocodec[METADATA_ITEM_SIZE_SMALL];
    gint channels;
    gint samplerate;
    gint audiobitrate;
    gchar audiocodec[METADATA_ITEM_SIZE_SMALL];
#endif
    gint n_audio;
    gint n_video;
    gint n_subtitle;
    imx_audio_info audio_info[MAX_AUDIO_TRACK_COUNT];
    imx_video_info video_info[MAX_VIDEO_TRACK_COUNT];
    imx_subtitle_info subtitle_info[MAX_SUBTITLE_TRACK_COUNT];
} imx_metadata;

typedef struct _play_engine
{
  GstElement *pipeline;
  GstElement *bin;
  GstElement *video_sink;
  imx_metadata meta;
  gint64 duration;
  double play_rate;
  gint cur_video;
  gint cur_audio;
  gint cur_subtitle;
  void *player;
#ifdef PREPARE_WINDOW_MESSAGE
  guintptr video_window_handle;
#endif

  void (*set_file) (struct _play_engine *engine, gchar *uri);
  void (*play) (struct _play_engine *engine);
  void (*stop) (struct _play_engine *engine);
  void (*pause) (struct _play_engine *engine);
  void (*seek) (struct _play_engine *engine, gint64 value, gboolean accurate);
  void (*set_play_rate) (struct _play_engine *engine, double playback_rate);
  double (*get_play_rate) (struct _play_engine *engine);
  void (*rotate) (struct _play_engine *engine, gint rotation);
  void (*force_ratio) (struct _play_engine *engine, gboolean force);
  PlayEngineState (*get_state) (struct _play_engine *engine);
  void (*set_volume) (struct _play_engine *engine, gdouble volume);
  gdouble (*get_volume) (struct _play_engine *engine);
  void (*set_mute) (struct _play_engine *engine, gboolean mute);
  gboolean (*get_seekable) (struct _play_engine *engine);
  gint (*get_subtitle_num) (struct _play_engine *engine);
  gint (*get_audio_num) (struct _play_engine *engine);
  gint (*get_video_num) (struct _play_engine *engine);
  gint (*get_cur_subtitle_no) (struct _play_engine *engine);
  gint (*get_cur_audio_no) (struct _play_engine *engine);
  gint (*get_cur_video_no) (struct _play_engine *engine);
  void (*select_subtitle) (struct _play_engine *engine, gint text_no);
  void (*select_audio) (struct _play_engine *engine, gint audio_no);
  void (*select_video) (struct _play_engine *engine, gint video_no);
  gboolean (*get_subtitle_text) (struct _play_engine *engine, gchar *text);
  gint64 (*get_duration) (struct _play_engine *engine);
  gint64 (*get_position) (struct _play_engine *engine);
  void (*get_metadata) (struct _play_engine *engine, imx_metadata *meta);

  void (*set_window) (struct _play_engine *engine, guintptr handle);
  void (*set_render_rect) (struct _play_engine *engine,
                            gint x, gint y, gint w, gint h);
  void (*expose_video) (struct _play_engine *engine);

  void (*error_cb) (void* player, const gchar *error_str);
  void (*eos_cb) (void* player);
  void (*state_change_cb) (void* player,
                           GstState old_s, GstState new_s, GstState pending_s);
} play_engine;

play_engine * play_engine_create(int *argc, char **argv[],
              void (*eos_cb)(void *),
              void (*error_cb)(void *, const gchar *),
              void (*state_change_cb)(void *, GstState, GstState, GstState));
void play_engine_destroy(play_engine *engine);

#endif /* IMX_PLAY_ENGINE_H */
