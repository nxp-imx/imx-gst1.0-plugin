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
 * Copyright (c) 2014,2016 Freescale Semiconductor, Inc. All rights reserved.
 *
 */


/**
 * SECTION:element-gstimxmp3enc
 *
 * MP3 audio encoder based on fsl mp3 encoder library
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch filesrc location=abc.wav ! wavparse ! audioresample ! audioconvert ! gstimxmp3enc ! filesink location=abc.mp3
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/pbutils/codec-utils.h>

#include "gstimxmp3enc.h"

#define LOW_QUALITY     0
#define HIGH_QUALITY    1
#define IMX_MP3ENC_DEFAULT_BITRATE (128)
#define IMX_MP3ENC_DEFAULT_QUALITY (HIGH_QUALITY)
#define IMX_MP3ENC_MPEGVERSION (1)
#define IMX_MP3ENC_BYTE_PER_SAMPLE (2)
#define IMX_MP3ENC_FRAME_SAMPLE (1152)
#define IMX_MP3ENC_OUTBUF_MAX (1440)

enum
{
  PROP_0,
  PROP_BITRATE,
  PROP_QUALITY
};

#define SAMPLE_RATES " 32000, " "44100, " "48000"


static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
       /* "layout = (string) interleaved, "*/
        "rate = (int) { " SAMPLE_RATES " }, " "channels = (int) [1, 2]")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, "
        "mpegversion = (int) 1, "
        "layer = (int) 3, "
        "rate = (int) {  " SAMPLE_RATES " }, "
        "channels = (int) [ 1, 2 ]")
    );

GST_DEBUG_CATEGORY_STATIC (gst_imx_mp3enc_debug);
#define GST_CAT_DEFAULT gst_imx_mp3enc_debug

static gboolean imx_mp3enc_core_prepare (GstImxMp3Enc * imx_mp3enc);
static void imx_mp3enc_free_mem(GstImxMp3Enc * imx_mp3enc);
static gboolean gst_imx_mp3enc_start (GstAudioEncoder * enc);
static gboolean gst_imx_mp3enc_stop (GstAudioEncoder * enc);
static void gst_imx_mp3enc_flush(GstAudioEncoder *enc);
static gboolean gst_imx_mp3enc_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static GstFlowReturn gst_imx_mp3enc_handle_frame (GstAudioEncoder * enc,
    GstBuffer * in_buf);

G_DEFINE_TYPE (GstImxMp3Enc, gst_imx_mp3enc, GST_TYPE_AUDIO_ENCODER);

static void
gst_imx_mp3enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstImxMp3Enc *self = GST_IMX_MP3ENC (object);

  switch (prop_id) {
    case PROP_BITRATE:
      self->bitrate = g_value_get_int (value);
      break;
    case PROP_QUALITY:
      self->quality = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}

static void
gst_imx_mp3enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstImxMp3Enc *self = GST_IMX_MP3ENC (object);

  switch (prop_id) {
    case PROP_BITRATE:
      g_value_set_int (value, self->bitrate);
      break;
    case PROP_QUALITY:
      g_value_set_int (value, self->quality);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}

