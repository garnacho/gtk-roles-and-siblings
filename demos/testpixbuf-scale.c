#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>

GdkInterpType interp_type = GDK_INTERP_BILINEAR;
int overall_alpha = 255;
GdkPixbuf *pixbuf;
GtkWidget *darea;
  
void
set_interp_type (GtkWidget *widget, gpointer data)
{
  interp_type = GPOINTER_TO_UINT (data);
  gtk_widget_queue_draw (darea);
}

void
overall_changed_cb (GtkAdjustment *adjustment, gpointer data)
{
  if (adjustment->value != overall_alpha)
    {
      overall_alpha = adjustment->value;
      gtk_widget_queue_draw (darea);
    }
}

gboolean
expose_cb (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  GdkPixbuf *dest;

  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
  
  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, event->area.width, event->area.height);

  gdk_pixbuf_composite_color (pixbuf, dest,
			      0, 0, event->area.width, event->area.height,
			      -event->area.x, -event->area.y,
			      (double) widget->allocation.width / gdk_pixbuf_get_width (pixbuf),
			      (double) widget->allocation.height / gdk_pixbuf_get_height (pixbuf),
			      interp_type, overall_alpha,
			      event->area.x, event->area.y, 16, 0xaaaaaa, 0x555555);

  gdk_draw_pixbuf (widget->window, widget->style->fg_gc[GTK_STATE_NORMAL], dest,
		   0, 0, event->area.x, event->area.y,
		   event->area.width, event->area.height,
		   GDK_RGB_DITHER_NORMAL, event->area.x, event->area.y);
  
  g_object_unref (dest);
  
  return TRUE;
}

extern void pixbuf_init();

int
main(int argc, char **argv)
{
	GtkWidget *window, *vbox;
	GtkWidget *menuitem, *optionmenu, *menu;
	GtkWidget *alignment;
	GtkWidget *hbox, *label, *hscale;
	GtkAdjustment *adjustment;
	GtkRequisition scratch_requisition;
        const gchar *creator;
        GError *error;
        
	pixbuf_init ();

	gtk_init (&argc, &argv);

	if (argc != 2) {
		fprintf (stderr, "Usage: testpixbuf-scale FILE\n");
		exit (1);
	}

        error = NULL;
	pixbuf = gdk_pixbuf_new_from_file (argv[1], &error);
	if (!pixbuf) {
		fprintf (stderr, "Cannot load image: %s\n",
                         error->message);
                g_error_free (error);
		exit(1);
	}

        creator = gdk_pixbuf_get_option (pixbuf, "tEXt::Software");
        if (creator)
                g_print ("%s was created by '%s'\n", argv[1], creator);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (window, "destroy",
			  G_CALLBACK (gtk_main_quit), NULL);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	menu = gtk_menu_new ();
	
	menuitem = gtk_menu_item_new_with_label ("NEAREST");
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (set_interp_type),
			  GUINT_TO_POINTER (GDK_INTERP_NEAREST));
	gtk_widget_show (menuitem);
	gtk_container_add (GTK_CONTAINER (menu), menuitem);
	
	menuitem = gtk_menu_item_new_with_label ("BILINEAR");
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (set_interp_type),
			  GUINT_TO_POINTER (GDK_INTERP_BILINEAR));
	gtk_widget_show (menuitem);
	gtk_container_add (GTK_CONTAINER (menu), menuitem);
	
	menuitem = gtk_menu_item_new_with_label ("TILES");
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (set_interp_type),
			  GUINT_TO_POINTER (GDK_INTERP_TILES));
	gtk_container_add (GTK_CONTAINER (menu), menuitem);

	menuitem = gtk_menu_item_new_with_label ("HYPER");
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (set_interp_type),
			  GUINT_TO_POINTER (GDK_INTERP_HYPER));
	gtk_container_add (GTK_CONTAINER (menu), menuitem);

	optionmenu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 1);
	
	alignment = gtk_alignment_new (0.0, 0.0, 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new ("Overall Alpha:");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (overall_alpha, 0, 255, 1, 10, 0));
	g_signal_connect (adjustment, "value_changed",
			  G_CALLBACK (overall_changed_cb), NULL);
	
	hscale = gtk_hscale_new (adjustment);
	gtk_scale_set_digits (GTK_SCALE (hscale), 0);
	gtk_box_pack_start (GTK_BOX (hbox), hscale, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (alignment), optionmenu);
	gtk_widget_show_all (vbox);

	/* Compute the size without the drawing area, so we know how big to make the default size */
	gtk_widget_size_request (vbox, &scratch_requisition);

	darea = gtk_drawing_area_new ();
	gtk_box_pack_start (GTK_BOX (vbox), darea, TRUE, TRUE, 0);

	g_signal_connect (darea, "expose_event",
			  G_CALLBACK (expose_cb), NULL);

	gtk_window_set_default_size (GTK_WINDOW (window),
				     gdk_pixbuf_get_width (pixbuf),
				     scratch_requisition.height + gdk_pixbuf_get_height (pixbuf));
	
	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
