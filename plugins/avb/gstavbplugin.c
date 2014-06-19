/*
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
#include "gstimxcommon.h"
#include "gstavbpcmsink.h"
#include "gstavbpcmsrc.h"
#include "gstavbmpegtssink.h"
#include "gstavbmpegtssrc.h"

static gboolean
plugin_init (GstPlugin * plugin)
{

  if (!gst_element_register
      (plugin, "avbpcmsink", GST_RANK_PRIMARY+1, GST_TYPE_AVB_PCM_SINK))
    return FALSE;


  if (!gst_element_register
      (plugin, "avbpcmsrc", GST_RANK_PRIMARY+1, GST_TYPE_AVB_PCM_SRC))
    return FALSE;

  if (!gst_element_register
      (plugin, "avbmpegtssink", GST_RANK_PRIMARY+1, GST_TYPE_AVB_MPEGTS_SINK))
    return FALSE;

  if (!gst_element_register
      (plugin, "avbmpegtssrc", GST_RANK_PRIMARY+1, GST_TYPE_AVB_MPEGTS_SRC))
    return FALSE;
  return TRUE;
}

IMX_GST_PLUGIN_DEFINE (avb, "avb sink/src", plugin_init);
