/* Button Boxes
 *
 * The Button Box widgets are used to arrange buttons with padding.
 */

#include <gtk/gtk.h>

static GtkWidget *
create_bbox (gint  horizontal,
	     char* title, 
	     gint  spacing,
	     gint  child_w, 
	     gint  child_h, 
	     gint  layout)
{
  GtkWidget *frame;
  GtkWidget *bbox;
  GtkWidget *button;
	
  frame = gtk_frame_new (title);

  if (horizontal)
    bbox = gtk_hbutton_box_new ();
  else
    bbox = gtk_vbutton_box_new ();

  gtk_container_set_border_width (GTK_CONTAINER (bbox), 5);
  gtk_container_add (GTK_CONTAINER (frame), bbox);

  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), layout);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (bbox), spacing);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), child_w, child_h);
  
  button = gtk_button_new_with_label ("OK");
  gtk_container_add (GTK_CONTAINER (bbox), button);
  
  button = gtk_button_new_with_label ("Cancel");
  gtk_container_add (GTK_CONTAINER (bbox), button);
  
  button = gtk_button_new_with_label ("Help");
  gtk_container_add (GTK_CONTAINER (bbox), button);

  return frame;
}

GtkWidget *
do_button_box (void)
{
  static GtkWidget* window = NULL;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *frame_horz;
  GtkWidget *frame_vert;
	
  if (!window)
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Button Boxes");
    
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			&window);
    
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    main_vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), main_vbox);
    
    frame_horz = gtk_frame_new ("Horizontal Button Boxes");
    gtk_box_pack_start (GTK_BOX (main_vbox), frame_horz, TRUE, TRUE, 10);
    
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
    gtk_container_add (GTK_CONTAINER (frame_horz), vbox);

    gtk_box_pack_start (GTK_BOX (vbox), 
	   create_bbox (TRUE, "Spread", 40, 85, 20, GTK_BUTTONBOX_SPREAD),
			TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (vbox), 
	   create_bbox (TRUE, "Edge", 40, 85, 20, GTK_BUTTONBOX_EDGE),
			TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (vbox), 
	   create_bbox (TRUE, "Start", 40, 85, 20, GTK_BUTTONBOX_START),
			TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (vbox), 
	   create_bbox (TRUE, "End", 40, 85, 20, GTK_BUTTONBOX_END),
			TRUE, TRUE, 5);

    frame_vert = gtk_frame_new ("Vertical Button Boxes");
    gtk_box_pack_start (GTK_BOX (main_vbox), frame_vert, TRUE, TRUE, 10);
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
    gtk_container_add (GTK_CONTAINER (frame_vert), hbox);

    gtk_box_pack_start (GTK_BOX (hbox), 
	   create_bbox (FALSE, "Spread", 30, 85, 20, GTK_BUTTONBOX_SPREAD),
			TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (hbox), 
	   create_bbox (FALSE, "Edge", 30, 85, 20, GTK_BUTTONBOX_EDGE),
			TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (hbox), 
	   create_bbox (FALSE, "Start", 30, 85, 20, GTK_BUTTONBOX_START),
			TRUE, TRUE, 5);

    gtk_box_pack_start (GTK_BOX (hbox), 
	   create_bbox (FALSE, "End", 30, 85, 20, GTK_BUTTONBOX_END),
			TRUE, TRUE, 5);
  }

  if (!GTK_WIDGET_VISIBLE (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {    
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}

