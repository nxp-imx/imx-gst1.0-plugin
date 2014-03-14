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
 *
 */


/*
 * Module Name:   beepdec.c
 *
 * Description:   Implementation of unified audio decoder gstreamer plugin
 *
 * Portability:   This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */


#include <string.h>
#include <gst/tag/tag.h>

#include "beepdec.h"

GST_DEBUG_CATEGORY (beep_dec_debug);

#define CORE_FATAL_ERROR_MASK ((uint32)0xff)
#define CORE_STATUS_MASK (~(uint32)0xff)
#define MAX_PROFILE_ERROR_COUNT 50 //about 1 second decoding time, 1 seconds' audio data length
#define VORBIS_HEADER_FRAME 3
#define GST_TAG_BEEP_SAMPLING_RATE  "sampling_frequency"
#define GST_TAG_BEEP_CHANNELS       "channels"

#define GST_BUFFER_TIMESTAMP GST_BUFFER_PTS

static gstsutils_property beep_property[]=
{
    {"tolerance", G_TYPE_UINT64, gst_audio_decoder_set_tolerance},
    {"min-latency", G_TYPE_UINT64, gst_audio_decoder_set_min_latency},
    {"plc", G_TYPE_BOOLEAN, gst_audio_decoder_set_plc}
};


#define BEEP_PCM_CAPS \
   "audio/x-raw, "\
   "format = (string){S8, S16LE, S24LE, S32LE}, "\
   "rate = (int){7350, 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000}, "\
   "channels = (int)[1, 8]"

static GstStaticPadTemplate beep_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (BEEP_PCM_CAPS)
);

static gint32 alsa_1channel_layout[] = {
  /* FC */
  UA_CHANNEL_FRONT_CENTER,
};

static gint32 alsa_2channel_layout[] = {
  /* FL,FR */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
};

static gint32 alsa_3channel_layout[] = {
  /* FL,FR,LFE */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_FRONT_CENTER,
  //UA_CHANNEL_LFE
};

static gint32 alsa_4channel_layout[] = {
  /* FL,FR,BL,BR */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_REAR_LEFT,
  UA_CHANNEL_REAR_RIGHT,
};

static gint32 alsa_5channel_layout[] = {
/* FL,FR,BL,BR,FC*/
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_FRONT_CENTER,
  UA_CHANNEL_REAR_LEFT,
  UA_CHANNEL_REAR_RIGHT,
};

static gint32 alsa_6channel_layout[] = {
/* FL,FR,BL,BR,FC,LFE */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_FRONT_CENTER,
  UA_CHANNEL_LFE,
  UA_CHANNEL_REAR_LEFT,
  UA_CHANNEL_REAR_RIGHT,
};

static gint32 alsa_8channel_layout[] = {
/* FL,FR,BL,BR,FC,LFE,SL,SR */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_REAR_LEFT,
  UA_CHANNEL_REAR_RIGHT,
  UA_CHANNEL_FRONT_CENTER,
  UA_CHANNEL_LFE,
  UA_CHANNEL_SIDE_LEFT,
  UA_CHANNEL_SIDE_RIGHT,
};


static gint32 *alsa_channel_layouts[] = {
  NULL,
  alsa_1channel_layout,         // 1
  alsa_2channel_layout,         // 2
  alsa_3channel_layout,
  alsa_4channel_layout,
  alsa_5channel_layout,
  alsa_6channel_layout,
  NULL,
  alsa_8channel_layout,
};

#define gst_beep_dec_parent_class parent_class
G_DEFINE_TYPE (GstBeepDec, gst_beep_dec, GST_TYPE_AUDIO_DECODER);

static void gst_beep_dec_finalize (GObject * object);
static gboolean beep_dec_set_format(GstAudioDecoder *dec, GstCaps *caps);

static gboolean beep_dec_start (GstAudioDecoder * dec);
static gboolean beep_dec_stop (GstAudioDecoder * dec);
static GstFlowReturn beep_dec_parse_and_decode (GstAudioDecoder * dec,
    GstAdapter *adapter,gint *offset, gint *length);
