/**
*  Copyright (c) 2014-2015, Freescale Semiconductor Inc.,
*  All Rights Reserved.
*
*  The following programs are the sole property of Freescale Semiconductor Inc.,
*  and contain its proprietary and confidential information.
*
*/

#ifndef IMX_PLAY_ENGINE_H
#define IMX_PLAY_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gst/gst.h>

//#define PREPARE_WINDOW_MESSAGE
//#define ENABLE_OVERLAY_INTERNEL_WINDOW
//#define VIDEO_SINK_V4L2SINK
//#define GET_STREAM_INFO_FROM_TAGS
//#define PRINT_STREAM_INFO


#define METADATA_ITEM_SIZE_LARGE 256
#define METADATA_ITEM_SIZE_SMALL 64
#define METADATA_ALBUM_ART_MAX_SIZE  160000

#define MAX_AUDIO_TRACK_COUNT     8
#define MAX_VIDEO_TRACK_COUNT     8
#define MAX_SUBTITLE_TRACK_COUNT  8
#define MAX_SUBTITLE_BUF_COUNT    2

typedef enum
{
  PLAYENGINE_G2D,
  PLAYENGINE_G3D,
  PLAYENGINE_IPU,
  PLAYENGINE_PXP,
  PLAYENGINE_VPU
}FeatureType;

typedef enum
{
  PLAYENGINE_FAILURE = -1,
  PLAYENGINE_SUCCESS = 0,
  PLAYENGINE_ERROR_BAD_PARAM,
  PLAYENGINE_ERROR_NOT_SUPPORT,
  PLAYENGINE_ERROR_DEVICE_UNAVAILABLE,
  PLAYENGINE_ERROR_CANCELLED,
  PLAYENGINE_ERROR_TIMEOUT
}PlayEngineResult;

typedef enum
{
  EVENT_ID_EOS,
  EVENT_ID_ERROR,
  EVENT_ID_STATE_CHANGE,
  EVENT_ID_BUFFERING
}EventType;

typedef enum
{
  SUBTITLE_H_ALIGN_LEFT     = 0,
  SUBTITLE_H_ALIGN_CENTER   = 1,
  SUBTITLE_H_ALIGN_RIGHT    = 2,
  SUBTITLE_H_ALIGN_POSITION = 4
} SubtitleHAlign;

typedef enum
{
  SUBTITLE_V_ALIGN_BASELINE = 0,
  SUBTITLE_V_ALIGN_BOTTOM   = 1,
  SUBTITLE_V_ALIGN_TOP      = 2,
  SUBTITLE_V_ALIGN_POSITION = 3,
  SUBTITLE_V_ALIGN_CENTER   = 4
} SubtitleVAlign;

typedef struct {
  GstState old_st;
  GstState new_st;
  GstState pending_st;
}PlayEngineState;

typedef struct {
  gint32 offsetx;
  gint32 offsety;
  gint32 width;
  gint32 height;
}DisplayArea;

/* album art info */
typedef struct {
    guint8 *image;
    gsize size;     /* valid size */
    gint width;
    gint height;
}imx_album_art_info;
typedef imx_album_art_info imx_image_info;

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
    gchar albumartist[METADATA_ITEM_SIZE_LARGE];
    gchar composer[METADATA_ITEM_SIZE_LARGE];
    gchar comment[METADATA_ITEM_SIZE_LARGE];
    gchar description[METADATA_ITEM_SIZE_LARGE];
    gchar copyright[METADATA_ITEM_SIZE_LARGE];
    gchar keywords[METADATA_ITEM_SIZE_LARGE];
    gchar performer[METADATA_ITEM_SIZE_LARGE];
    gchar tool[METADATA_ITEM_SIZE_LARGE];
    gchar location_latitude[METADATA_ITEM_SIZE_LARGE];
    gchar location_longtitude[METADATA_ITEM_SIZE_LARGE];
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
    gint track_count;
    gint track_number;
    gint disc_number;
    gint rating;
    imx_audio_info audio_info[MAX_AUDIO_TRACK_COUNT];
    imx_video_info video_info[MAX_VIDEO_TRACK_COUNT];
    imx_subtitle_info subtitle_info[MAX_SUBTITLE_TRACK_COUNT];
    imx_album_art_info album_art_info;
} imx_metadata;


typedef void* PlayEngineHandle;
/* declaration of event callback handle*/
typedef void (*PlayEngineEventHandler) (void *context, EventType EventID, void *Eventpayload);

