/*
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

/*
 * Copyright (c) 2014, Freescale Semiconductor, Inc. All rights reserved. 
 *
 */


/*
 * Module Name:    gstimxmp3enc.h
 *
 * Description:    Head file of mp3 enoder gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */

#ifndef __GST_IMXMP3ENC_H__
#define __GST_IMXMP3ENC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudioencoder.h>

#include "mp3_enc_interface.h"


G_BEGIN_DECLS

#define GST_TYPE_IMX_MP3ENC \
  (gst_imx_mp3enc_get_type())
#define GST_IMX_MP3ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_IMX_MP3ENC, GstImxMp3Enc))
#define GST_IMX_MP3ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_IMX_MP3ENC, GstImxMp3EncClass))
#define GST_IS_IMX_MP3ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_IMX_MP3ENC))
#define GST_IS_IMX_MP3ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_IMX_MP3ENC))

typedef struct _GstImxMp3Enc GstImxMp3Enc;
typedef struct _GstImxMp3EncClass GstImxMp3EncClass;

struct _GstImxMp3Enc {
  GstAudioEncoder element;

  gboolean discont;

  /* properties */
  gint bitrate;
  gint quality;

  /* caps */
  gint channels;
  gint rate;
  gint interleave;  /* 0 - interleave, 1 - non-interleave */

//  gint output_format;

  gint inbuf_size;

  /* library handle */
  MP3E_Encoder_Config enc_config;
  MP3E_Encoder_Parameter enc_param;

  guint8* buf_blk[6];   /* alloc the buffer block for core encoder use */
  
};

struct _GstImxMp3EncClass {
  GstAudioEncoderClass parent_class;
};

GType gst_imx_mp3enc_get_type (void);

G_END_DECLS

#endif /* __GST_IMX_MP3ENC_H__ */