static GstFlowReturn beep_dec_handle_frame (GstAudioDecoder * dec,
    GstBuffer * buffer);
static void beep_dec_flush (GstAudioDecoder * dec, gboolean hard);


static GstPadTemplate *
gst_beep_dec_sink_pad_template (void)
{
  static GstPadTemplate *templ = NULL;

  if (!templ) {
    GstCaps *caps = beep_core_get_caps ();

    if (caps) {
      templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
    }
  }
  return templ;
}

static void
gst_beep_dec_class_init (GstBeepDecClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
    GstAudioDecoderClass *base_class = GST_AUDIO_DECODER_CLASS (klass);

    gobject_class->finalize = gst_beep_dec_finalize;

    gst_element_class_add_pad_template (gstelement_class,
        gst_beep_dec_sink_pad_template ());

    gst_element_class_add_pad_template (gstelement_class,
        gst_static_pad_template_get (&beep_src_template));

    base_class->set_format= GST_DEBUG_FUNCPTR (beep_dec_set_format);
    base_class->start = GST_DEBUG_FUNCPTR (beep_dec_start);
    base_class->stop = GST_DEBUG_FUNCPTR (beep_dec_stop);
    base_class->handle_frame = GST_DEBUG_FUNCPTR (beep_dec_handle_frame);
    //base_class->parse = GST_DEBUG_FUNCPTR (beep_dec_parse_and_decode);
    base_class->flush = GST_DEBUG_FUNCPTR (beep_dec_flush);
    
    gst_element_class_set_static_metadata (gstelement_class, "Beep universal decoder",
        "Codec/Decoder/Audio",
        "Decode compressed audio to raw data",
        "FreeScale Multimedia Team <shamm@freescale.com>");
    
    GST_DEBUG_CATEGORY_INIT (beep_dec_debug, "beepdec", 0, "beepdec plugin");
    GST_LOG("gst_beep_dec_class_init \n");
}
static void
gst_beep_dec_init (GstBeepDec * dec)
{
    gst_audio_decoder_set_tolerance(GST_AUDIO_DECODER_CAST(dec),200000000);
    gstsutils_load_default_property(beep_property,GST_AUDIO_DECODER_CAST(dec),
        FSL_GST_CONF_DEFAULT_FILENAME,"beepdec");

GST_LOG("gst_beep_dec_init \n");
}
static void gst_beep_dec_finalize (GObject * object)
{
    GST_LOG("gst_beep_dec_finalize \n");

    G_OBJECT_CLASS (parent_class)->finalize (object);
}
static void *
beepdec_core_mem_alloc (uint32 size)
{
  void *ret = NULL;
  if (size) {
    ret = g_try_malloc (size);
  }
  return ret;
}

static void
beepdec_core_mem_free (void *ptr)
{
  g_free (ptr);
}

static void *
beepdec_core_mem_calloc (uint32 numElements, uint32 size)
{
  uint32 alloc_size = size * numElements;
  void *ret = NULL;
  if (alloc_size) {
    ret = g_try_malloc (alloc_size);
    if (ret) {
      memset (ret, 0, alloc_size);
    }
  }
  return ret;
}

static void *
beepdec_core_mem_realloc (void *ptr, uint32 size)
{
  void *ret = g_try_realloc (ptr, size);
  return ret;
}

