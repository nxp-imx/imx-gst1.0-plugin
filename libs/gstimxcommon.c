/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2017,2018 NXP
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
#include "gstimx.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/version.h>
#include <linux/dma-buf.h>
#ifdef USE_ION
#include <linux/ion.h>
#endif
const char *dev_ion = "/dev/ion";

unsigned long phy_addr_from_fd(int dmafd)
{
#ifdef USE_ION
  int ret, fd;

  if (dmafd < 0)
    return NULL;
  
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 34)
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
  struct dma_buf_phys dma_phys;

  ret = ioctl(dmafd, DMA_BUF_IOCTL_PHYS, &dma_phys);
  if (ret < 0)
    return NULL;

  return dma_phys.phys;
#endif
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
  
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 34)
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
#else
  return NULL;
#endif
}
