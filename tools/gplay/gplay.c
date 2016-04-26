/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc. All rights reserved.
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
 * Description: New gplay application use new playengine
 * Author: Haihua Hu
 * Create Data: 2015/06/24
 * Modify By: Haihua Hu
 * Modify Data: 2015/08/03
 * Change log:
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


#include "playengine.h"
#include "playlist.h"


/* version information of gplay */
#define FSL_PLAYENGINE_VERSION "FSL_GPLAY_01.00"
#define OS_NAME "_LINUX"
#define SEPARATOR " "
#define FSL_PLAYENGINE_VERSION_STR \
    (FSL_PLAYENGINE_VERSION OS_NAME SEPARATOR "build on" \
     SEPARATOR __DATE__ SEPARATOR __TIME__)

typedef enum{
    PLAYER_REPEAT_NONE = 0,
    PLAYER_REPEAT_PLAYLIST = 1,
    PLAYER_REPEAT_CURRENT = 2,
} repeat_mode;

typedef struct{

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

}gplay_pconfigions;

/* used by display thread and event callback */
typedef struct{
  PlayEngine *engine;
  gplay_pconfigions *options;
}CustomData;

static volatile gboolean gexit_main = FALSE;
static volatile gboolean gexit_display_thread = FALSE;
static volatile gboolean gDisable_display = FALSE;


void 
alarm_signal(int signum)
{
}

void 
set_alarm(guint seconds)
{
  struct sigaction act;
  /* Register alarm handler for alarm */
  act.sa_handler = &alarm_signal;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  sigaction(SIGALRM, &act, NULL);
  /* Set timer for seconds */
  alarm(seconds);
}

PlayEngineResult
playlist_next(PlayEngine *engine,
              gplay_pconfigions *options)
{
    if(options->repeat > PLAYER_REPEAT_CURRENT || options->repeat < PLAYER_REPEAT_NONE)
    {
      g_print("Unknown repeat mode\n");
      return PLAYENGINE_FAILURE;
    }

    switch( options->repeat )
    {
        case PLAYER_REPEAT_NONE:
        {
            options->current = getNextItem(options->pl);
            if(options->current)
            {
                g_print("Now Playing: %s\n", options->current);
                engine->set_file((PlayEngineHandle)engine, options->current);
                engine->play((PlayEngineHandle)engine);
            }
            else
            {
                gboolean islast = FALSE;
                isLastItem(options->pl, &islast);
                if(islast)
                {
                    g_print("No more media file, exit gplay!\n");
                }else
                {
                    g_print("playlist unknown error\n");
                }
                return PLAYENGINE_FAILURE;
            }
        }
        break;

        case PLAYER_REPEAT_PLAYLIST:
        {
            options->current = getNextItem(options->pl);
            if(options->current)
            {
                printf("Now Playing: %s\n", options->current);
                engine->set_file((PlayEngineHandle)engine, options->current);
                engine->play((PlayEngineHandle)engine);
            }
            else
            {
                gboolean islast = FALSE;
                isLastItem(options->pl, &islast);
                if(islast)
                {
                    if (options->play_times>0){
                      options->play_times--;
                      if (options->play_times == 0) {
                        g_print("Repeat mode finished\n");
                        return PLAYENGINE_FAILURE;
                      }
                    }
                    options->current = getFirstItem(options->pl);
                    if(options->current)
                    {
                      printf("Now Playing: %s\n", options->current);
                      engine->set_file((PlayEngineHandle)engine, options->current);
                      engine->play((PlayEngineHandle)engine);
                    }
                }else
                {
                    g_print("playlist unknown error\n");
                    return PLAYENGINE_FAILURE;
                }
            }
        }
        break;

        case PLAYER_REPEAT_CURRENT:
        {
            engine->play((PlayEngineHandle)engine);
        }
        break;

        default:
        break;
    }
    return PLAYENGINE_SUCCESS;
}

PlayEngineResult
playlist_previous(PlayEngine *engine,
                  gplay_pconfigions * options)
{
    gchar iName[1024];
    if(options->repeat > PLAYER_REPEAT_CURRENT || options->repeat < PLAYER_REPEAT_NONE)
    {
      g_print("Unknown repeat mode\n");
      return PLAYENGINE_FAILURE;
    }

    switch( options->repeat )
    {
        case PLAYER_REPEAT_NONE:
        {
            options->current = getPrevItem(options->pl);
            if(options->current)
            {
                g_print("Now Playing: %s\n", options->current);
                engine->set_file((PlayEngineHandle)engine, options->current);
                engine->play((PlayEngineHandle)engine);
            }
            else
            {
                gboolean isFirst = FALSE;
                isFirstItem(options->pl, &isFirst);
                if(isFirst)
                {
                    g_print("No more media file, exit gplay!\n");
                }else
                {
                    g_print("playlist unknown error\n");
                }
                return PLAYENGINE_FAILURE;
            }
        }
        break;

        case PLAYER_REPEAT_PLAYLIST:
        {
            options->current = getPrevItem(options->pl);
            if(options->current)
            {
                printf("Now Playing: %s\n", options->current);
                engine->set_file((PlayEngineHandle)engine, options->current);
                engine->play((PlayEngineHandle)engine);
            }
            else
            {
                gboolean isFirst = FALSE;
                isFirstItem(options->pl, &isFirst);
                if(isFirst)
                {
                    if (options->play_times>0){
                      options->play_times--;
                      if (options->play_times == 0) {
                        g_print("Repeat mode finished\n");
                        return PLAYENGINE_FAILURE;
                      }
                    }

                    options->current = getLastItem(options->pl);
                    if(options->current)
                    {
                      printf("Now Playing: %s\n", options->current);
                      engine->set_file((PlayEngineHandle)engine, options->current);
                      engine->play((PlayEngineHandle)engine);
                    }
                }else
                {
                    g_print("playlist unknown error\n");
                    return PLAYENGINE_FAILURE;
                }
            }
        }
        break;

        case PLAYER_REPEAT_CURRENT:
        {
            engine->play((PlayEngineHandle)engine);
        }
        break;

        default:
        break;
    }
    return PLAYENGINE_SUCCESS;
}


