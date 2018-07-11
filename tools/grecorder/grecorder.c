/*
 * Copyright (c) 2014-2015, Freescale Semiconductor, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>
#define __USE_LARGEFILE64
#include <sys/statvfs.h>

#include "recorder_engine.h"

#define LOG_ERROR printf
#define LOG_INFO printf
#define LOG_DEBUG printf

#define LATEST_REMAIN_SPACE_SIZE (50*1024*1024)

#define START_MEDIATIME_INFO_THREAD(thread, recorder)\
  do{\
    if (thread == NULL){\
      exit_thread = RE_BOOLEAN_FALSE;\
      pthread_create(&(thread), NULL, display_media_time, (recorder));\
    }\
  }while(0)

#define STOP_MEDIATIME_INFO_THREAD(thread)\
  do{\
    if((thread && exit_thread == RE_BOOLEAN_FALSE)) {\
      exit_thread = RE_BOOLEAN_TRUE;\
      pthread_join ((thread), NULL);\
      (thread)=NULL;\
    }\
  }while(0)


#define START_SHOW_MEDIATIME_INFO \
  do{\
    bstartmediatime = RE_BOOLEAN_TRUE;\
  }while(0) 

#define STOP_SHOW_MEDIATIME_INFO \
  do{\
    bstartmediatime = RE_BOOLEAN_FALSE;\
  }while(0) 

#define START_MESSAGE_PROCESS_THREAD(thread, recorder)\
  do{\
    if (thread == NULL){\
      exit_thread = RE_BOOLEAN_FALSE;\
      pthread_create(&(thread), NULL, process_message, (recorder));\
    }\
  }while(0)

#define STOP_MESSAGE_PROCESS_THREAD(thread)\
  do{\
    if((thread && exit_thread == RE_BOOLEAN_FALSE)) {\
      exit_thread = RE_BOOLEAN_TRUE;\
      sem_post(&grecordersem);\
      pthread_join ((thread), NULL);\
      (thread)=NULL;\
    }\
  }while(0)

#define RECORDER_START \
  do{\
    post_message (MESSAGE_START);\
  }while(0) 

#define RECORDER_STOP \
  do{\
    post_message (MESSAGE_STOP);\
  }while(0) 

#define RECORDER_PAUSE \
  do{\
    post_message (MESSAGE_PAUSE);\
  }while(0) 

#define RECORDER_RESUME \
  do{\
    post_message (MESSAGE_RESUME);\
  }while(0) 

#define RECORDER_TAKE_SNAPSHOT \
  do{\
    post_message (MESSAGE_SNAPSHOT);\
  }while(0) 

#define RECORDER_RESET \
  do{\
    post_message (MESSAGE_RESET);\
  }while(0) 

typedef enum{
    MESSAGE_NULL,
    MESSAGE_START,
    MESSAGE_STOP,
    MESSAGE_PAUSE,
    MESSAGE_RESUME,
    MESSAGE_SNAPSHOT,
    MESSAGE_RESET
}RecorderMessage;

typedef struct {
  REuint32 list;
  REuint32 audio_source;
  REuint32 sample_rate;
  REuint32 channel;
  REuint32 video_source;
  REuint32 camera_id;
  REuint32 video_format;
  REuint32 width;
  REuint32 height;
  REuint32 fps;
  REboolean add_time_stamp;
  REuint32 video_effect;
  REuint32 video_detect;
  REuint32 preview_left;
  REuint32 preview_top;
  REuint32 preview_width;
  REuint32 preview_height;
  REuint32 disable_viewfinder;
  REuint32 preview_buffer;
  REuint32 audio_encoder;
  REuint32 audio_bitrate;
  REuint32 video_encoder;
  REuint32 video_bitrate;
  REuint32 container_format;
  REchar path[1024];
  REchar host[32];
  REuint32 port;
  REuint32 file_count;
  REuint32 duration;
  REuint64 file_size;
  REuint32 aging;
  REuint32 verbose;
  REboolean use_default_filename;
}REOptions;

static pthread_t media_time_thread = NULL;
static pthread_t message_process_thread = NULL;
static REboolean exit_thread = RE_BOOLEAN_FALSE;
static REboolean bstartmediatime = RE_BOOLEAN_FALSE;
static REchar path[1024];
static REboolean bAgingtest = RE_BOOLEAN_FALSE;
static sem_t grecordersem;
static RecorderMessage latest_message = MESSAGE_NULL;

static volatile sig_atomic_t quit_flag = 0;

static void signal_handler(int signum)
{
  quit_flag = 1;
}

static REuint64 get_storage_free_size (char *dirname)
{
  struct statvfs64 fsdata;
  REuint64 free_size;

  if (statvfs64(dirname, &fsdata) < 0) {
    LOG_ERROR ("get storage free size fail.\n");
    return 0;
  }

  free_size = fsdata.f_bfree * fsdata.f_bsize;
  return free_size;
}

static void monitor_storage_free_size (RecorderEngine* recorder)
{
  if (get_storage_free_size (path) < LATEST_REMAIN_SPACE_SIZE) {
    LOG_INFO ("storage free space is less then %d. stop recording", \
        LATEST_REMAIN_SPACE_SIZE);
    STOP_SHOW_MEDIATIME_INFO;
    RECORDER_STOP;
  }
}

static void display_media_time (void* param)
{
  RecorderEngine* recorder = 	(RecorderEngine*)param;
  REtime sCur;
  REuint32 Hours;
  REuint32 Minutes;
  REuint32 Seconds;

  while(exit_thread == RE_BOOLEAN_FALSE) 
  {
    if (bstartmediatime)
    {
      sCur = 0;
      if(RE_RESULT_SUCCESS == recorder->get_media_time ((RecorderEngineHandle)recorder, &sCur))
      {
        Hours = (sCur/1000000) / 3600;
        Minutes = (sCur/ (60*1000000)) % 60;
        Seconds = ((sCur %(3600*1000000)) % (60*1000000))/1000000;
        printf("\r[Current Media Time] %03d:%02d:%02d", 
            Hours, Minutes, Seconds);
        fflush(stdout);
      }

      monitor_storage_free_size (recorder);

      usleep (500000);
    }
    else
      usleep (50000);
  }

  return;
}

static int set_recoder_setting (RecorderEngine *recorder, REOptions * pOpt)
{
  REuint64 free_size;

  /* Audio source interface */
  if (RE_RESULT_SUCCESS != recorder->set_audio_source (
        (RecorderEngineHandle)recorder, pOpt->audio_source)) {
    LOG_ERROR ("set audio source fail.\n");
    return -1;
  }
  if (RE_RESULT_SUCCESS != recorder->set_audio_sample_rate (
        (RecorderEngineHandle)recorder, pOpt->sample_rate)) {
    LOG_ERROR ("set audio sample rate fail.\n");
    return -1;
  }
  if (RE_RESULT_SUCCESS != recorder->set_audio_channel (
        (RecorderEngineHandle)recorder, pOpt->channel)) {
    LOG_ERROR ("set audio channel fail.\n");
    return -1;
  }

  /* Camera interface */
  if (RE_RESULT_SUCCESS != recorder->set_video_source (
        (RecorderEngineHandle)recorder, pOpt->video_source)) {
    LOG_ERROR ("set video source fail.\n");
    return -1;
  }
  if (RE_RESULT_SUCCESS != recorder->set_camera_id (
        (RecorderEngineHandle)recorder, pOpt->camera_id)) {
    LOG_ERROR ("set video source fail.\n");
    return -1;
  }
  {
    RERawVideoSettings video_property;
    video_property.videoFormat = pOpt->video_format;
    video_property.width = pOpt->width;
    video_property.height = pOpt->height;
    video_property.framesPerSecond = pOpt->fps;

    if (RE_RESULT_SUCCESS != recorder->set_camera_output_settings (
          (RecorderEngineHandle)recorder, &video_property)) {
      LOG_ERROR ("set video source fail.\n");
      return -1;
    }
  }

  /* View finder interface */
  recorder->disable_viewfinder ((RecorderEngineHandle)recorder, pOpt->disable_viewfinder);
  {
    REVideoRect rect;
    rect.left = pOpt->preview_left;
    rect.top = pOpt->preview_top;
    rect.width = pOpt->preview_width;
    rect.height = pOpt->preview_height;

    if (RE_RESULT_SUCCESS != recorder->set_preview_region (
          (RecorderEngineHandle)recorder, &rect)) {
      LOG_ERROR ("set video source fail.\n");
      return -1;
    }
  }

  /* Preview buffer after capture */
  if (RE_RESULT_SUCCESS != recorder->need_preview_buffer (
        (RecorderEngineHandle)recorder, pOpt->preview_buffer)) {
    LOG_ERROR ("set video source fail.\n");
    return -1;
  }

  /* Video time stamp and video effect */
  recorder->add_time_stamp ((RecorderEngineHandle)recorder, pOpt->add_time_stamp);
  recorder->add_video_effect ((RecorderEngineHandle)recorder, pOpt->video_effect);
