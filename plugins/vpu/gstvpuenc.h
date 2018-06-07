/*
 * Copyright (c) 2014, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2018 NXP
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

#ifndef __GST_VPU_ENC_H__
#define __GST_VPU_ENC_H__

#include <gst/video/gstvideoencoder.h>
#include "gstvpuallocator.h"
#include "gstvpu.h"

G_BEGIN_DECLS

typedef struct _GstVpuEnc           GstVpuEnc;
typedef struct _GstVpuEncClass      GstVpuEncClass;

struct _GstVpuEnc {
  GstVideoEncoder encoder;

	guint gop_size;
	guint bitrate;
	gint quant;

	VpuEncHandle handle;
	VpuEncInitInfo init_info;
  VpuInternalMem vpu_internal_mem;
	VpuEncOpenParamSimp open_param;
  GstVideoCodecState *state;
  GstVideoAlignment video_align;
	GstBufferPool *pool;
	GList * gstbuffer_in_vpuenc;
	GstBuffer *internal_input_buffer;
  guint gop_count;
  gboolean bitrate_updated;
  gint64 total_frames;
  gint64 total_time;
};

struct _GstVpuEncClass {
  GstVideoEncoderClass encoder_class;
};

gboolean gst_vpu_enc_register (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_VPU_ENC_H__ */
