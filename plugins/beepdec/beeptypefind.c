/*
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
    Copyright (c) 2012-2014, Freescale Semiconductor, Inc. 
  */


 /*
  * Module Name:    beeptypefind.c
  *
  * Description:    typefind functions to support new format on lower gstreamer base plugin
  *
  * Portability:    This code is written for Linux OS and Gstreamer
  */

#include <gst/gst.h>

typedef void (*BeepTypeFindFunc) (GstTypeFind *, gpointer);

typedef struct
{
  gchar *name;
  BeepTypeFindFunc func;
  gchar *exts;
  gchar *mime;
} BeepExternalTypeFind;


static void ac3_typefind (GstTypeFind * tf, gpointer data);
static void ac3_bigendian_typefind (GstTypeFind * tf, gpointer data);

static gchar ac3_exts[] = "ac3";

static BeepExternalTypeFind g_beepextypefinders[] = {
  {"ac3", ac3_typefind, ac3_exts, "video/x-ac3"},
  {"3ca", ac3_bigendian_typefind, ac3_exts, "video/x-3ca"},
  {NULL, NULL, NULL, NULL},
};


gboolean
beep_register_external_typefinders (GstPlugin * plugin)
{
  gboolean ret = TRUE;
  BeepExternalTypeFind *t = g_beepextypefinders;
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

static GstStaticCaps caps_3ca = GST_STATIC_CAPS("audio/x-3ca");
#define CAPS_3CA (gst_static_caps_get(&caps_3ca))
static GstStaticCaps caps_ac3 = GST_STATIC_CAPS("audio/x-ac3");
#define CAPS_AC3 (gst_static_caps_get(&caps_ac3))

static void
ac3_typefind (GstTypeFind * tf, gpointer data)
{
  guint8 *data_in = gst_type_find_peek (tf, 0, 2);

  if (data_in) {
    if (data_in[0] == 0x0b && data_in[1] == 0x77)
      gst_type_find_suggest(tf, GST_TYPE_FIND_MAXIMUM, CAPS_AC3);
  }
}

static void
ac3_bigendian_typefind (GstTypeFind * tf, gpointer data)
{
  guint8 *data_in = gst_type_find_peek (tf, 0, 2);

  if (data_in) {
    if (data_in[1] == 0x0b && data_in[0] == 0x77)
      gst_type_find_suggest(tf, GST_TYPE_FIND_MAXIMUM, CAPS_3CA);
  }
}
