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

#ifndef SUBTITLE_H_
#define SUBTITLE_H_

#define  SUBTITLE_Y_PROPOTION 7/8

typedef struct
{
  GtkWidget *subtitle;
  GtkWidget *text;
} SubTitle;

void subtitle_create(void *imxplayer);
void subtitle_show(SubTitle *subtitle, gboolean show);
void subtitle_resize(GtkWidget *container, SubTitle *subtitle,
                    gint x, gint y, gint w, gint h);
void subtitle_set_text(SubTitle *subtitle, const gchar *text);

#endif /* SUBTITLE_H_ */
