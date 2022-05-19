/*
 * Copyright 2014-2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2020 NXP
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your pconfigion) any later version.
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

/*
 * Description: gplay application base on gstplay API
 */


#include <termio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <gstimxcommon.h>

#include <gst/play/play.h>

#include "playlist.h"

#include <fcntl.h>
#include <linux/fb.h>
#define FB_DEIVCE "/dev/fb0"

/* version information of gplay */
#define FSL_GPLAY_VERSION "FSL_GPLAY2_01.00"
#define OS_NAME "_LINUX"
#define SEPARATOR " "
#define FSL_GPLAY_VERSION_STR \
    (FSL_GPLAY_VERSION OS_NAME SEPARATOR "build on" \
     SEPARATOR __DATE__ SEPARATOR __TIME__)


#define DEFAULT_TIME_OUT  20

typedef enum
{
  PLAY_REPEAT_NONE = 0,
  PLAY_REPEAT_PLAYLIST = 1,
  PLAY_REPEAT_CURRENT = 2,
} repeat_mode;

typedef struct
{

  gchar *video_sink_name;
  gchar *audio_sink_name;
  gchar *text_sink_name;
  gchar *suburi;

  PlayListHandle pl;
  const gchar *current;

  repeat_mode repeat;
  gint play_times;
  gint timeout;
  gint display_refresh_frq;
  gboolean no_auto_next;
  gboolean handle_buffering;
  guint64 connection_speed;
} gplay_pconfigions;

typedef struct
{
  GstPlay *play;
  GstPlayVideoRenderer *VideoRender;
  gplay_pconfigions *options;
  GstPlayState gstPlayState;

  gboolean seek_finished;
  gboolean error_found;
  gboolean eos_found;
  gint pending_audio_track;
  gint pending_sub_track;

  GMainLoop *loop;
} GstPlayData;


static volatile gboolean gexit_input_thread = FALSE;
static volatile gboolean gexit_display_thread = FALSE;
static volatile gboolean gDisable_display = FALSE;
static volatile gboolean gexit_main = FALSE;
GMainLoop *gloop = NULL;

/* return a new allocate string, need be freed after use */
static gchar *
filename2uri (const gchar * fn)
{
  char *tmp;
  if (strstr (fn, "://")) {
    tmp = g_strdup_printf ("%s", fn);
  } else if (fn[0] == '/') {
    tmp = g_strdup_printf ("file://%s", fn);
  } else {
    gchar *pwd = getenv ("PWD");
    tmp = g_strdup_printf ("file://%s/%s", pwd, fn);
  }

  return tmp;
}

void
alarm_signal (int signum)
{
}

void
set_alarm (guint seconds)
{
  struct sigaction act;
  /* Register alarm handler for alarm */
  act.sa_handler = &alarm_signal;
  act.sa_flags = 0;
  sigemptyset (&act.sa_mask);
  sigaction (SIGALRM, &act, NULL);
  /* Set timer for seconds */
  alarm (seconds);
}

void
reset_playdata (GstPlayData * sPlay)
{
  sPlay->gstPlayState = GST_PLAY_STATE_STOPPED;
  sPlay->seek_finished = FALSE;
  sPlay->error_found = FALSE;
  sPlay->eos_found = FALSE;
  sPlay->pending_audio_track = -1;
  sPlay->pending_sub_track = -1;
}

gint
playlist_next (GstPlay * play, gplay_pconfigions * options, GstPlayData * sPlay)
{
  gchar *uri = NULL;
  if (options->repeat > PLAY_REPEAT_CURRENT
      || options->repeat < PLAY_REPEAT_NONE) {
    g_print ("Unknown repeat mode\n");
    return RET_FAILURE;
  }

  switch (options->repeat) {
    case PLAY_REPEAT_NONE:
    {
      options->current = getNextItem (options->pl);
      if (options->current) {
        g_print ("Now Playing: %s\n", options->current);
        reset_playdata (sPlay);
        uri = filename2uri (options->current);
        gst_play_set_uri (play, uri);
        //gst_play_play (play);
        gst_play_play_sync (play, options->timeout);
        g_free (uri);
      } else {
        gboolean islast = FALSE;
        isLastItem (options->pl, &islast);
        if (islast) {
          g_print ("No more media file, exit gplay!\n");
        } else {
          g_print ("playlist unknown error\n");
        }
        return RET_FAILURE;
      }
    }
      break;

    case PLAY_REPEAT_PLAYLIST:
    {
      options->current = getNextItem (options->pl);
      if (options->current) {
        printf ("Now Playing: %s\n", options->current);
        reset_playdata (sPlay);
        uri = filename2uri (options->current);
        gst_play_set_uri (play, uri);
        //gst_play_play (play);
        gst_play_play_sync (play, options->timeout);
        g_free (uri);
      } else {
        gboolean islast = FALSE;
        isLastItem (options->pl, &islast);
        if (islast) {
          if (options->play_times > 0) {
            options->play_times--;
            if (options->play_times == 0) {
              g_print ("Repeat mode finished\n");
              return RET_FAILURE;
            }
          }
          options->current = getFirstItem (options->pl);
          if (options->current) {
            printf ("Now Playing: %s\n", options->current);
            reset_playdata (sPlay);
            uri = filename2uri (options->current);
            gst_play_set_uri (play, uri);
            //gst_play_play (play);
            gst_play_play_sync (play, options->timeout);
            g_free (uri);
          }
        } else {
          g_print ("playlist unknown error\n");
          return RET_FAILURE;
        }
      }
    }
      break;

    case PLAY_REPEAT_CURRENT:
    {
      //gst_play_play (play);
      reset_playdata (sPlay);
      gst_play_play_sync (play, options->timeout);
    }
      break;

    default:
      break;
  }
  return RET_SUCCESS;
}