static gboolean beep_dec_set_init_parameter(GstBeepDec * beep_dec,
    GstCaps *caps)
{
    GstStructure *structure;
    UniACodecParameter parameter;
    gint intvalue;
    gboolean framed = FALSE;
    gchar * stream_format;
    gboolean ret = FALSE;
    BeepCoreInterface *IDecoder = NULL;
    UniACodec_Handle handle;
    const GValue *value = NULL;

    do{

        if(!beep_dec || !caps)
            break;

        IDecoder = beep_dec->beep_interface;
        handle = beep_dec->handle;

        if(!IDecoder || !handle)
            break;

        structure = gst_caps_get_structure (caps, 0);

        if (gst_structure_get_int (structure, "rate", &intvalue)) {
            GST_INFO ("Set rate %d", intvalue);
            parameter.samplerate = intvalue;
            IDecoder->setDecoderPara(handle, UNIA_SAMPLERATE, &parameter);
        }

        if (gst_structure_get_int (structure, "bitrate", &intvalue)) {
            GST_INFO ("Set rate %d", intvalue);
            parameter.bitrate = intvalue;
            IDecoder->setDecoderPara(handle, UNIA_BITRATE, &parameter);
        }

        if (gst_structure_get_int (structure, "channels", &intvalue)) {
            GST_INFO ("Set channel %d", intvalue);
            parameter.channels = intvalue;
            IDecoder->setDecoderPara(handle, UNIA_CHANNEL, &parameter);
        }

        //TODO:depth

        if (gst_structure_get_int (structure, "block_align", &intvalue)) {
            GST_INFO ("Set block align %d", intvalue);
            parameter.blockalign = intvalue;
            IDecoder->setDecoderPara(handle,UNIA_WMA_BlOCKALIGN, &parameter);
        }

        if (gst_structure_get_int (structure, "wmaversion", &intvalue)) {
            GST_INFO ("Set wma version %d", intvalue);
            parameter.version = intvalue;
            IDecoder->setDecoderPara(handle, UNIA_WMA_VERSION, &parameter);
        }
        
        value = gst_structure_get_value (structure, "codec_data");
        if (value) {
            GstBuffer *codec_data = gst_value_get_buffer (value);
            GstMapInfo map;
            
            if ((codec_data) && gst_buffer_get_size (codec_data)) {
                gst_buffer_map(codec_data, &map, GST_MAP_READ);
                GST_INFO ("Set codec_data %" GST_PTR_FORMAT, codec_data);
                parameter.codecData.size = map.size;
                parameter.codecData.buf = map.data;
                IDecoder->setDecoderPara(handle,UNIA_CODEC_DATA, &parameter);
                gst_buffer_unmap(codec_data, &map);
            }
        }

        stream_format = gst_structure_get_string(structure, "stream-format");
        if (stream_format) {
            GST_INFO ("Set stream_type %s", stream_format);
            if(g_strcmp0(stream_format, "adts") == 0) {
                parameter.stream_type = STREAM_ADTS;
            } 
            else if(g_strcmp0(stream_format, "adif") == 0) {
                parameter.stream_type = STREAM_ADIF;
            }
            else if(g_strcmp0(stream_format, "raw") == 0) {
                parameter.stream_type = STREAM_RAW;
            }
            else {
                parameter.stream_type = STREAM_UNKNOW;
            }

            IDecoder->setDecoderPara(handle,UNIA_STREAM_TYPE, &parameter);
        }

        parameter.framed = FALSE;

        //set framed for all vorbis
        if(!strcmp(IDecoder->name,"vorbis"))
            parameter.framed = TRUE;

        if(gst_structure_get_boolean(structure, "framed",&framed)){
            parameter.framed |= framed;
        }

        if(gst_structure_get_boolean(structure, "parsed",&framed)){
            parameter.framed |= framed;
        }

        //beep_dec->framed = parameter.framed;
        GST_INFO ("Set framed %s", ((parameter.framed) ? "true" : "false"));
        IDecoder->setDecoderPara(handle,UNIA_FRAMED, &parameter);

        {
        CHAN_TABLE table;
        memset(&table,0,sizeof(table));
        table.size = 8;
        memcpy(&table.channel_table,alsa_channel_layouts,sizeof(alsa_channel_layouts));
        //set output format for channel layout
        IDecoder->setDecoderPara(handle,UNIA_CHAN_MAP_TABLE,(UniACodecParameter*)&table);
        }

        ret = TRUE;
    }while(0);

    return ret;
}

