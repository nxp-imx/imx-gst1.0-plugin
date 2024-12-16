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

#include <alsa/asoundlib.h>
#include "imxasrc-utils.h"

int ring_buffer_create(RingBuffer *ringbuffer, int block_size, int num_blocks)
{
  int ret = -ENOMEM;

  ringbuffer->block_size = block_size;
  ringbuffer->num_blocks = num_blocks;
  ringbuffer->index_in = 0;
  ringbuffer->index_out = 0;
  ringbuffer->mem = malloc(block_size * num_blocks);
  if (ringbuffer->mem == NULL) {
    printf("Allocate ring buffer failed %d", ret);
    return ret;
  }

  return 0;
}

void ring_buffer_destroy(RingBuffer *ringbuffer)
{
  ringbuffer->block_size = 0;
  ringbuffer->num_blocks = 0;
  ringbuffer->index_in = 0;
  ringbuffer->index_out = 0;
  free(ringbuffer->mem);
}

int ring_buffer_avail(RingBuffer *ringbuffer)
{
  int count;

  count = ringbuffer->index_in - ringbuffer->index_out;
  if (count < 0) {
    count += ringbuffer->num_blocks;
  }

  return count;
}

int ring_buffer_get(RingBuffer *ringbuffer, int num_block_out, void *data)
{
  int ret = 0;
  int block_out1, block_out2;

  if (ringbuffer->num_blocks - ringbuffer->index_out < num_block_out) {
    block_out1 = ringbuffer->num_blocks - ringbuffer->index_out;
    block_out2 = num_block_out - block_out1;
  } else {
    block_out1 = num_block_out;
    block_out2 = 0;
  }

  memcpy(data, ringbuffer->mem + ringbuffer->index_out * ringbuffer->block_size,
         block_out1 * ringbuffer->block_size);
  ringbuffer->index_out += block_out1;
  if (ringbuffer->index_out >= ringbuffer->num_blocks)
    ringbuffer->index_out -= ringbuffer->num_blocks;

  if (block_out2) {
    memcpy(data + block_out1 * ringbuffer->block_size, ringbuffer->mem,
           block_out2 * ringbuffer->block_size);
    ringbuffer->index_out += block_out2;
    if (ringbuffer->index_out >= ringbuffer->num_blocks)
      ringbuffer->index_out -= ringbuffer->num_blocks;
  }

  return ret;
}

int ring_buffer_put(RingBuffer *ringbuffer, int num_block_in, void *data)
{
  int ret = 0;
  int block_in1, block_in2;

  if (ringbuffer->num_blocks - ringbuffer->index_in < num_block_in) {
    block_in1 = ringbuffer->num_blocks - ringbuffer->index_in;
    block_in2 = num_block_in - block_in1;
  } else {
    block_in1 = num_block_in;
    block_in2 = 0;
  }

  memcpy(ringbuffer->mem + ringbuffer->index_in * ringbuffer->block_size, data,
         block_in1 * ringbuffer->block_size);
  ringbuffer->index_in += block_in1;
  if (ringbuffer->index_in >= ringbuffer->num_blocks)
    ringbuffer->index_in -= ringbuffer->num_blocks;

  if (block_in2) {
    memcpy(ringbuffer->mem, data + block_in1 * ringbuffer->block_size,
           block_in2 * ringbuffer->block_size);
    ringbuffer->index_in += block_in2;
    if (ringbuffer->index_in >= ringbuffer->num_blocks)
      ringbuffer->index_in -= ringbuffer->num_blocks;
  }

  return ret;
}

snd_pcm_format_t get_alsa_pcm_format (GstAudioFormat fmt)
{
  switch (fmt) {
    case GST_AUDIO_FORMAT_S8:
      return SND_PCM_FORMAT_S8;
    case GST_AUDIO_FORMAT_U8:
      return SND_PCM_FORMAT_U8;
      /* 16 bit */
    case GST_AUDIO_FORMAT_S16LE:
      return SND_PCM_FORMAT_S16_LE;
    case GST_AUDIO_FORMAT_S16BE:
      return SND_PCM_FORMAT_S16_BE;
    case GST_AUDIO_FORMAT_U16LE:
      return SND_PCM_FORMAT_U16_LE;
    case GST_AUDIO_FORMAT_U16BE:
      return SND_PCM_FORMAT_U16_BE;
      /* 24 bit in low 3 bytes of 32 bits */
    case GST_AUDIO_FORMAT_S24_32LE:
      return SND_PCM_FORMAT_S24_LE;
    case GST_AUDIO_FORMAT_S24_32BE:
      return SND_PCM_FORMAT_S24_BE;
    case GST_AUDIO_FORMAT_U24_32LE:
      return SND_PCM_FORMAT_U24_LE;
    case GST_AUDIO_FORMAT_U24_32BE:
      return SND_PCM_FORMAT_U24_BE;
      /* 24 bit in 3 bytes */
    case GST_AUDIO_FORMAT_S24LE:
      return SND_PCM_FORMAT_S24_3LE;
    case GST_AUDIO_FORMAT_S24BE:
      return SND_PCM_FORMAT_S24_3BE;
    case GST_AUDIO_FORMAT_U24LE:
      return SND_PCM_FORMAT_U24_3LE;
    case GST_AUDIO_FORMAT_U24BE:
      return SND_PCM_FORMAT_U24_3BE;
      /* 32 bit */
    case GST_AUDIO_FORMAT_S32LE:
      return SND_PCM_FORMAT_S32_LE;
    case GST_AUDIO_FORMAT_S32BE:
      return SND_PCM_FORMAT_S32_BE;
    case GST_AUDIO_FORMAT_U32LE:
      return SND_PCM_FORMAT_U32_LE;
    case GST_AUDIO_FORMAT_U32BE:
      return SND_PCM_FORMAT_U32_BE;
    case GST_AUDIO_FORMAT_F32LE:
      return SND_PCM_FORMAT_FLOAT_LE;
    case GST_AUDIO_FORMAT_F32BE:
      return SND_PCM_FORMAT_FLOAT_BE;
    case GST_AUDIO_FORMAT_F64LE:
      return SND_PCM_FORMAT_FLOAT64_LE;
    case GST_AUDIO_FORMAT_F64BE:
      return SND_PCM_FORMAT_FLOAT64_BE;
    default:
      break;
  }
  return SND_PCM_FORMAT_UNKNOWN;
}
