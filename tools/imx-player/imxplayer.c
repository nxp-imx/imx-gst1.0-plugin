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

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include "imxplayer.h"
#include "controlbar.h"
#include "infobox.h"
#include "menubar.h"
#include "playlistbox.h"
#include "subtitle.h"
#include "volumebar.h"
#include "playengine.h"

void eos_callback(void* player);
void show_error(void* player, const gchar *error);
void state_change(void* player,
                  GstState old_st, GstState new_st, GstState pending_st);

static gboolean config_change(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  gint w, h;
  ImxPlayer *player = (ImxPlayer*)data;

  if (GDK_CONFIGURE == event->type) {
    gtk_window_get_size(GTK_WINDOW(player->top_window), &w, &h);
    //g_print("current window size, %d:%d\n", w, h);
    player->window_w = w;
    player->window_h = h;

    player->video_x = 0;
    player->video_w = w;

    if (player->fullscreen) {
      player->video_h = h;
      player->video_y = 0;
    } else {
      player->video_h = h - CTRLBAR_H - MENUBAR_H;
      player->video_y = MENUBAR_H;
    }

    gtk_widget_set_size_request(player->video_win,
                                player->video_w, player->video_h);
    gtk_fixed_move(GTK_FIXED(player->fixed_ct), player->video_win,
                             player->video_x, player->video_y);
    player->playengine->set_render_rect(player->playengine, 0, 0,
                               player->video_w, player->video_h);
    player->playengine->expose_video(player->playengine);

    gtk_fixed_move(GTK_FIXED(player->fixed_ct), player->infobox.info_box,
                   (w-INFO_BOX_W), MENUBAR_H);

    ctrlbar_resize(player->fixed_ct, &player->ctrlbar,
                    0, h-CTRLBAR_H, w, CTRLBAR_H);
    subtitle_resize(player->fixed_ct, &player->subtitle, player->video_x,
        player->video_y, player->video_w, player->video_h);
  }

  // return FALSE to let parent to handle this event further
  return FALSE;
}

static gboolean key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  switch (event->keyval) {
  case GDK_P:
  case GDK_p:
  case GDK_space:
    gtk_button_clicked(GTK_BUTTON(player->ctrlbar.play_pause));
    break;
#ifdef ENABLE_STOP_BUTTON
  case GDK_S:
  case GDK_s:
    gtk_button_clicked(GTK_BUTTON(player->ctrlbar.play_stop));
    break;
#endif
  case GDK_F:
  case GDK_f:
    gtk_button_clicked(GTK_BUTTON(player->ctrlbar.full));
    break;
  case GDK_i:
  case GDK_I:
    gtk_button_clicked(GTK_BUTTON(player->ctrlbar.info));
    break;
#ifdef ENABLE_REPEAT_MODE_BUTTON
  case GDK_R:
  case GDK_r:
    gtk_button_clicked(GTK_BUTTON(player->ctrlbar.repeat));
    break;
#endif
#ifdef ENABLE_STEP_SEEK_BUTTON
  case GDK_Right:
    gtk_button_clicked(GTK_BUTTON(player->ctrlbar.step_forward));
    break;
  case GDK_Left:
    gtk_button_clicked(GTK_BUTTON(player->ctrlbar.step_rewind));
    break;
#endif
  case GDK_Page_Up:
    gtk_button_clicked(GTK_BUTTON(player->ctrlbar.trick_forward));
    break;
  case GDK_Page_Down:
    gtk_button_clicked(GTK_BUTTON(player->ctrlbar.trick_backward));
    break;
  case GDK_Q:
  case GDK_q:
    gtk_main_quit();
    break;
  default:
    break;
  }

  return TRUE;
}

