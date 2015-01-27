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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "imxplayer.h"
#include "playlistbox.h"

static void playlist_set_file(ImxPlayer *player, gchar *filename)
{
  gchar title[256];
  player->playengine->stop(player->playengine);
  player->playengine->set_file(player->playengine, filename);

  gchar *file = strrchr(filename, '/');
  if (file)
    file++;
  else
    file = filename;

  sprintf(title, "%s - %s", "IMXPlayer", file);
  gtk_window_set_title(GTK_WINDOW(player->top_window), title);
  player->playengine->play(player->playengine);

  gtk_range_set_value(GTK_RANGE(player->ctrlbar.progress), 0);
  gtk_label_set_text(GTK_LABEL(player->ctrlbar.current), "00:00");
  gtk_label_set_text(GTK_LABEL(player->ctrlbar.duration), "00:00");

  GtkWidget *image = gtk_image_new_from_file("./icons/pause.png");
  gtk_button_set_image(GTK_BUTTON(player->ctrlbar.play_pause), image);
#ifdef ENABLE_STOP_BUTTON
  image = gtk_image_new_from_file("./icons/stop.png");
  gtk_button_set_image(GTK_BUTTON(player->ctrlbar.play_stop), image);
#endif
//  player->metainfo_refresh = TRUE;
}

static void playlist_set_repeat(PlaylistBox *playlistbox, RepeatMode repeat)
{
  if (repeat < REPEAT_MAX)
    playlistbox->repeat = repeat;
  else
    playlistbox->repeat = REPEAT_OFF;
}

static RepeatMode playlist_get_repeat(PlaylistBox *playlistbox)
{
  return playlistbox->repeat;
}

static void playlist_next_previous(PlaylistBox *playlistbox, gboolean next)
{
  gboolean no_more = FALSE;
  GtkTreeModel *model;
  GtkTreeIter  iter;
  GtkWidget *image;
  gchar *name;
  gchar *folder;
  gchar pathname[256];
  gint num;

  ImxPlayer *player = (ImxPlayer *)playlistbox->player;

  if (playlistbox->count) {
    if (playlistbox->repeat == REPEAT_ONE) {
      // playlistbox->current no change
    } else if (playlistbox->repeat == REPEAT_RANDOM) {
      playlistbox->current = (random() % playlistbox->count) + 1;
    } else {
      if (next) {
        playlistbox->current++;
        if (playlistbox->current > playlistbox->count) {
          if (playlistbox->repeat == REPEAT_ALL) {
            playlistbox->current = 1;
          } else {
            playlistbox->current = playlistbox->count;
            no_more = TRUE;
          }
        }
      } else {
        playlistbox->current--;
        if (playlistbox->current <= 0) {
          if (playlistbox->repeat == REPEAT_ALL) {
            playlistbox->current = playlistbox->count;
          } else {
            playlistbox->current = 1;
            no_more = TRUE;
          }
        }
      }
    }

    if (no_more) {
      //stop the playing
      player->playengine->stop(player->playengine);
      g_print("No more %s file, stopped\n", next ? "next": "previous");
#ifdef ENABLE_STOP_BUTTON
      image = gtk_image_new_from_file("./icons/play.png");
      gtk_button_set_image(GTK_BUTTON(player->ctrlbar.play_stop), image);
#endif
      image = gtk_image_new_from_file("./icons/play.png");
      gtk_button_set_image(GTK_BUTTON(player->ctrlbar.play_pause), image);
    } else {
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (playlistbox->list));

      gtk_tree_model_iter_nth_child(model, &iter, NULL, playlistbox->current-1);
      gtk_tree_model_get(model, &iter, NUMBER, &num,
                          FILE_NAME, &name, FOLDER_PATH, &folder, -1);
      sprintf(pathname, "%s/%s", folder, name);
      g_print ("select and start play [%d]%s\n", num, pathname);

      if (num == playlistbox->current) {
        playlist_set_file(playlistbox->player, pathname);
      } else {
        g_print("Retrieve %s failed, index not match [%d:%d]\n",
                next ? "next": "previous", num, playlistbox->current);
      }
      g_free(name);
      g_free(folder);
    }
  } else {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(player->top_window),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_WARNING,
                                      GTK_BUTTONS_OK,
                                      "Playlist is empty");
    gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
}

