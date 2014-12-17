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

#ifndef __GST_PHY_MEM_META_H__ 
#define __GST_PHY_MEM_META_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include "gstphymemmeta.h"

G_BEGIN_DECLS

typedef struct _GstPhyMemMeta GstPhyMemMeta;

#define GST_PHY_MEM_META_API_TYPE  (gst_phy_mem_meta_api_get_type())
#define GST_PHY_MEM_META_INFO  (gst_phy_mem_meta_get_info())

#define GST_PHY_MEM_META_GET(buffer)      ((GstPhyMemMeta *)gst_buffer_get_meta((buffer), gst_phy_mem_meta_api_get_type()))
#define GST_PHY_MEM_META_ADD(buffer)      ((GstPhyMemMeta *)gst_buffer_add_meta((buffer), gst_phy_mem_meta_get_info(), NULL))
#define GST_PHY_MEM_META_DEL(buffer)      (gst_buffer_remove_meta((buffer), gst_buffer_get_meta((buffer), gst_phy_mem_meta_api_get_type())))

struct _GstPhyMemMeta
{
  GstMeta meta;
  guint x_padding;
  guint y_padding;
};

GType gst_phy_mem_meta_api_get_type(void);
GstMetaInfo const * gst_phy_mem_meta_get_info(void);

G_END_DECLS

#endif

