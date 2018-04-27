/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Copyright (c) 2011-2014, Freescale Semiconductor, Inc. All rights reserved. 
 * Copyright 2017-2018 NXP
 *
 */




/*
 * Module Name:    beepdec.h
 *
 * Description:    Head file of unified audio decoder gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */


#ifndef __BEEPDEC_H__
#define __BEEPDEC_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/gstaudiodecoder.h>
#include <gst/audio/audio.h>

#include "fsl_types.h"
#include "fsl_unia.h"
#include "beepregistry.h"



G_BEGIN_DECLS
    
GST_DEBUG_CATEGORY_EXTERN (beep_dec_debug);
#define GST_CAT_DEFAULT beep_dec_debug

#define GST_TYPE_BEEP_DEC \
  (gst_beep_dec_get_type())
#define GST_BEEP_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BEEP_DEC,GstBeepDec))
#define GST_BEEP_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BEEP_DEC,GstBeepDecClass))
#define GST_IS_BEEP_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BEEP_DEC))
#define GST_IS_BEEP_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BEEP_DEC))


typedef struct _GstBeepDec GstBeepDec;
typedef struct _GstBeepDecClass GstBeepDecClass;

struct _GstBeepDec
{
    GstAudioDecoder element;
    BeepCoreInterface *beep_interface;
    UniACodec_Handle handle;
    UniAcodecOutputPCMFormat outputformat;
    GstAudioFormat audio_format;
    GstAudioInfo      audio_info;
    gint err_cnt;
    GstAdapter *adapter;
    gint not_enough_cnt;//for wma walk around
    GstAudioChannelPosition * core_layout;
    GstAudioChannelPosition * out_layout;
    gboolean output_changed;
    gboolean set_codec_data;
    gint frame_cnt;
    gint in_cnt;
    gboolean eos_sent;
    gboolean dsp_dec;       /* use hifi decoder or not*/
};

struct _GstBeepDecClass
{
    GstAudioDecoderClass parent_class;
};

GType gst_beep_dec_get_type(void);
G_END_DECLS



#endif /* __BEEPDEC_H__ */
