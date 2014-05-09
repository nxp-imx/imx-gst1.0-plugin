/* GStreamer MP3 encoder plugin
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstimxmp3enc.h"
#include "gstimxcommon.h"

static gboolean
plugin_init (GstPlugin * plugin)
{


  if (!gst_element_register
      (plugin, "imxmp3enc", IMX_GST_PLUGIN_RANK, GST_TYPE_IMX_MP3ENC))
    return FALSE;

  return TRUE;
}

IMX_GST_PLUGIN_DEFINE (imxmp3enc, "IMX MP3 Encoder based audio encoder", plugin_init);

