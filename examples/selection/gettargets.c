/* example-start selection gettargets.c */

#include <gtk/gtk.h>

void selection_received( GtkWidget        *widget, 
                         GtkSelectionData *selection_data, 
                         gpointer          data );

/* Signal handler invoked when user clicks on the "Get Targets" button */
void get_targets( GtkWidget *widget,
                  gpointer data )
{
  static GdkAtom targets_atom = GDK_NONE;

  /* Get the atom corresponding to the string "TARGETS" */
  if (targets_atom == GDK_NONE)
    targets_atom = gdk_atom_intern ("TARGETS", FALSE);

  /* And request the "TARGETS" target for the primary selection */
  gtk_selection_convert (widget, GDK_SELECTION_PRIMARY, targets_atom,
			 GDK_CURRENT_TIME);
}

/* Signal handler called when the selections owner returns the data */
void selection_received( GtkWidget        *widget,
                         GtkSelectionData *selection_data, 
		         gpointer          data )
{
  GdkAtom *atoms;
  GList *item_list;
  int i;

  /* **** IMPORTANT **** Check to see if retrieval succeeded  */
  if (selection_data->length < 0)
    {
      g_print ("Selection retrieval failed\n");
      return;
    }
  /* Make sure we got the data in the expected form */
  if (selection_data->type != GDK_SELECTION_TYPE_ATOM)
    {
      g_print ("Selection \"TARGETS\" was not returned as atoms!\n");
      return;
    }
  
  /* Print out the atoms we received */
  atoms = (GdkAtom *)selection_data->data;

  item_list = NULL;
  for (i=0; i<selection_data->length/sizeof(GdkAtom); i++)
    {
      char *name;
      name = gdk_atom_name (atoms[i]);
      if (name != NULL)
	g_print ("%s\n",name);
      else
	g_print ("(bad atom)\n");
    }

  return;
}

int main( int   argc,
          char *argv[] )
{
  GtkWidget *window;
  GtkWidget *button;
  
  gtk_init (&argc, &argv);

  /* Create the toplevel window */

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Event Box");
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_exit), NULL);

  /* Create a button the user can click to get targets */

  button = gtk_button_new_with_label ("Get Targets");
  gtk_container_add (GTK_CONTAINER (window), button);

  gtk_signal_connect (GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC (get_targets), NULL);
  gtk_signal_connect (GTK_OBJECT(button), "selection_received",
		      GTK_SIGNAL_FUNC (selection_received), NULL);

  gtk_widget_show (button);
  gtk_widget_show (window);
  
  gtk_main ();
  
  return 0;
}
/* example-end */
