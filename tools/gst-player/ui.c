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
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include "gst-backend.h"

//#define EXPOSE_VIDEO_FOR_EACH_EXPOSE_EVENT
//#define REMOVE_WINDOW_MANAGER_DECORATION
#define TEST_UI_LAYOUT

#define DEFAULT_VIDEO_W   1024
#define DEFAULT_VIDEO_H   768
#define MIN_VIDEO_W       80
#define MIN_VIDEO_H       48
#define DEFAULT_BOTTON_W  50
#define DEFAULT_BOTTON_H  50
#define DEFAULT_BOTTON_GAP 1
#define DEFAULT_INFO_W    500
#define DEFAULT_INFO_H    300

#define WINDOW_MOVE_STEP  50
#define WINDOW_SIZE_STEP  10

static gboolean fullscreen = FALSE;
static gboolean metadata_got = FALSE;
static gboolean show_info = FALSE;
static gboolean show_control_bar = TRUE;
static gboolean initial_play = FALSE;
static gboolean stopped = FALSE;
static gint video_size_mode = 0;
static GtkWidget *window;
static GtkWidget *fixed;
static GtkWidget *scale;
static GtkWidget *escape_label;
static GtkWidget *info_label;
static GtkWidget *info_box;
static GtkWidget *control_bar;
static GtkWidget *video_output;
static GtkWidget *play_stop;
static gint screen_w, screen_h;
static gint window_w, window_h;
static gint video_x, video_y;
static gint video_w = DEFAULT_VIDEO_W, video_h = DEFAULT_VIDEO_H;

#define DURATION_IS_VALID(x) (x != 0 && x != (guint64) -1)
#define FF_STEP 10

static void layout_config_change(void)
{
  gint w, h;
  gtk_window_get_size(window, &w, &h);
  //g_print("current window size, %d:%d\n", w, h);
  window_w = w;
  window_h = h;

  video_size_mode = 0;
  video_x = 0;
  video_y = 0;
  video_w = w;
  if (fullscreen)
    video_h = h;
  else
    video_h = h - DEFAULT_BOTTON_H;

  gtk_widget_set_size_request(video_output, video_w, video_h);
  gtk_fixed_move(GTK_FIXED(fixed), video_output, video_x, video_y);
  backend_set_render_rect(0, 0, video_w, video_h);
  backend_video_expose();

  gtk_fixed_move(GTK_FIXED(fixed), info_box, w-DEFAULT_INFO_W, 0);

  gtk_widget_set_size_request(control_bar, w, DEFAULT_BOTTON_H);
  gtk_fixed_move(GTK_FIXED(fixed), control_bar, 0, h-DEFAULT_BOTTON_H);
  gtk_widget_show(control_bar);
}

static void toggle_paused(GtkWidget *pause_button) {
  static gboolean paused = FALSE;
  if (paused) {
    backend_resume();
    gtk_button_set_label(GTK_BUTTON(pause_button), "Pause");
    paused = FALSE;
  } else {
    backend_pause();
    gtk_button_set_label(GTK_BUTTON(pause_button), "Play");
    paused = TRUE;
  }
}

static void toggle_fullscreen(GtkWidget *widget, gpointer data) {
  if (fullscreen) {
#ifndef REMOVE_WINDOW_MANAGER_DECORATION
    gtk_widget_hide(control_bar);
    gtk_window_unfullscreen(GTK_WINDOW(window));
    gtk_window_set_decorated(GTK_WINDOW(window), TRUE);
#else
    video_h = screen_h - DEFAULT_BOTTON_H;
    video_w = screen_w;
    gtk_widget_set_size_request(video_output, video_w, video_h);
    gtk_widget_show(control_bar);
#endif
    video_size_mode = 0;
    fullscreen = FALSE;
  } else {
#ifndef REMOVE_WINDOW_MANAGER_DECORATION
    gtk_widget_hide(control_bar);
    gtk_window_fullscreen(GTK_WINDOW(window));
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
#else
    video_h = screen_h;
    video_w = screen_w;
    gtk_widget_set_size_request(video_output, video_w, video_h);
#endif
    backend_set_render_rect(0, 0, video_w, video_h);
    backend_video_expose();
    //gtk_widget_queue_draw (control_bar);
    fullscreen = TRUE;
  }
}

