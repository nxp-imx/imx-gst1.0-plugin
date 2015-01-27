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
#include "speedbox.h"

void speedbox_create(void *imxplayer)
{
  GdkColor color;
  ImxPlayer *player = (ImxPlayer *)imxplayer;

  SpeedBox *sbox = &player->speed;
  sbox->speed_box = gtk_event_box_new();
  gtk_widget_modify_bg(sbox->speed_box, GTK_STATE_NORMAL, &player->color_key);
  gtk_widget_set_size_request(sbox->speed_box, SPEED_BOX_W, SPEED_BOX_H);
  gtk_fixed_put(GTK_FIXED(player->fixed_ct), sbox->speed_box,
                SPEED_BOX_X, SPEED_BOX_Y);

  sbox->text = gtk_label_new("1.0");
  gdk_color_parse ("lightgreen", &color);
  gtk_widget_modify_fg (sbox->text, GTK_STATE_NORMAL, &color);
  gtk_widget_modify_font(sbox->text,
      pango_font_description_from_string("Tahoma Bold 20"));
  gtk_container_add(GTK_CONTAINER(sbox->speed_box), sbox->text);
}

void speedbox_show(SpeedBox *sbox, gboolean show)
{
  if (show)
    gtk_widget_show(sbox->speed_box);
  else
    gtk_widget_hide(sbox->speed_box);

}

void speedbox_update(void *imxplayer, double rate)
{
  gchar str[8] = {0};
  ImxPlayer *player = (ImxPlayer *)imxplayer;

  if (rate > 1.0 || rate <= 0.5) {
    sprintf(str, "%.1fX", rate);
    gtk_label_set_text(GTK_LABEL(player->speed.text), str);
    gtk_widget_show(player->speed.speed_box);
  } else {
    gtk_label_set_text(GTK_LABEL(player->speed.text), "1.0");
    gtk_widget_hide(player->speed.speed_box);
  }
}




