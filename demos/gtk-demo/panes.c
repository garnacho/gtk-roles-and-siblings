/* Paned Widgets
 *
 * The GtkHPaned and GtkVPaned Widgets divide their content
 * area into two panes with a divider in between that the
 * user can adjust. A separate child is placed into each
 * pane.
 *
 * There are a number of options that can be set for each pane.
 * This test contains both a horizontal (HPaned) and a vertical
 * (VPaned) widget, and allows you to adjust the options for
 * each side of each widget.
 */

#include <gtk/gtk.h>

void
toggle_resize (GtkWidget *widget, GtkWidget *child)
{
  GtkPaned *paned = GTK_PANED (child->parent);
  gboolean is_child1 = (child == paned->child1);
  gboolean resize, shrink;

  resize = is_child1 ? paned->child1_resize : paned->child2_resize;
  shrink = is_child1 ? paned->child1_shrink : paned->child2_shrink;

  gtk_widget_ref (child);
  gtk_container_remove (GTK_CONTAINER (child->parent), child);
  if (is_child1)
    gtk_paned_pack1 (paned, child, !resize, shrink);
  else
    gtk_paned_pack2 (paned, child, !resize, shrink);
  gtk_widget_unref (child);
}

void
toggle_shrink (GtkWidget *widget, GtkWidget *child)
{
  GtkPaned *paned = GTK_PANED (child->parent);
  gboolean is_child1 = (child == paned->child1);
  gboolean resize, shrink;

  resize = is_child1 ? paned->child1_resize : paned->child2_resize;
  shrink = is_child1 ? paned->child1_shrink : paned->child2_shrink;

  gtk_widget_ref (child);
  gtk_container_remove (GTK_CONTAINER (child->parent), child);
  if (is_child1)
    gtk_paned_pack1 (paned, child, resize, !shrink);
  else
    gtk_paned_pack2 (paned, child, resize, !shrink);
  gtk_widget_unref (child);
}

GtkWidget *
create_pane_options (GtkPaned *paned,
		     const gchar *frame_label,
		     const gchar *label1,
		     const gchar *label2)
{
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *check_button;
  
  frame = gtk_frame_new (frame_label);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  
  table = gtk_table_new (3, 2, TRUE);
  gtk_container_add (GTK_CONTAINER (frame), table);
  
  label = gtk_label_new (label1);
  gtk_table_attach_defaults (GTK_TABLE (table), label,
			     0, 1, 0, 1);
  
  check_button = gtk_check_button_new_with_label ("Resize");
  gtk_table_attach_defaults (GTK_TABLE (table), check_button,
			     0, 1, 1, 2);
  gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
		      GTK_SIGNAL_FUNC (toggle_resize),
		      paned->child1);
  
  check_button = gtk_check_button_new_with_label ("Shrink");
  gtk_table_attach_defaults (GTK_TABLE (table), check_button,
			     0, 1, 2, 3);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
		      GTK_SIGNAL_FUNC (toggle_shrink),
		      paned->child1);
  
  label = gtk_label_new (label2);
  gtk_table_attach_defaults (GTK_TABLE (table), label,
			     1, 2, 0, 1);
  
  check_button = gtk_check_button_new_with_label ("Resize");
  gtk_table_attach_defaults (GTK_TABLE (table), check_button,
			     1, 2, 1, 2);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
		      GTK_SIGNAL_FUNC (toggle_resize),
		      paned->child2);
  
  check_button = gtk_check_button_new_with_label ("Shrink");
  gtk_table_attach_defaults (GTK_TABLE (table), check_button,
			     1, 2, 2, 3);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
			       TRUE);
  gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
		      GTK_SIGNAL_FUNC (toggle_shrink),
		      paned->child2);

  return frame;
}

GtkWidget *
do_panes (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hpaned;
  GtkWidget *vpaned;
  GtkWidget *button;
  GtkWidget *vbox;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Panes");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      
      vpaned = gtk_vpaned_new ();
      gtk_box_pack_start (GTK_BOX (vbox), vpaned, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER(vpaned), 5);

      hpaned = gtk_hpaned_new ();
      gtk_paned_add1 (GTK_PANED (vpaned), hpaned);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_usize (frame, 60, 60);
      gtk_paned_add1 (GTK_PANED (hpaned), frame);
      
      button = gtk_button_new_with_label ("Hi there");
      gtk_container_add (GTK_CONTAINER(frame), button);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_usize (frame, 80, 60);
      gtk_paned_add2 (GTK_PANED (hpaned), frame);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
      gtk_widget_set_usize (frame, 60, 80);
      gtk_paned_add2 (GTK_PANED (vpaned), frame);

      /* Now create toggle buttons to control sizing */

      gtk_box_pack_start (GTK_BOX (vbox),
			  create_pane_options (GTK_PANED (hpaned),
					       "Horizontal",
					       "Left",
					       "Right"),
			  FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (vbox),
			  create_pane_options (GTK_PANED (vpaned),
					       "Vertical",
					       "Top",
					       "Bottom"),
			  FALSE, FALSE, 0);

      gtk_widget_show_all (vbox);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    {
      gtk_widget_show (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