static gboolean beep_dec_set_format(GstAudioDecoder *dec, GstCaps *caps)
{
    gboolean ret = FALSE;
    GstBeepDec *beepdec;
    UniACodecMemoryOps ops;
    BeepCoreInterface *IDecoder = NULL;

    do{
        beepdec = GST_BEEP_DEC (dec);

        if(beepdec->beep_interface == NULL)
            beepdec->beep_interface = beep_core_create_interface_from_caps (caps);

        if(beepdec->beep_interface == NULL)
            break;

        IDecoder = beepdec->beep_interface;

        ops.Malloc = beepdec_core_mem_alloc;
        ops.Calloc = beepdec_core_mem_calloc;
        ops.ReAlloc = beepdec_core_mem_realloc;
        ops.Free = beepdec_core_mem_free;

        beepdec->handle = IDecoder->createDecoder (&ops);

        if(beepdec->handle == NULL)
            break;

        ret = beep_dec_set_init_parameter(beepdec,caps);
        
    }while(0);

    if (ret == FALSE && beepdec->handle) {
        beepdec->beep_interface->deleteDecoder (beepdec->handle);
        beepdec->handle = NULL;
    }

    GST_LOG_OBJECT (beepdec,"beep_dec_set_format called ret=%d", ret);
    return ret;
}

static gboolean beep_dec_start (GstAudioDecoder * dec)
{
    GstBeepDec *beepdec;

    beepdec = GST_BEEP_DEC (dec);

    beepdec->err_cnt = 0;
    beepdec->not_enough_cnt = 0;
    memset (&beepdec->outputformat, 0, sizeof(UniAcodecOutputPCMFormat));
    memset (&beepdec->audio_info, 0, sizeof(GstAudioInfo));
    beepdec->adapter = gst_adapter_new ();
    beepdec->core_layout = NULL;
    beepdec->core_layout = NULL;
    beepdec->output_changed = FALSE;
    beepdec->frame_cnt = 0;
    beepdec->send_vorbis_codec_data = FALSE;
    beepdec->in_cnt = 0;
    
    gst_tag_register (GST_TAG_BEEP_CHANNELS, GST_TAG_FLAG_DECODED,
        G_TYPE_UINT, "number of channels", "number of channels", NULL);
    gst_tag_register (GST_TAG_BEEP_SAMPLING_RATE, GST_TAG_FLAG_DECODED,
        G_TYPE_UINT, "sampling frequency (Hz)", "sampling frequency (Hz)",
        NULL);

    GST_LOG_OBJECT (beepdec,"beep_dec_start called ");

    return TRUE;
}
static gboolean beep_dec_stop (GstAudioDecoder * dec)
{
    GstBeepDec *beepdec;

    beepdec = GST_BEEP_DEC (dec);

    if (beepdec->beep_interface && beepdec->handle) {
        if (beepdec->beep_interface->deleteDecoder) {
          beepdec->beep_interface->deleteDecoder (beepdec->handle);
          beepdec->handle = NULL;
        }
    }
    if (beepdec->adapter) {
        gst_adapter_clear (beepdec->adapter);
        g_object_unref (beepdec->adapter);
        beepdec->adapter = NULL;
    }

    if(beepdec->core_layout)
        g_free(beepdec->core_layout);

    if(beepdec->out_layout)
        g_free(beepdec->out_layout);


    beep_core_destroy_interface (beepdec->beep_interface);
    beepdec->beep_interface = NULL;
    GST_LOG_OBJECT (beepdec,"beep_dec_stop called ");
    return TRUE;
}
static gboolean beep_dec_map_channel_layout(UniAcodecOutputPCMFormat * outputValue,
    GstAudioChannelPosition *pos)
{
    uint32 i = 0;

    uint32 nChannels = outputValue->channels;


    if(outputValue == NULL || pos == NULL){
        return FALSE;
    }

    if(nChannels == 1){
        pos[0] = GST_AUDIO_CHANNEL_POSITION_MONO;
        return TRUE;
    }
    
    if(nChannels == 2){
        pos[0] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
        pos[1] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
        return TRUE;
    }

    for( i = 0; i < nChannels; i++){
        
        switch(outputValue->layout[i]){
            case UA_CHANNEL_FRONT_LEFT:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
                break;
            case UA_CHANNEL_FRONT_RIGHT:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
                break;
            case UA_CHANNEL_REAR_CENTER:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_REAR_CENTER;
                break;
            case UA_CHANNEL_REAR_LEFT:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_REAR_LEFT;
                break;
            case UA_CHANNEL_REAR_RIGHT:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT;
                break;
            case UA_CHANNEL_LFE:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_LFE1;
                break;
            case UA_CHANNEL_FRONT_CENTER:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER;
                break;
            case UA_CHANNEL_FRONT_LEFT_CENTER:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
                break;
            case UA_CHANNEL_FRONT_RIGHT_CENTER:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
                break;
            case UA_CHANNEL_SIDE_LEFT:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT;
                break;
            case UA_CHANNEL_SIDE_RIGHT:
                pos[i] = GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT;
                break;
            default:
                break;
            }

    }

    return TRUE;
}