gint
playlist_previous (GstPlay * play, gplay_pconfigions * options, GstPlayData * sPlay)
{
  gchar *uri = NULL;
  if (options->repeat > PLAY_REPEAT_CURRENT
      || options->repeat < PLAY_REPEAT_NONE) {
    g_print ("Unknown repeat mode\n");
    return RET_FAILURE;
  }

  switch (options->repeat) {
    case PLAY_REPEAT_NONE:
    {
      options->current = getPrevItem (options->pl);
      if (options->current) {
        g_print ("Now Playing: %s\n", options->current);
        reset_playdata (sPlay);
        uri = filename2uri (options->current);
        gst_play_set_uri (play, uri);
        //gst_play_play (play);
        gst_play_play_sync (play, options->timeout);
        g_free (uri);
      } else {
        gboolean isFirst = FALSE;
        isFirstItem (options->pl, &isFirst);
        if (isFirst) {
          g_print ("No more media file, exit gplay!\n");
        } else {
          g_print ("playlist unknown error\n");
        }
        return RET_FAILURE;
      }
    }
      break;

    case PLAY_REPEAT_PLAYLIST:
    {
      options->current = getPrevItem (options->pl);
      if (options->current) {
        printf ("Now Playing: %s\n", options->current);
        reset_playdata (sPlay);
        uri = filename2uri (options->current);
        gst_play_set_uri (play, uri);
        //gst_play_play (play);
        gst_play_play_sync (play, options->timeout);
        g_free (uri);
      } else {
        gboolean isFirst = FALSE;
        isFirstItem (options->pl, &isFirst);
        if (isFirst) {
          if (options->play_times > 0) {
            options->play_times--;
            if (options->play_times == 0) {
              g_print ("Repeat mode finished\n");
              return RET_FAILURE;
            }
          }

          options->current = getLastItem (options->pl);
          if (options->current) {
            printf ("Now Playing: %s\n", options->current);
            reset_playdata (sPlay);
            uri = filename2uri (options->current);
            gst_play_set_uri (play, uri);
            //gst_play_play (play);
            gst_play_play_sync (play, options->timeout);
            g_free (uri);
          }
        } else {
          g_print ("playlist unknown error\n");
          return RET_FAILURE;
        }
      }
    }
      break;

    case PLAY_REPEAT_CURRENT:
    {
      //gst_play_play (play);
      reset_playdata (sPlay);
      gst_play_play_sync (play, options->timeout);
    }
      break;

    default:
      break;
  }
  return RET_SUCCESS;
}


void
print_help ()
{
  g_print ("options :\n");
  g_print
      ("    --quiet           Disable playback status display updating\n\n");
  g_print ("    --repeat          Set gplay to playlist repeat mode.\n");
  g_print
      ("                      Use below option to specify your repeat times\n");
  g_print ("                      --repeat=PlayTimes\n\n");
  g_print
      ("    --video-sink      Specify the video sink instead of default sink\n");
  g_print
      ("                      Use below option to input your video sink name\n");
  g_print ("                      --video-sink=video_sink_name\n\n");
  g_print
      ("    --audio-sink      Specify the audio sink instead of default sink\n");
  g_print
      ("                      Use below option to input your audio sink name\n");
  g_print ("                      --audio-sink=audio_sink_name\n\n");
  g_print
      ("    --text-sink       Specify the text sink instead of default sink\n");
  g_print
      ("                      Use below option to input your text sink name\n");
  g_print ("                      --text-sink=text_sink_name\n\n");
  g_print ("    --suburi          Set subtitle path\n");
  g_print ("                      Use below option to input your pathname\n");
  g_print ("                      --suburi=pathname\n\n");
  g_print
      ("    --connection-speed Specify the default adaptive playback connection speed in bps\n");
  g_print ("                      Use below option to specify your connection speed\n");
  g_print ("                      --connection-speed=BPS\n\n");
}

gint
parse_pconfigions (gplay_pconfigions * pconfig, gint32 argc, gchar * argv[])
{
  gint32 i;


  pconfig->pl = createPlayList ();
  if ((void *) pconfig->pl == NULL) {
    printf ("Can not create Playlist!!\n");
    return RET_FAILURE;
  }

  pconfig->play_times = -1;

  for (i = 1; i < argc; i++) {
    if (strlen (argv[i])) {
      if (argv[i][0] == '-') {
        if ((strcmp (argv[i], "-h") == 0) || (strcmp (argv[i], "--help") == 0)) {
          g_print ("Usage: gplay-1.0 [OPTIONS] PATH [PATH...]\n");
          print_help ();
          goto err;
        }

        if (strcmp (argv[i], "--quiet") == 0) {
          pconfig->display_refresh_frq = 0;
          continue;
        }

        if (strncmp (argv[i], "--repeat", 8) == 0) {
          if (argv[i][8] == '=') {
            pconfig->play_times = atoi (&(argv[i][9]));
          }
          pconfig->repeat = PLAY_REPEAT_PLAYLIST;
          continue;
        }

        if ((strncmp (argv[i], "--video-sink", 12) == 0)) {
          if (argv[i][12] == '=') {
            pconfig->video_sink_name = &(argv[i][13]);
          }
          continue;
        }

        if ((strncmp (argv[i], "--audio-sink", 12) == 0)) {
          if (argv[i][12] == '=') {
            pconfig->audio_sink_name = &(argv[i][13]);
          }
          continue;
        }

        if ((strncmp (argv[i], "--text-sink", 11) == 0)) {
          if (argv[i][11] == '=') {
            pconfig->text_sink_name = &(argv[i][12]);
          }
          continue;
        }

        if ((strncmp (argv[i], "--suburi", 8) == 0)) {
          if (argv[i][8] == '=') {
            pconfig->suburi = &(argv[i][9]);
          }
          continue;
        }

        if (strncmp (argv[i], "--timeout", 9) == 0) {
          if (argv[i][9] == '=') {
            pconfig->timeout = atoi (&(argv[i][10]));
          }
          continue;
        }

        if ((strncmp (argv[i], "--info-interval", 14) == 0)) {
          if (argv[i][14] == '=') {
            pconfig->display_refresh_frq = atoi (&(argv[i][15]));
          }
          continue;
        }

        if ((strcmp (argv[i], "--noautonext") == 0)) {
          pconfig->no_auto_next = TRUE;
          continue;
        }

        if ((strcmp (argv[i], "--handle-buffering") == 0)) {
          pconfig->handle_buffering = TRUE;
          continue;
        }

        if (strncmp (argv[i], "--connection-speed", 18) == 0) {
          if (argv[i][18] == '=') {
            gint64 connection_speed = atoi(&(argv[i][19]));
            if (connection_speed <= 0) {
              g_print ("Invalid connection speed\n");
              pconfig->connection_speed = 0;
            } else {
              pconfig->connection_speed = (connection_speed - 1) / 1000 + 1;
            }
          }
          continue;
        }

        continue;
      } else {

        if (addItemAtTail (pconfig->pl, argv[i]) != RET_SUCCESS)
          goto err;
        continue;
      }
    }
  }

  pconfig->current = getFirstItem (pconfig->pl);
  if (pconfig->current == NULL) {
    g_print ("NO File specified!!\n");
    goto err;
  }

  return RET_SUCCESS;

err:
  if (pconfig->pl) {
    destroyPlayList (pconfig->pl);
    pconfig->pl = NULL;
  }
  return RET_FAILURE;
}

