/* Application main window
 *
 * Demonstrates a typical application window, with menubar, toolbar, statusbar.
 */

#include <gtk/gtk.h>
#include "demo-common.h"

static GtkWidget *window = NULL;

static void
activate_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);

  GtkWidget *dialog;
  
  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "You activated action: \"%s\" of type \"%s\"",
                                    name, typename);

  /* Close dialog on user response */
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);
  
  gtk_widget_show (dialog);
}

static void
activate_radio_action (GtkAction *action, GtkRadioAction *current)
{
  const gchar *name = gtk_action_get_name (GTK_ACTION (current));
  const gchar *typename = G_OBJECT_TYPE_NAME (GTK_ACTION (current));
  gboolean active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (current));
  gint value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));

  if (active) 
    {
      GtkWidget *dialog;
  
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
				       GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_INFO,
				       GTK_BUTTONS_CLOSE,
				       "You activated radio action: \"%s\" of type \"%s\".\nCurrent value: %d",
				       name, typename, value);

      /* Close dialog on user response */
      g_signal_connect (dialog,
			"response",
			G_CALLBACK (gtk_widget_destroy),
			NULL);
      
      gtk_widget_show (dialog);
    }
}


static GtkActionEntry entries[] = {
  { "FileMenu", NULL, "_File" },               /* name, stock id, label */
  { "PreferencesMenu", NULL, "_Preferences" }, /* name, stock id, label */
  { "ColorMenu", NULL, "_Color"  },            /* name, stock id, label */
  { "ShapeMenu", NULL, "_Shape" },             /* name, stock id, label */
  { "HelpMenu", NULL, "_Help" },               /* name, stock id, label */
  { "New", GTK_STOCK_NEW,                      /* name, stock id */
    "_New", "<control>N",                      /* label, accelerator */
    "Create a new file",                       /* tooltip */ 
    G_CALLBACK (activate_action) },      
  { "Open", GTK_STOCK_OPEN,                    /* name, stock id */
    "_Open","<control>O",                      /* label, accelerator */     
    "Open a file",                             /* tooltip */
    G_CALLBACK (activate_action) }, 
  { "Save", GTK_STOCK_SAVE,                    /* name, stock id */
    "_Save","<control>S",                      /* label, accelerator */     
    "Save current file",                       /* tooltip */
    G_CALLBACK (activate_action) },
  { "SaveAs", GTK_STOCK_SAVE,                  /* name, stock id */
    "Save _As...", NULL,                       /* label, accelerator */     
    "Save to a file",                          /* tooltip */
    G_CALLBACK (activate_action) },
  { "Quit", GTK_STOCK_QUIT,                    /* name, stock id */
    "_Quit", "<control>Q",                     /* label, accelerator */     
    "Quit",                                    /* tooltip */
    G_CALLBACK (activate_action) },
  { "About", NULL,                             /* name, stock id */
    "_About", "<control>A",                    /* label, accelerator */     
    "About",                                   /* tooltip */  
    G_CALLBACK (activate_action) },
  { "Logo", "demo-gtk-logo",                   /* name, stock id */
     NULL, NULL,                               /* label, accelerator */     
    "GTK+",                                    /* tooltip */
    G_CALLBACK (activate_action) },
};
static guint n_entries = G_N_ELEMENTS (entries);


static GtkToggleActionEntry toggle_entries[] = {
  { "Bold", GTK_STOCK_BOLD,                    /* name, stock id */
     "_Bold", "<control>B",                    /* label, accelerator */     
    "Bold",                                    /* tooltip */
    G_CALLBACK (activate_action), 
    TRUE },                                    /* is_active */
};
static guint n_toggle_entries = G_N_ELEMENTS (toggle_entries);

enum {
  COLOR_RED,
  COLOR_GREEN,
  COLOR_BLUE
};

static GtkRadioActionEntry color_entries[] = {
  { "Red", NULL,                               /* name, stock id */
    "_Red", "<control>R",                      /* label, accelerator */     
    "Blood", COLOR_RED },                      /* tooltip, value */
  { "Green", NULL,                             /* name, stock id */
    "_Green", "<control>G",                    /* label, accelerator */     
    "Grass", COLOR_GREEN },                    /* tooltip, value */
  { "Blue", NULL,                              /* name, stock id */
    "_Blue", "<control>B",                     /* label, accelerator */     
    "Sky", COLOR_BLUE },                       /* tooltip, value */
};
static guint n_color_entries = G_N_ELEMENTS (color_entries);

enum {
  SHAPE_SQUARE,
  SHAPE_RECTANGLE,
  SHAPE_OVAL,
};