static void beep_dec_handle_output_changed(GstBeepDec *beepdec)
{
    BeepCoreInterface *IDecoder = NULL;
    UniACodec_Handle handle;
    UniACodecParameter parameter = { 0 };
    GstAudioInfo info;
    GstAudioChannelPosition *pos = NULL;
    GstTagList *list = NULL;

    do{

        if(beepdec == NULL)
            break;

        IDecoder = beepdec->beep_interface;
        handle = beepdec->handle;

        if(!IDecoder || !handle){
            break;
        }
        
        IDecoder->getDecoderPara(handle,UNIA_OUTPUT_PCM_FORMAT,&parameter);

        if (!memcmp (&parameter.outputFormat, &beepdec->outputformat,
                sizeof (UniAcodecOutputPCMFormat))) {
            break;
        }

        beepdec->outputformat = parameter.outputFormat;

        GST_DEBUG_OBJECT(beepdec,"output changed channel num=%d,core layout=%d,%d,%d,%d,%d,%d,%d,%d",
            beepdec->outputformat.channels,
            beepdec->outputformat.layout[0], beepdec->outputformat.layout[1],
            beepdec->outputformat.layout[2], beepdec->outputformat.layout[3],
            beepdec->outputformat.layout[4], beepdec->outputformat.layout[5],
            beepdec->outputformat.layout[6], beepdec->outputformat.layout[7]);


        if(beepdec->core_layout != NULL)
            g_free(beepdec->core_layout);
        
        if(beepdec->out_layout != NULL)
            g_free(beepdec->out_layout);

        if(beepdec->outputformat.channels > 0){
            beepdec->core_layout = g_malloc(sizeof(GstAudioChannelPosition)
                * 64);
            beepdec->out_layout = g_malloc(sizeof(GstAudioChannelPosition)
                * 64);

        }

        if(beepdec->core_layout && beepdec->out_layout){
            memset(beepdec->core_layout,0,sizeof(GstAudioChannelPosition)* 64);
            memset(beepdec->out_layout,0,sizeof(GstAudioChannelPosition)* 64);
            beep_dec_map_channel_layout(&beepdec->outputformat,beepdec->core_layout);
            memcpy (beepdec->out_layout,beepdec->core_layout,
                sizeof (GstAudioChannelPosition) * beepdec->outputformat.channels);
        }

        GST_DEBUG_OBJECT(beepdec,"output changed after convert num=%d,core layout=%d,%d,%d,%d,%d,%d,%d,%d",
            beepdec->outputformat.channels,
            beepdec->core_layout[0], beepdec->core_layout[1],
            beepdec->core_layout[2], beepdec->core_layout[3],
            beepdec->core_layout[4], beepdec->core_layout[5],
            beepdec->core_layout[6], beepdec->core_layout[7]);

        gst_audio_channel_positions_to_valid_order (beepdec->out_layout,
            beepdec->outputformat.channels);

        GST_DEBUG_OBJECT(beepdec,"output changed after map num=%d,gstreamer layout=%d,%d,%d,%d,%d,%d,%d,%d",
            beepdec->outputformat.channels,
            beepdec->out_layout[0], beepdec->out_layout[1],
            beepdec->out_layout[2], beepdec->out_layout[3],
            beepdec->out_layout[4], beepdec->out_layout[5],
            beepdec->out_layout[6], beepdec->out_layout[7]);

        beepdec->audio_format = gst_audio_format_build_integer (TRUE, G_BYTE_ORDER,
            beepdec->outputformat.width, beepdec->outputformat.depth);

        gst_audio_info_set_format (&info,
            beepdec->audio_format,
            beepdec->outputformat.samplerate,
            beepdec->outputformat.channels, NULL);

        if (!memcmp (&beepdec->audio_info, &info, sizeof (GstAudioInfo))) {
            break;
        }

        beepdec->audio_info = info;

        beepdec->output_changed = TRUE;
        gst_audio_decoder_set_output_format (GST_AUDIO_DECODER (beepdec), &beepdec->audio_info);

        list = gst_tag_list_new_empty ();

        gst_tag_list_add (list, GST_TAG_MERGE_REPLACE, GST_TAG_BEEP_CHANNELS,
            parameter.outputFormat.channels, NULL);

        IDecoder->getDecoderPara(handle,UNIA_BITRATE,&parameter);
        if(parameter.bitrate > 0){
            gst_tag_list_add (list, GST_TAG_MERGE_REPLACE, GST_TAG_BITRATE,
                parameter.bitrate, NULL);
        }

        IDecoder->getDecoderPara(handle,UNIA_SAMPLERATE,&parameter);
        gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
            GST_TAG_BEEP_SAMPLING_RATE, parameter.samplerate, NULL);

        IDecoder->getDecoderPara(handle,UNIA_CODEC_DESCRIPTION,&parameter);
        gst_tag_list_add (list, GST_TAG_MERGE_REPLACE, GST_TAG_AUDIO_CODEC,
            *(parameter.codecDesc), NULL);

        gst_audio_decoder_merge_tags (GST_AUDIO_DECODER_CAST (beepdec), list,
            GST_TAG_MERGE_REPLACE);
        gst_tag_list_unref (list);

    }while(0);

}
#if 0
static GstFlowReturn beep_dec_parse_and_decode (GstAudioDecoder * dec,
    GstAdapter *adapter,gint *offset, gint *length)
{
    GstBeepDec *beepdec;
    BeepCoreInterface *IDecoder = NULL;
    UniACodec_Handle handle;
    GstFlowReturn ret = GST_FLOW_EOS;
    uint32 core_ret;
    GstBuffer *gstbuf;
    uint8 *inbuf = NULL;
    uint32 inbuf_size;
    uint32 inbuf_offset = 0;

    uint8 *outbuf = NULL;
    uint32 outbuf_size;


    GstMapInfo map;

    uint32 status;

    beepdec = GST_BEEP_DEC (dec);

    if(!beepdec || !adapter || !offset || !length)
        goto bail;

    IDecoder = beepdec->beep_interface;
    handle = beepdec->handle;

    if(!IDecoder || !handle){
        goto bail;
    }

    inbuf_size = gst_adapter_available (adapter);

    inbuf = gst_adapter_map (adapter, inbuf_size);

    do{

        core_ret = IDecoder->decode(handle,inbuf,inbuf_size,&inbuf_offset,
            &outbuf,&outbuf_size);

        GST_LOG("beep_dec_parse_and_decode RET=%x input size=%d,used size=%d,output_size=%d"
            ,core_ret,inbuf_size,inbuf_offset,outbuf_size);

        status = core_ret & CORE_STATUS_MASK;
    
        if (status == ACODEC_CAPIBILITY_CHANGE) {
            beep_dec_handle_output_changed(beepdec);
        }

        if (ACODEC_ERROR_STREAM == core_ret) {
            GST_WARNING("error = %x\n", core_ret);
            IDecoder->resetDecoder(handle);
            goto bail;
        }else if(core_ret == ACODEC_PROFILE_NOT_SUPPORT){
            beepdec->err_cnt ++;
            goto bail;
        }else if(core_ret == ACODEC_NOT_ENOUGH_DATA){
            beepdec->not_enough_cnt ++;
            goto bail;
        }else if(core_ret == ACODEC_END_OF_STREAM){
            goto bail;
        }

        if(outbuf && outbuf_size > 0){
            gstbuf = gst_audio_decoder_allocate_output_buffer (GST_AUDIO_DECODER (dec), outbuf_size);
            gstbuf = gst_buffer_make_writable (gstbuf);
            
            gst_buffer_fill (gstbuf, 0, outbuf, outbuf_size);

            gst_audio_buffer_reorder_channels (gstbuf, beepdec->audio_format,
                beepdec->outputformat.channels, beepdec->core_layout, beepdec->out_layout);

            gst_adapter_push (beepdec->adapter, gstbuf);
            beepdec->err_cnt = 0;
        }

        if(inbuf_offset == inbuf_size && outbuf_size > 0)
            goto done;


    }while(1);

done:
    *offset = 0;
    *length = inbuf_offset;
    
    GST_LOG("done");

    ret = GST_FLOW_OK;
    return ret;

bail:

    if(length && offset){
        *length = inbuf_size;
        *offset = inbuf_size;
    }

    if(gst_adapter_available (beepdec->adapter) && offset){
        GST_LOG("bail , ok");
        *offset = 0;
        ret = GST_FLOW_OK;
    }else{
        GST_LOG("bail , eos");
        ret = GST_FLOW_EOS;
    }
    return ret;

}
#endif

