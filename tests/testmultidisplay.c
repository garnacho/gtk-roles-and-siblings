#include <strings.h>
#include <gtk/gtk.h>
#include <gtk/gtkstock.h>
#include <gdk/gdk.h>

gchar *screen2_name = NULL;

typedef struct
{
  GtkEntry *e1;
  GtkEntry *e2;
}
MyDoubleGtkEntry;

void
get_screen_response (GtkDialog *dialog,
		     gint       response_id,
		     GtkEntry  *entry)
{
  if (response_id == GTK_RESPONSE_DELETE_EVENT)
    return;
  if (screen2_name)
    g_free (screen2_name);
  screen2_name = g_strdup (gtk_entry_get_text (entry));
}

void
entry_dialog_response (GtkDialog        *dialog,
		       gint              response_id,
		       MyDoubleGtkEntry *de)
{
  if (response_id == GTK_RESPONSE_APPLY)
    gtk_entry_set_text (de->e2, gtk_entry_get_text (de->e1));
  else
    gtk_main_quit ();
}

void
make_selection_dialog (GdkScreen * screen,
		       GtkWidget * entry,
		       GtkWidget * other_entry)
{
  GtkWidget *window, *vbox;
  MyDoubleGtkEntry *double_entry = g_new (MyDoubleGtkEntry, 1);
  double_entry->e1 = GTK_ENTRY (entry);
  double_entry->e2 = GTK_ENTRY (other_entry);

  if (!screen)
    screen = gdk_screen_get_default ();

  window = gtk_widget_new (GTK_TYPE_DIALOG,
			   "screen", screen,
			   "user_data", NULL,
			   "type", GTK_WINDOW_TOPLEVEL,
			   "title", "MultiDisplay Cut & Paste",
			   "border_width", 10, NULL);
  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);


  vbox = gtk_widget_new (GTK_TYPE_VBOX,
			 "border_width", 5,
			 NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
  gtk_widget_grab_focus (entry);

  gtk_dialog_add_buttons (GTK_DIALOG (window),
			  GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
			  GTK_STOCK_QUIT, GTK_RESPONSE_DELETE_EVENT,
			  NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (window), GTK_RESPONSE_APPLY);

  g_signal_connect (G_OBJECT (window), "response",
		    G_CALLBACK (entry_dialog_response), double_entry);

  gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
  GtkWidget *dialog, *display_entry, *dialog_label;
  GtkWidget *entry, *entry2;
  GdkDisplay *dpy2;
  GdkScreen *scr2 = NULL;	/* Quiet GCC */
  gboolean correct_second_display = FALSE;

  gtk_init (&argc, &argv);

  if (argc == 2)
    screen2_name = g_strdup (argv[1]);
  
  /* Get the second display information */

  dialog = gtk_dialog_new_with_buttons ("Second Display Selection",
					NULL,
					GTK_DIALOG_MODAL,
					GTK_STOCK_OK,
					GTK_RESPONSE_OK, NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  display_entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (display_entry), TRUE);
  dialog_label =
    gtk_label_new ("Please enter the name of\nthe second display\n");

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog_label);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
		     display_entry);
  gtk_signal_connect (GTK_OBJECT (dialog), "response",
		      GTK_SIGNAL_FUNC (get_screen_response), display_entry);

  gtk_widget_grab_focus (display_entry);
  gtk_widget_show_all (GTK_BIN (dialog)->child);
  
  while (!correct_second_display)
    {
      if (screen2_name)
	{
	  if (!g_strcasecmp (screen2_name, ""))
	    g_printerr ("No display name, reverting to default display\n");
	  
	  dpy2 = gdk_display_open (screen2_name);
	  if (dpy2)
	    {
	      scr2 = gdk_display_get_default_screen (dpy2);
	      correct_second_display = TRUE;
	    }
	  else
	    {
	      char *error_msg =
		g_strdup_printf  ("Can't open display :\n\t%s\nplease try another one\n",
				  screen2_name);
	      gtk_label_set_text (GTK_LABEL (dialog_label), error_msg);
	      g_free (error_msg);
	    }
       }

      if (!correct_second_display)
	gtk_dialog_run (GTK_DIALOG (dialog));
    }
  
  gtk_widget_destroy (dialog);

  entry = gtk_widget_new (GTK_TYPE_ENTRY,
			  "activates_default", TRUE,
			  "visible", TRUE,
			  NULL);
  entry2 = gtk_widget_new (GTK_TYPE_ENTRY,
			   "activates_default", TRUE,
			   "visible", TRUE,
			   NULL);

  /* for default display */
  make_selection_dialog (NULL, entry2, entry);
  /* for selected display */
  make_selection_dialog (scr2, entry, entry2);
  gtk_main ();

  return 0;
}
