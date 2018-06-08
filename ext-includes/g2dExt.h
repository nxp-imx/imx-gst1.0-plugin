/*
 *  Copyright (C) 2013-2015 Freescale Semiconductor, Inc.
 *  Copyright 2018 NXP
 */
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
 *  g2dExt.h
 *  g2dExt.h is a extension to g2d.h. It's private and should not be exported to customer.
 *  So any extension which is private should be here, others will still be updated in g2d.h.
 *  History :
 *  Date(y.m.d)        Author            Version        Description
 *
*/

#ifndef __G2DEXT_H__
#define __G2DEXT_H__

#include "g2d.h"

#ifdef __cplusplus
extern "C"  {
#endif

enum g2d_tiling
{
    G2D_LINEAR              = 0x1,
    G2D_TILED               = 0x2,
    G2D_SUPERTILED          = 0x4,
    G2D_AMPHION_TILED       = 0x8,
    G2D_AMPHION_INTERLACED  = 0x10,
};

struct g2d_surfaceEx
{
    struct g2d_surface base;
    enum g2d_tiling tiling;
};

int g2d_blitEx(void *handle, struct g2d_surfaceEx *srcEx, struct g2d_surfaceEx *dstEx);
int g2d_blitEx_dispatch(void *handle, struct g2d_surfaceEx *srcEx, struct g2d_surfaceEx *dstEx, int imxdpu_id);

int g2d_set_clipping(void *handle, int left, int top, int right, int bottom);

#ifdef __cplusplus
}
#endif

#endif
