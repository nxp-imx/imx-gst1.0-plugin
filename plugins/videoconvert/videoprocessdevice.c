/* GStreamer IMX Video Processing device
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

#include "videoprocessdevice.h"
#ifdef USE_IPU
extern ImxVideoProcessDevice * imx_ipu_create(void);
extern gint imx_ipu_destroy(ImxVideoProcessDevice *device);
#endif
#ifdef USE_G2D
extern ImxVideoProcessDevice * imx_g2d_create(void);
extern gint imx_g2d_destroy(ImxVideoProcessDevice *device);
#endif
static const ImxVideoProcessDeviceInfo VPDevices[] = {
#ifdef USE_IPU
    { .name                     ="ipu",
      .description              ="IMX IPU Video Converter",
      .detail                   ="Video CSC/resize/rotate/deinterlace",
      .create                   =imx_ipu_create,
      .destroy                  =imx_ipu_destroy
    },
#endif

#ifdef USE_G2D
    { .name                     ="g2d",
      .description              ="IMX G2D Video Converter",
      .detail                   ="Video CSC/resize/rotate",
      .create                   =imx_g2d_create,
      .destroy                  =imx_g2d_destroy
    },
#endif
    {
      NULL
    }
};

const ImxVideoProcessDeviceInfo * imx_get_video_process_devices(void)
{
  return &VPDevices[0];
}
