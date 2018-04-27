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
 * Copyright (C) 2010-2011, 2014 Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2018 NXP
 *
 */



/*
 * Module Name:    aiur.c
 *
 * Description:    Registration of unified parser gstreamer plugin
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

#include "aiurdemux.h"
#include "gstimxcommon.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  aiur_register_external_typefinders (plugin);
  //gst_aiur_stream_cache_get_type ();
  if (!gst_element_register
      (plugin, "aiurdemux", (GST_RANK_PRIMARY+1), GST_TYPE_AIURDEMUX)){
    return FALSE;
  }
  return TRUE;

}

IMX_GST_PLUGIN_DEFINE (aiurdemux, "aiur universal demux", plugin_init);
