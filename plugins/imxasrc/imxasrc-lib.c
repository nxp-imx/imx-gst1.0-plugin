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

#include <gst/gst.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sound/compress_offload.h>
#include "imxasrc-lib.h"

/* maximum buffer time in ring buffer is 200ms */
#define MAX_RING_BUFFER_TIME 200
/* silence time in ring buffer is 20ms */
#define SILENCE_RING_BUFFER_TIME 20
/* 20 samples size 8ch/32bit */
#define TAIL_SIZE (20 * 8 * 32)

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static size_t cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    size_t cat_done;

    cat_done = (size_t) _gst_debug_category_new ("imxasrc_lib", 0,
        "imxasrc_lib object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

int imx_asrc_open(ASRCConfig *asrc)
{
  char path[64];
  int i;

  for (i = 0; i < 10; i++) {
    memset(path, 0, 64);
    sprintf(path, "/dev/snd/comprC%uD0", i);
    if (access(path, F_OK) == 0)
      break;
  }
  if (i == 10) {
    GST_ERROR ("no asrc sound card found\n");
    return -1;
  }

  asrc->fd = open(path, O_RDWR);

  if (asrc->fd < 0)
  {
    GST_ERROR ("failed to open ASRC");
    return -1;
  }

  return 0;
}

void imx_asrc_close(ASRCConfig *asrc)
{
  close(asrc->fd);

  ring_buffer_destroy(&asrc->ring_buffer);

  return;
}

/* Request one available ASRC CONTEXT/PAIR then configure it */
int imx_asrc_config(ASRCConfig *asrc)
{
  struct snd_compr_codec_caps codec_caps = {};
  struct snd_compr_caps caps;
  struct snd_compr_params params;
  int i, j;
  int err = 0;
  int block_size;
  int num_blocks;
  uint8_t *silence;
  size_t silence_size;

  if (!asrc)
  {
    GST_ERROR ("asrc is invalid");
    return -1;
  }

  if ((err = ioctl(asrc->fd, SNDRV_COMPRESS_GET_CAPS, &caps)) < 0) {
    GST_ERROR("get caps FAILED: %d\n", err);
    return err;
  }

  codec_caps.codec = SND_AUDIOCODEC_PCM;
  if ((err = ioctl(asrc->fd, SNDRV_COMPRESS_GET_CODEC_CAPS, &codec_caps)) < 0) {
    GST_ERROR ("get codec caps FAILED: %d\n", err);
    return err;
  }

  for (i = 0; i < codec_caps.num_descriptors; i++) {
    if (codec_caps.descriptor[i].formats != asrc->audio_info.input_format)
      continue;

    for (j = 0; j < codec_caps.descriptor[i].num_sample_rates; j++) {
      if (codec_caps.descriptor[i].sample_rates[j] == asrc->audio_info.input_sample_rate)
        break;
    }

    if (j == codec_caps.descriptor[i].num_sample_rates)
      continue;

    if (asrc->audio_info.output_sample_rate >= codec_caps.descriptor[i].src.out_sample_rate_min &&
        asrc->audio_info.output_sample_rate <= codec_caps.descriptor[i].src.out_sample_rate_max)
      break;
  }

  if (i == codec_caps.num_descriptors) {
    GST_ERROR ("caps don't support\n");
    return err;
  }

  params.buffer.fragment_size = 4096;
  params.buffer.fragments = 1;
  params.codec.id = SND_AUDIOCODEC_PCM;
  params.codec.ch_in  = asrc->audio_info.channels;
  params.codec.ch_out = asrc->audio_info.channels;
  params.codec.format = asrc->audio_info.input_format;
  params.codec.sample_rate = asrc->audio_info.input_sample_rate;
  params.codec.pcm_format = asrc->audio_info.output_format;
  params.codec.options.src_d.out_sample_rate = asrc->audio_info.output_sample_rate;
  if ((err = ioctl(asrc->fd, SNDRV_COMPRESS_SET_PARAMS, &params)) < 0) {
    GST_ERROR("set params FAILED\n");
    return err;
  }

  if ((err = ioctl(asrc->fd, SNDRV_COMPRESS_TASK_CREATE, &asrc->task)) < 0) {
    GST_ERROR("task create FAILED %d\n", err);
    return err;
  }

  asrc->status.seqno = asrc->task.seqno;

  asrc->bufin_start = mmap(NULL,
                           512 * 1024, /* set by the driver */
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           asrc->task.input_fd,
                           0);
  if (asrc->bufin_start == MAP_FAILED) {
    GST_ERROR ("MMAP IN err\n");
    return err;
  }
  /* empty capture buffer */
  memset(asrc->bufin_start, 0, 512 * 1024);
  asrc->bufout_start = mmap(NULL,
                           512 * 1024, /* set by the driver */
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           asrc->task.output_fd,
                           0);
  if (asrc->bufout_start == MAP_FAILED) {
    GST_ERROR("MMAP OUT err\n");
    return err;
  }
  /* empty capture buffer */
  memset(asrc->bufout_start, 0, 512 * 1024);

  /* create ring buffer and feed silence data */
  block_size = asrc->out_bps * asrc->audio_info.channels;
  num_blocks = asrc->audio_info.output_sample_rate * MAX_RING_BUFFER_TIME / 1000 * block_size;

  err = ring_buffer_create(&asrc->ring_buffer, block_size, num_blocks);
  if (err < 0) {
    GST_ERROR ("ring_buffer_create failed");
    return err;
  }

  silence_size = asrc->audio_info.output_sample_rate * SILENCE_RING_BUFFER_TIME / 1000 * block_size;
  silence = malloc(silence_size);
  memset(silence, 0, silence_size);
  ring_buffer_put(&asrc->ring_buffer, silence_size / block_size, silence);

  free(silence);

  return err;
}

int imx_asrc_start(ASRCConfig *asrc)
{
  int err = 0;

  return err;
}

size_t imx_asrc_get_out_frames(ASRCConfig *asrc_config, size_t in_frames)
{
  return ring_buffer_avail(&asrc_config->ring_buffer);
}

int imx_asrc_resample(ASRCConfig *asrc, gpointer in[],
    size_t in_frames, gpointer out[], size_t out_frames)
{
  size_t in_chunk, out_chunk;
  uint8_t *in_p = in[0];
  uint8_t *out_p = out[0];
  int err;
  int num_blocks_in;

  out_chunk = imx_asrc_get_out_frames(asrc, in_frames);
  if (out_frames < out_chunk) {
    GST_ERROR ("no enough frames to get");
    return -1;
  }

  ring_buffer_get(&asrc->ring_buffer, out_frames, out_p);

  in_chunk = in_frames * asrc->in_bps * asrc->audio_info.channels;
  memcpy(asrc->bufin_start, in_p, in_chunk);
  asrc->task.input_size = in_chunk;

  if ((err = ioctl(asrc->fd, SNDRV_COMPRESS_TASK_START, &asrc->task)) < 0) {
      GST_ERROR ("task start FAILED\n");
      return err;
  }

  if ((err = ioctl(asrc->fd, SNDRV_COMPRESS_TASK_STOP, &asrc->task.seqno)) < 0) {
    GST_ERROR ("task stop FAILED\n");
    return err;
  }

  if ((err = ioctl(asrc->fd, SNDRV_COMPRESS_TASK_STATUS, &asrc->status)) < 0) {
    GST_ERROR ("task status FAILED\n");
    return err;
  }

  num_blocks_in = asrc->status.output_size / asrc->ring_buffer.block_size;
  ring_buffer_put(&asrc->ring_buffer, num_blocks_in, asrc->bufout_start);
  GST_DEBUG("asrc convert: in %ld frames, out %d frames\n", in_frames, num_blocks_in);

  return err;
}
