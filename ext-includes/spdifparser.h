/*
 * Copyright 2021 NXP
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __SPDIFPARSER_H__
#define __SPDIFPARSER_H__
#ifdef __cplusplus

extern "C" {
#endif /* __cplusplus */

#include "spdifparser_types.h"

/* log print configuration */
#define SPDIF_LOG_PRINTF                    //printf

#define SPDIF_PARSER_IEC958_BLOCK_BYTE_LEN  (192 << 3)

typedef enum
{
    SPDIF_AUTO_PARSER_MODE      = 0,
    SPDIF_IEC937_PARSER_MODE    = 1,
    SPDIF_IEC958_PARSER_MODE    = 2,
}SPDIF_PARSER_MODE;

typedef enum
{
    SPDIF_AUDIO_FORMAT_UNSET    = 0,
    SPDIF_AUDIO_FORMAT_COMPRESS = 1,
    SPDIF_AUDIO_FORMAT_PCM      = 2,
    SPDIF_AUDIO_FORMAT_UNKNOWN  = 0xFF,
} SPDIF_AUDIO_FORMAT_TYPE;

typedef enum
{
    SPDIF_IEC937_FORMAT_TYPE_NULL           = 0x00,
    SPDIF_IEC937_FORMAT_TYPE_AC3            = 0x01,
    SPDIF_IEC937_FORMAT_TYPE_EAC3           = 0x15,
    SPDIF_IEC937_FORMAT_TYPE_MPEG1L1        = 0x4,
    SPDIF_IEC937_FORMAT_TYPE_MPEG1L23       = 0x5,
    SPDIF_IEC937_FORMAT_TYPE_MPEG2          = 0x6,
    SPDIF_IEC937_FORMAT_TYPE_MPEG2L1        = 0x8,
    SPDIF_IEC937_FORMAT_TYPE_MPEG2L2        = 0x9,
    SPDIF_IEC937_FORMAT_TYPE_MPEG2L3        = 0xA,
    SPDIF_IEC937_FORMAT_TYPE_MPEG2_4_AAC    = 0x7,
    SPDIF_IEC937_FORMAT_TYPE_MPEG2_4_AAC_2  = 0x13,
    SPDIF_IEC937_FORMAT_TYPE_MPEG2_4_AAC_3  = 0x33
} SPDIF_IEC937_FORMAT_TYPE;

typedef struct 
{
    uint8_t valid_flag;
    SPDIF_AUDIO_FORMAT_TYPE audio_type;
    SPDIF_IEC937_FORMAT_TYPE iec937_type;
    uint32_t sample_rate;
    uint32_t data_length;
    uint32_t channel_num;
    uint32_t frame_size;
    uint32_t audio_size;
    uint32_t bit_rate;
    uint32_t sample_per_frame;
} spdif_audio_info_t;

typedef struct
{
    void* (*malloc)(uint32_t size);
    void (*free)(void *ptr);
}spdif_parser_memory_ops;

typedef void * spdif_parser_handle;

typedef enum
{
    SPDIF_PARSER_API_GET_VERSION_INFO               = 0,
    SPDIF_PARSER_API_OPEN                           = 1,
    SPDIF_PARSER_API_CLOSE                          = 2,
    SPDIF_PARSER_API_SET_MODE                       = 3,
    SPDIF_PARSER_API_SET_IEC958_TYPE                = 4,
    SPDIF_PARSER_API_GET_MODE                       = 5,
    SPDIF_PARSER_API_SEARCH_HEADER                  = 6,
    SPDIF_PARSER_API_GET_COMPRESS_AUDIO_FRAME_SIZE  = 7,
    SPDIF_PARSER_API_GET_COMPRESS_AUDIO_LEN         = 8,
    SPDIF_PARSER_API_READ                           = 9,
    SPDIF_PARSER_API_READ_WITH_SYNC                 = 10,
    SPDIF_PARSER_API_GET_AUDIO_INFO                 = 11,
    SPDIF_PARSER_API_GET_AUDIO_TYPE                 = 12,
    SPDIF_PARSER_API_GET_IEC937_TYPE                = 13,
    SPDIF_PARSER_API_GET_SAMPLE_RATE                = 14,
    SPDIF_PARSER_API_GET_CHANNEL_NUM                = 15,
    SPDIF_PARSER_API_GET_DATA_LENGTH                = 16,
}SPDIF_PARSER_API_TYPE;

