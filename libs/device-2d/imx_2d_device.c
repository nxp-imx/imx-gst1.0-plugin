/* GStreamer IMX Video 2D device
 * Copyright (c) 2014-2015, Freescale Semiconductor, Inc. All rights reserved.
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

#include "imx_2d_device.h"

GST_DEBUG_CATEGORY (imx2ddevice_debug);
#define GST_CAT_DEFAULT imx2ddevice_debug

#ifdef USE_IPU
extern Imx2DDevice * imx_ipu_create(Imx2DDeviceType  device_type);
extern gint imx_ipu_destroy(Imx2DDevice *device);
extern gboolean imx_ipu_is_exist (void);
#endif

#ifdef USE_G2D
extern Imx2DDevice * imx_g2d_create(Imx2DDeviceType  device_type);
extern gint imx_g2d_destroy(Imx2DDevice *device);
extern gboolean imx_g2d_is_exist (void);
#endif

#ifdef USE_PXP
extern Imx2DDevice * imx_pxp_create(Imx2DDeviceType  device_type);
extern gint imx_pxp_destroy(Imx2DDevice *device);
extern gboolean imx_pxp_is_exist (void);
#endif

static const Imx2DDeviceInfo Imx2DDevices[] = {
#ifdef USE_IPU
    { .name                     ="ipu",
      .device_type              =IMX_2D_DEVICE_IPU,
      .create                   =imx_ipu_create,
      .destroy                  =imx_ipu_destroy,
      .is_exist                 =imx_ipu_is_exist
    },
#endif

#ifdef USE_G2D
    { .name                     ="g2d",
      .device_type              =IMX_2D_DEVICE_G2D,
      .create                   =imx_g2d_create,
      .destroy                  =imx_g2d_destroy,
      .is_exist                 =imx_g2d_is_exist
    },
#endif

#ifdef USE_PXP
    { .name                     ="pxp",
      .device_type              =IMX_2D_DEVICE_PXP,
      .create                   =imx_pxp_create,
      .destroy                  =imx_pxp_destroy,
      .is_exist                 =imx_pxp_is_exist
    },
#endif
    {
      NULL
    }
};

const Imx2DDeviceInfo * imx_get_2d_devices(void)
{
  static gint debug_init = 0;
  if (debug_init == 0) {
    GST_DEBUG_CATEGORY_INIT (imx2ddevice_debug, "imx2ddevice", 0,
                           "Freescale IMX 2D Devices");
    debug_init = 1;
  }

  return &Imx2DDevices[0];
}

Imx2DDevice * imx_2d_device_create(Imx2DDeviceType  device_type)
{
  const Imx2DDeviceInfo *dev_info = imx_get_2d_devices();
  while (dev_info->name) {
    if (dev_info->device_type == device_type) {
      if (dev_info->is_exist()) {
        return dev_info->create(device_type);
      } else {
        GST_ERROR("device %s not exist", dev_info->name);
        return NULL;
      }
    }
    dev_info++;
  }

  GST_ERROR("Unknown 2D device type %d\n", device_type);
  return NULL;
}

gint imx_2d_device_destroy(Imx2DDevice *device)
{
  if (!device)
    return -1;

  const Imx2DDeviceInfo *dev_info = imx_get_2d_devices();
  while (dev_info->name) {
    if (dev_info->device_type == device->device_type)
      return dev_info->destroy(device);
    dev_info++;
  }

  GST_ERROR("Unknown 2D device type %d\n", device->device_type);
  return -1;
}