static GtkRadioActionEntry shape_entries[] = {
  { "Square", NULL,                            /* name, stock id */
    "_Square", "<control>S",                   /* label, accelerator */     
    "Square",  SHAPE_SQUARE },                 /* tooltip, value */
  { "Rectangle", NULL,                         /* name, stock id */
    "_Rectangle", "<control>R",                /* label, accelerator */     
    "Rectangle", SHAPE_RECTANGLE },            /* tooltip, value */
  { "Oval", NULL,                              /* name, stock id */
    "_Oval", "<control>O",                     /* label, accelerator */     
    "Egg", SHAPE_OVAL },                       /* tooltip, value */  
};
static guint n_shape_entries = G_N_ELEMENTS (shape_entries);

static const gchar *ui_info = 
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu action='FileMenu'>"
"      <menuitem action='New'/>"
"      <menuitem action='Open'/>"
"      <menuitem action='Save'/>"
"      <menuitem action='SaveAs'/>"
"      <separator/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='PreferencesMenu'>"
"      <menu action='ColorMenu'>"
"	<menuitem action='Red'/>"
"	<menuitem action='Green'/>"
"	<menuitem action='Blue'/>"
"      </menu>"
"      <menu action='ShapeMenu'>"
"        <menuitem action='Square'/>"
"        <menuitem action='Rectangle'/>"
"        <menuitem action='Oval'/>"
"      </menu>"
"      <menuitem action='Bold'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='About'/>"
"    </menu>"
"  </menubar>"
"  <toolbar  name='ToolBar'>"
"    <toolitem action='Open'/>"
"    <toolitem action='Quit'/>"
"    <separator action='Sep1'/>"
"    <toolitem action='Logo'/>"
"  </toolbar>"
"</ui>";



/* This function registers our custom toolbar icons, so they can be themed.
 *
 * It's totally optional to do this, you could just manually insert icons
 * and have them not be themeable, especially if you never expect people
 * to theme your app.
 */
static void
register_stock_icons (void)
{
  static gboolean registered = FALSE;
  
  if (!registered)
    {
      GdkPixbuf *pixbuf;
      GtkIconFactory *factory;
      char *filename;

      static GtkStockItem items[] = {
        { "demo-gtk-logo",
          "_GTK!",
          0, 0, NULL }
      };
      
      registered = TRUE;

      /* Register our stock items */
      gtk_stock_add (items, G_N_ELEMENTS (items));
      
      /* Add our custom icon factory to the list of defaults */
      factory = gtk_icon_factory_new ();
      gtk_icon_factory_add_default (factory);

      /* demo_find_file() looks in the the current directory first,
       * so you can run gtk-demo without installing GTK, then looks
       * in the location where the file is installed.
       */
      pixbuf = NULL;
      filename = demo_find_file ("gtk-logo-rgb.gif", NULL);
      if (filename)
	{
	  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	  g_free (filename);
	}

      /* Register icon to accompany stock item */
      if (pixbuf != NULL)
        {
          GtkIconSet *icon_set;
          GdkPixbuf *transparent;

          /* The gtk-logo-rgb icon has a white background, make it transparent */
          transparent = gdk_pixbuf_add_alpha (pixbuf, TRUE, 0xff, 0xff, 0xff);
          
          icon_set = gtk_icon_set_new_from_pixbuf (transparent);
          gtk_icon_factory_add (factory, "demo-gtk-logo", icon_set);
          gtk_icon_set_unref (icon_set);
          g_object_unref (pixbuf);
          g_object_unref (transparent);
        }
      else
        g_warning ("failed to load GTK logo for toolbar");
      
      /* Drop our reference to the factory, GTK will hold a reference. */
      g_object_unref (factory);
    }
}

static void
update_statusbar (GtkTextBuffer *buffer,
                  GtkStatusbar  *statusbar)
{
  gchar *msg;
  gint row, col;
  gint count;
  GtkTextIter iter;
  
  gtk_statusbar_pop (statusbar, 0); /* clear any previous message, underflow is allowed */

  count = gtk_text_buffer_get_char_count (buffer);

  gtk_text_buffer_get_iter_at_mark (buffer,
                                    &iter,
                                    gtk_text_buffer_get_insert (buffer));

  row = gtk_text_iter_get_line (&iter);
  col = gtk_text_iter_get_line_offset (&iter);

  msg = g_strdup_printf ("Cursor at row %d column %d - %d chars in document",
                         row, col, count);

  gtk_statusbar_push (statusbar, 0, msg);

  g_free (msg);
}

static void
mark_set_callback (GtkTextBuffer     *buffer,
                   const GtkTextIter *new_location,
                   GtkTextMark       *mark,
                   gpointer           data)
{
  update_statusbar (buffer, GTK_STATUSBAR (data));
}

