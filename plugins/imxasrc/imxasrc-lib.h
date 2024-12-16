/* GStreamer
 * Copyright 2024 NXP
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef _IMXASRC_LIB_H
#define _IMXASRC_LIB_H

#include <sound/compress_offload.h>
#include "imxasrc-utils.h"

typedef struct _ASRCConfig ASRCConfig;
typedef struct _ASRCAudioInfo ASRCAudioInfo;

struct _ASRCAudioInfo {
  int channels;
  int input_sample_rate;
  int output_sample_rate;
  snd_pcm_format_t input_format;
  snd_pcm_format_t output_format;
};

struct _ASRCConfig {
  int fd;

  ASRCAudioInfo audio_info;
  int in_bps;
  int out_bps;

  int inclk;
  int outclk;
  int input_dma_size;
  int output_dma_size;
  int pair_index;
  int supported_in_format;
  int supported_out_format;

  void *bufin_start;
  void *bufout_start;
  struct snd_compr_task task;
  struct snd_compr_task_status status;

  RingBuffer ring_buffer;
};

int imx_asrc_open(ASRCConfig *asrc);
void imx_asrc_close(ASRCConfig *asrc);
int imx_asrc_start(ASRCConfig *asrc);
int imx_asrc_config(ASRCConfig *asrc);
int imx_asrc_resample(ASRCConfig *asrc_config, gpointer in[],
                      size_t in_bytes, gpointer out[], size_t out_bytes);
size_t imx_asrc_get_out_len(ASRCConfig *asrc_config, size_t in_frames);
size_t imx_asrc_get_out_frames(ASRCConfig *asrc_config, size_t in_frames);

#endif
