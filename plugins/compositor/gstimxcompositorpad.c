/* GStreamer IMX video compositor plugin
 * Copyright (c) 2015, Freescale Semiconductor, Inc. All rights reserved.
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

#include <string.h>
#include <gst/video/gstvideoaggregator.h>
#include "gstimxcompositorpad.h"
#include "gstimxcompositor.h"
#include "imx_2d_device.h"

#define DEFAULT_IMXCOMPOSITOR_PAD_ZORDER 0
#define DEFAULT_IMXCOMPOSITOR_PAD_XPOS   0
#define DEFAULT_IMXCOMPOSITOR_PAD_YPOS   0
#define DEFAULT_IMXCOMPOSITOR_PAD_WIDTH  0
#define DEFAULT_IMXCOMPOSITOR_PAD_HEIGHT 0
#define DEFAULT_IMXCOMPOSITOR_PAD_ROTATE IMX_2D_ROTATION_0
#define DEFAULT_IMXCOMPOSITOR_PAD_ALPHA  1.0
#define DEFAULT_IMXCOMPOSITOR_PAD_KEEP_RATIO  FALSE

GST_DEBUG_CATEGORY_EXTERN (gst_imxcompositor_debug);

GType gst_imx_compositor_rotation_get_type(void) {
  static GType gst_imx_compositor_rotation_type = 0;

  if (!gst_imx_compositor_rotation_type) {
    static GEnumValue rotation_values[] = {
      {IMX_2D_ROTATION_0, "No rotation", "none"},
      {IMX_2D_ROTATION_90, "Rotate 90 degrees", "rotate-90"},
      {IMX_2D_ROTATION_180, "Rotate 180 degrees", "rotate-180"},
      {IMX_2D_ROTATION_270, "Rotate 270 degrees", "rotate-270"},
      {IMX_2D_ROTATION_HFLIP, "Flip horizontally", "horizontal-flip"},
      {IMX_2D_ROTATION_VFLIP, "Flip vertically", "vertical-flip"},
      {0, NULL, NULL },
    };

    gst_imx_compositor_rotation_type =
        g_enum_register_static("ImxCompositorRotationMode", rotation_values);
  }

  return gst_imx_compositor_rotation_type;
}

enum
{
  PROP_IMXCOMPOSITOR_PAD_0,
  PROP_IMXCOMPOSITOR_PAD_ZORDER,
  PROP_IMXCOMPOSITOR_PAD_XPOS,
  PROP_IMXCOMPOSITOR_PAD_YPOS,
  PROP_IMXCOMPOSITOR_PAD_WIDTH,
  PROP_IMXCOMPOSITOR_PAD_HEIGHT,
  PROP_IMXCOMPOSITOR_PAD_ALPHA,
  PROP_IMXCOMPOSITOR_PAD_ROTATE,
  PROP_IMXCOMPOSITOR_PAD_DEINTERLACE,
  PROP_IMXCOMPOSITOR_PAD_KEEP_RATIO
};

G_DEFINE_TYPE (GstImxCompositorPad, gst_imxcompositor_pad, \
                GST_TYPE_VIDEO_AGGREGATOR_PAD);

static void
gst_imxcompositor_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstImxCompositorPad *pad = GST_IMXCOMPOSITOR_PAD (object);

  switch (prop_id) {
    case PROP_IMXCOMPOSITOR_PAD_ZORDER:
      g_value_set_uint (value, pad->zorder);
      break;
    case PROP_IMXCOMPOSITOR_PAD_XPOS:
      g_value_set_int (value, pad->xpos);
      break;
    case PROP_IMXCOMPOSITOR_PAD_YPOS:
      g_value_set_int (value, pad->ypos);
      break;
    case PROP_IMXCOMPOSITOR_PAD_WIDTH:
      g_value_set_int (value, pad->width);
      break;
    case PROP_IMXCOMPOSITOR_PAD_HEIGHT:
      g_value_set_int (value, pad->height);
      break;
    case PROP_IMXCOMPOSITOR_PAD_ROTATE:
      g_value_set_enum (value, pad->rotate);
      break;
    case PROP_IMXCOMPOSITOR_PAD_ALPHA:
    {
      GstImxCompositor* comp = (GstImxCompositor*)gst_pad_get_parent(pad);
      if (comp->capabilities & IMX_2D_DEVICE_CAP_ALPHA) {
        g_value_set_double (value, pad->alpha);
      } else {
        g_value_set_double (value, 1.0);
        g_print("!This device don't support alpha blending!\n");
      }
    }
      break;
    case PROP_IMXCOMPOSITOR_PAD_KEEP_RATIO:
      g_value_set_boolean(value, pad->keep_ratio);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_imxcompositor_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstImxCompositorPad *pad = GST_IMXCOMPOSITOR_PAD (object);

  switch (prop_id) {
    case PROP_IMXCOMPOSITOR_PAD_ZORDER:
      pad->zorder = g_value_get_int(value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_XPOS:
      pad->xpos = g_value_get_int (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_YPOS:
      pad->ypos = g_value_get_int (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_ALPHA:
    {
      GstImxCompositor* comp = (GstImxCompositor*)gst_pad_get_parent(pad);
      if (comp->capabilities & IMX_2D_DEVICE_CAP_ALPHA) {
        pad->alpha = g_value_get_double (value);
      } else {
        pad->alpha = 1.0;
        g_print("!This device don't support alpha blending, "
            "pad alpha setting will be ignored!\n");
      }
      gst_object_unref(comp);
    }
      break;
    case PROP_IMXCOMPOSITOR_PAD_WIDTH:
      pad->width = g_value_get_int (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_HEIGHT:
      pad->height = g_value_get_int (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_ROTATE:
      pad->rotate = g_value_get_enum (value);
      break;
    case PROP_IMXCOMPOSITOR_PAD_KEEP_RATIO:
      pad->keep_ratio = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_imxcompositor_pad_finalize (GObject * object)
{
  GstImxCompositorPad *pad = GST_IMXCOMPOSITOR_PAD (object);

  if (pad->sink_pool) {
    gst_buffer_pool_set_active (pad->sink_pool, FALSE);
    gst_object_unref (pad->sink_pool);
    pad->sink_pool = NULL;
  }

  G_OBJECT_CLASS (gst_imxcompositor_pad_parent_class)->finalize (object);
}

static void
gst_imxcompositor_pad_class_init (GstImxCompositorPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_imxcompositor_pad_set_property;
  gobject_class->get_property = gst_imxcompositor_pad_get_property;
  gobject_class->finalize = gst_imxcompositor_pad_finalize;

  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_XPOS,
      g_param_spec_int ("xpos", "X Position", "X Position of the picture",
          G_MININT32, G_MAXINT32, DEFAULT_IMXCOMPOSITOR_PAD_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_YPOS,
      g_param_spec_int ("ypos", "Y Position", "Y Position of the picture",
          G_MININT32, G_MAXINT32, DEFAULT_IMXCOMPOSITOR_PAD_YPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_WIDTH,
      g_param_spec_int ("width", "Width", "Target width of the picture",
          G_MININT32, G_MAXINT32, DEFAULT_IMXCOMPOSITOR_PAD_WIDTH,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_HEIGHT,
      g_param_spec_int ("height", "Height", "Target height of the picture",
          G_MININT32, G_MAXINT32, DEFAULT_IMXCOMPOSITOR_PAD_HEIGHT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_ROTATE,
      g_param_spec_enum("rotate", "input rotation",
        "Rotation that shall be applied to input frames",
        gst_imx_compositor_rotation_get_type(),
        DEFAULT_IMXCOMPOSITOR_PAD_ROTATE,
        G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_ALPHA,
      g_param_spec_double ("alpha", "Alpha", "Alpha of the picture", 0.0, 1.0,
          DEFAULT_IMXCOMPOSITOR_PAD_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_IMXCOMPOSITOR_PAD_KEEP_RATIO,
      g_param_spec_boolean ("keep-ratio", "Keep Aspect Ratio",
          "Keep the video aspect ratio after resize",
          DEFAULT_IMXCOMPOSITOR_PAD_KEEP_RATIO,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IMXCOMPOSITOR_PAD_ZORDER,
      g_param_spec_int ("zorder", "Z-order", "Z order of the picture",
          0, 10000, DEFAULT_IMXCOMPOSITOR_PAD_ZORDER,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_imxcompositor_pad_init (GstImxCompositorPad * compo_pad)
{
  compo_pad->xpos = DEFAULT_IMXCOMPOSITOR_PAD_XPOS;
  compo_pad->ypos = DEFAULT_IMXCOMPOSITOR_PAD_YPOS;
  compo_pad->width = DEFAULT_IMXCOMPOSITOR_PAD_WIDTH;
  compo_pad->height = DEFAULT_IMXCOMPOSITOR_PAD_HEIGHT;
  compo_pad->rotate = DEFAULT_IMXCOMPOSITOR_PAD_ROTATE;
  compo_pad->alpha = DEFAULT_IMXCOMPOSITOR_PAD_ALPHA;
  compo_pad->keep_ratio = DEFAULT_IMXCOMPOSITOR_PAD_KEEP_RATIO;
  compo_pad->zorder = DEFAULT_IMXCOMPOSITOR_PAD_ZORDER;
  compo_pad->sink_pool = NULL;
  memset(&compo_pad->align, 0, sizeof(GstVideoAlignment));
}
