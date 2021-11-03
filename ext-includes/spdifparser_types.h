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

#ifndef _SPDIF_PARSER_TYPES_H
#define _SPDIF_PARSER_TYPES_H
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//#include "stdio.h"
/* data type definition */
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
typedef int             int32_t;

typedef enum
{
    SPDIF_OK                    = 0,
    SPDIF_ERR_PARAM             = -1,
    SPDIF_ERR_INSUFFICIENT_DATA = -2,
    SPDIF_ERR_HEADER            = -3,
    SPDIF_ERR_IEC937_PA         = -4,
    SPDIF_ERR_READ_LEN          = -5,
    SPDIF_ERR_IEC958_PREAMBLE   = -6,
    SPDIF_ERR_UNREGISTER_FUN    = -7,
    SPDIF_EOS                   = -8,
    SPDIF_ERR_INCOMPLETE        = -9,
    SPDIF_ERR_UNKNOWN           = -10,
}SPDIF_RET_TYPE;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

