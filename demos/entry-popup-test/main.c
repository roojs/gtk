/*
 * Copyright (C) 2026 Alan Knowles
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gmodule.h>
#include <gtk/gtk.h>

static GtkWidget *dialog_window;

static void
on_open_dialog (GtkWidget *button,
                gpointer   user_data)
{
  GtkWindow *parent = GTK_WINDOW (user_data);
  GtkWidget *dialog;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *entry;

  if (dialog_window != NULL)
    {
      gtk_window_present (GTK_WINDOW (dialog_window));
      return;
    }

  dialog = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), "Nested Dialog");
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 340, 180);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 16);
  gtk_widget_set_margin_bottom (box, 16);
  gtk_widget_set_margin_start (box, 16);
  gtk_widget_set_margin_end (box, 16);

  label = gtk_label_new ("Long-press the entry below.\n"
                         "Hold to see the magnifier, release for the paste bubble.");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_box_append (GTK_BOX (box), label);

  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry),
                         "Nested dialog entry — long-press me");
  gtk_box_append (GTK_BOX (box), entry);

  gtk_window_set_child (GTK_WINDOW (dialog), box);

  dialog_window = dialog;
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &dialog_window);

  gtk_window_present (GTK_WINDOW (dialog));
}

static char *
make_sample_text (void)
{
  GString *s = g_string_new (NULL);
  int i;

  g_string_append (s,
                   "TextView touch selection harness.\n\n"
                   "Expected: finger drag scrolls without selecting text. "
                   "Long-press a word to select; keep holding and drag to extend. "
                   "Release should keep the selection.\n\n");

  for (i = 1; i <= 80; i++)
    {
      g_string_append_printf (s,
                              "Paragraph %d — The quick brown fox jumps over the lazy dog. "
                              "Pack my box with five dozen liquor jugs. "
                              "Sphinx of black quartz, judge my vow. "
                              "Scrolling past this line must not start a text selection on touch.\n\n",
                              i);
    }

  return g_string_free (s, FALSE);
}

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *tv_label;
  GtkWidget *text_view;
  GtkWidget *scrolled;
  GtkTextBuffer *buffer;
  char *sample;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Entry / TextView Touch Test");
  gtk_window_set_default_size (GTK_WINDOW (window), 360, 720);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 16);
  gtk_widget_set_margin_bottom (box, 16);
  gtk_widget_set_margin_start (box, 16);
  gtk_widget_set_margin_end (box, 16);

  label = gtk_label_new ("Entry: long-press for selection / paste bubble.\n"
                         "TextView below: drag should scroll only; long-press to select.");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_box_append (GTK_BOX (box), label);

  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry),
                         "Main window entry — long-press me");
  gtk_box_append (GTK_BOX (box), entry);

  button = gtk_button_new_with_label ("Open Nested Dialog");
  g_signal_connect (button, "clicked", G_CALLBACK (on_open_dialog), window);
  gtk_box_append (GTK_BOX (box), button);

  tv_label = gtk_label_new ("Scrollable GtkTextView:");
  gtk_label_set_xalign (GTK_LABEL (tv_label), 0.0);
  gtk_box_append (GTK_BOX (box), tv_label);

  text_view = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_view), 8);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (text_view), 8);
  gtk_widget_set_hexpand (text_view, TRUE);
  gtk_widget_set_vexpand (text_view, TRUE);

  sample = make_sample_text ();
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
  gtk_text_buffer_set_text (buffer, sample, -1);
  g_free (sample);

  scrolled = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scrolled, TRUE);
  gtk_widget_set_vexpand (scrolled, TRUE);
  gtk_widget_set_size_request (scrolled, -1, 360);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), text_view);
  gtk_box_append (GTK_BOX (box), scrolled);

  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_present (GTK_WINDOW (window));
}

G_MODULE_EXPORT
int
main (int argc, char *argv[])
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.EntryPopupTest",
                               G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}
