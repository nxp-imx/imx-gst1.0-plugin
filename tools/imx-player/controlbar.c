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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imxplayer.h"
#include "controlbar.h"
#include "infobox.h"
#include "seektodialog.h"
#include "playlistbox.h"
#include "playengine.h"
#include "speedbox.h"

static void previous_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  player->playlistbox.previous(&player->playlistbox);
}

static void next_cb (GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  player->playlistbox.next(&player->playlistbox);
}

static void trick_play_forward_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  double rate = player->playengine->get_play_rate(player->playengine);

  if (rate <= 0)
    rate = 0.5;
  else if (rate >= 0.5 && rate < 1.0)
    rate = 1.0;
  else if (rate >= 1.0 && rate < 1.5)
    rate = 1.5;
  else if (rate >= 1.5 && rate < 2.0)
    rate = 2.0;
  else if (rate >= 2.0 && rate < 4.0)
    rate = 4.0;
  else if (rate >= 4.0 && rate < 8.0)
    rate = 8.0;
  else if (rate >= 8.0 && rate < 16.0)
    rate = 16.0;
  else
    rate = 0.5;

  player->playengine->set_play_rate(player->playengine, rate);
  speedbox_update(player, rate);
}

static void trick_play_backward_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  double rate = player->playengine->get_play_rate(player->playengine);

  if (rate >= 0)
    rate = -2.0;
  else if (rate > -4.0 && rate <= -2.0)
    rate = -4.0;
  else if (rate > -8.0 && rate <= -4.0)
    rate = -8.0;
  else if (rate > -16.0 && rate <= -8.0)
    rate = -16.0;
  else
    rate = 1.0;

  player->playengine->set_play_rate(player->playengine, rate);
  speedbox_update(player, rate);
}

#ifdef USE_IDLE_SEEK
typedef struct
{
  play_engine *engine;
  gint64  pos;
  gboolean accurate;
} seek_param;

static gboolean idle_seek(gpointer data)
{
  if (data) {
    seek_param *param = (seek_param *)data;
    if (param->engine)
      param->engine->seek(param->engine, param->pos, param->accurate);
    g_free(data);
  }

  /* once */
  return FALSE;
}
#endif

#ifdef ENABLE_STEP_SEEK_BUTTON
static void step_forward_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  gint64 cur = player->playengine->get_position(player->playengine);
  cur += (STEP_SEEK_STEP * GST_SECOND);
  player->playengine->seek(player->playengine, cur, FALSE);
}

static void step_rewind_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  gint64 cur = player->playengine->get_position(player->playengine);
  cur -= (STEP_SEEK_STEP * GST_SECOND);
  player->playengine->seek(player->playengine, cur, FALSE);
}
#endif

#ifdef ENABLE_STOP_BUTTON
static void stop_cb(GtkWidget *widget, gpointer data) {
  ImxPlayer *player = (ImxPlayer*)data;
  if (PLAYENGINE_STOPPED == player->playengine->get_state(player->playengine)) {
    player->playengine->play(player->playengine);
    GtkWidget *image = gtk_image_new_from_file("./icons/stop.png");
    gtk_button_set_image(GTK_BUTTON(widget), image);
  } else {
    player->playengine->stop(player->playengine);
    gtk_range_set_value(GTK_RANGE(player->ctrlbar.progress), 0);
    gtk_label_set_text(GTK_LABEL(player->ctrlbar.current), "00:00");
    gtk_label_set_text(GTK_LABEL(player->ctrlbar.duration), "00:00");
    GtkWidget *image = gtk_image_new_from_file("./icons/play.png");
    gtk_button_set_image(GTK_BUTTON(widget), image);
  }
}
#endif

static void pause_cb(GtkWidget *widget, gpointer data) {
  ImxPlayer *player = (ImxPlayer*)data;
  if (PLAYENGINE_PLAYING == player->playengine->get_state(player->playengine)) {
    player->playengine->pause(player->playengine);
    GtkWidget *image = gtk_image_new_from_file("./icons/play.png");
    gtk_button_set_image(GTK_BUTTON(widget), image);
  } else {
    player->playengine->play(player->playengine);
    GtkWidget *image = gtk_image_new_from_file("./icons/pause.png");
    gtk_button_set_image(GTK_BUTTON(widget), image);
  }
}