void 
event_callback(void *context,
               EventType EventID,
               void *Eventpayload)
{
  CustomData * PlayData = (CustomData *)context;

  if(!PlayData || !PlayData->engine || !PlayData->options)
  {
    g_print("player pointer is invalid!\n");
    gexit_main = TRUE;
    gexit_display_thread = TRUE;
    set_alarm(1);
    return;
  }
  gplay_pconfigions *options = PlayData->options;
  PlayEngine *engine = PlayData->engine;

  switch(EventID)
  {
    case EVENT_ID_EOS:
    {
      /* for auto test script */
      g_print("EOS Found\n");
      engine->stop((PlayEngineHandle)engine);

      if(!options->no_auto_next)
      {
        if(playlist_next(engine, options) != PLAYENGINE_SUCCESS)
        {
          gexit_main = TRUE;
          gexit_display_thread = TRUE;
          set_alarm(1);
        }else{
          g_print("play next item successfully\n");
        }
      }else{
        g_print("no auto next is on\n");
        gexit_main = TRUE;
        gexit_display_thread = TRUE;
        set_alarm(1);
      }

    }
    break;

    case EVENT_ID_ERROR:
    {
      const gchar * errstr = (gchar *)Eventpayload;
      if(errstr)
        g_print("gplay meet internal error: %s\n",errstr);
      engine->stop((PlayEngineHandle)engine);
      gexit_main = TRUE;
      gexit_display_thread = TRUE;
      set_alarm(1);
    }
    break;

    case EVENT_ID_STATE_CHANGE:
    {
      /* to do here to process state change */
      PlayEngineState *state = (PlayEngineState *)Eventpayload;
      if(state)
        g_print("State change from %s to %s\n",gst_element_state_get_name(state->old_st),
                                              gst_element_state_get_name(state->new_st));
    }
    break;

    case EVENT_ID_BUFFERING:
    {
      /* to do here to process buffering */
      const gint *percent = (gint *)Eventpayload;
      if(options->handle_buffering){
        if(percent){
          g_print("\r\t\t\t\t\t\t%s %d%%", "buffering...", *percent);

          if(*percent == 0)
          {
            engine->pause((PlayEngineHandle)engine);
          }else if(*percent >= 100)
          {
            engine->play((PlayEngineHandle)engine);
          }
        }
      }
    }
    break;

    default:
    break;
  }
}

void
print_help()
{
  g_print ("options :\n");
  g_print ("    --quiet           Disable playback status display updating\n\n");
  g_print ("    --repeat          Set gplay to playlist repeat mode.\n");
  g_print ("                      Use below option to specify your repeat times\n");
  g_print ("                      --repeat=PlayTimes\n\n");
  g_print ("    --video-sink      Specify the video sink instead of default sink\n");
  g_print ("                      Use below option to input your video sink name\n");
  g_print ("                      --video-sink=video_sink_name\n\n");
  g_print ("    --audio-sink      Specify the audio sink instead of default sink\n");
  g_print ("                      Use below option to input your audio sink name\n");
  g_print ("                      --audio-sink=audio_sink_name\n\n");
  g_print ("    --text-sink       Specify the text sink instead of default sink\n");
  g_print ("                      Use below option to input your text sink name\n");
  g_print ("                      --text-sink=text_sink_name\n\n");
  g_print ("    --suburi          Set subtitle path\n");
  g_print ("                      Use below option to input your pathname\n");
  g_print ("                      --suburi=pathname\n\n");
}