#ifdef SUPPORT_VIDEO_DETECT
  recorder->add_video_detect ((RecorderEngineHandle)recorder, pOpt->video_detect);
#endif

  /* Audio encoder interface */
  {
    REAudioEncoderSettings audio_encoder;
    audio_encoder.encoderType = pOpt->audio_encoder;
    audio_encoder.bitRate = pOpt->audio_bitrate;

    if (RE_RESULT_SUCCESS != recorder->set_audio_encoder_settings (
          (RecorderEngineHandle)recorder, &audio_encoder)) {
      LOG_ERROR ("set video source fail.\n");
      return -1;
    }
  }

  /* Video encoder interface */
  {
    REVideoEncoderSettings video_encoder;
    video_encoder.encoderType = pOpt->video_encoder;
    video_encoder.bitRate = pOpt->video_bitrate;
    video_encoder.IFrameIntervalMs = 0;
    video_encoder.profile = 0;
    video_encoder.level = 0;

    if (RE_RESULT_SUCCESS != recorder->set_video_encoder_settings (
          (RecorderEngineHandle)recorder, &video_encoder)) {
      LOG_ERROR ("set video source fail.\n");
      return -1;
    }
  }

  /* Recorded output interface */
  if (RE_RESULT_SUCCESS != recorder->set_container_format (
        (RecorderEngineHandle)recorder, pOpt->container_format)) {
    LOG_ERROR ("set video source fail.\n");
    return -1;
  }

  if (pOpt->host[0])
    recorder->set_rtp_host ((RecorderEngineHandle)recorder, pOpt->host, pOpt->port);

  /* fileCount is 0 means unlimited */
  if (RE_RESULT_SUCCESS != recorder->set_file_count (
        (RecorderEngineHandle)recorder, pOpt->file_count)) {
    LOG_ERROR ("set video source fail.\n");
    return -1;
  }
  if (pOpt->duration) {
    if (RE_RESULT_SUCCESS != recorder->set_max_file_duration (
          (RecorderEngineHandle)recorder, pOpt->duration)) {
    LOG_ERROR ("set video source fail.\n");
    return -1;
    }
  }
  free_size = get_storage_free_size(path);
  if (pOpt->file_size) {
    if (RE_RESULT_SUCCESS != recorder->set_max_file_size_bytes (
          (RecorderEngineHandle)recorder,
          pOpt->file_size <= free_size ? pOpt->file_size:free_size)) {
      LOG_ERROR ("set video source fail.\n");
      return -1;
    }
  } else {
    if (RE_RESULT_SUCCESS != recorder->set_max_file_size_bytes (
          (RecorderEngineHandle)recorder, free_size)) {
      LOG_ERROR ("set video source fail.\n");
      return -1;
    }
  }

  return 0;
}

