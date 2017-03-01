/*
 * Copyright (c) 2010-2013, Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Module Name:    aiurdemux.h
 *
 * Description:    Head file of unified parser gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */


#ifndef __AIURDEMUX_H__
#define __AIURDEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/tag/tag.h>

//#include "mfw_gst_utils.h"

#include "fsl_parser.h"

#include "aiurregistry.h"
#include "aiurstreamcache.h"
#include "aiuridxtab.h"
#include "aiurcontent.h"

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN (aiurdemux_debug);
#define GST_CAT_DEFAULT aiurdemux_debug


#define GST_TYPE_AIURDEMUX \
    (gst_aiurdemux_get_type())
#define GST_AIURDEMUX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AIURDEMUX,GstAiurDemux))
#define GST_AIURDEMUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AIURDEMUX,GstAiurDemuxClass))
#define GST_IS_AIURDEMUX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AIURDEMUX))
#define GST_IS_AIURDEMUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AIURDEMUX))
#define GST_AIURDEMUX_CAST(obj) ((GstAiurDemux *)(obj))


#define GST_AIURDEMUX_MAX_STREAMS   32
#define AIURDEMUX_INIT_BLOCK_SIZE   4
#define AIURDEMUX_INTERLEAVE_QUEUE_SIZE 10240000
#define AIURDEMUX_CODEC_DATA_MAX_LEN    10000000
#define AIURDEMUX_FRAME_N_DEFAULT 30
#define AIURDEMUX_FRAME_D_DEFAULT 1
#define AIURDEMUX_VIDEO_WIDTH_DEFAULT 176
#define AIURDEMUX_VIDEO_HEIGHT_DEFAULT 144
#define AIURDEMUX_AUDIO_CHANNEL_DEFAULT 2
#define AIURDEMUX_AUDIO_SAMPLERATE_DEFAULT 44100
#define AIURDEMUX_AUDIO_SAMPLEWIDTH_DEFAULT 16
#define AIURDEMUX_MIN_OUTPUT_BUFFER_SIZE 8
#define AIURDEMUX_TIMESTAMP_DISCONT_MAX_GAP   (10*GST_SECOND)
#define AIURDEMUX_TIMESTAMP_LAG_MAX_TIME      (10*GST_SECOND)

#define GST_BUFFER_TIMESTAMP GST_BUFFER_PTS

enum
{
  PROP_0,
  PROP_PROGRAM_NUMBER,
  PROP_MULTIPROGRAM_ENABLED,
  PROP_PROGRAM_MASK,
  PROP_INTERLEAVE_QUEUE_SIZE,
  PROP_STREAMING_LATENCY,
  PROP_INDEX_ENABLED,
  PROP_DISABLE_VORBIS_CODEC_DATA,
  PROP_LOW_LATENCY_TOLERANCE,
};


typedef enum
{
  AIUR_PLAY_MODE_NORMAL = 0,
  AIUR_PLAY_MODE_TRICK_FORWARD,
  AIUR_PLAY_MODE_TRICK_BACKWARD
} AiurDemuxPlayMode;

enum
{
  AIURDEMUX_STATE_PROBE = 0,        /* Wait for mime set and select right core */
  AIURDEMUX_STATE_INITIAL,      /* Initial state, initial core interfaces  */
  AIURDEMUX_STATE_HEADER,       /* Parsing the header */
  AIURDEMUX_STATE_MOVIE,        /* Parsing/Playing the media data */
};


typedef struct _GstAiurDemuxClass GstAiurDemuxClass;
typedef struct _AiurDemuxStream AiurDemuxStream;

typedef struct _AiurDemuxContentInfo AiurDemuxContentInfo;
typedef struct _AiurDemuxTimeStamp AiurDemuxTimeStamp;


typedef struct _AiurDemuxVideoInfo AiurDemuxVideoInfo;
typedef struct _AiurDemuxAudioInfo AiurDemuxAudioInfo;
typedef struct _AiurDemuxSubtitleInfo AiurDemuxSubtitleInfo;



typedef struct _GstAiurDemux GstAiurDemux;
typedef struct _AiurDemuxStream AiurDemuxStream;
typedef struct _AiurDemuxProgram AiurDemuxProgram;





