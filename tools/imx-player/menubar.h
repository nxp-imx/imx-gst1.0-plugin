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

#ifndef MENUBAR_H_
#define MENUBAR_H_

#define WINDOW_MOVE_STEP    50
#define MENUBAR_H           40
#define MENU_ITEM_W         150
#define MENU_ITEM_H         40

typedef enum
{
  FULL_WINDOW,
  TOP_LEFT_HALF,
  TOP_RIGHT_QUARTER,
  BOTTOM_LEFT_HALF,
  BOTTOM_RIGHT_QUARTER,
  CENTER_EIGHTH,
  CENTER_HALF_RENDER_RECT,
  VIDEO_SIZE_MODE_MAX
} VideoSizeMode;

typedef struct
{
  GtkWidget *menubar;
  GtkWidget *filemenu;
  GtkWidget *seekmenu;
  GtkWidget *rotatemenu;
  GtkWidget *resizemenu;
  GtkWidget *movemenu;
  GtkWidget *videomenu;
  GtkWidget *audiomenu;
  GtkWidget *subtitlemenu;
  GtkWidget *aspectmenu;
  GtkWidget *optionmenu;
  GtkWidget *helpmenu;
} MenuBar;

void menubar_create(void *imxplayer);
void menubar_show(MenuBar *menubar, gboolean show);
void menubar_update(void *player);

#endif /* MENUBAR_H_ */
