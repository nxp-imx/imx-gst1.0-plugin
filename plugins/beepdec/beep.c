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
 * Copyright (c) 2011-2014, Freescale Semiconductor, Inc. All rights reserved. 
 * Copyright 2018 NXP
 *
 */

/*
 * Module Name:    beep.c
 *
 * Description:    Registration of universal audio decoder gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */
/*
 * Changelog:
 *
 */
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "beepdec.h"
#include "gstimxcommon.h"

static gboolean
plugin_init (GstPlugin * plugin)
{

  beep_register_external_typefinders (plugin);

  if (!gst_element_register
      (plugin, "beepdec", GST_RANK_PRIMARY+1, GST_TYPE_BEEP_DEC))
    return FALSE;

  return TRUE;
}

IMX_GST_PLUGIN_DEFINE (beepdec, "universal audio decoder", plugin_init);