typedef struct _AiurDemuxOption
{
  gboolean multiprogram_enabled;
  guint program_mask;
  gint program_number;
  guint interleave_queue_size;
  guint streaming_latency;
  guint8* caps;
  gboolean index_enabled;
  gboolean merge_h264_codec_data;
  gboolean disable_vorbis_codec_data;
  gint low_latency_tolerance;
} AiurDemuxOption;




typedef struct
{
  gint64 start;
  gint64 duration;
  uint32 flag;
} AiurSampleStat;








struct _AiurDemuxVideoInfo
{
    uint32 width;
    uint32 height;
    uint32 fps_n;
    uint32 fps_d;
};

struct _AiurDemuxAudioInfo
{
    uint32 rate;
    uint32 n_channels;
    uint32 sample_width;
    uint32 block_align;
};

struct _AiurDemuxSubtitleInfo
{
    uint32 width;
    uint32 height;
};

typedef struct
{
    uint8 *codec_data;
    uint32 length;
} AiurDemuxCodecData;

typedef struct
{
  guint32 id;
  gint32 pid;
  gchar lang[4];
} AiurDemuxProgramTrack;

struct _AiurDemuxProgram
{
  gboolean enabled;
  gint32 program_number;
  gint32 pid;
  uint32 track_num;
  AiurDemuxProgramTrack tracks[0];
};


struct _AiurDemuxStream
{
    GstCaps *caps;
    GstPad *pad;

    guint32 track_idx;
    gint32 pid;
    gint32 ppid;

    uint32 type;
    uint32 codec_type;
    uint32 codec_sub_type;

    guint64 track_duration;
    gchar lang[4];
    uint32 bitrate;

    guint32 mask;

    union
    {
        AiurDemuxVideoInfo video;
        AiurDemuxAudioInfo audio;
        AiurDemuxSubtitleInfo subtitle;
    } info;

    AiurDemuxCodecData codec_data;
    GstTagList *pending_tags;
    gboolean send_global_tags;

    GstBuffer *buffer;

    gboolean valid;
    gboolean sent_eos;
    gboolean bad_stream;
    gboolean block;

    gboolean pending_eos;
    gboolean new_segment;
    gboolean partial_sample;

    gboolean discont;
    gboolean pending_event;
    gboolean send_codec_data;
    gboolean merge_codec_data;

    GQueue *buf_queue;
    guint buf_queue_size;
    guint buf_queue_size_max;

    GstAdapter *adapter;
    uint32 adapter_buffer_size;

    AiurSampleStat sample_stat;

    guint64 time_position;
    gint64 last_stop;
    gint64 last_start;
    gint64 last_timestamp;
    gint64 lag_time;
    GstFlowReturn last_ret;

    

};


struct _GstAiurDemux
{
    GstElement element;
    GstPad *sinkpad;

    gint state;
    GMutex runmutex;
    gboolean loop_push;
    gboolean pullbased;
    gboolean pending_event;
    gboolean seekable;
    gboolean isMPEG;
    uint64 movie_duration;
    uint32     track_count; //temp number for track count
    guint32     n_streams;
    guint32     n_video_streams;
    guint32     n_audio_streams;
    guint32     n_sub_streams;
    guint32     sub_read_cnt;
    gboolean    sub_read_ready;
    AiurDemuxStream *streams[GST_AIURDEMUX_MAX_STREAMS];
    guint32 valid_mask;
    uint32     program_num;//temp number for programe count
    AiurDemuxProgram **programs;

    GstAiurStreamCache *stream_cache;
    AiurContent * content_info;

    
    /* core interface */
    AiurCoreInterface *core_interface;
    FslParserHandle core_handle;


    guint32 read_mode;
    AiurDemuxPlayMode play_mode;
    
    guint32 interleave_queue_size;

    GstSegment segment;
    GstTagList *tag_list;

    GstClockTime base_offset;
    GstClockTime clock_offset;//clock running time when first buffer arrives
    GstClockTime start_time;//first buffer timestamp
    GstClockTimeDiff media_offset;
    GstClockTimeDiff avg_diff;

    AiurDemuxOption option;

    GThread *thread;  // for push mode thread
    GMutex seekmutex;

};

struct _GstAiurDemuxClass
{
    GstElementClass parent_class;
};


GType gst_aiurdemux_get_type (void);

G_END_DECLS
#endif /* __AIURDEMUX_H__ */
