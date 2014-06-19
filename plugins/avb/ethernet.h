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

#ifndef ETHERNET_H
#define ETHERNET_H
#include "fsl_types.h"

#define MAC_LEN      (6)


#define ETHERNET_DEFAULT_TPID    (0x8100)
#define ETHERNET_DEFAULT_PCP    (0x3)
#define ETHERNET_DEFAULT_CFI    (0)
#define ETHERNET_DEFAULT_VID    (0x2)
#define ETHERNET_ETYPE   (0x22f0)


// IEEE 802.3 stream header
typedef struct
{
    uint8 DA[MAC_LEN];
    uint8 SA[MAC_LEN];
    uint8 TPID[2];//0x8100
    uint8 VID[2];
    uint8 AVTP_Etype[2];
}ETHERNET_HEADER;

typedef struct
{
    uint8 DA[MAC_LEN];
    uint8 SA[MAC_LEN];
    uint8 AVTP_Etype[2];
}ETHERNET_HEADER_WITHOUT_TPID;


#define SET_ETHERNET_TPID(header, a)          do{ header->TPID[0] = (a >> 8); \
                                                header->TPID[1] =  a & 0xFF; } while (0)
#define SET_ETHERNET_AVTP_ETYPE(header, a)     do{ header->AVTP_Etype[0] = (a >> 8); \
                                                header->AVTP_Etype[1] = a & 0xFF; } while (0)

#define SET_ETHERNET_PCP(header, a)  (header->VID[0] |= (a & 0x7) << 5)
#define SET_ETHERNET_CFI(header, a)  (header->VID[0] |= (a & 0x1) << 4)
#define SET_ETHERNET_VID(header, a)   do{ header->VID[0] |= (a & 0xF00) >> 8; \
                                        header->VID[1] = (a & 0xFF); } while (0)


#define GET_ETHERNET_TPID(header) (( header->TPID[0] << 8) | \
                                                  header->TPID[1])
void Ethernet_Header_Init(ETHERNET_HEADER * header);
void Ethernet_Set_SA(ETHERNET_HEADER * header, uint8 * addr);
void Ethernet_Get_SA(ETHERNET_HEADER * header, uint8 * addr);

void Ethernet_Set_DA(ETHERNET_HEADER * header, uint8 * addr);

int Is_Valid_Ethernet_Header(uint8 * data);


#endif
