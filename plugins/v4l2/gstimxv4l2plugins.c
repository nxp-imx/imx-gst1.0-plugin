/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. All rights reserved.
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

#include "gstimxv4l2sink.h"

//used in imx v4l2 core debug
GST_DEBUG_CATEGORY (imxv4l2_debug);

CHIP_CODE gimx_chip;

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (imxv4l2_debug, "imxv4l2", 0, "IMX V4L2 Core");
  gimx_chip = getChipCode();

  if (!gst_element_register (plugin, "imxv4l2sink", IMX_GST_PLUGIN_RANK,
        GST_TYPE_IMX_V4L2SINK))
    return FALSE;

  return TRUE;
}

IMX_GST_PLUGIN_DEFINE (imxv4l2sink, "IMX SoC v4l2-based video sink", plugin_init);
