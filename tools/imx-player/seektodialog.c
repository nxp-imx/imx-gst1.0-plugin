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
#include <gtk/gtk.h>
#include "seektodialog.h"

SeekToDialog * seekto_dialog_create(void)
{
  SeekToDialog *seekto = (SeekToDialog *)malloc(sizeof (SeekToDialog));
  GtkWidget *lbl1;

  seekto->dialog = gtk_dialog_new_with_buttons ("Seek To", NULL,
                                  GTK_DIALOG_MODAL,
                                  GTK_STOCK_OK, GTK_RESPONSE_OK,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  NULL);
  gtk_dialog_set_default_response (GTK_DIALOG(seekto->dialog),
                                   GTK_RESPONSE_CANCEL);
  gtk_dialog_set_has_separator (GTK_DIALOG (seekto->dialog), FALSE);

  lbl1 = gtk_label_new ("Seek To:");
  seekto->time = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY(seekto->time), 8);
  gtk_entry_set_text (GTK_ENTRY (seekto->time), "00:00");
  seekto->accurate = gtk_check_button_new_with_label("Accurate seek");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(seekto->accurate), TRUE);

  GtkWidget *vbox = GTK_DIALOG(seekto->dialog)->vbox;
  gtk_box_pack_start_defaults (GTK_BOX (vbox), lbl1);
  gtk_box_pack_start_defaults (GTK_BOX (vbox), seekto->time);
  gtk_box_pack_start_defaults (GTK_BOX (vbox), seekto->accurate);
  gtk_widget_show_all (seekto->dialog);

  return seekto;
}

void seekto_dialog_destroy(SeekToDialog *seekto)
{
  gtk_widget_destroy (seekto->dialog);
  free(seekto);
}

gint seekto_dialog_run(SeekToDialog *seekto)
{
  return gtk_dialog_run (GTK_DIALOG (seekto->dialog));
}

void seekto_dialog_get_time(SeekToDialog *seekto, gchar *str)
{
  const gchar *s = gtk_entry_get_text (GTK_ENTRY (seekto->time));
  strcpy(str, s);
}

gboolean seekto_dialog_get_accurate(SeekToDialog *seekto)
{
  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (seekto->accurate));
}