PlayEngineResult
parse_pconfigions(gplay_pconfigions *pconfig, 
              gint32 argc, 
              gchar* argv[])
{
    gint32 i;


    pconfig->pl = createPlayList();
    if ((void *)pconfig->pl == NULL){
        printf("Can not create Playlist!!\n");
        return PLAYENGINE_FAILURE;
    }

    pconfig->play_times = -1;

    for (i=1;i<argc;i++){
        if (strlen(argv[i])) {
            if (argv[i][0]=='-'){
                if ((strcmp(argv[i], "-h")==0)||(strcmp(argv[i], "--help")==0)){
                    g_print ("Usage: gplay-1.0 [OPTIONS] PATH [PATH...]\n");
                    print_help();
                    goto err;
                }

                if (strcmp(argv[i], "--quiet")==0){
                    pconfig->display_refresh_frq = 0;
                    continue;
                }

                if (strncmp(argv[i], "--repeat", 8)==0){
                  if (argv[i][8]=='='){
                    pconfig->play_times = atoi(&(argv[i][9]));
                  }
                  pconfig->repeat = PLAYER_REPEAT_PLAYLIST;
                  continue;
                }

                if ((strncmp(argv[i], "--video-sink", 12)==0)){
                  if (argv[i][12]=='='){
                    pconfig->video_sink_name = &(argv[i][13]);
                  }
                  continue;
                }

                if ((strncmp(argv[i], "--audio-sink", 12)==0)){
                  if (argv[i][12]=='='){
                    pconfig->audio_sink_name = &(argv[i][13]);
                  }
                  continue;
                }

                if ((strncmp(argv[i], "--text-sink", 11)==0)) {
                  if (argv[i][11]=='=') {
                    pconfig->text_sink_name = &(argv[i][12]);
                  }
                  continue;
                }

                if ((strncmp(argv[i], "--suburi", 8)==0)) {
                  if (argv[i][8]=='=') {
                    pconfig->suburi = &(argv[i][9]);
                  }
                  continue;
                }

                if (strncmp(argv[i], "--timeout", 9)==0){
                  if (argv[i][9]=='='){
                    pconfig->timeout = atoi(&(argv[i][10]));
                  }
                  continue;
                }

                if ((strncmp(argv[i], "--info-interval", 14)==0)){
                  if (argv[i][14]=='='){
                    pconfig->display_refresh_frq = atoi(&(argv[i][15]));
                  }
                  continue;
                }

                if ((strcmp(argv[i], "--noautonext") == 0)) {
                  pconfig->no_auto_next = TRUE;
                  continue;
                }

                if ((strcmp(argv[i], "--handle-buffering")==0)){
                  pconfig->handle_buffering = TRUE;
                  continue;
                }

                continue;
            }else{

              if(addItemAtTail(pconfig->pl, argv[i]) != PLAYENGINE_SUCCESS)
                goto err;
              continue;
            }
        }
    }

    pconfig->current = getFirstItem(pconfig->pl);
    if(pconfig->current == NULL)
    {
        g_print("NO File specified!!\n");
        goto err;
    }

    return PLAYENGINE_SUCCESS;

err:
    if (pconfig->pl){
        destroyPlayList(pconfig->pl);
        pconfig->pl=NULL;
    }
    return PLAYENGINE_FAILURE;
}



void
display_thread_fun(gpointer data)
{
  CustomData * PlayData = (CustomData *)data;
  gchar str_repeated_mode[3][20] = {"(No Repeated)", "(List Repeated)", "(Current Repeated)"};

  if(!PlayData || !PlayData->engine || !PlayData->options)
  {
    g_print("Invalid Custom Data pointer\n");
    return;
  }
  PlayEngine * engine = PlayData->engine;
  gplay_pconfigions *options = PlayData->options;

  while(1)
  {
    if( TRUE == gexit_display_thread )
    {
        g_print("Exit display thread\n");
        engine->stop_wait_state_change((PlayEngineHandle)engine);
        return;
    }
    if( FALSE == gDisable_display )
    {
        gchar str_player_state_rate[16];
        gchar str_volume[16];
        gchar* prepeated_mode = &(str_repeated_mode[options->repeat][0]);
        guint64 hour, minute, second;
        guint64 hour_d, minute_d, second_d;
        guint64 duration=0;
        guint64 elapsed=0;
        GstState player_state = GST_STATE_NULL;
        gdouble playback_rate = 0.0;
        gboolean bmute = 0;
        gdouble volume = 0.0;

        engine->get_duration((PlayEngineHandle)engine, &duration);
        engine->get_position((PlayEngineHandle)engine, &elapsed);
        engine->get_state((PlayEngineHandle)engine, &player_state);
        engine->get_play_rate((PlayEngineHandle)engine, &playback_rate);
        engine->get_mute((PlayEngineHandle)engine, &bmute);
        engine->get_volume((PlayEngineHandle)engine, &volume);
        
        if(duration == GST_CLOCK_TIME_NONE || elapsed == GST_CLOCK_TIME_NONE)
          continue;

        hour = (elapsed/ (gint64)3600000000000);
        minute = (elapsed / (guint64)60000000000) - (hour * 60);
        second = (elapsed / 1000000000) - (hour * 3600) - (minute * 60);
        hour_d = (duration/ (guint64)3600000000000);
        minute_d = (duration / (guint64)60000000000) - (hour_d * 60);
        second_d = (duration / 1000000000) - (hour_d * 3600) - (minute_d * 60);

        switch(player_state)
        {
          case GST_STATE_PLAYING:
          {
            if( playback_rate > 1.0 && playback_rate <= 8.0 )
            {
              sprintf(str_player_state_rate, "%s(%1.1fX)", "FF ", playback_rate);
            }
            if( playback_rate > 0.0 && playback_rate < 1.0 )
            {
              sprintf(str_player_state_rate, "%s(%1.1fX)", "SF ", playback_rate);
            }
            if( playback_rate >= -8.0 && playback_rate <= -0.0 )
            {
              sprintf(str_player_state_rate, "%s(%1.1fX)", "FB ", playback_rate);
            }
            if( playback_rate == 1.0 )
            {
              sprintf(str_player_state_rate, "%s", "Playing ");
            }
          }
          break;

          case GST_STATE_PAUSED:
            sprintf(str_player_state_rate, "%s", "Pause ");
          break;

          case GST_STATE_READY:
            sprintf(str_player_state_rate, "%s", "Ready ");

          case GST_STATE_NULL:
            sprintf(str_player_state_rate, "%s", "Stop ");
          break;

          default:
            sprintf(str_player_state_rate, "%s", "Unknown ");
          break;
        }

        if( bmute )
        {
            sprintf(str_volume, "%s", "MUTE ");
        }
        else
        {
            sprintf(str_volume, "Vol=%.1lf", volume);
        }

        g_print("\r[%s%s][%s][%02d:%02d:%02d/%02d:%02d:%02d]",
            str_player_state_rate, prepeated_mode, str_volume,
            (gint32)hour, (gint32)minute, (gint32)second,
            (gint32)hour_d, (gint32)minute_d, (gint32)second_d);

        fflush (stdout);
    }
    sleep(1);
  }
}