static void
print_one_tag (const GstTagList * list, const gchar * tag, gpointer user_data)
{
  gint i, num;
  gchar *str;
  num = gst_tag_list_get_tag_size (list, tag);
  for (i = 0; i < num; ++i) {
    const GValue *val;

    val = gst_tag_list_get_value_index (list, tag, i);
    if (G_VALUE_HOLDS_STRING (val)) {
      printf ("    %s : %s \n", tag, g_value_get_string (val));
    } else if (G_VALUE_HOLDS_UINT (val)) {
      printf ("    %s : %u \n", tag, g_value_get_uint (val));
    } else if (G_VALUE_HOLDS_DOUBLE (val)) {
      printf ("    %s : %g \n", tag, g_value_get_double (val));
    } else if (G_VALUE_HOLDS_BOOLEAN (val)) {
      printf ("    %s : %s \n", tag,
          g_value_get_boolean (val) ? "true" : "false");
    } else if (GST_VALUE_HOLDS_DATE_TIME (val)) {
      GstDateTime *dt = g_value_get_boxed (val);
      gchar *dt_str = gst_date_time_to_iso8601_string (dt);

      printf ("    %s : %s \n", tag, dt_str);
      g_free (dt_str);
    } else if (G_VALUE_TYPE (val) == GST_TYPE_SAMPLE) {
      GstSample *sample = gst_value_get_sample (val);
      GstBuffer *img = gst_sample_get_buffer (sample);
      GstCaps *caps = gst_sample_get_caps (sample);

      if (img) {
        if (caps) {
          gchar *caps_str;

          caps_str = gst_caps_to_string (caps);
          str = g_strdup_printf ("buffer of %" G_GSIZE_FORMAT " bytes, "
              "type: %s", gst_buffer_get_size (img), caps_str);
          g_free (caps_str);
        } else {
          str = g_strdup_printf ("buffer of %" G_GSIZE_FORMAT " bytes",
              gst_buffer_get_size (img));
        }
      } else {
        str = g_strdup ("NULL buffer");
      }
      g_print ("    %s: %s\n",  gst_tag_get_nick (tag), str);
      g_free (str);
    } else {
      str = gst_value_serialize(val);
      g_print ("    %s: %s\n",  gst_tag_get_nick (tag), str);
      g_free (str);
    }
  }
}

static void
print_video_info (GstPlayVideoInfo * info)
{
  gint fps_n, fps_d;
  guint par_n, par_d;

  if (info == NULL)
    return;

  g_print ("  width : %d\n", gst_play_video_info_get_width (info));
  g_print ("  height : %d\n", gst_play_video_info_get_height (info));
  g_print ("  max_bitrate : %d\n",
      gst_play_video_info_get_max_bitrate (info));
  g_print ("  bitrate : %d\n", gst_play_video_info_get_bitrate (info));
  gst_play_video_info_get_framerate (info, &fps_n, &fps_d);
  g_print ("  framerate : %.2f\n", (gdouble) fps_n / fps_d);
  gst_play_video_info_get_pixel_aspect_ratio (info, &par_n, &par_d);
  g_print ("  pixel-aspect-ratio  %u:%u\n", par_n, par_d);
}

static void
print_audio_info (GstPlayAudioInfo * info)
{
  if (info == NULL)
    return;

  g_print ("  sample rate : %d\n",
      gst_play_audio_info_get_sample_rate (info));
  g_print ("  channels : %d\n", gst_play_audio_info_get_channels (info));
  g_print ("  max_bitrate : %d\n",
      gst_play_audio_info_get_max_bitrate (info));
  g_print ("  bitrate : %d\n", gst_play_audio_info_get_bitrate (info));
  g_print ("  language : %s\n", gst_play_audio_info_get_language (info));
}

static void
print_subtitle_info (GstPlaySubtitleInfo * info)
{
  if (info == NULL)
    return;

  g_print ("  language : %s\n", gst_play_subtitle_info_get_language (info));
}

static void
print_all_stream_info (GstPlayMediaInfo * media_info)
{
  guint count = 0;
  GList *list, *l;

  g_print ("URI : %s\n", gst_play_media_info_get_uri (media_info));
  g_print ("Duration: %" GST_TIME_FORMAT "\n",
      GST_TIME_ARGS (gst_play_media_info_get_duration (media_info)));
  g_print ("Global taglist:\n");
  if (gst_play_media_info_get_tags (media_info))
    gst_tag_list_foreach (gst_play_media_info_get_tags (media_info),
        print_one_tag, NULL);
  else
    g_print ("  (nil) \n");

  list = gst_play_media_info_get_stream_list (media_info);
  if (!list)
    return;

  g_print ("All Stream information\n");
  for (l = list; l != NULL; l = l->next) {
    GstTagList *tags = NULL;
    GstPlayStreamInfo *stream = (GstPlayStreamInfo *) l->data;

    g_print (" Stream # %u \n", count++);
    g_print ("  type : %s_%u\n",
        gst_play_stream_info_get_stream_type (stream),
        gst_play_stream_info_get_index (stream));
    tags = gst_play_stream_info_get_tags (stream);
    g_print ("  taglist : \n");
    if (tags) {
      gst_tag_list_foreach (tags, print_one_tag, NULL);
    }

    if (GST_IS_PLAY_VIDEO_INFO (stream))
      print_video_info ((GstPlayVideoInfo *) stream);
    else if (GST_IS_PLAY_AUDIO_INFO (stream))
      print_audio_info ((GstPlayAudioInfo *) stream);
    else
      print_subtitle_info ((GstPlaySubtitleInfo *) stream);
  }
}

static void
clear_pending_trackselect (GstPlayData *sPlay)
{
  sPlay->pending_audio_track = -1;
  sPlay->pending_sub_track = -1;
}