static void
update_resize_grip (GtkWidget           *widget,
		    GdkEventWindowState *event,
		    GtkStatusbar        *statusbar)
{
  if (event->changed_mask & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN))
    gtk_statusbar_set_has_resize_grip (statusbar, !(event->new_window_state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)));
}
		    

GtkWidget *
do_appwindow (GtkWidget *do_widget)
{  
  if (!window)
    {
      GtkWidget *table;
      GtkWidget *statusbar;
      GtkWidget *contents;
      GtkWidget *sw;
      GtkWidget *bar;
      GtkTextBuffer *buffer;
      GtkActionGroup *action_group;
      GtkUIManager *merge;
      GError *error = NULL;

      register_stock_icons ();
      
      /* Create the toplevel window
       */
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Application Window");

      /* NULL window variable when window is closed */
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);

      table = gtk_table_new (1, 4, FALSE);
      
      gtk_container_add (GTK_CONTAINER (window), table);
      
      /* Create the menubar and toolbar
       */
      
      action_group = gtk_action_group_new ("AppWindowActions");
      gtk_action_group_add_actions (action_group, 
				    entries, n_entries, 
				    NULL);
      gtk_action_group_add_toggle_actions (action_group, 
					   toggle_entries, n_toggle_entries, 
					   NULL);
      gtk_action_group_add_radio_actions (action_group, 
					  color_entries, n_color_entries, 
					  COLOR_RED,
					  G_CALLBACK (activate_radio_action), 
					  NULL);
      gtk_action_group_add_radio_actions (action_group, 
					  shape_entries, n_shape_entries, 
					  SHAPE_SQUARE,
					  G_CALLBACK (activate_radio_action), 
					  NULL);

      merge = gtk_ui_manager_new ();
      g_object_set_data_full (G_OBJECT (window), "ui-manager", merge, g_object_unref);
      gtk_ui_manager_insert_action_group (merge, action_group, 0);
      gtk_window_add_accel_group (GTK_WINDOW (window), 
				  gtk_ui_manager_get_accel_group (merge));
      
      if (!gtk_ui_manager_add_ui_from_string (merge, ui_info, -1, &error))
	{
	  g_message ("building menus failed: %s", error->message);
	  g_error_free (error);
	}

      bar = gtk_ui_manager_get_widget (merge, "/MenuBar");
      gtk_widget_show (bar);
      gtk_table_attach (GTK_TABLE (table),
			bar, 
                        /* X direction */          /* Y direction */
                        0, 1,                      0, 1,
                        GTK_EXPAND | GTK_FILL,     0,
                        0,                         0);

      bar = gtk_ui_manager_get_widget (merge, "/ToolBar");
      gtk_toolbar_set_tooltips (GTK_TOOLBAR (bar), TRUE);
      gtk_widget_show (bar);
      gtk_table_attach (GTK_TABLE (table),
			bar, 
                        /* X direction */       /* Y direction */
                        0, 1,                   1, 2,
                        GTK_EXPAND | GTK_FILL,  0,
                        0,                      0);

      /* Create document
       */

      sw = gtk_scrolled_window_new (NULL, NULL);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);

      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                           GTK_SHADOW_IN);
      
      gtk_table_attach (GTK_TABLE (table),
                        sw,
                        /* X direction */       /* Y direction */
                        0, 1,                   2, 3,
                        GTK_EXPAND | GTK_FILL,  GTK_EXPAND | GTK_FILL,
                        0,                      0);

      gtk_window_set_default_size (GTK_WINDOW (window),
                                   200, 200);
      
      contents = gtk_text_view_new ();
      gtk_widget_grab_focus (contents);
      
      gtk_container_add (GTK_CONTAINER (sw),
                         contents);

      /* Create statusbar */

      statusbar = gtk_statusbar_new ();
      gtk_table_attach (GTK_TABLE (table),
                        statusbar,
                        /* X direction */       /* Y direction */
                        0, 1,                   3, 4,
                        GTK_EXPAND | GTK_FILL,  0,
                        0,                      0);

      /* Show text widget info in the statusbar */
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (contents));
      
      g_signal_connect_object (buffer,
                               "changed",
                               G_CALLBACK (update_statusbar),
                               statusbar,
                               0);

      g_signal_connect_object (buffer,
                               "mark_set", /* cursor moved */
                               G_CALLBACK (mark_set_callback),
                               statusbar,
                               0);

      g_signal_connect_object (window, 
			       "window_state_event", 
			       G_CALLBACK (update_resize_grip),
			       statusbar,
			       0);
      
      update_statusbar (buffer, GTK_STATUSBAR (statusbar));
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

