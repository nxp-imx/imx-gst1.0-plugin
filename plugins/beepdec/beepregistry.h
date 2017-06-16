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
 * Copyright 2017 NXP
 *
 */



/*
 * Module Name:    beepregistry.h
 *
 * Description:    Head file of unified audio decoder core functions
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */


#ifndef __BEEPREGISTRY_H__
#define __BEEPREGISTRY_H__
#include <gst/gst.h>
#include "fsl_unia.h"
#include "../../libs/gstsutils/gstsutils.h"

typedef struct
{
  UniACodecVersionInfo getVersionInfo;
  UniACodecCreate createDecoder;
  UniACodecCreatePlus createDecoderplus;
  UniACodecDelete deleteDecoder;
  UniACodecReset resetDecoder;
  UniACodecSetParameter setDecoderPara;
  UniACodecGetParameter getDecoderPara;
  UniACodec_decode_frame decode;

  void *dl_handle;              /* must be last, for dl handle */

  gchar * name;
  
  const char *coreid;

} BeepCoreInterface;


GstCaps *beep_core_get_caps ();
BeepCoreInterface *beep_core_create_interface_from_caps_dsp (GstCaps * caps);
BeepCoreInterface *beep_core_create_interface_from_caps (GstCaps * caps);

void
beep_core_destroy_interface (BeepCoreInterface * inf);

#endif /* __BEEPREGISTRY_H__ */