static int set_recoder_setting_snap_shot (RecorderEngine *recorder, REOptions * pOpt)
{
  REuint64 free_size;

  if (pOpt->use_default_filename == RE_BOOLEAN_TRUE) {
    strcpy(pOpt->path, "./grecorder_output.jpg");
  }
  free_size = get_storage_free_size(path);
  if (free_size < LATEST_REMAIN_SPACE_SIZE) {
    LOG_ERROR ("storage free size is less than: %d is: %lld\n", 
        LATEST_REMAIN_SPACE_SIZE, free_size);
    return -1;
  }
  if (RE_RESULT_SUCCESS != recorder->set_output_file_path (
        (RecorderEngineHandle)recorder, pOpt->path)) {
    LOG_ERROR ("set video source fail.\n");
    return -1;
  }

  {
    REOutputFileSettings file_setting;
    file_setting.interleaveMs = 0;
    file_setting.movieTimeScale = 0;
    file_setting.audioTimeScale = 0;
    file_setting.videoTimeScale = 0;
    file_setting.rotation = 0;
    file_setting.longitudex = 0;
    file_setting.latitudex = 0;

    if (RE_RESULT_SUCCESS != recorder->set_output_file_settings (
          (RecorderEngineHandle)recorder, &file_setting)) {
      LOG_ERROR ("set video source fail.\n");
      return -1;
    }
  }

  return 0;
}

