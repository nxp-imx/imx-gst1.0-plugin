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
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. All rights reserved. 
 * Copyright 2017 NXP
 *
 */


/*
 * Module Name:    gstsutils.h
 *
 * Description:    simple utils head file for gst plugins
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */

#include <gst/gst.h>

#ifndef __GST_SUTILS__
#define __GST_SUTILS__

#define G_MININT_STR "0x80000000"
#define G_MAXINT_STR "0x7fffffff"

#define G_MINUINT_STR "0"
#define G_MAXUINT_STR "0xffffffff"

#define G_MININT64_STR "0x8000000000000000"
#define G_MAXINT64_STR "0x7fffffffffffffff"

#define G_MINUINT64_STR "0"
#define G_MAXUINT64_STR "0xffffffffffffffffU"

#define FSL_GST_CONF_DEFAULT_FILENAME "/usr/share/gst-fsl-plugins_1.0.conf"




#define FSL_KEY_LIB "library"
#define FSL_KEY_LIB2 "library2"
#define FSL_KEY_DSP_LIB "dsp_lib"
#define FSL_KEY_MIME "mime"
#define FSL_KEY_DESC "description"
#define FSL_KEY_RANK "rank"


typedef GType (*gtype_func) (void);

typedef struct
{
  gint id;
  const gchar *name;
  const gchar *nickname;
  const gchar *desc;
  GType gtype;
  int offset;
  const char *def;
  const char *min;
  const char *max;
  gtype_func typefunc;
} GstsutilsOptionEntry;

typedef void (*gstsutils_set_func) (GstObject* obj, ...);
typedef struct
{
    const gchar * name;
    GType gtype;
    gstsutils_set_func set_func;
}gstsutils_property;
void gstsutils_load_default_property(gstsutils_property * table,GstObject * dec,
    const gchar * filename, const gchar * group);



typedef struct _GstsutilsGroup GstsutilsGroup;
typedef struct _GstsutilsEntry GstsutilsEntry;



void
gstsutils_options_install_properties_by_options (GstsutilsOptionEntry * table,
    GObjectClass * oclass);
gboolean
gstsutils_options_get_option (GstsutilsOptionEntry * table, gchar * option,
    guint id, GValue * value);
gboolean gstsutils_options_set_option (GstsutilsOptionEntry * table,
    gchar * option, guint id, const GValue * value);
void gstsutils_options_load_default (GstsutilsOptionEntry * table,
    gchar * option);
gboolean gstsutils_options_load_from_keyfile (GstsutilsOptionEntry * table,
    gchar * option, gchar * filename, gchar * group);

gboolean
gstsutils_elementutil_get_int (gchar * filename, gchar * group, gchar * field,
    gint * value);


GstsutilsEntry *gstsutils_init_entry (gchar * filename);
int gstsutils_get_group_count(GstsutilsEntry * entry);
gboolean gstsutils_get_group_by_index (GstsutilsEntry * entry,int index,GstsutilsGroup ** group_out);
int gstsutils_get_data_count_in_group (GstsutilsGroup * group);
gchar * gstsutils_get_group_name (GstsutilsGroup * group);
gboolean gstsutils_get_value_by_index(GstsutilsGroup *group,int index, gchar** key_out, gchar**value_out);
gboolean gstsutils_get_value_by_key(GstsutilsGroup *group,gchar * key, gchar**value_out);
void gstsutils_deinit_entry (GstsutilsEntry * entry);




#endif