void 
print_metadata(PlayEngine *engine)
{
    gint64 duration=0;
    imx_metadata metadata;
    gint32 i;

    if(!engine)
    {
      g_print("PlayEngine pointer invalid, print metadata failed\n");
      return;
    }

    engine->get_metadata((PlayEngineHandle)engine, &metadata);
    engine->get_duration((PlayEngineHandle)engine, &duration);

    /* g_print not support Chinese, should use printf when show metadata */
    printf("\nMedia URI: %s\n", metadata.pathname);
    printf("\tContainer: %s\n", metadata.container);
    printf("\tDuration: %d seconds\n", (gint32)(duration/1000000000));
    printf("\tTitle: %s\n", metadata.title);
    printf("\tAritist: %s\n", metadata.artist);
    printf("\tAlbum: %s\n", metadata.album);
    printf("\tCreationDate: %s\n", metadata.year);
    printf("\tGenre: %s\n", metadata.genre);
    printf("\tAlbumartist: %s\n", metadata.albumartist);
    printf("\tComposer: %s\n", metadata.composer);
    printf("\tCopyright: %s\n", metadata.copyright);
    printf("\tDescription: %s\n", metadata.description);
    printf("\tPerformer: %s\n", metadata.performer);
    printf("\tKeywords: %s\n", metadata.keywords);
    printf("\tComment: %s\n", metadata.comment);
    printf("\tTool: %s\n", metadata.tool);
    printf("\tLocation latitude: %s\n", metadata.location_latitude);
    printf("\tLocation longtitude: %s\n", metadata.location_longtitude);
    printf("\tTrackCount: %d\n", metadata.track_count);
    printf("\tTrackNumber: %d\n", metadata.track_number);
    printf("\tDiscNumber: %d\n", metadata.disc_number);
    printf("\tRating: %d\n", metadata.rating);
    for (i=0; i<metadata.n_audio; i++) {
      printf("Audio%d:\n", i);
      printf("\tCodec: %s\n", metadata.audio_info[i].codec_type);
      printf("\tSample Rate: %d\n", metadata.audio_info[i].samplerate);
      printf("\tChannels: %d\n", metadata.audio_info[i].channels);
      printf("\tBitrate: %d\n", metadata.audio_info[i].bitrate);
      printf("\tLanguage Code: %s\n", metadata.audio_info[i].language);
    }

    for (i=0; i<metadata.n_video; i++) {
      printf("Video%d:\n", i);
      printf("\tCodec: %s\n", metadata.video_info[i].codec_type);
      printf("\tWidth: %d\n", metadata.video_info[i].width);
      printf("\tHeight: %d\n", metadata.video_info[i].height);
      printf("\tFrame Rate: %f\n", (float)metadata.video_info[i].framerate_numerator / (float)metadata.video_info[i].framerate_denominator);
      printf("\tBitrate: %d\n", metadata.video_info[i].bitrate);
      printf("\tLanguage Code: %s\n", metadata.video_info[i].language);
    }

    for (i=0; i<metadata.n_subtitle; i++) {
      printf("subtitle%d:\n", i);
      printf("\tCodec: %s\n", metadata.subtitle_info[i].codec_type);
      printf("\tLanguage Code: %s\n", metadata.subtitle_info[i].language);
    }
}

