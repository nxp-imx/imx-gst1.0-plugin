/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2017 NXP
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#ifdef USE_ION
#include <linux/ion.h>
#endif

/* define rotate and flip glib enum for overlaysink and imxv4l2sink */
static const GEnumValue rotate_methods[] = {
  {GST_IMX_ROTATION_0, "no rotation", "none"},
  {GST_IMX_ROTATION_90, "Rotate clockwise 90 degrees", "rotate-90"},
  {GST_IMX_ROTATION_180, "Rotate clockwise 180 degrees", "rotate-180"},
  {GST_IMX_ROTATION_270, "Rotate clockwise 270 degrees", "rotate-270"},
  {GST_IMX_ROTATION_HFLIP, "Flip horizontally", "horizontal-flip"},
  {GST_IMX_ROTATION_VFLIP, "Flip vertically", "vertically-flip"},
  {0, NULL, NULL}
};

GType
gst_imx_rotate_method_get_type()
{
  static GType rotate_method_type = 0;
  static volatile gsize once = 0;

  if (g_once_init_enter (&once)) {
    rotate_method_type = g_enum_register_static ("GstImxRotateMethod",
        rotate_methods);
    g_once_init_leave (&once, rotate_method_type);
  }

  return rotate_method_type;
}

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
  {CC_MX7ULP, "i.MX7ULP"},
  {CC_MX8, "i.MX8DV"},
  {CC_MX8Q, "i.MX8QM"},
  {CC_MX8Q, "i.MX8QXP"},
  {CC_MX8M, "i.MX8MQ"},
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

int get_kernel_version (void)
{
  struct utsname sys_name;
  int kv_major, kv_minor, kv_rel;

  if (uname(&sys_name) < 0) {
    g_print("get kernel version via uname failed.\n");
    return -1;
  }

  if (sscanf(sys_name.release, "%d.%d.%d", &kv_major, &kv_minor, &kv_rel) != 3) {
    g_print("sscanf kernel version failed.\n");
    return -1;
  }

  return KERN_VER (kv_major, kv_minor, kv_rel);
}

static CHIP_CODE gimx_chip_code = CC_UNKN;

CHIP_CODE imx_chip_code (void)
{
  int kv;

  if (gimx_chip_code != CC_UNKN)
    return gimx_chip_code;

  kv = get_kernel_version();
  if (kv < 0)
    return CC_UNKN;

  if (kv < KERN_VER(3, 10, 0))
    gimx_chip_code = getChipCodeFromCpuinfo();
  else
    gimx_chip_code = getChipCodeFromSocid();

  return gimx_chip_code;
}

static IMXV4l2FeatureMap g_imxv4l2feature_maps[] = {
  /* chip_name, g3d, g2d, ipu, pxp, vpu, dpu, dcss*/
  {CC_MX6Q, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE},
  {CC_MX6SL, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE},
  {CC_MX6SLL, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE},
  {CC_MX6SX, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE},
  {CC_MX6UL, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE},
  {CC_MX7D, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE},
  {CC_MX7ULP, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE},
  {CC_MX8, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE},
  {CC_MX8Q, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE},
  {CC_MX8M, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE},
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
        case DCSS:
          ret = g_imxv4l2feature_maps[i].dcss;
          break;
        default:
          break;
      }
      break;
    }
  }
  return ret;
}

const char *dev_ion = "/dev/ion";

unsigned long phy_addr_from_fd(int dmafd)
{
#ifdef USE_ION
  int ret, fd;

  if (dmafd < 0)
    return NULL;
  
  fd = open(dev_ion, O_RDWR);
  if(fd < 0) {
    return NULL;
  }

  struct ion_phys_dma_data data = {
    .phys = 0,
    .size = 0,
    .dmafd = dmafd,
  };

  struct ion_custom_data custom = {
    .cmd = ION_IOC_PHYS_DMA,
    .arg = (unsigned long)&data,
  };

  ret = ioctl(fd, ION_IOC_CUSTOM, &custom);
  close(fd);
  if (ret < 0)
    return NULL;

  return data.phys;
#else
  return NULL;
#endif
}

unsigned long phy_addr_from_vaddr(void *vaddr, int size)
{
#ifdef USE_ION
  int ret, fd;

  if (!vaddr)
    return NULL;
  
  fd = open(dev_ion, O_RDWR);
  if(fd < 0) {
    return NULL;
  }

  struct ion_phys_virt_data data = {
    .virt = (unsigned long)vaddr,
    .phys = 0,
    .size = size,
  };

  struct ion_custom_data custom = {
    .cmd = ION_IOC_PHYS_VIRT,
    .arg = (unsigned long)&data,
  };

  ret = ioctl(fd, ION_IOC_CUSTOM, &custom);
  close(fd);
  if (ret < 0)
    return NULL;

  return data.phys;
#else
  return NULL;
#endif
}
