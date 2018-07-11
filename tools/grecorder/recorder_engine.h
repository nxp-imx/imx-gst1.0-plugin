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

#ifndef _RECORDER_ENGINE_h_
#define _RECORDER_ENGINE_h_

#include "fsl_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

  /*****************************************************************/
  /* TYPES                                                         */
  /*****************************************************************/

/* remap common types to RE types for clarity */
typedef int8        REchar;    /* UTF-8 is to be used     */
typedef int8        REint8;    /* 8 bit signed integer    */
typedef uint8       REuint8;   /* 8 bit unsigned integer  */
typedef int16       REint16;   /* 16 bit signed integer   */
typedef uint16      REuint16;  /* 16 bit unsigned integer */
typedef int32       REint32;   /* 32 bit signed integer   */
typedef uint32      REuint32;  /* 32 bit unsigned integer */
typedef uint64      REuint64;  /* 64 bit unsigned integer */

typedef REuint32    REboolean;
typedef REuint32    REmillisecond;
typedef REuint32    REmicrosecond;
typedef REuint64    REtime;
typedef REuint32    REresult;

#define RE_BOOLEAN_FALSE                    ((REuint32) 0x00000000)
#define RE_BOOLEAN_TRUE                     ((REuint32) 0x00000001)

  /*****************************************************************/
  /* RESULT CODES                                                  */
  /*****************************************************************/

#define RE_RESULT_SUCCESS                   ((REuint32) 0x00000000)
#define RE_RESULT_PARAMETER_INVALID         ((REuint32) 0x00000001)
#define RE_RESULT_MEMORY_FAILURE            ((REuint32) 0x00000002)
#define RE_RESULT_IO_ERROR                  ((REuint32) 0x00000003)
#define RE_RESULT_WRONG_STATE               ((REuint32) 0x00000003)
#define RE_RESULT_CONTENT_CORRUPTED         ((REuint32) 0x00000004)
#define RE_RESULT_CONTENT_NOT_FOUND         ((REuint32) 0x00000005)
#define RE_RESULT_PERMISSION_DENIED         ((REuint32) 0x00000006)
#define RE_RESULT_FEATURE_UNSUPPORTED       ((REuint32) 0x00000007)
#define RE_RESULT_INTERNAL_ERROR            ((REuint32) 0x00000008)
#define RE_RESULT_UNKNOWN_ERROR             ((REuint32) 0x00000009)
#define RE_RESULT_NO_MORE                   ((REuint32) 0x0000000A)

#define RE_VIDEO_SOURCE_DEFAULT             ((REuint32) 0x00000000)
#define RE_VIDEO_SOURCE_CAMERA              ((REuint32) 0x00000001)
#define RE_VIDEO_SOURCE_TEST                ((REuint32) 0x00000002)
#define RE_VIDEO_SOURCE_SCREEN              ((REuint32) 0x00000003)
#define RE_VIDEO_SOURCE_LIST_END            ((REuint32) 0x00000004)

#define RE_COLORFORMAT_DEFAULT              ((REuint32) 0x00000000)
#define RE_COLORFORMAT_YUV420PLANAR         ((REuint32) 0x00000001)
#define RE_COLORFORMAT_YUV420SEMIPLANAR     ((REuint32) 0x00000002)
#define RE_COLORFORMAT_YUVY                 ((REuint32) 0x00000003)
#define RE_COLORFORMAT_UYVY                 ((REuint32) 0x00000004)
#define RE_COLORFORMAT_LIST_END             ((REuint32) 0x00000005)

#define RE_VIDEO_EFFECT_DEFAULT             ((REuint32) 0x00000000)
#define RE_VIDEO_EFFECT_CUBE                ((REuint32) 0x00000001)
#define RE_VIDEO_EFFECT_MIRROR              ((REuint32) 0x00000002)
#define RE_VIDEO_EFFECT_SQUEEZE             ((REuint32) 0x00000003)
#define RE_VIDEO_EFFECT_FISHEYE             ((REuint32) 0x00000004)
#define RE_VIDEO_EFFECT_GRAY                ((REuint32) 0x00000005)
#define RE_VIDEO_EFFECT_TUNNEL              ((REuint32) 0x00000006)
#define RE_VIDEO_EFFECT_TWIRL               ((REuint32) 0x00000007)
#define RE_VIDEO_EFFECT_LIST_END            ((REuint32) 0x00000008)