void 
print_menu()
{
    g_print("\n%s\n", FSL_PLAYENGINE_VERSION_STR);
    g_print("\t[h]display the operation Help\n");
    g_print("\t[p]Play\n");
    g_print("\t[s]Stop\n");
    g_print("\t[e]Seek\n");
    g_print("\t[a]Pause when playing, play when paused\n");
    g_print("\t[v]Volume\n");
    g_print("\t[m]Switch to mute or not\n");
    g_print("\t[>]Play next file\n");
    g_print("\t[<]Play previous file\n");
    g_print("\t[r]Switch to repeated mode or not\n");
    g_print("\t[u]Select the video track\n");
    g_print("\t[d]Select the audio track\n");
    g_print("\t[b]Select the subtitle track\n");
    g_print("\t[f]Set full screen\n");
    g_print("\t[z]resize the width and height\n");
    g_print("\t[t]Rotate\n");
    g_print("\t[c]Setting play rate\n");
    g_print("\t[i]Display the metadata\n");
    g_print("\t[x]eXit\n");
}

static void 
signal_handler(int sig)
{
  switch(sig)
  {
    case SIGINT:
      g_print(" Aborted by signal[%d] Interrupt...\n", sig);
      gexit_main = TRUE;
      gexit_display_thread = TRUE;
    break;

    case SIGTTIN:
    /* Nothing need do */
    break;

    default:
    break;
  }
}

static void
reset_signal(int sig, void *handler)
{
  if(!handler)
    return;

  struct sigaction act;
  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  sigaction(sig, &act, NULL);
}