typedef const char * (*spdif_parser_get_version_info_t)(void);
typedef SPDIF_RET_TYPE (*spdif_parser_open_t) (spdif_parser_handle *p_handle, spdif_parser_memory_ops *p_mem_ops);
typedef SPDIF_RET_TYPE (*spdif_parser_close_t) (spdif_parser_handle *p_handle);
typedef void (*spdif_parser_set_mode_t)(spdif_parser_handle handle, SPDIF_PARSER_MODE mode);
typedef void (*spdif_parser_set_iec958_type_t)(spdif_parser_handle handle, SPDIF_AUDIO_FORMAT_TYPE audio_type);
typedef SPDIF_PARSER_MODE (*spdif_parser_get_mode_t)(spdif_parser_handle handle);
typedef SPDIF_RET_TYPE (*spdif_parser_search_header_t)(spdif_parser_handle handle, uint8_t *p_buf, uint32_t len, uint32_t *p_out_pos);
typedef uint32_t (*spdif_parser_get_compress_audio_frame_size_t)(spdif_parser_handle handle);
typedef uint32_t (*spdif_parser_get_compress_audio_len_t)(spdif_parser_handle handle);
typedef SPDIF_RET_TYPE (*spdif_parser_read_t)(spdif_parser_handle handle, uint8_t*src, uint8_t *dst, uint32_t src_len, uint32_t *p_dst_len);
typedef SPDIF_RET_TYPE (*spdif_parser_read_with_sync_t)(spdif_parser_handle handle, uint8_t*src, uint8_t *dst, uint32_t src_len, uint32_t *p_dst_len, uint32_t *p_out_src_pos);
typedef void (*spdif_parser_get_audio_info_t)(spdif_parser_handle handle, spdif_audio_info_t *p_spdif_audio_info);
typedef SPDIF_AUDIO_FORMAT_TYPE (*spdif_parser_get_audio_type_t)(spdif_parser_handle handle);
typedef SPDIF_IEC937_FORMAT_TYPE (*spdif_parser_get_iec937_type_t)(spdif_parser_handle handle);
typedef uint32_t (*spdif_parser_get_sample_rate_t)(spdif_parser_handle handle);
typedef uint32_t (*spdif_parser_get_channel_num_t)(spdif_parser_handle handle);
typedef uint32_t (*spdif_parser_get_data_length_t)(spdif_parser_handle handle);
typedef SPDIF_RET_TYPE (*spdif_parser_query_interface_t)(uint32_t id, void ** func);

const char * spdif_parser_get_version_info(void);
SPDIF_RET_TYPE spdif_parser_open (spdif_parser_handle *p_handle, spdif_parser_memory_ops *p_mem_ops);
SPDIF_RET_TYPE spdif_parser_close (spdif_parser_handle *p_handle);
void spdif_parser_set_mode(spdif_parser_handle handle, SPDIF_PARSER_MODE mode);
void spdif_parser_set_iec958_type(spdif_parser_handle handle, SPDIF_AUDIO_FORMAT_TYPE audio_type);
SPDIF_PARSER_MODE spdif_parser_get_mode(spdif_parser_handle handle);
SPDIF_RET_TYPE spdif_parser_search_header(spdif_parser_handle handle, uint8_t *p_buf, uint32_t len, uint32_t *p_out_pos);
uint32_t spdif_parser_get_compress_audio_frame_size(spdif_parser_handle handle);
uint32_t spdif_parser_get_compress_audio_len(spdif_parser_handle handle);
SPDIF_RET_TYPE spdif_parser_read(spdif_parser_handle handle, uint8_t*src, uint8_t *dst, uint32_t src_len, uint32_t *p_dst_len);
SPDIF_RET_TYPE spdif_parser_read_with_sync(spdif_parser_handle handle, uint8_t*src, uint8_t *dst, uint32_t src_len, uint32_t *p_dst_len, uint32_t *p_out_src_pos);
void spdif_parser_get_audio_info(spdif_parser_handle handle, spdif_audio_info_t *p_spdif_audio_info);
SPDIF_AUDIO_FORMAT_TYPE spdif_parser_get_audio_type(spdif_parser_handle handle);
SPDIF_IEC937_FORMAT_TYPE spdif_parser_get_iec937_type(spdif_parser_handle handle);
uint32_t spdif_parser_get_sample_rate(spdif_parser_handle handle);
uint32_t spdif_parser_get_channel_num(spdif_parser_handle handle);
uint32_t spdif_parser_get_data_length(spdif_parser_handle handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

