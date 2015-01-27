/*
 * Copyright (C) 2009-2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 */

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

#ifndef IMXPLAYER_H_
#define IMXPLAYER_H_

#include <gtk/gtk.h>
#include "menubar.h"
#include "controlbar.h"
#include "infobox.h"
#include "volumebar.h"
#include "subtitle.h"
#include "playlistbox.h"
#include "speedbox.h"
#include "playengine.h"

#define SYS_GTK_STYLE_CONFIG_FILE "/etc/gtk-2.0/gtkrc"
#define MY_GTK_STYLE_CONFIG_FILE  "./config/gtkrc"
#define BAK_GTK_STYLE_CONFIG_FILE  "./config/gtkrc.bak"

//#define EXPOSE_VIDEO_FOR_EACH_EXPOSE_EVENT
//#define REMOVE_WINDOW_MANAGER_DECORATION
//#define ENABLE_SET_COLOR_KEY

#define INFO_BOX_W      500
#define INFO_BOX_H      300
#define PLAYLISTBOX_W   INFO_BOX_W
#define PLAYLISTBOX_H   400
#define SUBTITLE_BOX_W  800
#define SUBTITLE_BOX_H  40
#define SPEED_BOX_W     100
#define SPEED_BOX_H     50
#define SPEED_BOX_X     50
#define SPEED_BOX_Y     50

typedef struct
{
  GtkWidget *top_window;
  GtkWidget *fixed_ct;
  GtkWidget *video_win;

  MenuBar menubar;
  CtrlBar ctrlbar;
  InfoBox infobox;
  VolumeBar volumebar;
  SubTitle subtitle;
  PlaylistBox playlistbox;
  SpeedBox speed;
  GdkColor color_key;
  play_engine *playengine;
  imx_metadata meta;

  gboolean fullscreen;
  gboolean accurate_seek;
//  gboolean metainfo_refresh;
  gboolean show_info;
  gboolean show_ctrlbar;
  gboolean show_menubar;
  gboolean show_playlist;
  gboolean show_volbar;
  gboolean show_subtitle;
  gint video_x;
  gint video_y;
  gint video_w;
  gint video_h;
  gint screen_w;
  gint screen_h;
  gint window_w;
  gint window_h;
  gint video_w_pre;
  gint video_h_pre;
} ImxPlayer;

#endif /* IMXPLAYER_H_ */