static int set_recoder_setting_video (RecorderEngine *recorder, REOptions * pOpt)
{
  REuint64 free_size;

  if (pOpt->use_default_filename == RE_BOOLEAN_TRUE) {
    switch (pOpt->container_format) {
      case RE_OUTPUT_FORMAT_DEFAULT:
      case RE_OUTPUT_FORMAT_MOV:
        strcpy(pOpt->path, "./grecorder_output.mp4");
        break;
      case RE_OUTPUT_FORMAT_MKV:
        strcpy(pOpt->path, "./grecorder_output.mkv");
        break;
      case RE_OUTPUT_FORMAT_AVI:
        strcpy(pOpt->path, "./grecorder_output.avi");
        break;
      case RE_OUTPUT_FORMAT_FLV:
        strcpy(pOpt->path, "./grecorder_output.flv");
        break;
      case RE_OUTPUT_FORMAT_TS:
        strcpy(pOpt->path, "./grecorder_output.ts");
        break;
      default:
        strcpy(pOpt->path, "./grecorder_output.mp4");
        break;
    }
  }
  free_size = get_storage_free_size(path);
  if (free_size < LATEST_REMAIN_SPACE_SIZE) {
    LOG_ERROR ("storage free size is less than: %d is: %lld\n", 
        LATEST_REMAIN_SPACE_SIZE, free_size);
    return -1;
  }
  if (RE_RESULT_SUCCESS != recorder->set_output_file_path (
        (RecorderEngineHandle)recorder, pOpt->path)) {
    LOG_ERROR ("set video source fail.\n");
    return -1;
  }

  {
    REOutputFileSettings file_setting;
    file_setting.interleaveMs = 0;
    file_setting.movieTimeScale = 0;
    file_setting.audioTimeScale = 0;
    file_setting.videoTimeScale = 0;
    file_setting.rotation = 0;
    file_setting.longitudex = 0;
    file_setting.latitudex = 0;

    if (RE_RESULT_SUCCESS != recorder->set_output_file_settings (
          (RecorderEngineHandle)recorder, &file_setting)) {
      LOG_ERROR ("set video source fail.\n");
      return -1;
    }
  }

  return 0;
}

void post_message (RecorderMessage message)
{
  latest_message = message;
  sem_post(&grecordersem);
}

static void process_message (void* param)
{
  RecorderEngine* recorder = 	(RecorderEngine*)param;

  while(exit_thread == RE_BOOLEAN_FALSE) 
  {
    latest_message = MESSAGE_NULL;
    sem_wait(&grecordersem);

    LOG_INFO ("process_message: %d\n", latest_message);
    switch (latest_message) {
      case MESSAGE_START:
        if (RE_RESULT_SUCCESS == recorder->start((RecorderEngineHandle)recorder)) {
          START_SHOW_MEDIATIME_INFO;
          LOG_INFO ("start recording\n");
        } else 
          LOG_ERROR ("start recording failed\n");
        break;
      case MESSAGE_STOP:
        recorder->stop((RecorderEngineHandle)recorder);
        break;
      case MESSAGE_PAUSE:
        recorder->pause((RecorderEngineHandle)recorder);
        break;
      case MESSAGE_RESUME:
        recorder->resume((RecorderEngineHandle)recorder);
        break;
      case MESSAGE_SNAPSHOT:
        recorder->take_snapshot((RecorderEngineHandle)recorder);
        break;
      case MESSAGE_RESET:
        recorder->reset((RecorderEngineHandle)recorder);
        break;
      default:
        break;
    }
  }

  return;
}

static void list_camera_capabilities (RecorderEngine* recorder) 
{ 
  REresult ret; 
  RERawVideoSettings videoProperty;
  REuint32 index; 

  LOG_INFO ("\nCamera Capebilities:\n\n"); 

  for (index = 0; ; index ++) {
    ret = recorder->get_camera_capabilities((RecorderEngineHandle)recorder, 
        index, &videoProperty);
    if (ret == RE_RESULT_NO_MORE) {
      break;
    }

    LOG_INFO ("width: %d height: %d frame rate: %d\n", \
        videoProperty.width, videoProperty.height, \
        videoProperty.framesPerSecond);
  }
}

