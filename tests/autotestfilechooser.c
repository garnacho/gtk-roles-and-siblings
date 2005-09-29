/* GTK - The GIMP Toolkit
 * autotestfilechooser.c: Automated unit tests for the GtkFileChooser widget
 * Copyright (C) 2005, Novell, Inc.
 *
 * Authors:
 *   Federico Mena-Quintero <federico@novell.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define GTK_FILE_SYSTEM_ENABLE_UNSUPPORTED

#include <config.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include "gtk/gtkfilechooserprivate.h"
#include "gtk/gtkfilechooserdefault.h"
#include "gtk/gtkfilechooserentry.h"

static void
log_test (gboolean passed, const char *test_name, ...)
{
  va_list args;
  char *str;

  va_start (args, test_name);
  str = g_strdup_vprintf (test_name, args);
  va_end (args);

  g_printf ("%s: %s\n", passed ? "PASSED" : "FAILED", str);
  g_free (str);
}

static const GtkFileChooserAction open_actions[] = {
  GTK_FILE_CHOOSER_ACTION_OPEN,
  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
};

static const GtkFileChooserAction save_actions[] = {
  GTK_FILE_CHOOSER_ACTION_SAVE,
  GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
};


static gboolean
has_action (const GtkFileChooserAction *actions,
	    int n_actions,
	    GtkFileChooserAction sought_action)
{
  int i;

  for (i = 0; i < n_actions; i++)
    if (actions[i] == sought_action)
      return TRUE;

  return FALSE;
}

static const char *
get_action_name (GtkFileChooserAction action)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_peek (GTK_TYPE_FILE_CHOOSER_ACTION);
  if (!enum_class)
    g_error ("BUG: get_action_name(): no GEnumClass for GTK_TYPE_FILE_CHOOSER_ACTION");

  enum_value = g_enum_get_value (enum_class, (int) action);
  if (!enum_value)
    g_error ("BUG: get_action_name(): no GEnumValue for GtkFileChooserAction %d", (int) action);

  return enum_value->value_name;
}

static GtkFileChooserDefault *
get_impl_from_dialog (GtkWidget *dialog)
{
  GtkFileChooserDialog *d;
  GtkFileChooserDialogPrivate *dialog_priv;
  GtkFileChooserWidget *chooser_widget;
  GtkFileChooserWidgetPrivate *widget_priv;
  GtkFileChooserDefault *impl;

  d = GTK_FILE_CHOOSER_DIALOG (dialog);
  dialog_priv = d->priv;
  chooser_widget = GTK_FILE_CHOOSER_WIDGET (dialog_priv->widget);
  if (!chooser_widget)
    g_error ("BUG: dialog_priv->widget is not a GtkFileChooserWidget");

  widget_priv = chooser_widget->priv;
  impl = (GtkFileChooserDefault *) (widget_priv->impl);
  if (!impl)
    g_error ("BUG: widget_priv->impl is not a GtkFileChooserDefault");

  return impl;
}

static gboolean
test_widgets_for_current_action (GtkFileChooserDialog *dialog,
				 GtkFileChooserAction  expected_action)
{
  GtkFileChooserDefault *impl;
  gboolean passed;

  if (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog)) != expected_action)
    return FALSE;

  impl = get_impl_from_dialog (GTK_WIDGET (dialog));

  g_assert (impl->action == expected_action);

  passed = TRUE;

  /* OPEN implies that the "new folder" button is hidden; otherwise it is shown */
  if (impl->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    passed = passed && !GTK_WIDGET_VISIBLE (impl->browse_new_folder_button);
  else
    passed = passed && GTK_WIDGET_VISIBLE (impl->browse_new_folder_button);

  /* Check that the widgets are present/visible or not */
  if (has_action (open_actions, G_N_ELEMENTS (open_actions), impl->action))
    {
      passed = passed && (impl->save_widgets == NULL
			  && impl->save_file_name_entry == NULL
			  && impl->save_folder_label == NULL
			  && impl->save_folder_combo == NULL
			  && impl->save_expander == NULL
			  && GTK_IS_CONTAINER (impl->browse_widgets) && GTK_WIDGET_DRAWABLE (impl->browse_widgets));
    }
  else if (has_action (save_actions, G_N_ELEMENTS (save_actions), impl->action))
    {
      /* FIXME: we can't use GTK_IS_FILE_CHOOSER_ENTRY() because it uses
       * _gtk_file_chooser_entry_get_type(), which is a non-exported symbol.
       * So, we just test impl->save_file_name_entry for being non-NULL
       */
      passed = passed && (GTK_IS_CONTAINER (impl->save_widgets) && GTK_WIDGET_DRAWABLE (impl->save_widgets)
			  && impl->save_file_name_entry != NULL && GTK_WIDGET_DRAWABLE (impl->save_file_name_entry)
			  && GTK_IS_LABEL (impl->save_folder_label) && GTK_WIDGET_DRAWABLE (impl->save_folder_label)
			  && GTK_IS_COMBO_BOX (impl->save_folder_combo) && GTK_WIDGET_DRAWABLE (impl->save_folder_combo)
			  && GTK_IS_EXPANDER (impl->save_expander) && GTK_WIDGET_DRAWABLE (impl->save_expander)
			  && GTK_IS_CONTAINER (impl->browse_widgets));

      /* FIXME: we are in a SAVE mode; test the visibility and sensitivity of
       * the children that change depending on the state of the expander.
       */
    }
  else
    {
      g_error ("BAD TEST: test_widgets_for_current_action() doesn't know about %s", get_action_name (impl->action));
      passed = FALSE;
    }

  return passed;
}