void
display_thread_fun (gpointer data)
{
  GstPlayData *sPlay = (GstPlayData *) data;
  gchar str_repeated_mode[3][20] =
      { "(No Repeated)", "(List Repeated)", "(Current Repeated)" };

  if (!sPlay || !sPlay->play || !sPlay->options) {
    g_print ("Invalid GstPlayData pointer\n");
    return;
  }
  GstPlay *play = sPlay->play;
  gplay_pconfigions *options = sPlay->options;

  while (1) {
    if (TRUE == gexit_display_thread) {
      if (g_main_loop_is_running (sPlay->loop) == TRUE) {
        g_main_loop_quit (sPlay->loop);
      }
      gexit_main = TRUE;
      g_print ("Exit display thread\n");
      return;
    }
    if (FALSE == gDisable_display) {
      gchar str_play_state_rate[16];
      gchar str_volume[16];
      gchar *prepeated_mode = &(str_repeated_mode[options->repeat][0]);
      guint64 hour, minute, second;
      guint64 hour_d, minute_d, second_d;
      guint64 duration = 0;
      guint64 elapsed = 0;
      GstPlayState gstplay_state;
      gdouble playback_rate = 0.0;
      gboolean bmute = 0;
      gdouble volume = 0.0;


      duration = gst_play_get_duration (play);
      elapsed = gst_play_get_position (play);
      bmute = gst_play_get_mute (play);
      volume = gst_play_get_volume (play);
      playback_rate = gst_play_get_rate (play);
      gstplay_state = sPlay->gstPlayState;

      /* when play rtsp streaming, gplay cannot get duration, need add sleep
         here to reduce while loop cpu loading. Avoid performance issue on
         single core soc, eg. 6sll*/
      if (duration == GST_CLOCK_TIME_NONE || elapsed == GST_CLOCK_TIME_NONE) {
        usleep (100000);
        continue;
      }

      hour = (elapsed / (gint64) 3600000000000);
      minute = (elapsed / (guint64) 60000000000) - (hour * 60);
      second = (elapsed / 1000000000) - (hour * 3600) - (minute * 60);
      hour_d = (duration / (guint64) 3600000000000);
      minute_d = (duration / (guint64) 60000000000) - (hour_d * 60);
      second_d = (duration / 1000000000) - (hour_d * 3600) - (minute_d * 60);

      switch (gstplay_state) {
        case GST_PLAY_STATE_PLAYING:
        {
          if (playback_rate > 1.0 && playback_rate <= 8.0) {
            sprintf (str_play_state_rate, "%s(%1.1fX)", "FF ", playback_rate);
          }
          if (playback_rate > 0.0 && playback_rate < 1.0) {
            sprintf (str_play_state_rate, "%s(%1.1fX)", "SF ", playback_rate);
          }
          if (playback_rate >= -8.0 && playback_rate <= -0.0) {
            sprintf (str_play_state_rate, "%s(%1.1fX)", "FB ", playback_rate);
          }
          if (playback_rate == 1.0) {
            sprintf (str_play_state_rate, "%s", "Playing ");
          }
        }
          break;

        case GST_PLAY_STATE_PAUSED:
          sprintf (str_play_state_rate, "%s", "Pause ");
          break;

        case GST_PLAY_STATE_BUFFERING:
          sprintf (str_play_state_rate, "%s", "Buffering ");

        case GST_PLAY_STATE_STOPPED:
          sprintf (str_play_state_rate, "%s", "Stop ");
          break;

        default:
          sprintf (str_play_state_rate, "%s", "Unknown ");
          break;
      }

      if (bmute) {
        sprintf (str_volume, "%s", "MUTE ");
      } else {
        sprintf (str_volume, "Vol=%.1lf", volume);
      }

      g_print ("\r[%s%s][%s][%02d:%02d:%02d/%02d:%02d:%02d]",
          str_play_state_rate, prepeated_mode, str_volume,
          (gint32) hour, (gint32) minute, (gint32) second,
          (gint32) hour_d, (gint32) minute_d, (gint32) second_d);

      fflush (stdout);
    }
    sleep (1);
  }
}


void
print_menu ()
{
  g_print ("\n%s\n", FSL_GPLAY_VERSION_STR);
  g_print ("\t[h]display the operation Help\n");
  g_print ("\t[p]Play\n");
  g_print ("\t[s]Stop\n");
  g_print ("\t[e]Seek\n");
  g_print ("\t[a]Pause when playing, play when paused\n");
  g_print ("\t[v]Volume\n");
  g_print ("\t[m]Switch to mute or not\n");
  g_print ("\t[>]Play next file\n");
  g_print ("\t[<]Play previous file\n");
  g_print ("\t[r]Switch to repeated mode or not\n");
  g_print ("\t[u]Select the video track\n");
  g_print ("\t[d]Select the audio track\n");
  g_print ("\t[b]Select the subtitle track\n");
  g_print ("\t[n]Select adaptive playback track\n");
  g_print ("\t[f]Set full screen\n");
  g_print ("\t[z]resize the width and height\n");
  g_print ("\t[t]Rotate\n");
  g_print ("\t[c]Setting play rate\n");
  g_print ("\t[i]Display the metadata\n");
  g_print ("\t[x]eXit\n");
}

static void
reset_signal (int sig, void *handler)
{
  struct sigaction act;
  act.sa_handler = handler;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;
  sigaction (sig, &act, NULL);
}

static void
signal_handler (int sig)
{
  switch (sig) {
    case SIGINT:
      g_print (" Aborted by signal[%d] Interrupt...\n", sig);
      /* restore to default handler for SIGINT */
      reset_signal (SIGINT, SIG_DFL);
      gexit_input_thread = TRUE;
      gexit_display_thread = TRUE;
      if (g_main_loop_is_running (gloop) == TRUE) {
        g_main_loop_quit (gloop);
      }
      gexit_main = TRUE;
      break;

    case SIGTTIN:
      /* Nothing need do */
      break;

    default:
      break;
  }
}

gboolean
gplay_checkfeature (CHIP_FEATURE type)
{
  gboolean ret = FALSE;

  switch (type) {
    case G2D:
      ret = HAS_G2D ();
      break;
    case G3D:
      ret = HAS_G3D ();
      break;
    case PXP:
      ret = HAS_PXP ();
      break;
    case IPU:
      ret = HAS_IPU ();
      break;
    case VPU:
      ret = HAS_VPU ();
      break;
    case DPU:
      ret = HAS_DPU ();
      break;
    case DCSS:
      ret = HAS_DCSS();
      break;
    default:
      ret = FALSE;
      break;
  }

  return ret;
}


static gboolean
gplay_get_fullscreen_size (gint32 * pfullscreen_width,
    gint32 * pfullscreen_height)
{
  struct fb_var_screeninfo scrinfo;
  gint32 fb;

  if ((fb = open (FB_DEIVCE, O_RDWR, 0)) < 0) {
    g_warning ("Unable to open %s %d\n", FB_DEIVCE, fb);
    fb = 0;
    return FALSE;
  }

  if (ioctl (fb, FBIOGET_VSCREENINFO, &scrinfo) < 0) {
    g_warning ("Get var of fb0 failed\n");
    close (fb);
    return FALSE;
  }
  *pfullscreen_width = scrinfo.xres;
  *pfullscreen_height = scrinfo.yres;
  close (fb);

  return TRUE;
}

static void gplay_set_subtitle_track_enabled (GstPlay * play, gboolean enabled) 
{
  if (gplay_checkfeature (VPU)
      && (gplay_checkfeature (DCSS) || gplay_checkfeature (DPU))) {
    gst_play_set_subtitle_track_enabled (play, FALSE);
  } else {
    gst_play_set_subtitle_track_enabled (play, enabled);
  }
}

static void
error_cb (GstPlay * play, GError * err, GstPlayData * sPlay)
{
  gplay_pconfigions *options = sPlay->options;
  g_printerr ("ERROR %s for %s\n", err->message, options->current);

  sPlay->error_found = TRUE;
  /* if current repeat is enabled, then disable it else will keep looping forever */
  if (options->repeat == PLAY_REPEAT_CURRENT) {
    options->repeat = PLAY_REPEAT_NONE;
    gexit_input_thread = TRUE;
    gexit_display_thread = TRUE;
  }

  gplay_set_subtitle_track_enabled (play, TRUE);
  clear_pending_trackselect (sPlay);

  /* try next item in list then */
  if (!options->no_auto_next) {
    if (playlist_next (play, options, sPlay) != RET_SUCCESS) {
      gexit_input_thread = TRUE;
      gexit_display_thread = TRUE;
    } else {
      g_print ("play next item successfully\n");
    }
  } else {
    g_print ("no auto next is on\n");
    gexit_input_thread = TRUE;
    gexit_display_thread = TRUE;
  }
  if (gexit_input_thread == TRUE ) {
    if (g_main_loop_is_running (gloop) == TRUE) {
      g_main_loop_quit (gloop);
    }
  }
}