static int event_handler(void* context, REuint32 eventID, void* Eventpayload)
{
  RecorderEngine* recorder = (RecorderEngine*) context;
  switch(eventID) {
    case RE_EVENT_ERROR_UNKNOWN:
      LOG_ERROR ("error, post stop message.\n");
      STOP_SHOW_MEDIATIME_INFO;
      RECORDER_STOP;
      break;
    case RE_EVENT_PREVIEW_BUFFER:
      LOG_INFO ("received preview buffer.\n");
      break;
    case RE_EVENT_MAX_DURATION_REACHED:
    case RE_EVENT_MAX_FILESIZE_REACHED:
      LOG_ERROR ("reach max duration, post stop message.\n");
      STOP_SHOW_MEDIATIME_INFO;
      RECORDER_STOP;
      break;
    case RE_EVENT_OBJECT_POSITION: {
      REVideoRect *object_pos = (REVideoRect *) Eventpayload;
      LOG_INFO ("Object Detected. Position: [x:%d y:%d width: %d height: %d]\n",
          object_pos->left, object_pos->top,object_pos->width,object_pos->height);
      }
      break;
    default:
      break;
  }

  return 0;
}

static void recorder_main_menu()
{
  printf("\nSelect Command:\n");
  printf("\t[r]Start Record\n");
  printf("\t[n]Snapshot\n");
  printf("\t[t]Reset\n");
  printf("\t[s]Stop\n");
  printf("\t[x]Exit\n\n");
}

