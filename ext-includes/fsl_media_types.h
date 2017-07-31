/*
***********************************************************************
* Copyright (c) 2009-2014, 2016 Freescale Semiconductor, Inc.
* All modifications are confidential and proprietary information
* of Freescale Semiconductor, Inc. 
*
* Copyright 2017 NXP
***********************************************************************/
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
*  History :
*  Date             Author              Version    Description
*
*  Oct,2009         Amanda              1.0        Initial Version
*
*/

#ifndef _FSL_MMLAYER_MEDIA_TYPES_H
#define _FSL_MMLAYER_MEDIA_TYPES_H


/*
 * Media types of a track. 
 */
typedef enum
{    
    MEDIA_TYPE_UNKNOWN = 0,
    MEDIA_VIDEO,
    MEDIA_AUDIO,
    MEDIA_TEXT, /* subtitle text or stand-alone application, string-based or bitmap-based */
    MEDIA_MIDI    
}MediaType;


#define UNKNOWN_CODEC_TYPE 0
#define UNKNOWN_CODEC_SUBTYPE 0 /*  Value 0 is reserved for unknown subtypes or no subtypes. */

/*
 * Video codec types. 
 */
typedef enum
{    
    VIDEO_TYPE_UNKNOWN = 0,
    VIDEO_UNCOMPRESSED, /* uncompressed video, every frame is a key frame */
    VIDEO_MPEG2, /* MPEG-2 video, ISO/IEC 13818-2 */
    VIDEO_MPEG4, /* MPEG-4 video, ISO/IEC 14496-2 */
    VIDEO_MS_MPEG4, /* Microsoft MPEG-4 video*/   
    VIDEO_H263, /* ITU-T H.263 */
    VIDEO_H264, /* H.264, ISO/IEC 14496-10 */
    VIDEO_MJPG, /* Motion-JPEG (M-JPEG) is a variant of the ISO JPEG specification 
                for use with digital video streams.
                Instead of compressing an entire image into a single bitstream, 
                Motion-JPEG compresses each video field separately, returning 
                the resulting JPEG bitstreams consecutively in a single frame.*/
    VIDEO_DIVX, /* DivX video types*/
    VIDEO_XVID,
    VIDEO_WMV,    
    VIDEO_SORENSON_H263, /* Sorenson Spark. Deprecated value. 
                            Please use type "VIDEO_SORENSON" and subtype "VIDEO_SORENSON_SPARK.*/
    VIDEO_FLV_SCREEN, /* Screen videos, by Adobe */    
    VIDEO_ON2_VP, /* True Motion video types by On2 */
    VIDEO_REAL, /* Real video types */
    VIDEO_JPEG, /* ISO JPEG still image */
    VIDEO_SORENSON, /* Sorenson video types, including Sorenson Spark, SVQ1, SVQ3 etc */
    VIDEO_HEVC,
    VIDEO_AVS
}VideoCodecType;


/*
 * Audio codec types. 
 */
typedef enum
{    
    AUDIO_TYPE_UNKNOWN = 0,
    AUDIO_PCM, /* Linear PCM, little-endian or big-endian */
    AUDIO_PCM_ALAW,
    AUDIO_PCM_MULAW,
    AUDIO_ADPCM,
    AUDIO_MP3,  /* MPEG-1/2 Layer 1,2,3 */  
    AUDIO_AAC,   /* MPEG-4 AAC, 14496-3 */
    AUDIO_MPEG2_AAC, /* MPEG-2 AAC, 13818-7 */
    AUDIO_AC3,
    AUDIO_WMA,    
    AUDIO_AMR,  /* Adaptive Multi-Rate audio */  
    AUDIO_DTS,
    AUDIO_VORBIS,
    AUDIO_FLAC,
    AUDIO_NELLYMOSER,
    AUIDO_SPEEX,
    AUDIO_REAL, /* Real audio types */
    AUDIO_EC3,
    AUDIO_OPUS,
    AUDIO_APE,   /*Monkey's audio*/
    AUDIO_WMS  /* Windows Media Voice */
}AudioCodecType;


/*
 * text types. 
 */
typedef enum
{    
    TXT_TYPE_UNKNOWN = 0,
    TXT_3GP_STREAMING_TEXT, /* 3GP streaming text, timed code, string-based */
    TXT_DIVX_FEATURE_SUBTITLE, /* DivX feature subtitle, bitmap-based */
    TXT_DIVX_MENU_SUBTITLE, /* DivX menu subtitle, bitmap-based */
    
    //TXT_QT_TIMECODE,      /* Quicktime timed code */
    TXT_QT_TEXT,            /* Quicktime text */
    TXT_SUBTITLE_SSA,       /* SubStation Alpha */
    TXT_SUBTITLE_ASS,       /* Advanced SubStation Alpha */
    TXT_SUBTITLE_TEXT                 
}TextType;


/*******************************************************************************
 *  Video Subtypes.
 *******************************************************************************/
typedef enum
{
    VIDEO_DIVX3 = 1,    /* version 3*/
    VIDEO_DIVX4,    /* version 4*/
    VIDEO_DIVX5_6 /* version 5 & 6*/
    
}DivXVideoTypes; /* DivX video types */