static void playlist_set_next(PlaylistBox *playlistbox)
{
  playlist_next_previous(playlistbox, TRUE);
}

static void playlist_set_previous(PlaylistBox *playlistbox)
{
  playlist_next_previous(playlistbox, FALSE);
}

static void playlist_add_file(PlaylistBox *playlistbox, gchar *file,
                              gboolean clear, gboolean play)
{
  GtkListStore *store;
  GtkTreeIter  iter;
  GtkTreeModel *model;
  gchar pathname[256];

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (playlistbox->list));
  store = GTK_LIST_STORE(model);

  if (clear) {
    playlistbox->count = 0;
    playlistbox->current = 0;
    if (gtk_tree_model_get_iter_first(model, &iter))
      gtk_list_store_clear(store);
  }

  gtk_list_store_append(store, &iter);

  strncpy(pathname, file, 255);
  pathname[255] = 0;
  playlistbox->count++;
  gchar *filename = strrchr(pathname, '/');
  if (!filename) {
    filename = file;
    gtk_list_store_set(store, &iter,
                       NUMBER, playlistbox->count,
                       FILE_NAME, filename,
                       FOLDER_PATH, "",
                       -1);
  } else {
    *filename = '\0';
    filename++;
    gtk_list_store_set(store, &iter,
                       NUMBER, playlistbox->count,
                       FILE_NAME, filename,
                       FOLDER_PATH, pathname,
                       -1);
  }

  if (play) {
    playlist_set_file(playlistbox->player, file);
    playlistbox->current = playlistbox->count;
  }
}

static void playlist_add_folder(PlaylistBox *playlistbox, gchar *folder,
                                gboolean clear, gboolean play)
{
  DIR *dir;
  struct dirent *ent;
  struct stat st;
  GtkListStore *store;
  GtkTreeModel *model;
  GtkTreeIter  iter;
  gchar filename[256];
  gboolean start_play = play;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (playlistbox->list));
  store = GTK_LIST_STORE(model);
  //gint n_rows = gtk_tree_model_iter_n_children(store, NULL) + 1;

  if (clear) {
    playlistbox->count = 0;
    playlistbox->current = 0;
    if (gtk_tree_model_get_iter_first(model, &iter))
      gtk_list_store_clear(store);
  }

  gchar *folder_path = strchr(folder, ':');
  if (!folder_path)
    folder_path = folder;
  else
    folder_path++;

  dir = opendir (folder_path);
  if (dir) {
    ent = readdir(dir);
    while (ent) {
      sprintf(filename, "%s/%s", folder_path, ent->d_name);
      if (stat(filename, &st) == 0) {
        if (S_ISREG(st.st_mode)) {
/*
         // get the mime type
          gboolean certain = FALSE;
          char *content_type = g_content_type_guess (filename, NULL, 0, &certain);
          if (content_type != NULL) {
            char *mime_type = g_content_type_get_mime_type (content_type);
          }
*/
          playlistbox->count++;
          gtk_list_store_append(store, &iter);
          gtk_list_store_set(store, &iter,
                             NUMBER, playlistbox->count,
                             FILE_NAME, ent->d_name,
                             FOLDER_PATH, folder_path,
                             -1);

          if (start_play) {
            playlist_set_file(playlistbox->player, filename);
            playlistbox->current = playlistbox->count;
            start_play = FALSE;
          }
        }
      }
      ent = readdir(dir);
    }
    closedir(dir);
  } else {
    g_print("Open %s failed\n", folder_path);
  }
}

static void add_item(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;

  GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open File",
                                        GTK_WINDOW(player->top_window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
  GtkFileFilter *filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "video/*");
  gtk_file_filter_add_mime_type (filter, "audio/*");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    gchar *str = gtk_file_chooser_get_filename (chooser);
    playlist_add_file(&player->playlistbox, str, FALSE, FALSE);
    g_free (str);
  }

  gtk_widget_destroy (dialog);
}

