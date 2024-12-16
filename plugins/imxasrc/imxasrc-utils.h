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

#ifndef _IMXASRC_UTILS_H
#define _IMXASRC_UTILS_H

#include <gst/audio/audio.h>
#include <stdint.h>

typedef struct ring_buffer_struct {
  int block_size;
  int num_blocks;
  int index_in;
  int index_out;
  uint8_t *mem;
} RingBuffer;

int ring_buffer_create(RingBuffer *ringbuffer, int block_size, int num_blocks);
void ring_buffer_destroy(RingBuffer *ringbuffer);
int ring_buffer_avail(RingBuffer *ringbuffer);
int ring_buffer_get(RingBuffer *ringbuffer, int num_block_out, void *data);
int ring_buffer_put(RingBuffer *ringbuffer, int num_block_in, void *data);

snd_pcm_format_t get_alsa_pcm_format (GstAudioFormat fmt);

#endif
