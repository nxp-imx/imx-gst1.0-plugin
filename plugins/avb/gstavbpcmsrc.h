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
#ifndef __GST_AVB_PCM_SRC_H__
#define __GST_AVB_PCM_SRC_H__
#include "gstavbsrc.h"
#include "gstimxcommon.h"

G_BEGIN_DECLS


#define GST_TYPE_AVB_PCM_SRC \
  (gst_avb_pcm_src_get_type())

#define GST_AVB_PCM_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVB_PCM_SRC,GstAvbPcmSrc))

#define GST_AVB_PCM_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVB_PCM_SRC,GstAvbPcmSrcClass))

#define GST_IS_AVB_PCM_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVB_PCM_SRC))

#define GST_IS_AVB_PCM_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVB_PCM_SRC))



typedef struct _GstAvbPcmSrc GstAvbPcmSrc;
typedef struct _GstAvbPcmSrcClass GstAvbPcmSrcClass;

struct _GstAvbPcmSrc
{
  GstAvbSrc parent;
  guint32 malloc_buffer_size;
  gint bitPerSample;
  gint channels;
  gint rate;
  guint64 output_ts;
};

struct _GstAvbPcmSrcClass
{
  GstAvbSrcClass parent_class;
};

GType gst_avb_pcm_src_get_type(void);
G_END_DECLS


#endif /* __GST_AVB_PCM_SRC_H__ */

