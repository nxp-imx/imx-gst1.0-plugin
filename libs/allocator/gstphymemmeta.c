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

#include "gstphymemmeta.h"

GST_DEBUG_CATEGORY_STATIC(phy_mem_meta_debug);
#define GST_CAT_DEFAULT phy_mem_meta_debug

static gboolean
gst_phy_mem_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstPhyMemMeta *dmeta, *smeta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    smeta = (GstPhyMemMeta *) meta;
    dmeta = GST_PHY_MEM_META_ADD (dest);

    GST_DEBUG ("copy phy metadata");

    dmeta->x_padding = smeta->x_padding;
    dmeta->y_padding = smeta->y_padding;
  } else if (GST_VIDEO_META_TRANSFORM_IS_SCALE (type)) {
    GstVideoMetaTransform *trans = data;
    gint ow, oh, nw, nh;

    smeta = (GstPhyMemMeta *) meta;
    dmeta = GST_PHY_MEM_META_ADD (dest);

    ow = GST_VIDEO_INFO_WIDTH (trans->in_info);
    nw = GST_VIDEO_INFO_WIDTH (trans->out_info);
    oh = GST_VIDEO_INFO_HEIGHT (trans->in_info);
    nh = GST_VIDEO_INFO_HEIGHT (trans->out_info);

    GST_DEBUG ("scaling phy metadata %dx%d -> %dx%d", ow, oh, nw, nh);

    dmeta->x_padding = (smeta->x_padding * nw) / ow;
    dmeta->y_padding = (smeta->y_padding * nh) / oh;
  }
  return TRUE;
}

  GType
gst_phy_mem_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
  { GST_META_TAG_VIDEO_STR, GST_META_TAG_VIDEO_SIZE_STR,
    GST_META_TAG_VIDEO_ORIENTATION_STR, NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstPhyMemMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_phy_mem_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buf)
{
  return TRUE;
}

  const GstMetaInfo *
gst_phy_mem_meta_get_info (void)
{
  static const GstMetaInfo *phy_mem_meta_info = NULL;

  if (g_once_init_enter (&phy_mem_meta_info)) {
    const GstMetaInfo *meta =
      gst_meta_register (GST_PHY_MEM_META_API_TYPE, "GstPhyMemMeta",
          sizeof (GstPhyMemMeta), (GstMetaInitFunction) gst_phy_mem_meta_init,
          (GstMetaFreeFunction) NULL, gst_phy_mem_meta_transform);
    GST_DEBUG_CATEGORY_INIT (phy_mem_meta_debug, "phymemmeta", 0,
                               "Freescale physical memory meta");
    g_once_init_leave (&phy_mem_meta_info, meta);
  }
  return phy_mem_meta_info;
}


