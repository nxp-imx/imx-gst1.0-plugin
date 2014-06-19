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

#ifndef CIP_H
#define CIP_H
#include "fsl_types.h"

// IEEE Std 1722-2011 6.2.6 IEC 61883 encapsulation: CIP header
#define CIP_HEADER_SIZE        (8)//when SPH = 0

#define CIP_DEFAULT_EOH1    (0)
#define CIP_DEFAULT_SID     (63)
#define CIP_DEFAULT_DBS     (6)
#define CIP_DEFAULT_FN      (0)
#define CIP_DEFAULT_QPC     (0)
#define CIP_DEFAULT_SPH     (0)
#define CIP_DEFAULT_RSV     (0)
#define CIP_DEFAULT_DBC     (0)
#define CIP_DEFAULT_EOH2    (2)
#define CIP_DEFAULT_FMT     (0x0)//according to 61883-1:2003
#define CIP_DEFAULT_FDF     (0x00)//according to 61883-6
#define CIP_DEFAULT_SYT     (0x0)

#define CIP_FMT_AUDIO     (0x10)//according to 61883-6
#define CIP_FMT_MPEGTS     (0x20)//according to 61883-4


//1IEEE Std 1722-2011 CIP Header
typedef struct
{
    uint8 SID;
    uint8 DBS;
    uint8 FN_QPC_SPH;
    uint8 DBC;
    uint8 FMT;
    uint8 FDF;
    uint8 SYT[2];
}CIP_HEADER;

#define SET_CIP_1QI(header, a)  (header->SID |= (a & 0x3) << 6)
#define SET_CIP_SID(header, a)   (header->SID |= (a & 0x3F))
#define SET_CIP_DBS(header, a)   (header->DBS = a)
#define SET_CIP_FN(header, a)    (header->FN_QPC_SPH |= (a & 0x3) << 6)
#define SET_CIP_QPC(header, a)   (header->FN_QPC_SPH |= (a & 0x7) << 3)
#define SET_CIP_SPH(header, a)   (header->FN_QPC_SPH |= (a & 0x1) << 2)
#define SET_CIP_RSV(header, a)   (header->FN_QPC_SPH |= (a & 0x3))
#define SET_CIP_DBC(header, a)   (header->DBC = (a & 0xFF))

#define SET_CIP_2QI(header, a)  (header->FMT |= (a & 0x3) << 6)
#define SET_CIP_FMT(header, a)   (header->FMT |= (a & 0x3F))
#define SET_CIP_FDF(header, a)   (header->FDF = a)
#define SET_CIP_SYT(header, a)   do {header->SYT[0] = a >> 8; \
                                    header->SYT[1] = a & 0xFF; } while (0)

#define GET_CIP_1QI(header)  ((header->SID >> 6)& 0x3)
#define GET_CIP_SID(header)  (header->SID & 0x3F)
#define GET_CIP_DBS(header)   (header->DBS)
#define GET_CIP_FN(header)   ((header->FN_QPC_SPH >> 6) & 0x3)
#define GET_CIP_QPC(header)   ((header->FN_QPC_SPH >> 3) & 0x7)
#define GET_CIP_SPH(header)   ((header->FN_QPC_SPH >> 2) & 0x1)
#define GET_CIP_DBC(header)   (header->DBC)

#define GET_CIP_2QI(header)   ((header->FMT >> 6) & 0x3)
#define GET_CIP_FMT(header)   (header->FMT & 0x3F)
#define GET_CIP_FDF(header)   (header->FDF)
#define GET_CIP_SYT(header)   (( header->SYT[0] << 8) | \
                                                  header->SYT[1])



void CIP_Header_Init(CIP_HEADER * header);

#endif