static void info_cb(GtkWidget *widget, gpointer data) {
  if (show_info) {
    gtk_widget_hide(info_box);
    show_info = FALSE;
  } else {
    gtk_widget_show(info_box);
    show_info = TRUE;
  }
}

static gboolean
button_press_cb(GtkWidget *widget, GdkEvent  *event, gpointer data) {
  if (GDK_2BUTTON_PRESS == event->type && 0x01 == event->button.button) {
    g_print("double click left mouse\n");
    toggle_fullscreen(widget, data);
  }

  if (GDK_BUTTON_PRESS == event->type && 0x01 == event->button.button) {
    if (fullscreen) {
      if (show_control_bar) {
        gtk_widget_hide(control_bar);
        show_control_bar = FALSE;
      } else {
        gtk_widget_show(control_bar);
        show_control_bar = TRUE;
      }
    }
  }
  return FALSE;
}

static gboolean
config_change_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
  gint x, y, w, h;
  if (GDK_CONFIGURE == event->type) {
    x = event->configure.x;
    y = event->configure.y;
    w = event->configure.width;
    h = event->configure.height;
    g_print("x:y:w:h = %d:%d:%d:%d\n", x, y, w, h);
    layout_config_change();
  }

  // return FALSE to let parent to handle this event further
  return FALSE;
}

static void open_cb(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog;
  gint res;
  dialog = gtk_file_chooser_dialog_new ("Open File",
                                        GTK_WINDOW(data),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
  GtkFileFilter *filter = gtk_file_filter_new ();
  //gtk_file_filter_add_pattern (filter, "*");
  gtk_file_filter_add_mime_type (filter, "video/*");
  gtk_file_filter_add_mime_type (filter, "audio/*");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

  res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res == GTK_RESPONSE_ACCEPT)
    {
      char *filename;
      GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
      filename = gtk_file_chooser_get_filename (chooser);
      g_print("Selected file : %s\n", filename);
      backend_stop();
      gtk_range_set_value(GTK_RANGE(scale), 0);
      gtk_label_set_text(GTK_LABEL(escape_label), "00:00/00:00");
      metadata_got = FALSE;
      backend_set_file_location(filename);
      backend_play();
      gtk_button_set_label(GTK_BUTTON(play_stop), "Stop");
      stopped = FALSE;
      g_free (filename);
    }

  gtk_widget_destroy (dialog);
}

static void pause_cb(GtkWidget *widget, gpointer data) {
  toggle_paused(widget);
}

static void stop_cb(GtkWidget *widget, gpointer data) {
  if (stopped) {
    backend_play();
    gtk_button_set_label(GTK_BUTTON(play_stop), "Stop");
    stopped = FALSE;
  } else {
    backend_stop();
    gtk_range_set_value(GTK_RANGE(scale), 0);
    gtk_label_set_text(GTK_LABEL(escape_label), "00:00:00/00:00:00");
    gtk_button_set_label(GTK_BUTTON(widget), "Play");
    gtk_widget_hide(info_box);
    show_info = FALSE;
    stopped = TRUE;
  }
}

static gboolean
delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
  return FALSE;
}

static void destroy(GtkWidget *widget, gpointer data) {
  gtk_main_quit();
}

static void FF(GtkWidget *widget, gpointer data) {
  backend_seek(FF_STEP);
}

static void FREW(GtkWidget *widget, gpointer data) {
  backend_seek(-FF_STEP);
}

static void move_left(GtkWidget *widget, gpointer data) {
  if (video_x > WINDOW_MOVE_STEP)
    video_x -= WINDOW_MOVE_STEP;
  else
    video_x = 0;

  gtk_fixed_move(GTK_FIXED(fixed), GTK_WIDGET(data), video_x, video_y);
}

static void move_right(GtkWidget *widget, gpointer data) {
  if (video_x + video_w < (window_w - WINDOW_MOVE_STEP))
    video_x += WINDOW_MOVE_STEP;
  else
    video_x = window_w - video_w;

  gtk_fixed_move(GTK_FIXED(fixed), GTK_WIDGET(data), video_x, video_y);
}

static void move_up(GtkWidget *widget, gpointer data) {
  if (video_y > WINDOW_MOVE_STEP)
    video_y -= WINDOW_MOVE_STEP;
  else
    video_y = 0;

  gtk_fixed_move(GTK_FIXED(fixed), GTK_WIDGET(data), video_x, video_y);
}