int 
main(int argc,char *argv[])
{ 
  gchar sCommand[256];
  PlayEngine * playengine = NULL;
  gplay_pconfigions options;
  GThread * display_thread = NULL;
  CustomData PlayData;

  reset_signal(SIGINT, signal_handler);
  /* support gplay to run in backend */
  reset_signal(SIGTTIN, signal_handler);

  if( argc < 2 )
  {
    g_print ("Use -h or --help to see help message.\n");
    g_print ("Usage: gplay-1.0 [OPTIONS] PATH [PATH...]\n");
    print_help();
    return PLAYENGINE_FAILURE;
  }

  memset(&options, 0, sizeof(gplay_pconfigions));
  options.play_times = 0;
  options.repeat = PLAYER_REPEAT_NONE;
  options.no_auto_next = FALSE;
  options.handle_buffering = FALSE;
  options.display_refresh_frq = 1;
  //strcpy(options.text_sink_name,"fakesink");

  if(parse_pconfigions(&options, argc, argv) != PLAYENGINE_SUCCESS)
  {
    return PLAYENGINE_FAILURE;
  }


  /* create playback engine */
  playengine = play_engine_create();

  /* check if the play engine is created successfully */
  if(!playengine)
  {
    g_print("Create play engine failed!\n");
    return PLAYENGINE_FAILURE;
  }

  PlayData.engine = playengine;
  PlayData.options = &options;

  if(options.timeout != 0)
  {
    playengine->set_state_change_timeout((PlayEngineHandle)playengine, options.timeout);
  }

  if (!options.video_sink_name)
    if(play_engine_checkfeature(PLAYENGINE_G2D)) {
      options.video_sink_name = "overlaysink";
    }else {
      options.video_sink_name = "imxv4l2sink";
    }

  g_print("Set VideoSink %s\n", options.video_sink_name);
  playengine->set_video_sink((PlayEngineHandle)playengine, options.video_sink_name);

  if (options.audio_sink_name){
      g_print("Set AudioSink %s\n", options.audio_sink_name);
      playengine->set_audio_sink((PlayEngineHandle)playengine, options.audio_sink_name);
  }

  if (options.text_sink_name) {
      g_print("Set TextSink %s\n", options.text_sink_name);
      playengine->set_text_sink((PlayEngineHandle)playengine, options.text_sink_name);
  }
  if (options.suburi)
  {
    /* load usr define subtitle uri */
    g_print("Load subtitle: %s\n",options.suburi);
    playengine->set_subtitle_uri((PlayEngineHandle)playengine, options.suburi);
  }else
  {
    /* if there is dafault subtitle file, try to load - not support now*/
     
  }

  playengine->reg_event_handler((PlayEngineHandle)playengine, &PlayData, event_callback);

  if(playengine->set_file(playengine,options.current) != PLAYENGINE_SUCCESS)
  {
    g_print("set file failed\n");
    if(options.pl)
      destroyPlayList(options.pl);
    options.pl = NULL;
    if(playengine)
      play_engine_destroy(playengine);
    playengine = NULL;
    return PLAYENGINE_FAILURE;
  }

  if(options.display_refresh_frq != 0)
  {
    display_thread = g_thread_new("display_thread", (GThreadFunc)display_thread_fun, &PlayData);
  }

  if(playengine->play((PlayEngineHandle)playengine) !=PLAYENGINE_SUCCESS)
  {
    g_print("try to play failed\n");
    if(options.pl)
      destroyPlayList(options.pl);
    options.pl = NULL;
    if(playengine)
      play_engine_destroy(playengine);
    playengine = NULL;

    gexit_display_thread = TRUE;
    if(display_thread)
      g_thread_join(display_thread);

    return PLAYENGINE_FAILURE;
  }
  /* for auto test script */
  g_print("%s\n","fsl_player_play()");

  fflush(stdout);
  print_menu();
  
  while(gexit_main == FALSE){    
    sCommand[0] = ' ';
    errno = 0;
    if(scanf("%256s", sCommand) != 1){
      continue;
    }

    switch( sCommand[0] )
    {
        case 'h': // display the operation Help.
            print_menu();
            break;

        case 'p': // Play.
        {
          GstState player_state = GST_STATE_NULL;
          playengine->get_state((PlayEngineHandle)playengine,&player_state);
          if (player_state == GST_STATE_NULL) {
            playengine->set_file(playengine, options.current);
            playengine->play(playengine);
          }
        }
        break;

        case 's': // Stop
        {
          playengine->stop((PlayEngineHandle)playengine);
        }
        break;

        case 'a': // pAuse
        {
          GstState player_state = GST_STATE_NULL;
          playengine->get_state((PlayEngineHandle)playengine, &player_state);

          if (player_state == GST_STATE_NULL) {
            playengine->set_file(playengine, options.current);
            playengine->play(playengine);
          }else if(player_state == GST_STATE_PLAYING)
            playengine->pause((PlayEngineHandle)playengine);
          else if(player_state == GST_STATE_PAUSED)
             playengine->play((PlayEngineHandle)playengine);
        }
        break;

        case 'e': // sEek
        {
          guint32 seek_point_sec = 0;
          guint64 duration_ns = 0;
          guint64 duration_sec = 0;
          guint32 seek_portion = 0;
          gboolean accurate_seek = FALSE;//
          gint input_mode = -1;
          gboolean seekable = FALSE;

          gDisable_display = TRUE;
          playengine->get_seekable((PlayEngineHandle)playengine, &seekable);
          if(!seekable){
              g_print("file is not seekable!\n");
              gDisable_display = FALSE;
              //kb_set_raw_term(STDIN_FILENO);
              break;
          }
          playengine->get_duration((PlayEngineHandle)playengine, &duration_ns);
          duration_sec = duration_ns / 1000000000;
          //kb_restore_term(STDIN_FILENO);

          g_print("Select seek mode[Fast seek:0,Accurate seek:1]:");
          if(scanf("%d",&input_mode) != 1)
          {
            gDisable_display = FALSE;
            break;
          }

          if( input_mode != 0 && input_mode != 1  )
          {
              g_print("Invalid seek mode!\n");
              gDisable_display = FALSE;
              break;
          }else{
              accurate_seek = (gboolean)input_mode;
          }

          g_print("%s seek to percentage[0:100] or second [t?]:", accurate_seek?"Accurate":"Normal");
          if(scanf("%256s",sCommand) != 1)
          {
            gDisable_display = FALSE;
            break;
          }
          if (sCommand[0]=='t'){
              seek_point_sec = atoi(&sCommand[1]);
          }else{
              seek_portion = atoi(sCommand);
              
              if( seek_portion>100 )
              {
                  g_print("Invalid seek point!\n");
                  gDisable_display = FALSE;
                  break;
              }
              seek_point_sec = (guint64)(seek_portion * duration_sec / 100);
          }
          gDisable_display = FALSE;
          //kb_set_raw_term(STDIN_FILENO);
          playengine->seek((PlayEngineHandle)playengine, seek_point_sec, accurate_seek);
        }
        break;

        case 'v': //Volume

        {
          gdouble volume;
          g_print("Set volume[0-1.0]:");
          //kb_restore_term(STDIN_FILENO);
          gDisable_display = TRUE;
          if(scanf("%lf",&volume) != 1)
          {
            gDisable_display = FALSE;
            break;
          }
          gDisable_display = FALSE;
          //kb_set_raw_term(STDIN_FILENO);
          playengine->set_volume((PlayEngineHandle)playengine, volume);
        }
        break;

        case 'm':
        {
          gboolean mute_status = FALSE;
          playengine->get_mute((PlayEngineHandle)playengine, &mute_status);
          if(mute_status == FALSE)
          {
            playengine->set_mute((PlayEngineHandle)playengine, TRUE);
          }else
          {
            playengine->set_mute((PlayEngineHandle)playengine, FALSE);
          }
        }
        break;
 
        case 'c': // playing direction and speed Control.
        {
            gdouble playback_rate;
            g_print("Set playing speed[-8,-4,-2,0.125,0.25,0.5,1,2,4,8]:");
            //kb_restore_term(STDIN_FILENO);
            gDisable_display = TRUE;
            if(scanf("%lf",&playback_rate) != 1)
            {
              gDisable_display = FALSE;
              break;
            }
            gDisable_display = FALSE;
            //kb_set_raw_term(STDIN_FILENO);
            playengine->set_play_rate((PlayEngineHandle)playengine, playback_rate);
        }
        break;

        case 'i':
          print_metadata((PlayEngineHandle)playengine);
        break;

        case 'x':// eXit
        {
          playengine->stop((PlayEngineHandle)playengine);
          g_print("Ready to exit this app!\n");
          gexit_main = TRUE;
          gexit_display_thread = TRUE;
        }
        break;
        
        case 'r': // Switch to repeated mode or not
        {
          gint setrepeat;
          g_print("input repeated mode[0 for no repeated,1 for play list repeated, 2 for current file repeated]:");
          //kb_restore_term(STDIN_FILENO);
          gDisable_display = TRUE;
          if(scanf("%d",&setrepeat) != 1)
          {
            gDisable_display = FALSE;
            break;
          }
          if( setrepeat<0 || setrepeat>2  )
          {
              g_print("Invalid repeated mode!\n");
          }
          else
          {
              options.repeat = setrepeat;
          }
          gDisable_display = FALSE;
          //kb_set_raw_term(STDIN_FILENO);
        }
        break;
 
        case '>': // Play next file
          g_print("next\n");
          playengine->stop((PlayEngineHandle)playengine);
          if (playlist_next(playengine, &options) != PLAYENGINE_SUCCESS)
          {  
            gexit_main = TRUE;
            gexit_display_thread = TRUE;
            set_alarm(1);
          }
        break;

        case '<': // Play previous file
          g_print("previous\n");
          playengine->stop((PlayEngineHandle)playengine);
          if(playlist_previous(playengine, &options) != PLAYENGINE_SUCCESS)
          {
            gexit_main = TRUE;
            gexit_display_thread = TRUE;
            set_alarm(1);
          }
        break;

        case 't': // Rotate 90 degree every time
        {
            gint rotate_value;
            g_print("Set rotation between 0, 90, 180, 270: ");
            //kb_restore_term(STDIN_FILENO);
            gDisable_display = TRUE;
            if(scanf("%d",&rotate_value) != 1)
            {
              gDisable_display = FALSE;
              break;
            }
            gDisable_display = FALSE;
            //kb_set_raw_term(STDIN_FILENO);
            if(rotate_value != 0  && rotate_value != 90 && rotate_value != 180 && rotate_value != 270 )
            {
                g_print("Invalid rotation value=%d, should input [0, 90, 180, 270]\n", rotate_value);
                break;
            }
            playengine->set_rotate((PlayEngineHandle)playengine, rotate_value);
        }
        break;

        case 'z': // resize the width and height
        {
            DisplayArea area;
            guint x = 0;
            guint y = 0;
            guint width = 0;
            guint height = 0;
            g_print("Input [x y width height]:");
            //kb_restore_term(STDIN_FILENO);
            gDisable_display = TRUE;
            if(scanf("%d %d %d %d", &x, &y, &width, &height) != 4)
            {
              gDisable_display = FALSE;
              break;
            }
            gDisable_display = FALSE;
            //kb_set_raw_term(STDIN_FILENO);
            area.offsetx = x;
            area.offsety = y;
            area.width = width;
            area.height = height;

            playengine->set_render_rect((PlayEngineHandle)playengine, area);
            playengine->expose_video((PlayEngineHandle)playengine);
        }
        break;

        case 'f':
        {
          playengine->set_fullscreen((PlayEngineHandle)playengine);
          playengine->expose_video((PlayEngineHandle)playengine);
        }
        break;

        case 'u': // Select the video track
        {
            gint32 video_track_no = 0;
            gint32 total_video_no = 0;
            guint64 elapsed=0;
            playengine->get_video_num((PlayEngineHandle)playengine, &total_video_no);
            g_print("input video track number[0,%d]:",total_video_no-1);
            //kb_restore_term(STDIN_FILENO);
            gDisable_display = TRUE;
            if(scanf("%d",&video_track_no) != 1)
            {
              gDisable_display = FALSE;
              break;
            }
            if( video_track_no < 0 || video_track_no > total_video_no-1 )
            {
                g_print("Invalid video track!\n");
            }
            else
            {
                playengine->select_video((PlayEngineHandle)playengine, video_track_no);
            }
            gDisable_display = FALSE;
            //kb_set_raw_term(STDIN_FILENO);
        }
        break;

        case 'd': // Select the audio track
        {
            gint32 audio_track_no = 0;
            gint32 total_audio_no = 0;
            guint64 elapsed=0;
            playengine->get_audio_num((PlayEngineHandle)playengine, &total_audio_no);
            g_print("input audio track number[0,%d]:",total_audio_no-1);
            //kb_restore_term(STDIN_FILENO);
            gDisable_display = TRUE;
            if(scanf("%d",&audio_track_no) != 1)
            {
              gDisable_display = FALSE;
              break;
            }
            if( audio_track_no < 0 || audio_track_no > total_audio_no-1 )
            {
                g_print("Invalid audio track!\n");
            }
            else
            {
                playengine->select_audio((PlayEngineHandle)playengine, audio_track_no);
            }
            gDisable_display = FALSE;
            //kb_set_raw_term(STDIN_FILENO);
        }
        break;
        
        case 'b': // Select the subtitle
        {
            gint32 subtitle_no = 0;
            gint32 total_subtitle_no = 0;
            playengine->get_subtitle_num((PlayEngineHandle)playengine, &total_subtitle_no);
            g_print("input subtitle number[0,%d]:",total_subtitle_no-1);
            //kb_restore_term(STDIN_FILENO);
            gDisable_display = TRUE;
            if(scanf("%d",&subtitle_no) != 1)
            {
              gDisable_display = FALSE;
              break;
            }
            if( subtitle_no < 0 || subtitle_no > total_subtitle_no-1 )
            {
                g_print("Invalid subtitle track!\n");
            }
            else
            {
                playengine->select_subtitle((PlayEngineHandle)playengine, subtitle_no);
            }
            gDisable_display = FALSE;
            //kb_set_raw_term(STDIN_FILENO);
        }
        break;

        case '*':// sleep 1 second
        {
          sleep(1);
        }
        break;

        case '#':// sleep 10 seconds
        {
          sleep(10);
        }
        break;

        case 'q': // Query information
        {
            g_print("Input query type[v:has video?, e:seekable?, s:state, p:position, u:duration, z:size, t:rotation, c:play rate]:\n");

            //kb_restore_term(STDIN_FILENO);
            gDisable_display = TRUE;
            if(scanf("%256s",sCommand) != 1)
            {
              gDisable_display = FALSE;
              break;
            }
            gDisable_display = FALSE;
            //kb_set_raw_term(STDIN_FILENO);

            switch(sCommand[0])
            {
              case 'v':
              {
                gint video_num = 0;
                playengine->get_video_num((PlayEngineHandle)playengine, &video_num);
                g_print("Number of Video Streams : %d\n", video_num);
              }
              break;

              case 'e':
              {
                gboolean seekable = FALSE;
                playengine->get_seekable((PlayEngineHandle)playengine, &seekable);
                g_print("Seekable : %s\n", seekable ? "Yes" : "No");
              }
              break;

              case 's':
              {
                GstState player_state = GST_STATE_NULL;
                gdouble playback_rate = 0.0;
                playengine->get_state((PlayEngineHandle)playengine, &player_state);
                playengine->get_play_rate((PlayEngineHandle)playengine, &playback_rate);
                switch(player_state)
                {
                  case GST_STATE_PLAYING:
                  {
                    if( playback_rate > 1.0 && playback_rate <= 8.0 )
                    {
                      g_print("Current State : Fast Forward\n");
                    }
                    if( playback_rate > 0.0 && playback_rate < 1.0 )
                    {
                      g_print("Current State : Slow Forward\n");
                    }
                    if( playback_rate >= -8.0 && playback_rate <= -0.0 )
                    {
                       g_print("Current State : Fast Backward\n");
                    }
                    if( playback_rate == 1.0 )
                    {
                      g_print("Current State : Playing\n");
                    }
                  }
                  break;

                  case GST_STATE_PAUSED:
                    g_print("Current State : Paused\n");
                  break;

                  case GST_STATE_READY:
                    g_print("Current State : Ready\n");
                  break;

                  case GST_STATE_NULL:
                    g_print("Current State : Stopped\n");
                  break;

                  default:
                    g_print("Current State : Unknown\n");
                  break;
                }
              }
              break;

              case 'p':
              {
                  gint64 pos = 0;
                  playengine->get_position((PlayEngineHandle)playengine, &pos);
                  g_print("Current playing position : %lld\n", pos);
              }
              break;

              case 'u':
              {
                  gint64 duration = 0;
                  playengine->get_duration((PlayEngineHandle)playengine, &duration);
                  g_print("Duration : %lld\n", duration);
              }
              break;

              case 'z':
              {
                  DisplayArea area = {0};
                  playengine->get_display_area((PlayEngineHandle)playengine, &area);
                  g_print("Current video display area : %d %d %d %d\n", area.offsetx, area.offsety, area.width, area.height);
              }
              break;

              case 't':
              {
                  gint32 rotation = 0;
                  playengine->get_rotate((PlayEngineHandle)playengine, &rotation);
                  g_print("Current rotation : %d\n", rotation);
              }
              break;

              case 'c':
              {
                  gdouble playback_rate = 0.0;
                  playengine->get_play_rate((PlayEngineHandle)playengine, &playback_rate);
                  g_print("Current play rate : %1.1f\n", playback_rate);
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
    fflush(stdout);
    fflush(stdin);
  }

  gboolean ismute = FALSE;
  playengine->get_mute((PlayEngineHandle)playengine, &ismute);
  if(ismute)
  {
    playengine->pause((PlayEngineHandle)playengine);
    playengine->set_mute((PlayEngineHandle)playengine, FALSE);
  }
  playengine->set_volume((PlayEngineHandle)playengine, 1.0);

  /* for auto test script */
  g_print("FSL_PLAYENGINE_UI_MSG_EXIT\n");

  if(options.pl)
    destroyPlayList(options.pl);
  options.pl = NULL;
  if(playengine)
    play_engine_destroy(playengine);
  playengine = NULL;
  if(display_thread)
    g_thread_join(display_thread);

  /* for auto test script */
  g_print("fsl_player_deinit\n");

  return PLAYENGINE_SUCCESS;
}