typedef struct _PlayEngine
{
  PlayEngineResult (*set_file)                (PlayEngineHandle handle, const gchar *uri);
  /* playengine state change and get interfaces */
  PlayEngineResult (*play)                    (PlayEngineHandle handle);
  PlayEngineResult (*stop)                    (PlayEngineHandle handle);
  PlayEngineResult (*pause)                   (PlayEngineHandle handle);
  PlayEngineResult (*get_state)               (PlayEngineHandle handle, GstState *state);
  PlayEngineResult (*set_state_change_timeout)(PlayEngineHandle handle, gint timeout_second);
  PlayEngineResult (*stop_wait_state_change)  (PlayEngineHandle handle);

  /* playengine operation interfaces */
  PlayEngineResult (*seek)                    (PlayEngineHandle handle, guint64 value, gboolean accurate);
  PlayEngineResult (*set_play_rate)           (PlayEngineHandle handle, gdouble playback_rate);
  PlayEngineResult (*get_play_rate)           (PlayEngineHandle handle, gdouble *playback_rate);
  PlayEngineResult (*set_rotate)              (PlayEngineHandle handle, gint rotation);
  PlayEngineResult (*get_rotate)              (PlayEngineHandle handle, gint *rotation);
  PlayEngineResult (*force_ratio)             (PlayEngineHandle handle, gboolean force);
  PlayEngineResult (*set_volume)              (PlayEngineHandle handle, gdouble volume);
  PlayEngineResult (*get_volume)              (PlayEngineHandle handle, gdouble *volume);
  PlayEngineResult (*set_mute)                (PlayEngineHandle handle, gboolean mute);
  PlayEngineResult (*get_mute)                (PlayEngineHandle handle, gboolean *mute_stat);
  PlayEngineResult (*get_seekable)            (PlayEngineHandle handle, gboolean *seekable);
  PlayEngineResult (*get_subtitle_num)        (PlayEngineHandle handle, gint *subtitle_num);
  PlayEngineResult (*get_audio_num)           (PlayEngineHandle handle, gint *audio_num);
  PlayEngineResult (*get_video_num)           (PlayEngineHandle handle, gint *video_num);
  PlayEngineResult (*get_cur_subtitle_no)     (PlayEngineHandle handle, gint *cur_subtitle);
  PlayEngineResult (*get_cur_audio_no)        (PlayEngineHandle handle, gint *cur_audio);
  PlayEngineResult (*get_cur_video_no)        (PlayEngineHandle handle, gint *cur_video);
  PlayEngineResult (*get_video_thumbnail)     (PlayEngineHandle handle, gint seconds, imx_image_info *thumbnail);
  PlayEngineResult (*select_subtitle)         (PlayEngineHandle handle, gint text_no);
  PlayEngineResult (*select_audio)            (PlayEngineHandle handle, gint audio_no);
  PlayEngineResult (*select_video)            (PlayEngineHandle handle, gint video_no);
  PlayEngineResult (*get_duration)            (PlayEngineHandle handle, gint64 *duration);
  PlayEngineResult (*get_position)            (PlayEngineHandle handle, gint64 *position);
  PlayEngineResult (*get_metadata)            (PlayEngineHandle handle, imx_metadata *meta);

  /* playengine resize interfaces */
  PlayEngineResult (*set_window)              (PlayEngineHandle handle, guintptr winhandle);
  PlayEngineResult (*set_render_rect)         (PlayEngineHandle handle, DisplayArea area);
  PlayEngineResult (*expose_video)            (PlayEngineHandle handle);
  PlayEngineResult (*set_fullscreen)          (PlayEngineHandle handle);
  PlayEngineResult (*get_display_area)        (PlayEngineHandle handle, DisplayArea *area);

  /* playengine custom setting interfaces */
  PlayEngineResult (*set_text_sink)           (PlayEngineHandle handle, const gchar *sink_name);
  PlayEngineResult (*set_video_sink)          (PlayEngineHandle handle, const gchar *sink_name);
  PlayEngineResult (*set_video_sink_element)  (PlayEngineHandle handle, GstElement *sink);
  PlayEngineResult (*set_audio_sink)          (PlayEngineHandle handle, const gchar *sink_name);
  PlayEngineResult (*set_visual)              (PlayEngineHandle handle, const gchar *visual_name);

  /* playengine subtitle setting interface */
  PlayEngineResult (*set_subtitle_uri)        (PlayEngineHandle handle, const gchar *filename);
  PlayEngineResult (*set_subtitle_font)       (PlayEngineHandle handle, gchar *font_desc);
  PlayEngineResult (*set_subtitle_color)      (PlayEngineHandle handle, guint argb);
  PlayEngineResult (*set_subtitle_outline_color) (PlayEngineHandle handle, guint argb);
  PlayEngineResult (*set_subtitle_shaded_background) (PlayEngineHandle handle, gboolean enable);
  PlayEngineResult (*set_subtitle_halignment) (PlayEngineHandle handle, SubtitleHAlign mode, gdouble xpos);
  PlayEngineResult (*set_subtitle_valignment) (PlayEngineHandle handle, SubtitleVAlign mode, gdouble ypos);
  PlayEngineResult (*get_subtitle_font)       (PlayEngineHandle handle, gchar *font_desc);
  PlayEngineResult (*get_subtitle_color)      (PlayEngineHandle handle, guint *argb);
  PlayEngineResult (*get_subtitle_outline_color) (PlayEngineHandle handle, guint *argb);
  PlayEngineResult (*get_subtitle_shaded_background) (PlayEngineHandle handle, gboolean *enable);
  PlayEngineResult (*get_subtitle_halignment) (PlayEngineHandle handle, SubtitleHAlign *mode, gdouble *xpos);
  PlayEngineResult (*get_subtitle_valignment) (PlayEngineHandle handle, SubtitleVAlign *mode, gdouble *ypos);
  PlayEngineResult (*get_subtitle_text)       (PlayEngineHandle handle, gchar *text, guint32 len,
                                                                  guint64 *duration, guint64* pts);

  /* playengine event handler regist interface, context is custom data pointer which will be pass to user define event callback*/
  PlayEngineResult (*reg_event_handler)       (PlayEngineHandle handle, void *context, 
                                                                  PlayEngineEventHandler handler);

  void *priv;
} PlayEngine;

PlayEngine * play_engine_create();
void play_engine_destroy(PlayEngine *engine);
/* unify the soc_id usage in gstremaer plugin and imx player */
gboolean play_engine_checkfeature (FeatureType type);

#ifdef __cplusplus
}
#endif
#endif /* IMX_PLAY_ENGINE_H */