static void move_down(GtkWidget *widget, gpointer data) {
  if (video_y + video_h < (window_h - WINDOW_MOVE_STEP))
    video_y += WINDOW_MOVE_STEP;
  else
    video_y = window_h - video_h;

  gtk_fixed_move(GTK_FIXED(fixed), GTK_WIDGET(data), video_x, video_y);
}

static void resize_cb(GtkWidget *widget, gpointer data) {
  video_size_mode++;
  if (video_size_mode > 6)
    video_size_mode = 0;

  switch(video_size_mode) {
  case 1: //1/2 left top
    video_w = window_w / 2;
    video_h = window_h / 2;
    video_x = 0;
    video_y = 0;
    break;
  case 2: //1/4 right top
    video_w = window_w / 4;
    video_h = window_h / 4;
    video_x = window_w / 2;
    video_y = 0;
    break;
  case 3: //1/2 left bottom
    video_w = window_w / 2;
    video_h = window_h / 2;
    video_x = 0;
    video_y = window_h / 2;
    break;
  case 4: //1/4 right bottom
    video_w = window_w / 4;
    video_h = window_h / 4;
    video_x = window_w / 2;
    video_y = window_h / 2;
    break;
  case 5: //1/8 center
    video_w = window_w / 8;
    video_h = window_h / 8;
    video_x = (window_w - window_w / 8) / 2;
    video_y = (window_h - window_w / 8) / 2;
    break;
  case 6: //full window but 1/2 render rectangle
    video_w = window_w;
    video_h = window_h;
    video_x = 0;
    video_y = 0;
    backend_set_render_rect(video_w/4, video_h/4, video_w/2, video_h/2);
    break;
  default: //full window
    video_w = window_w;
    video_h = window_h;
    video_x = 0;
    video_y = 0;
    backend_set_render_rect(0, 0, video_w, video_h);
    backend_video_expose();
    break;
  }

  gtk_widget_set_size_request(GTK_WIDGET(data), video_w, video_h);
  gtk_fixed_move(GTK_FIXED(fixed), GTK_WIDGET(data), video_x, video_y);
}

