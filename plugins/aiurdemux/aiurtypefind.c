/* GStreamer
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 * Copyright (C) 2005-2009 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * gsttypefindfunctions.c: collection of various typefind functions
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

 /*
    Based on gsttypefindfunctions.c
    Copyright (c) 2011-2012, Freescale Semiconductor, Inc. 
  */


 /*
  * Module Name:    aiurtypefind.c
  *
  * Description:    typefind functions to support new format on lower gstreamer base plugin
  *
  * Portability:    This code is written for Linux OS and Gstreamer
  */
#include <gst/gst.h>
#include <string.h>

typedef void (*AiurTypeFindFunc) (GstTypeFind *, gpointer);

typedef struct
{
  gchar *name;
  AiurTypeFindFunc func;
  gchar *exts;
  gchar *mime;
} AiurExternalTypeFind;

static void webm_type_find (GstTypeFind * tf, gpointer ununsed);
static gboolean ebml_check_header (GstTypeFind * tf, const gchar * doctype,
    int doctype_len);
static gchar webm_exts[] = "webm";

static AiurExternalTypeFind g_aiurextypefinders[] = {
  {"webm", webm_type_find, webm_exts, "video/webm"},
  {NULL, NULL, NULL, NULL},
};


gboolean
aiur_register_external_typefinders (GstPlugin * plugin)
{
  gboolean ret = TRUE;
  AiurExternalTypeFind *t = g_aiurextypefinders;
  while (t->name) {
    GstCaps *caps;
    caps = gst_caps_new_simple(t->mime, NULL);
    ret &= gst_type_find_register (plugin, t->name, GST_RANK_PRIMARY,
        t->func, t->exts, caps, NULL, NULL);
    gst_caps_unref(caps);
    t++;
  }
  return ret;

}

/*** video/webm ***/
static GstStaticCaps webm_caps = GST_STATIC_CAPS ("video/webm");

#define WEBM_CAPS (gst_static_caps_get(&webm_caps))
static void
webm_type_find (GstTypeFind * tf, gpointer ununsed)
{
  if (ebml_check_header (tf, "webm", 4))
    gst_type_find_suggest (tf, GST_TYPE_FIND_MAXIMUM, WEBM_CAPS);
}

static gboolean
ebml_check_header (GstTypeFind * tf, const gchar * doctype, int doctype_len)
{
  /* 4 bytes for EBML ID, 1 byte for header length identifier */
  guint8 *data = gst_type_find_peek (tf, 0, 4 + 1);
  gint len_mask = 0x80, size = 1, n = 1, total;

  if (!data)
    return FALSE;

  /* ebml header? */
  if (data[0] != 0x1A || data[1] != 0x45 || data[2] != 0xDF || data[3] != 0xA3)
    return FALSE;

  /* length of header */
  total = data[4];
  while (size <= 8 && !(total & len_mask)) {
    size++;
    len_mask >>= 1;
  }
  if (size > 8)
    return FALSE;
  total &= (len_mask - 1);
  while (n < size)
    total = (total << 8) | data[4 + n++];

  /* get new data for full header, 4 bytes for EBML ID,
   * EBML length tag and the actual header */
  data = gst_type_find_peek (tf, 0, 4 + size + total);
  if (!data)
    return FALSE;

  /* only check doctype if asked to do so */
  if (doctype == NULL || doctype_len == 0)
    return TRUE;

  /* the header must contain the doctype. For now, we don't parse the
   * whole header but simply check for the availability of that array
   * of characters inside the header. Not fully fool-proof, but good
   * enough. */
  for (n = 4 + size; n <= 4 + size + total - doctype_len; n++)
    if (!memcmp (&data[n], doctype, doctype_len))
      return TRUE;

  return FALSE;
}
