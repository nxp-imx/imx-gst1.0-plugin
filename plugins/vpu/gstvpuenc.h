/*
 * Copyright (c) 2014, Freescale Semiconductor, Inc. All rights reserved.
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

#define GST_TYPE_VPU_ENC \
  (gst_vpu_enc_get_type())
#define GST_VPU_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VPU_ENC,GstVpuEnc))
#define GST_VPU_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VPU_ENC,GstVpuEncClass))
#define GST_IS_VPU_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VPU_ENC))
#define GST_IS_VPU_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VPU_ENC))

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
  GstMemory *output_gst_memory;
  PhyMemBlock *output_phys_buffer;
  guint gop_count;
};

struct _GstVpuEncClass {
  GstVideoEncoderClass encoder_class;
};

GType gst_vpu_enc_get_type(void);

G_END_DECLS

#endif /* __GST_VPU_ENC_H__ */