static gboolean
key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
  switch (event->keyval) {
  case GDK_P:
  case GDK_p:
  case GDK_space:
    toggle_paused(widget);
    break;
  case GDK_F:
  case GDK_f:
    toggle_fullscreen(widget, data);
    break;
  case GDK_R:
  case GDK_r:
    backend_reset();
    break;
  case GDK_Right:
    FF(widget, data);
    break;
  case GDK_Left:
    FREW(widget, data);
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

static void seek_cb(GtkRange *range, GtkScrollType scroll, gdouble value,
    gpointer data) {
  guint64 to_seek;
  guint64 dur;

  dur = backend_query_duration();

  if (!DURATION_IS_VALID(dur))
    return;

  to_seek = (value / 100) * dur;
  backend_seek_absolute(to_seek);
}

static void video_widget_realize_cb(GtkWidget * widget, gpointer data) {
#if GTK_CHECK_VERSION(2,18,0)
  // Tell Gtk+/Gdk to create a native window for this widget instead of
  // drawing onto the parent widget.
  // This is here just for pedagogical purposes, GDK_WINDOW_XID will call
  // it as well in newer Gtk versions
  if (!gdk_window_ensure_native(gtk_widget_get_window(widget)))
    g_error("Couldn't create native window needed for GstVideoOverlay!");
#endif
}

#ifdef EXPOSE_VIDEO_FOR_EACH_EXPOSE_EVENT
static void video_expose_cb(GtkWidget * widget, gpointer data) {
  backend_video_expose();
}
#endif

static gboolean position_update(gpointer data) {
  guint64 elapsed;
  guint64 dur;
  char str[1024];
  gint64 hour, minute, second;
  gint64 hour_d, minute_d, second_d;

  if (!backend_is_playing()) {
    return TRUE;
  }

  elapsed = backend_query_position();
  dur = backend_query_duration();

  if (!DURATION_IS_VALID(dur))
    return TRUE;

  hour = (elapsed/ (gint64)3600000000000);
  minute = (elapsed / (gint64)60000000000) - (hour * 60);
  second = (elapsed / 1000000000) - (hour * 3600) - (minute * 60);
  hour_d = (dur/ (gint64)3600000000000);
  minute_d = (dur / (gint64)60000000000) - (hour_d * 60);
  second_d = (dur / 1000000000) - (hour_d * 3600) - (minute_d * 60);

  if (hour_d > 0)
    sprintf(str, "%02d:%02d:%02d/%02d:%02d:%02d", (gint)hour, (gint)minute,
            (gint)second, (gint)hour_d, (gint)minute_d, (gint)second_d);
  else
    sprintf(str, "%02d:%02d/%02d:%02d", (gint)minute, (gint)second,
            (gint)minute_d, (gint)second_d);

  if (elapsed != 0) {
    double value;
    value = (elapsed * (((double) 100) / dur));
    gtk_range_set_value(GTK_RANGE(scale), value);

    gtk_label_set_text(GTK_LABEL(escape_label), str);

    if (!metadata_got) {
      metadata_got = TRUE;
      backend_metadata *meta = backend_get_metadata();
      sprintf(str,
          "URI: %s\n"
          "Title: %s\n"
          "Artist: %s\n"
          "Album: %s\n"
          "Year: %s\n"
          "Genre: %s\n"
          "Duration: %d\n"
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
          meta->pathname, meta->title, meta->artist, meta->album,
          meta->year, meta->genre, (gint)(dur/1000000), meta->width,
          meta->height, meta->framerate, meta->videobitrate, meta->videocodec,
          meta->channels, meta->samplerate, meta->audiobitrate,
          meta->audiocodec);
      gtk_label_set_text(GTK_LABEL(info_label), str);
    }
  }

  return TRUE;
}

#ifdef TEST_UI_LAYOUT
static void create_ui(void) {
  GtkWidget *button;
  GdkColor bg_color, color;

  gulong color_key = backend_get_color_key();
  g_print("Get color key = %08x\n", (guint)color_key);
  bg_color.pixel = 0;
  bg_color.red = (color_key & 0xFF0000) >> 16;
  bg_color.green = (color_key & 0xFF00) >> 8;
  bg_color.blue = (color_key & 0xFF);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  GdkScreen* scr = gtk_window_get_screen( GTK_WINDOW( window));
  screen_w = gdk_screen_get_width(scr);
  screen_h = gdk_screen_get_height(scr);

  gtk_window_set_title(GTK_WINDOW(window), "GstXplayer");
  gdk_color_parse ("Grey", &color);
  gtk_widget_modify_bg (window, GTK_STATE_NORMAL, &color);

  window_w = screen_w;
  window_h = screen_h;
  gtk_window_set_default_size(GTK_WINDOW(window), screen_w, screen_h);

#ifdef REMOVE_WINDOW_MANAGER_DECORATION
  gtk_window_fullscreen(GTK_WINDOW(window));
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
#endif

  fixed = gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(window), fixed);

  video_output = gtk_drawing_area_new();
  gtk_fixed_put(GTK_FIXED(fixed), video_output, 0, 0);
  video_w = window_w;
  video_h = window_h - DEFAULT_BOTTON_H;
  gtk_widget_set_size_request(video_output, video_w, video_h);
  //gdk_color_parse ("green", &color);
  gtk_widget_modify_bg (video_output, GTK_STATE_NORMAL, &bg_color);

  gtk_widget_add_events (window, GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events(window, GDK_STRUCTURE_MASK);
  g_signal_connect(video_output, "realize",
      G_CALLBACK (video_widget_realize_cb), NULL);
  g_signal_connect(G_OBJECT (window), "delete_event",
      G_CALLBACK (delete_event), NULL);
  g_signal_connect(G_OBJECT (window), "destroy",
      G_CALLBACK (destroy), NULL);
  g_signal_connect(G_OBJECT (window), "key-press-event",
      G_CALLBACK (key_press), video_output);
  g_signal_connect(G_OBJECT (window), "button-press-event",
      G_CALLBACK (button_press_cb), NULL);
  g_signal_connect(G_OBJECT (window), "configure-event",
      G_CALLBACK (config_change_cb), NULL);
#ifdef EXPOSE_VIDEO_FOR_EACH_EXPOSE_EVENT
  g_signal_connect(video_output, "expose-event",
      G_CALLBACK (video_expose_cb), NULL);
#endif

  //Box to hold the control bar
  control_bar = gtk_event_box_new();
  GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(control_bar), hbox);

  gtk_widget_modify_bg (control_bar, GTK_STATE_NORMAL, &bg_color);
  gtk_widget_set_size_request(control_bar, window_w, DEFAULT_BOTTON_H);
  gtk_fixed_put(GTK_FIXED(fixed), control_bar, 0, window_h-DEFAULT_BOTTON_H);

  button = gtk_button_new_with_label("Open");
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (open_cb), window);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  play_stop = gtk_button_new_with_label("Stop");
  g_signal_connect(G_OBJECT (play_stop), "clicked", G_CALLBACK (stop_cb), NULL);
  gtk_widget_set_size_request(play_stop, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), play_stop, FALSE, FALSE, 0);

  button = gtk_button_new_with_label("Close");
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (destroy), NULL);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label("<<");
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (FREW), NULL);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label(">>");
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (FF), NULL);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label("Pause");
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (pause_cb), button);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label("Info");
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK (info_cb), button);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  //button for move window
  button = gtk_button_new_with_label("<");
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (move_left), video_output);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label(">");
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (move_right), video_output);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label("^");
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (move_up), video_output);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label("v");
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (move_down), video_output);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label("Resize");
  g_signal_connect(G_OBJECT (button), "clicked",
      G_CALLBACK (resize_cb), video_output);
  gtk_widget_set_size_request(button, DEFAULT_BOTTON_W, DEFAULT_BOTTON_H);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  scale = gtk_hscale_new_with_range(0, 100, 1);
  g_signal_connect(G_OBJECT (scale), "change-value",
      G_CALLBACK (seek_cb), NULL);
  g_timeout_add(1000, position_update, scale);
  gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
  gtk_box_pack_start(GTK_BOX(hbox), scale, TRUE, TRUE, 0);

  escape_label = gtk_label_new("00:00/00:00");
  gdk_color_parse ("White", &color);
  gtk_widget_modify_fg (escape_label, GTK_STATE_NORMAL, &color);
  gtk_widget_set_size_request(escape_label, 80, DEFAULT_BOTTON_H);
  gtk_box_pack_end(GTK_BOX(hbox), escape_label, FALSE, FALSE, 0);

  //Box to show media information
  info_box = gtk_event_box_new();
  gtk_widget_modify_bg (info_box, GTK_STATE_NORMAL, &bg_color);
  gtk_widget_set_size_request(info_box, DEFAULT_INFO_W, DEFAULT_INFO_H);
  gtk_fixed_put(GTK_FIXED(fixed), info_box, window_w-DEFAULT_INFO_W, 0);
  info_label = gtk_label_new("URI: \n"
                             "Title: \n"
                             "Artist: \n"
                             "Album: \n"
                             "Year: \n"
                             "Genre: \n"
                             "Duration: \n"
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
  gtk_widget_modify_fg (info_label, GTK_STATE_NORMAL, &color);
  gtk_container_add(GTK_CONTAINER(info_box), info_label);

  gtk_widget_show_all(window);
  gtk_widget_hide(info_box);
}
#else
  //New UI for normal player application
