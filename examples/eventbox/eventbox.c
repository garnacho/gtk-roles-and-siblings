/* example-start eventbox eventbox.c */

#include <gtk/gtk.h>

int main( int argc,
          char *argv[] )
{
    GtkWidget *window;
    GtkWidget *event_box;
    GtkWidget *label;
    
    gtk_init (&argc, &argv);
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    
    gtk_window_set_title (GTK_WINDOW (window), "Event Box");
    
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC (gtk_exit), NULL);
    
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    
    /* Create an EventBox and add it to our toplevel window */
    
    event_box = gtk_event_box_new ();
    gtk_container_add (GTK_CONTAINER(window), event_box);
    gtk_widget_show (event_box);
    
    /* Create a long label */
    
    label = gtk_label_new ("Click here to quit, quit, quit, quit, quit");
    gtk_container_add (GTK_CONTAINER (event_box), label);
    gtk_widget_show (label);
    
    /* Clip it short. */
    gtk_widget_set_usize (label, 110, 20);
    
    /* And bind an action to it */
    gtk_widget_set_events (event_box, GDK_BUTTON_PRESS_MASK);
    gtk_signal_connect (GTK_OBJECT(event_box), "button_press_event",
			GTK_SIGNAL_FUNC (gtk_exit), NULL);
    
    /* Yet one more thing you need an X window for ... */
    
    gtk_widget_realize (event_box);
    gdk_window_set_cursor (event_box->window, gdk_cursor_new (GDK_HAND1));
    
    gtk_widget_show (window);
    
    gtk_main ();
    
    return(0);
}
/* example-end */