#define RE_VIDEO_DETECT_DEFAULT             ((REuint32) 0x00000000)
#define RE_VIDEO_DETECT_FACEDETECT          ((REuint32) 0x00000001)
#define RE_VIDEO_DETECT_FACEBLUR            ((REuint32) 0x00000002)
#define RE_VIDEO_DETECT_LIST_END            ((REuint32) 0x00000003)

#define RE_OUTPUT_FORMAT_DEFAULT            ((REuint32) 0x00000000)
#define RE_OUTPUT_FORMAT_MOV                ((REuint32) 0x00000001)
#define RE_OUTPUT_FORMAT_MKV                ((REuint32) 0x00000002)
#define RE_OUTPUT_FORMAT_AVI                ((REuint32) 0x00000003)
#define RE_OUTPUT_FORMAT_FLV                ((REuint32) 0x00000004)
#define RE_OUTPUT_FORMAT_TS                 ((REuint32) 0x00000005)
#define RE_OUTPUT_FORMAT_LIST_END           ((REuint32) 0x00000006)

#define RE_AUDIO_SOURCE_DEFAULT             ((REuint32) 0x00000000)
#define RE_AUDIO_SOURCE_MIC                 ((REuint32) 0x00000001)
#define RE_AUDIO_SOURCE_TEST                ((REuint32) 0x00000002)
#define RE_AUDIO_SOURCE_LIST_END            ((REuint32) 0x00000003)

#define RE_AUDIO_ENCODER_DEFAULT            ((REuint32) 0x00000000)
#define RE_AUDIO_ENCODER_MP3                ((REuint32) 0x00000001)
#define RE_AUDIO_ENCODER_NO_AUDIO           ((REuint32) 0x00000002)
#define RE_AUDIO_ENCODER_LIST_END           ((REuint32) 0x00000003)

#define RE_VIDEO_ENCODER_DEFAULT            ((REuint32) 0x00000000)
#define RE_VIDEO_ENCODER_H264               ((REuint32) 0x00000001)
#define RE_VIDEO_ENCODER_MPEG4              ((REuint32) 0x00000002)
#define RE_VIDEO_ENCODER_H263               ((REuint32) 0x00000003)
#define RE_VIDEO_ENCODER_MJPEG              ((REuint32) 0x00000004)
#define RE_VIDEO_ENCODER_VP8                ((REuint32) 0x00000005)
#define RE_VIDEO_ENCODER_LIST_END           ((REuint32) 0x00000006)

#define RE_EVENT_NONE                       ((REuint32) 0x00000000)
#define RE_EVENT_ERROR_UNKNOWN              ((REuint32) 0x00000001)
#define RE_EVENT_PREVIEW_BUFFER             ((REuint32) 0x00000002)
#define RE_EVENT_MAX_DURATION_REACHED       ((REuint32) 0x00000003)
#define RE_EVENT_MAX_FILESIZE_REACHED       ((REuint32) 0x00000004)
#define RE_EVENT_MAX_FILE_COUNT_REACHED     ((REuint32) 0x00000005)
#define RE_EVENT_COMPLETION_STATUS          ((REuint32) 0x00000006)
#define RE_EVENT_PROGRESS_FRAME_STATUS      ((REuint32) 0x00000007)
#define RE_EVENT_PROGRESS_TIME_STATUS       ((REuint32) 0x00000008)
#define RE_EVENT_EOS                        ((REuint32) 0x00000009)
#define RE_EVENT_OBJECT_POSITION            ((REuint32) 0x0000000A)
#define RE_EVENT_LIST_END                   ((REuint32) 0x0000000B)

typedef struct RERawVideoSettings_ {
  REuint32 videoFormat;
  REuint32 width;
  REuint32 height;
  REuint32 framesPerSecond;
} RERawVideoSettings;

typedef struct REVideoRect_ {
  REuint32 left;
  REuint32 top;
  REuint32 width;
  REuint32 height;
} REVideoRect;

typedef struct REAudioEncoderSettings_ {
  REuint32 encoderType;
  REuint32 bitRate;
  REuint32 reserved[8];
} REAudioEncoderSettings;

/* 0 value means user not care, use default value. */
typedef struct REVideoEncoderSettings_ {
  REuint32 encoderType;
  REuint32 bitRate;
  REmicrosecond IFrameIntervalMs;
  REuint32 profile;
  REuint32 level;
  REuint32 reserved[8];
} REVideoEncoderSettings;

typedef struct REOutputFileSettings_ {
  REmicrosecond interleaveMs;
  REuint32 movieTimeScale;
  REuint32 audioTimeScale;
  REuint32 videoTimeScale;
  REuint32 rotation;
  REuint64 longitudex;
  REuint64 latitudex;
  REuint32 reserved[8];
} REOutputFileSettings;