typedef enum
{
    VIDEO_MS_MPEG4_V2 = 1, /* Microsoft MPEG-4 video version 2, fourcc 'mp42'*/
    VIDEO_MS_MPEG4_V3 /* Microsoft MPEG-4 video version 3, fourcc 'mp43' */
    
}MsMPEG4VideoTypes; /* Microsoft MPEG-4 video types */


typedef enum
{
    MPEG4_VIDEO_AS_PROFILE  = 1 /* Fourcc 'RMP4', MPEG-4 AS profile */
    
}MPEG4VideoTypes; /* Microsoft MPEG-4 video types */


typedef enum
{
    VIDEO_WMV7 = 1,
    VIDEO_WMV8,
    VIDEO_WMV9,
    VIDEO_WMV9A,    /* Windows Media Video 9 Advanced Profile. The codec originally submitted for consideration as SMPTE VC1. 
                    This is not VC1 compliant and is no longer supported by Microsoft */
                    
    VIDEO_WVC1      /* Microsoft's implementation of the SMPTE VC1 codec */
    
}WMVVideoTypes; /* WMV video types */


typedef enum
{
    FLV_SCREEN_VIDEO = 1, /* Screen video version 1*/
    FLV_SCREEN_VIDEO_2  /* Screen video version 2 */
    
}ScreenVideoTypes;  /* Screen video types by Adobe*/


typedef enum
{
    VIDEO_VP6 = 1,
    VIDEO_VP6A,
    VIDEO_VP7,
    VIDEO_VP8,
    VIDEO_VP9
}On2VideoTypes; /* On2 video types */


typedef enum
{
    REAL_VIDEO_RV10 = 1,
    REAL_VIDEO_RV20,   
    REAL_VIDEO_RV30,
    REAL_VIDEO_RV40
}RealVideoTypes;

/*
There are two flavors of Motion-JPEG currently in use. These two formats differ 
based on their use of markers.
Motion-JPEG format A supportsmarkers;Motion-JPEG format B does not.
Each field of Motion-JPEG format A fully complies with the ISO JPEG specification, 
and therefore supports application markers.*/
typedef enum
{
    VIDEO_MJPEG_FORMAT_A = 1, /* Motion-JPEG(format A), support markers */
    VIDEO_MJPEG_FORMAT_B,      /* Motion-JPEG(format B), not support markers */
    VIDEO_MJPEG_2000

}MotionJPEGVideoTypes;



typedef enum
{
    VIDEO_SORENSON_SPARK = 1, /* Sorenson H.263, almost H.263 but not standard */
    VIDEO_SVQ1, /* Sorenson Video 1, a custom beast */
    VIDEO_SVQ3  /* Sorenson Video 3, SVQ3 is quite similar to H.264, not H.263 */
    
}SorensonVideoTypes;


/*******************************************************************************
 *  Audio Subtypes
 *******************************************************************************/

typedef enum
{
    REAL_AUDIO_SIPR = 1,
    REAL_AUDIO_COOK,    
    REAL_AUDIO_ATRC,
    REAL_AUDIO_RAAC,
}RealAudioTypes;

typedef enum
{
    AUDIO_WMA1 = 1,
    AUDIO_WMA2,
    AUDIO_WMA3,
    AUDIO_WMALL
    
}WMAAudioTypes; 

typedef enum
{
    AUDIO_AMR_NB = 1, /* Adaptive Multi-Rate - narrow band */
    AUDIO_AMR_WB, /* Adaptive Multi-Rate - Wideband */
    AUDIO_AMR_WB_PLUS   /* Extended Adaptive Multi-Rate - Wideband */
    
}AmrAudioTypes;


typedef enum
{  
    AUDIO_PCM_U8 = 1,   /* PCM, unsigned, 8 pits per sample */
    AUDIO_PCM_S16LE,    /* PCM, signed little-endian, 16 bits per sample */
    AUDIO_PCM_S24LE,    /* PCM, signed little-endian, 24 bits per sample */
    AUDIO_PCM_S32LE,    /* PCM, signed little-endian, 32 bits per sample */

    AUDIO_PCM_S16BE,    /* PCM, signed big-endian, 16 bits per sample */
    AUDIO_PCM_S24BE,    /* PCM, signed big-endian, 24 bits per sample */
    AUDIO_PCM_S32BE,     /* PCM, signed big-endian, 32 bits per sample */

    AUDIO_PCM_DVD,    /* PCM, dvd, 24 bits per sample */
    AUDIO_PCM_S8
}PCMAudioTypes;


typedef enum
{
    AUDIO_IMA_ADPCM = 1,  /* IMA 4:1 */
    AUDIO_ADPCM_MS = 2,  /* Microsoft ADPCM audio */
    AUDIO_ADPCM_QT

}ADPCMAudioTypes;

typedef enum
{
    AUDIO_ER_BSAC = 1, /* In fact we only care about whether it's BSAC or not */
    AUDIO_AAC_RAW = 2, /* ADTS without sync word, such as in mp4 container */
    AUDIO_AAC_ADTS = 3,
    AUDIO_AAC_ADIF = 4
}AACAudioTypes;



#endif /* _FSL_MMLAYER_MEDIA_TYPES_H */