static void add_dir_item(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;

  GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open Folder",
                                        GTK_WINDOW(player->top_window),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    gchar *str = gtk_file_chooser_get_uri (chooser);
    playlist_add_folder(&player->playlistbox, str, FALSE, FALSE);
    g_free (str);
  }

  gtk_widget_destroy (dialog);
}

static void remove_item(GtkWidget *widget, gpointer data)
{
  GtkListStore *store;
  GtkTreeModel *model;
  GtkTreeIter  iter;
  GtkTreeSelection *selection;
  gint n_rows;

  ImxPlayer *player = (ImxPlayer *)data;

  selection  = gtk_tree_view_get_selection(
                 GTK_TREE_VIEW(player->playlistbox.list));
  store = GTK_LIST_STORE(gtk_tree_view_get_model(
      GTK_TREE_VIEW (player->playlistbox.list)));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (player->playlistbox.list));

  if (gtk_tree_model_get_iter_first(model, &iter) == FALSE)
      return;

  if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection),
      &model, &iter)) {
    //update row number
    GtkTreeIter new_iter = iter;
    gtk_tree_model_get (model, &new_iter, NUMBER, &n_rows, -1);
    while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &new_iter)) {
      gtk_list_store_set(store, &new_iter, NUMBER, n_rows++, -1);
    }

    gtk_list_store_remove(store, &iter);
    player->playlistbox.count--;
  }
}

static void remove_all(GtkWidget *widget, gpointer data)
{
  GtkListStore *store;
  GtkTreeModel *model;
  GtkTreeIter  iter;

  ImxPlayer *player = (ImxPlayer *)data;
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (player->playlistbox.list));
  store = GTK_LIST_STORE(model);

  player->playlistbox.count = 0;
  player->playlistbox.current = 0;
  if (gtk_tree_model_get_iter_first(model, &iter) == FALSE)
      return;
  gtk_list_store_clear(store);
}

static void close_list(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  gtk_widget_hide(player->playlistbox.playlist_box);
  player->show_playlist = FALSE;
  if (player->video_w_pre > 0) {
    player->video_w = player->video_w_pre;
    player->video_w_pre = 0;
    player->playengine->set_render_rect(player->playengine, 0, 0,
                                        player->video_w, player->video_h);
    player->playengine->expose_video(player->playengine);
    subtitle_resize(player->fixed_ct, &player->subtitle, player->video_x,
            player->video_y, player->video_w, player->video_h);
  }
}

static void item_activate(GtkTreeView *treeview, GtkTreePath *path,
                     GtkTreeViewColumn *col, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model(treeview);

  if (gtk_tree_model_get_iter(model, &iter, path)) {
    gchar *name;
    gchar *folder;
    gchar pathname[256];
    gint num;

    ImxPlayer *player = (ImxPlayer *)data;
    gtk_tree_model_get(model, &iter, NUMBER, &num,
                        FILE_NAME, &name, FOLDER_PATH, &folder, -1);
    sprintf(pathname, "%s/%s", folder, name);
    g_print ("select and start play [%d]%s\n", num, pathname);

    playlist_set_file(player, pathname);
    player->playlistbox.current = num;

    g_free(name);
    g_free(folder);
  }
}

