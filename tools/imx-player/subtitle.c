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

#include "imxplayer.h"
#include "subtitle.h"

void subtitle_create(void *imxplayer)
{
  GdkColor color;
  ImxPlayer *player = (ImxPlayer *)imxplayer;
  SubTitle *subtitle = &player->subtitle;
  subtitle->subtitle = gtk_event_box_new();
  gtk_widget_modify_bg(subtitle->subtitle, GTK_STATE_NORMAL,
                        &player->color_key);
  gtk_widget_set_size_request(subtitle->subtitle,
                              player->video_w, SUBTITLE_BOX_H);
  gtk_fixed_put(GTK_FIXED(player->fixed_ct), subtitle->subtitle,
                player->video_x,
                player->video_y + (player->video_h*SUBTITLE_Y_PROPOTION));
  subtitle->text = gtk_label_new(" ");
  gdk_color_parse ("blue", &color);
  gtk_widget_modify_fg (subtitle->text, GTK_STATE_NORMAL, &color);
  gtk_widget_modify_font(subtitle->text,
        pango_font_description_from_string("Tahoma Bold 18"));
  gtk_container_add(GTK_CONTAINER(subtitle->subtitle), subtitle->text);
}

void subtitle_show(SubTitle *subtitle, gboolean show)
{
  if (show)
    gtk_widget_show(subtitle->subtitle);
  else
    gtk_widget_hide(subtitle->subtitle);
}

void subtitle_resize(GtkWidget *container, SubTitle *subtitle,
                     gint x, gint y, gint w, gint h)
{
  gtk_widget_set_size_request(subtitle->subtitle, w, SUBTITLE_BOX_H);
  gtk_fixed_move(GTK_FIXED(container), subtitle->subtitle, x,
                  (y + (h * SUBTITLE_Y_PROPOTION)));
}

void subtitle_set_text(SubTitle *subtitle, const gchar *text)
{
  gtk_label_set_text(GTK_LABEL(subtitle->text), text);
}
