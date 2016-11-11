/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc. All rights reserved.
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

#include "gstimxcommon.h"

/*=============================================================================
FUNCTION:               get_chipname

DESCRIPTION:            To get chipname from /proc/cpuinfo

ARGUMENTS PASSED: STR of chipname

RETURN VALUE:            chip code
=============================================================================*/
//*

static CPU_INFO cpu_info[] = {
  {CC_MX23, 0x23},
  {CC_MX25, 0x25},
  {CC_MX27, 0x27},
  {CC_MX28, 0x28},
  {CC_MX31, 0x31},
  {CC_MX35, 0x35},
  {CC_MX37, 0x37},
  {CC_MX50, 0x50},
  {CC_MX51, 0x51},
  {CC_MX53, 0x53},
  {CC_MX6Q, 0x61},
  {CC_MX6Q, 0x63},
  {CC_MX60, 0x60}
};

CHIP_CODE getChipCodeFromCpuinfo (void)
{
  FILE *fp = NULL;
  char buf[100], *p, *rev;
  char chip_name[3];
  int len = 0, i;
  int chip_num = -1;
  CHIP_CODE cc = CC_UNKN;
  fp = fopen ("/proc/cpuinfo", "r");
  if (fp == NULL) {
    return cc;
  }
  while (!feof (fp)) {
    p = fgets (buf, 100, fp);
    p = strstr (buf, "Revision");
    if (p != NULL) {
      rev = index (p, ':');
      if (rev != NULL) {
        rev++;
        chip_num = strtoul (rev, NULL, 16);
        chip_num >>= 12;
        break;
      }
    }
  }

  fclose (fp);

  if (chip_num < 0) {
    return cc;
  }

  int num = sizeof(cpu_info) / sizeof(CPU_INFO);
  for(i=0; i<num; i++) {
    if(chip_num == cpu_info[i].chip_num) {
      cc = cpu_info[i].code;
      break;
    }
  }

  return cc;
}

static SOC_INFO soc_info[] = {
  {CC_MX23, "i.MX23"},
  {CC_MX25, "i.MX25"},
  {CC_MX27, "i.MX27"},
  {CC_MX28, "i.MX28"},
  {CC_MX31, "i.MX31"},
  {CC_MX35, "i.MX35"},
  {CC_MX37, "i.MX37"},
  {CC_MX50, "i.MX50"},
  {CC_MX51, "i.MX51"},
  {CC_MX53, "i.MX53"},
  {CC_MX6Q, "i.MX6DL"},
  {CC_MX6Q, "i.MX6Q"},
  {CC_MX6Q, "i.MX6QP"},
  {CC_MX6SL, "i.MX6SL"},
  {CC_MX6SLL, "i.MX6SLL"},
  {CC_MX6SX, "i.MX6SX"},
  {CC_MX6UL, "i.MX6UL"},
  {CC_MX6UL, "i.MX6ULL"},
  {CC_MX7D, "i.MX7D"},
  {CC_MX8, "i.MX8DV"},
};

CHIP_CODE getChipCodeFromSocid (void)
{
  FILE *fp = NULL;
  char soc_name[100];
  CHIP_CODE code = CC_UNKN;

  fp = fopen("/sys/devices/soc0/soc_id", "r");
  if (fp == NULL) {
    g_print("open /sys/devices/soc0/soc_id failed.\n");
    return  CC_UNKN;
  }

  if (fscanf(fp, "%100s", soc_name) != 1) {
    g_print("fscanf soc_id failed.\n");
    fclose(fp);
    return CC_UNKN;
  }
  fclose(fp);

  //GST_INFO("SOC is %s\n", soc_name);

  int num = sizeof(soc_info) / sizeof(SOC_INFO);
  int i;
  for(i=0; i<num; i++) {
    if(!strcmp(soc_name, soc_info[i].name)) {
      code = soc_info[i].code;
      break;
    }
  }

  return code;
}


#define KERN_VER(a, b, c) (((a) << 16) + ((b) << 8) + (c))

static CHIP_CODE gimx_chip_code = CC_UNKN;

CHIP_CODE imx_chip_code (void)
{
  struct utsname sys_name;
  int kv, kv_major, kv_minor, kv_rel;
  char soc_name[255];
  int rev_major, rev_minor;
  int idx, num;

  if (gimx_chip_code != CC_UNKN)
    return gimx_chip_code;

  if (uname(&sys_name) < 0) {
    g_print("get kernel version via uname failed.\n");
    return CC_UNKN;
  }

  if (sscanf(sys_name.release, "%d.%d.%d", &kv_major, &kv_minor, &kv_rel) != 3) {
    g_print("sscanf kernel version failed.\n");
    return CC_UNKN;
  }

  kv = ((kv_major << 16) + (kv_minor << 8) + kv_rel);
  //GST_INFO("kernel:%s, %d.%d.%d\n", sys_name.release, kv_major, kv_minor, kv_rel);

  if (kv < KERN_VER(3, 10, 0))
    gimx_chip_code = getChipCodeFromCpuinfo();
  else
    gimx_chip_code = getChipCodeFromSocid();

  return gimx_chip_code;
}

static IMXV4l2FeatureMap g_imxv4l2feature_maps[] = {
  {CC_MX6Q, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE},
  {CC_MX6SL, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE},
  {CC_MX6SLL, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE},
  {CC_MX6SX, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE},
  {CC_MX6UL, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE},
  {CC_MX7D, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE},
  {CC_MX8, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE},
};


gboolean check_feature(CHIP_CODE chip_name, CHIP_FEATURE feature)
{
  int i;
  gboolean ret = FALSE;
  for (i=0; i<sizeof(g_imxv4l2feature_maps)/sizeof(IMXV4l2FeatureMap); i++) {
    if ( chip_name== g_imxv4l2feature_maps[i].chip_name) {
      switch (feature) {
        case G3D:
          ret = g_imxv4l2feature_maps[i].g3d;
          break;
        case G2D:
          ret = g_imxv4l2feature_maps[i].g2d;
          break;
        case IPU:
          ret = g_imxv4l2feature_maps[i].ipu;
          break;
        case PXP:
          ret = g_imxv4l2feature_maps[i].pxp;
          break;
        case VPU:
          ret = g_imxv4l2feature_maps[i].vpu;
          break;
        case DPU:
          ret = g_imxv4l2feature_maps[i].dpu;
          break;
        default:
          break;
      }
      break;
    }
  }
  return ret;
}
