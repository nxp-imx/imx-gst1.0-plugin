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

#ifndef AVTP_H
#define AVTP_H
#include "fsl_types.h"

#define AVTPDU_HEADER_SIZE  (24)

#define AVTPDU_CD_DATA      (0)
#define AVTPDU_CD_CONTROL   (1)
#define IIDC_SUBTYPE_61883  (0)
#define AVTPDU_SV_VALID     (1)
#define AVTPDU_SV_INVALID   (0)
#define AVTPDU_MR_RESTART   (1)
#define AVTPDU_MR_NORMAL    (0)

#define AVTPDU_GV_INVALID  (0)
#define AVTPDU_GV_VALID  (1)
#define AVTPDU_TV_INVALID  (0)
#define AVTPDU_TV_VALID  (1)



#define AVTPDU_DEFAULT_CD       AVTPDU_CD_DATA
#define AVTPDU_DEFAULT_SUBTYPE  IIDC_SUBTYPE_61883
#define AVTPDU_DEFAULT_SV       AVTPDU_SV_VALID
#define AVTPDU_DEFAULT_VERTION  (0)
#define AVTPDU_DEFAULT_MR       AVTPDU_MR_NORMAL
#define AVTPDU_DEFAULT_R        (0)
#define AVTPDU_DEFAULT_GV       AVTPDU_GV_INVALID
#define AVTPDU_DEFAULT_TV       AVTPDU_TV_INVALID
#define AVTPDU_DEFAULT_SEQUENCE_NUM       (0)
#define AVTPDU_DEFAULT_RESERVE            (0)
#define AVTPDU_DEFAULT_TU       (0)

#define AVTPDU_DEFAULT_TAG       (1)
#define AVTPDU_DEFAULT_CHANNEL   (31)
#define AVTPDU_DEFAULT_TCODE     (0xA)
#define AVTPDU_DEFAULT_SY        (0)


#define AVTPDU_GET_U32_TS(a)  (a & 0xFFFFFFFF)


// IEEE Std 1722-2011 5.2 AVTPDU common header format
typedef struct{
    uint8 subtype;
    uint8 version;
    uint8 sequence_num;
    uint8 tu;
    uint8 stream_id[8];
    uint8 avtp_timestamp[4];
    uint8 gateway_info[4];
    uint8 stream_data_length[2];//should be less than len of mtu
    uint8 tag;
    uint8 tcode;
} AVTPDU_DATA_HEADER;

#define SET_AVTPDU_CD(header, a)        (header->subtype |= a << 7)
#define SET_AVTPDU_SUBTYPE(header, a)   (header->subtype |= (a & 0x7F))
#define SET_AVTPDU_SV(header, a)        (header->version |= (a & 0x1) << 7)
#define SET_AVTPDU_VERSION(header, a)   (header->version |= (a & 0x7) << 4)
#define SET_AVTPDU_MR(header, a)        (header->version |= (a & 0x1) << 3)
#define SET_AVTPDU_R(header, a)         (header->version |= (a & 0x1) << 2)
#define SET_AVTPDU_GV(header, a)        (header->version |= (a & 0x1) << 1)
#define SET_AVTPDU_TV(header, a)        (header->version |= (a & 0x1))

#define SET_AVTPDU_SEQUENCE_NUM(header, a)        (header->sequence_num = (a & 0xFF))
#define SET_AVTPDU_RESERVE(header, a)             (header->tu |= (a & 0x7F) << 1)
#define SET_AVTPDU_TU(header, a)                  (header->tu |= (a & 0x1))
#define SET_AVTPDU_STREAM_ID0(header, a)       do {header->stream_id[0] = (a >> 24) & 0xFF; \
                                                 header->stream_id[1] = (a >> 16) & 0xFF; \
                                                 header->stream_id[2] = (a >> 8) & 0xFF; \
                                                 header->stream_id[3] = (a & 0xFF); } while (0)
#define SET_AVTPDU_STREAM_ID1(header, a)       do {header->stream_id[4] = ((a) >> 24) & 0xFF; \
                                                 header->stream_id[5] = (a >> 16) & 0xFF; \
                                                 header->stream_id[6] = (a >> 8) & 0xFF; \
                                                 header->stream_id[7] = (a & 0xFF); } while (0)

#define SET_AVTPDU_AVTP_TS(header, a)        do {header->avtp_timestamp[0] = (a >> 24) & 0xFF; \
                                                 header->avtp_timestamp[1] = (a >> 16) & 0xFF; \
                                                 header->avtp_timestamp[2] = (a >> 8) & 0xFF; \
                                                 header->avtp_timestamp[3] = (a & 0xFF); } while (0)


#define SET_AVTPDU_GATEWAY_INFO(header, a)     do {header->gateway_info[0] = (a >> 24) & 0xFF; \
                                                 header->gateway_info[1] = (a >> 16) & 0xFF; \
                                                 header->gateway_info[2] = (a >> 8) & 0xFF; \
                                                 header->gateway_info[3] = (a & 0xFF); } while (0)

#define SET_AVTPDU_STREAM_DATA_LEN(header, a)    do {header->stream_data_length[0] = a >> 8; \
                                                  header->stream_data_length[1] = a & 0xFF; } while (0)

#define SET_ACTPDU_TAG(header, a)                 (header->tag |= (a & 0x3) << 6)
#define SET_ACTPDU_CHANNEL(header, a)             (header->tag |= (a & 0x3F))
#define SET_ACTPDU_TCODE(header, a)               (header->tcode |= (a & 0xF) << 4)
#define SET_ACTPDU_SY(header, a)                  (header->tcode |= (a & 0xF))

#define GET_AVTPDU_CD(header)        ((header->subtype >> 7) & 0x1)
#define GET_AVTPDU_SUBTYPE(header)        (header->subtype & 0x7F)
#define GET_AVTPDU_VERSION(header)        ((header->subtype >> 4) & 0x7)

#define GET_AVTPDU_TV(header)       (header->version & 0x1)
#define GET_AVTPDU_SEQUENCE_NUM(header)        (header->sequence_num)
#define GET_AVTPDU_AVTP_TS(header)        ((header->avtp_timestamp[0] << 24) | \
                                            (header->avtp_timestamp[1] << 16) | \
                                            (header->avtp_timestamp[2] << 8) | \
                                            (header->avtp_timestamp[3]))
#define GET_AVTPDU_STREAM_DATA_LEN(header)   (( header->stream_data_length[0] << 8) | \
                                                  header->stream_data_length[1])

#define GET_ACTPDU_TAG(header)                 ((header->tag >> 6) & 0x3)
#define GET_ACTPDU_CHANNEL(header)                 (header->tag & 0x3F)
#define GET_ACTPDU_TCODE(header)                 ((header->tcode >> 4) & 0xF)


void AVTPDU_Header_Init(AVTPDU_DATA_HEADER * header);
int Is_Valid_AVTPDU_Header(uint8 * data);

#endif
