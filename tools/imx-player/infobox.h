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

#ifndef INFOBOX_H_
#define INFOBOX_H_

#define INFO_TEXT_LEN_MAX 2048
#define HIDE_INVALID_INFO 0

typedef struct
{
  GtkWidget *info_box;
  GtkWidget *info_text;
} InfoBox;

void infobox_create(void *imxplayer);
void infobox_show(InfoBox *infobox, gboolean show);
void infobox_update(void *player);

#endif /* INFOBOX_H_ */