static gboolean button_press(GtkWidget *widget, GdkEvent  *event, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  if (GDK_2BUTTON_PRESS == event->type && 0x01 == event->button.button) {
/*
    g_print("%d,%d [%d,%d,%d,%d]\n",
        (gint)event->button.x_root, (gint)event->button.y_root,
        player->video_x, player->video_y, player->video_w, player->video_h);
*/
    if ((gint)event->button.x_root < player->video_w + player->video_x &&
        (gint)event->button.y_root < player->video_h + player->video_y) {
      gtk_button_clicked(GTK_BUTTON(player->ctrlbar.full));
    }
  }

  if (GDK_BUTTON_PRESS == event->type && 0x01 == event->button.button) {
    if (player->fullscreen) {
      if ((gint)event->button.x_root < player->video_w + player->video_x &&
          (gint)event->button.y_root < player->video_h+player->video_y-CTRLBAR_H
          && (gint)event->button.y_root > MENUBAR_H) {
        player->show_ctrlbar = !player->show_ctrlbar;
        ctrlbar_show(&player->ctrlbar, player->show_ctrlbar);
        player->show_menubar = !player->show_menubar;
        menubar_show(&player->menubar, player->show_menubar);
        if (!player->show_ctrlbar) {
          player->show_volbar = FALSE;
          volumebar_show(&player->volumebar, player->show_volbar);
        }

        if (player->show_ctrlbar || player->show_menubar ||
            player->show_playlist) {
          gdk_window_set_cursor(gtk_widget_get_window(player->top_window),
                gdk_cursor_new(GDK_HAND2));
        } else {
          gdk_window_set_cursor(gtk_widget_get_window(player->top_window),
                gdk_cursor_new(GDK_BLANK_CURSOR));
        }
      }
    }
  }
  return FALSE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  return FALSE;
}

static void destroy(GtkWidget *widget, gpointer data)
{
  gtk_main_quit();
}

static void video_win_realize(GtkWidget * widget, gpointer data)
{
#if GTK_CHECK_VERSION(2,18,0)
  // Tell Gtk+/Gdk to create a native window for this widget instead of
  // drawing onto the parent widget.
  // This is here just for pedagogical purposes, GDK_WINDOW_XID will call
  // it as well in newer Gtk versions
  if (!gdk_window_ensure_native(gtk_widget_get_window(widget)))
    g_error("Couldn't create native window needed for GstVideoOverlay!");
#endif
  ImxPlayer *player = ((ImxPlayer*)data);
  GtkWidget *win = GTK_WIDGET(widget);
  guintptr win_id = (guintptr)(GDK_WINDOW_XID(gtk_widget_get_window(win)));
  player->playengine->set_window(player->playengine, win_id);
  gdk_window_set_cursor(gtk_widget_get_window(player->top_window),
          gdk_cursor_new(GDK_HAND2));
}

#ifdef EXPOSE_VIDEO_FOR_EACH_EXPOSE_EVENT
static void video_expose_cb(GtkWidget * widget, gpointer data)
{
  ImxPlayer *player = ((ImxPlayer*)data);
  player->playengine->expose_video(player->playengine);
}
#endif

static gboolean position_update_cb(gpointer data) {
  gint64 elapsed;
  char cur_str[20];
  char dur_str[20];
  gint hour, minute, second;
  gint hour_d, minute_d, second_d;
  ImxPlayer *player = (ImxPlayer*)data;
  gint64 dur = player->playengine->get_duration(player->playengine);

  if (PLAYENGINE_PLAYING == player->playengine->get_state(player->playengine) &&
      dur > 0 ) {
    elapsed = player->playengine->get_position(player->playengine);

    hour = (elapsed/ (gint64)3600000000000);
    minute = (elapsed / (gint64)60000000000) - (hour * 60);
    second = (elapsed / 1000000000) - (hour * 3600) - (minute * 60);
    hour_d = (dur/ (gint64)3600000000000);
    minute_d = (dur / (gint64)60000000000) - (hour_d * 60);
    second_d = (dur / (gint64)1000000000) - (hour_d * 3600) - (minute_d * 60);

    if (hour_d > 0) {
      sprintf(cur_str, "%02d:%02d:%02d", hour, minute, second);
      sprintf(dur_str, "%02d:%02d:%02d", hour_d, minute_d, second_d);
    } else {
      sprintf(cur_str, "%02d:%02d", minute, second);
      sprintf(dur_str, "%02d:%02d", minute_d, second_d);
    }

    if (elapsed > 0) {
      double value = (elapsed * (((double) 100) / dur));
      gtk_range_set_value(GTK_RANGE(player->ctrlbar.progress), value);
      gtk_label_set_text(GTK_LABEL(player->ctrlbar.current), cur_str);
      gtk_label_set_text(GTK_LABEL(player->ctrlbar.duration), dur_str);
#if 0
      if (player->metainfo_refresh) {
        infobox_update(player);
        menubar_update(player);
        player->metainfo_refresh = FALSE;
      }
#endif
    }
  }
  return TRUE;
}