static int recorder_parse_options(int argc, char* argv[], REOptions * pOpt)
{
  int ret = 0;
  static int verbose;
  static int list;
  static int preview_buffer;
  static int disable_viewfinder;
  static int add_time_stamp;
  int option_index = 0;
  int c;

  while (1)
  {
    static char long_options_desc[][128] = {
      {"list camera supported video property"},
      {"audio input: 0->default(mic), 1->mic, 2->audiotestsrc"},
      {"audio sample rate"},
      {"audio channel"},
      {"video input: 0->default(camera), 1->camera, 2->videotestsrc"},
      {"camera id: 0->/dev/video0, 1->/dev/video1"},
      {"camera output video format: 0->default(I420), 1->I420, 2->NV12, 3->YUYV, 4->UYVY"},
      {"camera output video width"},
      {"camera output video height"},
      {"camera output video FPS"},
      {"add date/time onto video"},
      {"video effect: 0->default(no effect),1:cube,2:mirror,3:squeeze,4:fisheye,5:gray,6:tunnel,7:twirl"},
#ifdef SUPPORT_VIDEO_DETECT
      {"video detect: 0->default(no detect),1:face detect,2:faceblur"},
#endif
      {"preview video left"},
      {"preview video top"},
      {"preview video width"},
      {"preview video height"},
      {"disable view finder"},
      {"need preview buffer"},
      {"audio encoder type: 0->default(MP3), 1->MP3, 2->No Audio"},
      {"audio encoder bitrate(kbps)"},
      {"video encoder type: 0->default(H264), 1->H264, 2->MPEG4, 3->H263, 4->MPEG, 5->VP8"},
      {"video encoder bitrate(kbps)"},
      {"media container format: 0->default(MP4), 1->MP4, 2->MKV, 3->AVI, 4->FLV, 5->TS"},
      {"output path"},
      {"RTP streaming host IP address"},
      {"RTP streaming port"},
      {"recording file count(0 means unlimited)"},
      {"max duration for recorded file(second)"},
      {"max file size for recorded file(Byte)"},
      {"display application log"},
      {0, 0, 0, 0}
    };

    static struct option long_options[] =
    {
      {"ls", no_argument,       &list, 1},
      {"audio_source",  required_argument, 0, 'a'},
      {"sample_rate",  required_argument, 0, 's'},
      {"channel",    required_argument, 0, 'c'},
      {"video_source",    required_argument, 0, 'v'},
      {"camera_id",    required_argument, 0, 'i'},
      {"video_format",    required_argument, 0, 'u'},
      {"width",    required_argument, 0, 'w'},
      {"height",    required_argument, 0, 'e'},
      {"fps",    required_argument, 0, 'f'},
      {"date_time", no_argument,       &add_time_stamp, 1},
      {"video_effect",    required_argument, 0, 'q'},
#ifdef SUPPORT_VIDEO_DETECT
      {"video_detect",    required_argument, 0, 'x'},
#endif
      {"preview_left",    required_argument, 0, 'l'},
      {"preview_top",    required_argument, 0, 't'},
      {"preview_width",    required_argument, 0, 'b'},
      {"preview_height",    required_argument, 0, 'n'},
      {"disable_viewfinder", no_argument,       &disable_viewfinder, 1},
      {"preview_buffer", no_argument,       &preview_buffer, 1},
      {"audio_encoder",    required_argument, 0, 'g'},
      {"audio_bitrate",    required_argument, 0, 'j'},
      {"video_encoder",    required_argument, 0, 'k'},
      {"video_bitrate",    required_argument, 0, 'm'},
      {"container_format",    required_argument, 0, 't'},
      {"path",    required_argument, 0, 'p'},
      {"host",    required_argument, 0, 'o'},
      {"port",    required_argument, 0, 'r'},
      {"file_count",    required_argument, 0, 'n'},
      {"duration",    required_argument, 0, 'd'},
      {"file_size",    required_argument, 0, 'z'},
      {"verbose", no_argument,       &verbose, 1},
      {0, 0, 0, 0}
    };

    c = getopt_long (argc, argv, "a:s:w:e:u:f:k:t:q:i:v:n:z:o:r:x:g:",
        long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c)
    {
      case 0:
        /* If this option set a flag, do nothing else now. */
        if (long_options[option_index].flag != 0)
          break;
        printf ("option %s", long_options[option_index].name);
        if (optarg)
          printf (" with arg %s", optarg);
        printf ("\n");
        break;
      case 'a':
        if (optarg)
          pOpt->audio_source = atoi (optarg);
        break;
      case 's':
        if (optarg)
          pOpt->sample_rate = atoi (optarg);
        break;
      case 'w':
        if (optarg)
          pOpt->width = atoi (optarg);
        break;
      case 'e':
        if (optarg)
          pOpt->height = atoi (optarg);
        break;
      case 'u':
        if (optarg)
          pOpt->video_format = atoi (optarg);
        break;
      case 'f':
        if (optarg)
          pOpt->fps = atoi (optarg);
        break;
      case 'k':
        if (optarg)
          pOpt->video_encoder = atoi (optarg);
        break;
      case 't':
        if (optarg)
          pOpt->container_format = atoi (optarg);
        break;
      case 'q':
        if (optarg)
          pOpt->video_effect = atoi (optarg);
        break;
      case 'i':
        if (optarg)
          pOpt->camera_id = atoi (optarg);
        break;
      case 'v':
        if (optarg)
          pOpt->video_source = atoi (optarg);
        break;
      case 'n':
        if (optarg)
          pOpt->file_count = atoi (optarg);
        break;
      case 'z':
        if (optarg)
          pOpt->file_size = atoll (optarg);
        break;
      case 'o':
        if (optarg && strlen (optarg) < 32)
          strcpy (pOpt->host, optarg);
        break;
      case 'r':
        if (optarg)
          pOpt->port = atoi (optarg);
        break;
      case 'x':
        if (optarg)
          pOpt->video_detect = atoi (optarg);
        break;
      case 'g':
        if (optarg)
          pOpt->audio_encoder = atoi (optarg);
        break;
      case 'h':
        printf ("Usage: grecorder-1.0 [OPTION]\n");
        for (c = 0; long_options[c].name; ++c) {
          printf ("-%c, --%s\r\t\t\t\t%s\n", long_options[c].val,
              long_options[c].name, long_options_desc[c]);
        }
        ret = -1;
        break;
      case '?':
        /* getopt_long already printed an error message. */
        printf ("Use -h to see help message.\n");
        printf ("Usage: grecorder-1.0 [OPTION]\n");
        for (c = 0; long_options[c].name; ++c) {
          printf ("-%c, --%s\r\t\t\t\t%s\n", long_options[c].val,
              long_options[c].name, long_options_desc[c]);
        }
        ret = -1;
        break;
      default:
        break;
    }
  }
  pOpt->verbose = verbose;
  pOpt->list = list;
  pOpt->preview_buffer = preview_buffer;
  pOpt->disable_viewfinder = disable_viewfinder;
  pOpt->add_time_stamp = add_time_stamp;
  pOpt->use_default_filename = RE_BOOLEAN_FALSE;
  if (pOpt->path[0] == 0) {
    pOpt->use_default_filename = RE_BOOLEAN_TRUE;
    if (getcwd(path, sizeof(path)) == NULL) {
      LOG_ERROR ("get current path fail\n");
      return -1;
    }
  } else {
    strcpy(path, pOpt->path);
  }

  return ret;
}

