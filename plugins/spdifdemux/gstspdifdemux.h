/* GStreamer
 * Copyright 2020-2021 NXP
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifndef __GST_SPDIFDEMUX_H__
#define __GST_SPDIFDEMUX_H__


#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/audio/gstaudioringbuffer.h>

G_BEGIN_DECLS
#define GST_TYPE_SPDIFDEMUX \
  (gst_spdifdemux_get_type())
#define GST_SPDIFDEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SPDIFDEMUX,GstSpdifDemux))
#define GST_SPDIFDEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SPDIFDEMUX,GstSpdifDemuxClass))
#define GST_IS_SPDIFDEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SPDIFDEMUX))
#define GST_IS_SPDIFDEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SPDIFDEMUX))

typedef enum
{
  GST_SPDIFDEMUX_HEADER,
  GST_SPDIFDEMUX_DATA
} GstSpdifDemuxState;

typedef struct _GstSpdifDemux GstSpdifDemux;
typedef struct _GstSpdifDemuxClass GstSpdifDemuxClass;

typedef struct
{
  spdif_parser_get_version_info_t spdif_parser_get_version_info;
  spdif_parser_open_t spdif_parser_open;
  spdif_parser_close_t spdif_parser_close;
  spdif_parser_set_mode_t spdif_parser_set_mode;
  spdif_parser_set_iec958_type_t spdif_parser_set_iec958_type;
  spdif_parser_get_mode_t spdif_parser_get_mode;
  spdif_parser_search_header_t spdif_parser_search_header;
  spdif_parser_get_compress_audio_frame_size_t
  spdif_parser_get_compress_audio_frame_size;
  spdif_parser_get_compress_audio_len_t spdif_parser_get_compress_audio_len;
  spdif_parser_read_t spdif_parser_read;
  spdif_parser_read_with_sync_t spdif_parser_read_with_sync;
  spdif_parser_get_audio_info_t spdif_parser_get_audio_info;
  spdif_parser_get_audio_type_t spdif_parser_get_audio_type;
  spdif_parser_get_iec937_type_t spdif_parser_get_iec937_type;
  spdif_parser_get_sample_rate_t spdif_parser_get_sample_rate;
  spdif_parser_get_channel_num_t spdif_parser_get_channel_num;
  spdif_parser_get_data_length_t spdif_parser_get_data_length;
} spdif_parser_if_t;

typedef enum
{
  IEC958_FORMAT_UNKNOWN,
  IEC958_FORMAT_PCM,
  IEC958_FORMAT_IEC937
} IEC958_FORMAT;

typedef struct
{
  gboolean flag;
  guint32 buf_count;
  guint64 data_len;
  GstClockTime start_ts;
  GstClockTime end_ts;
} BpsCalcInfo;

/**
 * GstSpdifDemux:
 *
 * Opaque data structure.
 */
struct _GstSpdifDemux
{
  GstElement parent;
  GstPad *sinkpad, *srcpad;

  /* for delayed source pad creation for when
   * we have the first chunk of data and know
   * the format for sure */
  GstCaps *caps;
  GstEvent *start_segment;

  /* decoding state */
  GstSpdifDemuxState state;
  gboolean abort_buffering;

  /* real bps used or 0 when no bitrate is known */
  guint32 bps;
  /* position in data part */
  guint64 offset;
  
  /* valid audio data offset */
  guint64 audio_offset;

  /* For streaming */
  GstAdapter *adapter;
  gboolean got_fmt;
  gboolean streaming;

  /* configured segment, start/stop expressed in time or bytes */
  GstSegment segment;

  /* for late pad configuration */
  gboolean first;
  /* discont after seek */
  gboolean discont;

  /* data structure for spdif parser library 
   * this library can parse iec937 or iec958 frame automatically
   * this library can also detect iec958 PCM or compress audio automatically 
   * User can also configure the specify frame type */
  spdif_parser_if_t *spdif_parser_if;
  spdif_audio_info_t spdif_audio_info;
  void *dl_handle;
  const char *spdif_parser_id;
  spdif_parser_handle handle;
  /* iec958 audio format selection if know */
  IEC958_FORMAT iec958_format;
  
  /* data structure for calculating bps */
  BpsCalcInfo fs_calc_param;

  /* latency control definition */
  /* clock running time when first buffer arrives */
  GstClockTime clock_offset;
  /* first buffer timestamp */
  GstClockTime start_time;
  GstClockTime pipeline_latency;
  GstClockTime running_time;
};

struct _GstSpdifDemuxClass
{
  GstElementClass parent_class;
};

GType gst_spdifdemux_get_type (void);

G_END_DECLS
#endif /* __GST_SPDIFDEMUX_H__ */