static void info_cb(GtkWidget *widget, gpointer data) {
  gint x, y, w, h;
  ImxPlayer *player = (ImxPlayer*)data;
  player->show_info = !player->show_info;
  if (player->show_playlist) {
    x = player->window_w - PLAYLISTBOX_W;
    w = PLAYLISTBOX_W;

    if (player->show_info)
      y = MENUBAR_H + INFO_BOX_H;
    else
      y = MENUBAR_H;

    h = player->window_h - y - CTRLBAR_H;
    playlistbox_resize(player->fixed_ct, &player->playlistbox, x, y, w, h);
  }
  infobox_show(&player->infobox, player->show_info);
}

#ifdef ENABLE_REPEAT_MODE_BUTTON
static void repeat_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  GtkWidget *image;
  RepeatMode repeat = player->playlistbox.get_repeat(&player->playlistbox);

  if (REPEAT_OFF == repeat) {
    repeat = REPEAT_ONE;
    image = gtk_image_new_from_file("./icons/repeat-one.png");
  } else if (REPEAT_ONE == repeat) {
    repeat = REPEAT_ALL;
    image = gtk_image_new_from_file("./icons/repeat-all.png");
  } else if (REPEAT_ALL == repeat) {
    repeat = REPEAT_RANDOM;
    image = gtk_image_new_from_file("./icons/repeat-random.png");
  } else {
    repeat = REPEAT_OFF;
    image = gtk_image_new_from_file("./icons/repeat-off.png");
  }

  player->playlistbox.set_repeat(&player->playlistbox, repeat);
  gtk_button_set_image(GTK_BUTTON(widget), image);
}
#endif

static void full_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  if (player->fullscreen) {
    player->fullscreen = FALSE;
#ifndef REMOVE_WINDOW_MANAGER_DECORATION
    gtk_window_unfullscreen(GTK_WINDOW(player->top_window));
    gtk_window_set_decorated(GTK_WINDOW(player->top_window), TRUE);
#else
    player->video_h = player->screen_h - CTRLBAR_H;
    player->video_w = player->screen_w;
    gtk_widget_set_size_request(player->video_win,
                                player->video_w, player->video_h);
    gtk_widget_show(player->ctrl_bar);
    player->hide_ctrl_bar = FALSE;
#endif
    GtkWidget *image = gtk_image_new_from_file("./icons/full-screen.png");
    gtk_button_set_image(GTK_BUTTON(widget), image);
    player->show_ctrlbar = TRUE;
    player->show_menubar = TRUE;
    subtitle_resize(player->fixed_ct, &player->subtitle, player->video_x,
            player->video_y, player->video_w, player->video_h);
    gdk_window_set_cursor(gtk_widget_get_window(player->top_window),
        gdk_cursor_new(GDK_HAND2));
  } else {
    player->fullscreen = TRUE;
#ifndef REMOVE_WINDOW_MANAGER_DECORATION
    gtk_window_fullscreen(GTK_WINDOW(player->top_window));
    gtk_window_set_decorated(GTK_WINDOW(player->top_window), FALSE);
#else
    player->video_h = player->screen_h;
    player->video_w = player->screen_w;
    gtk_widget_set_size_request(player->video_win,
                                player->video_w, player->video_h);
#endif
    player->playengine->set_render_rect(player->playengine, 0, 0,
                               player->video_w, player->video_h);
    player->playengine->expose_video(player->playengine);

    GtkWidget *image = gtk_image_new_from_file("./icons/unfull-screen.png");
    gtk_button_set_image(GTK_BUTTON(widget), image);
    player->show_ctrlbar = FALSE;
    player->show_menubar = FALSE;
    subtitle_resize(player->fixed_ct, &player->subtitle, player->video_x,
            player->video_y, player->video_w, player->video_h);
    gdk_window_set_cursor(gtk_widget_get_window(player->top_window),
        gdk_cursor_new(GDK_BLANK_CURSOR));
  }
  ctrlbar_show(&player->ctrlbar, player->show_ctrlbar);
  menubar_show(&player->menubar, player->show_menubar);
}