static void
eos_cb (GstPlay * play, GstPlayData * sPlay)
{
  gplay_pconfigions *options;

  options = sPlay->options;
  /* for auto test script */
  g_print ("EOS Found\n");
  sPlay->eos_found = TRUE;
  gst_play_set_rate (play, 1.0);
  gplay_set_subtitle_track_enabled (play, TRUE);
  //gst_play_stop (play);
  gst_play_stop_sync (play, options->timeout);
  clear_pending_trackselect (sPlay);

  if (!options->no_auto_next) {
    if (playlist_next (play, options, sPlay) != RET_SUCCESS) {
      gexit_input_thread = TRUE;
      gexit_display_thread = TRUE;
    } else {
      g_print ("play next item successfully\n");
    }
  } else {
    g_print ("no auto next is on\n");
    gexit_input_thread = TRUE;
    gexit_display_thread = TRUE;
  }
  if (gexit_input_thread == TRUE ) {
    if (g_main_loop_is_running (gloop) == TRUE) {
      g_main_loop_quit (gloop);
    }
  }
}

static void
state_changed_cb (GstPlay * play, GstPlayState state, GstPlayData * sPlay)
{
  sPlay->gstPlayState = state;
  g_print ("State changed: %s\n", gst_play_state_get_name (state));
}

static void
seek_done_cb (GstPlay * play, guint64 position, GstPlayData * sPlay)
{
  //g_print("======= seek done signal got ==========\n");
  sPlay->seek_finished = TRUE;

}

void
wait_for_seek_done (GstPlayData * sPlay, gint time_out)
{
  gint wait_cnt = 0;

  while (time_out < 0 || wait_cnt < time_out * 20) {
    if (sPlay->seek_finished == TRUE) {
      sPlay->seek_finished = FALSE;
      break;
    } else if (sPlay->error_found == TRUE) {
      sPlay->error_found = FALSE;
      return;
    } else if (sPlay->eos_found == TRUE) {
      sPlay->eos_found = FALSE;
      return;
    }else {
      wait_cnt++;
      usleep (50000);
    }
  }
  if (wait_cnt >= time_out * 20) {
    g_print ("Wait seek done time out !!!\n");
  }
}

