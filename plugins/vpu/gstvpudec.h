/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef __GST_VPU_DEC_H__
#define __GST_VPU_DEC_H__

#include <gst/video/gstvideodecoder.h>
#include "gstvpudecobject.h"

G_BEGIN_DECLS

#define GST_TYPE_VPU_DEC \
  (gst_vpu_dec_get_type())
#define GST_VPU_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VPU_DEC,GstVpuDec))
#define GST_VPU_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VPU_DEC,GstVpuDecClass))
#define GST_IS_VPU_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VPU_DEC))
#define GST_IS_VPU_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VPU_DEC))

typedef struct _GstVpuDec           GstVpuDec;
typedef struct _GstVpuDecClass      GstVpuDecClass;

struct _GstVpuDec {
  GstVideoDecoder decoder;

  GstVpuDecObject *vpu_dec_object;
};

struct _GstVpuDecClass {
  GstVideoDecoderClass decoder_class;
};

GType gst_vpu_dec_get_type(void);

G_END_DECLS

#endif /* __GST_VPU_DEC_H__ */
