
/************************************************************************
* Copyright 2005-2009, 2014 by Freescale Semiconductor, Inc.

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

/************************************************************************
* ANSI C source code
*
* Project Name : MP3 Encoder
*
***************************************************************************/
/***************************************************************************
 *   CHANGE HISTORY
 *   dd/mm/yy   Code Ver     Description                   Author
 *   --------   -------      -----------                   ------
 *   Aug 07    0.1 		created file
 **************************************************************************/
/*********************************************************************************************
 *   CHANGE HISTORY
 *   dd/mm/yy   Code Ver     Description                   Author
 *   --------   -------      -----------                   ------
 *   27/08/07    0.1         created file
 *   07/09/07    0.2         made changes to accomodate
 *                           new api functions             Wang Qinling
 *   21/05/08    0.3         update api function           Huang Shen
 *********************************************************************************************/
#ifndef __MP3_ENC_INTERFACE_H_
#define __MP3_ENC_INTERFACE_H_

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

#define ENC_NUM_MEM_BLOCKS                6
#define MP3E_INPUT_BUFFER_SIZE            1152
/* Data type */
typedef unsigned char     MP3E_UINT8;
typedef char              MP3E_INT8;
typedef unsigned short    MP3E_UINT16;
typedef short             MP3E_INT16;
typedef unsigned int      MP3E_UINT32;
typedef int               MP3E_INT32;

typedef enum
{
    MP3E_16_BIT_INTPUT,            /* 16-bit input format */
}MP3E_INPUT_FORMAT;

typedef enum
{
    MP3E_SUCCESS = 0,              /* Successful initialization */
    MP3E_ERROR_INIT_BITRATE,       /* If the bitrate passed by the application to the init
                                   routine is invalid */
    MP3E_ERROR_INIT_SAMPLING_RATE, /* If the sampling rate passed by the application to the
                                   init routine is invalid */
    MP3E_ERROR_INIT_MODE,          /* If the stereo mode passed by the application to the init
                                   routine is invalid */
    MP3E_ERROR_INIT_FORMAT,        /* If the input format passed by the application to the init
                                   routine is invalid */
    MP3E_ERROR_INIT_QUALITY,       /* If the value of quality passed by the application to the init
                                   routine is invalid */
    MP3E_ERROR_INIT_QUERY_MEM      /* If the call to query_mem_mp3e() is unsuccessful */
}MP3E_RET_VAL;

typedef enum
{
    FAST_STATIC_MEMORY = 0,  /* Fast Static memory (state) */
    SLOW_STATIC_MEMORY,      /* Slow Static memory (state) */
    FAST_SCRATCH_MEMORY,     /* Fast Scratch memory */
    SLOW_SCRATCH_MEMORY      /* Slow Scratch memory */
}MP3E_MEM_DESC;

typedef struct
{
    MP3E_INT32 app_sampling_rate;     /* sampling rate of the input file in Hz. The following
                                      sampling rates are possible: 32000, 44100 and 48000.
                                      This parameter needs to be filled by the
                                      application.*/
    MP3E_INT32 app_bit_rate;          /* bit rate for encoding, in kbps. The following bit
                                      rates are possible: 32, 40, 48, 56, 64, 80, 96, 112,
                                      128, 160, 192, 224, 256, 320 kbps. This parameter
                                      needs to be filled by the application */
    MP3E_INT32 app_mode;              /* mode for the encoder. The various modes are defined by
                                      different bit fields of this 32-bit word.
                                      This parameter needs to be filled by the application.
                                      The following bits are used:
                                      b1-b0: Stereo mode bits
                                      Two values are currently possible:
                                      00: stereo mode is joint stereo
                                      01: stereo mode is mono
                                      b9-b8: Input format bit
                                      00: Input format is L/R interleaved
                                      01: Input format is with contiguous L samples,
                                      followed by contiguous R samples
                                      b17-b16: Input quality bits
                                      00: Low quality
                                      01: High quality
                                      Other bits are reserved. */
    MP3E_INT32 mp3e_outbuf_size;      /* size of the required output buffer in bytes.
                                      The MP3 encoder will fill this parameter and return.
                                      The application has to allocate an output buffer of
                                      this size or more. The maximum value that can be
                                      returned by the MP3 encoder for this output buffer
                                      size is 1440 bytes */
}MP3E_Encoder_Parameter;

typedef struct
{
    MP3E_MEM_DESC type;           /* Memory block type (Fast or Slow) */
    MP3E_INT32 size;              /* Memory block size */
    MP3E_INT32 align;             /* Memory block alignment in bytes */
    MP3E_INT32 *ptr;              /* Memory block pointer */
}MP3E_Mem_Alloc_Info;


typedef struct
{
    MP3E_INT32 instance_id;
    MP3E_Mem_Alloc_Info mem_info[ENC_NUM_MEM_BLOCKS];
    MP3E_INT32 num_bytes;
}MP3E_Encoder_Config;

EXTERN MP3E_RET_VAL mp3e_query_mem (MP3E_Encoder_Config *enc_config);

EXTERN MP3E_RET_VAL mp3e_encode_init (MP3E_Encoder_Parameter *params, MP3E_Encoder_Config *enc_config);

EXTERN void mp3e_encode_frame (MP3E_INT16 *inbuf, MP3E_Encoder_Config *enc_config, MP3E_INT8 *outbuf);

EXTERN void mp3e_flush_bitstream ( MP3E_Encoder_Config *enc_config,MP3E_INT8 *outbuf);

EXTERN const char *MP3ECodecVersionInfo (void);

#endif
