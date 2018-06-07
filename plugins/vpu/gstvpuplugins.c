/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2018 NXP
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gst/gst.h>

#include "gstimxcommon.h"
#include "gstvpuenc.h"
#include "gstvpudec.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (HAS_VPU()) {
    if (!IS_HANTRO() || IS_IMX8MM())
      if (!gst_vpu_enc_register (plugin))
        return FALSE;
    
    if (!gst_element_register (plugin, "vpudec", IMX_GST_PLUGIN_RANK,
          GST_TYPE_VPU_DEC))
      return FALSE;

    return TRUE;
  } else {
    return FALSE;
  }
}

IMX_GST_PLUGIN_DEFINE (vpu, "VPU video codec", plugin_init);