int main(int argc, char* argv[])
{
  RecorderEngine* recorder = NULL;
  REboolean bexit = RE_BOOLEAN_FALSE;
  REboolean read_input = RE_BOOLEAN_TRUE;
  REboolean bPause = RE_BOOLEAN_FALSE;
  REOptions options;
  char rep[128];
  int ret = 0;

  struct sigaction act;
  act.sa_handler = signal_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  sigaction(SIGINT, &act, NULL);

  if (argc < 2) {
      g_print ("Use -h to get usage help.\n");
      return 0;
  }

  memset(&options, 0, sizeof(REOptions));
  if (recorder_parse_options(argc,argv,&options)){
    LOG_ERROR ("recorder_parse_options fail.\n");
    exit (1);
  }
  
  recorder = recorder_engine_create();
  if (recorder == NULL) {
    LOG_ERROR ("RecorderEngineCreate fail.\n");
    exit (1);
  }

  LOG_INFO ("create recorder successfully\n");

  recorder->init((RecorderEngineHandle)recorder);
  recorder->register_event_handler((RecorderEngineHandle)recorder, recorder,
      event_handler);

  if (set_recoder_setting (recorder, &options)) {
    LOG_ERROR ("set_recoder_setting fail.\n");
    goto bail;
  }

  if (recorder->prepare((RecorderEngineHandle)recorder)) {
    LOG_ERROR ("prepare fail.\n");
    goto bail;
  }

  if (options.list) {
    list_camera_capabilities(recorder);
    goto bail;
  }

  START_MEDIATIME_INFO_THREAD(media_time_thread, recorder);
  sem_init(&grecordersem, 0, 0);
  START_MESSAGE_PROCESS_THREAD(message_process_thread, recorder);

  while(bexit == RE_BOOLEAN_FALSE) {
    {
      if (read_input){
        recorder_main_menu();
        scanf("%128s", rep);
      }
      read_input=RE_BOOLEAN_TRUE;
      if (quit_flag) {
        LOG_INFO ("receive Ctrl+c user input.\n");
        STOP_SHOW_MEDIATIME_INFO;
        RECORDER_STOP;
        bexit = RE_BOOLEAN_TRUE;
      }
      if(rep[0] == 'r') {
        if (set_recoder_setting_video (recorder, &options)) {
          LOG_ERROR ("set_recoder_setting_video fail.\n");
          continue;
        }
        RECORDER_START;
      }
      else if(rep[0] == 'a') {
        if(bPause != RE_BOOLEAN_TRUE) {
          RECORDER_PAUSE;
          bPause = RE_BOOLEAN_TRUE;
        }
        else {
          RECORDER_RESUME;
          bPause = RE_BOOLEAN_FALSE;
        }
      }
      else if(rep[0] == 's') {
        STOP_SHOW_MEDIATIME_INFO;
        RECORDER_STOP;
      }
      else if(rep[0] == 'n') {
        if (set_recoder_setting_snap_shot (recorder, &options)) {
          LOG_ERROR ("set_recoder_setting_snap_shot fail\n");
          continue;
        }
        RECORDER_TAKE_SNAPSHOT;
      }
      else if(rep[0] == 't')
      {
        STOP_SHOW_MEDIATIME_INFO;
        RECORDER_RESET;
      }
      else if(rep[0] == 'x')
      {
        STOP_SHOW_MEDIATIME_INFO;
        RECORDER_STOP;
        bexit = RE_BOOLEAN_TRUE;
      }
      else if(rep[0] == '*') {
        sleep(1);
      }
      else if(rep[0] == '#') {
        sleep(10);
      }
    }
  }

  STOP_MESSAGE_PROCESS_THREAD(message_process_thread);
  sem_destroy(&grecordersem);
  STOP_MEDIATIME_INFO_THREAD(media_time_thread);

bail:
  recorder->close((RecorderEngineHandle)recorder);        
  recorder->delete_it((RecorderEngineHandle)recorder);        

  return 0;
}


