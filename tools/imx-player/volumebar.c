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
#include "controlbar.h"
#include "volumebar.h"

static void set_value(GtkWidget * widget, gpointer data)
{
  gdouble volume;
  GtkRange *range = (GtkRange *) widget;
  ImxPlayer *player = (ImxPlayer *) data;
  volume = gtk_range_get_value(range);

  player->playengine->set_volume(player->playengine, volume/100);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(player->volumebar.mute), FALSE);
  player->playengine->set_mute(player->playengine, FALSE);
}

static void toggle_mute(GtkWidget * widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *) data;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (widget)))
    player->playengine->set_mute(player->playengine, TRUE);
  else
    player->playengine->set_mute(player->playengine, FALSE);
}

void volumebar_create(void *imxplayer)
{
  ImxPlayer *player = (ImxPlayer *)imxplayer;
  VolumeBar *volumebar = &player->volumebar;
  gdouble volume;
  GtkWidget *vbox;
  GtkWidget *label;
  GdkColor color;

  volumebar->volumebar = gtk_event_box_new();
  vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_add(GTK_CONTAINER(volumebar->volumebar), vbox);
  gtk_widget_modify_bg(volumebar->volumebar, GTK_STATE_NORMAL,
                        &player->color_key);
  gtk_widget_set_size_request(volumebar->volumebar, VOLUME_BAR_W, VOLUME_BAR_H);
  gtk_fixed_put(GTK_FIXED(player->fixed_ct), volumebar->volumebar,
      (player->window_w - (player->window_w - CTRLBAR_W)/2 - CTRLBAR_BOTTON_W),
      (player->window_h - CTRLBAR_H - VOLUME_BAR_H - CTRLBAR_BOTTON_H));

  volumebar->volume = gtk_vscale_new_with_range(0.0, 100.0, 1.0);
  gtk_range_set_inverted(GTK_RANGE(volumebar->volume), TRUE);
  gtk_scale_set_value_pos(GTK_SCALE(volumebar->volume), GTK_POS_TOP);
  gtk_widget_set_size_request(volumebar->volume, VOLUME_BAR_W, VOLUME_BAR_H);
  g_signal_connect(G_OBJECT(volumebar->volume), "value_changed",
                    G_CALLBACK(set_value), (gpointer) player);
  volume = player->playengine->get_volume(player->playengine);
  gtk_range_set_value(GTK_RANGE(volumebar->volume), volume * 100);
  gdk_color_parse ("White", &color);
  gtk_widget_modify_fg (volumebar->volume, GTK_STATE_NORMAL, &color);

  volumebar->mute = gtk_check_button_new();
  g_signal_connect(G_OBJECT(volumebar->mute), "toggled",
                      G_CALLBACK(toggle_mute), (gpointer) player);

  label = gtk_label_new("Mute");
  gdk_color_parse ("Red", &color);
  gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &color);

  gtk_box_pack_start(GTK_BOX(vbox), volumebar->volume, TRUE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), volumebar->mute, FALSE, FALSE, 1);
  gtk_box_pack_end(GTK_BOX(vbox), label, FALSE, FALSE, 1);
}

void volumebar_show(VolumeBar *volumebar, gboolean show)
{
  if (show)
    gtk_widget_show(volumebar->volumebar);
  else
    gtk_widget_hide(volumebar->volumebar);
}
