#include <gtk/gtk.h>
#include <stdlib.h>

GtkObject *model;

static void
selection_changed (GtkTreeSelection *selection,
		   GtkWidget         *button)
{
  if (gtk_tree_selection_get_selected (selection))
    gtk_widget_set_sensitive (button, TRUE);
  else
    gtk_widget_set_sensitive (button, FALSE);
}

static GtkTreeNode *
node_new ()
{
  static GValue value = {0, };
  static gint i = 0;
  gchar *str;
  GtkTreeNode *node = gtk_tree_store_node_new ();

  g_value_init (&value, G_TYPE_STRING);
  str = g_strdup_printf ("FOO: %d", i++);
  g_value_set_string (&value, str);
  g_free (str);
  gtk_tree_store_node_set_cell (GTK_TREE_STORE (model), node, 0, &value);
  g_value_unset (&value);

  return node;
}

static void
node_remove (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeNode *selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)));
  gtk_tree_store_node_remove (GTK_TREE_STORE (model),
			      selected);
}

static void
node_insert (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkWidget *entry;
  GtkTreeNode *selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)));

  entry = gtk_object_get_user_data (GTK_OBJECT (button));
  gtk_tree_store_node_insert (GTK_TREE_STORE (model),
			      selected,
			      atoi (gtk_entry_get_text (GTK_ENTRY (entry))),
			      node_new ());
}

static void
node_insert_before  (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeNode *selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)));
  gtk_tree_store_node_insert_before (GTK_TREE_STORE (model),
				    NULL,
				    selected,
				    node_new ());
}

static void
node_insert_after (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeNode *selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)));
  gtk_tree_store_node_insert_after (GTK_TREE_STORE (model),
				    NULL,
				    selected,
				    node_new ());
}

static void
node_prepend (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeNode *selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)));
  gtk_tree_store_node_prepend (GTK_TREE_STORE (model),
			       selected,
			       node_new ());
}

static void
node_append (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeNode *selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)));
  gtk_tree_store_node_append (GTK_TREE_STORE (model),
			      selected,
			      node_new ());
}

static void
make_window ()
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *hbox, *entry;
  GtkWidget *button;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkObject *column;
  GtkCellRenderer *cell;
  GtkObject *selection;

  /* Make the Widgets/Objects */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 350);
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  selection = GTK_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)));
  gtk_tree_selection_set_type (GTK_TREE_SELECTION (selection), GTK_TREE_SELECTION_SINGLE);

  /* Put them together */
  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_signal_connect (GTK_OBJECT (window), "destroy", gtk_main_quit, NULL);

  /* buttons */
  button = gtk_button_new_with_label ("gtk_tree_store_node_remove");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (selection),
		      "selection_changed",
		      selection_changed,
		      button);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", node_remove, tree_view);
  gtk_widget_set_sensitive (button, FALSE);

  button = gtk_button_new_with_label ("gtk_tree_store_node_insert");
  hbox = gtk_hbox_new (FALSE, 8);
  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
  gtk_object_set_user_data (GTK_OBJECT (button), entry);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", node_insert, tree_view);

  
  button = gtk_button_new_with_label ("gtk_tree_store_node_insert_before");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", node_insert_before, tree_view);
  gtk_signal_connect (GTK_OBJECT (selection),
		      "selection_changed",
		      selection_changed,
		      button);
  gtk_widget_set_sensitive (button, FALSE);

  button = gtk_button_new_with_label ("gtk_tree_store_node_insert_after");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", node_insert_after, tree_view);
  gtk_signal_connect (GTK_OBJECT (selection),
		      "selection_changed",
		      selection_changed,
		      button);
  gtk_widget_set_sensitive (button, FALSE);

  button = gtk_button_new_with_label ("gtk_tree_store_node_prepend");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", node_prepend, tree_view);

  button = gtk_button_new_with_label ("gtk_tree_store_node_append");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", node_append, tree_view);

  /* The selected column */
  cell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("nodes", cell, "text", 0, NULL);
  gtk_tree_view_add_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));

  /* Show it all */
  gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);

  model = gtk_tree_store_new_with_values (2, G_TYPE_STRING, G_TYPE_STRING);

  make_window ();
  make_window ();

  /* A few to start */
  gtk_tree_store_node_append (GTK_TREE_STORE (model), NULL, node_new ());
  gtk_tree_store_node_append (GTK_TREE_STORE (model), NULL, node_new ());
  gtk_tree_store_node_append (GTK_TREE_STORE (model), NULL, node_new ());

  gtk_main ();

  return 0;
}
