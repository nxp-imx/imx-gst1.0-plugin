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

#include "avtp.h"

void AVTPDU_Header_Init(AVTPDU_DATA_HEADER * header)
{
    if(header){
        memset(header, 0, sizeof(AVTPDU_DATA_HEADER));

        SET_AVTPDU_CD(header, AVTPDU_DEFAULT_CD);
        SET_AVTPDU_SUBTYPE(header, AVTPDU_DEFAULT_SUBTYPE);
        SET_AVTPDU_SV(header, AVTPDU_DEFAULT_SV);
        SET_AVTPDU_VERSION(header, AVTPDU_DEFAULT_VERTION);
        SET_AVTPDU_MR(header, AVTPDU_DEFAULT_MR);
        SET_AVTPDU_R(header, AVTPDU_DEFAULT_R);
        SET_AVTPDU_GV(header, AVTPDU_DEFAULT_GV);
        SET_AVTPDU_TV(header, AVTPDU_DEFAULT_TV);

        SET_AVTPDU_SEQUENCE_NUM(header, AVTPDU_DEFAULT_SEQUENCE_NUM);
        SET_AVTPDU_RESERVE(header, AVTPDU_DEFAULT_RESERVE);
        SET_AVTPDU_TU(header, AVTPDU_DEFAULT_TU);

        SET_AVTPDU_STREAM_ID0(header, 0);
        SET_AVTPDU_STREAM_ID1(header, 0);
        SET_AVTPDU_AVTP_TS(header, 0);
        SET_AVTPDU_GATEWAY_INFO(header, 0);
        SET_AVTPDU_STREAM_DATA_LEN(header, 8);

        SET_ACTPDU_TAG(header, AVTPDU_DEFAULT_TAG);
        SET_ACTPDU_CHANNEL(header, AVTPDU_DEFAULT_CHANNEL);
        SET_ACTPDU_TCODE(header, AVTPDU_DEFAULT_TCODE);
        SET_ACTPDU_SY(header, AVTPDU_DEFAULT_SY);
    }
}
int Is_Valid_AVTPDU_Header(uint8 * data)
{
  int ret = -1;
  AVTPDU_DATA_HEADER * header;

  bool dataIndicator = TRUE;
  bool b61883 = TRUE;
  bool version = TRUE;
  bool dataLen = TRUE;
  bool tag = TRUE;
  bool channel = TRUE;
  bool tcode = TRUE;

  header = (AVTPDU_DATA_HEADER * )data;

  if(header){
    if(GET_AVTPDU_CD(header) != AVTPDU_CD_DATA)
      dataIndicator = FALSE;
    if(GET_AVTPDU_SUBTYPE(header) != IIDC_SUBTYPE_61883)
      b61883 = FALSE;
    if(GET_AVTPDU_VERSION(header) != AVTPDU_DEFAULT_VERTION)
      version = FALSE;

    if(GET_AVTPDU_STREAM_DATA_LEN(header) < 2)
      dataLen = FALSE;

    if(GET_ACTPDU_TAG(header) != AVTPDU_DEFAULT_TAG)
      tag = FALSE;

    if(GET_ACTPDU_CHANNEL(header) != AVTPDU_DEFAULT_CHANNEL)
      channel = FALSE;

    if(GET_ACTPDU_TCODE(header) != AVTPDU_DEFAULT_TCODE)
      tcode = FALSE;

    if(dataIndicator && b61883 && version && dataLen
      && tag && channel && tcode)
      ret = 0;
  }

  return ret;
}

