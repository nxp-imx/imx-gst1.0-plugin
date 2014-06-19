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

#include "cip.h"

void CIP_Header_Init(CIP_HEADER * header)
{
    if(header){
        memset(header, 0, sizeof(CIP_HEADER));

        SET_CIP_1QI(header,CIP_DEFAULT_EOH1);
        SET_CIP_SID(header,CIP_DEFAULT_SID);
        SET_CIP_DBS(header, CIP_DEFAULT_DBS);
        SET_CIP_FN(header, CIP_DEFAULT_FN);
        SET_CIP_QPC(header, CIP_DEFAULT_QPC);
        SET_CIP_SPH(header, CIP_DEFAULT_SPH);
        SET_CIP_RSV(header, CIP_DEFAULT_RSV);
        SET_CIP_DBC(header, CIP_DEFAULT_DBC);

        SET_CIP_2QI(header, CIP_DEFAULT_EOH2);
        SET_CIP_FMT(header, CIP_DEFAULT_FMT);
        SET_CIP_FDF(header, CIP_DEFAULT_FDF);
        SET_CIP_SYT(header, CIP_DEFAULT_SYT);

    }
}