typedef void * RecorderEngineHandle;
typedef REresult (*RecorderEngineEventHandler)(void* context, REuint32 eventID, void* Eventpayload);

typedef struct RecorderEngine_
{
  /* Audio source interface */
  REresult (*set_audio_source)(RecorderEngineHandle handle, REuint32 as);
  REresult (*get_audio_supported_sample_rate)(RecorderEngineHandle handle, REuint32 index, REuint32 *sampleRate);
  REresult (*set_audio_sample_rate)(RecorderEngineHandle handle, REuint32 sampleRate);
  REresult (*get_audio_supported_channel)(RecorderEngineHandle handle, REuint32 index, REuint32 *channels);
  REresult (*set_audio_channel)(RecorderEngineHandle handle, REuint32 channels);

  /* Camera interface */
  REresult (*set_video_source)(RecorderEngineHandle handle, REuint32 vs);
  REresult (*set_camera_id)(RecorderEngineHandle handle, REuint32 cameraId);
  REresult (*get_camera_capabilities)(RecorderEngineHandle handle, REuint32 index, RERawVideoSettings *videoProperty);
  REresult (*set_camera_output_settings)(RecorderEngineHandle handle, RERawVideoSettings *videoProperty);

  /* View finder interface */
  REresult (*disable_viewfinder)(RecorderEngineHandle handle, REboolean bDisableViewfinder);
  REresult (*set_preview_region)(RecorderEngineHandle handle, REVideoRect *rect);
  REresult (*set_preview_win_id)(RecorderEngineHandle handle, void *wid);

  /* Preview buffer after capture */
  REresult (*need_preview_buffer)(RecorderEngineHandle handle, REboolean bNeedPreviewBuffer);
  REresult (*get_preview_buffer_format)(RecorderEngineHandle handle, RERawVideoSettings *videoProperty);

  /* Video time stamp and video effect */
  REresult (*add_time_stamp)(RecorderEngineHandle handle, REboolean bAddTimeStamp);
  REresult (*add_video_effect)(RecorderEngineHandle handle, REuint32 videoEffect);
  REresult (*add_video_detect)(RecorderEngineHandle handle, REuint32 videoDetect);

  /* Audio encoder interface */
  REresult (*set_audio_encoder_settings)(RecorderEngineHandle handle, REAudioEncoderSettings *audioEncoderSettings);

  /* Video encoder interface */
  REresult (*set_video_encoder_settings)(RecorderEngineHandle handle, REVideoEncoderSettings *videoEncoderSettings);

  /* Recorded output interface */
  REresult (*set_container_format)(RecorderEngineHandle handle, REuint32 of);
  REresult (*set_output_file_path)(RecorderEngineHandle handle, const REchar *path);
  REresult (*set_rtp_host)(RecorderEngineHandle handle, const REchar *host, REuint32 port);
  /* fileCount is 0 means unlimited */
  REresult (*set_file_count)(RecorderEngineHandle handle, REuint32 fileCount);
  REresult (*set_max_file_duration)(RecorderEngineHandle handle, REtime timeUs);
  REresult (*set_max_file_size_bytes)(RecorderEngineHandle handle, REuint64 bytes);
  REresult (*set_output_file_settings)(RecorderEngineHandle handle, REOutputFileSettings *outputFileSettings);

  /* Snapshot interface */
  REresult (*set_snapshot_output_format)(RecorderEngineHandle handle, REuint32 of);
  REresult (*set_snapshot_output_file)(RecorderEngineHandle handle, const REchar *path);
  REresult (*take_snapshot)(RecorderEngineHandle handle);

  /* Recorder engine interface */
  REresult (*init)(RecorderEngineHandle handle);
  REresult (*register_event_handler)(RecorderEngineHandle handle, void * context, RecorderEngineEventHandler handler);
  REresult (*prepare)(RecorderEngineHandle handle);
  REresult (*start)(RecorderEngineHandle handle);
  REresult (*pause)(RecorderEngineHandle handle);
  REresult (*resume)(RecorderEngineHandle handle);
  REresult (*stop)(RecorderEngineHandle handle);
  REresult (*close)(RecorderEngineHandle handle);
  REresult (*reset)(RecorderEngineHandle handle);
  REresult (*get_max_amplitude)(RecorderEngineHandle handle, REuint32 *max);
  REresult (*get_media_time)(RecorderEngineHandle handle, REtime *pMediaTimeUs);
  REresult (*delete_it)(RecorderEngineHandle handle);

  void * pData;
}RecorderEngine;

RecorderEngine * recorder_engine_create();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