typedef gboolean (* ForeachActionCallback) (GtkFileChooserDialog *dialog,
					    GtkFileChooserAction  action,
					    gpointer              user_data);

static gboolean
foreach_action (GtkFileChooserDialog *dialog,
		ForeachActionCallback callback,
		gpointer              user_data)
{
  GEnumClass *enum_class;
  int i;

  enum_class = g_type_class_peek (GTK_TYPE_FILE_CHOOSER_ACTION);
  if (!enum_class)
    g_error ("BUG: get_action_name(): no GEnumClass for GTK_TYPE_FILE_CHOOSER_ACTION");

  for (i = 0; i < enum_class->n_values; i++)
    {
      GEnumValue *enum_value;
      GtkFileChooserAction action;
      gboolean passed;

      enum_value = enum_class->values + i;
      action = enum_value->value;

      passed = (* callback) (dialog, action, user_data);
      if (!passed)
	return FALSE;
    }

  return TRUE;
}

struct action_closure {
  GtkFileChooserAction from_action;
};

static gboolean
switch_from_to_action_cb (GtkFileChooserDialog *dialog,
			  GtkFileChooserAction  action,
			  gpointer              user_data)
{
  struct action_closure *closure;
  gboolean passed;

  closure = user_data;

  gtk_file_chooser_set_action (GTK_FILE_CHOOSER (dialog), closure->from_action);

  passed = test_widgets_for_current_action (dialog, closure->from_action);
  log_test (passed, "switch_from_to_action_cb(): reset to action %s", get_action_name (closure->from_action));
  if (!passed)
    return FALSE;

  gtk_file_chooser_set_action (GTK_FILE_CHOOSER (dialog), action);

  passed = test_widgets_for_current_action (dialog, action);
  log_test (passed, "switch_from_to_action_cb(): transition from %s to %s",
	    get_action_name (closure->from_action),
	    get_action_name (action));
  return passed;
}

static gboolean
switch_from_action_cb (GtkFileChooserDialog *dialog,
		       GtkFileChooserAction  action,
		       gpointer              user_data)
{
  struct action_closure closure;

  closure.from_action = action;

  return foreach_action (dialog, switch_from_to_action_cb, &closure);
}

