/*
 * Copyright (c) 2013-2016, Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef __IMX_COMMON_H__
#define __IMX_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <gst/gst.h>
#include <string.h>

#define IMX_GST_PLUGIN_AUTHOR "Multimedia Team <shmmmw@freescale.com>"
#define IMX_GST_PLUGIN_PACKAGE_NAME "Freescle Gstreamer Multimedia Plugins"
#define IMX_GST_PLUGIN_PACKAGE_ORIG "http://www.freescale.com"
#define IMX_GST_PLUGIN_LICENSE "LGPL"

#define IMX_GST_PLUGIN_RANK (GST_RANK_PRIMARY+1)

#define IMX_GST_PLUGIN_DEFINE(name, description, initfunc)\
  GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,\
      GST_VERSION_MINOR,\
      name.imx,\
      description,\
      initfunc,\
      VERSION,\
      IMX_GST_PLUGIN_LICENSE,\
      IMX_GST_PLUGIN_PACKAGE_NAME, IMX_GST_PLUGIN_PACKAGE_ORIG)

#define CHIPCODE(a,b,c,d)( (((unsigned int)((a)))<<24) | (((unsigned int)((b)))<<16)|(((unsigned int)((c)))<<8)|(((unsigned int)((d)))))
typedef enum
{
  CC_MX23 = CHIPCODE ('M', 'X', '2', '3'),
  CC_MX25 = CHIPCODE ('M', 'X', '2', '5'),
  CC_MX27 = CHIPCODE ('M', 'X', '2', '7'),
  CC_MX28 = CHIPCODE ('M', 'X', '2', '8'),
  CC_MX31 = CHIPCODE ('M', 'X', '3', '1'),
  CC_MX35 = CHIPCODE ('M', 'X', '3', '5'),
  CC_MX37 = CHIPCODE ('M', 'X', '3', '7'),
  CC_MX50 = CHIPCODE ('M', 'X', '5', '0'),
  CC_MX51 = CHIPCODE ('M', 'X', '5', '1'),
  CC_MX53 = CHIPCODE ('M', 'X', '5', '3'),
  CC_MX6Q = CHIPCODE ('M', 'X', '6', 'Q'),
  CC_MX60 = CHIPCODE ('M', 'X', '6', '0'),
  CC_MX6SL = CHIPCODE ('M', 'X', '6', '1'),
  CC_MX6SX = CHIPCODE ('M', 'X', '6', '2'),
  CC_MX6UL = CHIPCODE ('M', 'X', '6', '3'),
  CC_MX6SLL = CHIPCODE ('M', 'X', '6', '4'),
  CC_MX7D = CHIPCODE ('M', 'X', '7', 'D'),
  CC_MX7ULP = CHIPCODE ('M', 'X', '7', 'U'),
  CC_MX8 = CHIPCODE ('M', 'X', '8', '0'),
  CC_UNKN = CHIPCODE ('U', 'N', 'K', 'N')

} CHIP_CODE;

typedef struct {
  CHIP_CODE code;
  int chip_num;
} CPU_INFO;

typedef struct {
  CHIP_CODE code;
  char *name;
} SOC_INFO;

typedef struct {
  CHIP_CODE chip_name;
  gboolean g3d;
  gboolean g2d;
  gboolean ipu;
  gboolean pxp;
  gboolean vpu;
  gboolean dpu;
} IMXV4l2FeatureMap;

typedef enum {
  G3D = 1,
  G2D,
  IPU,
  PXP,
  VPU,
  DPU
} CHIP_FEATURE;

CHIP_CODE getChipCodeFromCpuinfo (void);
CHIP_CODE getChipCodeFromSocid (void);
CHIP_CODE imx_chip_code (void);
gboolean check_feature(CHIP_CODE chip_name, CHIP_FEATURE feature);

#define HAS_G3D() check_feature(imx_chip_code(), G3D)
#define HAS_G2D() check_feature(imx_chip_code(), G2D)
#define HAS_IPU() check_feature(imx_chip_code(), IPU)
#define HAS_PXP() check_feature(imx_chip_code(), PXP)
#define HAS_VPU() check_feature(imx_chip_code(), VPU)
#define HAS_DPU() check_feature(imx_chip_code(), DPU)

/* define rotate and flip glib enum for overlaysink and imxv4l2sink */
typedef enum
{
  GST_IMX_ROTATION_0 = 0,
  GST_IMX_ROTATION_90,
  GST_IMX_ROTATION_180,
  GST_IMX_ROTATION_270,
  GST_IMX_ROTATION_HFLIP,
  GST_IMX_ROTATION_VFLIP
}GstImxRotateMethod;

GType gst_imx_rotate_method_get_type();

#define DEFAULT_IMX_ROTATE_METHOD GST_IMX_ROTATION_0
#define GST_TYPE_IMX_ROTATE_METHOD (gst_imx_rotate_method_get_type())

unsigned long phy_addr_from_fd(int dmafd);
unsigned long phy_addr_from_vaddr(void *vaddr, int size);

#endif
