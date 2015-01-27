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

#ifndef PLAYLISTBOX_H_
#define PLAYLISTBOX_H_

enum
{
  NUMBER = 0,
  FILE_NAME,
  FOLDER_PATH,
  NBR_COLUMNS
};

typedef enum
{
  REPEAT_OFF,
  REPEAT_ONE,
  REPEAT_ALL,
  REPEAT_RANDOM,
  REPEAT_MAX
} RepeatMode;

typedef struct _PlaylistBox
{
  GtkWidget *playlist_box;
  GtkWidget *list;
  void *player;
  RepeatMode repeat;
  gint count;
  gint current;

  void (*previous) (struct _PlaylistBox *playlistbox);
  void (*next) (struct _PlaylistBox *playlistbox);
  void (*set_repeat) (struct _PlaylistBox *playlistbox, RepeatMode repeat);
  RepeatMode (*get_repeat) (struct _PlaylistBox *playlistbox);
  void (*add_file) (struct _PlaylistBox *playlistbox, gchar *file,
                    gboolean clear, gboolean play);
  void (*add_folder) (struct _PlaylistBox *playlistbox, gchar *folder,
                      gboolean clear, gboolean play);
} PlaylistBox;

void playlistbox_create(void *imxplayer);
void playlistbox_show(PlaylistBox *playlistbox, gboolean show);
void playlistbox_resize(GtkWidget *container, PlaylistBox *playlistbox,
                        gint x, gint y, gint w, gint h);

#endif /* PLAYLISTBOX_H_ */