static gboolean
test_action_widgets (void)
{
  GtkWidget *dialog;
  GtkFileChooserAction action;
  gboolean passed;

  dialog = gtk_file_chooser_dialog_new ("Test file chooser",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK,
					GTK_RESPONSE_ACCEPT,
					NULL);
  gtk_widget_show_now (dialog);

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog));

  passed = test_widgets_for_current_action (GTK_FILE_CHOOSER_DIALOG (dialog), action);
  log_test (passed, "test_action_widgets(): widgets for initial action %s", get_action_name (action));
  if (!passed)
    return FALSE;

  passed = foreach_action (GTK_FILE_CHOOSER_DIALOG (dialog), switch_from_action_cb, NULL);
  log_test (passed, "test_action_widgets(): all transitions through property change");

  gtk_widget_destroy (dialog);

  return passed;
}

static gboolean
test_reload_sequence (gboolean set_folder_before_map)
{
  GtkWidget *dialog;
  GtkFileChooserDefault *impl;
  gboolean passed;

  passed = TRUE;

  dialog = gtk_file_chooser_dialog_new ("Test file chooser",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK,
					GTK_RESPONSE_ACCEPT,
					NULL);
  impl = get_impl_from_dialog (dialog);

  if (set_folder_before_map)
    {
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), g_get_home_dir ());

      passed = (impl->current_folder != NULL
		&& impl->browse_files_model != NULL
		&& (impl->load_state == LOAD_PRELOAD || impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
		&& impl->reload_state == RELOAD_HAS_FOLDER
		&& (impl->load_state == LOAD_PRELOAD ? (impl->load_timeout_id != 0) : TRUE)
		&& ((impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
		    ? (impl->load_timeout_id == 0 && impl->sort_model != NULL)
		    : TRUE));
    }
  else
    {
      /* Initially, no folder is not loaded or pending */
      passed = passed && (impl->current_folder == NULL
			  && impl->sort_model == NULL
			  && impl->browse_files_model == NULL
			  && impl->load_state == LOAD_EMPTY
			  && impl->reload_state == RELOAD_EMPTY
			  && impl->load_timeout_id == 0);
    }

  if (!passed)
    return FALSE;

  /* After mapping, it is loading some folder, either the one that was explicitly set or the default one */

  gtk_widget_show_now (dialog);

  passed = (impl->current_folder != NULL
	    && impl->browse_files_model != NULL
	    && (impl->load_state == LOAD_PRELOAD || impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
	    && impl->reload_state == RELOAD_HAS_FOLDER
	    && (impl->load_state == LOAD_PRELOAD ? (impl->load_timeout_id != 0) : TRUE)
	    && ((impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
		? (impl->load_timeout_id == 0 && impl->sort_model != NULL)
		: TRUE));
  if (!passed)
    return FALSE;

  /* Unmap it; we should still have a folder */

  gtk_widget_hide (dialog);

  passed = (impl->current_folder != NULL
	    && impl->browse_files_model != NULL
	    && (impl->load_state == LOAD_PRELOAD || impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
	    && impl->reload_state == RELOAD_WAS_UNMAPPED
	    && (impl->load_state == LOAD_PRELOAD ? (impl->load_timeout_id != 0) : TRUE)
	    && ((impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
		? (impl->load_timeout_id == 0 && impl->sort_model != NULL)
		: TRUE));
  if (!passed)
    return FALSE;

  /* Map it again! */

  gtk_widget_show_now (dialog);
  
  passed = (impl->current_folder != NULL
	    && impl->browse_files_model != NULL
	    && (impl->load_state == LOAD_PRELOAD || impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
	    && impl->reload_state == RELOAD_HAS_FOLDER
	    && (impl->load_state == LOAD_PRELOAD ? (impl->load_timeout_id != 0) : TRUE)
	    && ((impl->load_state == LOAD_LOADING || impl->load_state == LOAD_FINISHED)
		? (impl->load_timeout_id == 0 && impl->sort_model != NULL)
		: TRUE));
  if (!passed)
    return FALSE;

  gtk_widget_destroy (dialog);

  return passed;
}

static gboolean
test_reload (void)
{
  gboolean passed;

  passed = test_reload_sequence (FALSE);
  log_test (passed, "test_reload(): create and use the default folder");
  if (!passed)
    return FALSE;

  passed = test_reload_sequence (TRUE);
  log_test (passed, "test_reload(): set a folder explicitly before mapping");

  return passed;
}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  test_action_widgets ();
  test_reload ();

  return 0;
}
