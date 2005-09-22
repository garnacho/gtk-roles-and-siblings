/* gtkcellrendereraccel.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

static void
accel_edited_callback (GtkCellRendererText *cell,
                       const char          *path_string,
                       guint                keyval,
                       GdkModifierType      mask,
                       guint                hardware_keycode,
                       gpointer             data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;

  gtk_tree_model_get_iter (model, &iter, path);

  g_print ("%u %d %u\n", keyval, mask, hardware_keycode);
  
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      0, (gint)mask,
		      1, keyval,
		      -1);
  gtk_tree_path_free (path);
}

static GtkWidget *
key_test (void)
{
	GtkWidget *window, *sw, *tv;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *rend;
	gint i;

	/* create window */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);


	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (window), sw);

	store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_UINT);
	tv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_container_add (GTK_CONTAINER (sw), tv);
	column = gtk_tree_view_column_new ();
	rend = gtk_cell_renderer_accel_new ();
	g_object_set (G_OBJECT (rend), 
		      "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_GTK, 
                      "editable", TRUE, 
		      NULL);
	g_signal_connect (G_OBJECT (rend),
			  "accel-edited",
			  G_CALLBACK (accel_edited_callback),
			  store);

	gtk_tree_view_column_pack_start (column, rend,
					 TRUE);
	gtk_tree_view_column_set_attributes (column, rend,
					     "accel-mods", 0,
					     "accel-key", 1,
					     NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

	for (i = 0; i < 10; i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
	}

	/* done */

	return window;
}

gint
main (gint argc, gchar **argv)
{
  GtkWidget *dialog;
  
  gtk_init (&argc, &argv);

  dialog = key_test ();

  gtk_widget_show_all (dialog);

  gtk_main ();

  return 0;
}