void
input_thread_fun (gpointer data)
{
  gchar sCommand[256];
  gchar *uri = NULL;
  GstPlayData *sPlay = (GstPlayData *) data;
  GstPlay *play = sPlay->play;
  GstPlayVideoRenderer *VideoRender = sPlay->VideoRender;
  gplay_pconfigions *options = sPlay->options;

  while (gexit_input_thread == FALSE) {
    sCommand[0] = ' ';
    errno = 0;
    if (scanf ("%256s", sCommand) != 1) {
      // need to seek in case read to EOF
      fseek (stdin, 0, SEEK_CUR);
      usleep (100000);
      continue;
    }

    switch (sCommand[0]) {
      case 'h':                // display the operation Help.
        print_menu ();
        break;

      case 'p':                // Play.
      {
        if (sPlay->gstPlayState == GST_PLAY_STATE_STOPPED) {
          uri = filename2uri (options->current);
          gst_play_set_uri (play, uri);
          //gst_play_play (play);
          gst_play_play_sync (play, options->timeout);
          g_free (uri);
        }
      }
        break;

      case 's':                // Stop
      {
        //gst_play_stop (play);
        gplay_set_subtitle_track_enabled (play, TRUE);
        gst_play_stop_sync (play, options->timeout);
        clear_pending_trackselect (sPlay);
      }
        break;

      case 'a':                // pAuse
      {
        if (sPlay->gstPlayState == GST_PLAY_STATE_PLAYING) {
          //gst_play_pause (sPlay->play);
          gst_play_pause_sync (sPlay->play, options->timeout);
        } else if (sPlay->gstPlayState == GST_PLAY_STATE_PAUSED) {
          //gst_play_play (sPlay->play);
          gst_play_play_sync (play, options->timeout);
        }
      }
        break;

      case 'e':                // sEek
      {
        guint32 seek_point_sec = 0;
        guint64 duration_ns = 0;
        guint64 duration_sec = 0;
        guint32 seek_portion = 0;
        gboolean accurate_seek = FALSE; //
        gint input_mode = -1;
        gboolean seekable = FALSE;

        GstPlayMediaInfo *media_info = gst_play_get_media_info (play);

        gDisable_display = TRUE;
        if (media_info) {
          seekable = gst_play_media_info_is_seekable (media_info);
          g_object_unref (media_info);
        } else {
          gDisable_display = FALSE;
          break;
        }

        if (!seekable) {
          g_print ("file is not seekable!\n");
          gDisable_display = FALSE;
          break;
        }
        duration_ns = gst_play_get_duration (play);
        duration_sec = duration_ns / 1000000000;

        g_print ("Select seek mode[Fast seek:0,Accurate seek:1]:");
        if (scanf ("%d", &input_mode) != 1) {
          gDisable_display = FALSE;
          break;
        }

        if (input_mode != 0 && input_mode != 1) {
          g_print ("Invalid seek mode!\n");
          gDisable_display = FALSE;
          break;
        } else {
          accurate_seek = (gboolean) input_mode;
        }

        g_print ("%s seek to percentage[0:100] or second [t?]:",
            accurate_seek ? "Accurate" : "Normal");
        if (scanf ("%256s", sCommand) != 1) {
          gDisable_display = FALSE;
          break;
        }
        if (sCommand[0] == 't') {
          seek_point_sec = atoi (&sCommand[1]);
        } else {
          seek_portion = atoi (sCommand);

          if (seek_portion > 100) {
            g_print ("Invalid seek point!\n");
            gDisable_display = FALSE;
            break;
          }
          seek_point_sec = (guint64) (seek_portion * duration_sec / 100);
        }
        gDisable_display = FALSE;
        gst_play_config_set_seek_accurate (play, accurate_seek);
        gst_play_seek (play, seek_point_sec * GST_SECOND);
        wait_for_seek_done (sPlay, options->timeout);
      }
        break;

      case 'v':                //Volume

      {
        gdouble volume;
        g_print ("Set volume[0-1.0]:");
        gDisable_display = TRUE;
        if (scanf ("%lf", &volume) != 1) {
          gDisable_display = FALSE;
          break;
        }
        gDisable_display = FALSE;
        gst_play_set_volume (play, volume);
      }
        break;

      case 'm':
      {
        gboolean mute_status = gst_play_get_mute (play);
        gst_play_set_mute (play, mute_status);
      }
        break;

      case 'n':
      {
        guint64 connection_speed = 0;
        g_print ("Set adaptive playback connection speed in bps:");
        gDisable_display = TRUE;
        if (scanf ("%lu", &connection_speed) != 1) {
          gDisable_display = FALSE;
          break;
        }
        gDisable_display = FALSE;
        /* + 1: Get the value which greater than the current value and */
        /* + 1: closet to multiples of 1000 when multiplied by 1000 */
        /* - 1: Avoid adding 1 when the value equal to multiples of 1000 */
        connection_speed = (connection_speed - 1) / 1000 + 1;
        if (connection_speed <= 0) {
          g_print ("Invalid connection speed\n");
        } else {
          gst_play_set_connection_speed (play, connection_speed);
          g_print ("connection speed update done\n");
        }
      }
        break;

      case 'c':                // playing direction and speed Control.
      {
        gdouble playback_rate;
        gboolean seekable = FALSE;
        GstPlayMediaInfo *media_info = gst_play_get_media_info (play);

        gDisable_display = TRUE;
        if (media_info) {
          seekable = gst_play_media_info_is_seekable (media_info);
          g_object_unref (media_info);
        } else {
          gDisable_display = FALSE;
          break;
        }

        if (!seekable) {
          g_print ("file is not seekable!, rate can not be set! \n");
          gDisable_display = FALSE;
          break;
        }
        g_print ("Set playing speed[-8,-4,-2,0.125,0.25,0.5,1,2,4,8]:");
        gDisable_display = TRUE;
        if (scanf ("%lf", &playback_rate) != 1) {
          gDisable_display = FALSE;
          break;
        }
        gDisable_display = FALSE;
        gst_play_set_rate (play, playback_rate);
        wait_for_seek_done (sPlay, options->timeout);
        if (playback_rate > 2.0 || playback_rate < 0){
          gplay_set_subtitle_track_enabled (play, FALSE);
        } else {
          gplay_set_subtitle_track_enabled (play, TRUE);
        }
        if (playback_rate > 0 && playback_rate <= 2.0){
          /* now do pending track select */
          if (sPlay->pending_audio_track >= 0) {
            gst_play_set_audio_track (play, sPlay->pending_audio_track);
            sPlay->pending_audio_track = -1;
          }
          if(sPlay->pending_sub_track >= 0) {
            gst_play_set_subtitle_track (play, sPlay->pending_sub_track);
            sPlay->pending_sub_track = -1;
          }
        }
      }
        break;

      case 'i':
      {
        GstPlayMediaInfo *media_info = gst_play_get_media_info (play);
        print_all_stream_info (media_info);
        g_object_unref (media_info);
      }
        break;

      case 'x':                // eXit
      {
        g_print ("Ready to exit this app!\n");
        /* just ignore this case if eos has been found before entering 'x' */
        if (sPlay->eos_found == FALSE) {
          //gst_play_stop (play);
          gst_play_stop_sync (play, options->timeout);
          clear_pending_trackselect (sPlay);
          gexit_input_thread = TRUE;
          gexit_display_thread = TRUE;
          /* add protection before quit main loop to avoid critical info
          'g_atomic_int_get (&loop->ref_count) > 0' if clip has reached eos*/
          if (sPlay->eos_found == FALSE) {
            if (sPlay->loop) {
              if (g_main_loop_is_running (sPlay->loop) == TRUE) {
                g_main_loop_quit (sPlay->loop);
              }
            }
          }
        }
      }
        break;

      case 'r':                // Switch to repeated mode or not
      {
        gint setrepeat;
        g_print
            ("input repeated mode[0 for no repeated,1 for play list repeated, 2 for current file repeated]:");
        gDisable_display = TRUE;
        if (scanf ("%d", &setrepeat) != 1) {
          gDisable_display = FALSE;
          break;
        }
        if (setrepeat < 0 || setrepeat > 2) {
          g_print ("Invalid repeated mode!\n");
        } else {
          options->repeat = setrepeat;
        }
        gDisable_display = FALSE;
      }
        break;

      case '>':                // Play next file
        g_print ("next\n");
        gst_play_set_rate (play, 1.0);
        gplay_set_subtitle_track_enabled (play, TRUE);
        //gst_play_stop (play);
        gst_play_stop_sync (play, options->timeout);
        clear_pending_trackselect (sPlay);
        if (playlist_next (play, options, sPlay) != RET_SUCCESS) {
          gexit_input_thread = TRUE;
          gexit_display_thread = TRUE;
          set_alarm (1);
        }
        break;

      case '<':                // Play previous file
        g_print ("previous\n");
        gst_play_set_rate (play, 1.0);
        gplay_set_subtitle_track_enabled (play, TRUE);
        //gst_play_stop (play);
        gst_play_stop_sync (play, options->timeout);
        clear_pending_trackselect (sPlay);
        if (playlist_previous (play, options, sPlay) != RET_SUCCESS) {
          gexit_input_thread = TRUE;
          gexit_display_thread = TRUE;
          set_alarm (1);
        }
        break;

      case 't':                // Rotate 90 degree every time
      {
        gint rotate_value;
        g_print ("Set rotation between 0, 90, 180, 270: ");
        gDisable_display = TRUE;
        if (scanf ("%d", &rotate_value) != 1) {
          gDisable_display = FALSE;
          break;
        }
        gDisable_display = FALSE;
        if (rotate_value != 0 && rotate_value != 90 && rotate_value != 180
            && rotate_value != 270) {
          g_print
              ("Invalid rotation value=%d, should input [0, 90, 180, 270]\n",
              rotate_value);
          break;
        }
        gst_play_set_rotate (play, rotate_value);
      }
        break;

      case 'z':                // resize the width and height
      {
        guint x = 0;
        guint y = 0;
        guint width = 0;
        guint height = 0;
        GstPlayVideoOverlayVideoRenderer *VideoOverlayVideoRenderer =
                              GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER (VideoRender);
        g_print ("Input [x y width height]:");
        gDisable_display = TRUE;
        if (scanf ("%d %d %d %d", &x, &y, &width, &height) != 4) {
          gDisable_display = FALSE;
          break;
        }
        gDisable_display = FALSE;
        gst_play_video_overlay_video_renderer_set_render_rectangle
            (VideoOverlayVideoRenderer, x, y, width, height);
        gst_play_video_overlay_video_renderer_expose
            (VideoOverlayVideoRenderer);

      }
        break;

      case 'f':
      {
        guint width = 0;
        guint height = 0;
        GstPlayVideoOverlayVideoRenderer *VideoOverlayVideoRenderer =
                              GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER (VideoRender);
        if (!gplay_get_fullscreen_size (&width, &height));
        gst_play_video_overlay_video_renderer_set_render_rectangle
            (VideoOverlayVideoRenderer, 0, 0, width, height);
        gst_play_video_overlay_video_renderer_expose
            (VideoOverlayVideoRenderer);
      }
        break;

      case 'u':                // Select the video track
      {
        gint32 video_track_no = 0;
        gint32 total_video_no = 0;
        GstPlayMediaInfo *media_info = gst_play_get_media_info (play);
        total_video_no =
            gst_play_media_info_get_number_of_video_streams (media_info);
        g_object_unref (media_info);
        g_print ("input video track number[0,%d]:", total_video_no - 1);
        gDisable_display = TRUE;
        if (scanf ("%d", &video_track_no) != 1) {
          gDisable_display = FALSE;
          break;
        }
        if (video_track_no < 0 || video_track_no > total_video_no - 1) {
          g_print ("Invalid video track!\n");
        } else {
          gst_play_set_video_track (play, video_track_no);
        }
        gDisable_display = FALSE;
      }
        break;

      case 'd':                // Select the audio track
      {
        gint32 audio_track_no = 0;
        gint32 total_audio_no = 0;
        gdouble rate = gst_play_get_rate (play);
        GstPlayMediaInfo *media_info = gst_play_get_media_info (play);
        total_audio_no =
            gst_play_media_info_get_number_of_audio_streams (media_info);
        g_object_unref (media_info);
        g_print ("input audio track number[0,%d]:", total_audio_no - 1);
        gDisable_display = TRUE;
        if (scanf ("%d", &audio_track_no) != 1) {
          gDisable_display = FALSE;
          break;
        }
        if (audio_track_no < 0 || audio_track_no > total_audio_no - 1) {
          g_print ("Invalid audio track!\n");
        } else {
          if (rate > 2.0 || rate < 0) {
            /* just record the audio track num, will do track select when play in normal mode*/
            sPlay->pending_audio_track = audio_track_no;
            gDisable_display = FALSE;
            break;
          }
          gst_play_set_audio_track (play, audio_track_no);
        }
        gDisable_display = FALSE;
      }
        break;

      case 'b':                // Select the subtitle
      {
        gint32 subtitle_no = 0;
        gint32 total_subtitle_no = 0;
        gdouble rate = gst_play_get_rate (play);
        GstPlayMediaInfo *media_info = gst_play_get_media_info (play);
        total_subtitle_no =
            gst_play_media_info_get_number_of_subtitle_streams (media_info);
        g_object_unref (media_info);
        g_print ("input subtitle track number[0,%d]:", total_subtitle_no - 1);
        gDisable_display = TRUE;
        if (scanf ("%d", &subtitle_no) != 1) {
          gDisable_display = FALSE;
          break;
        }
        if (subtitle_no < 0 || subtitle_no > total_subtitle_no - 1) {
          g_print ("Invalid subtitle track!\n");
        } else {
          if (rate > 2.0 || rate < 0) {
            /* just record the subtitle track num, will do track select when play in normal mode*/
            sPlay->pending_sub_track = subtitle_no;
            gDisable_display = FALSE;
            break;
          }
          gst_play_set_subtitle_track (play, subtitle_no);
        }
        gDisable_display = FALSE;
      }
        break;

      case '*':                // sleep 1 second
      {
        sleep (1);
      }
        break;

      case '#':                // sleep 10 seconds
      {
        sleep (10);
      }
        break;

      case 'q':                // Query information
      {
        g_print
            ("Input query type[v:has video?, e:seekable?, s:state, p:position, u:duration, z:size, t:rotation, c:play rate]:\n");

        gDisable_display = TRUE;
        if (scanf ("%256s", sCommand) != 1) {
          gDisable_display = FALSE;
          break;
        }
        gDisable_display = FALSE;

        switch (sCommand[0]) {
          case 'v':
          {
            gint video_num = 0;
            GstPlayMediaInfo *media_info = gst_play_get_media_info (play);
            video_num =
                gst_play_media_info_get_number_of_video_streams (media_info);
            g_print ("Number of Video Streams : %d\n", video_num);
            g_object_unref (media_info);
          }
            break;

          case 'e':
          {
            gboolean seekable = FALSE;
            GstPlayMediaInfo *media_info = gst_play_get_media_info (play);
            seekable = gst_play_media_info_is_seekable (media_info);
            g_print ("Seekable : %s\n", seekable ? "Yes" : "No");
            g_object_unref (media_info);
          }
            break;

          case 's':
          {
            GstPlayState gstplay_state = GST_PLAY_STATE_STOPPED;
            gdouble playback_rate = 0.0;
            gstplay_state = sPlay->gstPlayState;
            switch (gstplay_state) {
              case GST_PLAY_STATE_PLAYING:
              {
                playback_rate = gst_play_get_rate (play);
                if (playback_rate > 1.0 && playback_rate <= 8.0) {
                  g_print ("Current State : Fast Forward\n");
                }
                if (playback_rate > 0.0 && playback_rate < 1.0) {
                  g_print ("Current State : Slow Forward\n");
                }
                if (playback_rate >= -8.0 && playback_rate <= -0.0) {
                  g_print ("Current State : Fast Backward\n");
                }
                if (playback_rate == 1.0) {
                  g_print ("Current State : Playing\n");
                }
              }
                break;

              case GST_PLAY_STATE_PAUSED:
                g_print ("Current State : Paused\n");
                break;

              case GST_PLAY_STATE_BUFFERING:
                g_print ("Current State : Buffering\n");
                break;

              case GST_PLAY_STATE_STOPPED:
                g_print ("Current State : Stopped\n");
                break;

              default:
                g_print ("Current State : Unknown\n");
                break;
            }
          }
            break;

          case 'p':
          {
            gint64 pos = 0;
            pos = gst_play_get_position (play);
            g_print ("Current playing position : %lld\n", pos);
          }
            break;

          case 'u':
          {
            gint64 duration = 0;
            duration = gst_play_get_duration (play);
            g_print ("Duration : %lld\n", duration);
          }
            break;

          case 'z':
          {
            guint x = 0;
            guint y = 0;
            guint width = 0;
            guint height = 0;
            GstPlayVideoOverlayVideoRenderer *VideoOverlayVideoRenderer =
                              GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER (VideoRender);
            gst_play_video_overlay_video_renderer_get_render_rectangle
                (VideoOverlayVideoRenderer, &x, &y, &width, &height);
            g_print ("Current video display area : %d %d %d %d\n", x, y, width,
                height);
          }
            break;

          case 't':
          {
            gint32 rotation = 0;
            rotation = gst_play_get_rotate (play);
            g_print ("Current rotation : %d\n", rotation);
          }
            break;

          case 'c':
          {
            gdouble playback_rate = 0.0;
            playback_rate = gst_play_get_rate (play);
            g_print ("Current play rate : %1.1f\n", playback_rate);
          }
            break;

          default:
            break;
        }
      }
        break;

      default:
        break;
    }
    fflush (stdout);
    fflush (stdin);
  }
  gexit_main = TRUE;
  g_print ("Exit input thread\n");
  return;
}