void playlistbox_create(void *imxplayer)
{
  GtkWidget *sw;
  GtkWidget *remove;
  GtkWidget *add;
  GtkWidget *adddir;
  GtkWidget *removeAll;
  GtkWidget *close;
  GtkWidget *hbox;
  GtkWidget *vbox;
  ImxPlayer *player = (ImxPlayer *)imxplayer;
  PlaylistBox *playlistbox = &player->playlistbox;

  sw = gtk_scrolled_window_new(NULL, NULL);
  playlistbox->playlist_box = gtk_event_box_new();
  playlistbox->list = gtk_tree_view_new();
  gtk_widget_set_size_request(playlistbox->playlist_box, PLAYLISTBOX_W,
                              PLAYLISTBOX_H);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(playlistbox->playlist_box), vbox);

  /*
  gtk_widget_modify_bg(playlistbox->playlist_box, GTK_STATE_NORMAL,
                       &player->color_key);
  gtk_fixed_put(GTK_FIXED(player->fixed_ct), playlistbox->playlist_box,
                player->window_w-PLAYLISTBOX_W, INFO_BOX_H);
*/

  hbox = gtk_hbox_new(TRUE, 1);
  add = gtk_button_new_with_label("Add");
  adddir = gtk_button_new_with_label("Add Folder");
  remove = gtk_button_new_with_label("Remove");
  removeAll = gtk_button_new_with_label("Remove All");
  close = gtk_button_new_with_label("Close");

  gtk_box_pack_start(GTK_BOX(hbox), add, FALSE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), adddir, FALSE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), remove, FALSE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), removeAll, FALSE, TRUE, 1);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(sw),
              GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw),
              GTK_SHADOW_ETCHED_IN);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (playlistbox->list), TRUE);

  gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 1);
  gtk_box_pack_end(GTK_BOX(vbox), close, FALSE, TRUE, 1);

  gtk_container_add(GTK_CONTAINER(sw), playlistbox->list);
  gtk_container_add(GTK_CONTAINER(player->fixed_ct), playlistbox->playlist_box);

  GtkCellRenderer    *renderer;
  GtkTreeViewColumn  *column;
  GtkListStore       *store;
  GtkTreeSelection *select;

  /* --- Column #1 --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(renderer), "foreground", "green", NULL);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (playlistbox->list),
                                     -1, "No.", renderer, "text", NUMBER, NULL);

  /* --- Column #2 --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(renderer), "foreground", "green", NULL);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (playlistbox->list),
                            -1, "File Name", renderer, "text", FILE_NAME, NULL);

  /* --- Column #3 --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(renderer), "foreground", "green", NULL);
  column = gtk_tree_view_column_new_with_attributes("Folder", renderer,
                                                    "text", FOLDER_PATH, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW (playlistbox->list), column);
  gtk_tree_view_column_set_visible(column, FALSE);

  store = gtk_list_store_new (NBR_COLUMNS, G_TYPE_UINT, G_TYPE_STRING,
                              G_TYPE_STRING);
  gtk_tree_view_set_model(GTK_TREE_VIEW (playlistbox->list),
                          GTK_TREE_MODEL(store));
  g_object_unref(store);

  select = gtk_tree_view_get_selection(GTK_TREE_VIEW(playlistbox->list));
  gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

  g_signal_connect(G_OBJECT(add), "clicked",
          G_CALLBACK(add_item), player);

  g_signal_connect(G_OBJECT(adddir), "clicked",
          G_CALLBACK(add_dir_item), player);

  g_signal_connect(G_OBJECT(remove), "clicked",
          G_CALLBACK(remove_item), player);

  g_signal_connect(G_OBJECT(removeAll), "clicked",
          G_CALLBACK(remove_all), player);

  g_signal_connect(G_OBJECT(close), "clicked",
            G_CALLBACK(close_list), player);

  g_signal_connect(G_OBJECT(playlistbox->list), "row-activated",
          G_CALLBACK (item_activate), player);

  playlistbox->previous = playlist_set_previous;
  playlistbox->next = playlist_set_next;
  playlistbox->set_repeat = playlist_set_repeat;
  playlistbox->get_repeat = playlist_get_repeat;
  playlistbox->add_file = playlist_add_file;
  playlistbox->add_folder = playlist_add_folder;

  playlistbox->player = player;
  playlistbox->repeat = REPEAT_OFF;
  playlistbox->count = 0;
  playlistbox->current = 0;
}

void playlistbox_show(PlaylistBox *playlistbox, gboolean show)
{
  if (show)
    gtk_widget_show(playlistbox->playlist_box);
  else
    gtk_widget_hide(playlistbox->playlist_box);
}

void playlistbox_resize(GtkWidget *container, PlaylistBox *playlistbox,
                        gint x, gint y, gint w, gint h)
{
  gtk_widget_set_size_request(playlistbox->playlist_box, w, h);
  gtk_fixed_move(GTK_FIXED(container), playlistbox->playlist_box, x, y);
}

