/* Menus
 *
 * There are several widgets involved in displaying menus. The
 * GtkMenuBar widget is a horizontal menu bar, which normally appears
 * at the top of an application. The GtkMenu widget is the actual menu
 * that pops up. Both GtkMenuBar and GtkMenu are subclasses of
 * GtkMenuShell; a GtkMenuShell contains menu items
 * (GtkMenuItem). Each menu item contains text and/or images and can
 * be selected by the user.
 *
 * There are several kinds of menu item, including plain GtkMenuItem,
 * GtkCheckMenuItem which can be checked/unchecked, GtkRadioMenuItem
 * which is a check menu item that's in a mutually exclusive group,
 * GtkSeparatorMenuItem which is a separator bar, GtkTearoffMenuItem
 * which allows a GtkMenu to be torn off, and GtkImageMenuItem which
 * can place a GtkImage or other widget next to the menu text.
 *
 * A GtkMenuItem can have a submenu, which is simply a GtkMenu to pop
 * up when the menu item is selected. Typically, all menu items in a menu bar
 * have submenus.
 *
 * The GtkOptionMenu widget is a button that pops up a GtkMenu when clicked.
 * It's used inside dialogs and such.
 *
 * GtkItemFactory provides a higher-level interface for creating menu bars
 * and menus; while you can construct menus manually, most people don't
 * do that. There's a separate demo for GtkItemFactory.
 * 
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <stdio.h>

static GtkWidget *
create_menu (gint     depth,
	     gboolean tearoff)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  GSList *group;
  char buf[32];
  int i, j;

  if (depth < 1)
    return NULL;

  menu = gtk_menu_new ();
  group = NULL;

  if (tearoff)
    {
      menuitem = gtk_tearoff_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
    }

  for (i = 0, j = 1; i < 5; i++, j++)
    {
      sprintf (buf, "item %2d - %d", depth, j);
      menuitem = gtk_radio_menu_item_new_with_label (group, buf);
      group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      if (i == 3)
	gtk_widget_set_sensitive (menuitem, FALSE);

      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (depth - 1, TRUE));
    }

  return menu;
}

GtkWidget *
do_menus (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *optionmenu;
  GtkWidget *separator;
  
  if (!window)
    {
      GtkWidget *menubar;
      GtkWidget *menu;
      GtkWidget *menuitem;
      GtkAccelGroup *accel_group;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete-event",
			  GTK_SIGNAL_FUNC (gtk_true),
			  NULL);
      
      accel_group = gtk_accel_group_new ();
      gtk_accel_group_attach (accel_group, GTK_OBJECT (window));

      gtk_window_set_title (GTK_WINDOW (window), "menus");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);
      
      menubar = gtk_menu_bar_new ();
      gtk_box_pack_start (GTK_BOX (box1), menubar, FALSE, TRUE, 0);
      gtk_widget_show (menubar);
      
      menu = create_menu (2, TRUE);
      
      menuitem = gtk_menu_item_new_with_label ("test\nline2");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      menuitem = gtk_menu_item_new_with_label ("foo");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (3, TRUE));
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);

      menuitem = gtk_menu_item_new_with_label ("bar");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (4, TRUE));
      gtk_menu_item_right_justify (GTK_MENU_ITEM (menuitem));
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      menu = create_menu (1, FALSE);
      gtk_menu_set_accel_group (GTK_MENU (menu), accel_group);
      
      menuitem = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      
      menuitem = gtk_check_menu_item_new_with_label ("Accelerate Me");
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_F1,
				  0,
				  GTK_ACCEL_VISIBLE | GTK_ACCEL_SIGNAL_VISIBLE);
      menuitem = gtk_check_menu_item_new_with_label ("Accelerator Locked");
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_F2,
				  0,
				  GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
      menuitem = gtk_check_menu_item_new_with_label ("Accelerators Frozen");
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_F2,
				  0,
				  GTK_ACCEL_VISIBLE);
      gtk_widget_add_accelerator (menuitem,
				  "activate",
				  accel_group,
				  GDK_F3,
				  0,
				  GTK_ACCEL_VISIBLE);
      gtk_widget_lock_accelerators (menuitem);
      
      optionmenu = gtk_option_menu_new ();
      gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);
      gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 3);
      gtk_box_pack_start (GTK_BOX (box2), optionmenu, TRUE, TRUE, 0);
      gtk_widget_show (optionmenu);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
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