static void
gst_imx_mp3enc_class_init (GstImxMp3EncClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioEncoderClass *base_class = GST_AUDIO_ENCODER_CLASS (klass);

  object_class->set_property = GST_DEBUG_FUNCPTR (gst_imx_mp3enc_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_imx_mp3enc_get_property);

  base_class->start = GST_DEBUG_FUNCPTR (gst_imx_mp3enc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_imx_mp3enc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_imx_mp3enc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_imx_mp3enc_handle_frame);
  base_class->flush = GST_DEBUG_FUNCPTR (gst_imx_mp3enc_flush);

  g_object_class_install_property (object_class, PROP_BITRATE,
      g_param_spec_int ("bitrate", "Bitrate (kb/s)",
          "Target Audio Bitrate in kbit/sec (only valid if bitrate is one of "
          "32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320)",
          32, 320, IMX_MP3ENC_DEFAULT_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_QUALITY,
      g_param_spec_int ("quality", "Quality",
          "MP3 Encoder Quality (0-low quality, 1-high quality) ",
          0, 1, IMX_MP3ENC_DEFAULT_QUALITY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  gst_element_class_set_static_metadata (element_class, "imx mp3 audio encoder",
      "Codec/Encoder/Audio", "mp3 audio encoder", "FreeScale Multimedia Team <shamm@freescale.com>");

  GST_DEBUG_CATEGORY_INIT (gst_imx_mp3enc_debug, "imx_mp3enc", 0, "imxmp3enc plugin");
}

static void
gst_imx_mp3enc_init (GstImxMp3Enc * imx_mp3enc)
{
  imx_mp3enc->bitrate = IMX_MP3ENC_DEFAULT_BITRATE;
  imx_mp3enc->quality = IMX_MP3ENC_DEFAULT_QUALITY;

  /* init rest */
  memset(&imx_mp3enc->enc_config, 0, sizeof(MP3E_Encoder_Config));
  memset(&imx_mp3enc->enc_param, 0, sizeof(MP3E_Encoder_Parameter));

  g_print("====== MP3ENC: %s build on %s %s. ======\n",  (VERSION),__DATE__,__TIME__);

}

static gboolean
gst_imx_mp3enc_start (GstAudioEncoder * enc)
{
  GstImxMp3Enc *imx_mp3enc = GST_IMX_MP3ENC (enc);

  GST_DEBUG_OBJECT (enc, "start");

  if (imx_mp3enc_core_prepare (imx_mp3enc) == FALSE)
    return FALSE;

  imx_mp3enc->rate = 0;
  imx_mp3enc->channels = 0;

  return TRUE;
}

static gboolean
gst_imx_mp3enc_stop (GstAudioEncoder * enc)
{
  GstImxMp3Enc *imx_mp3enc = GST_IMX_MP3ENC (enc);

  GST_DEBUG_OBJECT (enc, "stop");
  imx_mp3enc_free_mem(imx_mp3enc);

  return TRUE;
}

static gboolean
gst_imx_mp3enc_set_format (GstAudioEncoder * benc, GstAudioInfo * info)
{
  gboolean ret = TRUE;
  GstImxMp3Enc *imx_mp3enc;
  GstCaps *src_caps;
  MP3E_RET_VAL rc;

  imx_mp3enc = GST_IMX_MP3ENC (benc);

  GST_DEBUG_OBJECT (benc, "set_format");

  /* parameters already parsed for us */
  imx_mp3enc->channels = GST_AUDIO_INFO_CHANNELS (info);
  imx_mp3enc->rate = GST_AUDIO_INFO_RATE (info);
  imx_mp3enc->interleave = GST_AUDIO_INFO_LAYOUT(info); /* 0 - interleave, 1 - noninterleave */


  imx_mp3enc->enc_param.app_bit_rate = imx_mp3enc->bitrate;
  imx_mp3enc->enc_param.app_sampling_rate = imx_mp3enc->rate;

  imx_mp3enc->enc_param.app_mode = ((imx_mp3enc->channels % 2) & 0x3) +
                                   ((imx_mp3enc->interleave & 0x3) << 8) +
	                                 ((imx_mp3enc->quality & 0x3) << 16);

  rc = mp3e_encode_init (&imx_mp3enc->enc_param, &imx_mp3enc->enc_config);
  if (rc != MP3E_SUCCESS) {
    GST_ERROR("MP3 core encoder initialization failed, rc= %d", rc);
    return FALSE;
  }

  /* precalc buffer size as it's constant now */
  imx_mp3enc->inbuf_size = imx_mp3enc->channels * IMX_MP3ENC_FRAME_SAMPLE * IMX_MP3ENC_BYTE_PER_SAMPLE;

  /* create reverse caps */
  src_caps =  gst_caps_new_simple ("audio/mpeg",
        "mpegversion", G_TYPE_INT, IMX_MP3ENC_MPEGVERSION,
        "channels", G_TYPE_INT, imx_mp3enc->channels,
        "layer", G_TYPE_INT, 3,
        "channels", G_TYPE_INT, imx_mp3enc->channels,
        "rate", G_TYPE_INT, imx_mp3enc->rate,
        NULL);


  if (src_caps) {
    gst_audio_encoder_set_output_format (GST_AUDIO_ENCODER (imx_mp3enc),
        src_caps);
    gst_caps_unref (src_caps);
  }

  /* report needs to base class */
  gst_audio_encoder_set_frame_samples_min (benc, IMX_MP3ENC_FRAME_SAMPLE);
  gst_audio_encoder_set_frame_samples_max (benc, IMX_MP3ENC_FRAME_SAMPLE);
  gst_audio_encoder_set_frame_max (benc, 1);

  return ret;
}

static GstFlowReturn
gst_imx_mp3enc_handle_frame (GstAudioEncoder * benc, GstBuffer * buf)
{
  GstImxMp3Enc *imx_mp3enc;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *out;

  MP3E_Encoder_Config *pEnc_config;
  GstMapInfo map, omap;
  GstAudioInfo *info = gst_audio_encoder_get_audio_info (benc);

  GST_DEBUG_OBJECT (benc, "handle_frame");

  imx_mp3enc = GST_IMX_MP3ENC (benc);

  /* we don't deal with squeezing remnants, so simply discard those */
  if (G_UNLIKELY (buf == NULL)) {
    GST_DEBUG_OBJECT (benc, "no data");
    goto exit;
  }

  gst_buffer_map (buf, &map, GST_MAP_READ);

  if (G_UNLIKELY (map.size < imx_mp3enc->inbuf_size)) {
    gst_buffer_unmap (buf, &map);
    GST_DEBUG_OBJECT (imx_mp3enc, "discarding trailing data %d", (gint) map.size);
    ret = gst_audio_encoder_finish_frame (benc, NULL, -1);
    goto exit;
  }

  out = gst_buffer_new_and_alloc (imx_mp3enc->enc_param.mp3e_outbuf_size);
  gst_buffer_map (out, &omap, GST_MAP_WRITE);

  pEnc_config = &imx_mp3enc->enc_config;
  mp3e_encode_frame((MP3E_INT16 *)map.data, pEnc_config, omap.data);

  GST_LOG_OBJECT (imx_mp3enc, "encoded to %lu bytes", pEnc_config->num_bytes);
  gst_buffer_unmap (buf, &map);
  gst_buffer_unmap (out, &omap);
  gst_buffer_resize (out, 0, pEnc_config->num_bytes);

  ret = gst_audio_encoder_finish_frame (benc, out, IMX_MP3ENC_FRAME_SAMPLE);

exit:
  return ret;

}

static void
gst_imx_mp3enc_flush(GstAudioEncoder *benc)
{
  GstImxMp3Enc *imx_mp3enc;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *out;
  GstMapInfo omap;
  MP3E_Encoder_Config *pEnc_config;

  GST_DEBUG_OBJECT (benc, "flush");

  imx_mp3enc = GST_IMX_MP3ENC (benc);

  pEnc_config = &imx_mp3enc->enc_config;

  out = gst_buffer_new_and_alloc (imx_mp3enc->enc_param.mp3e_outbuf_size);
  gst_buffer_map (out, &omap, GST_MAP_WRITE);

  mp3e_flush_bitstream(pEnc_config, omap.data);

  GST_LOG_OBJECT (imx_mp3enc, "mp3e flush %lu bytes", pEnc_config->num_bytes);
  gst_buffer_unmap (out, &omap);
  gst_buffer_resize (out, 0, pEnc_config->num_bytes);

  ret = gst_audio_encoder_finish_frame (benc, out, IMX_MP3ENC_FRAME_SAMPLE);

  if (ret!=GST_FLOW_ERROR) {
    gst_buffer_unref (out);
  }
}


static gboolean imx_mp3enc_alloc_mem(GstImxMp3Enc * imx_mp3enc)
{
  MP3E_Encoder_Config *enc_config = &imx_mp3enc->enc_config;
  int instance_id = enc_config->instance_id;
  guint8 * buf_pt = NULL;
  int i;

  for (i=0; i<6; i++)  {
    imx_mp3enc->buf_blk[i] = (char *)g_malloc(sizeof(char)*enc_config->mem_info[i].size);
    buf_pt = imx_mp3enc->buf_blk[i];
    if (NULL == buf_pt)
      return FALSE;
    enc_config->mem_info[i].ptr = (int*)((unsigned int )(buf_pt + enc_config->mem_info[i].align - 1 )
                   & (0xffffffff ^ (enc_config->mem_info[i].align - 1 )));

  }

  return TRUE;
}

static gboolean
imx_mp3enc_core_prepare (GstImxMp3Enc * imx_mp3enc)
{
  MP3E_RET_VAL rc;

  GST_INFO_OBJECT(imx_mp3enc, "%s", MP3ECodecVersionInfo());

  imx_mp3enc->enc_config.instance_id = 0;

  /* memory setup */
  rc = mp3e_query_mem (&imx_mp3enc->enc_config);
  if (rc != MP3E_SUCCESS) {
    GST_ERROR("MP3 Encoder query memory failed");
    return FALSE;
  }

  if (imx_mp3enc_alloc_mem(imx_mp3enc) == FALSE ) {
    GST_ERROR("mp3 enc alloc mem failed");
    return FALSE;
  }

  return TRUE;
}

static void imx_mp3enc_free_mem(GstImxMp3Enc * imx_mp3enc)
{
  MP3E_Encoder_Config *enc_config = &imx_mp3enc->enc_config;
  guint8 * buf_pt = NULL;
  int i;

  for (i=0; i<6; i++)  {
    buf_pt = imx_mp3enc->buf_blk[i];
    if (buf_pt) {
      g_free(buf_pt);
      imx_mp3enc->buf_blk[i] = NULL;
    }
  }

  return;
}

