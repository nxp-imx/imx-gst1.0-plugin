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

#ifndef CONTROLBAR_H_
#define CONTROLBAR_H_

/* Configure the optional buttons on the control bar */
//#define ENABLE_STOP_BUTTON
//#define ENABLE_STEP_SEEK_BUTTON
//#define ENABLE_DIRECT_SEEK_BUTTON
//#define ENABLE_REPEAT_MODE_BUTTON
//#define ENABLE_VOLUME_BUTTON

/* Enable this to have the seeking performed in a separated thread */
//#define USE_IDLE_SEEK

#define CTRLBAR_BOTTON_W      50
#define CTRLBAR_BOTTON_H      50
#define CTRLBAR_BOTTON_GAP    2
#define CTRLBAR_PROGRESS_H    50
#define CTRLBAR_PROGRESS_GAP  1
#define CTRLBAR_Y_OFFSET      10
#define CTRLBAR_TIME_W        120

#define NUM_OF_BUTTONS_BASE   9

#ifdef ENABLE_STOP_BUTTON
#define STOP_BUTTON           1
#else
#define STOP_BUTTON           0
#endif

#ifdef ENABLE_STEP_SEEK_BUTTON
#define STEP_SEEK_BUTTON      1
#else
#define STEP_SEEK_BUTTON      0
#endif

#ifdef ENABLE_DIRECT_SEEK_BUTTON
#define DIRECT_SEEK_BUTTON    1
#else
#define DIRECT_SEEK_BUTTON    0
#endif

#ifdef ENABLE_REPEAT_MODE_BUTTON
#define REPEAT_MODE_BUTTON    1
#else
#define REPEAT_MODE_BUTTON    0
#endif

#ifdef ENABLE_VOLUME_BUTTON
#define VOLUME_BUTTON         1
#else
#define VOLUME_BUTTON         0
#endif

#define NUM_OF_BUTTONS        (NUM_OF_BUTTONS_BASE + STOP_BUTTON + \
                                STEP_SEEK_BUTTON + DIRECT_SEEK_BUTTON + \
                                REPEAT_MODE_BUTTON + VOLUME_BUTTON)

#define CTRLBAR_H (CTRLBAR_BOTTON_H + CTRLBAR_PROGRESS_GAP + CTRLBAR_PROGRESS_H)
#define CTRLBAR_W ((CTRLBAR_BOTTON_W + CTRLBAR_BOTTON_GAP) * NUM_OF_BUTTONS \
                  - CTRLBAR_BOTTON_GAP)

#define STEP_SEEK_STEP        10

typedef struct
{
  GtkWidget *control_bar;
  GtkWidget *progress;
  GtkWidget *current;
  GtkWidget *duration;
#ifdef ENABLE_STOP_BUTTON
  GtkWidget *play_stop;
#endif
  GtkWidget *play_pause;
  GtkWidget *trick_forward;
  GtkWidget *trick_backward;
#ifdef ENABLE_STEP_SEEK_BUTTON
  GtkWidget *step_forward;
  GtkWidget *step_rewind;
#endif
  GtkWidget *next;
  GtkWidget *previous;
  GtkWidget *info;
#ifdef ENABLE_REPEAT_MODE_BUTTON
  GtkWidget *repeat;
#endif
  GtkWidget *full;
#ifdef ENABLE_DIRECT_SEEK_BUTTON
  GtkWidget *seekto;
#endif
#ifdef ENABLE_VOLUME_BUTTON
  GtkWidget *volume;
#endif
} CtrlBar;

void ctrlbar_create(void *imxplayer);
void ctrlbar_show(CtrlBar *ctrlbar, gboolean show);
void ctrlbar_resize(GtkWidget *container, CtrlBar *ctrlbar,
                    gint x, gint y, gint w, gint h);

#endif /* CONTROLBAR_H_ */
