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

#include "ethernet.h"

void Ethernet_Header_Init(ETHERNET_HEADER * header)
{

  uint8 desc_addr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  int i;

  if(header){
    memset(header, 0, sizeof(ETHERNET_HEADER));

    for( i = 0; i < MAC_LEN; i++){
      header->DA[i] = desc_addr[i];
    }


    SET_ETHERNET_TPID(header,ETHERNET_DEFAULT_TPID);
    SET_ETHERNET_PCP(header, ETHERNET_DEFAULT_PCP);
    SET_ETHERNET_CFI(header, ETHERNET_DEFAULT_CFI);
    SET_ETHERNET_VID(header, ETHERNET_DEFAULT_VID);
    SET_ETHERNET_AVTP_ETYPE(header,ETHERNET_ETYPE);

  }
}
void Ethernet_Set_SA(ETHERNET_HEADER * header, uint8 * addr)
{
  int i;
  if(header == NULL || addr == NULL)
      return NULL;

  for( i = 0; i < MAC_LEN; i++){
    header->SA[i] = addr[i];
  }

}
void Ethernet_Get_SA(ETHERNET_HEADER * header, uint8 * addr)
{
  int i;

  if(header == NULL || addr == NULL)
      return NULL;

  for( i = 0; i < MAC_LEN; i++){
    addr[i] = header->SA[i];
  }

}

void Ethernet_Set_DA(ETHERNET_HEADER * header, uint8 * addr)
{
  int i;

  if(header == NULL || addr == NULL)
      return NULL;

  for( i = 0; i < MAC_LEN; i++){
    header->DA[i] = addr[i];
  }

}
int Is_Valid_Ethernet_Header(uint8 * data)
{
  ETHERNET_HEADER * header = NULL;
  int ret = -1;

  if(data == NULL)
    return ret;

  header = (ETHERNET_HEADER *)data;

  if(header->TPID[0] == 0x81 && header->TPID[1] == 0x0
    && header->AVTP_Etype[0] == 0x22 && header->AVTP_Etype[1] == 0xf0){
    return 0;
  }
}

