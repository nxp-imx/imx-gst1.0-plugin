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
#include "infobox.h"

void infobox_create(void *imxplayer)
{
  GdkColor color;
  ImxPlayer *player = (ImxPlayer *)imxplayer;
  InfoBox *infobox = &player->infobox;
  infobox->info_box = gtk_event_box_new();
  gtk_widget_modify_bg(infobox->info_box, GTK_STATE_NORMAL, &player->color_key);
  gtk_widget_set_size_request(infobox->info_box, INFO_BOX_W, INFO_BOX_H);
  gtk_fixed_put(GTK_FIXED(player->fixed_ct), infobox->info_box,
                player->window_w-INFO_BOX_W, MENUBAR_H);
  infobox->info_text = gtk_label_new(
                             "Title: \n"
                             "Artist: \n"
                             "Album: \n"
                             "Year: \n"
                             "Genre: \n"
                             "Duration: \n"
                             "Seekable: \n"
                             "Number of Videos: \n"
                             "Number of Audios: \n"
                             "Number of subtitles: \n"
                             "Video:\n"
                             "      Width: \n"
                             "      Height: \n"
                             "      Frame Rate: xx\n"
                             "      BitRate: \n"
                             "      Codec: \n"
                             "Audio:\n"
                             "      Channels: \n"
                             "      Sample Rate: \n"
                             "      Bitrate: \n"
                             "      Codec: \n"
                             );
  gdk_color_parse ("white", &color);
  gtk_widget_modify_fg (infobox->info_text, GTK_STATE_NORMAL, &color);
  gtk_widget_modify_font(infobox->info_text,
      pango_font_description_from_string("Tahoma Bold 10"));
  gtk_container_add(GTK_CONTAINER(infobox->info_box), infobox->info_text);
}

void infobox_show(InfoBox *infobox, gboolean show)
{
  if (show)
    gtk_widget_show(infobox->info_box);
  else
    gtk_widget_hide(infobox->info_box);

}

void infobox_update(void *imxplayer)
{
  char str[1024];
  ImxPlayer *player = (ImxPlayer *)imxplayer;
  imx_metadata *meta = &player->meta;
  player->playengine->get_metadata(player->playengine, meta);
  gint64 dur = player->playengine->get_duration(player->playengine);

  sprintf(str,
      "Title: %s\n"
      "Artist: %s\n"
      "Album: %s\n"
      "Year: %s\n"
      "Genre: %s\n"
      "Duration: %d\n"
      "Seekable: %s\n"
      "Number of Videos: %d\n"
      "Number of Audios: %d\n"
      "Number of subtitles: %d\n"
      "Video:\n"
      "      Width: %d\n"
      "      Height: %d\n"
      "      Frame Rate: %d\n"
      "      BitRate: %d\n"
      "      Codec: %s\n"
      "Audio:\n"
      "      Channels: %d\n"
      "      Sample Rate: %d\n"
      "      Bitrate: %d\n"
      "      Codec: %s",
      meta->title, meta->artist, meta->album,
      meta->year, meta->genre, (gint)(dur/1000000),
      player->playengine->get_seekable(player->playengine) ? "Yes" : "No",
      player->playengine->get_video_num(player->playengine),
      player->playengine->get_audio_num(player->playengine),
      player->playengine->get_subtitle_num(player->playengine), meta->width,
      meta->height, meta->framerate, meta->videobitrate, meta->videocodec,
      meta->channels, meta->samplerate, meta->audiobitrate, meta->audiocodec);

  gtk_label_set_text(GTK_LABEL(player->infobox.info_text), str);
}