static guint imx_player_get_color_key(void)
{
  guint key = 0;
  const gchar *colorkey = g_getenv ("COLORKEY");
  if (colorkey) {
    if (strlen(colorkey) > 1)
      key = strtoul(colorkey, NULL, 16);
  }
  return key;
}

#ifdef ENABLE_SET_COLOR_KEY
static void imx_player_set_color_key(guint key)
{
  gchar str[10] = {0};
  sprintf(str, "%08x", key);
  setenv("COLORKEY", str, TRUE);
}
#endif

void eos_callback(void* player)
{
  ImxPlayer *imxplayer = (ImxPlayer *)player;
  if (imxplayer) {
    imxplayer->playlistbox.next(&imxplayer->playlistbox);
  }
}

void show_error(void* player, const gchar *error)
{
  GtkWidget *dialog;
  if (player) {
    dialog = gtk_message_dialog_new(
                                GTK_WINDOW(((ImxPlayer *)player)->top_window),
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_WARNING,
                                GTK_BUTTONS_OK,
                                "%s", error);
    gtk_window_set_title(GTK_WINDOW(dialog), "Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
}

void state_change(void* imxplayer,
                  GstState old_st, GstState new_st, GstState pending_st)
{
  if (old_st == GST_STATE_READY && new_st == GST_STATE_PAUSED) {
    ImxPlayer *player = (ImxPlayer *)imxplayer;
    imx_metadata *meta = &player->meta;
    player->playengine->get_metadata(player->playengine, meta);
    infobox_update(player);
    menubar_update(player);
  }
}

int main(int argc, char *argv[])
{
  ImxPlayer player = {0};
  GdkColor color;
  gchar colorstr[8] = {0};

  /* check if DISPLAY envionment variable set */
  const gchar *disp = g_getenv ("DISPLAY");
  if (!disp || strlen(disp) < 2) {
    /* DISPLAY not set, set to :0 */
    g_setenv("DISPLAY", ":0", TRUE);
  }

  /* check if ~/.local/share exit, this folder is used to record the recent
   * opened file or folder by GTK
   */
  gchar s1[64] = {0};
  gchar s2[64] = {0};
  const gchar *guname = g_get_user_name();
  g_sprintf(s1, "/home/%s/.local", guname);
  g_sprintf(s2, "/home/%s/.local/share", guname);

  if (!g_file_test(s1, G_FILE_TEST_EXISTS)) {
    g_mkdir(s1, 0755);
    g_mkdir(s2, 0755);
  } else {
    if (!g_file_test(s2, G_FILE_TEST_EXISTS)) {
      g_mkdir(s2, 0755);
    } else {
      if (!g_file_test(s2, G_FILE_TEST_IS_DIR)) {
        g_remove(s2);
        g_mkdir(s2, 0755);
      }
    }
  }

  /* load gtk style configuration */
  if (g_file_test(MY_GTK_STYLE_CONFIG_FILE, G_FILE_TEST_EXISTS)) {
    if (g_file_test(SYS_GTK_STYLE_CONFIG_FILE, G_FILE_TEST_EXISTS)) {
      system("cp /etc/gtk-2.0/gtkrc ./config/gtkrc.bak");
    }
    system("cp ./config/gtkrc /etc/gtk-2.0/gtkrc");
  }

  gtk_init(&argc, &argv);

  /* make sure the type is realized */
  GtkSettings *gtk_setting = gtk_settings_get_default ();
  g_type_class_unref (g_type_class_ref (GTK_TYPE_IMAGE_MENU_ITEM));
  g_type_class_unref (g_type_class_ref (GTK_TYPE_ICON_SIZE));
  g_object_set (gtk_setting, "gtk-menu-images", TRUE, NULL);
  g_object_set (gtk_setting, "gtk-icon-sizes",
      "gtk-menu=20,20:gtk-button=32,32", NULL);
  g_object_set (gtk_setting, "gtk-font-name", "Sans Bold 12", NULL);

  player.playengine = play_engine_create(&argc, &argv,
                                         eos_callback,
                                         show_error,
                                         state_change);
  player.playengine->player = &player;

  player.top_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GdkScreen* scr = gtk_window_get_screen( GTK_WINDOW(player.top_window));
  player.screen_w = gdk_screen_get_width(scr);
  player.screen_h = gdk_screen_get_height(scr);
  player.window_w = player.screen_w;
  player.window_h = player.screen_h;
  gtk_window_set_default_size(GTK_WINDOW(player.top_window),
                              player.screen_w, player.screen_h);
  gdk_color_parse ("Grey", &color);
  gtk_widget_modify_bg (player.top_window, GTK_STATE_NORMAL, &color);

#ifdef REMOVE_WINDOW_MANAGER_DECORATION
  gtk_window_fullscreen(GTK_WINDOW(window));
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
#endif

  player.fixed_ct = gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(player.top_window), player.fixed_ct);

  sprintf(colorstr, "#%06x",
      RGB565TOCOLORKEY(RGB888TORGB565(imx_player_get_color_key())));
  gdk_color_parse (colorstr, &player.color_key);
/*  g_print("%08x,%s,%08x,%08x,%08x\n", player.color_key, colorstr,
          bg_color.red, bg_color.green, bg_color.blue);*/

  menubar_create(&player);

  player.video_win = gtk_drawing_area_new();
  gtk_fixed_put(GTK_FIXED(player.fixed_ct), player.video_win, 0, MENUBAR_H);
  player.video_w = player.window_w;
  player.video_h = player.window_h - CTRLBAR_H - MENUBAR_H;
  gtk_widget_set_size_request(player.video_win, player.video_w, player.video_h);
  gtk_widget_modify_bg (player.video_win, GTK_STATE_NORMAL, &player.color_key);

  gtk_widget_add_events (player.top_window, GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events(player.top_window, GDK_STRUCTURE_MASK);
  g_signal_connect(player.video_win, "realize",
      G_CALLBACK (video_win_realize), &player);
  g_signal_connect(G_OBJECT (player.top_window), "delete_event",
      G_CALLBACK (delete_event), &player);
  g_signal_connect(G_OBJECT (player.top_window), "destroy",
      G_CALLBACK (destroy), &player);
  g_signal_connect(G_OBJECT (player.top_window), "key-press-event",
      G_CALLBACK (key_press), &player);
  g_signal_connect(G_OBJECT (player.top_window), "button-press-event",
      G_CALLBACK (button_press), &player);
  g_signal_connect(G_OBJECT (player.top_window), "configure-event",
      G_CALLBACK (config_change), &player);
#ifdef EXPOSE_VIDEO_FOR_EACH_EXPOSE_EVENT
  g_signal_connect(player.video_win, "expose-event",
      G_CALLBACK (video_expose_cb), &player);
#endif

  ctrlbar_create(&player);
  infobox_create(&player);
  playlistbox_create(&player);
  subtitle_create(&player);
  volumebar_create(&player);
  speedbox_create(&player);

  subtitle_set_text(&player.subtitle, "DEMO SUBTITLE TEXT");

//  player.metainfo_refresh = TRUE;
  player.show_ctrlbar = TRUE;
  player.show_menubar = TRUE;
  player.show_subtitle = FALSE;

  g_timeout_add(1000, position_update_cb, (gpointer)&player);
  GtkSettings *default_settings = gtk_settings_get_default();
  g_object_set(default_settings, "gtk-button-images", TRUE, NULL);

  gtk_widget_show_all(player.top_window);
  infobox_show(&player.infobox, player.show_info);
  volumebar_show(&player.volumebar, player.show_volbar);
  playlistbox_show(&player.playlistbox, player.show_playlist);
  speedbox_show(&player.speed, FALSE);
  subtitle_show(&player.subtitle, player.show_subtitle);

  if (argc > 1) {
    player.playlistbox.add_file(&player.playlistbox, argv[1], TRUE, TRUE);
  } else {
    gtk_window_set_title(GTK_WINDOW(player.top_window), "IMXPlayer");
  }

  gtk_main();

  play_engine_destroy(player.playengine);

  /* recover the gtk style configuration */
  if (g_file_test(BAK_GTK_STYLE_CONFIG_FILE, G_FILE_TEST_EXISTS)) {
    system("cp ./config/gtkrc.bak /etc/gtk-2.0/gtkrc");
    g_remove(BAK_GTK_STYLE_CONFIG_FILE);
  }

  return 0;
}