static GstFlowReturn beep_dec_handle_frame (GstAudioDecoder * dec,
    GstBuffer * buffer)
{
    GstBeepDec *beepdec;
    BeepCoreInterface *IDecoder = NULL;
    UniACodec_Handle handle;
    GstFlowReturn ret = GST_FLOW_OK;
    uint8 *inbuf = NULL;
    uint32 inbuf_size = 0, offset = 0;
    uint8 *outbuf = NULL;
    uint32 out_size = 0;
    uint32 status;
    uint32 core_ret;
    GstBuffer *out = NULL;
    GstBuffer * temp_buffer = NULL;
    uint32 adapter_size = 0;
    GstMapInfo map;
    gboolean twice = FALSE;
    beepdec = GST_BEEP_DEC (dec);

    if(!beepdec)
        goto bail;
    
    IDecoder = beepdec->beep_interface;
    handle = beepdec->handle;
    
    if(!IDecoder || !handle){
        goto bail;
    }


    if(!buffer){
        //audio decoder will send a null frame after 3 codec data buffer
        //received when playing vorbis audio.
        //return if in this case.
        if(beepdec->in_cnt > 0)
            goto begin;
        else 
            goto bail;
    }

    inbuf_size = gst_buffer_get_size(buffer);

    gst_buffer_map(buffer, &map, GST_MAP_READ);
    inbuf = map.data;
    gst_buffer_unmap(buffer, &map);
    beepdec->in_cnt++;

    GST_LOG_OBJECT (beepdec,"handle_frame [%d] BEGIN size=%d",beepdec->in_cnt,inbuf_size);

    
    if(!strcmp(IDecoder->name,"mp3"))
        twice = TRUE;

    if(!strcmp(IDecoder->name,"vorbis") && !beepdec->send_vorbis_codec_data){
        if(beepdec->frame_cnt < VORBIS_HEADER_FRAME){
            temp_buffer = gst_buffer_new_allocate (NULL, inbuf_size, NULL);
            temp_buffer = gst_buffer_make_writable (temp_buffer);
            gst_buffer_fill (temp_buffer, 0, inbuf, inbuf_size);
            gst_adapter_push (beepdec->adapter, temp_buffer);
            beepdec->frame_cnt ++;
            buffer = NULL;
            beepdec->in_cnt--;
            gst_audio_decoder_finish_frame (dec, NULL, 1);
            goto bail;
        }else if(beepdec->frame_cnt == VORBIS_HEADER_FRAME){
            UniACodecParameter parameter;
            GstBuffer *codec_data;
            GstMapInfo map;

            adapter_size = gst_adapter_available(beepdec->adapter);
            codec_data = gst_adapter_take_buffer (beepdec->adapter, adapter_size);

            if ((codec_data) && gst_buffer_get_size (codec_data)) {
                gst_buffer_map(codec_data, &map, GST_MAP_READ);
                GST_INFO ("Set codec_data %" GST_PTR_FORMAT, codec_data);
                parameter.codecData.size = map.size;
                parameter.codecData.buf = map.data;
                IDecoder->setDecoderPara(handle,UNIA_CODEC_DATA, &parameter);
                gst_buffer_unmap(codec_data, &map);
                beepdec->send_vorbis_codec_data = TRUE;
            }
            gst_adapter_clear (beepdec->adapter);
        }
    }


begin:

    do{

        outbuf = NULL;
        out_size = 0;
        core_ret = IDecoder->decode(handle,inbuf,inbuf_size,&offset,&outbuf,&out_size);

        GST_LOG_OBJECT (beepdec,"decode RET=%x input size=%d,used size=%d,output_size=%d"
            ,core_ret,inbuf_size,offset,out_size);

        if (ACODEC_ERROR_STREAM == core_ret) {
            GST_WARNING("decode END error = %x\n", core_ret);
            IDecoder->resetDecoder(handle);
            //send null frame to delete the timestamp
            beepdec->in_cnt--;
            beepdec->err_cnt ++;
            gst_audio_decoder_finish_frame (dec, NULL, 1);
            break;
        }else if(core_ret == ACODEC_PROFILE_NOT_SUPPORT){
            beepdec->err_cnt ++;
            break;
        }else if(core_ret == ACODEC_NOT_ENOUGH_DATA){
            break;
        }
        
        status = core_ret & CORE_STATUS_MASK;
        
        if (status == ACODEC_CAPIBILITY_CHANGE) {
            beep_dec_handle_output_changed(beepdec);
        }

        if(outbuf && out_size > 0){

           temp_buffer = gst_audio_decoder_allocate_output_buffer (GST_AUDIO_DECODER (dec),out_size);
           temp_buffer = gst_buffer_make_writable (temp_buffer);
           gst_buffer_fill (temp_buffer, 0, outbuf, out_size);
           gst_audio_buffer_reorder_channels (temp_buffer, beepdec->audio_format,
               beepdec->outputformat.channels, beepdec->core_layout, beepdec->out_layout);

           g_free(outbuf);
           if(beepdec->in_cnt > 1 )
           {
                beepdec->in_cnt--;
                gst_audio_decoder_finish_frame (dec, temp_buffer, 1);
                GST_LOG_OBJECT (beepdec,"output one frame[%d] size=%d",beepdec->in_cnt,out_size);
           }else{
                gst_adapter_push (beepdec->adapter, temp_buffer);
           }
           beepdec->err_cnt = 0;

           temp_buffer = NULL;
        }



    } while (((status != ACODEC_NOT_ENOUGH_DATA)
            && (status != ACODEC_END_OF_STREAM) && (((inbuf_size)
                    && (offset < inbuf_size)) || (inbuf_size == 0))));

#if 0
    if(outbuf && out_size > 0 && twice){
        twice = FALSE;
        goto begin;
    }
#endif

    adapter_size = gst_adapter_available (beepdec->adapter);

    if(adapter_size > 0){
        out = gst_adapter_take_buffer (beepdec->adapter, adapter_size);
        beepdec->in_cnt--;
        gst_audio_decoder_finish_frame (dec, out, 1);
        gst_adapter_clear (beepdec->adapter);
        ret = GST_FLOW_OK;
        GST_LOG_OBJECT (beepdec,"output frames[%d] size=%d",beepdec->in_cnt,adapter_size);
    }else{
        //gst_audio_decoder_finish_frame (dec, NULL, 1);
        GST_LOG_OBJECT (beepdec,"beep_dec_parse_and_decode ret=%x",ret);
    }

    if(beepdec->err_cnt > MAX_PROFILE_ERROR_COUNT)
        ret = GST_FLOW_NOT_SUPPORTED;

bail:
    return ret;

}
static void beep_dec_flush (GstAudioDecoder * dec, gboolean hard)
{
    GstBeepDec *beepdec;

    beepdec = GST_BEEP_DEC (dec);

    if (beepdec->beep_interface && beepdec->handle) {
        if (beepdec->beep_interface->resetDecoder) {
          beepdec->beep_interface->resetDecoder (beepdec->handle);
        }
    }
    beepdec->in_cnt = 0;

    GST_LOG_OBJECT (beepdec,"beep_dec_flush called ");
}