/*
  GtkWidget *vbox;
  GtkWidget *toolbar;
  GtkToolItem *open;
  GtkToolItem *exit;
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  toolbar = gtk_toolbar_new();
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
  gtk_container_set_border_width(GTK_CONTAINER(toolbar), 2);
  open = gtk_tool_button_new_from_stock(GTK_STOCK_OPEN);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), open, -1);
  exit = gtk_tool_button_new_from_stock(GTK_STOCK_QUIT);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), exit, -1);
  g_signal_connect(G_OBJECT(exit), "clicked", G_CALLBACK(gtk_main_quit), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 5);
  gtk_box_pack_end(GTK_BOX(vbox), fixed, FALSE, FALSE, 2);
*/
#endif

static gboolean defer_play(gpointer data) {
  GtkWidget *win = GTK_WIDGET(video_output);
  backend_set_window_id((guintptr)(GDK_WINDOW_XID(gtk_widget_get_window(win))));

  if (initial_play) {
    backend_play();
  }
  return FALSE;
}

int main(int argc, char *argv[]) {
  gtk_init(&argc, &argv);
  backend_init(&argc, &argv);

  create_ui();

  if (argc > 1) {
    backend_set_file_location(argv[1]);
    initial_play = TRUE;
  }

  g_idle_add(defer_play, NULL);

  gtk_main();

  backend_deinit();

  return 0;
}
