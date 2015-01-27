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

#include <string.h>
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
                             "Container: \n"
                             "Seekable: \n"
                             "Videos: 0, Audios: 0, Subtitles: 0\n"
                             "Video:\n"
                             "Audio:\n"
                             );
  gdk_color_parse ("white", &color);
  gtk_widget_modify_fg (infobox->info_text, GTK_STATE_NORMAL, &color);
  gtk_widget_modify_font(infobox->info_text,
      pango_font_description_from_string("Tahoma Bold 12"));
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
  char str[INFO_TEXT_LEN_MAX];
  char s[256];
  gint ntracks, i;
  ImxPlayer *player = (ImxPlayer *)imxplayer;
  gint64 dur = player->playengine->get_duration(player->playengine);
  imx_metadata *meta = &player->meta;

  sprintf(str,
      "Title: %s\n"
      "Artist: %s\n"
      "Album: %s\n"
      "Year: %s\n"
      "Genre: %s\n"
      "Duration: %d\n"
      "Container: %s\n"
      "Seekable: %s\n"
      "Videos: [%d], Audios: [%d], Subtitles: [%d]\n",
      meta->title,
      meta->artist,
      meta->album,
      meta->year,
      meta->genre,
      (gint)(dur/1000000),
      meta->container,
      player->playengine->get_seekable(player->playengine) ? "Yes" : "No",
      meta->n_video, meta->n_audio, meta->n_subtitle);

  ntracks = meta->n_video;
  if (ntracks > MAX_VIDEO_TRACK_COUNT)
    ntracks = MAX_VIDEO_TRACK_COUNT;
  for (i = 0; i < ntracks; i++) {
    sprintf(s, "Video %d: %s, Language: %s\n        %d x %d (%d/%d)",
               i+1, meta->video_info[i].codec_type, meta->video_info[i].language,
               meta->video_info[i].width, meta->video_info[i].height,
               meta->video_info[i].framerate_numerator,
               meta->video_info[i].framerate_denominator);
    strncat(str, s, INFO_TEXT_LEN_MAX);

    if (meta->video_info[i].bitrate || !HIDE_INVALID_INFO) {
      sprintf(s, ", %d bps\n", meta->video_info[i].bitrate);
    } else {
      sprintf(s, "\n");
    }

    strncat(str, s, INFO_TEXT_LEN_MAX);
  }

  ntracks = meta->n_audio;
  if (ntracks > MAX_AUDIO_TRACK_COUNT)
    ntracks = MAX_AUDIO_TRACK_COUNT;
  for (i = 0; i < ntracks; i++) {
    sprintf(s,"Audio %d: %s, Language: %s\n        %d Channels, %d Hz",
              i+1, meta->audio_info[i].codec_type, meta->audio_info[i].language,
              meta->audio_info[i].channels, meta->audio_info[i].samplerate);
    strncat(str, s, INFO_TEXT_LEN_MAX);

    if (meta->audio_info[i].bitrate || !HIDE_INVALID_INFO) {
      sprintf(s, ", %d bps\n", meta->audio_info[i].bitrate);
    } else {
      sprintf(s, "\n");
    }

    strncat(str, s, INFO_TEXT_LEN_MAX);
  }

  ntracks = meta->n_subtitle;
  if (ntracks > MAX_SUBTITLE_TRACK_COUNT)
    ntracks = MAX_SUBTITLE_TRACK_COUNT;
  for (i = 0; i < ntracks; i++) {
    sprintf(s,"Subtitle %d: %s, Language: %s\n",
              i+1, meta->subtitle_info[i].codec_type,
              meta->subtitle_info[i].language);
    strncat(str, s, INFO_TEXT_LEN_MAX);
  }

  gtk_label_set_text(GTK_LABEL(player->infobox.info_text), str);
}