#ifdef ENABLE_DIRECT_SEEK_BUTTON
static void seekto_cb (GtkWidget *widget, gpointer data)
{
  gchar str[20];
  gint64 seekto;

  SeekToDialog *seektodialog = seekto_dialog_create();
  if (!seektodialog)
    return;

  if (GTK_RESPONSE_OK == seekto_dialog_run(seektodialog)) {
    seekto_dialog_get_time(seektodialog, str);
    gboolean accurate = seekto_dialog_get_accurate(seektodialog);

    gchar *sec = strrchr(str, ':');
    if (NULL == sec) {
      seekto = strtoul(str, NULL, 10);
    } else {
      seekto = strtoul((sec+1), NULL, 10);
      *sec = '\0';
      sec = strrchr(str, ':');
      if (NULL == sec) {
        seekto += 60 * strtoul(str, NULL, 10);
      } else {
        seekto += 60 * strtoul((sec+1), NULL, 10);
        *sec = '\0';
        seekto += 60 * 60 * strtoul(str, NULL, 10);
      }
    }

    seekto *= GST_SECOND;
    ImxPlayer *player = (ImxPlayer*)data;
    gint64 dur = player->playengine->get_duration(player->playengine);

    if (seekto > dur) {
      GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(player->top_window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_WARNING,
                                        GTK_BUTTONS_OK,
                                        "Seek point larger than duration");
      gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
    } else {
      player->playengine->seek(player->playengine, seekto, accurate);
    }
  }

  seekto_dialog_destroy(seektodialog);
}
#endif

#ifdef ENABLE_VOLUME_BUTTON
static void volume_cb (GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  player->show_volbar = !player->show_volbar;
  volumebar_show(&player->volumebar, player->show_volbar);
}
#endif

static gboolean progress_seek_cb(GtkRange *range, GtkScrollType scroll,
                              gdouble value, gpointer data)
{
  ImxPlayer *player = (ImxPlayer*)data;
  gint64 dur = player->playengine->get_duration(player->playengine);

  if (dur > 0) {
    gint64 to_seek = (gint64)((value / 100) * dur);
    gtk_range_set_value(GTK_RANGE(player->ctrlbar.progress), value);
#ifdef USE_IDLE_SEEK
    seek_param *pm = g_new0(seek_param, 1);
    if (pm) {
      pm->accurate = FALSE;
      pm->pos = to_seek;
      pm->engine = player->playengine;
      g_idle_add(idle_seek, (gpointer)pm);
    } else {
      player->playengine->seek(player->playengine, to_seek, FALSE);
    }
#else
    player->playengine->seek(player->playengine, to_seek,player->accurate_seek);
#endif
  }

  return TRUE;
}

