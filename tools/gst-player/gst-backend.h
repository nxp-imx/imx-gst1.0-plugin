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

#ifndef GST_BACKEND_H
#define GST_BACKEND_H

#define METADATA_ITEM_MAX_SIZE_LARGE 256
#define METADATA_ITEM_MAX_SIZE_SMALL 64

typedef struct
{
    gchar pathname[METADATA_ITEM_MAX_SIZE_LARGE];
    gchar title[METADATA_ITEM_MAX_SIZE_LARGE];
    gchar artist[METADATA_ITEM_MAX_SIZE_LARGE];
    gchar album[METADATA_ITEM_MAX_SIZE_LARGE];
    gchar year[METADATA_ITEM_MAX_SIZE_SMALL];
    gchar genre[METADATA_ITEM_MAX_SIZE_SMALL];
    gint width;
    gint height;
    gint framerate;
    gint videobitrate;
    gchar videocodec[METADATA_ITEM_MAX_SIZE_SMALL];
    gint channels;
    gint samplerate;
    gint audiobitrate;
    gchar audiocodec[METADATA_ITEM_MAX_SIZE_SMALL];
} backend_metadata;

void backend_init (int *argc, char **argv[]);
void backend_deinit (void);
void backend_play (void);
void backend_stop (void);
void backend_seek (gint value);
void backend_seek_absolute (guint64 value);
void backend_reset (void);
void backend_pause (void);
void backend_resume (void);
guint64 backend_query_position (void);
guint64 backend_query_duration (void);
void backend_set_window_id(guintptr handle);
void backend_set_render_rect(gint x, gint y, gint w, gint h);
backend_metadata * backend_get_metadata(void);
gulong backend_get_color_key(void);
void backend_video_expose(void);
void backend_set_file_location(gchar *uri);
gboolean backend_is_playing(void);

#endif /* GST_BACKEND_H */