static gboolean
bus_callback (GstBus *bus, GstMessage *message, gpointer data)
{
  GstPlayData *sPlay = (GstPlayData *) data;
  GstPlay *play = sPlay->play;
  GstPlayMessage type;
  GstPlayState state;
  GstClockTime position;
  GError *error;
  GstStructure *details;

  gst_play_message_parse_type (message, &type);

  switch (type) {
    case GST_PLAY_MESSAGE_END_OF_STREAM:
      eos_cb (play, sPlay);
      break;
    case GST_PLAY_MESSAGE_STATE_CHANGED:
      gst_play_message_parse_state_changed (message, &state);
      state_changed_cb (play, state, sPlay);
      break;
    case GST_PLAY_MESSAGE_ERROR:
      gst_play_message_parse_error (message, &error, &details);
      error_cb (play, error, sPlay);
      break;
    case GST_PLAY_MESSAGE_SEEK_DONE:
      gst_play_message_parse_position_updated (message, &position);
      seek_done_cb (play, position, sPlay);
      break;
    default:
      break;
  }
  return TRUE;
}

int
main (int argc, char *argv[])
{
  gplay_pconfigions options;
  GThread *display_thread = NULL;
  GThread *input_thread = NULL;
  GstPlayData sPlay;
  GstPlay *play = NULL;
  gchar *uri = NULL;
  GstPlayVideoRenderer *VideoRender = NULL;
  GstPlayVideoOverlayVideoRenderer *VideoOverlayVideoRenderer = NULL;
  sPlay.loop = NULL;
  GstElement *video_sink = NULL;
  GstElement *audio_sink = NULL;
  GstElement *text_sink = NULL;
  GstBus *bus;

  reset_signal (SIGINT, signal_handler);
  /* support gplay to run in backend */
  reset_signal (SIGTTIN, signal_handler);

  g_print ("\n%s\n\n", FSL_GPLAY_VERSION_STR);

  if (argc < 2) {
    g_print ("Use -h or --help to see help message.\n");
    g_print ("Usage: gplay-1.0 [OPTIONS] PATH [PATH...]\n");
    print_help ();
    return RET_FAILURE;
  }

  gst_init (NULL, NULL);

  memset (&options, 0, sizeof (gplay_pconfigions));
  options.play_times = 0;
  options.repeat = PLAY_REPEAT_NONE;
  options.no_auto_next = FALSE;
  options.handle_buffering = FALSE;
  options.display_refresh_frq = 1;

  if (parse_pconfigions (&options, argc, argv) != RET_SUCCESS) {
    return RET_FAILURE;
  }

  if (options.timeout == 0) {
    options.timeout = DEFAULT_TIME_OUT;
  }

  if (!options.video_sink_name) {
    if (gplay_checkfeature (VPU) && gplay_checkfeature (DPU)) {
      options.video_sink_name = "imxvideoconvert_g2d ! queue ! waylandsink";
      g_print ("Set VideoSink %s \n", options.video_sink_name);
      video_sink =
        gst_parse_bin_from_description (options.video_sink_name, TRUE, NULL);
      VideoRender =
        gst_play_video_overlay_video_renderer_new_with_sink (NULL, video_sink);
    } else
      VideoRender =
        gst_play_video_overlay_video_renderer_new (NULL);
  } else {
    g_print ("Set VideoSink %s \n", options.video_sink_name);
    video_sink =
      gst_parse_bin_from_description (options.video_sink_name, TRUE, NULL);
    VideoRender =
      gst_play_video_overlay_video_renderer_new_with_sink (NULL, video_sink);
  }

  VideoOverlayVideoRenderer =
      GST_PLAY_VIDEO_OVERLAY_VIDEO_RENDERER (VideoRender);

  play =
      gst_play_new (VideoRender);

  /* check if the play engine is created successfully */
  if (!play) {
    g_print ("Create gstplay failed!\n");
    return RET_FAILURE;
  }

  if (options.audio_sink_name) {
    g_print ("Set AudioSink %s\n", options.audio_sink_name);
    audio_sink =
        gst_parse_bin_from_description (options.audio_sink_name, TRUE, NULL);
    gst_play_set_audio_sink (play, audio_sink);
  }

  if (options.text_sink_name) {
    g_print ("Set TextSink %s\n", options.text_sink_name);
    text_sink =
        gst_parse_bin_from_description (options.text_sink_name, TRUE, NULL);
    gst_play_set_text_sink (play, text_sink);
  } else if (gplay_checkfeature (VPU)
      && (gplay_checkfeature (DCSS) || gplay_checkfeature (DPU))) {
    g_print ("Disable subtitle rendering\n");
    gst_play_set_subtitle_track_enabled (play, FALSE);
  }

  sPlay.options = &options;
  sPlay.play = play;
  sPlay.VideoRender = VideoRender;
  sPlay.gstPlayState = GST_PLAY_STATE_STOPPED;
  sPlay.seek_finished = FALSE;
  sPlay.error_found = FALSE;
  sPlay.eos_found = FALSE;
  sPlay.pending_audio_track = -1;
  sPlay.pending_sub_track = -1;

  bus = gst_play_get_message_bus (play);
  gst_bus_add_watch (bus, bus_callback, &sPlay);

  sPlay.loop = g_main_loop_new (NULL, FALSE);
  gloop = sPlay.loop;

  uri = filename2uri (options.current);
  gst_play_set_uri (play, uri);
  g_free (uri);

  if (options.suburi) {
    /* load usr define subtitle uri */
    g_print ("Load subtitle: %s\n", options.suburi);
    uri = filename2uri (options.suburi);
    gst_play_set_subtitle_uri (play, uri);
    g_free (uri);

  } else {
    /* if there is dafault subtitle file, try to load - not support now */

  }

  if (options.connection_speed) {
    gst_play_set_connection_speed (play, options.connection_speed);
    g_print ("connection speed update done, value: %ld\n", options.connection_speed);
  }

  if (options.display_refresh_frq != 0) {
    display_thread =
        g_thread_new ("display_thread", (GThreadFunc) display_thread_fun,
        &sPlay);
  }
  //gst_play_play (play);
  gst_play_play_sync (play, options.timeout);

  /* for auto test script */
  g_print ("%s\n", "=========== fsl_player_play() ==================");

  fflush (stdout);
  print_menu ();

  input_thread =
      g_thread_new ("input_thread", (GThreadFunc) input_thread_fun, &sPlay);

  if (gexit_main == FALSE)
    g_main_loop_run (sPlay.loop);

  gboolean ismute = FALSE;
  ismute = gst_play_get_mute (play);
  if (ismute) {
    gst_play_pause (play);
    gst_play_set_mute (play, ismute);
  }
  gst_play_set_volume (play, 1.0);

  /* for auto test script */
//  g_print ("FSL_PLAYENGINE_UI_MSG_EXIT\n");
  g_print ("FSL_PLAYER_UI_MSG_EXIT\n");

  if (display_thread) {
    g_thread_join (display_thread);
    display_thread = NULL;
  }

  if (input_thread) {
    g_thread_unref (input_thread);
    input_thread = NULL;
  }

  if (options.pl)
    destroyPlayList (options.pl);
  options.pl = NULL;

  /* Fix gplay cannot dispose gstplay object */
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);

  gst_object_unref (play);
  g_main_loop_unref (sPlay.loop);

  /* for auto test script */
  g_print ("fsl_player_deinit\n");

  return RET_SUCCESS;
}