void ctrlbar_create(void *imxplayer)
{
  GtkWidget *button;
  GdkColor color;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *hbox2;
  GtkWidget *image;
  GtkWidget *halign;

  ImxPlayer *player = (ImxPlayer *)imxplayer;
  CtrlBar *ctrlbar = &player->ctrlbar;

  ctrlbar->control_bar = gtk_event_box_new();
  gtk_widget_modify_bg (ctrlbar->control_bar, GTK_STATE_NORMAL,
                        &player->color_key);
  gtk_widget_set_size_request(ctrlbar->control_bar, CTRLBAR_W,
                              CTRLBAR_H + CTRLBAR_Y_OFFSET);
  gtk_fixed_put(GTK_FIXED(player->fixed_ct), ctrlbar->control_bar,
                          (player->window_w - CTRLBAR_W) / 2,
                          (player->window_h - CTRLBAR_H - CTRLBAR_Y_OFFSET));

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(ctrlbar->control_bar), vbox);

  hbox = gtk_hbox_new(FALSE, 0);
  halign = gtk_alignment_new(0.5, 1, 0, 0);
  gtk_container_add(GTK_CONTAINER(halign), hbox);
  gtk_box_pack_start(GTK_BOX(vbox), halign, FALSE, FALSE, 0);

  ctrlbar->current = gtk_label_new("00:00");
  ctrlbar->duration = gtk_label_new("00:00");
  gdk_color_parse ("White", &color);
  gtk_widget_modify_fg (ctrlbar->current, GTK_STATE_NORMAL, &color);
  gtk_widget_set_size_request(ctrlbar->current, CTRLBAR_TIME_W,
                              CTRLBAR_PROGRESS_H);
  gtk_widget_modify_fg (ctrlbar->duration, GTK_STATE_NORMAL, &color);
  gtk_widget_set_size_request(ctrlbar->duration, CTRLBAR_TIME_W,
                              CTRLBAR_PROGRESS_H);
  gtk_widget_modify_font(ctrlbar->current,
        pango_font_description_from_string("Tahoma Bold 12"));
  gtk_widget_modify_font(ctrlbar->duration,
          pango_font_description_from_string("Tahoma Bold 12"));

  ctrlbar->progress = gtk_hscale_new_with_range(0, 100, 1);
  g_signal_connect(G_OBJECT (ctrlbar->progress), "change-value",
      G_CALLBACK (progress_seek_cb), player);
  gtk_scale_set_draw_value(GTK_SCALE(ctrlbar->progress), FALSE);
  gtk_widget_set_size_request(ctrlbar->progress, CTRLBAR_W - (CTRLBAR_TIME_W*2),
                              CTRLBAR_PROGRESS_H);

  hbox2 = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox2), ctrlbar->current, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox2), ctrlbar->progress, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(hbox2), ctrlbar->duration, FALSE, FALSE, 0);

  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/previous.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (previous_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->previous = button;

  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/fast-backward.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (trick_play_backward_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->trick_backward = button;

#ifdef ENABLE_STEP_SEEK_BUTTON
  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/step-backward.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (step_rewind_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->step_rewind = button;
#endif

#ifdef ENABLE_STOP_BUTTON
  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/stop.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (stop_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->play_stop = button;
#endif

  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/play.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (pause_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->play_pause = button;

#ifdef ENABLE_STEP_SEEK_BUTTON
  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/step-forward.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (step_forward_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->step_forward = button;
#endif

  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/fast-forward.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (trick_play_forward_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->trick_forward = button;

  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/next-track.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (next_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->next = button;

  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/info.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (info_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->info = button;

#ifdef ENABLE_REPEAT_MODE_BUTTON
  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/repeat-off.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK (repeat_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->repeat = button;
#endif

  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/full-screen.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK (full_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->full = button;

#ifdef ENABLE_DIRECT_SEEK_BUTTON
  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/seek-to.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK (seekto_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->seekto = button;
#endif

#ifdef ENABLE_VOLUME_BUTTON
  button = gtk_button_new();
  image = gtk_image_new_from_file("./icons/volume.png");
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK (volume_cb), player);
  gtk_widget_set_size_request(button, CTRLBAR_BOTTON_W, CTRLBAR_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  ctrlbar->volume = button;
#endif
}

void ctrlbar_show(CtrlBar *ctrlbar, gboolean show)
{
  if (show)
    gtk_widget_show(ctrlbar->control_bar);
  else
    gtk_widget_hide(ctrlbar->control_bar);
}

void ctrlbar_resize(GtkWidget *container, CtrlBar *ctrlbar,
                    gint x, gint y, gint w, gint h)
{
  gtk_widget_set_size_request(ctrlbar->control_bar, w, h + CTRLBAR_Y_OFFSET);
  gtk_fixed_move(GTK_FIXED(container), ctrlbar->control_bar,
                  x, y - CTRLBAR_Y_OFFSET);
}
