/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#undef	G_LOG_DOMAIN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define GTK_ENABLE_BROKEN
#include "gtk/gtk.h"
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"

#ifdef G_OS_WIN32
#define sleep(n) _sleep(n)
#endif

#include "prop-editor.h"

#include "circles.xbm"
#include "test.xpm"

gboolean
file_exists (const char *filename)
{
  struct stat statbuf;

  return stat (filename, &statbuf) == 0;
}

GtkWidget *
shape_create_icon (char     *xpm_file,
		   gint      x,
		   gint      y,
		   gint      px,
		   gint      py,
		   gint      window_type);

static GtkWidget *
build_option_menu (gchar    *items[],
		   gint      num_items,
		   gint      history,
		   void    (*func)(GtkWidget *widget, gpointer data),
		   gpointer  data);

/* macro, structure and variables used by tree window demos */
#define DEFAULT_NUMBER_OF_ITEM  3
#define DEFAULT_RECURSION_LEVEL 3

struct {
  GSList* selection_mode_group;
  GtkWidget* single_button;
  GtkWidget* browse_button;
  GtkWidget* multiple_button;
  GtkWidget* draw_line_button;
  GtkWidget* view_line_button;
  GtkWidget* no_root_item_button;
  GtkWidget* nb_item_spinner;
  GtkWidget* recursion_spinner;
} sTreeSampleSelection;

typedef struct sTreeButtons {
  guint nb_item_add;
  GtkWidget* add_button;
  GtkWidget* remove_button;
  GtkWidget* subtree_button;
} sTreeButtons;
/* end of tree section */

static GtkWidget *
build_option_menu (gchar     *items[],
		   gint       num_items,
		   gint       history,
		   void     (*func)(GtkWidget *widget, gpointer data),
		   gpointer   data)
{
  GtkWidget *omenu;
  GtkWidget *menu;
  GtkWidget *menu_item;
  GSList *group;
  gint i;

  omenu = gtk_option_menu_new ();
  gtk_signal_connect (GTK_OBJECT (omenu), "changed",
		      GTK_SIGNAL_FUNC (func), data);
      
  menu = gtk_menu_new ();
  group = NULL;
  
  for (i = 0; i < num_items; i++)
    {
      menu_item = gtk_radio_menu_item_new_with_label (group, items[i]);
      group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_item));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
      if (i == history)
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), TRUE);
      gtk_widget_show (menu_item);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), history);
  
  return omenu;
}

static void
destroy_tooltips (GtkWidget *widget, GtkWindow **window)
{
  GtkTooltips *tt = gtk_object_get_data (GTK_OBJECT (*window), "tooltips");
  gtk_object_unref (GTK_OBJECT (tt));
  *window = NULL;
}


/*
 * Big windows and guffaw scrolling
 */

static gboolean
pattern_expose (GtkWidget      *widget,
		GdkEventExpose *event,
		gpointer        data)
{
  GdkColor *color;
  GdkWindow *window = event->window;

  color = g_object_get_data (G_OBJECT (window), "pattern-color");
  if (color)
    {
      GdkGC *tmp_gc = gdk_gc_new (window);
      gdk_gc_set_rgb_fg_color (tmp_gc, color);

      gdk_draw_rectangle (window, tmp_gc, TRUE,
			  event->area.x, event->area.y,
			  event->area.width, event->area.height);

      g_object_unref (G_OBJECT (tmp_gc));
    }

  return FALSE;
}

static void
pattern_set_bg (GtkWidget   *widget,
		GdkWindow   *child,
		gint         level)
{
  static const GdkColor colors[] = {
    { 0, 0x4444, 0x4444, 0xffff },
    { 0, 0x8888, 0x8888, 0xffff },
    { 0, 0xaaaa, 0xaaaa, 0xffff }
  };
    
  g_object_set_data (G_OBJECT (child), "pattern-color", (gpointer)&colors[level]);
  gdk_window_set_user_data (child, widget);
}

static void
create_pattern (GtkWidget   *widget,
		GdkWindow   *parent,
		gint         level,
		gint         width,
		gint         height)
{
  gint h = 1;
  gint i = 0;
    
  GdkWindow *child;

  while (2 * h <= height)
    {
      gint w = 1;
      gint j = 0;
      
      while (2 * w <= width)
	{
	  if ((i + j) % 2 == 0)
	    {
	      gint x = w  - 1;
	      gint y = h - 1;
	      
	      GdkWindowAttr attributes;

	      attributes.window_type = GDK_WINDOW_CHILD;
	      attributes.x = x;
	      attributes.y = y;
	      attributes.width = w;
	      attributes.height = h;
	      attributes.wclass = GDK_INPUT_OUTPUT;
	      attributes.event_mask = GDK_EXPOSURE_MASK;
	      attributes.visual = gtk_widget_get_visual (widget);
	      attributes.colormap = gtk_widget_get_colormap (widget);
	      
	      child = gdk_window_new (parent, &attributes,
				      GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP);

	      pattern_set_bg (widget, child, level);

	      if (level < 2)
		create_pattern (widget, child, level + 1, w, h);

	      gdk_window_show (child);
	    }
	  j++;
	  w *= 2;
	}
      i++;
      h *= 2;
    }
}

#define PATTERN_SIZE (1 << 18)

static void
pattern_hadj_changed (GtkAdjustment *adj,
		      GtkWidget     *darea)
{
  gint *old_value = g_object_get_data (G_OBJECT (adj), "old-value");
  gint new_value = adj->value;

  if (GTK_WIDGET_REALIZED (darea))
    {
      gdk_window_scroll (darea->window, *old_value - new_value, 0);
      *old_value = new_value;
    }
}

static void
pattern_vadj_changed (GtkAdjustment *adj,
		      GtkWidget *darea)
{
  gint *old_value = g_object_get_data (G_OBJECT (adj), "old-value");
  gint new_value = adj->value;

  if (GTK_WIDGET_REALIZED (darea))
    {
      gdk_window_scroll (darea->window, 0, *old_value - new_value);
      *old_value = new_value;
    }
}

static void
pattern_realize (GtkWidget *widget,
		 gpointer   data)
{
  pattern_set_bg (widget, widget->window, 0);
  create_pattern (widget, widget->window, 1, PATTERN_SIZE, PATTERN_SIZE);
}

static void 
create_big_windows (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *darea, *table, *scrollbar;
  GtkWidget *eventbox;
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  static gint current_x;
  static gint current_y;

  if (!window)
    {
      current_x = 0;
      current_y = 0;
      
      window = gtk_dialog_new_with_buttons ("Big Windows",
                                            NULL, 0,
                                            GTK_STOCK_CLOSE,
                                            GTK_RESPONSE_NONE,
                                            NULL);

      gtk_window_set_default_size (GTK_WINDOW (window), 200, 300);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      gtk_signal_connect (GTK_OBJECT (window), "response",
                          GTK_SIGNAL_FUNC (gtk_widget_destroy),
                          NULL);

      table = gtk_table_new (2, 2, FALSE);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox),
			  table, TRUE, TRUE, 0);

      darea = gtk_drawing_area_new ();

      hadj = (GtkAdjustment *)gtk_adjustment_new (0, 0, PATTERN_SIZE, 10, 100, 100);
      gtk_signal_connect (GTK_OBJECT (hadj), "value_changed",
			  GTK_SIGNAL_FUNC (pattern_hadj_changed), darea);
      g_object_set_data (G_OBJECT (hadj), "old-value", &current_x);
      
      vadj = (GtkAdjustment *)gtk_adjustment_new (0, 0, PATTERN_SIZE, 10, 100, 100);
      gtk_signal_connect (GTK_OBJECT (vadj), "value_changed",
			  GTK_SIGNAL_FUNC (pattern_vadj_changed), darea);
      g_object_set_data (G_OBJECT (vadj), "old-value", &current_y);
      
      gtk_signal_connect (GTK_OBJECT (darea), "realize",
                          GTK_SIGNAL_FUNC (pattern_realize),
                          NULL);
      gtk_signal_connect (GTK_OBJECT (darea), "expose_event",
                          GTK_SIGNAL_FUNC (pattern_expose),
                          NULL);

      eventbox = gtk_event_box_new ();
      gtk_table_attach (GTK_TABLE (table), eventbox,
			0, 1,                  0, 1,
			GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			0,                     0);

      gtk_container_add (GTK_CONTAINER (eventbox), darea);

      scrollbar = gtk_hscrollbar_new (hadj);
      gtk_table_attach (GTK_TABLE (table), scrollbar,
			0, 1,                  1, 2,
			GTK_FILL | GTK_EXPAND, GTK_FILL,
			0,                     0);

      scrollbar = gtk_vscrollbar_new (vadj);
      gtk_table_attach (GTK_TABLE (table), scrollbar,
			1, 2,                  0, 1,
			GTK_FILL,              GTK_EXPAND | GTK_FILL,
			0,                     0);

    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_hide (window);
}

/*
 * GtkButton
 */

static void
button_window (GtkWidget *widget,
	       GtkWidget *button)
{
  if (!GTK_WIDGET_VISIBLE (button))
    gtk_widget_show (button);
  else
    gtk_widget_hide (button);
}

static void
create_buttons (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *table;
  GtkWidget *button[10];
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkButton");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      table = gtk_table_new (3, 3, FALSE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 5);
      gtk_table_set_col_spacings (GTK_TABLE (table), 5);
      gtk_container_set_border_width (GTK_CONTAINER (table), 10);
      gtk_box_pack_start (GTK_BOX (box1), table, TRUE, TRUE, 0);

      button[0] = gtk_button_new_with_label ("button1");
      button[1] = gtk_button_new_with_mnemonic ("_button2");
      button[2] = gtk_button_new_with_mnemonic ("_button3");
      button[3] = gtk_button_new_from_stock (GTK_STOCK_OK);
      button[4] = gtk_button_new_with_label ("button5");
      button[5] = gtk_button_new_with_label ("button6");
      button[6] = gtk_button_new_with_label ("button7");
      button[7] = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
      button[8] = gtk_button_new_with_label ("button9");
      
      gtk_signal_connect (GTK_OBJECT (button[0]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[1]);

      gtk_table_attach (GTK_TABLE (table), button[0], 0, 1, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      gtk_signal_connect (GTK_OBJECT (button[1]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[2]);

      gtk_table_attach (GTK_TABLE (table), button[1], 1, 2, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      gtk_signal_connect (GTK_OBJECT (button[2]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[3]);
      gtk_table_attach (GTK_TABLE (table), button[2], 2, 3, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      gtk_signal_connect (GTK_OBJECT (button[3]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[4]);
      gtk_table_attach (GTK_TABLE (table), button[3], 0, 1, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      gtk_signal_connect (GTK_OBJECT (button[4]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[5]);
      gtk_table_attach (GTK_TABLE (table), button[4], 2, 3, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      gtk_signal_connect (GTK_OBJECT (button[5]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[6]);
      gtk_table_attach (GTK_TABLE (table), button[5], 1, 2, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      gtk_signal_connect (GTK_OBJECT (button[6]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[7]);
      gtk_table_attach (GTK_TABLE (table), button[6], 1, 2, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      gtk_signal_connect (GTK_OBJECT (button[7]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[8]);
      gtk_table_attach (GTK_TABLE (table), button[7], 2, 3, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      gtk_signal_connect (GTK_OBJECT (button[8]), "clicked",
			  GTK_SIGNAL_FUNC(button_window),
			  button[0]);
      gtk_table_attach (GTK_TABLE (table), button[8], 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button[9] = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button[9]), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button[9], TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button[9], GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button[9]);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkToggleButton
 */

static void
create_toggle_buttons (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkToggleButton");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("inconsistent");
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkCheckButton
 */

static void
create_check_buttons (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkCheckButton");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_check_button_new_with_mnemonic ("_button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_check_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_check_button_new_with_label ("button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_check_button_new_with_label ("inconsistent");
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkRadioButton
 */

static void
create_radio_buttons (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "radio buttons");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (NULL, "button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
	         gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "button2");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
                 gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "button3");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
                 gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "inconsistent");
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (NULL, "button4");
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
	         gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "button5");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_radio_button_new_with_label (
                 gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
		 "button6");
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkButtonBox
 */

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
  gtk_box_set_spacing (GTK_BOX (bbox), spacing);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), child_w, child_h);
  
  button = gtk_button_new_with_label ("OK");
  gtk_container_add (GTK_CONTAINER (bbox), button);
  
  button = gtk_button_new_with_label ("Cancel");
  gtk_container_add (GTK_CONTAINER (bbox), button);
  
  button = gtk_button_new_with_label ("Help");
  gtk_container_add (GTK_CONTAINER (bbox), button);

  return frame;
}

static void
create_button_box (void)
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
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkToolBar
 */

static GtkWidget*
new_pixmap (char      *filename,
	    GdkWindow *window,
	    GdkColor  *background)
{
  GtkWidget *wpixmap;
  GdkPixmap *pixmap;
  GdkBitmap *mask;

  if (strcmp (filename, "test.xpm") == 0 ||
      !file_exists (filename))
    {
      pixmap = gdk_pixmap_create_from_xpm_d (window, &mask,
					     background,
					     openfile);
    }
  else
    pixmap = gdk_pixmap_create_from_xpm (window, &mask,
					 background,
					 filename);
  
  wpixmap = gtk_pixmap_new (pixmap, mask);

  return wpixmap;
}


static void
set_toolbar_small_stock (GtkWidget *widget,
			 gpointer   data)
{
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (data), GTK_ICON_SIZE_SMALL_TOOLBAR);
}

static void
set_toolbar_large_stock (GtkWidget *widget,
			 gpointer   data)
{
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (data), GTK_ICON_SIZE_LARGE_TOOLBAR);
}

static void
set_toolbar_horizontal (GtkWidget *widget,
			gpointer   data)
{
  gtk_toolbar_set_orientation (GTK_TOOLBAR (data), GTK_ORIENTATION_HORIZONTAL);
}

static void
set_toolbar_vertical (GtkWidget *widget,
		      gpointer   data)
{
  gtk_toolbar_set_orientation (GTK_TOOLBAR (data), GTK_ORIENTATION_VERTICAL);
}

static void
set_toolbar_icons (GtkWidget *widget,
		   gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_ICONS);
}

static void
set_toolbar_text (GtkWidget *widget,
	          gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_TEXT);
}

static void
set_toolbar_both (GtkWidget *widget,
		  gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_BOTH);
}

static void
set_toolbar_both_horiz (GtkWidget *widget,
			gpointer   data)
{
  gtk_toolbar_set_style (GTK_TOOLBAR (data), GTK_TOOLBAR_BOTH_HORIZ);
}

static void
set_toolbar_enable (GtkWidget *widget,
		    gpointer   data)
{
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (data), TRUE);
}

static void
set_toolbar_disable (GtkWidget *widget,
		     gpointer   data)
{
  gtk_toolbar_set_tooltips (GTK_TOOLBAR (data), FALSE);
}

static void
create_toolbar (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *toolbar;
  GtkWidget *entry;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Toolbar test");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      gtk_widget_realize (window);

      toolbar = gtk_toolbar_new ();

      gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
				GTK_STOCK_NEW,
				"Stock icon: New", "Toolbar/New",
				(GtkSignalFunc) set_toolbar_small_stock, toolbar, -1);
      
      gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
				GTK_STOCK_OPEN,
				"Stock icon: Open", "Toolbar/Open",
				(GtkSignalFunc) set_toolbar_large_stock, toolbar, -1);
      
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Horizontal", "Horizontal toolbar layout", "Toolbar/Horizontal",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_horizontal, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Vertical", "Vertical toolbar layout", "Toolbar/Vertical",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_vertical, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR(toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Icons", "Only show toolbar icons", "Toolbar/IconsOnly",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_icons, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Text", "Only show toolbar text", "Toolbar/TextOnly",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_text, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Both", "Show toolbar icons and text", "Toolbar/Both",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_both, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Both (horizontal)",
			       "Show toolbar icons and text in a horizontal fashion",
			       "Toolbar/BothHoriz",
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_both_horiz, toolbar);
			       
      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      entry = gtk_entry_new ();

      gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), entry, "This is an unusable GtkEntry ;)", "Hey don't click me!!!");

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));


      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Enable", "Enable tooltips", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_enable, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Disable", "Disable tooltips", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) set_toolbar_disable, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Frobate", "Frobate tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) NULL, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Baz", "Baz tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) NULL, toolbar);

      gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
      
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Blah", "Blah tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) NULL, toolbar);
      gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			       "Bar", "Bar tooltip", NULL,
			       new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			       (GtkSignalFunc) NULL, toolbar);

      gtk_container_add (GTK_CONTAINER (window), toolbar);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

static GtkWidget*
make_toolbar (GtkWidget *window)
{
  GtkWidget *toolbar;

  if (!GTK_WIDGET_REALIZED (window))
    gtk_widget_realize (window);

  toolbar = gtk_toolbar_new ();

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Horizontal", "Horizontal toolbar layout", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_horizontal, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Vertical", "Vertical toolbar layout", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_vertical, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR(toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Icons", "Only show toolbar icons", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_icons, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Text", "Only show toolbar text", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_text, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Both", "Show toolbar icons and text", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_both, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Woot", "Woot woot woot", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) NULL, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Blah", "Blah blah blah", "Toolbar/Big",
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) NULL, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Enable", "Enable tooltips", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_enable, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Disable", "Disable tooltips", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) set_toolbar_disable, toolbar);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
  
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Hoo", "Hoo tooltip", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) NULL, toolbar);
  gtk_toolbar_append_item (GTK_TOOLBAR (toolbar),
			   "Woo", "Woo tooltip", NULL,
			   new_pixmap ("test.xpm", window->window, &window->style->bg[GTK_STATE_NORMAL]),
			   (GtkSignalFunc) NULL, toolbar);

  return toolbar;
}

/*
 * GtkStatusBar
 */

static guint statusbar_counter = 1;

static void
statusbar_push (GtkWidget *button,
		GtkStatusbar *statusbar)
{
  gchar text[1024];

  sprintf (text, "something %d", statusbar_counter++);

  gtk_statusbar_push (statusbar, 1, text);
}

static void
statusbar_pop (GtkWidget *button,
	       GtkStatusbar *statusbar)
{
  gtk_statusbar_pop (statusbar, 1);
}

static void
statusbar_steal (GtkWidget *button,
	         GtkStatusbar *statusbar)
{
  gtk_statusbar_remove (statusbar, 1, 4);
}

static void
statusbar_popped (GtkStatusbar  *statusbar,
		  guint          context_id,
		  const gchar	*text)
{
  if (!statusbar->messages)
    statusbar_counter = 1;
}

static void
statusbar_contexts (GtkStatusbar *statusbar)
{
  gchar *string;

  string = "any context";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
  
  string = "idle messages";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
  
  string = "some text";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));

  string = "hit the mouse";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));

  string = "hit the mouse2";
  g_print ("GtkStatusBar: context=\"%s\", context_id=%d\n",
	   string,
	   gtk_statusbar_get_context_id (statusbar, string));
}

static void
create_statusbar (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *statusbar;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "statusbar");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      statusbar = gtk_statusbar_new ();
      gtk_box_pack_end (GTK_BOX (box1), statusbar, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (statusbar),
			  "text_popped",
			  GTK_SIGNAL_FUNC (statusbar_popped),
			  NULL);

      button = gtk_widget_new (gtk_button_get_type (),
			       "label", "push something",
			       "visible", TRUE,
			       "parent", box2,
			       NULL);
      g_object_connect (G_OBJECT (button),
			"signal::clicked", statusbar_push, statusbar,
			NULL);

      button = g_object_connect (gtk_widget_new (gtk_button_get_type (),
						 "label", "pop",
						 "visible", TRUE,
						 "parent", box2,
						 NULL),
				 "signal_after::clicked", statusbar_pop, statusbar,
				 NULL);

      button = g_object_connect (gtk_widget_new (gtk_button_get_type (),
						 "label", "steal #4",
						 "visible", TRUE,
						 "parent", box2,
						 NULL),
				 "signal_after::clicked", statusbar_steal, statusbar,
				 NULL);

      button = g_object_connect (gtk_widget_new (gtk_button_get_type (),
						 "label", "test contexts",
						 "visible", TRUE,
						 "parent", box2,
						 NULL),
				 "swapped_signal_after::clicked", statusbar_contexts, statusbar,
				 NULL);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkTree
 */

static void
cb_tree_destroy_event(GtkWidget* w)
{
  sTreeButtons* tree_buttons;

  /* free buttons structure associate at this tree */
  tree_buttons = gtk_object_get_user_data (GTK_OBJECT (w));
  g_free (tree_buttons);
}

static void
cb_add_new_item(GtkWidget* w, GtkTree* tree)
{
  sTreeButtons* tree_buttons;
  GList* selected_list;
  GtkWidget* selected_item;
  GtkWidget* subtree;
  GtkWidget* item_new;
  char buffer[255];

  tree_buttons = gtk_object_get_user_data(GTK_OBJECT(tree));

  selected_list = GTK_TREE_SELECTION_OLD(tree);

  if(selected_list == NULL)
    {
      /* there is no item in tree */
      subtree = GTK_WIDGET(tree);
    }
  else
    {
      /* list can have only one element */
      selected_item = GTK_WIDGET(selected_list->data);
      
      subtree = GTK_TREE_ITEM_SUBTREE(selected_item);

      if(subtree == NULL)
	{
	  /* current selected item have not subtree ... create it */
	  subtree = gtk_tree_new();
	  gtk_tree_item_set_subtree(GTK_TREE_ITEM(selected_item), 
				    subtree);
	}
    }

  /* at this point, we know which subtree will be used to add new item */
  /* create a new item */
  sprintf(buffer, "item add %d", tree_buttons->nb_item_add);
  item_new = gtk_tree_item_new_with_label(buffer);
  gtk_tree_append(GTK_TREE(subtree), item_new);
  gtk_widget_show(item_new);

  tree_buttons->nb_item_add++;
}

static void
cb_remove_item(GtkWidget*w, GtkTree* tree)
{
  GList* selected_list;
  GList* clear_list;
  
  selected_list = GTK_TREE_SELECTION_OLD(tree);

  clear_list = NULL;
    
  while (selected_list) 
    {
      clear_list = g_list_prepend (clear_list, selected_list->data);
      selected_list = selected_list->next;
    }
  
  clear_list = g_list_reverse (clear_list);
  gtk_tree_remove_items(tree, clear_list);

  g_list_free (clear_list);
}

static void
cb_remove_subtree(GtkWidget*w, GtkTree* tree)
{
  GList* selected_list;
  GtkTreeItem *item;
  
  selected_list = GTK_TREE_SELECTION_OLD(tree);

  if (selected_list)
    {
      item = GTK_TREE_ITEM (selected_list->data);
      if (item->subtree)
	gtk_tree_item_remove_subtree (item);
    }
}

static void
cb_tree_changed(GtkTree* tree)
{
  sTreeButtons* tree_buttons;
  GList* selected_list;
  guint nb_selected;

  tree_buttons = gtk_object_get_user_data(GTK_OBJECT(tree));

  selected_list = GTK_TREE_SELECTION_OLD(tree);
  nb_selected = g_list_length(selected_list);

  if(nb_selected == 0) 
    {
      if(tree->children == NULL)
	gtk_widget_set_sensitive(tree_buttons->add_button, TRUE);
      else
	gtk_widget_set_sensitive(tree_buttons->add_button, FALSE);
      gtk_widget_set_sensitive(tree_buttons->remove_button, FALSE);
      gtk_widget_set_sensitive(tree_buttons->subtree_button, FALSE);
    } 
  else 
    {
      gtk_widget_set_sensitive(tree_buttons->remove_button, TRUE);
      gtk_widget_set_sensitive(tree_buttons->add_button, (nb_selected == 1));
      gtk_widget_set_sensitive(tree_buttons->subtree_button, (nb_selected == 1));
    }  
}

static void 
create_subtree(GtkWidget* item, guint level, guint nb_item_max, guint recursion_level_max)
{
  GtkWidget* item_subtree;
  GtkWidget* item_new;
  guint nb_item;
  char buffer[255];
  int no_root_item;

  if(level == recursion_level_max) return;

  if(level == -1)
    {
      /* query with no root item */
      level = 0;
      item_subtree = item;
      no_root_item = 1;
    }
  else
    {
      /* query with no root item */
      /* create subtree and associate it with current item */
      item_subtree = gtk_tree_new();
      no_root_item = 0;
    }
  
  for(nb_item = 0; nb_item < nb_item_max; nb_item++)
    {
      sprintf(buffer, "item %d-%d", level, nb_item);
      item_new = gtk_tree_item_new_with_label(buffer);
      gtk_tree_append(GTK_TREE(item_subtree), item_new);
      create_subtree(item_new, level+1, nb_item_max, recursion_level_max);
      gtk_widget_show(item_new);
    }

  if(!no_root_item)
    gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), item_subtree);
}

static void
create_tree_sample(guint selection_mode, 
		   guint draw_line, guint view_line, guint no_root_item,
		   guint nb_item_max, guint recursion_level_max) 
{
  GtkWidget* window;
  GtkWidget* box1;
  GtkWidget* box2;
  GtkWidget* separator;
  GtkWidget* button;
  GtkWidget* scrolled_win;
  GtkWidget* root_tree;
  GtkWidget* root_item;
  sTreeButtons* tree_buttons;

  /* create tree buttons struct */
  if ((tree_buttons = g_malloc (sizeof (sTreeButtons))) == NULL)
    {
      g_error("can't allocate memory for tree structure !\n");
      return;
    }
  tree_buttons->nb_item_add = 0;

  /* create top level window */
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Tree Sample");
  gtk_signal_connect(GTK_OBJECT(window), "destroy",
		     (GtkSignalFunc) cb_tree_destroy_event, NULL);
  gtk_object_set_user_data(GTK_OBJECT(window), tree_buttons);

  box1 = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), box1);
  gtk_widget_show(box1);

  /* create tree box */
  box2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(box2), 5);
  gtk_widget_show(box2);

  /* create scrolled window */
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (box2), scrolled_win, TRUE, TRUE, 0);
  gtk_widget_set_usize (scrolled_win, 200, 200);
  gtk_widget_show (scrolled_win);
  
  /* create root tree widget */
  root_tree = gtk_tree_new();
  gtk_signal_connect(GTK_OBJECT(root_tree), "selection_changed",
		     (GtkSignalFunc)cb_tree_changed,
		     (gpointer)NULL);
  gtk_object_set_user_data(GTK_OBJECT(root_tree), tree_buttons);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_win), root_tree);
  gtk_tree_set_selection_mode(GTK_TREE(root_tree), selection_mode);
  gtk_tree_set_view_lines(GTK_TREE(root_tree), draw_line);
  gtk_tree_set_view_mode(GTK_TREE(root_tree), !view_line);
  gtk_widget_show(root_tree);

  if ( no_root_item )
    {
      /* set root tree to subtree function with root item variable */
      root_item = GTK_WIDGET(root_tree);
    }
  else
    {
      /* create root tree item widget */
      root_item = gtk_tree_item_new_with_label("root item");
      gtk_tree_append(GTK_TREE(root_tree), root_item);
      gtk_widget_show(root_item);
     }
  create_subtree(root_item, -no_root_item, nb_item_max, recursion_level_max);

  box2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(box2), 5);
  gtk_widget_show(box2);

  button = gtk_button_new_with_label("Add Item");
  gtk_widget_set_sensitive(button, FALSE);
  gtk_signal_connect(GTK_OBJECT (button), "clicked",
		     (GtkSignalFunc) cb_add_new_item, 
		     (gpointer)root_tree);
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  tree_buttons->add_button = button;

  button = gtk_button_new_with_label("Remove Item(s)");
  gtk_widget_set_sensitive(button, FALSE);
  gtk_signal_connect(GTK_OBJECT (button), "clicked",
		     (GtkSignalFunc) cb_remove_item, 
		     (gpointer)root_tree);
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  tree_buttons->remove_button = button;

  button = gtk_button_new_with_label("Remove Subtree");
  gtk_widget_set_sensitive(button, FALSE);
  gtk_signal_connect(GTK_OBJECT (button), "clicked",
		     (GtkSignalFunc) cb_remove_subtree, 
		     (gpointer)root_tree);
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_widget_show(button);
  tree_buttons->subtree_button = button;

  /* create separator */
  separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(box1), separator, FALSE, FALSE, 0);
  gtk_widget_show(separator);

  /* create button box */
  box2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(box2), 5);
  gtk_widget_show(box2);

  button = gtk_button_new_with_label("Close");
  gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
  gtk_signal_connect_object(GTK_OBJECT (button), "clicked",
			    (GtkSignalFunc) gtk_widget_destroy, 
			    GTK_OBJECT(window));
  gtk_widget_show(button);

  gtk_widget_show(window);
}

static void
cb_create_tree(GtkWidget* w)
{
  guint selection_mode = GTK_SELECTION_SINGLE;
  guint view_line;
  guint draw_line;
  guint no_root_item;
  guint nb_item;
  guint recursion_level;

  /* get selection mode choice */
  if(GTK_TOGGLE_BUTTON(sTreeSampleSelection.single_button)->active)
    selection_mode = GTK_SELECTION_SINGLE;
  else
    if(GTK_TOGGLE_BUTTON(sTreeSampleSelection.browse_button)->active)
      selection_mode = GTK_SELECTION_BROWSE;
    else
      selection_mode = GTK_SELECTION_MULTIPLE;

  /* get options choice */
  draw_line = GTK_TOGGLE_BUTTON(sTreeSampleSelection.draw_line_button)->active;
  view_line = GTK_TOGGLE_BUTTON(sTreeSampleSelection.view_line_button)->active;
  no_root_item = GTK_TOGGLE_BUTTON(sTreeSampleSelection.no_root_item_button)->active;
    
  /* get levels */
  nb_item = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sTreeSampleSelection.nb_item_spinner));
  recursion_level = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sTreeSampleSelection.recursion_spinner));

  if (pow (nb_item, recursion_level) > 10000)
    {
      g_print ("%g total items? That will take a very long time. Try less\n",
	       pow (nb_item, recursion_level));
      return;
    }

  create_tree_sample(selection_mode, draw_line, view_line, no_root_item, nb_item, recursion_level);
}

void 
create_tree_mode_window(void)
{
  static GtkWidget* window;
  GtkWidget* box1;
  GtkWidget* box2;
  GtkWidget* box3;
  GtkWidget* box4;
  GtkWidget* box5;
  GtkWidget* button;
  GtkWidget* frame;
  GtkWidget* separator;
  GtkWidget* label;
  GtkWidget* spinner;
  GtkAdjustment *adj;

  if (!window)
    {
      /* create toplevel window  */
      window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title(GTK_WINDOW(window), "Set Tree Parameters");
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);
      box1 = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(window), box1);

      /* create upper box - selection box */
      box2 = gtk_vbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
      gtk_container_set_border_width(GTK_CONTAINER(box2), 5);

      box3 = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box2), box3, TRUE, TRUE, 0);

      /* create selection mode frame */
      frame = gtk_frame_new("Selection Mode");
      gtk_box_pack_start(GTK_BOX(box3), frame, TRUE, TRUE, 0);

      box4 = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(frame), box4);
      gtk_container_set_border_width(GTK_CONTAINER(box4), 5);

      /* create radio button */  
      button = gtk_radio_button_new_with_label(NULL, "SINGLE");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      sTreeSampleSelection.single_button = button;

      button = gtk_radio_button_new_with_label(gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
					       "BROWSE");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      sTreeSampleSelection.browse_button = button;

      button = gtk_radio_button_new_with_label(gtk_radio_button_get_group (GTK_RADIO_BUTTON (button)),
					       "MULTIPLE");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      sTreeSampleSelection.multiple_button = button;

      sTreeSampleSelection.selection_mode_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));

      /* create option mode frame */
      frame = gtk_frame_new("Options");
      gtk_box_pack_start(GTK_BOX(box3), frame, TRUE, TRUE, 0);

      box4 = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(frame), box4);
      gtk_container_set_border_width(GTK_CONTAINER(box4), 5);

      /* create check button */
      button = gtk_check_button_new_with_label("Draw line");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
      sTreeSampleSelection.draw_line_button = button;
  
      button = gtk_check_button_new_with_label("View Line mode");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
      sTreeSampleSelection.view_line_button = button;
  
      button = gtk_check_button_new_with_label("Without Root item");
      gtk_box_pack_start(GTK_BOX(box4), button, TRUE, TRUE, 0);
      sTreeSampleSelection.no_root_item_button = button;

      /* create recursion parameter */
      frame = gtk_frame_new("Size Parameters");
      gtk_box_pack_start(GTK_BOX(box2), frame, TRUE, TRUE, 0);

      box4 = gtk_hbox_new(FALSE, 5);
      gtk_container_add(GTK_CONTAINER(frame), box4);
      gtk_container_set_border_width(GTK_CONTAINER(box4), 5);

      /* create number of item spin button */
      box5 = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box4), box5, FALSE, FALSE, 0);

      label = gtk_label_new("Number of items : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (box5), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (DEFAULT_NUMBER_OF_ITEM, 1.0, 255.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (box5), spinner, FALSE, TRUE, 0);
      sTreeSampleSelection.nb_item_spinner = spinner;
  
      /* create recursion level spin button */
      box5 = gtk_hbox_new(FALSE, 5);
      gtk_box_pack_start(GTK_BOX(box4), box5, FALSE, FALSE, 0);

      label = gtk_label_new("Depth : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (box5), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (DEFAULT_RECURSION_LEVEL, 0.0, 255.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (box5), spinner, FALSE, TRUE, 0);
      sTreeSampleSelection.recursion_spinner = spinner;
  
      /* create horizontal separator */
      separator = gtk_hseparator_new();
      gtk_box_pack_start(GTK_BOX(box1), separator, FALSE, FALSE, 0);

      /* create bottom button box */
      box2 = gtk_hbox_new(TRUE, 10);
      gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, FALSE, 0);
      gtk_container_set_border_width(GTK_CONTAINER(box2), 5);

      button = gtk_button_new_with_label("Create Tree");
      gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
      gtk_signal_connect(GTK_OBJECT (button), "clicked",
			 (GtkSignalFunc) cb_create_tree, NULL);

      button = gtk_button_new_with_label("Close");
      gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                 GTK_OBJECT (window));
    }
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkHandleBox
 */

static void
handle_box_child_signal (GtkHandleBox *hb,
			 GtkWidget    *child,
			 const gchar  *action)
{
  printf ("%s: child <%s> %sed\n",
	  gtk_type_name (GTK_OBJECT_TYPE (hb)),
	  gtk_type_name (GTK_OBJECT_TYPE (child)),
	  action);
}

static void
create_handle_box (void)
{
  static GtkWidget* window = NULL;
  GtkWidget *handle_box;
  GtkWidget *handle_box2;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *toolbar;
  GtkWidget *label;
  GtkWidget *separator;
	
  if (!window)
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window),
			  "Handle Box Test");
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
    
    gtk_signal_connect (GTK_OBJECT (window), "destroy",
			GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			&window);
    
    gtk_container_set_border_width (GTK_CONTAINER (window), 20);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);

    label = gtk_label_new ("Above");
    gtk_container_add (GTK_CONTAINER (vbox), label);
    gtk_widget_show (label);

    separator = gtk_hseparator_new ();
    gtk_container_add (GTK_CONTAINER (vbox), separator);
    gtk_widget_show (separator);
    
    hbox = gtk_hbox_new (FALSE, 10);
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    gtk_widget_show (hbox);

    separator = gtk_hseparator_new ();
    gtk_container_add (GTK_CONTAINER (vbox), separator);
    gtk_widget_show (separator);

    label = gtk_label_new ("Below");
    gtk_container_add (GTK_CONTAINER (vbox), label);
    gtk_widget_show (label);

    handle_box = gtk_handle_box_new ();
    gtk_box_pack_start (GTK_BOX (hbox), handle_box, FALSE, FALSE, 0);
    gtk_signal_connect (GTK_OBJECT (handle_box),
			"child_attached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"attached");
    gtk_signal_connect (GTK_OBJECT (handle_box),
			"child_detached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"detached");
    gtk_widget_show (handle_box);

    toolbar = make_toolbar (window);
    
    gtk_container_add (GTK_CONTAINER (handle_box), toolbar);
    gtk_widget_show (toolbar);

    handle_box = gtk_handle_box_new ();
    gtk_box_pack_start (GTK_BOX (hbox), handle_box, FALSE, FALSE, 0);
    gtk_signal_connect (GTK_OBJECT (handle_box),
			"child_attached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"attached");
    gtk_signal_connect (GTK_OBJECT (handle_box),
			"child_detached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"detached");
    gtk_widget_show (handle_box);

    handle_box2 = gtk_handle_box_new ();
    gtk_container_add (GTK_CONTAINER (handle_box), handle_box2);
    gtk_signal_connect (GTK_OBJECT (handle_box2),
			"child_attached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"attached");
    gtk_signal_connect (GTK_OBJECT (handle_box2),
			"child_detached",
			GTK_SIGNAL_FUNC (handle_box_child_signal),
			"detached");
    gtk_widget_show (handle_box2);

    label = gtk_label_new ("Fooo!");
    gtk_container_add (GTK_CONTAINER (handle_box2), label);
    gtk_widget_show (label);
  }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Test for getting an image from a drawable
 */

struct GetImageData
{
  GtkWidget *src;
  GtkWidget *snap;
  GtkWidget *sw;
};

static void
take_snapshot (GtkWidget *button,
               gpointer data)
{
  struct GetImageData *gid = data;
  GdkRectangle visible;
  int width_fraction;
  int height_fraction;
  GdkGC *gc;
  GdkGC *black_gc;
  GdkColor color = { 0, 30000, 0, 0 };
  GdkRectangle target;
  GdkImage *shot;
  
  /* Do some begin_paint_rect on some random rects, draw some
   * distinctive stuff into those rects, then take the snapshot.
   * figure out whether any rects were overlapped and report to
   * user.
   */

  visible = gid->sw->allocation;

  visible.x = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (gid->sw))->value;
  visible.y = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (gid->sw))->value;
  
  width_fraction = visible.width / 4;
  height_fraction = visible.height / 4;

  gc = gdk_gc_new (gid->src->window);
  black_gc = gid->src->style->black_gc;
  
  gdk_gc_set_rgb_fg_color (gc, &color);

    
  target.x = visible.x + width_fraction;
  target.y = visible.y + height_fraction * 3;
  target.width = width_fraction;
  target.height = height_fraction / 2;
  
  gdk_window_begin_paint_rect (gid->src->window,
                               &target);

  gdk_draw_rectangle (gid->src->window,
                      gc,
                      TRUE,
                      target.x, target.y,
                      target.width, target.height);

  gdk_draw_rectangle (gid->src->window,
                      black_gc,
                      FALSE,
                      target.x + 10, target.y + 10,
                      target.width - 20, target.height - 20);
  
  target.x = visible.x + width_fraction;
  target.y = visible.y + height_fraction;
  target.width = width_fraction;
  target.height = height_fraction;
  
  gdk_window_begin_paint_rect (gid->src->window,
                               &target);

  gdk_draw_rectangle (gid->src->window,
                      gc,
                      TRUE,
                      target.x, target.y,
                      target.width, target.height);

  gdk_draw_rectangle (gid->src->window,
                      black_gc,
                      FALSE,
                      target.x + 10, target.y + 10,
                      target.width - 20, target.height - 20);
  
  target.x = visible.x + width_fraction * 3;
  target.y = visible.y + height_fraction;
  target.width = width_fraction / 2;
  target.height = height_fraction;
  
  gdk_window_begin_paint_rect (gid->src->window,
                               &target);

  gdk_draw_rectangle (gid->src->window,
                      gc,
                      TRUE,
                      target.x, target.y,
                      target.width, target.height);

  gdk_draw_rectangle (gid->src->window,
                      black_gc,
                      FALSE,
                      target.x + 10, target.y + 10,
                      target.width - 20, target.height - 20);
  
  target.x = visible.x + width_fraction * 2;
  target.y = visible.y + height_fraction * 2;
  target.width = width_fraction / 4;
  target.height = height_fraction / 4;
  
  gdk_window_begin_paint_rect (gid->src->window,
                               &target);

  gdk_draw_rectangle (gid->src->window,
                      gc,
                      TRUE,
                      target.x, target.y,
                      target.width, target.height);

  gdk_draw_rectangle (gid->src->window,
                      black_gc,
                      FALSE,
                      target.x + 10, target.y + 10,
                      target.width - 20, target.height - 20);  

  target.x += target.width / 2;
  target.y += target.width / 2;
  
  gdk_window_begin_paint_rect (gid->src->window,
                               &target);

  gdk_draw_rectangle (gid->src->window,
                      gc,
                      TRUE,
                      target.x, target.y,
                      target.width, target.height);

  gdk_draw_rectangle (gid->src->window,
                      black_gc,
                      FALSE,
                      target.x + 10, target.y + 10,
                      target.width - 20, target.height - 20);
  
  /* Screen shot area */

  target.x = visible.x + width_fraction * 1.5;
  target.y = visible.y + height_fraction * 1.5;
  target.width = width_fraction * 2;
  target.height = height_fraction * 2;  

  shot = gdk_drawable_get_image (gid->src->window,
                                 target.x, target.y,
                                 target.width, target.height);

  gtk_image_set_from_image (GTK_IMAGE (gid->snap),
                            shot, NULL);

  g_object_unref (G_OBJECT (shot));
  
  gdk_window_end_paint (gid->src->window);
  gdk_window_end_paint (gid->src->window);
  gdk_window_end_paint (gid->src->window);
  gdk_window_end_paint (gid->src->window);
  gdk_window_end_paint (gid->src->window);

  gdk_draw_rectangle (gid->src->window,
                      gid->src->style->black_gc,
                      FALSE,
                      target.x, target.y,
                      target.width, target.height);
  
  g_object_unref (G_OBJECT (gc));
}

static gint
image_source_expose (GtkWidget *da,
                     GdkEventExpose *event,
                     gpointer data)
{
  int x = event->area.x;
  GdkColor red = { 0, 65535, 0, 0 };
  GdkColor green = { 0, 0, 65535, 0 };
  GdkColor blue = { 0, 0, 0, 65535 };
  GdkGC *gc;

  gc = gdk_gc_new (event->window);
  
  while (x < (event->area.x + event->area.width))
    {
      switch (x % 7)
        {
        case 0:
        case 1:
        case 2:
          gdk_gc_set_rgb_fg_color (gc, &red);
          break;

        case 3:
        case 4:
        case 5:
          gdk_gc_set_rgb_fg_color (gc, &green);
          break;

        case 6:
        case 7:
        case 8:
          gdk_gc_set_rgb_fg_color (gc, &blue);
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      gdk_draw_line (event->window,
                     gc,
                     x, event->area.y,
                     x, event->area.y + event->area.height);
      
      ++x;
    }

  g_object_unref (G_OBJECT (gc));
  
  return TRUE;
}

static void
create_get_image (void)
{
  static GtkWidget *window = NULL;

  if (window)
    gtk_widget_destroy (window);
  else
    {
      GtkWidget *sw;
      GtkWidget *src;
      GtkWidget *snap;
      GtkWidget *vbox;
      GtkWidget *hbox;
      GtkWidget *button;
      struct GetImageData *gid;

      gid = g_new (struct GetImageData, 1);
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window),
                          "destroy",
                          GTK_SIGNAL_FUNC (gtk_widget_destroyed),
                          &window);

      gtk_object_set_data_full (GTK_OBJECT (window),
                                "testgtk-get-image-data",
                                gid,
                                g_free);
      
      vbox = gtk_vbox_new (FALSE, 0);
      
      gtk_container_add (GTK_CONTAINER (window), vbox);
      
      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);

      gid->sw = sw;

      gtk_widget_set_usize (sw, 400, 400);
      
      src = gtk_drawing_area_new ();
      gtk_widget_set_usize (src, 10000, 10000);

      gtk_signal_connect (GTK_OBJECT (src),
                          "expose_event",
                          GTK_SIGNAL_FUNC (image_source_expose),
                          gid);
      
      gid->src = src;
      
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
                                             src);
      
      gtk_box_pack_start (GTK_BOX (vbox),
                          sw, TRUE, TRUE, 0);                          


      hbox = gtk_hbox_new (FALSE, 3);

      snap = gtk_widget_new (GTK_TYPE_IMAGE, NULL);

      gid->snap = snap;

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_widget_set_usize (sw, 300, 300);
      
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), snap);

      gtk_box_pack_end (GTK_BOX (hbox), sw, FALSE, FALSE, 5);

      button = gtk_button_new_with_label ("Get image from drawable");

      gtk_signal_connect (GTK_OBJECT (button),
                          "clicked",
                          GTK_SIGNAL_FUNC (take_snapshot),
                          gid);
      
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

      gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
      
      gtk_widget_show_all (window);
    }
}

/* 
 * Label Demo
 */
static void
sensitivity_toggled (GtkWidget *toggle,
                     GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget,  GTK_TOGGLE_BUTTON (toggle)->active);  
}

static GtkWidget*
create_sensitivity_control (GtkWidget *widget)
{
  GtkWidget *button;

  button = gtk_toggle_button_new_with_label ("Sensitive");  

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                GTK_WIDGET_IS_SENSITIVE (widget));
  
  gtk_signal_connect (GTK_OBJECT (button),
                      "toggled",
                      GTK_SIGNAL_FUNC (sensitivity_toggled),
                      widget);
  
  gtk_widget_show_all (button);

  return button;
}

static void
set_selectable_recursive (GtkWidget *widget,
                          gboolean   setting)
{
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children;
      GList *tmp;
      
      children = gtk_container_children (GTK_CONTAINER (widget));
      tmp = children;
      while (tmp)
        {
          set_selectable_recursive (tmp->data, setting);

          tmp = tmp->next;
        }
      g_list_free (children);
    }
  else if (GTK_IS_LABEL (widget))
    {
      gtk_label_set_selectable (GTK_LABEL (widget), setting);
    }
}

static void
selectable_toggled (GtkWidget *toggle,
                    GtkWidget *widget)
{
  set_selectable_recursive (widget,
                            GTK_TOGGLE_BUTTON (toggle)->active);
}

static GtkWidget*
create_selectable_control (GtkWidget *widget)
{
  GtkWidget *button;

  button = gtk_toggle_button_new_with_label ("Selectable");  

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                FALSE);
  
  gtk_signal_connect (GTK_OBJECT (button),
                      "toggled",
                      GTK_SIGNAL_FUNC (selectable_toggled),
                      widget);
  
  gtk_widget_show_all (button);

  return button;
}

void create_labels (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *button;
  
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Label");

      vbox = gtk_vbox_new (FALSE, 5);
      
      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      button = create_sensitivity_control (hbox);

      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      button = create_selectable_control (hbox);

      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      
      vbox = gtk_vbox_new (FALSE, 5);
      
      gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (window), 5);

      frame = gtk_frame_new ("Normal Label");
      label = gtk_label_new ("This is a Normal label");
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Multi-line Label");
      label = gtk_label_new ("This is a Multi-line label.\nSecond line\nThird line");
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Left Justified Label");
      label = gtk_label_new ("This is a Left-Justified\nMulti-line label.\nThird      line");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Right Justified Label");
      label = gtk_label_new ("This is a Right-Justified\nMulti-line label.\nFourth line, (j/k)");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Internationalized Label");
      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
			    "French (Français) Bonjour, Salut\n"
			    "Korean (한글)   안녕하세요, 안녕하십니까\n"
			    "Russian (Русский) Здравствуйте!\n"
			    "Chinese (Simplified) <span lang=\"zh-cn\">元气	开发</span>\n"
			    "Chinese (Traditional) <span lang=\"zh-tw\">元氣	開發</span>\n"
			    "Japanese <span lang=\"ja\">元気	開発</span>");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Bidirection Label");
      label = gtk_label_new ("Arabic	السلام عليكم\n"
			     "Hebrew	שלום");
      gtk_widget_set_direction (label, GTK_TEXT_DIR_RTL);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
      frame = gtk_frame_new ("Line wrapped label");
      label = gtk_label_new ("This is an example of a line-wrapped label.  It should not be taking "\
			     "up the entire             "/* big space to test spacing */\
			     "width allocated to it, but automatically wraps the words to fit.  "\
			     "The time has come, for all good men, to come to the aid of their party.  "\
			     "The sixth sheik's six sheep's sick.\n"\
			     "     It supports multiple paragraphs correctly, and  correctly   adds "\
			     "many          extra  spaces. ");

      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Filled, wrapped label");
      label = gtk_label_new ("This is an example of a line-wrapped, filled label.  It should be taking "\
			     "up the entire              width allocated to it.  Here is a seneance to prove "\
			     "my point.  Here is another sentence. "\
			     "Here comes the sun, do de do de do.\n"\
			     "    This is a new paragraph.\n"\
			     "    This is another newer, longer, better paragraph.  It is coming to an end, "\
			     "unfortunately.");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_FILL);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Underlined label");
      label = gtk_label_new ("This label is underlined!\n"
			     "This one is underlined (こんにちは) in quite a funky fashion");
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_label_set_pattern (GTK_LABEL (label), "_________________________ _ _________ _ _____ _ __ __  ___ ____ _____");
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Markup label");
      label = gtk_label_new (NULL);

      /* There's also a gtk_label_set_markup() without accel if you
       * don't have an accelerator key
       */
      gtk_label_set_markup_with_mnemonic (GTK_LABEL (label),
					  "This <span foreground=\"blue\" background=\"orange\">label</span> has "
					  "<b>markup</b> _such as "
					  "<big><i>Big Italics</i></big>\n"
					  "<tt>Monospace font</tt>\n"
					  "<u>Underline!</u>\n"
					  "foo\n"
					  "<span foreground=\"green\" background=\"red\">Ugly colors</span>\n"
					  "and nothing on this line,\n"
					  "or this.\n"
					  "or this either\n"
					  "or even on this one\n"
					  "la <big>la <big>la <big>la <big>la</big></big></big></big>\n"
					  "but this _word is <span foreground=\"purple\"><big>purple</big></span>\n"
					  "<span underline=\"double\">We like <sup>superscript</sup> and <sub>subscript</sub> too</span>");

      g_assert (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)) == GDK_s);
      
      gtk_container_add (GTK_CONTAINER (frame), label);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
    }
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Reparent demo
 */

static void
reparent_label (GtkWidget *widget,
		GtkWidget *new_parent)
{
  GtkWidget *label;

  label = gtk_object_get_user_data (GTK_OBJECT (widget));

  gtk_widget_reparent (label, new_parent);
}

static void
set_parent_signal (GtkWidget *child,
		   GtkWidget *old_parent,
		   gpointer   func_data)
{
  g_print ("set_parent for \"%s\": new parent: \"%s\", old parent: \"%s\", data: %d\n",
	   gtk_type_name (GTK_OBJECT_TYPE (child)),
	   child->parent ? gtk_type_name (GTK_OBJECT_TYPE (child->parent)) : "NULL",
	   old_parent ? gtk_type_name (GTK_OBJECT_TYPE (old_parent)) : "NULL",
	   GPOINTER_TO_INT (func_data));
}

static void
create_reparent (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "reparent");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      label = gtk_label_new ("Hello World");

      frame = gtk_frame_new ("Frame 1");
      gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);

      box3 = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 5);
      gtk_container_add (GTK_CONTAINER (frame), box3);

      button = gtk_button_new_with_label ("switch");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(reparent_label),
			  box3);
      gtk_object_set_user_data (GTK_OBJECT (button), label);
      gtk_box_pack_start (GTK_BOX (box3), button, FALSE, TRUE, 0);

      gtk_box_pack_start (GTK_BOX (box3), label, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (label),
			  "parent_set",
			  GTK_SIGNAL_FUNC (set_parent_signal),
			  GINT_TO_POINTER (42));

      frame = gtk_frame_new ("Frame 2");
      gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);

      box3 = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 5);
      gtk_container_add (GTK_CONTAINER (frame), box3);

      button = gtk_button_new_with_label ("switch");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(reparent_label),
			  box3);
      gtk_object_set_user_data (GTK_OBJECT (button), label);
      gtk_box_pack_start (GTK_BOX (box3), button, FALSE, TRUE, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Saved Position
 */
gint upositionx = 0;
gint upositiony = 0;

static gint
uposition_configure (GtkWidget *window)
{
  GtkLabel *lx;
  GtkLabel *ly;
  gchar buffer[64];

  lx = gtk_object_get_data (GTK_OBJECT (window), "x");
  ly = gtk_object_get_data (GTK_OBJECT (window), "y");

  gdk_window_get_root_origin (window->window, &upositionx, &upositiony);
  sprintf (buffer, "%d", upositionx);
  gtk_label_set_text (lx, buffer);
  sprintf (buffer, "%d", upositiony);
  gtk_label_set_text (ly, buffer);

  return FALSE;
}

static void
uposition_stop_configure (GtkToggleButton *toggle,
			  GtkObject       *window)
{
  if (toggle->active)
    gtk_signal_handler_block_by_func (window, GTK_SIGNAL_FUNC (uposition_configure), NULL);
  else
    gtk_signal_handler_unblock_by_func (window, GTK_SIGNAL_FUNC (uposition_configure), NULL);
}

static void
create_saved_position (void)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *hbox;
      GtkWidget *main_vbox;
      GtkWidget *vbox;
      GtkWidget *x_label;
      GtkWidget *y_label;
      GtkWidget *button;
      GtkWidget *label;
      GtkWidget *any;

      window = g_object_connect (gtk_widget_new (GTK_TYPE_WINDOW,
						 "type", GTK_WINDOW_TOPLEVEL,
						 "x", upositionx,
						 "y", upositiony,
						 "title", "Saved Position",
						 NULL),
				 "signal::configure_event", uposition_configure, NULL,
				 NULL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      main_vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 0);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);

      vbox =
	gtk_widget_new (gtk_vbox_get_type (),
			"GtkBox::homogeneous", FALSE,
			"GtkBox::spacing", 5,
			"GtkContainer::border_width", 10,
			"GtkWidget::parent", main_vbox,
			"GtkWidget::visible", TRUE,
			"child", g_object_connect (gtk_widget_new (GTK_TYPE_TOGGLE_BUTTON,
								   "label", "Stop Events",
								   "active", FALSE,
								   "visible", TRUE,
								   NULL),
						   "signal::clicked", uposition_stop_configure, window,
						   NULL),
			NULL);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      label = gtk_label_new ("X Origin : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      x_label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), x_label, TRUE, TRUE, 0);
      gtk_object_set_data (GTK_OBJECT (window), "x", x_label);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      label = gtk_label_new ("Y Origin : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      y_label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), y_label, TRUE, TRUE, 0);
      gtk_object_set_data (GTK_OBJECT (window), "y", y_label);

      any =
	gtk_widget_new (gtk_hseparator_get_type (),
			"GtkWidget::visible", TRUE,
			NULL);
      gtk_box_pack_start (GTK_BOX (main_vbox), any, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      
      gtk_widget_show_all (window);
    }
  else
    gtk_widget_destroy (window);
}

/*
 * GtkPixmap
 */

static void
create_pixmap (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *pixmapwid;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkPixmap");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      gtk_widget_realize(window);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_button_new ();
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, FALSE, 0);

      pixmapwid = new_pixmap ("test.xpm", window->window, NULL);

      label = gtk_label_new ("Pixmap\ntest");
      box3 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 2);
      gtk_container_add (GTK_CONTAINER (box3), pixmapwid);
      gtk_container_add (GTK_CONTAINER (box3), label);
      gtk_container_add (GTK_CONTAINER (button), box3);

      button = gtk_button_new ();
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, FALSE, 0);
      
      pixmapwid = new_pixmap ("test.xpm", window->window, NULL);

      label = gtk_label_new ("Pixmap\ntest");
      box3 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (box3), 2);
      gtk_container_add (GTK_CONTAINER (box3), pixmapwid);
      gtk_container_add (GTK_CONTAINER (box3), label);
      gtk_container_add (GTK_CONTAINER (button), box3);

      gtk_widget_set_sensitive (button, FALSE);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

static void
tips_query_widget_entered (GtkTipsQuery   *tips_query,
			   GtkWidget      *widget,
			   const gchar    *tip_text,
			   const gchar    *tip_private,
			   GtkWidget	  *toggle)
{
  if (GTK_TOGGLE_BUTTON (toggle)->active)
    {
      gtk_label_set_text (GTK_LABEL (tips_query), tip_text ? "There is a Tip!" : "There is no Tip!");
      /* don't let GtkTipsQuery reset its label */
      gtk_signal_emit_stop_by_name (GTK_OBJECT (tips_query), "widget_entered");
    }
}

static gint
tips_query_widget_selected (GtkWidget      *tips_query,
			    GtkWidget      *widget,
			    const gchar    *tip_text,
			    const gchar    *tip_private,
			    GdkEventButton *event,
			    gpointer        func_data)
{
  if (widget)
    g_print ("Help \"%s\" requested for <%s>\n",
	     tip_private ? tip_private : "None",
	     gtk_type_name (GTK_OBJECT_TYPE (widget)));
  return TRUE;
}

static void
create_tooltips (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *toggle;
  GtkWidget *frame;
  GtkWidget *tips_query;
  GtkWidget *separator;
  GtkTooltips *tooltips;

  if (!window)
    {
      window =
	gtk_widget_new (gtk_window_get_type (),
			"GtkWindow::type", GTK_WINDOW_TOPLEVEL,
			"GtkContainer::border_width", 0,
			"GtkWindow::title", "Tooltips",
			"GtkWindow::allow_shrink", TRUE,
			"GtkWindow::allow_grow", FALSE,
			"GtkWindow::auto_shrink", TRUE,
			"GtkWidget::width", 200,
			NULL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC (destroy_tooltips),
                          &window);

      tooltips=gtk_tooltips_new();
      g_object_ref (tooltips);
      gtk_object_sink (GTK_OBJECT (tooltips));
      gtk_object_set_data (GTK_OBJECT (window), "tooltips", tooltips);
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      button = gtk_toggle_button_new_with_label ("button1");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_tooltips_set_tip (tooltips,button,"This is button 1", "ContextHelp/buttons/1");

      button = gtk_toggle_button_new_with_label ("button2");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      gtk_tooltips_set_tip (tooltips,
			    button,
			    "This is button 2. This is also a really long tooltip which probably won't fit on a single line and will therefore need to be wrapped. Hopefully the wrapping will work correctly.",
			    "ContextHelp/buttons/2_long");

      toggle = gtk_toggle_button_new_with_label ("Override TipsQuery Label");
      gtk_box_pack_start (GTK_BOX (box2), toggle, TRUE, TRUE, 0);

      gtk_tooltips_set_tip (tooltips, toggle, "Toggle TipsQuery view.", "Hi msw! ;)");

      box3 =
	gtk_widget_new (gtk_vbox_get_type (),
			"homogeneous", FALSE,
			"spacing", 5,
			"border_width", 5,
			"visible", TRUE,
			NULL);

      tips_query = gtk_tips_query_new ();

      button =
	gtk_widget_new (gtk_button_get_type (),
			"label", "[?]",
			"visible", TRUE,
			"parent", box3,
			NULL);
      g_object_connect (G_OBJECT (button),
			"swapped_signal::clicked", gtk_tips_query_start_query, tips_query,
			NULL);
      gtk_box_set_child_packing (GTK_BOX (box3), button, FALSE, FALSE, 0, GTK_PACK_START);
      gtk_tooltips_set_tip (tooltips,
			    button,
			    "Start the Tooltips Inspector",
			    "ContextHelp/buttons/?");
      
      
      g_object_set (g_object_connect (tips_query,
				      "signal::widget_entered", tips_query_widget_entered, toggle,
				      "signal::widget_selected", tips_query_widget_selected, NULL,
				      NULL),
		    "visible", TRUE,
		    "parent", box3,
		    "caller", button,
		    NULL);
      
      frame = gtk_widget_new (gtk_frame_get_type (),
			      "label", "ToolTips Inspector",
			      "label_xalign", (double) 0.5,
			      "border_width", 0,
			      "visible", TRUE,
			      "parent", box2,
			      "child", box3,
			      NULL);
      gtk_box_set_child_packing (GTK_BOX (box2), frame, TRUE, TRUE, 10, GTK_PACK_START);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                                 GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);

      gtk_tooltips_set_tip (tooltips, button, "Push this button to close window", "ContextHelp/buttons/Close");
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkImage
 */

static void
pack_image (GtkWidget *box,
            const gchar *text,
            GtkWidget *image)
{
  gtk_box_pack_start (GTK_BOX (box),
                      gtk_label_new (text),
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (box),
                      image,
                      TRUE, TRUE, 0);  
}

static void
create_image (void)
{
  static GtkWidget *window = NULL;
  
  if (window == NULL)
    {
      GtkWidget *vbox;
      GdkPixmap *pixmap;
      GdkBitmap *mask;
        
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      /* this is bogus for testing drawing when allocation < request,
       * don't copy into real code
       */
      gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      vbox = gtk_vbox_new (FALSE, 5);

      gtk_container_add (GTK_CONTAINER (window), vbox);

      pack_image (vbox, "Stock Warning Dialog",
                  gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
                                            GTK_ICON_SIZE_DIALOG));

      pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL,
                                                      gtk_widget_get_colormap (window),
                                                      &mask,
                                                      NULL,
                                                      openfile);
      
      pack_image (vbox, "Pixmap",
                  gtk_image_new_from_pixmap (pixmap, mask));
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}
     
/*
 * Menu demo
 */

static GtkWidget*
create_menu (gint depth, gint length, gboolean tearoff)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  GtkWidget *image;
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

  image = gtk_image_new_from_stock (GTK_STOCK_OPEN,
                                    GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  menuitem = gtk_image_menu_item_new_with_label ("Image item");
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);
  
  for (i = 0, j = 1; i < length; i++, j++)
    {
      sprintf (buf, "item %2d - %d", depth, j);

      menuitem = gtk_radio_menu_item_new_with_label (group, buf);
      group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menuitem));

      if (depth % 2)
	gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menuitem), TRUE);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
      gtk_widget_show (menuitem);
      if (i == 3)
	gtk_widget_set_sensitive (menuitem, FALSE);

      if (i == 5)
        gtk_check_menu_item_set_inconsistent (GTK_CHECK_MENU_ITEM (menuitem),
                                              TRUE);

      if (i < 5)
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (depth - 1, 5,  TRUE));
    }

  return menu;
}

static void
create_menus (void)
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
      GtkWidget *image;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete-event",
			  GTK_SIGNAL_FUNC (gtk_true),
			  NULL);
      
      accel_group = gtk_accel_group_new ();
      gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

      gtk_window_set_title (GTK_WINDOW (window), "menus");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);
      
      menubar = gtk_menu_bar_new ();
      gtk_box_pack_start (GTK_BOX (box1), menubar, FALSE, TRUE, 0);
      gtk_widget_show (menubar);
      
      menu = create_menu (2, 50, TRUE);
      
      menuitem = gtk_menu_item_new_with_label ("test\nline2");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      menuitem = gtk_menu_item_new_with_label ("foo");
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (3, 5, TRUE));
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);

      image = gtk_image_new_from_stock (GTK_STOCK_HELP,
                                        GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      menuitem = gtk_image_menu_item_new_with_label ("Help");
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), create_menu (4, 5, TRUE));
      gtk_menu_item_set_right_justified (GTK_MENU_ITEM (menuitem), TRUE);
      gtk_menu_bar_append (GTK_MENU_BAR (menubar), menuitem);
      gtk_widget_show (menuitem);
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      menu = create_menu (1, 5, FALSE);
      gtk_menu_set_accel_group (GTK_MENU (menu), accel_group);

      menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_NEW, accel_group);
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
				  GTK_ACCEL_VISIBLE);
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
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
gtk_ifactory_cb (gpointer             callback_data,
		 guint                callback_action,
		 GtkWidget           *widget)
{
  g_message ("ItemFactory: activated \"%s\"", gtk_item_factory_path_from_widget (widget));
}

/* This file was automatically generated by the make-inline-pixbuf program.
 * It contains inline RGB image data.
 */
static const guchar apple[] =
{
  /* File magic (1197763408) */
  0x47, 0x64, 0x6b, 0x50,
  /* Format of following stuff (0) */
  0x00, 0x00, 0x00, 0x00,
  /* Rowstride (64) */
  0x00, 0x00, 0x00, 0x40,
  /* Width (16) */
  0x00, 0x00, 0x00, 0x10,
  /* Height (16) */
  0x00, 0x00, 0x00, 0x10,
  /* Has an alpha channel (TRUE) */
  0x01,
  /* Colorspace (0 == RGB, no other options implemented) (0) */
  0x00, 0x00, 0x00, 0x00,
  /* Number of channels (4) */
  0x00, 0x00, 0x00, 0x04,
  /* Bits per sample (8) */
  0x00, 0x00, 0x00, 0x08,
  /* Image data */
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x16, 0x14, 0x0f, 0x04,
  0x00, 0x00, 0x00, 0x01,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x61, 0x6d, 0x5b, 0x2b,  0x6e, 0x7c, 0x61, 0xd9,
  0x71, 0x80, 0x63, 0xd7,  0x5f, 0x6b, 0x5b, 0x35,  0x00, 0x00, 0x00, 0x00,
  0x3a, 0x35, 0x28, 0x8f,  0x00, 0x00, 0x00, 0x32,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x60, 0x6c, 0x5c, 0x07,  0x6d, 0x7b, 0x61, 0xd8,
  0x75, 0x84, 0x65, 0xf6,  0x76, 0x86, 0x66, 0xf7,  0x6a, 0x77, 0x60, 0xec,
  0x5e, 0x6a, 0x58, 0x47,  0x1c, 0x1a, 0x13, 0xa2,  0x4b, 0x47, 0x30, 0x07,
  0x55, 0x4e, 0x33, 0x21,  0x48, 0x3e, 0x2a, 0x08,  0xd0, 0xb8, 0x84, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x69, 0x76, 0x5f, 0x74,
  0x75, 0x84, 0x65, 0xf3,  0x67, 0x75, 0x5e, 0xc4,  0x69, 0x62, 0x55, 0x75,
  0x94, 0x50, 0x50, 0x69,  0x75, 0x5c, 0x52, 0xb2,  0x69, 0x38, 0x34, 0xa2,
  0xa7, 0x5b, 0x53, 0xea,  0xa3, 0x52, 0x4f, 0xff,  0x90, 0x47, 0x42, 0xfa,
  0x76, 0x44, 0x36, 0xb9,  0x59, 0x38, 0x29, 0x3c,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x5f, 0x6b, 0x5a, 0x09,
  0x69, 0x76, 0x5e, 0xb0,  0x5f, 0x6b, 0x59, 0x57,  0x9a, 0x4b, 0x4d, 0x5b,
  0xb8, 0x5f, 0x63, 0xfa,  0xcc, 0x7d, 0x7e, 0xff,  0xc5, 0x69, 0x68, 0xff,
  0xc7, 0x6b, 0x67, 0xff,  0xc5, 0x6f, 0x67, 0xff,  0xba, 0x5e, 0x5a, 0xff,
  0xb1, 0x4d, 0x4d, 0xff,  0x92, 0x4b, 0x42, 0xff,  0x6a, 0x3e, 0x30, 0xfc,
  0x5c, 0x3b, 0x27, 0x6d,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x5d, 0x69, 0x57, 0x09,  0x5d, 0x69, 0x57, 0x09,  0x92, 0x47, 0x46, 0x1e,
  0xba, 0x65, 0x64, 0xf4,  0xe7, 0xbf, 0xc0, 0xff,  0xdf, 0xa5, 0xa3, 0xff,
  0xd4, 0x84, 0x81, 0xff,  0xd1, 0x7c, 0x76, 0xff,  0xc9, 0x78, 0x6d, 0xff,
  0xbb, 0x6a, 0x5d, 0xff,  0xb3, 0x5a, 0x52, 0xff,  0x9f, 0x4b, 0x47, 0xff,
  0x78, 0x45, 0x35, 0xff,  0x5f, 0x3c, 0x28, 0xfa,  0x53, 0x5a, 0x38, 0x24,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0xa1, 0x54, 0x4d, 0x8c,  0xcf, 0x8e, 0x89, 0xff,  0xe3, 0xb1, 0xae, 0xff,
  0xd8, 0x94, 0x8e, 0xff,  0xd3, 0x8a, 0x82, 0xff,  0xcf, 0x80, 0x76, 0xff,
  0xc4, 0x75, 0x67, 0xff,  0xb7, 0x6c, 0x5c, 0xff,  0xab, 0x5e, 0x51, 0xff,
  0x9c, 0x4c, 0x46, 0xff,  0x7e, 0x4a, 0x3a, 0xff,  0x5c, 0x3c, 0x26, 0xff,
  0x58, 0x3d, 0x28, 0x55,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0xa2, 0x59, 0x4f, 0xc3,  0xcd, 0x8e, 0x88, 0xff,
  0xd3, 0x93, 0x8c, 0xff,  0xd0, 0x8c, 0x83, 0xff,  0xcc, 0x84, 0x79, 0xff,
  0xc7, 0x7c, 0x6e, 0xff,  0xbc, 0x73, 0x61, 0xff,  0xb1, 0x6b, 0x59, 0xff,
  0xa3, 0x5f, 0x4f, 0xff,  0x93, 0x50, 0x44, 0xff,  0x78, 0x48, 0x35, 0xff,
  0x59, 0x3b, 0x25, 0xff,  0x4f, 0x3d, 0x28, 0x4f,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x9b, 0x5b, 0x4d, 0xbc,
  0xbd, 0x7e, 0x72, 0xff,  0xc6, 0x86, 0x7a, 0xff,  0xc5, 0x7f, 0x72, 0xff,
  0xc2, 0x7b, 0x6c, 0xff,  0xbf, 0x77, 0x63, 0xff,  0xb7, 0x72, 0x5b, 0xff,
  0xa9, 0x6b, 0x53, 0xff,  0x9a, 0x60, 0x4b, 0xff,  0x8b, 0x56, 0x41, 0xff,
  0x6a, 0x44, 0x2e, 0xff,  0x53, 0x38, 0x21, 0xfd,  0x42, 0x4b, 0x2e, 0x1a,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x8e, 0x57, 0x48, 0x6e,  0xa6, 0x6b, 0x5a, 0xff,  0xb3, 0x74, 0x62, 0xff,
  0xb8, 0x75, 0x61, 0xff,  0xba, 0x76, 0x61, 0xff,  0xb7, 0x74, 0x5c, 0xff,
  0xae, 0x6e, 0x54, 0xff,  0x9f, 0x67, 0x4c, 0xff,  0x90, 0x5d, 0x43, 0xff,
  0x79, 0x4d, 0x38, 0xff,  0x5c, 0x3d, 0x25, 0xff,  0x50, 0x39, 0x23, 0xb8,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x78, 0x52, 0x43, 0x07,  0x92, 0x5c, 0x47, 0xdc,
  0x9e, 0x64, 0x4e, 0xff,  0xa8, 0x6b, 0x52, 0xff,  0xaa, 0x6d, 0x53, 0xff,
  0xa7, 0x6d, 0x50, 0xff,  0x9c, 0x67, 0x4a, 0xff,  0x8e, 0x5d, 0x41, 0xff,
  0x7d, 0x54, 0x3a, 0xff,  0x6a, 0x4b, 0x32, 0xff,  0x51, 0x39, 0x23, 0xff,
  0x28, 0x20, 0x12, 0x77,  0x00, 0x00, 0x00, 0x12,  0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x6f, 0x4a, 0x37, 0x2a,  0x81, 0x54, 0x3d, 0xec,  0x8b, 0x5a, 0x41, 0xff,
  0x8b, 0x5a, 0x3f, 0xff,  0x85, 0x56, 0x3c, 0xff,  0x7d, 0x52, 0x38, 0xff,
  0x77, 0x51, 0x33, 0xff,  0x6f, 0x4e, 0x34, 0xff,  0x5f, 0x45, 0x2c, 0xff,
  0x2e, 0x21, 0x14, 0xff,  0x00, 0x00, 0x00, 0xf8,  0x00, 0x00, 0x00, 0x92,
  0x00, 0x00, 0x00, 0x0e,  0x00, 0x00, 0x00, 0x04,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x14,  0x11, 0x0b, 0x08, 0xb4,
  0x50, 0x37, 0x25, 0xfe,  0x6d, 0x49, 0x2f, 0xff,  0x52, 0x37, 0x22, 0xff,
  0x50, 0x37, 0x21, 0xff,  0x66, 0x45, 0x2b, 0xff,  0x60, 0x46, 0x2c, 0xff,
  0x2d, 0x22, 0x16, 0xff,  0x00, 0x00, 0x00, 0xfe,  0x00, 0x00, 0x00, 0xd2,
  0x00, 0x00, 0x00, 0x63,  0x00, 0x00, 0x00, 0x07,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x22,  0x00, 0x00, 0x00, 0x64,  0x09, 0x0a, 0x07, 0xa4,
  0x00, 0x00, 0x00, 0xbd,  0x00, 0x00, 0x00, 0xbe,  0x00, 0x00, 0x00, 0xc4,
  0x00, 0x00, 0x00, 0xb8,  0x00, 0x00, 0x00, 0x9d,  0x00, 0x00, 0x00, 0x6c,
  0x00, 0x00, 0x00, 0x2c,  0x00, 0x00, 0x00, 0x07,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x05,  0x00, 0x00, 0x00, 0x0b,  0x00, 0x00, 0x00, 0x0d,
  0x00, 0x00, 0x00, 0x0b,  0x00, 0x00, 0x00, 0x08,  0x00, 0x00, 0x00, 0x06,
  0x00, 0x00, 0x00, 0x02,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};


static void
dump_accels (gpointer             callback_data,
	     guint                callback_action,
	     GtkWidget           *widget)
{
  gtk_accel_map_save_fd (1 /* stdout */);
}
    
static GtkItemFactoryEntry menu_items[] =
{
  { "/_File",                  NULL,         0,                     0, "<Branch>" },
  { "/File/tearoff1",          NULL,         gtk_ifactory_cb,       0, "<Tearoff>" },
  { "/File/_New",              NULL,         gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_NEW },
  { "/File/_Open",             NULL,         gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_OPEN },
  { "/File/_Save",             NULL,         gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/Save _As...",       "<control>A", gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/_Dump \"_Accels\"",  NULL,        dump_accels,           0 },
  { "/File/\\/Test__Escaping/And\\/\n\tWei\\\\rdly",
                                NULL,        gtk_ifactory_cb,       0 },
  { "/File/sep1",        NULL,               gtk_ifactory_cb,       0, "<Separator>" },
  { "/File/_Quit",       NULL,               gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_QUIT },

  { "/_Preferences",     		NULL, 0,               0, "<Branch>" },
  { "/_Preferences/_Color", 		NULL, 0,               0, "<Branch>" },
  { "/_Preferences/Color/_Red",      	NULL, gtk_ifactory_cb, 0, "<RadioItem>" },
  { "/_Preferences/Color/_Green",   	NULL, gtk_ifactory_cb, 0, "/Preferences/Color/Red" },
  { "/_Preferences/Color/_Blue",        NULL, gtk_ifactory_cb, 0, "/Preferences/Color/Red" },
  { "/_Preferences/_Shape", 		NULL, 0,               0, "<Branch>" },
  { "/_Preferences/Shape/_Square",      NULL, gtk_ifactory_cb, 0, "<RadioItem>" },
  { "/_Preferences/Shape/_Rectangle",   NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Square" },
  { "/_Preferences/Shape/_Oval",        NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Rectangle" },
  { "/_Preferences/Shape/_Rectangle",   NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Square" },
  { "/_Preferences/Shape/_Oval",        NULL, gtk_ifactory_cb, 0, "/Preferences/Shape/Rectangle" },
  { "/_Preferences/Shape/_Image",       NULL, gtk_ifactory_cb, 0, "<ImageItem>", apple },

  /* For testing deletion of menus */
  { "/_Preferences/Should_NotAppear",          NULL, 0,               0, "<Branch>" },
  { "/Preferences/ShouldNotAppear/SubItem1",   NULL, gtk_ifactory_cb, 0 },
  { "/Preferences/ShouldNotAppear/SubItem2",   NULL, gtk_ifactory_cb, 0 },

  { "/_Help",            NULL,         0,                     0, "<LastBranch>" },
  { "/Help/_Help",       NULL,         gtk_ifactory_cb,       0, "<StockItem>", GTK_STOCK_HELP},
  { "/Help/_About",      NULL,         gtk_ifactory_cb,       0 },
};


static int nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

static void
create_item_factory (void)
{
  static GtkWidget *window = NULL;
  
  if (!window)
    {
      GtkWidget *box1;
      GtkWidget *box2;
      GtkWidget *separator;
      GtkWidget *label;
      GtkWidget *button;
      GtkAccelGroup *accel_group;
      GtkItemFactory *item_factory;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);
      gtk_signal_connect (GTK_OBJECT (window), "delete-event",
			  GTK_SIGNAL_FUNC (gtk_true),
			  NULL);
      
      accel_group = gtk_accel_group_new ();
      item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
      gtk_object_set_data_full (GTK_OBJECT (window),
				"<main>",
				item_factory,
				(GtkDestroyNotify) gtk_object_unref);
      gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
      gtk_window_set_title (GTK_WINDOW (window), "Item Factory");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

      /* preselect /Preferences/Shape/Oval over the other radios
       */
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (item_factory,
										      "/Preferences/Shape/Oval")),
				      TRUE);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      
      gtk_box_pack_start (GTK_BOX (box1),
			  gtk_item_factory_get_widget (item_factory, "<main>"),
			  FALSE, FALSE, 0);

      label = gtk_label_new ("Type\n<alt>\nto start");
      gtk_widget_set_usize (label, 200, 200);
      gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
      gtk_box_pack_start (GTK_BOX (box1), label, TRUE, TRUE, 0);


      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);

      gtk_item_factory_delete_item (item_factory, "/Preferences/ShouldNotAppear");
      
      gtk_widget_show_all (window);
    }
  else
    gtk_widget_destroy (window);
}

/*
 create_modal_window
 */

static gboolean
cmw_destroy_cb(GtkWidget *widget)
{
  /* This is needed to get out of gtk_main */
  gtk_main_quit ();

  return FALSE;
}

static void
cmw_color (GtkWidget *widget, GtkWidget *parent)
{
    GtkWidget *csd;

    csd=gtk_color_selection_dialog_new ("This is a modal color selection dialog");

    gtk_color_selection_set_has_palette (GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (csd)->colorsel),
                                         TRUE);
    
    /* Set as modal */
    gtk_window_set_modal (GTK_WINDOW(csd),TRUE);

    /* And mark it as a transient dialog */
    gtk_window_set_transient_for (GTK_WINDOW (csd), GTK_WINDOW (parent));
    
    gtk_signal_connect (GTK_OBJECT(csd), "destroy",
		        GTK_SIGNAL_FUNC(cmw_destroy_cb),NULL);

    gtk_signal_connect_object (GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(csd)->ok_button),
                               "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
                               GTK_OBJECT (csd));
    gtk_signal_connect_object (GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(csd)->cancel_button),
                               "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
                               GTK_OBJECT (csd));
    
    /* wait until destroy calls gtk_main_quit */
    gtk_widget_show (csd);    
    gtk_main ();
}

static void
cmw_file (GtkWidget *widget, GtkWidget *parent)
{
    GtkWidget *fs;

    fs = gtk_file_selection_new("This is a modal file selection dialog");

    /* Set as modal */
    gtk_window_set_modal (GTK_WINDOW(fs),TRUE);

    /* And mark it as a transient dialog */
    gtk_window_set_transient_for (GTK_WINDOW (fs), GTK_WINDOW (parent));

    gtk_signal_connect (GTK_OBJECT(fs), "destroy",
                        GTK_SIGNAL_FUNC(cmw_destroy_cb),NULL);

    gtk_signal_connect_object (GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
                               "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                               GTK_OBJECT (fs));
    gtk_signal_connect_object (GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
                               "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                               GTK_OBJECT (fs));
    
    /* wait until destroy calls gtk_main_quit */
    gtk_widget_show (fs);
    
    gtk_main();
}


static void
create_modal_window (void)
{
  GtkWidget *window = NULL;
  GtkWidget *box1,*box2;
  GtkWidget *frame1;
  GtkWidget *btnColor,*btnFile,*btnClose;

  /* Create modal window (Here you can use any window descendent )*/
  window=gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW(window),"This window is modal");

  /* Set window as modal */
  gtk_window_set_modal (GTK_WINDOW(window),TRUE);

  /* Create widgets */
  box1 = gtk_vbox_new (FALSE,5);
   frame1 = gtk_frame_new ("Standard dialogs in modal form");
    box2 = gtk_vbox_new (TRUE,5);
     btnColor = gtk_button_new_with_label ("Color");
     btnFile = gtk_button_new_with_label ("File Selection");
     btnClose = gtk_button_new_with_label ("Close");

  /* Init widgets */
  gtk_container_set_border_width (GTK_CONTAINER(box1),3);
  gtk_container_set_border_width (GTK_CONTAINER(box2),3);
    
  /* Pack widgets */
  gtk_container_add (GTK_CONTAINER (window), box1);
  gtk_box_pack_start (GTK_BOX (box1), frame1, TRUE, TRUE, 4);
  gtk_container_add (GTK_CONTAINER (frame1), box2);
  gtk_box_pack_start (GTK_BOX (box2), btnColor, FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (box2), btnFile, FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (box1), gtk_hseparator_new (), FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (box1), btnClose, FALSE, FALSE, 4);
   
  /* connect signals */
  gtk_signal_connect_object (GTK_OBJECT (btnClose), "clicked",
                             GTK_SIGNAL_FUNC (gtk_widget_destroy),
                             GTK_OBJECT (window));

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
                      GTK_SIGNAL_FUNC (cmw_destroy_cb),NULL);
  
  gtk_signal_connect (GTK_OBJECT (btnColor), "clicked",
                      GTK_SIGNAL_FUNC (cmw_color),window);
  gtk_signal_connect (GTK_OBJECT (btnFile), "clicked",
                      GTK_SIGNAL_FUNC (cmw_file),window);

  /* Show widgets */
  gtk_widget_show_all (window);

  /* wait until dialog get destroyed */
  gtk_main();
}

/*
 * GtkMessageDialog
 */

static void
make_message_dialog (GtkWidget **dialog,
                     GtkMessageType  type,
                     GtkButtonsType  buttons)
{
  if (*dialog)
    {
      gtk_widget_destroy (*dialog);

      return;
    }

  *dialog = gtk_message_dialog_new (NULL, 0, type, buttons,
                                    "This is a message dialog; it can wrap long lines. This is a long line. La la la. Look this line is wrapped. Blah blah blah blah blah blah. (Note: testgtk has a nonstandard gtkrc that changes some of the message dialog icons.)");

  gtk_signal_connect_object (GTK_OBJECT (*dialog),
                             "response",
                             GTK_SIGNAL_FUNC (gtk_widget_destroy),
                             GTK_OBJECT (*dialog));
  
  gtk_signal_connect (GTK_OBJECT (*dialog),
                      "destroy",
                      GTK_SIGNAL_FUNC (gtk_widget_destroyed),
                      dialog);
  
  gtk_widget_show (*dialog);
}

static void
create_message_dialog (void)
{
  static GtkWidget *info = NULL;
  static GtkWidget *warning = NULL;
  static GtkWidget *error = NULL;
  static GtkWidget *question = NULL;

  make_message_dialog (&info, GTK_MESSAGE_INFO, GTK_BUTTONS_OK);
  make_message_dialog (&warning, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE);
  make_message_dialog (&error, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK_CANCEL);
  make_message_dialog (&question, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO);
}

/*
 * GtkScrolledWindow
 */

static GtkWidget *sw_parent = NULL;
static GtkWidget *sw_float_parent;
static guint sw_destroyed_handler = 0;

static gboolean
scrolled_windows_delete_cb (GtkWidget *widget, GdkEventAny *event, GtkWidget *scrollwin)
{
  gtk_widget_reparent (scrollwin, sw_parent);
  
  gtk_signal_disconnect (GTK_OBJECT (sw_parent), sw_destroyed_handler);
  sw_float_parent = NULL;
  sw_parent = NULL;
  sw_destroyed_handler = 0;

  return FALSE;
}

static void
scrolled_windows_destroy_cb (GtkWidget *widget, GtkWidget *scrollwin)
{
  gtk_widget_destroy (sw_float_parent);

  sw_float_parent = NULL;
  sw_parent = NULL;
  sw_destroyed_handler = 0;
}

static void
scrolled_windows_remove (GtkWidget *widget, GtkWidget *scrollwin)
{
  if (sw_parent)
    {
      gtk_widget_reparent (scrollwin, sw_parent);
      gtk_widget_destroy (sw_float_parent);

      gtk_signal_disconnect (GTK_OBJECT (sw_parent), sw_destroyed_handler);
      sw_float_parent = NULL;
      sw_parent = NULL;
      sw_destroyed_handler = 0;
    }
  else
    {
      sw_parent = scrollwin->parent;
      sw_float_parent = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_default_size (GTK_WINDOW (sw_float_parent), 200, 200);
      
      gtk_widget_reparent (scrollwin, sw_float_parent);
      gtk_widget_show (sw_float_parent);

      sw_destroyed_handler =
	gtk_signal_connect (GTK_OBJECT (sw_parent), "destroy",
			    GTK_SIGNAL_FUNC (scrolled_windows_destroy_cb), scrollwin);
      gtk_signal_connect (GTK_OBJECT (sw_float_parent), "delete_event",
			  GTK_SIGNAL_FUNC (scrolled_windows_delete_cb), scrollwin);
    }
}

static void
create_scrolled_windows (void)
{
  static GtkWidget *window;
  GtkWidget *scrolled_window;
  GtkWidget *table;
  GtkWidget *button;
  char buffer[32];
  int i, j;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "dialog");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);


      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 10);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  scrolled_window, TRUE, TRUE, 0);
      gtk_widget_show (scrolled_window);

      table = gtk_table_new (20, 20, FALSE);
      gtk_table_set_row_spacings (GTK_TABLE (table), 10);
      gtk_table_set_col_spacings (GTK_TABLE (table), 10);
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), table);
      gtk_container_set_focus_hadjustment (GTK_CONTAINER (table),
					   gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
      gtk_container_set_focus_vadjustment (GTK_CONTAINER (table),
					   gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
      gtk_widget_show (table);

      for (i = 0; i < 20; i++)
	for (j = 0; j < 20; j++)
	  {
	    sprintf (buffer, "button (%d,%d)\n", i, j);
	    button = gtk_toggle_button_new_with_label (buffer);
	    gtk_table_attach_defaults (GTK_TABLE (table), button,
				       i, i+1, j, j+1);
	    gtk_widget_show (button);
	  }


      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Reparent Out");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(scrolled_windows_remove),
			  scrolled_window);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkEntry
 */

static void
entry_toggle_frame (GtkWidget *checkbutton,
                    GtkWidget *entry)
{
   gtk_entry_set_has_frame (GTK_ENTRY(entry),
                            GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static void
entry_toggle_sensitive (GtkWidget *checkbutton,
			GtkWidget *entry)
{
   gtk_widget_set_sensitive (entry, GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static void
entry_props_clicked (GtkWidget *button,
		     GObject   *entry)
{
  GtkWidget *window = create_prop_editor (entry, 0);

  gtk_window_set_title (GTK_WINDOW (window), "Entry Properties");
}

static void
create_entry (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *hbox;
  GtkWidget *has_frame_check;
  GtkWidget *sensitive_check;
  GtkWidget *entry, *cb;
  GtkWidget *button;
  GtkWidget *separator;
  GList *cbitems = NULL;

  if (!window)
    {
      cbitems = g_list_append(cbitems, "item0");
      cbitems = g_list_append(cbitems, "item1 item1");
      cbitems = g_list_append(cbitems, "item2 item2 item2");
      cbitems = g_list_append(cbitems, "item3 item3 item3 item3");
      cbitems = g_list_append(cbitems, "item4 item4 item4 item4 item4");
      cbitems = g_list_append(cbitems, "item5 item5 item5 item5 item5 item5");
      cbitems = g_list_append(cbitems, "item6 item6 item6 item6 item6");
      cbitems = g_list_append(cbitems, "item7 item7 item7 item7");
      cbitems = g_list_append(cbitems, "item8 item8 item8");
      cbitems = g_list_append(cbitems, "item9 item9");

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "entry");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (box2), hbox, TRUE, TRUE, 0);
      
      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "hello world السلام عليكم");
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, 5);
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

      button = gtk_button_new_with_mnemonic ("_Props");
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (entry_props_clicked),
			  entry);

      cb = gtk_combo_new ();
      gtk_combo_set_popdown_strings (GTK_COMBO (cb), cbitems);
      gtk_entry_set_text (GTK_ENTRY (GTK_COMBO(cb)->entry), "hello world \n\n\n foo");
      gtk_editable_select_region (GTK_EDITABLE (GTK_COMBO(cb)->entry),
				  0, -1);
      gtk_box_pack_start (GTK_BOX (box2), cb, TRUE, TRUE, 0);

      sensitive_check = gtk_check_button_new_with_label("Sensitive");
      gtk_box_pack_start (GTK_BOX (box2), sensitive_check, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT(sensitive_check), "toggled",
			  GTK_SIGNAL_FUNC(entry_toggle_sensitive), entry);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sensitive_check), TRUE);

      has_frame_check = gtk_check_button_new_with_label("Has Frame");
      gtk_box_pack_start (GTK_BOX (box2), has_frame_check, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT(has_frame_check), "toggled",
			  GTK_SIGNAL_FUNC(entry_toggle_frame), entry);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(has_frame_check), TRUE);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkSizeGroup
 */

#define SIZE_GROUP_INITIAL_SIZE 50

static void
size_group_hsize_changed (GtkSpinButton *spin_button,
			  GtkWidget     *button)
{
  gtk_widget_set_usize (GTK_BIN (button)->child,
			gtk_spin_button_get_value_as_int (spin_button),
			-2);
}

static void
size_group_vsize_changed (GtkSpinButton *spin_button,
			  GtkWidget     *button)
{
  gtk_widget_set_usize (GTK_BIN (button)->child,
			-2,
			gtk_spin_button_get_value_as_int (spin_button));
}

static GtkWidget *
create_size_group_window (GtkSizeGroup *master_size_group)
{
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *main_button;
  GtkWidget *button;
  GtkWidget *spin_button;
  GtkWidget *hbox;
  GtkSizeGroup *hgroup1;
  GtkSizeGroup *hgroup2;
  GtkSizeGroup *vgroup1;
  GtkSizeGroup *vgroup2;

  window = gtk_dialog_new_with_buttons ("GtkSizeGroup",
					NULL, 0,
					GTK_STOCK_CLOSE,
					GTK_RESPONSE_NONE,
					NULL);

  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  gtk_signal_connect (GTK_OBJECT (window), "response",
		      GTK_SIGNAL_FUNC (gtk_widget_destroy),
		      NULL);

  table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), table, TRUE, TRUE, 0);

  gtk_table_set_row_spacings (GTK_TABLE (table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_widget_set_usize (table, 250, 250);

  hgroup1 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  hgroup2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  vgroup1 = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  vgroup2 = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  main_button = gtk_button_new_with_label ("X");
  
  gtk_table_attach (GTK_TABLE (table), main_button,
		    0, 1,       0, 1,
		    GTK_EXPAND, GTK_EXPAND,
		    0,          0);
  gtk_size_group_add_widget (master_size_group, main_button);
  gtk_size_group_add_widget (hgroup1, main_button);
  gtk_size_group_add_widget (vgroup1, main_button);
  gtk_widget_set_usize (GTK_BIN (main_button)->child, SIZE_GROUP_INITIAL_SIZE, SIZE_GROUP_INITIAL_SIZE);

  button = gtk_button_new ();
  gtk_table_attach (GTK_TABLE (table), button,
		    1, 2,       0, 1,
		    GTK_EXPAND, GTK_EXPAND,
		    0,          0);
  gtk_size_group_add_widget (vgroup1, button);
  gtk_size_group_add_widget (vgroup2, button);

  button = gtk_button_new ();
  gtk_table_attach (GTK_TABLE (table), button,
		    0, 1,       1, 2,
		    GTK_EXPAND, GTK_EXPAND,
		    0,          0);
  gtk_size_group_add_widget (hgroup1, button);
  gtk_size_group_add_widget (hgroup2, button);

  button = gtk_button_new ();
  gtk_table_attach (GTK_TABLE (table), button,
		    1, 2,       1, 2,
		    GTK_EXPAND, GTK_EXPAND,
		    0,          0);
  gtk_size_group_add_widget (hgroup2, button);
  gtk_size_group_add_widget (vgroup2, button);

  g_object_unref (G_OBJECT (hgroup1));
  g_object_unref (G_OBJECT (hgroup2));
  g_object_unref (G_OBJECT (vgroup1));
  g_object_unref (G_OBJECT (vgroup2));
  
  hbox = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox, FALSE, FALSE, 0);
  
  spin_button = gtk_spin_button_new_with_range (1, 100, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), SIZE_GROUP_INITIAL_SIZE);
  gtk_box_pack_start (GTK_BOX (hbox), spin_button, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (spin_button), "value_changed",
		      GTK_SIGNAL_FUNC (size_group_hsize_changed), main_button);

  spin_button = gtk_spin_button_new_with_range (1, 100, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), SIZE_GROUP_INITIAL_SIZE);
  gtk_box_pack_start (GTK_BOX (hbox), spin_button, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (spin_button), "value_changed",
		      GTK_SIGNAL_FUNC (size_group_vsize_changed), main_button);

  return window;
}

static void
create_size_groups (void)
{
  static GtkWidget *window1 = NULL;
  static GtkWidget *window2 = NULL;
  static GtkSizeGroup *master_size_group;

  if (!master_size_group)
    master_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
  
  if (!window1)
    {
      window1 = create_size_group_window (master_size_group);

      gtk_signal_connect (GTK_OBJECT (window1), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window1);
    }

  if (!window2)
    {
      window2 = create_size_group_window (master_size_group);

      gtk_signal_connect (GTK_OBJECT (window2), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window2);
    }

  if (GTK_WIDGET_VISIBLE (window1) && GTK_WIDGET_VISIBLE (window2))
    {
      gtk_widget_destroy (window1);
      gtk_widget_destroy (window2);
    }
  else
    {
      if (!GTK_WIDGET_VISIBLE (window1))
	gtk_widget_show_all (window1);
      if (!GTK_WIDGET_VISIBLE (window2))
	gtk_widget_show_all (window2);
    }
}

/*
 * GtkSpinButton
 */

static GtkWidget *spinner1;

static void
toggle_snap (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_snap_to_ticks (spin, GTK_TOGGLE_BUTTON (widget)->active);
}

static void
toggle_numeric (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_numeric (spin, GTK_TOGGLE_BUTTON (widget)->active);
}

static void
change_digits (GtkWidget *widget, GtkSpinButton *spin)
{
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spinner1),
			      gtk_spin_button_get_value_as_int (spin));
}

static void
get_value (GtkWidget *widget, gpointer data)
{
  gchar buf[32];
  GtkLabel *label;
  GtkSpinButton *spin;

  spin = GTK_SPIN_BUTTON (spinner1);
  label = GTK_LABEL (gtk_object_get_user_data (GTK_OBJECT (widget)));
  if (GPOINTER_TO_INT (data) == 1)
    sprintf (buf, "%d", gtk_spin_button_get_value_as_int (spin));
  else
    sprintf (buf, "%0.*f", spin->digits,
	     gtk_spin_button_get_value_as_float (spin));
  gtk_label_set_text (label, buf);
}

static void
get_spin_value (GtkWidget *widget, gpointer data)
{
  gchar *buffer;
  GtkLabel *label;
  GtkSpinButton *spin;

  spin = GTK_SPIN_BUTTON (widget);
  label = GTK_LABEL (data);

  buffer = g_strdup_printf ("%0.*f", spin->digits,
			    gtk_spin_button_get_value_as_float (spin));
  gtk_label_set_text (label, buffer);

  g_free (buffer);
}

static gint
spin_button_time_output_func (GtkSpinButton *spin_button)
{
  static gchar buf[6];
  gdouble hours;
  gdouble minutes;

  hours = spin_button->adjustment->value / 60.0;
  minutes = (fabs(floor (hours) - hours) < 1e-5) ? 0.0 : 30;
  sprintf (buf, "%02.0f:%02.0f", floor (hours), minutes);
  if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
    gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
  return TRUE;
}

static gint
spin_button_month_input_func (GtkSpinButton *spin_button,
			      gdouble       *new_val)
{
  gint i;
  static gchar *month[12] = { "January", "February", "March", "April",
			      "May", "June", "July", "August",
			      "September", "October", "November", "December" };
  gchar *tmp1, *tmp2;
  gboolean found = FALSE;

  for (i = 1; i <= 12; i++)
    {
      tmp1 = g_strdup (month[i-1]);
      g_strup (tmp1);
      tmp2 = g_strdup (gtk_entry_get_text (GTK_ENTRY (spin_button)));
      g_strup (tmp2);
      if (strstr (tmp1, tmp2) == tmp1)
	found = TRUE;
      g_free (tmp1);
      g_free (tmp2);
      if (found)
	break;
    }
  if (!found)
    {
      *new_val = 0.0;
      return GTK_INPUT_ERROR;
    }
  *new_val = (gdouble) i;
  return TRUE;
}

static gint
spin_button_month_output_func (GtkSpinButton *spin_button)
{
  gint i;
  static gchar *month[12] = { "January", "February", "March", "April",
			      "May", "June", "July", "August", "September",
			      "October", "November", "December" };

  for (i = 1; i <= 12; i++)
    if (fabs (spin_button->adjustment->value - (double)i) < 1e-5)
      {
	if (strcmp (month[i-1], gtk_entry_get_text (GTK_ENTRY (spin_button))))
	  gtk_entry_set_text (GTK_ENTRY (spin_button), month[i-1]);
      }
  return TRUE;
}

static gint
spin_button_hex_input_func (GtkSpinButton *spin_button,
			    gdouble       *new_val)
{
  const gchar *buf;
  gchar *err;
  gdouble res;

  buf = gtk_entry_get_text (GTK_ENTRY (spin_button));
  res = strtol(buf, &err, 16);
  *new_val = res;
  if (*err)
    return GTK_INPUT_ERROR;
  else
    return TRUE;
}

static gint
spin_button_hex_output_func (GtkSpinButton *spin_button)
{
  static gchar buf[7];
  gint val;

  val = (gint) spin_button->adjustment->value;
  if (fabs (val) < 1e-5)
    sprintf (buf, "0x00");
  else
    sprintf (buf, "0x%.2X", val);
  if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
    gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
  return TRUE;
}

static void
create_spins (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *spinner2;
  GtkWidget *spinner;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *val_label;
  GtkAdjustment *adj;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);
      
      gtk_window_set_title (GTK_WINDOW (window), "GtkSpinButton");
      
      main_vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 10);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);
      
      frame = gtk_frame_new ("Not accelerated");
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
      
      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox);
      
      /* Time, month, hex spinners */
      
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);
      
      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
      
      label = gtk_label_new ("Time :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (0, 0, 1410, 30, 60, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_editable_set_editable (GTK_EDITABLE (spinner), FALSE);
      gtk_signal_connect (GTK_OBJECT (spinner),
			  "output",
			  GTK_SIGNAL_FUNC (spin_button_time_output_func),
			  NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_widget_set_usize (spinner, 55, -1);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
      
      label = gtk_label_new ("Month :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (1.0, 1.0, 12.0, 1.0,
						  5.0, 0.0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinner),
					 GTK_UPDATE_IF_VALID);
      gtk_signal_connect (GTK_OBJECT (spinner),
			  "input",
			  GTK_SIGNAL_FUNC (spin_button_month_input_func),
			  NULL);
      gtk_signal_connect (GTK_OBJECT (spinner),
			  "output",
			  GTK_SIGNAL_FUNC (spin_button_month_output_func),
			  NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_widget_set_usize (spinner, 85, -1);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);
      
      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);

      label = gtk_label_new ("Hex :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (0, 0, 255, 1, 16, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_editable_set_editable (GTK_EDITABLE (spinner), TRUE);
      gtk_signal_connect (GTK_OBJECT (spinner),
			  "input",
			  GTK_SIGNAL_FUNC (spin_button_hex_input_func),
			  NULL);
      gtk_signal_connect (GTK_OBJECT (spinner),
			  "output",
			  GTK_SIGNAL_FUNC (spin_button_hex_output_func),
			  NULL);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
      gtk_widget_set_usize (spinner, 55, 0);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);

      frame = gtk_frame_new ("Accelerated");
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
  
      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox);
      
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);
      
      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 5);
      
      label = gtk_label_new ("Value :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (0.0, -10000.0, 10000.0,
						  0.5, 100.0, 0.0);
      spinner1 = gtk_spin_button_new (adj, 1.0, 2);
      gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner1, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 5);

      label = gtk_label_new ("Digits :");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (2, 1, 15, 1, 1, 0);
      spinner2 = gtk_spin_button_new (adj, 0.0, 0);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (change_digits),
			  (gpointer) spinner2);
      gtk_box_pack_start (GTK_BOX (vbox2), spinner2, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

      button = gtk_check_button_new_with_label ("Snap to 0.5-ticks");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (toggle_snap),
			  spinner1);
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

      button = gtk_check_button_new_with_label ("Numeric only input mode");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (toggle_numeric),
			  spinner1);
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

      val_label = gtk_label_new ("");

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);

      button = gtk_button_new_with_label ("Value as Int");
      gtk_object_set_user_data (GTK_OBJECT (button), val_label);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (get_value),
			  GINT_TO_POINTER (1));
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

      button = gtk_button_new_with_label ("Value as Float");
      gtk_object_set_user_data (GTK_OBJECT (button), val_label);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (get_value),
			  GINT_TO_POINTER (2));
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

      gtk_box_pack_start (GTK_BOX (vbox), val_label, TRUE, TRUE, 0);
      gtk_label_set_text (GTK_LABEL (val_label), "0");

      frame = gtk_frame_new ("Using Convenience Constructor");
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
  
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_container_add (GTK_CONTAINER (frame), hbox);
      
      val_label = gtk_label_new ("0.0");

      spinner = gtk_spin_button_new_with_range (0.0, 10.0, 0.009);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinner), 0.0);
      gtk_signal_connect (GTK_OBJECT (spinner), "value_changed",
			  GTK_SIGNAL_FUNC (get_spin_value), val_label);
      gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX (hbox), val_label, TRUE, TRUE, 5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);
  
      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Cursors
 */

static gint
cursor_expose_event (GtkWidget *widget,
		     GdkEvent  *event,
		     gpointer   user_data)
{
  GtkDrawingArea *darea;
  GdkDrawable *drawable;
  GdkGC *black_gc;
  GdkGC *gray_gc;
  GdkGC *white_gc;
  guint max_width;
  guint max_height;

  g_return_val_if_fail (widget != NULL, TRUE);
  g_return_val_if_fail (GTK_IS_DRAWING_AREA (widget), TRUE);

  darea = GTK_DRAWING_AREA (widget);
  drawable = widget->window;
  white_gc = widget->style->white_gc;
  gray_gc = widget->style->bg_gc[GTK_STATE_NORMAL];
  black_gc = widget->style->black_gc;
  max_width = widget->allocation.width;
  max_height = widget->allocation.height;

  gdk_draw_rectangle (drawable, white_gc,
		      TRUE,
		      0,
		      0,
		      max_width,
		      max_height / 2);

  gdk_draw_rectangle (drawable, black_gc,
		      TRUE,
		      0,
		      max_height / 2,
		      max_width,
		      max_height / 2);

  gdk_draw_rectangle (drawable, gray_gc,
		      TRUE,
		      max_width / 3,
		      max_height / 3,
		      max_width / 3,
		      max_height / 3);

  return TRUE;
}

static void
set_cursor (GtkWidget *spinner,
	    GtkWidget *widget)
{
  guint c;
  GdkCursor *cursor;
  GtkWidget *label;
  GtkEnumValue *vals;

  c = CLAMP (gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinner)), 0, 152);
  c &= 0xfe;

  label = gtk_object_get_user_data (GTK_OBJECT (spinner));
  vals = gtk_type_enum_get_values (GDK_TYPE_CURSOR_TYPE);
  while (vals && vals->value != c)
    vals++;
  if (vals)
    gtk_label_set_text (GTK_LABEL (label), vals->value_nick);
  else
    gtk_label_set_text (GTK_LABEL (label), "<unknown>");

  cursor = gdk_cursor_new (c);
  gdk_window_set_cursor (widget->window, cursor);
  gdk_cursor_unref (cursor);
}

static gint
cursor_event (GtkWidget          *widget,
	      GdkEvent           *event,
	      GtkSpinButton	 *spinner)
{
  if ((event->type == GDK_BUTTON_PRESS) &&
      ((event->button.button == 1) ||
       (event->button.button == 3)))
    {
      gtk_spin_button_spin (spinner, event->button.button == 1 ?
			    GTK_SPIN_STEP_FORWARD : GTK_SPIN_STEP_BACKWARD, 0);
      return TRUE;
    }

  return FALSE;
}

static void
create_cursors (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *darea;
  GtkWidget *spinner;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *any;
  GtkAdjustment *adj;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);
      
      gtk_window_set_title (GTK_WINDOW (window), "Cursors");
      
      main_vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 0);
      gtk_container_add (GTK_CONTAINER (window), main_vbox);

      vbox =
	gtk_widget_new (gtk_vbox_get_type (),
			"GtkBox::homogeneous", FALSE,
			"GtkBox::spacing", 5,
			"GtkContainer::border_width", 10,
			"GtkWidget::parent", main_vbox,
			"GtkWidget::visible", TRUE,
			NULL);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
      
      label = gtk_label_new ("Cursor Value : ");
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (0,
						  0, 152,
						  2,
						  10, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spinner, TRUE, TRUE, 0);

      frame =
	gtk_widget_new (gtk_frame_get_type (),
			"GtkFrame::shadow", GTK_SHADOW_ETCHED_IN,
			"GtkFrame::label_xalign", 0.5,
			"GtkFrame::label", "Cursor Area",
			"GtkContainer::border_width", 10,
			"GtkWidget::parent", vbox,
			"GtkWidget::visible", TRUE,
			NULL);

      darea = gtk_drawing_area_new ();
      gtk_widget_set_usize (darea, 80, 80);
      gtk_container_add (GTK_CONTAINER (frame), darea);
      gtk_signal_connect (GTK_OBJECT (darea),
			  "expose_event",
			  GTK_SIGNAL_FUNC (cursor_expose_event),
			  NULL);
      gtk_widget_set_events (darea, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
      gtk_signal_connect (GTK_OBJECT (darea),
			  "button_press_event",
			  GTK_SIGNAL_FUNC (cursor_event),
			  spinner);
      gtk_widget_show (darea);

      gtk_signal_connect (GTK_OBJECT (spinner), "changed",
			  GTK_SIGNAL_FUNC (set_cursor),
			  darea);

      label = gtk_widget_new (GTK_TYPE_LABEL,
			      "visible", TRUE,
			      "label", "XXX",
			      "parent", vbox,
			      NULL);
      gtk_container_child_set (GTK_CONTAINER (vbox), label,
			       "expand", FALSE,
			       NULL);
      gtk_object_set_user_data (GTK_OBJECT (spinner), label);

      any =
	gtk_widget_new (gtk_hseparator_get_type (),
			"GtkWidget::visible", TRUE,
			NULL);
      gtk_box_pack_start (GTK_BOX (main_vbox), any, FALSE, TRUE, 0);
  
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

      gtk_widget_show_all (window);

      set_cursor (spinner, darea);
    }
  else
    gtk_widget_destroy (window);
}

/*
 * GtkList
 */

static void
list_add (GtkWidget *widget,
	  GtkWidget *list)
{
  static int i = 1;
  gchar buffer[64];
  GtkWidget *list_item;
  GtkContainer *container;

  container = GTK_CONTAINER (list);

  sprintf (buffer, "added item %d", i++);
  list_item = gtk_list_item_new_with_label (buffer);
  gtk_widget_show (list_item);

  gtk_container_add (container, list_item);
}

static void
list_remove (GtkWidget *widget,
	     GtkList   *list)
{
  GList *clear_list = NULL;
  GList *sel_row = NULL;
  GList *work = NULL;

  if (list->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GtkWidget *item;

      item = GTK_CONTAINER (list)->focus_child;
      if (!item && list->selection)
	item = list->selection->data;

      if (item)
	{
	  work = g_list_find (list->children, item);
	  for (sel_row = work; sel_row; sel_row = sel_row->next)
	    if (GTK_WIDGET (sel_row->data)->state != GTK_STATE_SELECTED)
	      break;

	  if (!sel_row)
	    {
	      for (sel_row = work; sel_row; sel_row = sel_row->prev)
		if (GTK_WIDGET (sel_row->data)->state != GTK_STATE_SELECTED)
		  break;
	    }
	}
    }

  for (work = list->selection; work; work = work->next)
    clear_list = g_list_prepend (clear_list, work->data);

  clear_list = g_list_reverse (clear_list);
  gtk_list_remove_items (GTK_LIST (list), clear_list);
  g_list_free (clear_list);

  if (list->selection_mode == GTK_SELECTION_EXTENDED && sel_row)
    gtk_list_select_child (list, GTK_WIDGET(sel_row->data));
}

static void
list_clear (GtkWidget *widget,
	    GtkWidget *list)
{
  gtk_list_clear_items (GTK_LIST (list), 0, -1);
}

static GtkWidget *list_omenu;

static void 
list_toggle_sel_mode (GtkWidget *widget, gpointer data)
{
  GtkList *list;
  gint i;

  list = GTK_LIST (data);

  if (!GTK_WIDGET_MAPPED (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_list_set_selection_mode (list, (GtkSelectionMode) i);
}

static void
create_list (void)
{
  static GtkWidget *window = NULL;

  static gchar *items[] =
  {
    "Single",
    "Browse",
    "Multiple"
  };

  if (!window)
    {
      GtkWidget *cbox;
      GtkWidget *vbox;
      GtkWidget *hbox;
      GtkWidget *label;
      GtkWidget *scrolled_win;
      GtkWidget *list;
      GtkWidget *button;
      GtkWidget *separator;
      FILE *infile;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "list");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
      gtk_widget_set_usize (scrolled_win, -1, 300);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);

      list = gtk_list_new ();
      gtk_list_set_selection_mode (GTK_LIST (list), GTK_SELECTION_EXTENDED);
      gtk_scrolled_window_add_with_viewport
	(GTK_SCROLLED_WINDOW (scrolled_win), list);
      gtk_container_set_focus_vadjustment
	(GTK_CONTAINER (list),
	 gtk_scrolled_window_get_vadjustment
	 (GTK_SCROLLED_WINDOW (scrolled_win)));
      gtk_container_set_focus_hadjustment
	(GTK_CONTAINER (list),
	 gtk_scrolled_window_get_hadjustment
	 (GTK_SCROLLED_WINDOW (scrolled_win)));

      if ((infile = fopen("gtkenums.h", "r")))
	{
	  char buffer[256];
	  char *pos;
	  GtkWidget *item;

	  while (fgets (buffer, 256, infile))
	    {
	      if ((pos = strchr (buffer, '\n')))
		*pos = 0;
	      item = gtk_list_item_new_with_label (buffer);
	      gtk_container_add (GTK_CONTAINER (list), item);
	    }
	  
	  fclose (infile);
	}


      hbox = gtk_hbox_new (TRUE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("Insert Row");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (list_add),
			  list);

      button = gtk_button_new_with_label ("Clear List");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (list_clear),
			  list);

      button = gtk_button_new_with_label ("Remove Selection");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (list_remove),
			  list);

      cbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (cbox), hbox, TRUE, FALSE, 0);

      label = gtk_label_new ("Selection Mode :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      list_omenu = build_option_menu (items, 3, 3, 
				      list_toggle_sel_mode,
				      list);
      gtk_box_pack_start (GTK_BOX (hbox), list_omenu, FALSE, TRUE, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);

      cbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), cbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_container_set_border_width (GTK_CONTAINER (button), 10);
      gtk_box_pack_start (GTK_BOX (cbox), button, TRUE, TRUE, 0);
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));

      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkCList
 */

static char * book_open_xpm[] = {
"16 16 4 1",
"       c None s None",
".      c black",
"X      c #808080",
"o      c white",
"                ",
"  ..            ",
" .Xo.    ...    ",
" .Xoo. ..oo.    ",
" .Xooo.Xooo...  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
" .Xooo.Xooo.X.  ",
" .Xooo.oooo.X.  ",
"  .Xoo.Xoo..X.  ",
"   .Xo.o..ooX.  ",
"    .X..XXXXX.  ",
"    ..X.......  ",
"     ..         ",
"                "};

static char * book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "};

static char * mini_page_xpm[] = {
"16 16 4 1",
"       c None s None",
".      c black",
"X      c white",
"o      c #808080",
"                ",
"   .......      ",
"   .XXXXX..     ",
"   .XoooX.X.    ",
"   .XXXXX....   ",
"   .XooooXoo.o  ",
"   .XXXXXXXX.o  ",
"   .XooooooX.o  ",
"   .XXXXXXXX.o  ",
"   .XooooooX.o  ",
"   .XXXXXXXX.o  ",
"   .XooooooX.o  ",
"   .XXXXXXXX.o  ",
"   ..........o  ",
"    oooooooooo  ",
"                "};

static char * gtk_mini_xpm[] = {
"15 20 17 1",
"       c None",
".      c #14121F",
"+      c #278828",
"@      c #9B3334",
"#      c #284C72",
"$      c #24692A",
"%      c #69282E",
"&      c #37C539",
"*      c #1D2F4D",
"=      c #6D7076",
"-      c #7D8482",
";      c #E24A49",
">      c #515357",
",      c #9B9C9B",
"'      c #2FA232",
")      c #3CE23D",
"!      c #3B6CCB",
"               ",
"      ***>     ",
"    >.*!!!*    ",
"   ***....#*=  ",
"  *!*.!!!**!!# ",
" .!!#*!#*!!!!# ",
" @%#!.##.*!!$& ",
" @;%*!*.#!#')) ",
" @;;@%!!*$&)'' ",
" @%.%@%$'&)$+' ",
" @;...@$'*'*)+ ",
" @;%..@$+*.')$ ",
" @;%%;;$+..$)# ",
" @;%%;@$$$'.$# ",
" %;@@;;$$+))&* ",
"  %;;;@+$&)&*  ",
"   %;;@'))+>   ",
"    %;@'&#     ",
"     >%$$      ",
"      >=       "};

#define TESTGTK_CLIST_COLUMNS 12
static gint clist_rows = 0;
static GtkWidget *clist_omenu;

static void
add1000_clist (GtkWidget *widget, gpointer data)
{
  gint i, row;
  char text[TESTGTK_CLIST_COLUMNS][50];
  char *texts[TESTGTK_CLIST_COLUMNS];
  GdkBitmap *mask;
  GdkPixmap *pixmap;
  GtkCList  *clist;

  clist = GTK_CLIST (data);

  pixmap = gdk_pixmap_create_from_xpm_d (clist->clist_window,
					 &mask, 
					 &GTK_WIDGET (data)->style->white,
					 gtk_mini_xpm);

  for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
    {
      texts[i] = text[i];
      sprintf (text[i], "Column %d", i);
    }
  
  texts[3] = NULL;
  sprintf (text[1], "Right");
  sprintf (text[2], "Center");
  
  gtk_clist_freeze (GTK_CLIST (data));
  for (i = 0; i < 1000; i++)
    {
      sprintf (text[0], "CListRow %d", rand() % 10000);
      row = gtk_clist_append (clist, texts);
      gtk_clist_set_pixtext (clist, row, 3, "gtk+", 5, pixmap, mask);
    }

  gtk_clist_thaw (GTK_CLIST (data));

  gdk_pixmap_unref (pixmap);
  gdk_bitmap_unref (mask);
}

static void
add10000_clist (GtkWidget *widget, gpointer data)
{
  gint i;
  char text[TESTGTK_CLIST_COLUMNS][50];
  char *texts[TESTGTK_CLIST_COLUMNS];

  for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
    {
      texts[i] = text[i];
      sprintf (text[i], "Column %d", i);
    }
  
  sprintf (text[1], "Right");
  sprintf (text[2], "Center");
  
  gtk_clist_freeze (GTK_CLIST (data));
  for (i = 0; i < 10000; i++)
    {
      sprintf (text[0], "CListRow %d", rand() % 10000);
      gtk_clist_append (GTK_CLIST (data), texts);
    }
  gtk_clist_thaw (GTK_CLIST (data));
}

void
clear_clist (GtkWidget *widget, gpointer data)
{
  gtk_clist_clear (GTK_CLIST (data));
  clist_rows = 0;
}

void clist_remove_selection (GtkWidget *widget, GtkCList *clist)
{
  gtk_clist_freeze (clist);

  while (clist->selection)
    {
      gint row;

      clist_rows--;
      row = GPOINTER_TO_INT (clist->selection->data);

      gtk_clist_remove (clist, row);

      if (clist->selection_mode == GTK_SELECTION_BROWSE)
	break;
    }

  if (clist->selection_mode == GTK_SELECTION_EXTENDED && !clist->selection &&
      clist->focus_row >= 0)
    gtk_clist_select_row (clist, clist->focus_row, -1);

  gtk_clist_thaw (clist);
}

void toggle_title_buttons (GtkWidget *widget, GtkCList *clist)
{
  if (GTK_TOGGLE_BUTTON (widget)->active)
    gtk_clist_column_titles_show (clist);
  else
    gtk_clist_column_titles_hide (clist);
}

void toggle_reorderable (GtkWidget *widget, GtkCList *clist)
{
  gtk_clist_set_reorderable (clist, GTK_TOGGLE_BUTTON (widget)->active);
}

static void
insert_row_clist (GtkWidget *widget, gpointer data)
{
  static char *text[] =
  {
    "This", "is an", "inserted", "row.",
    "This", "is an", "inserted", "row.",
    "This", "is an", "inserted", "row."
  };

  static GtkStyle *style1 = NULL;
  static GtkStyle *style2 = NULL;
  static GtkStyle *style3 = NULL;
  gint row;
  
  if (GTK_CLIST (data)->focus_row >= 0)
    row = gtk_clist_insert (GTK_CLIST (data), GTK_CLIST (data)->focus_row,
			    text);
  else
    row = gtk_clist_prepend (GTK_CLIST (data), text);

  if (!style1)
    {
      GdkColor col1;
      GdkColor col2;

      col1.red   = 0;
      col1.green = 56000;
      col1.blue  = 0;
      col2.red   = 32000;
      col2.green = 0;
      col2.blue  = 56000;

      style1 = gtk_style_copy (GTK_WIDGET (data)->style);
      style1->base[GTK_STATE_NORMAL] = col1;
      style1->base[GTK_STATE_SELECTED] = col2;

      style2 = gtk_style_copy (GTK_WIDGET (data)->style);
      style2->fg[GTK_STATE_NORMAL] = col1;
      style2->fg[GTK_STATE_SELECTED] = col2;

      style3 = gtk_style_copy (GTK_WIDGET (data)->style);
      style3->fg[GTK_STATE_NORMAL] = col1;
      style3->base[GTK_STATE_NORMAL] = col2;
      pango_font_description_free (style3->font_desc);
      style3->font_desc = pango_font_description_from_string ("courier 12");
    }

  gtk_clist_set_cell_style (GTK_CLIST (data), row, 3, style1);
  gtk_clist_set_cell_style (GTK_CLIST (data), row, 4, style2);
  gtk_clist_set_cell_style (GTK_CLIST (data), row, 0, style3);

  clist_rows++;
}

static void
clist_warning_test (GtkWidget *button,
		    GtkWidget *clist)
{
  GtkWidget *child;
  static gboolean add_remove = FALSE;

  add_remove = !add_remove;

  child = gtk_label_new ("Test");
  gtk_widget_ref (child);
  gtk_object_sink (GTK_OBJECT (child));

  if (add_remove)
    gtk_container_add (GTK_CONTAINER (clist), child);
  else
    {
      child->parent = clist;
      gtk_container_remove (GTK_CONTAINER (clist), child);
      child->parent = NULL;
    }

  gtk_widget_destroy (child);
  gtk_widget_unref (child);
}

static void
undo_selection (GtkWidget *button, GtkCList *clist)
{
  gtk_clist_undo_selection (clist);
}

static void 
clist_toggle_sel_mode (GtkWidget *widget, gpointer data)
{
  GtkCList *clist;
  gint i;

  clist = GTK_CLIST (data);

  if (!GTK_WIDGET_MAPPED (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_clist_set_selection_mode (clist, (GtkSelectionMode) i);
}

static void 
clist_click_column (GtkCList *clist, gint column, gpointer data)
{
  if (column == 4)
    gtk_clist_set_column_visibility (clist, column, FALSE);
  else if (column == clist->sort_column)
    {
      if (clist->sort_type == GTK_SORT_ASCENDING)
	clist->sort_type = GTK_SORT_DESCENDING;
      else
	clist->sort_type = GTK_SORT_ASCENDING;
    }
  else
    gtk_clist_set_sort_column (clist, column);

  gtk_clist_sort (clist);
}

static void
create_clist (void)
{
  gint i;
  static GtkWidget *window = NULL;

  static char *titles[] =
  {
    "auto resize", "not resizeable", "max width 100", "min width 50",
    "hide column", "Title 5", "Title 6", "Title 7",
    "Title 8",  "Title 9",  "Title 10", "Title 11"
  };

  static gchar *items[] =
  {
    "Single",
    "Browse",
    "Multiple",
  };

  char text[TESTGTK_CLIST_COLUMNS][50];
  char *texts[TESTGTK_CLIST_COLUMNS];

  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *clist;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *scrolled_win;
  GtkWidget *check;

  GtkWidget *undo_button;
  GtkWidget *label;

  GtkStyle *style;
  GdkColor col1;
  GdkColor col2;

  if (!window)
    {
      clist_rows = 0;
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed), &window);

      gtk_window_set_title (GTK_WINDOW (window), "clist");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC, 
				      GTK_POLICY_AUTOMATIC);

      /* create GtkCList here so we have a pointer to throw at the 
       * button callbacks -- more is done with it later */
      clist = gtk_clist_new_with_titles (TESTGTK_CLIST_COLUMNS, titles);
      gtk_container_add (GTK_CONTAINER (scrolled_win), clist);
      gtk_signal_connect (GTK_OBJECT (clist), "click_column",
			  (GtkSignalFunc) clist_click_column, NULL);

      /* control buttons */
      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Insert Row");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
                          (GtkSignalFunc) insert_row_clist, (gpointer) clist);

      button = gtk_button_new_with_label ("Add 1,000 Rows With Pixmaps");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
                          (GtkSignalFunc) add1000_clist, (gpointer) clist);

      button = gtk_button_new_with_label ("Add 10,000 Rows");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
                          (GtkSignalFunc) add10000_clist, (gpointer) clist);

      /* second layer of buttons */
      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Clear List");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
                          (GtkSignalFunc) clear_clist, (gpointer) clist);

      button = gtk_button_new_with_label ("Remove Selection");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
                          (GtkSignalFunc) clist_remove_selection,
			  (gpointer) clist);

      undo_button = gtk_button_new_with_label ("Undo Selection");
      gtk_box_pack_start (GTK_BOX (hbox), undo_button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (undo_button), "clicked",
                          (GtkSignalFunc) undo_selection, (gpointer) clist);

      button = gtk_button_new_with_label ("Warning Test");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
                          (GtkSignalFunc) clist_warning_test,(gpointer) clist);

      /* third layer of buttons */
      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      check = gtk_check_button_new_with_label ("Show Title Buttons");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (check), "clicked",
			  GTK_SIGNAL_FUNC (toggle_title_buttons), clist);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

      check = gtk_check_button_new_with_label ("Reorderable");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (check), "clicked",
			  GTK_SIGNAL_FUNC (toggle_reorderable), clist);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

      label = gtk_label_new ("Selection Mode :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      clist_omenu = build_option_menu (items, 3, 3, 
				       clist_toggle_sel_mode,
				       clist);
      gtk_box_pack_start (GTK_BOX (hbox), clist_omenu, FALSE, TRUE, 0);

      /* 
       * the rest of the clist configuration
       */

      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_clist_set_row_height (GTK_CLIST (clist), 18);
      gtk_widget_set_usize (clist, -1, 300);

      for (i = 1; i < TESTGTK_CLIST_COLUMNS; i++)
	gtk_clist_set_column_width (GTK_CLIST (clist), i, 80);

      gtk_clist_set_column_auto_resize (GTK_CLIST (clist), 0, TRUE);
      gtk_clist_set_column_resizeable (GTK_CLIST (clist), 1, FALSE);
      gtk_clist_set_column_max_width (GTK_CLIST (clist), 2, 100);
      gtk_clist_set_column_min_width (GTK_CLIST (clist), 3, 50);
      gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_EXTENDED);
      gtk_clist_set_column_justification (GTK_CLIST (clist), 1,
					  GTK_JUSTIFY_RIGHT);
      gtk_clist_set_column_justification (GTK_CLIST (clist), 2,
					  GTK_JUSTIFY_CENTER);
      
      for (i = 0; i < TESTGTK_CLIST_COLUMNS; i++)
	{
	  texts[i] = text[i];
	  sprintf (text[i], "Column %d", i);
	}

      sprintf (text[1], "Right");
      sprintf (text[2], "Center");

      col1.red   = 56000;
      col1.green = 0;
      col1.blue  = 0;
      col2.red   = 0;
      col2.green = 56000;
      col2.blue  = 32000;

      style = gtk_style_new ();
      style->fg[GTK_STATE_NORMAL] = col1;
      style->base[GTK_STATE_NORMAL] = col2;

      pango_font_description_set_size (style->font_desc, 14 * PANGO_SCALE);
      pango_font_description_set_weight (style->font_desc, PANGO_WEIGHT_BOLD);

      for (i = 0; i < 10; i++)
	{
	  sprintf (text[0], "CListRow %d", clist_rows++);
	  gtk_clist_append (GTK_CLIST (clist), texts);

	  switch (i % 4)
	    {
	    case 2:
	      gtk_clist_set_row_style (GTK_CLIST (clist), i, style);
	      break;
	    default:
	      gtk_clist_set_cell_style (GTK_CLIST (clist), i, i % 4, style);
	      break;
	    }
	}

      gtk_style_unref (style);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      gtk_container_set_border_width (GTK_CONTAINER (button), 10);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));

      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    {
      clist_rows = 0;
      gtk_widget_destroy (window);
    }
}

/*
 * GtkCTree
 */

GdkPixmap *pixmap1;
GdkPixmap *pixmap2;
GdkPixmap *pixmap3;
GdkBitmap *mask1;
GdkBitmap *mask2;
GdkBitmap *mask3;

static gint books = 0;
static gint pages = 0;

static GtkWidget *book_label;
static GtkWidget *page_label;
static GtkWidget *sel_label;
static GtkWidget *vis_label;
static GtkWidget *omenu1;
static GtkWidget *omenu2;
static GtkWidget *omenu3;
static GtkWidget *omenu4;
static GtkWidget *spin1;
static GtkWidget *spin2;
static GtkWidget *spin3;
static gint line_style;

void after_press (GtkCTree *ctree, gpointer data)
{
  char buf[80];

  sprintf (buf, "%d", g_list_length (GTK_CLIST (ctree)->selection));
  gtk_label_set_text (GTK_LABEL (sel_label), buf);

  sprintf (buf, "%d", g_list_length (GTK_CLIST (ctree)->row_list));
  gtk_label_set_text (GTK_LABEL (vis_label), buf);

  sprintf (buf, "%d", books);
  gtk_label_set_text (GTK_LABEL (book_label), buf);

  sprintf (buf, "%d", pages);
  gtk_label_set_text (GTK_LABEL (page_label), buf);
}

void after_move (GtkCTree *ctree, GtkCTreeNode *child, GtkCTreeNode *parent, 
		 GtkCTreeNode *sibling, gpointer data)
{
  char *source;
  char *target1;
  char *target2;

  gtk_ctree_get_node_info (ctree, child, &source, 
			   NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  if (parent)
    gtk_ctree_get_node_info (ctree, parent, &target1, 
			     NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  if (sibling)
    gtk_ctree_get_node_info (ctree, sibling, &target2, 
			     NULL, NULL, NULL, NULL, NULL, NULL, NULL);

  g_print ("Moving \"%s\" to \"%s\" with sibling \"%s\".\n", source,
	   (parent) ? target1 : "nil", (sibling) ? target2 : "nil");
}

void count_items (GtkCTree *ctree, GtkCTreeNode *list)
{
  if (GTK_CTREE_ROW (list)->is_leaf)
    pages--;
  else
    books--;
}

void expand_all (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_expand_recursive (ctree, NULL);
  after_press (ctree, NULL);
}

void collapse_all (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_collapse_recursive (ctree, NULL);
  after_press (ctree, NULL);
}

void select_all (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_select_recursive (ctree, NULL);
  after_press (ctree, NULL);
}

void change_style (GtkWidget *widget, GtkCTree *ctree)
{
  static GtkStyle *style1 = NULL;
  static GtkStyle *style2 = NULL;

  GtkCTreeNode *node;
  GdkColor col1;
  GdkColor col2;

  if (GTK_CLIST (ctree)->focus_row >= 0)
    node = GTK_CTREE_NODE
      (g_list_nth (GTK_CLIST (ctree)->row_list,GTK_CLIST (ctree)->focus_row));
  else
    node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

  if (!node)
    return;

  if (!style1)
    {
      col1.red   = 0;
      col1.green = 56000;
      col1.blue  = 0;
      col2.red   = 32000;
      col2.green = 0;
      col2.blue  = 56000;

      style1 = gtk_style_new ();
      style1->base[GTK_STATE_NORMAL] = col1;
      style1->fg[GTK_STATE_SELECTED] = col2;

      style2 = gtk_style_new ();
      style2->base[GTK_STATE_SELECTED] = col2;
      style2->fg[GTK_STATE_NORMAL] = col1;
      style2->base[GTK_STATE_NORMAL] = col2;
      pango_font_description_free (style2->font_desc);
      style2->font_desc = pango_font_description_from_string ("courier 30");
    }

  gtk_ctree_node_set_cell_style (ctree, node, 1, style1);
  gtk_ctree_node_set_cell_style (ctree, node, 0, style2);

  if (GTK_CTREE_ROW (node)->children)
    gtk_ctree_node_set_row_style (ctree, GTK_CTREE_ROW (node)->children,
				  style2);
}

void unselect_all (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_unselect_recursive (ctree, NULL);
  after_press (ctree, NULL);
}

void remove_selection (GtkWidget *widget, GtkCTree *ctree)
{
  GtkCList *clist;
  GtkCTreeNode *node;

  clist = GTK_CLIST (ctree);

  gtk_clist_freeze (clist);

  while (clist->selection)
    {
      node = clist->selection->data;

      if (GTK_CTREE_ROW (node)->is_leaf)
	pages--;
      else
	gtk_ctree_post_recursive (ctree, node,
				  (GtkCTreeFunc) count_items, NULL);

      gtk_ctree_remove_node (ctree, node);

      if (clist->selection_mode == GTK_SELECTION_BROWSE)
	break;
    }

  if (clist->selection_mode == GTK_SELECTION_EXTENDED && !clist->selection &&
      clist->focus_row >= 0)
    {
      node = gtk_ctree_node_nth (ctree, clist->focus_row);

      if (node)
	gtk_ctree_select (ctree, node);
    }
    
  gtk_clist_thaw (clist);
  after_press (ctree, NULL);
}

struct _ExportStruct {
  gchar *tree;
  gchar *info;
  gboolean is_leaf;
};

typedef struct _ExportStruct ExportStruct;

gboolean
gnode2ctree (GtkCTree   *ctree,
	     guint       depth,
	     GNode        *gnode,
	     GtkCTreeNode *cnode,
	     gpointer    data)
{
  ExportStruct *es;
  GdkPixmap *pixmap_closed;
  GdkBitmap *mask_closed;
  GdkPixmap *pixmap_opened;
  GdkBitmap *mask_opened;

  if (!cnode || !gnode || (!(es = gnode->data)))
    return FALSE;

  if (es->is_leaf)
    {
      pixmap_closed = pixmap3;
      mask_closed = mask3;
      pixmap_opened = NULL;
      mask_opened = NULL;
    }
  else
    {
      pixmap_closed = pixmap1;
      mask_closed = mask1;
      pixmap_opened = pixmap2;
      mask_opened = mask2;
    }

  gtk_ctree_set_node_info (ctree, cnode, es->tree, 2, pixmap_closed,
			   mask_closed, pixmap_opened, mask_opened,
			   es->is_leaf, (depth < 3));
  gtk_ctree_node_set_text (ctree, cnode, 1, es->info);
  g_free (es);
  gnode->data = NULL;

  return TRUE;
}

gboolean
ctree2gnode (GtkCTree   *ctree,
	     guint       depth,
	     GNode        *gnode,
	     GtkCTreeNode *cnode,
	     gpointer    data)
{
  ExportStruct *es;

  if (!cnode || !gnode)
    return FALSE;
  
  es = g_new (ExportStruct, 1);
  gnode->data = es;
  es->is_leaf = GTK_CTREE_ROW (cnode)->is_leaf;
  es->tree = GTK_CELL_PIXTEXT (GTK_CTREE_ROW (cnode)->row.cell[0])->text;
  es->info = GTK_CELL_PIXTEXT (GTK_CTREE_ROW (cnode)->row.cell[1])->text;
  return TRUE;
}

void export_ctree (GtkWidget *widget, GtkCTree *ctree)
{
  char *title[] = { "Tree" , "Info" };
  static GtkWidget *export_window = NULL;
  static GtkCTree *export_ctree;
  GtkWidget *vbox;
  GtkWidget *scrolled_win;
  GtkWidget *button;
  GtkWidget *sep;
  GNode *gnode;
  GtkCTreeNode *node;

  if (!export_window)
    {
      export_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
      gtk_signal_connect (GTK_OBJECT (export_window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &export_window);

      gtk_window_set_title (GTK_WINDOW (export_window), "exported ctree");
      gtk_container_set_border_width (GTK_CONTAINER (export_window), 5);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (export_window), vbox);
      
      button = gtk_button_new_with_label ("Close");
      gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, TRUE, 0);

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 (GtkSignalFunc) gtk_widget_destroy,
				 GTK_OBJECT(export_window));

      sep = gtk_hseparator_new ();
      gtk_box_pack_end (GTK_BOX (vbox), sep, FALSE, TRUE, 10);

      export_ctree = GTK_CTREE (gtk_ctree_new_with_titles (2, 0, title));
      gtk_ctree_set_line_style (export_ctree, GTK_CTREE_LINES_DOTTED);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (scrolled_win),
			 GTK_WIDGET (export_ctree));
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_clist_set_selection_mode (GTK_CLIST (export_ctree),
				    GTK_SELECTION_EXTENDED);
      gtk_clist_set_column_width (GTK_CLIST (export_ctree), 0, 200);
      gtk_clist_set_column_width (GTK_CLIST (export_ctree), 1, 200);
      gtk_widget_set_usize (GTK_WIDGET (export_ctree), 300, 200);
    }

  if (!GTK_WIDGET_VISIBLE (export_window))
    gtk_widget_show_all (export_window);
      
  gtk_clist_clear (GTK_CLIST (export_ctree));

  node = GTK_CTREE_NODE (g_list_nth (GTK_CLIST (ctree)->row_list,
				     GTK_CLIST (ctree)->focus_row));
  if (!node)
    return;

  gnode = gtk_ctree_export_to_gnode (ctree, NULL, NULL, node,
				     ctree2gnode, NULL);
  if (gnode)
    {
      gtk_ctree_insert_gnode (export_ctree, NULL, NULL, gnode,
			      gnode2ctree, NULL);
      g_node_destroy (gnode);
    }
}

void change_indent (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_set_indent (ctree, GTK_ADJUSTMENT (widget)->value);
}

void change_spacing (GtkWidget *widget, GtkCTree *ctree)
{
  gtk_ctree_set_spacing (ctree, GTK_ADJUSTMENT (widget)->value);
}

void change_row_height (GtkWidget *widget, GtkCList *clist)
{
  gtk_clist_set_row_height (clist, GTK_ADJUSTMENT (widget)->value);
}

void set_background (GtkCTree *ctree, GtkCTreeNode *node, gpointer data)
{
  GtkStyle *style = NULL;
  
  if (!node)
    return;
  
  if (ctree->line_style != GTK_CTREE_LINES_TABBED)
    {
      if (!GTK_CTREE_ROW (node)->is_leaf)
	style = GTK_CTREE_ROW (node)->row.data;
      else if (GTK_CTREE_ROW (node)->parent)
	style = GTK_CTREE_ROW (GTK_CTREE_ROW (node)->parent)->row.data;
    }

  gtk_ctree_node_set_row_style (ctree, node, style);
}

void 
ctree_toggle_line_style (GtkWidget *widget, gpointer data)
{
  GtkCTree *ctree;
  gint i;

  ctree = GTK_CTREE (data);

  if (!GTK_WIDGET_MAPPED (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  if ((ctree->line_style == GTK_CTREE_LINES_TABBED && 
       ((GtkCTreeLineStyle) i) != GTK_CTREE_LINES_TABBED) ||
      (ctree->line_style != GTK_CTREE_LINES_TABBED && 
       ((GtkCTreeLineStyle) i) == GTK_CTREE_LINES_TABBED))
    gtk_ctree_pre_recursive (ctree, NULL, set_background, NULL);
  gtk_ctree_set_line_style (ctree, i);
  line_style = i;
}

void 
ctree_toggle_expander_style (GtkWidget *widget, gpointer data)
{
  GtkCTree *ctree;
  gint i;

  ctree = GTK_CTREE (data);

  if (!GTK_WIDGET_MAPPED (widget))
    return;
  
  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));
  
  gtk_ctree_set_expander_style (ctree, (GtkCTreeExpanderStyle) i);
}

void 
ctree_toggle_justify (GtkWidget *widget, gpointer data)
{
  GtkCTree *ctree;
  gint i;

  ctree = GTK_CTREE (data);

  if (!GTK_WIDGET_MAPPED (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_clist_set_column_justification (GTK_CLIST (ctree), ctree->tree_column, 
				      (GtkJustification) i);
}

void 
ctree_toggle_sel_mode (GtkWidget *widget, gpointer data)
{
  GtkCTree *ctree;
  gint i;

  ctree = GTK_CTREE (data);

  if (!GTK_WIDGET_MAPPED (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_clist_set_selection_mode (GTK_CLIST (ctree), (GtkSelectionMode) i);
  after_press (ctree, NULL);
}
    
void build_recursive (GtkCTree *ctree, gint cur_depth, gint depth, 
		      gint num_books, gint num_pages, GtkCTreeNode *parent)
{
  gchar *text[2];
  gchar buf1[60];
  gchar buf2[60];
  GtkCTreeNode *sibling;
  gint i;

  text[0] = buf1;
  text[1] = buf2;
  sibling = NULL;

  for (i = num_pages + num_books; i > num_books; i--)
    {
      pages++;
      sprintf (buf1, "Page %02d", (gint) rand() % 100);
      sprintf (buf2, "Item %d-%d", cur_depth, i);
      sibling = gtk_ctree_insert_node (ctree, parent, sibling, text, 5,
				       pixmap3, mask3, NULL, NULL,
				       TRUE, FALSE);

      if (parent && ctree->line_style == GTK_CTREE_LINES_TABBED)
	gtk_ctree_node_set_row_style (ctree, sibling,
				      GTK_CTREE_ROW (parent)->row.style);
    }

  if (cur_depth == depth)
    return;

  for (i = num_books; i > 0; i--)
    {
      GtkStyle *style;

      books++;
      sprintf (buf1, "Book %02d", (gint) rand() % 100);
      sprintf (buf2, "Item %d-%d", cur_depth, i);
      sibling = gtk_ctree_insert_node (ctree, parent, sibling, text, 5,
				       pixmap1, mask1, pixmap2, mask2,
				       FALSE, FALSE);

      style = gtk_style_new ();
      switch (cur_depth % 3)
	{
	case 0:
	  style->base[GTK_STATE_NORMAL].red   = 10000 * (cur_depth % 6);
	  style->base[GTK_STATE_NORMAL].green = 0;
	  style->base[GTK_STATE_NORMAL].blue  = 65535 - ((i * 10000) % 65535);
	  break;
	case 1:
	  style->base[GTK_STATE_NORMAL].red   = 10000 * (cur_depth % 6);
	  style->base[GTK_STATE_NORMAL].green = 65535 - ((i * 10000) % 65535);
	  style->base[GTK_STATE_NORMAL].blue  = 0;
	  break;
	default:
	  style->base[GTK_STATE_NORMAL].red   = 65535 - ((i * 10000) % 65535);
	  style->base[GTK_STATE_NORMAL].green = 0;
	  style->base[GTK_STATE_NORMAL].blue  = 10000 * (cur_depth % 6);
	  break;
	}
      gtk_ctree_node_set_row_data_full (ctree, sibling, style,
					(GtkDestroyNotify) gtk_style_unref);

      if (ctree->line_style == GTK_CTREE_LINES_TABBED)
	gtk_ctree_node_set_row_style (ctree, sibling, style);

      build_recursive (ctree, cur_depth + 1, depth, num_books, num_pages,
		       sibling);
    }
}

void rebuild_tree (GtkWidget *widget, GtkCTree *ctree)
{
  gchar *text [2];
  gchar label1[] = "Root";
  gchar label2[] = "";
  GtkCTreeNode *parent;
  GtkStyle *style;
  guint b, d, p, n;

  text[0] = label1;
  text[1] = label2;
  
  d = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin1)); 
  b = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin2));
  p = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin3));

  n = ((pow (b, d) - 1) / (b - 1)) * (p + 1);

  if (n > 100000)
    {
      g_print ("%d total items? Try less\n",n);
      return;
    }

  gtk_clist_freeze (GTK_CLIST (ctree));
  gtk_clist_clear (GTK_CLIST (ctree));

  books = 1;
  pages = 0;

  parent = gtk_ctree_insert_node (ctree, NULL, NULL, text, 5, pixmap1,
				  mask1, pixmap2, mask2, FALSE, TRUE);

  style = gtk_style_new ();
  style->base[GTK_STATE_NORMAL].red   = 0;
  style->base[GTK_STATE_NORMAL].green = 45000;
  style->base[GTK_STATE_NORMAL].blue  = 55000;
  gtk_ctree_node_set_row_data_full (ctree, parent, style,
				    (GtkDestroyNotify) gtk_style_unref);

  if (ctree->line_style == GTK_CTREE_LINES_TABBED)
    gtk_ctree_node_set_row_style (ctree, parent, style);

  build_recursive (ctree, 1, d, b, p, parent);
  gtk_clist_thaw (GTK_CLIST (ctree));
  after_press (ctree, NULL);
}

static void 
ctree_click_column (GtkCTree *ctree, gint column, gpointer data)
{
  GtkCList *clist;

  clist = GTK_CLIST (ctree);

  if (column == clist->sort_column)
    {
      if (clist->sort_type == GTK_SORT_ASCENDING)
	clist->sort_type = GTK_SORT_DESCENDING;
      else
	clist->sort_type = GTK_SORT_ASCENDING;
    }
  else
    gtk_clist_set_sort_column (clist, column);

  gtk_ctree_sort_recursive (ctree, NULL);
}

void create_ctree (void)
{
  static GtkWidget *window = NULL;
  GtkTooltips *tooltips;
  GtkCTree *ctree;
  GtkWidget *scrolled_win;
  GtkWidget *vbox;
  GtkWidget *bbox;
  GtkWidget *mbox;
  GtkWidget *hbox;
  GtkWidget *hbox2;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *check;
  GtkAdjustment *adj;
  GtkWidget *spinner;
  GdkColor transparent = { 0 };

  char *title[] = { "Tree" , "Info" };
  char buf[80];

  static gchar *items1[] =
  {
    "No lines",
    "Solid",
    "Dotted",
    "Tabbed"
  };

  static gchar *items2[] =
  {
    "None",
    "Square",
    "Triangle",
    "Circular"
  };

  static gchar *items3[] =
  {
    "Left",
    "Right"
  };
  
  static gchar *items4[] =
  {
    "Single",
    "Browse",
    "Multiple",
  };

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "GtkCTree");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      tooltips = gtk_tooltips_new ();
      gtk_object_ref (GTK_OBJECT (tooltips));
      gtk_object_sink (GTK_OBJECT (tooltips));

      gtk_object_set_data_full (GTK_OBJECT (window), "tooltips", tooltips,
				(GtkDestroyNotify) gtk_object_unref);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
      
      label = gtk_label_new ("Depth :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (4, 1, 10, 1, 5, 0);
      spin1 = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spin1, FALSE, TRUE, 5);
  
      label = gtk_label_new ("Books :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (3, 1, 20, 1, 5, 0);
      spin2 = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spin2, FALSE, TRUE, 5);

      label = gtk_label_new ("Pages :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (5, 1, 20, 1, 5, 0);
      spin3 = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), spin3, FALSE, TRUE, 5);

      button = gtk_button_new_with_label ("Close");
      gtk_box_pack_end (GTK_BOX (hbox), button, TRUE, TRUE, 0);

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 (GtkSignalFunc) gtk_widget_destroy,
				 GTK_OBJECT(window));

      button = gtk_button_new_with_label ("Rebuild Tree");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (scrolled_win), 5);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_ALWAYS);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);

      ctree = GTK_CTREE (gtk_ctree_new_with_titles (2, 0, title));
      gtk_container_add (GTK_CONTAINER (scrolled_win), GTK_WIDGET (ctree));

      gtk_clist_set_column_auto_resize (GTK_CLIST (ctree), 0, TRUE);
      gtk_clist_set_column_width (GTK_CLIST (ctree), 1, 200);
      gtk_clist_set_selection_mode (GTK_CLIST (ctree), GTK_SELECTION_EXTENDED);
      gtk_ctree_set_line_style (ctree, GTK_CTREE_LINES_DOTTED);
      line_style = GTK_CTREE_LINES_DOTTED;

      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (rebuild_tree), ctree);
      gtk_signal_connect (GTK_OBJECT (ctree), "click_column",
			  (GtkSignalFunc) ctree_click_column, NULL);

      gtk_signal_connect_after (GTK_OBJECT (ctree), "button_press_event",
				GTK_SIGNAL_FUNC (after_press), NULL);
      gtk_signal_connect_after (GTK_OBJECT (ctree), "button_release_event",
				GTK_SIGNAL_FUNC (after_press), NULL);
      gtk_signal_connect_after (GTK_OBJECT (ctree), "tree_move",
				GTK_SIGNAL_FUNC (after_move), NULL);
      gtk_signal_connect_after (GTK_OBJECT (ctree), "end_selection",
				GTK_SIGNAL_FUNC (after_press), NULL);
      gtk_signal_connect_after (GTK_OBJECT (ctree), "toggle_focus_row",
				GTK_SIGNAL_FUNC (after_press), NULL);
      gtk_signal_connect_after (GTK_OBJECT (ctree), "select_all",
				GTK_SIGNAL_FUNC (after_press), NULL);
      gtk_signal_connect_after (GTK_OBJECT (ctree), "unselect_all",
				GTK_SIGNAL_FUNC (after_press), NULL);
      gtk_signal_connect_after (GTK_OBJECT (ctree), "scroll_vertical",
				GTK_SIGNAL_FUNC (after_press), NULL);

      bbox = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (bbox), 5);
      gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, TRUE, 0);

      mbox = gtk_vbox_new (TRUE, 5);
      gtk_box_pack_start (GTK_BOX (bbox), mbox, FALSE, TRUE, 0);

      label = gtk_label_new ("Row Height :");
      gtk_box_pack_start (GTK_BOX (mbox), label, FALSE, FALSE, 0);

      label = gtk_label_new ("Indent :");
      gtk_box_pack_start (GTK_BOX (mbox), label, FALSE, FALSE, 0);

      label = gtk_label_new ("Spacing :");
      gtk_box_pack_start (GTK_BOX (mbox), label, FALSE, FALSE, 0);

      mbox = gtk_vbox_new (TRUE, 5);
      gtk_box_pack_start (GTK_BOX (bbox), mbox, FALSE, TRUE, 0);

      adj = (GtkAdjustment *) gtk_adjustment_new (20, 12, 100, 1, 10, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (mbox), spinner, FALSE, FALSE, 5);
      gtk_tooltips_set_tip (tooltips, spinner,
			    "Row height of list items", NULL);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (change_row_height), ctree);
      gtk_clist_set_row_height ( GTK_CLIST (ctree), adj->value);

      adj = (GtkAdjustment *) gtk_adjustment_new (20, 0, 60, 1, 10, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (mbox), spinner, FALSE, FALSE, 5);
      gtk_tooltips_set_tip (tooltips, spinner, "Tree Indentation.", NULL);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (change_indent), ctree);

      adj = (GtkAdjustment *) gtk_adjustment_new (5, 0, 60, 1, 10, 0);
      spinner = gtk_spin_button_new (adj, 0, 0);
      gtk_box_pack_start (GTK_BOX (mbox), spinner, FALSE, FALSE, 5);
      gtk_tooltips_set_tip (tooltips, spinner, "Tree Spacing.", NULL);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (change_spacing), ctree);

      mbox = gtk_vbox_new (TRUE, 5);
      gtk_box_pack_start (GTK_BOX (bbox), mbox, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (mbox), hbox, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Expand All");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (expand_all), ctree);

      button = gtk_button_new_with_label ("Collapse All");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (collapse_all), ctree);

      button = gtk_button_new_with_label ("Change Style");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (change_style), ctree);

      button = gtk_button_new_with_label ("Export Tree");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (export_ctree), ctree);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_box_pack_start (GTK_BOX (mbox), hbox, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Select All");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (select_all), ctree);

      button = gtk_button_new_with_label ("Unselect All");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (unselect_all), ctree);

      button = gtk_button_new_with_label ("Remove Selection");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (remove_selection), ctree);

      check = gtk_check_button_new_with_label ("Reorderable");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, check,
			    "Tree items can be reordered by dragging.", NULL);
      gtk_signal_connect (GTK_OBJECT (check), "clicked",
			  GTK_SIGNAL_FUNC (toggle_reorderable), ctree);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

      hbox = gtk_hbox_new (TRUE, 5);
      gtk_box_pack_start (GTK_BOX (mbox), hbox, FALSE, FALSE, 0);

      omenu1 = build_option_menu (items1, 4, 2, 
				  ctree_toggle_line_style,
				  ctree);
      gtk_box_pack_start (GTK_BOX (hbox), omenu1, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, omenu1, "The tree's line style.", NULL);

      omenu2 = build_option_menu (items2, 4, 1, 
				  ctree_toggle_expander_style,
				  ctree);
      gtk_box_pack_start (GTK_BOX (hbox), omenu2, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, omenu2, "The tree's expander style.",
			    NULL);

      omenu3 = build_option_menu (items3, 2, 0, 
				  ctree_toggle_justify, ctree);
      gtk_box_pack_start (GTK_BOX (hbox), omenu3, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, omenu3, "The tree's justification.",
			    NULL);

      omenu4 = build_option_menu (items4, 3, 3, 
				  ctree_toggle_sel_mode, ctree);
      gtk_box_pack_start (GTK_BOX (hbox), omenu4, FALSE, TRUE, 0);
      gtk_tooltips_set_tip (tooltips, omenu4, "The list's selection mode.",
			    NULL);

      gtk_widget_realize (window);

      if (!pixmap1)
	pixmap1 = gdk_pixmap_create_from_xpm_d (window->window, &mask1, 
						&transparent, book_closed_xpm);
      if (!pixmap2)
	pixmap2 = gdk_pixmap_create_from_xpm_d (window->window, &mask2, 
						&transparent, book_open_xpm);
      if (!pixmap3)
	pixmap3 = gdk_pixmap_create_from_xpm_d (window->window, &mask3,
						&transparent, mini_page_xpm);

      gtk_widget_set_usize (GTK_WIDGET (ctree), 0, 300);

      frame = gtk_frame_new (NULL);
      gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

      hbox = gtk_hbox_new (TRUE, 2);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

      hbox2 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox2);

      label = gtk_label_new ("Books :");
      gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

      sprintf (buf, "%d", books);
      book_label = gtk_label_new (buf);
      gtk_box_pack_end (GTK_BOX (hbox2), book_label, FALSE, TRUE, 5);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

      hbox2 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox2);

      label = gtk_label_new ("Pages :");
      gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

      sprintf (buf, "%d", pages);
      page_label = gtk_label_new (buf);
      gtk_box_pack_end (GTK_BOX (hbox2), page_label, FALSE, TRUE, 5);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

      hbox2 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox2);

      label = gtk_label_new ("Selected :");
      gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

      sprintf (buf, "%d", g_list_length (GTK_CLIST (ctree)->selection));
      sel_label = gtk_label_new (buf);
      gtk_box_pack_end (GTK_BOX (hbox2), sel_label, FALSE, TRUE, 5);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, TRUE, 0);

      hbox2 = gtk_hbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox2), 2);
      gtk_container_add (GTK_CONTAINER (frame), hbox2);

      label = gtk_label_new ("Visible :");
      gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

      sprintf (buf, "%d", g_list_length (GTK_CLIST (ctree)->row_list));
      vis_label = gtk_label_new (buf);
      gtk_box_pack_end (GTK_BOX (hbox2), vis_label, FALSE, TRUE, 5);

      rebuild_tree (NULL, ctree);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkColorSelection
 */

void
color_selection_ok (GtkWidget               *w,
                    GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;
  gdouble color[4];

  colorsel=GTK_COLOR_SELECTION(cs->colorsel);

  gtk_color_selection_get_color(colorsel,color);
  gtk_color_selection_set_color(colorsel,color);
}

void
color_selection_changed (GtkWidget *w,
                         GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;
  gdouble color[4];

  colorsel=GTK_COLOR_SELECTION(cs->colorsel);
  gtk_color_selection_get_color(colorsel,color);
}

static void
opacity_toggled_cb (GtkWidget *w,
		    GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;

  colorsel = GTK_COLOR_SELECTION (cs->colorsel);
  gtk_color_selection_set_has_opacity_control (colorsel,
					       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)));
}

static void
palette_toggled_cb (GtkWidget *w,
		    GtkColorSelectionDialog *cs)
{
  GtkColorSelection *colorsel;

  colorsel = GTK_COLOR_SELECTION (cs->colorsel);
  gtk_color_selection_set_has_palette (colorsel,
				       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)));
}

void
create_color_selection (void)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *options_hbox;
      GtkWidget *check_button;
      
      window = gtk_color_selection_dialog_new ("color selection dialog");
      gtk_widget_show (GTK_COLOR_SELECTION_DIALOG (window)->help_button);

      gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
                          GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                          &window);

      options_hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), options_hbox, FALSE, FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (options_hbox), 10);
      
      check_button = gtk_check_button_new_with_label ("Show Opacity");
      gtk_box_pack_start (GTK_BOX (options_hbox), check_button, FALSE, FALSE, 0);
      gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
			  GTK_SIGNAL_FUNC (opacity_toggled_cb), window);

      check_button = gtk_check_button_new_with_label ("Show Palette");
      gtk_box_pack_end (GTK_BOX (options_hbox), check_button, FALSE, FALSE, 0);
      gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
			  GTK_SIGNAL_FUNC (palette_toggled_cb), window);

      gtk_widget_show_all (options_hbox);

      gtk_signal_connect (
	GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG (window)->colorsel),
	"color_changed",
	GTK_SIGNAL_FUNC(color_selection_changed),
	window);

      gtk_signal_connect (
	GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG (window)->ok_button),
	"clicked",
	GTK_SIGNAL_FUNC(color_selection_ok),
	window);

      gtk_signal_connect_object (
        GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG (window)->cancel_button),
	"clicked",
	GTK_SIGNAL_FUNC(gtk_widget_destroy),
	GTK_OBJECT (window));
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkFileSelection
 */

void
file_selection_hide_fileops (GtkWidget *widget,
			     GtkFileSelection *fs)
{
  gtk_file_selection_hide_fileop_buttons (fs);
}

void
file_selection_ok (GtkWidget        *w,
		   GtkFileSelection *fs)
{
  g_print ("%s\n", gtk_file_selection_get_filename (fs));
  gtk_widget_destroy (GTK_WIDGET (fs));
}

void
create_file_selection (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_file_selection_new ("file selection dialog");

      gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (window));

      gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (window)->ok_button),
			  "clicked", GTK_SIGNAL_FUNC(file_selection_ok),
			  window);
      gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (window)->cancel_button),
				 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      
      button = gtk_button_new_with_label ("Hide Fileops");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  (GtkSignalFunc) file_selection_hide_fileops, 
			  (gpointer) window);
      gtk_box_pack_start (GTK_BOX (GTK_FILE_SELECTION (window)->action_area), 
			  button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Show Fileops");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 (GtkSignalFunc) gtk_file_selection_show_fileop_buttons, 
				 (gpointer) window);
      gtk_box_pack_start (GTK_BOX (GTK_FILE_SELECTION (window)->action_area), 
			  button, FALSE, FALSE, 0);
      gtk_widget_show (button);
    }
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

void
flipping_toggled_cb (GtkWidget *widget, gpointer data)
{
  int state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  int new_direction = state ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;

  gtk_widget_set_default_direction (new_direction);
}

void
create_flipping (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *check_button, *button;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Bidirectional Flipping");

      check_button = gtk_check_button_new_with_label ("Right-to-left global direction");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  check_button, TRUE, TRUE, 0);

      if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), TRUE);

      gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
			  GTK_SIGNAL_FUNC (flipping_toggled_cb), FALSE);

      gtk_container_set_border_width (GTK_CONTAINER (check_button), 10);
      
      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
    }
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Focus test
 */

static GtkWidget*
make_focus_table (GList **list)
{
  GtkWidget *table;
  gint i, j;
  
  table = gtk_table_new (5, 5, FALSE);

  i = 0;
  j = 0;

  while (i < 5)
    {
      j = 0;
      while (j < 5)
        {
          GtkWidget *widget;
          
          if ((i + j) % 2)
            widget = gtk_entry_new ();
          else
            widget = gtk_button_new_with_label ("Foo");

          *list = g_list_prepend (*list, widget);
          
          gtk_table_attach (GTK_TABLE (table),
                            widget,
                            i, i + 1,
                            j, j + 1,
                            GTK_EXPAND | GTK_FILL,
                            GTK_EXPAND | GTK_FILL,
                            5, 5);
          
          ++j;
        }

      ++i;
    }

  *list = g_list_reverse (*list);
  
  return table;
}

static void
create_focus (void)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *table;
      GtkWidget *frame;
      GList *list = NULL;
      
      window = gtk_dialog_new_with_buttons ("Keyboard focus navigation",
                                            NULL, 0,
                                            GTK_STOCK_CLOSE,
                                            GTK_RESPONSE_NONE,
                                            NULL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);

      gtk_signal_connect (GTK_OBJECT (window), "response",
                          GTK_SIGNAL_FUNC (gtk_widget_destroy),
                          NULL);
      
      gtk_window_set_title (GTK_WINDOW (window), "Keyboard Focus Navigation");

      frame = gtk_frame_new ("Weird tab focus chain");

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  frame, TRUE, TRUE, 0);
      
      table = make_focus_table (&list);

      gtk_container_add (GTK_CONTAINER (frame), table);

      gtk_container_set_focus_chain (GTK_CONTAINER (table),
                                     list);

      g_list_free (list);
      
      frame = gtk_frame_new ("Default tab focus chain");

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  frame, TRUE, TRUE, 0);

      list = NULL;
      table = make_focus_table (&list);

      g_list_free (list);
      
      gtk_container_add (GTK_CONTAINER (frame), table);      
    }
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkFontSelection
 */

void
font_selection_ok (GtkWidget              *w,
		   GtkFontSelectionDialog *fs)
{
  gchar *s = gtk_font_selection_dialog_get_font_name (fs);

  g_print ("%s\n", s);
  g_free (s);
  gtk_widget_destroy (GTK_WIDGET (fs));
}

void
create_font_selection (void)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      window = gtk_font_selection_dialog_new ("Font Selection Dialog");

      gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_signal_connect (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG (window)->ok_button),
			  "clicked", GTK_SIGNAL_FUNC(font_selection_ok),
			  GTK_FONT_SELECTION_DIALOG (window));
      gtk_signal_connect_object (GTK_OBJECT (GTK_FONT_SELECTION_DIALOG (window)->cancel_button),
				 "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
    }
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkDialog
 */

static GtkWidget *dialog_window = NULL;

static void
label_toggle (GtkWidget  *widget,
	      GtkWidget **label)
{
  if (!(*label))
    {
      *label = gtk_label_new ("Dialog Test");
      gtk_signal_connect (GTK_OBJECT (*label),
			  "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  label);
      gtk_misc_set_padding (GTK_MISC (*label), 10, 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->vbox), 
			  *label, TRUE, TRUE, 0);
      gtk_widget_show (*label);
    }
  else
    gtk_widget_destroy (*label);
}

#define RESPONSE_TOGGLE_SEPARATOR 1

static void
print_response (GtkWidget *dialog,
                gint       response_id,
                gpointer   data)
{
  g_print ("response signal received (%d)\n", response_id);

  if (response_id == RESPONSE_TOGGLE_SEPARATOR)
    {
      gtk_dialog_set_has_separator (GTK_DIALOG (dialog),
                                    !gtk_dialog_get_has_separator (GTK_DIALOG (dialog)));
    }
}

static void
create_dialog (void)
{
  static GtkWidget *label;
  GtkWidget *button;

  if (!dialog_window)
    {
      /* This is a terrible example; it's much simpler to create
       * dialogs than this. Don't use testgtk for example code,
       * use gtk-demo ;-)
       */
      
      dialog_window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (dialog_window),
                          "response",
                          GTK_SIGNAL_FUNC (print_response),
                          NULL);
      
      gtk_signal_connect (GTK_OBJECT (dialog_window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &dialog_window);

      gtk_window_set_title (GTK_WINDOW (dialog_window), "GtkDialog");
      gtk_container_set_border_width (GTK_CONTAINER (dialog_window), 0);

      button = gtk_button_new_with_label ("OK");
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Toggle");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (label_toggle),
			  &label);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->action_area),
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      label = NULL;
      
      button = gtk_button_new_with_label ("Separator");

      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

      gtk_dialog_add_action_widget (GTK_DIALOG (dialog_window),
                                    button,
                                    RESPONSE_TOGGLE_SEPARATOR);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (dialog_window))
    gtk_widget_show (dialog_window);
  else
    gtk_widget_destroy (dialog_window);
}

/* Event Watcher
 */
static gboolean event_watcher_enter_id = 0;
static gboolean event_watcher_leave_id = 0;

static gboolean
event_watcher (GSignalInvocationHint *ihint,
	       guint                  n_param_values,
	       const GValue          *param_values,
	       gpointer               data)
{
  g_print ("Watch: \"%s\" emitted for %s\n",
	   gtk_signal_name (ihint->signal_id),
	   gtk_type_name (GTK_OBJECT_TYPE (g_value_get_object (param_values + 0))));

  return TRUE;
}

static void
event_watcher_down (void)
{
  if (event_watcher_enter_id)
    {
      guint signal_id;

      signal_id = gtk_signal_lookup ("enter_notify_event", GTK_TYPE_WIDGET);
      g_signal_remove_emission_hook (signal_id, event_watcher_enter_id);
      event_watcher_enter_id = 0;
      signal_id = gtk_signal_lookup ("leave_notify_event", GTK_TYPE_WIDGET);
      g_signal_remove_emission_hook (signal_id, event_watcher_leave_id);
      event_watcher_leave_id = 0;
    }
}

static void
event_watcher_toggle (void)
{
  if (event_watcher_enter_id)
    event_watcher_down ();
  else
    {
      guint signal_id;

      signal_id = gtk_signal_lookup ("enter_notify_event", GTK_TYPE_WIDGET);
      event_watcher_enter_id = g_signal_add_emission_hook (signal_id, 0, event_watcher, NULL, NULL);
      signal_id = gtk_signal_lookup ("leave_notify_event", GTK_TYPE_WIDGET);
      event_watcher_leave_id = g_signal_add_emission_hook (signal_id, 0, event_watcher, NULL, NULL);
    }
}

static void
create_event_watcher (void)
{
  GtkWidget *button;

  if (!dialog_window)
    {
      dialog_window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (dialog_window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &dialog_window);
      gtk_signal_connect (GTK_OBJECT (dialog_window),
			  "destroy",
			  GTK_SIGNAL_FUNC (event_watcher_down),
			  NULL);

      gtk_window_set_title (GTK_WINDOW (dialog_window), "Event Watcher");
      gtk_container_set_border_width (GTK_CONTAINER (dialog_window), 0);
      gtk_widget_set_usize (dialog_window, 200, 110);

      button = gtk_toggle_button_new_with_label ("Activate Watch");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (event_watcher_toggle),
			  NULL);
      gtk_container_set_border_width (GTK_CONTAINER (button), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->vbox), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 (GtkObject*) dialog_window);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_window)->action_area),
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (dialog_window))
    gtk_widget_show (dialog_window);
  else
    gtk_widget_destroy (dialog_window);
}

/*
 * GtkRange
 */

static gchar*
reformat_value (GtkScale *scale,
                gdouble   value)
{
  return g_strdup_printf ("-->%0.*g<--",
                          gtk_scale_get_digits (scale), value);
}

static void
create_range_controls (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *scrollbar;
  GtkWidget *scale;
  GtkWidget *separator;
  GtkObject *adjustment;
  GtkWidget *hbox;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "range controls");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      adjustment = gtk_adjustment_new (0.0, 0.0, 101.0, 0.1, 1.0, 1.0);

      scale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_usize (GTK_WIDGET (scale), 150, -1);
      gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
      gtk_scale_set_digits (GTK_SCALE (scale), 1);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (adjustment));
      gtk_range_set_update_policy (GTK_RANGE (scrollbar), 
				   GTK_UPDATE_CONTINUOUS);
      gtk_box_pack_start (GTK_BOX (box2), scrollbar, TRUE, TRUE, 0);
      gtk_widget_show (scrollbar);

      scale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_signal_connect (GTK_OBJECT (scale),
                          "format_value",
                          GTK_SIGNAL_FUNC (reformat_value),
                          NULL);
      gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);
      
      hbox = gtk_hbox_new (FALSE, 0);

      scale = gtk_vscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_usize (scale, -1, 200);
      gtk_scale_set_digits (GTK_SCALE (scale), 2);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scale = gtk_vscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_widget_set_usize (scale, -1, 200);
      gtk_scale_set_digits (GTK_SCALE (scale), 2);
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_range_set_inverted (GTK_RANGE (scale), TRUE);
      gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      scale = gtk_vscale_new (GTK_ADJUSTMENT (adjustment));
      gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
      gtk_signal_connect (GTK_OBJECT (scale),
                          "format_value",
                          GTK_SIGNAL_FUNC (reformat_value),
                          NULL);
      gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
      gtk_widget_show (scale);

      
      gtk_box_pack_start (GTK_BOX (box2), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);
      
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
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkRulers
 */

void
create_rulers (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *table;
  GtkWidget *ruler;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "rulers");
      gtk_widget_set_usize (window, 300, 300);
      gtk_widget_set_events (window, 
			     GDK_POINTER_MOTION_MASK 
			     | GDK_POINTER_MOTION_HINT_MASK);
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      table = gtk_table_new (2, 2, FALSE);
      gtk_container_add (GTK_CONTAINER (window), table);
      gtk_widget_show (table);

      ruler = gtk_hruler_new ();
      gtk_ruler_set_metric (GTK_RULER (ruler), GTK_CENTIMETERS);
      gtk_ruler_set_range (GTK_RULER (ruler), 100, 0, 0, 20);

      gtk_signal_connect_object (GTK_OBJECT (window), 
				 "motion_notify_event",
				 GTK_SIGNAL_FUNC (GTK_WIDGET_GET_CLASS (ruler)->motion_notify_event),
				 GTK_OBJECT (ruler));
      
      gtk_table_attach (GTK_TABLE (table), ruler, 1, 2, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (ruler);


      ruler = gtk_vruler_new ();
      gtk_ruler_set_range (GTK_RULER (ruler), 5, 15, 0, 20);

      gtk_signal_connect_object (GTK_OBJECT (window), 
				 "motion_notify_event",
				 GTK_SIGNAL_FUNC (GTK_WIDGET_GET_CLASS (ruler)->motion_notify_event),
				 GTK_OBJECT (ruler));
      
      gtk_table_attach (GTK_TABLE (table), ruler, 0, 1, 1, 2,
			GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (ruler);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

static void
text_toggle_editable (GtkWidget *checkbutton,
		       GtkWidget *text)
{
   gtk_text_set_editable(GTK_TEXT(text),
			  GTK_TOGGLE_BUTTON(checkbutton)->active);
}

static void
text_toggle_word_wrap (GtkWidget *checkbutton,
		       GtkWidget *text)
{
   gtk_text_set_word_wrap(GTK_TEXT(text),
			  GTK_TOGGLE_BUTTON(checkbutton)->active);
}

struct {
  GdkColor color;
  gchar *name;
} text_colors[] = {
 { { 0, 0x0000, 0x0000, 0x0000 }, "black" },
 { { 0, 0xFFFF, 0xFFFF, 0xFFFF }, "white" },
 { { 0, 0xFFFF, 0x0000, 0x0000 }, "red" },
 { { 0, 0x0000, 0xFFFF, 0x0000 }, "green" },
 { { 0, 0x0000, 0x0000, 0xFFFF }, "blue" }, 
 { { 0, 0x0000, 0xFFFF, 0xFFFF }, "cyan" },
 { { 0, 0xFFFF, 0x0000, 0xFFFF }, "magenta" },
 { { 0, 0xFFFF, 0xFFFF, 0x0000 }, "yellow" }
};

int ntext_colors = sizeof(text_colors) / sizeof(text_colors[0]);

/*
 * GtkText
 */
void
text_insert_random (GtkWidget *w, GtkText *text)
{
  int i;
  char c;
   for (i=0; i<10; i++)
    {
      c = 'A' + rand() % ('Z' - 'A');
      gtk_text_set_point (text, rand() % gtk_text_get_length (text));
      gtk_text_insert (text, NULL, NULL, NULL, &c, 1);
    }
}

void
create_text (void)
{
  int i, j;

  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *check;
  GtkWidget *separator;
  GtkWidget *scrolled_window;
  GtkWidget *text;

  FILE *infile;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_widget_set_name (window, "text window");
      gtk_widget_set_usize (window, 500, 500);
      gtk_window_set_policy (GTK_WINDOW(window), TRUE, TRUE, FALSE);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);


      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);


      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_box_pack_start (GTK_BOX (box2), scrolled_window, TRUE, TRUE, 0);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				      GTK_POLICY_NEVER,
				      GTK_POLICY_ALWAYS);
      gtk_widget_show (scrolled_window);

      text = gtk_text_new (NULL, NULL);
      gtk_text_set_editable (GTK_TEXT (text), TRUE);
      gtk_container_add (GTK_CONTAINER (scrolled_window), text);
      gtk_widget_grab_focus (text);
      gtk_widget_show (text);


      gtk_text_freeze (GTK_TEXT (text));

      for (i=0; i<ntext_colors; i++)
	{
	  gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, 
			   text_colors[i].name, -1);
	  gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, "\t", -1);

	  for (j=0; j<ntext_colors; j++)
	    {
	      gtk_text_insert (GTK_TEXT (text), NULL,
			       &text_colors[j].color, &text_colors[i].color,
			       "XYZ", -1);
	    }
	  gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, "\n", -1);
	}

      infile = fopen("testgtk.c", "r");
      
      if (infile)
	{
	  char *buffer;
	  int nbytes_read, nbytes_alloc;
	  
	  nbytes_read = 0;
	  nbytes_alloc = 1024;
	  buffer = g_new (char, nbytes_alloc);
	  while (1)
	    {
	      int len;
	      if (nbytes_alloc < nbytes_read + 1024)
		{
		  nbytes_alloc *= 2;
		  buffer = g_realloc (buffer, nbytes_alloc);
		}
	      len = fread (buffer + nbytes_read, 1, 1024, infile);
	      nbytes_read += len;
	      if (len < 1024)
		break;
	    }
	  
	  gtk_text_insert (GTK_TEXT (text), NULL, NULL,
			   NULL, buffer, nbytes_read);
	  g_free(buffer);
	  fclose (infile);
	}
      
      gtk_text_thaw (GTK_TEXT (text));

      hbox = gtk_hbutton_box_new ();
      gtk_box_pack_start (GTK_BOX (box2), hbox, FALSE, FALSE, 0);
      gtk_widget_show (hbox);

      check = gtk_check_button_new_with_label("Editable");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
      gtk_signal_connect (GTK_OBJECT(check), "toggled",
			  GTK_SIGNAL_FUNC(text_toggle_editable), text);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
      gtk_widget_show (check);

      check = gtk_check_button_new_with_label("Wrap Words");
      gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT(check), "toggled",
			  GTK_SIGNAL_FUNC(text_toggle_word_wrap), text);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), FALSE);
      gtk_widget_show (check);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);


      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);


      button = gtk_button_new_with_label ("insert random");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(text_insert_random),
			  GTK_TEXT (text));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_show (button);

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
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkNotebook
 */

GdkPixmap *book_open;
GdkPixmap *book_closed;
GdkBitmap *book_open_mask;
GdkBitmap *book_closed_mask;
GtkWidget *sample_notebook;

static void
set_page_pixmaps (GtkNotebook *notebook, gint page_num,
		  GdkPixmap *pixmap, GdkPixmap *mask)
{
  GtkWidget *page_widget;
  GtkWidget *pixwid;

  page_widget = gtk_notebook_get_nth_page (notebook, page_num);

  pixwid = gtk_object_get_data (GTK_OBJECT (page_widget), "tab_pixmap");
  gtk_pixmap_set (GTK_PIXMAP (pixwid), pixmap, mask);
  
  pixwid = gtk_object_get_data (GTK_OBJECT (page_widget), "menu_pixmap");
  gtk_pixmap_set (GTK_PIXMAP (pixwid), pixmap, mask);
}

static void
page_switch (GtkWidget *widget, GtkNotebookPage *page, gint page_num)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  gint old_page_num = gtk_notebook_get_current_page (notebook);
 
  if (page_num == old_page_num)
    return;

  set_page_pixmaps (notebook, page_num, book_open, book_open_mask);

  if (old_page_num != -1)
    set_page_pixmaps (notebook, old_page_num, book_closed, book_closed_mask);
}

static void
tab_fill (GtkToggleButton *button, GtkWidget *child)
{
  gboolean expand;
  GtkPackType pack_type;

  gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
					&expand, NULL, &pack_type);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
				      expand, button->active, pack_type);
}

static void
tab_expand (GtkToggleButton *button, GtkWidget *child)
{
  gboolean fill;
  GtkPackType pack_type;

  gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
					NULL, &fill, &pack_type);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
				      button->active, fill, pack_type);
}

static void
tab_pack (GtkToggleButton *button, GtkWidget *child)
	  
{ 
  gboolean expand;
  gboolean fill;

  gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
					&expand, &fill, NULL);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (sample_notebook), child,
				      expand, fill, button->active);
}

static void
create_pages (GtkNotebook *notebook, gint start, gint end)
{
  GtkWidget *child = NULL;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *label_box;
  GtkWidget *menu_box;
  GtkWidget *pixwid;
  gint i;
  char buffer[32];
  char accel_buffer[32];

  for (i = start; i <= end; i++)
    {
      sprintf (buffer, "Page %d", i);
      sprintf (accel_buffer, "Page _%d", i);

      child = gtk_frame_new (buffer);
      gtk_container_set_border_width (GTK_CONTAINER (child), 10);

      vbox = gtk_vbox_new (TRUE,0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_container_add (GTK_CONTAINER (child), vbox);

      hbox = gtk_hbox_new (TRUE,0);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);

      button = gtk_check_button_new_with_label ("Fill Tab");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_signal_connect (GTK_OBJECT (button), "toggled",
			  GTK_SIGNAL_FUNC (tab_fill), child);

      button = gtk_check_button_new_with_label ("Expand Tab");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
      gtk_signal_connect (GTK_OBJECT (button), "toggled",
      GTK_SIGNAL_FUNC (tab_expand), child);

      button = gtk_check_button_new_with_label ("Pack end");
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
      gtk_signal_connect (GTK_OBJECT (button), "toggled",
			  GTK_SIGNAL_FUNC (tab_pack), child);

      button = gtk_button_new_with_label ("Hide Page");
      gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 5);
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_hide),
				 GTK_OBJECT (child));

      gtk_widget_show_all (child);

      label_box = gtk_hbox_new (FALSE, 0);
      pixwid = gtk_pixmap_new (book_closed, book_closed_mask);
      gtk_object_set_data (GTK_OBJECT (child), "tab_pixmap", pixwid);
			   
      gtk_box_pack_start (GTK_BOX (label_box), pixwid, FALSE, TRUE, 0);
      gtk_misc_set_padding (GTK_MISC (pixwid), 3, 1);
      label = gtk_label_new_with_mnemonic (accel_buffer);
      gtk_box_pack_start (GTK_BOX (label_box), label, FALSE, TRUE, 0);
      gtk_widget_show_all (label_box);
      
				       
      menu_box = gtk_hbox_new (FALSE, 0);
      pixwid = gtk_pixmap_new (book_closed, book_closed_mask);
      gtk_object_set_data (GTK_OBJECT (child), "menu_pixmap", pixwid);
      
      gtk_box_pack_start (GTK_BOX (menu_box), pixwid, FALSE, TRUE, 0);
      gtk_misc_set_padding (GTK_MISC (pixwid), 3, 1);
      label = gtk_label_new (buffer);
      gtk_box_pack_start (GTK_BOX (menu_box), label, FALSE, TRUE, 0);
      gtk_widget_show_all (menu_box);

      gtk_notebook_append_page_menu (notebook, child, label_box, menu_box);
    }
}

static void
rotate_notebook (GtkButton   *button,
		 GtkNotebook *notebook)
{
  gtk_notebook_set_tab_pos (notebook, (notebook->tab_pos + 1) % 4);
}

static void
show_all_pages (GtkButton   *button,
		GtkNotebook *notebook)
{  
  gtk_container_foreach (GTK_CONTAINER (notebook),
			 (GtkCallback) gtk_widget_show, NULL);
}

static void
notebook_type_changed (GtkWidget *optionmenu,
		       gpointer   data)
{
  GtkNotebook *notebook;
  gint i, c;

  enum {
    STANDARD,
    NOTABS,
    BORDERLESS,
    SCROLLABLE
  };

  notebook = GTK_NOTEBOOK (data);

  c = gtk_option_menu_get_history (GTK_OPTION_MENU (optionmenu));

  switch (c)
    {
    case STANDARD:
      /* standard notebook */
      gtk_notebook_set_show_tabs (notebook, TRUE);
      gtk_notebook_set_show_border (notebook, TRUE);
      gtk_notebook_set_scrollable (notebook, FALSE);
      break;

    case NOTABS:
      /* notabs notebook */
      gtk_notebook_set_show_tabs (notebook, FALSE);
      gtk_notebook_set_show_border (notebook, TRUE);
      break;

    case BORDERLESS:
      /* borderless */
      gtk_notebook_set_show_tabs (notebook, FALSE);
      gtk_notebook_set_show_border (notebook, FALSE);
      break;

    case SCROLLABLE:  
      /* scrollable */
      gtk_notebook_set_show_tabs (notebook, TRUE);
      gtk_notebook_set_show_border (notebook, TRUE);
      gtk_notebook_set_scrollable (notebook, TRUE);
      if (g_list_length (notebook->children) == 5)
	create_pages (notebook, 6, 15);
      
      return;
      break;
    }
  
  if (g_list_length (notebook->children) == 15)
    for (i = 0; i < 10; i++)
      gtk_notebook_remove_page (notebook, 5);
}

static void
notebook_popup (GtkToggleButton *button,
		GtkNotebook     *notebook)
{
  if (button->active)
    gtk_notebook_popup_enable (notebook);
  else
    gtk_notebook_popup_disable (notebook);
}

static void
notebook_homogeneous (GtkToggleButton *button,
		      GtkNotebook     *notebook)
{
  gtk_notebook_set_homogeneous_tabs (notebook, button->active);
}

static void
create_notebook (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *omenu;
  GdkColor *transparent = NULL;
  GtkWidget *label;

  static gchar *items[] =
  {
    "Standard",
    "No tabs",
    "Borderless",
    "Scrollable"
  };

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "notebook");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      sample_notebook = gtk_notebook_new ();
      gtk_signal_connect (GTK_OBJECT (sample_notebook), "switch_page",
			  GTK_SIGNAL_FUNC (page_switch), NULL);
      gtk_notebook_set_tab_pos (GTK_NOTEBOOK (sample_notebook), GTK_POS_TOP);
      gtk_box_pack_start (GTK_BOX (box1), sample_notebook, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (sample_notebook), 10);

      gtk_widget_realize (sample_notebook);
      if (!book_open)
	book_open = gdk_pixmap_create_from_xpm_d (sample_notebook->window,
						  &book_open_mask, 
						  transparent, 
						  book_open_xpm);
      if (!book_closed)
	book_closed = gdk_pixmap_create_from_xpm_d (sample_notebook->window,
						    &book_closed_mask,
						    transparent, 
						    book_closed_xpm);

      create_pages (GTK_NOTEBOOK (sample_notebook), 1, 5);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 10);
      
      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_check_button_new_with_label ("popup menu");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, FALSE, 0);
      gtk_signal_connect (GTK_OBJECT(button), "clicked",
			  GTK_SIGNAL_FUNC (notebook_popup),
			  GTK_OBJECT (sample_notebook));

      button = gtk_check_button_new_with_label ("homogeneous tabs");
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, FALSE, 0);
      gtk_signal_connect (GTK_OBJECT(button), "clicked",
			  GTK_SIGNAL_FUNC (notebook_homogeneous),
			  GTK_OBJECT (sample_notebook));

      box2 = gtk_hbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      label = gtk_label_new ("Notebook Style :");
      gtk_box_pack_start (GTK_BOX (box2), label, FALSE, TRUE, 0);

      omenu = build_option_menu (items, G_N_ELEMENTS (items), 0,
				 notebook_type_changed,
				 sample_notebook);
      gtk_box_pack_start (GTK_BOX (box2), omenu, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("Show all Pages");
      gtk_box_pack_start (GTK_BOX (box2), button, FALSE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (show_all_pages), sample_notebook);

      box2 = gtk_hbox_new (TRUE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("prev");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_notebook_prev_page),
				 GTK_OBJECT (sample_notebook));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("next");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_notebook_next_page),
				 GTK_OBJECT (sample_notebook));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("rotate");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (rotate_notebook), sample_notebook);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);

      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 5);

      button = gtk_button_new_with_label ("close");
      gtk_container_set_border_width (GTK_CONTAINER (button), 5);
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box1), button, FALSE, FALSE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkPanes
 */

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

static void
paned_props_clicked (GtkWidget *button,
		     GObject   *paned)
{
  GtkWidget *window = create_prop_editor (paned, GTK_TYPE_PANED);

  gtk_window_set_title (GTK_WINDOW (window), "Paned Properties");
}

GtkWidget *
create_pane_options (GtkPaned    *paned,
		     const gchar *frame_label,
		     const gchar *label1,
		     const gchar *label2)
{
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *check_button;
  
  frame = gtk_frame_new (frame_label);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  
  table = gtk_table_new (4, 2, 4);
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

  button = gtk_button_new_with_mnemonic ("_Properties");
  gtk_table_attach_defaults (GTK_TABLE (table), button,
			     0, 2, 3, 4);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (paned_props_clicked),
		      paned);

  return frame;
}

void
create_panes (void)
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
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Shaped Windows
 */

typedef struct _cursoroffset {gint x,y;} CursorOffset;

static void
shape_pressed (GtkWidget *widget, GdkEventButton *event)
{
  CursorOffset *p;

  /* ignore double and triple click */
  if (event->type != GDK_BUTTON_PRESS)
    return;

  p = gtk_object_get_user_data (GTK_OBJECT(widget));
  p->x = (int) event->x;
  p->y = (int) event->y;

  gtk_grab_add (widget);
  gdk_pointer_grab (widget->window, TRUE,
		    GDK_BUTTON_RELEASE_MASK |
		    GDK_BUTTON_MOTION_MASK |
		    GDK_POINTER_MOTION_HINT_MASK,
		    NULL, NULL, 0);
}

static void
shape_released (GtkWidget *widget)
{
  gtk_grab_remove (widget);
  gdk_pointer_ungrab (0);
}

static void
shape_motion (GtkWidget      *widget, 
	      GdkEventMotion *event)
{
  gint xp, yp;
  CursorOffset * p;
  GdkModifierType mask;

  p = gtk_object_get_user_data (GTK_OBJECT (widget));

  /*
   * Can't use event->x / event->y here 
   * because I need absolute coordinates.
   */
  gdk_window_get_pointer (NULL, &xp, &yp, &mask);
  gtk_widget_set_uposition (widget, xp  - p->x, yp  - p->y);
}

GtkWidget *
shape_create_icon (char     *xpm_file,
		   gint      x,
		   gint      y,
		   gint      px,
		   gint      py,
		   gint      window_type)
{
  GtkWidget *window;
  GtkWidget *pixmap;
  GtkWidget *fixed;
  CursorOffset* icon_pos;
  GdkGC* gc;
  GdkBitmap *gdk_pixmap_mask;
  GdkPixmap *gdk_pixmap;
  GtkStyle *style;

  style = gtk_widget_get_default_style ();
  gc = style->black_gc;	

  /*
   * GDK_WINDOW_TOPLEVEL works also, giving you a title border
   */
  window = gtk_window_new (window_type);
  
  fixed = gtk_fixed_new ();
  gtk_widget_set_usize (fixed, 100,100);
  gtk_container_add (GTK_CONTAINER (window), fixed);
  gtk_widget_show (fixed);
  
  gtk_widget_set_events (window, 
			 gtk_widget_get_events (window) |
			 GDK_BUTTON_MOTION_MASK |
			 GDK_POINTER_MOTION_HINT_MASK |
			 GDK_BUTTON_PRESS_MASK);

  gtk_widget_realize (window);
  gdk_pixmap = gdk_pixmap_create_from_xpm (window->window, &gdk_pixmap_mask, 
					   &style->bg[GTK_STATE_NORMAL],
					   xpm_file);

  pixmap = gtk_pixmap_new (gdk_pixmap, gdk_pixmap_mask);
  gtk_fixed_put (GTK_FIXED (fixed), pixmap, px,py);
  gtk_widget_show (pixmap);
  
  gtk_widget_shape_combine_mask (window, gdk_pixmap_mask, px, py);
  
  gdk_drawable_unref (gdk_pixmap_mask);
  gdk_drawable_unref (gdk_pixmap);

  gtk_signal_connect (GTK_OBJECT (window), "button_press_event",
		      GTK_SIGNAL_FUNC (shape_pressed),NULL);
  gtk_signal_connect (GTK_OBJECT (window), "button_release_event",
		      GTK_SIGNAL_FUNC (shape_released),NULL);
  gtk_signal_connect (GTK_OBJECT (window), "motion_notify_event",
		      GTK_SIGNAL_FUNC (shape_motion),NULL);

  icon_pos = g_new (CursorOffset, 1);
  gtk_object_set_user_data(GTK_OBJECT(window), icon_pos);

  gtk_widget_set_uposition (window, x, y);
  gtk_widget_show (window);
  
  return window;
}

void 
create_shapes (void)
{
  /* Variables used by the Drag/Drop and Shape Window demos */
  static GtkWidget *modeller = NULL;
  static GtkWidget *sheets = NULL;
  static GtkWidget *rings = NULL;
  static GtkWidget *with_region = NULL;
  
  if (!(file_exists ("Modeller.xpm") &&
	file_exists ("FilesQueue.xpm") &&
	file_exists ("3DRings.xpm")))
    return;
  

  if (!modeller)
    {
      modeller = shape_create_icon ("Modeller.xpm",
				    440, 140, 0,0, GTK_WINDOW_POPUP);

      gtk_signal_connect (GTK_OBJECT (modeller), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &modeller);
    }
  else
    gtk_widget_destroy (modeller);

  if (!sheets)
    {
      sheets = shape_create_icon ("FilesQueue.xpm",
				  580, 170, 0,0, GTK_WINDOW_POPUP);

      gtk_signal_connect (GTK_OBJECT (sheets), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &sheets);

    }
  else
    gtk_widget_destroy (sheets);

  if (!rings)
    {
      rings = shape_create_icon ("3DRings.xpm",
				 460, 270, 25,25, GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (rings), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &rings);
    }
  else
    gtk_widget_destroy (rings);

  if (!with_region)
    {
      GdkRegion *region;
      gint x, y;
      
      with_region = shape_create_icon ("3DRings.xpm",
                                       460, 270, 25,25, GTK_WINDOW_TOPLEVEL);

      gtk_window_set_decorated (GTK_WINDOW (with_region), FALSE);
      
      gtk_signal_connect (GTK_OBJECT (with_region), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &with_region);

      /* reset shape from mask to a region */
      x = 0;
      y = 0;
      region = gdk_region_new ();

      while (x < 460)
        {
          while (y < 270)
            {
              GdkRectangle rect;
              rect.x = x;
              rect.y = y;
              rect.width = 10;
              rect.height = 10;

              gdk_region_union_with_rect (region, &rect);
              
              y += 20;
            }
          y = 0;
          x += 20;
        }

      gdk_window_shape_combine_region (with_region->window,
                                       region,
                                       0, 0);
    }
  else
    gtk_widget_destroy (with_region);
}

/*
 * WM Hints demo
 */

void
create_wmhints (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *button;
  GtkWidget *box1;
  GtkWidget *box2;

  GdkBitmap *circles;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "WM Hints");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      gtk_widget_realize (window);
      
      circles = gdk_bitmap_create_from_data (window->window,
					     circles_bits,
					     circles_width,
					     circles_height);
      gdk_window_set_icon (window->window, NULL,
			   circles, circles);
      
      gdk_window_set_icon_name (window->window, "WMHints Test Icon");
  
      gdk_window_set_decorations (window->window, GDK_DECOR_ALL | GDK_DECOR_MENU);
      gdk_window_set_functions (window->window, GDK_FUNC_ALL | GDK_FUNC_RESIZE);
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);

      label = gtk_label_new ("Try iconizing me!");
      gtk_widget_set_usize (label, 150, 50);
      gtk_box_pack_start (GTK_BOX (box1), label, TRUE, TRUE, 0);
      gtk_widget_show (label);


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
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Window state tracking
 */

static gint
window_state_callback (GtkWidget *widget,
                       GdkEventWindowState *event,
                       gpointer data)
{
  GtkWidget *label = data;
  gchar *msg;

  msg = g_strconcat (GTK_WINDOW (widget)->title, ": ",
                     (event->new_window_state & GDK_WINDOW_STATE_WITHDRAWN) ?
                     "withdrawn" : "not withdrawn", ", ",
                     (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) ?
                     "iconified" : "not iconified", ", ",
                     (event->new_window_state & GDK_WINDOW_STATE_STICKY) ?
                     "sticky" : "not sticky", ", ",
                     (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) ?
                     "maximized" : "not maximized",
                     NULL);
  
  gtk_label_set_text (GTK_LABEL (label), msg);

  g_free (msg);

  return FALSE;
}

static GtkWidget*
tracking_label (GtkWidget *window)
{
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *button;

  hbox = gtk_hbox_new (FALSE, 5);

  gtk_signal_connect_object (GTK_OBJECT (hbox),
                             "destroy",
                             GTK_SIGNAL_FUNC (gtk_widget_destroy),
                             GTK_OBJECT (window));
  
  label = gtk_label_new ("<no window state events received>");
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  
  gtk_signal_connect (GTK_OBJECT (window),
                      "window_state_event",
                      GTK_SIGNAL_FUNC (window_state_callback),
                      label);

  button = gtk_button_new_with_label ("Deiconify");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_window_deiconify),
                             GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Iconify");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_window_iconify),
                             GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Present");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_window_present),
                             GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Show");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_widget_show),
                             GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  
  gtk_widget_show_all (hbox);
  
  return hbox;
}

static GtkWidget*
get_state_controls (GtkWidget *window)
{
  GtkWidget *vbox;
  GtkWidget *button;

  vbox = gtk_vbox_new (FALSE, 0);
  
  button = gtk_button_new_with_label ("Stick");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_window_stick),
                             GTK_OBJECT (window));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unstick");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_window_unstick),
                             GTK_OBJECT (window));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Maximize");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_window_maximize),
                             GTK_OBJECT (window));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unmaximize");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_window_unmaximize),
                             GTK_OBJECT (window));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Iconify");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_window_iconify),
                             GTK_OBJECT (window));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Hide (withdraw)");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_widget_hide),
                             GTK_OBJECT (window));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  gtk_widget_show_all (vbox);

  return vbox;
}

void
create_window_states (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *box1;
  GtkWidget *iconified;
  GtkWidget *normal;
  GtkWidget *controls;
  
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Window states");
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      iconified = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_signal_connect_object (GTK_OBJECT (iconified), "destroy",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_window_iconify (GTK_WINDOW (iconified));
      gtk_window_set_title (GTK_WINDOW (iconified), "Iconified initially");
      controls = get_state_controls (iconified);
      gtk_container_add (GTK_CONTAINER (iconified), controls);
      
      normal = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_signal_connect_object (GTK_OBJECT (normal), "destroy",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      
      gtk_window_set_title (GTK_WINDOW (normal), "Deiconified initially");
      controls = get_state_controls (normal);
      gtk_container_add (GTK_CONTAINER (normal), controls);
      
      label = tracking_label (iconified);
      gtk_container_add (GTK_CONTAINER (box1), label);

      label = tracking_label (normal);
      gtk_container_add (GTK_CONTAINER (box1), label);

      gtk_widget_show_all (iconified);
      gtk_widget_show_all (normal);
      gtk_widget_show_all (box1);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Window sizing
 */

static gint
configure_event_callback (GtkWidget *widget,
                          GdkEventConfigure *event,
                          gpointer data)
{
  GtkWidget *label = data;
  gchar *msg;
  gint x, y;
  
  gtk_window_get_position (GTK_WINDOW (widget), &x, &y);
  
  msg = g_strdup_printf ("event: %d,%d  %d x %d\n"
                         "position: %d, %d",
                         event->x, event->y, event->width, event->height,
                         x, y);
  
  gtk_label_set_text (GTK_LABEL (label), msg);

  g_free (msg);

  return FALSE;
}

static void
get_ints (GtkWidget *window,
          gint      *a,
          gint      *b)
{
  GtkWidget *spin1;
  GtkWidget *spin2;

  spin1 = g_object_get_data (G_OBJECT (window), "spin1");
  spin2 = g_object_get_data (G_OBJECT (window), "spin2");

  *a = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin1));
  *b = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin2));
}

static void
set_size_callback (GtkWidget *widget,
                   gpointer   data)
{
  gint w, h;
  
  get_ints (data, &w, &h);

  gtk_window_resize (GTK_WINDOW (g_object_get_data (data, "target")), w, h);
}

static void
unset_default_size_callback (GtkWidget *widget,
                             gpointer   data)
{
  gtk_window_set_default_size (g_object_get_data (data, "target"),
                               -1, -1);
}

static void
set_default_size_callback (GtkWidget *widget,
                           gpointer   data)
{
  gint w, h;
  
  get_ints (data, &w, &h);

  gtk_window_set_default_size (g_object_get_data (data, "target"),
                               w, h);
}

static void
unset_usize_callback (GtkWidget *widget,
                      gpointer   data)
{
  gtk_widget_set_size_request (g_object_get_data (data, "target"),
                               -1, -1);
}

static void
set_usize_callback (GtkWidget *widget,
                    gpointer   data)
{
  gint w, h;
  
  get_ints (data, &w, &h);

  gtk_widget_set_size_request (g_object_get_data (data, "target"),
                               w, h);
}

static void
set_location_callback (GtkWidget *widget,
                       gpointer   data)
{
  gint x, y;
  
  get_ints (data, &x, &y);

  gtk_window_move (g_object_get_data (data, "target"), x, y);
}

static void
move_to_position_callback (GtkWidget *widget,
                           gpointer   data)
{
  gint x, y;
  GtkWindow *window;

  window = g_object_get_data (data, "target");
  
  gtk_window_get_position (window, &x, &y);

  gtk_window_move (window, x, y);
}

static void
set_geometry_callback (GtkWidget *entry,
                       gpointer   data)
{
  gchar *text;
  GtkWindow *target;

  target = GTK_WINDOW (g_object_get_data (G_OBJECT (data), "target"));
  
  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

  if (!gtk_window_parse_geometry (target, text))
    g_print ("Bad geometry string '%s'\n", text);

  g_free (text);
}

static void
allow_shrink_callback (GtkWidget *widget,
                       gpointer   data)
{
  g_object_set (G_OBJECT (g_object_get_data (data, "target")),
                "allow_shrink",
                GTK_TOGGLE_BUTTON (widget)->active,
                NULL);
}

static void
allow_grow_callback (GtkWidget *widget,
                     gpointer   data)
{
  g_object_set (G_OBJECT (g_object_get_data (data, "target")),
                "allow_grow",
                GTK_TOGGLE_BUTTON (widget)->active,
                NULL);
}

static void
auto_shrink_callback (GtkWidget *widget,
                      gpointer   data)
{
  g_object_set (G_OBJECT (g_object_get_data (data, "target")),
                "auto_shrink",
                GTK_TOGGLE_BUTTON (widget)->active,
                NULL);
}

static void
gravity_selected (GtkWidget *widget,
                  gpointer   data)
{
  gtk_window_set_gravity (GTK_WINDOW (g_object_get_data (data, "target")),
                          gtk_option_menu_get_history (GTK_OPTION_MENU (widget)) + GDK_GRAVITY_NORTH_WEST);
}

static void
pos_selected (GtkWidget *widget,
              gpointer   data)
{
  gtk_window_set_position (GTK_WINDOW (g_object_get_data (data, "target")),
                           gtk_option_menu_get_history (GTK_OPTION_MENU (widget)) + GTK_WIN_POS_NONE);
}

static void
move_gravity_window_to_current_position (GtkWidget *widget,
                                         gpointer   data)
{
  gint x, y;
  GtkWindow *window;

  window = GTK_WINDOW (data);    
  
  gtk_window_get_position (window, &x, &y);

  gtk_window_move (window, x, y);
}

static void
get_screen_corner (GtkWindow *window,
                   gint      *x,
                   gint      *y)
{
  int w, h;
  
  gtk_window_get_size (GTK_WINDOW (window), &w, &h);

  switch (gtk_window_get_gravity (window))
    {
    case GDK_GRAVITY_SOUTH_EAST:
      *x = gdk_screen_width () - w;
      *y = gdk_screen_height () - h;
      break;

    case GDK_GRAVITY_NORTH_EAST:
      *x = gdk_screen_width () - w;
      *y = 0;
      break;

    case GDK_GRAVITY_SOUTH_WEST:
      *x = 0;
      *y = gdk_screen_height () - h;
      break;

    case GDK_GRAVITY_NORTH_WEST:
      *x = 0;
      *y = 0;
      break;
      
    case GDK_GRAVITY_SOUTH:
      *x = (gdk_screen_width () - w) / 2;
      *y = gdk_screen_height () - h;
      break;

    case GDK_GRAVITY_NORTH:
      *x = (gdk_screen_width () - w) / 2;
      *y = 0;
      break;

    case GDK_GRAVITY_WEST:
      *x = 0;
      *y = (gdk_screen_height () - h) / 2;
      break;

    case GDK_GRAVITY_EAST:
      *x = gdk_screen_width () - w;
      *y = (gdk_screen_height () - h) / 2;
      break;

    case GDK_GRAVITY_CENTER:
      *x = (gdk_screen_width () - w) / 2;
      *y = (gdk_screen_height () - h) / 2;
      break;

    case GDK_GRAVITY_STATIC:
      /* pick some random numbers */
      *x = 350;
      *y = 350;
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
move_gravity_window_to_starting_position (GtkWidget *widget,
                                          gpointer   data)
{
  gint x, y;
  GtkWindow *window;

  window = GTK_WINDOW (data);    
  
  get_screen_corner (window,
                     &x, &y);
  
  gtk_window_move (window, x, y);
}

static GtkWidget*
make_gravity_window (GtkWidget   *destroy_with,
                     GdkGravity   gravity,
                     const gchar *title)
{
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *vbox;
  int x, y;
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);
  
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_gravity (GTK_WINDOW (window), gravity);

  gtk_signal_connect_object (GTK_OBJECT (destroy_with),
                             "destroy",
                             GTK_SIGNAL_FUNC (gtk_widget_destroy),
                             GTK_OBJECT (window));

  
  button = gtk_button_new_with_mnemonic ("_Move to current position");

  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (move_gravity_window_to_current_position),
                    window);

  gtk_container_add (GTK_CONTAINER (vbox), button);
  gtk_widget_show (button);

  button = gtk_button_new_with_mnemonic ("Move to _starting position");

  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (move_gravity_window_to_starting_position),
                    window);

  gtk_container_add (GTK_CONTAINER (vbox), button);
  gtk_widget_show (button);
  
  /* Pretend this is the result of --geometry.
   * DO NOT COPY THIS CODE unless you are setting --geometry results,
   * and in that case you probably should just use gtk_window_parse_geometry().
   * AGAIN, DO NOT SET GDK_HINT_USER_POS! It violates the ICCCM unless
   * you are parsing --geometry or equivalent.
   */
  gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                 NULL, NULL,
                                 GDK_HINT_USER_POS);

  gtk_window_set_default_size (GTK_WINDOW (window),
                               200, 200);

  get_screen_corner (GTK_WINDOW (window), &x, &y);
  
  gtk_window_move (GTK_WINDOW (window),
                   x, y);
  
  return window;
}

static void
do_gravity_test (GtkWidget *widget,
                 gpointer   data)
{
  GtkWidget *destroy_with = data;
  GtkWidget *window;
  
  /* We put a window at each gravity point on the screen. */
  window = make_gravity_window (destroy_with, GDK_GRAVITY_NORTH_WEST,
                                "NorthWest");
  gtk_widget_show (window);
  
  window = make_gravity_window (destroy_with, GDK_GRAVITY_SOUTH_EAST,
                                "SouthEast");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_NORTH_EAST,
                                "NorthEast");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_SOUTH_WEST,
                                "SouthWest");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_SOUTH,
                                "South");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_NORTH,
                                "North");
  gtk_widget_show (window);

  
  window = make_gravity_window (destroy_with, GDK_GRAVITY_WEST,
                                "West");
  gtk_widget_show (window);

    
  window = make_gravity_window (destroy_with, GDK_GRAVITY_EAST,
                                "East");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_CENTER,
                                "Center");
  gtk_widget_show (window);

  window = make_gravity_window (destroy_with, GDK_GRAVITY_STATIC,
                                "Static");
  gtk_widget_show (window);
}

static GtkWidget*
window_controls (GtkWidget *window)
{
  GtkWidget *control_window;
  GtkWidget *label;
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *spin;
  GtkAdjustment *adj;
  GtkWidget *entry;
  GtkWidget *om;
  GtkWidget *menu;
  gint i;
  
  control_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (control_window), "Size controls");
  
  g_object_set_data (G_OBJECT (control_window),
                     "target",
                     window);
  
  gtk_signal_connect_object (GTK_OBJECT (control_window),
                             "destroy",
                             GTK_SIGNAL_FUNC (gtk_widget_destroy),
                             GTK_OBJECT (window));

  vbox = gtk_vbox_new (FALSE, 5);
  
  gtk_container_add (GTK_CONTAINER (control_window), vbox);
  
  label = gtk_label_new ("<no configure events>");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  
  gtk_signal_connect (GTK_OBJECT (window),
                      "configure_event",
                      GTK_SIGNAL_FUNC (configure_event_callback),
                      label);

  adj = (GtkAdjustment *) gtk_adjustment_new (10.0, -2000.0, 2000.0, 1.0,
                                              5.0, 0.0);
  spin = gtk_spin_button_new (adj, 0, 0);

  gtk_box_pack_start (GTK_BOX (vbox), spin, FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (control_window), "spin1", spin);

  adj = (GtkAdjustment *) gtk_adjustment_new (10.0, -2000.0, 2000.0, 1.0,
                                              5.0, 0.0);
  spin = gtk_spin_button_new (adj, 0, 0);

  gtk_box_pack_start (GTK_BOX (vbox), spin, FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (control_window), "spin2", spin);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT (entry), "changed",
                      GTK_SIGNAL_FUNC (set_geometry_callback),
                      control_window);

  button = gtk_button_new_with_label ("Show gravity test windows");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (do_gravity_test),
                             control_window);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Reshow with initial size");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_window_reshow_with_initial_size),
                             GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Queue resize");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_widget_queue_resize),
                             GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Resize");
  gtk_signal_connect (GTK_OBJECT (button),
                      "clicked",
                      GTK_SIGNAL_FUNC (set_size_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Set default size");
  gtk_signal_connect (GTK_OBJECT (button),
                      "clicked",
                      GTK_SIGNAL_FUNC (set_default_size_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unset default size");
  gtk_signal_connect (GTK_OBJECT (button),
                      "clicked",
                      GTK_SIGNAL_FUNC (unset_default_size_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Set size request");
  gtk_signal_connect (GTK_OBJECT (button),
                      "clicked",
                      GTK_SIGNAL_FUNC (set_usize_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Unset size request");
  gtk_signal_connect (GTK_OBJECT (button),
                      "clicked",
                      GTK_SIGNAL_FUNC (unset_usize_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_button_new_with_label ("Move");
  gtk_signal_connect (GTK_OBJECT (button),
                      "clicked",
                      GTK_SIGNAL_FUNC (set_location_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Move to current position");
  gtk_signal_connect (GTK_OBJECT (button),
                      "clicked",
                      GTK_SIGNAL_FUNC (move_to_position_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_check_button_new_with_label ("Allow shrink");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_signal_connect (GTK_OBJECT (button),
                      "toggled",
                      GTK_SIGNAL_FUNC (allow_shrink_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_check_button_new_with_label ("Allow grow");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  gtk_signal_connect (GTK_OBJECT (button),
                      "toggled",
                      GTK_SIGNAL_FUNC (allow_grow_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  button = gtk_check_button_new_with_label ("Auto shrink");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_signal_connect (GTK_OBJECT (button),
                      "toggled",
                      GTK_SIGNAL_FUNC (auto_shrink_callback),
                      GTK_OBJECT (control_window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic ("_Show");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_widget_show),
                             GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic ("_Hide");
  gtk_signal_connect_object (GTK_OBJECT (button),
                             "clicked",
                             GTK_SIGNAL_FUNC (gtk_widget_hide),
                             GTK_OBJECT (window));
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  
  menu = gtk_menu_new ();
  
  i = 0;
  while (i < 10)
    {
      GtkWidget *mi;
      static gchar *names[] = {
        "GDK_GRAVITY_NORTH_WEST",
        "GDK_GRAVITY_NORTH",
        "GDK_GRAVITY_NORTH_EAST",
        "GDK_GRAVITY_WEST",
        "GDK_GRAVITY_CENTER",
        "GDK_GRAVITY_EAST",
        "GDK_GRAVITY_SOUTH_WEST",
        "GDK_GRAVITY_SOUTH",
        "GDK_GRAVITY_SOUTH_EAST",
        "GDK_GRAVITY_STATIC",
        NULL
      };

      g_assert (names[i]);
      
      mi = gtk_menu_item_new_with_label (names[i]);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

      ++i;
    }
  
  gtk_widget_show_all (menu);
  
  om = gtk_option_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (om), menu);
  

  gtk_signal_connect (GTK_OBJECT (om),
                      "changed",
                      GTK_SIGNAL_FUNC (gravity_selected),
                      control_window);

  gtk_box_pack_end (GTK_BOX (vbox), om, FALSE, FALSE, 0);


  menu = gtk_menu_new ();
  
  i = 0;
  while (i < 5)
    {
      GtkWidget *mi;
      static gchar *names[] = {
        "GTK_WIN_POS_NONE",
        "GTK_WIN_POS_CENTER",
        "GTK_WIN_POS_MOUSE",
        "GTK_WIN_POS_CENTER_ALWAYS",
        "GTK_WIN_POS_CENTER_ON_PARENT",
        NULL
      };

      g_assert (names[i]);
      
      mi = gtk_menu_item_new_with_label (names[i]);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

      ++i;
    }
  
  gtk_widget_show_all (menu);
  
  om = gtk_option_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (om), menu);
  

  gtk_signal_connect (GTK_OBJECT (om),
                      "changed",
                      GTK_SIGNAL_FUNC (pos_selected),
                      control_window);

  gtk_box_pack_end (GTK_BOX (vbox), om, FALSE, FALSE, 0);
  
  gtk_widget_show_all (vbox);
  
  return control_window;
}

void
create_window_sizing (void)
{
  static GtkWidget *window = NULL;
  static GtkWidget *target_window = NULL;
  
  if (!target_window)
    {
      GtkWidget *label;
      
      target_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), "<span foreground=\"purple\"><big>Window being resized</big></span>\nBlah blah blah blah\nblah blah blah\nblah blah blah blah blah");
      gtk_container_add (GTK_CONTAINER (target_window), label);
      gtk_widget_show (label);
      
      gtk_signal_connect (GTK_OBJECT (target_window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &target_window);

      window = window_controls (target_window);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			  &window);
      
      gtk_window_set_title (GTK_WINDOW (target_window), "Window to size");
    }

  /* don't show target window by default, we want to allow testing
   * of behavior on first show.
   */
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * GtkProgressBar
 */

typedef struct _ProgressData {
  GtkWidget *window;
  GtkWidget *pbar;
  GtkWidget *block_spin;
  GtkWidget *x_align_spin;
  GtkWidget *y_align_spin;
  GtkWidget *step_spin;
  GtkWidget *act_blocks_spin;
  GtkWidget *label;
  GtkWidget *omenu1;
  GtkWidget *omenu2;
  GtkWidget *entry;
  int timer;
} ProgressData;

gint
progress_timeout (gpointer data)
{
  gdouble new_val;
  GtkAdjustment *adj;

  adj = GTK_PROGRESS (data)->adjustment;

  new_val = adj->value + 1;
  if (new_val > adj->upper)
    new_val = adj->lower;

  gtk_progress_set_value (GTK_PROGRESS (data), new_val);

  return TRUE;
}

static void
destroy_progress (GtkWidget     *widget,
		  ProgressData **pdata)
{
  gtk_timeout_remove ((*pdata)->timer);
  (*pdata)->timer = 0;
  (*pdata)->window = NULL;
  g_free (*pdata);
  *pdata = NULL;
}

static void
progressbar_toggle_orientation (GtkWidget *widget, gpointer data)
{
  ProgressData *pdata;
  gint i;

  pdata = (ProgressData *) data;

  if (!GTK_WIDGET_MAPPED (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (pdata->pbar),
				    (GtkProgressBarOrientation) i);
}

static void
toggle_show_text (GtkWidget *widget, ProgressData *pdata)
{
  gtk_progress_set_show_text (GTK_PROGRESS (pdata->pbar),
			      GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->entry, GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->x_align_spin,
			    GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->y_align_spin,
			    GTK_TOGGLE_BUTTON (widget)->active);
}

static void
progressbar_toggle_bar_style (GtkWidget *widget, gpointer data)
{
  ProgressData *pdata;
  gint i;

  pdata = (ProgressData *) data;

  if (!GTK_WIDGET_MAPPED (widget))
    return;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (widget));

  if (i == 1)
    gtk_widget_set_sensitive (pdata->block_spin, TRUE);
  else
    gtk_widget_set_sensitive (pdata->block_spin, FALSE);
  
  gtk_progress_bar_set_bar_style (GTK_PROGRESS_BAR (pdata->pbar),
				  (GtkProgressBarStyle) i);
}

static void
progress_value_changed (GtkAdjustment *adj, ProgressData *pdata)
{
  char buf[20];

  if (GTK_PROGRESS (pdata->pbar)->activity_mode)
    sprintf (buf, "???");
  else
    sprintf (buf, "%.0f%%", 100 *
	     gtk_progress_get_current_percentage (GTK_PROGRESS (pdata->pbar)));
  gtk_label_set_text (GTK_LABEL (pdata->label), buf);
}

static void
adjust_blocks (GtkAdjustment *adj, ProgressData *pdata)
{
  gtk_progress_set_percentage (GTK_PROGRESS (pdata->pbar), 0);
  gtk_progress_bar_set_discrete_blocks (GTK_PROGRESS_BAR (pdata->pbar),
     gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pdata->block_spin)));
}

static void
adjust_step (GtkAdjustment *adj, ProgressData *pdata)
{
  gtk_progress_bar_set_activity_step (GTK_PROGRESS_BAR (pdata->pbar),
     gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pdata->step_spin)));
}

static void
adjust_act_blocks (GtkAdjustment *adj, ProgressData *pdata)
{
  gtk_progress_bar_set_activity_blocks (GTK_PROGRESS_BAR (pdata->pbar),
               gtk_spin_button_get_value_as_int 
		      (GTK_SPIN_BUTTON (pdata->act_blocks_spin)));
}

static void
adjust_align (GtkAdjustment *adj, ProgressData *pdata)
{
  gtk_progress_set_text_alignment (GTK_PROGRESS (pdata->pbar),
	 gtk_spin_button_get_value_as_float 
				   (GTK_SPIN_BUTTON (pdata->x_align_spin)),
	 gtk_spin_button_get_value_as_float
				   (GTK_SPIN_BUTTON (pdata->y_align_spin)));
}

static void
toggle_activity_mode (GtkWidget *widget, ProgressData *pdata)
{
  gtk_progress_set_activity_mode (GTK_PROGRESS (pdata->pbar),
				  GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->step_spin, 
			    GTK_TOGGLE_BUTTON (widget)->active);
  gtk_widget_set_sensitive (pdata->act_blocks_spin, 
			    GTK_TOGGLE_BUTTON (widget)->active);
}

static void
entry_changed (GtkWidget *widget, ProgressData *pdata)
{
  gtk_progress_set_format_string (GTK_PROGRESS (pdata->pbar),
			  gtk_entry_get_text (GTK_ENTRY (pdata->entry)));
}

void
create_progress_bar (void)
{
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *check;
  GtkWidget *frame;
  GtkWidget *tab;
  GtkWidget *label;
  GtkWidget *align;
  GtkAdjustment *adj;
  static ProgressData *pdata = NULL;

  static gchar *items1[] =
  {
    "Left-Right",
    "Right-Left",
    "Bottom-Top",
    "Top-Bottom"
  };

  static gchar *items2[] =
  {
    "Continuous",
    "Discrete"
  };

  if (!pdata)
    pdata = g_new0 (ProgressData, 1);

  if (!pdata->window)
    {
      pdata->window = gtk_dialog_new ();

      gtk_window_set_policy (GTK_WINDOW (pdata->window), FALSE, FALSE, TRUE);

      gtk_signal_connect (GTK_OBJECT (pdata->window), "destroy",
			  GTK_SIGNAL_FUNC (destroy_progress),
			  &pdata);

      pdata->timer = 0;

      gtk_window_set_title (GTK_WINDOW (pdata->window), "GtkProgressBar");
      gtk_container_set_border_width (GTK_CONTAINER (pdata->window), 0);

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (pdata->window)->vbox), 
			  vbox, FALSE, TRUE, 0);

      frame = gtk_frame_new ("Progress");
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);

      align = gtk_alignment_new (0.5, 0.5, 0, 0);
      gtk_box_pack_start (GTK_BOX (vbox2), align, FALSE, FALSE, 5);

      adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 300, 0, 0, 0);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (progress_value_changed), pdata);

      pdata->pbar = gtk_progress_bar_new_with_adjustment (adj);
      gtk_progress_set_format_string (GTK_PROGRESS (pdata->pbar),
				      "%v from [%l,%u] (=%p%%)");
      gtk_container_add (GTK_CONTAINER (align), pdata->pbar);
      pdata->timer = gtk_timeout_add (100, progress_timeout, pdata->pbar);

      align = gtk_alignment_new (0.5, 0.5, 0, 0);
      gtk_box_pack_start (GTK_BOX (vbox2), align, FALSE, FALSE, 5);

      hbox = gtk_hbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (align), hbox);
      label = gtk_label_new ("Label updated by user :"); 
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      pdata->label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), pdata->label, FALSE, TRUE, 0);

      frame = gtk_frame_new ("Options");
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

      vbox2 = gtk_vbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);

      tab = gtk_table_new (7, 2, FALSE);
      gtk_box_pack_start (GTK_BOX (vbox2), tab, FALSE, TRUE, 0);

      label = gtk_label_new ("Orientation :");
      gtk_table_attach (GTK_TABLE (tab), label, 0, 1, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

      pdata->omenu1 = build_option_menu (items1, 4, 0,
					 progressbar_toggle_orientation,
					 pdata);
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->omenu1, TRUE, TRUE, 0);
      
      check = gtk_check_button_new_with_label ("Show text");
      gtk_signal_connect (GTK_OBJECT (check), "clicked",
			  GTK_SIGNAL_FUNC (toggle_show_text),
			  pdata);
      gtk_table_attach (GTK_TABLE (tab), check, 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);

      label = gtk_label_new ("Format : ");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

      pdata->entry = gtk_entry_new ();
      gtk_signal_connect (GTK_OBJECT (pdata->entry), "changed",
			  GTK_SIGNAL_FUNC (entry_changed),
			  pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->entry, TRUE, TRUE, 0);
      gtk_entry_set_text (GTK_ENTRY (pdata->entry), "%v from [%l,%u] (=%p%%)");
      gtk_widget_set_usize (pdata->entry, 100, -1);
      gtk_widget_set_sensitive (pdata->entry, FALSE);

      label = gtk_label_new ("Text align :");
      gtk_table_attach (GTK_TABLE (tab), label, 0, 1, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 2, 3,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);

      label = gtk_label_new ("x :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 5);
      
      adj = (GtkAdjustment *) gtk_adjustment_new (0.5, 0, 1, 0.1, 0.1, 0);
      pdata->x_align_spin = gtk_spin_button_new (adj, 0, 1);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (adjust_align), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->x_align_spin, FALSE, TRUE, 0);
      gtk_widget_set_sensitive (pdata->x_align_spin, FALSE);

      label = gtk_label_new ("y :");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 5);

      adj = (GtkAdjustment *) gtk_adjustment_new (0.5, 0, 1, 0.1, 0.1, 0);
      pdata->y_align_spin = gtk_spin_button_new (adj, 0, 1);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (adjust_align), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->y_align_spin, FALSE, TRUE, 0);
      gtk_widget_set_sensitive (pdata->y_align_spin, FALSE);

      label = gtk_label_new ("Bar Style :");
      gtk_table_attach (GTK_TABLE (tab), label, 0, 1, 3, 4,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

      pdata->omenu2 = build_option_menu	(items2, 2, 0,
					 progressbar_toggle_bar_style,
					 pdata);
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 3, 4,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->omenu2, TRUE, TRUE, 0);

      label = gtk_label_new ("Block count :");
      gtk_table_attach (GTK_TABLE (tab), label, 0, 1, 4, 5,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 4, 5,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      adj = (GtkAdjustment *) gtk_adjustment_new (10, 2, 20, 1, 5, 0);
      pdata->block_spin = gtk_spin_button_new (adj, 0, 0);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (adjust_blocks), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->block_spin, FALSE, TRUE, 0);
      gtk_widget_set_sensitive (pdata->block_spin, FALSE);

      check = gtk_check_button_new_with_label ("Activity mode");
      gtk_signal_connect (GTK_OBJECT (check), "clicked",
			  GTK_SIGNAL_FUNC (toggle_activity_mode),
			  pdata);
      gtk_table_attach (GTK_TABLE (tab), check, 0, 1, 5, 6,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 5, 6,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      label = gtk_label_new ("Step size : ");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      adj = (GtkAdjustment *) gtk_adjustment_new (3, 1, 20, 1, 5, 0);
      pdata->step_spin = gtk_spin_button_new (adj, 0, 0);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (adjust_step), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->step_spin, FALSE, TRUE, 0);
      gtk_widget_set_sensitive (pdata->step_spin, FALSE);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (tab), hbox, 1, 2, 6, 7,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
			5, 5);
      label = gtk_label_new ("Blocks :     ");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
      adj = (GtkAdjustment *) gtk_adjustment_new (5, 2, 10, 1, 5, 0);
      pdata->act_blocks_spin = gtk_spin_button_new (adj, 0, 0);
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (adjust_act_blocks), pdata);
      gtk_box_pack_start (GTK_BOX (hbox), pdata->act_blocks_spin, FALSE, TRUE,
			  0);
      gtk_widget_set_sensitive (pdata->act_blocks_spin, FALSE);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (pdata->window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (pdata->window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
    }

  if (!GTK_WIDGET_VISIBLE (pdata->window))
    gtk_widget_show_all (pdata->window);
  else
    gtk_widget_destroy (pdata->window);
}

/*
 * Properties
 */

typedef struct {
  int x;
  int y;
  gboolean found;
  gboolean first;
  GtkWidget *res_widget;
} FindWidgetData;

static void
find_widget (GtkWidget *widget, FindWidgetData *data)
{
  GtkAllocation new_allocation;
  gint x_offset = 0;
  gint y_offset = 0;

  new_allocation = widget->allocation;

  if (data->found || !GTK_WIDGET_MAPPED (widget))
    return;

  /* Note that in the following code, we only count the
   * position as being inside a WINDOW widget if it is inside
   * widget->window; points that are outside of widget->window
   * but within the allocation are not counted. This is consistent
   * with the way we highlight drag targets.
   */
  if (!GTK_WIDGET_NO_WINDOW (widget))
    {
      new_allocation.x = 0;
      new_allocation.y = 0;
    }
  
  if (widget->parent && !data->first)
    {
      GdkWindow *window = widget->window;
      while (window != widget->parent->window)
	{
	  gint tx, ty, twidth, theight;
	  gdk_window_get_size (window, &twidth, &theight);

	  if (new_allocation.x < 0)
	    {
	      new_allocation.width += new_allocation.x;
	      new_allocation.x = 0;
	    }
	  if (new_allocation.y < 0)
	    {
	      new_allocation.height += new_allocation.y;
	      new_allocation.y = 0;
	    }
	  if (new_allocation.x + new_allocation.width > twidth)
	    new_allocation.width = twidth - new_allocation.x;
	  if (new_allocation.y + new_allocation.height > theight)
	    new_allocation.height = theight - new_allocation.y;

	  gdk_window_get_position (window, &tx, &ty);
	  new_allocation.x += tx;
	  x_offset += tx;
	  new_allocation.y += ty;
	  y_offset += ty;
	  
	  window = gdk_window_get_parent (window);
	}
    }

  if ((data->x >= new_allocation.x) && (data->y >= new_allocation.y) &&
      (data->x < new_allocation.x + new_allocation.width) && 
      (data->y < new_allocation.y + new_allocation.height))
    {
      /* First, check if the drag is in a valid drop site in
       * one of our children 
       */
      if (GTK_IS_CONTAINER (widget))
	{
	  FindWidgetData new_data = *data;
	  
	  new_data.x -= x_offset;
	  new_data.y -= y_offset;
	  new_data.found = FALSE;
	  new_data.first = FALSE;
	  
	  gtk_container_forall (GTK_CONTAINER (widget),
				(GtkCallback)find_widget,
				&new_data);
	  
	  data->found = new_data.found;
	  if (data->found)
	    data->res_widget = new_data.res_widget;
	}

      /* If not, and this widget is registered as a drop site, check to
       * emit "drag_motion" to check if we are actually in
       * a drop site.
       */
      if (!data->found)
	{
	  data->found = TRUE;
	  data->res_widget = widget;
	}
    }
}

static GtkWidget *
find_widget_at_pointer (void)
{
  GtkWidget *widget = NULL;
  GdkWindow *pointer_window;
  gint x, y;
  FindWidgetData data;
 
 pointer_window = gdk_window_at_pointer (NULL, NULL);
 
 if (pointer_window)
   gdk_window_get_user_data (pointer_window, (gpointer*) &widget);

 if (widget)
   {
     gdk_window_get_pointer (widget->window,
			     &x, &y, NULL);
     
     data.x = x;
     data.y = y;
     data.found = FALSE;
     data.first = TRUE;

     find_widget (widget, &data);
     if (data.found)
       return data.res_widget;
     return widget;
   }
 return NULL;
}

struct PropertiesData {
  GtkWidget **window;
  GdkCursor *cursor;
  gboolean in_query;
  gint handler;
};

static void
destroy_properties (GtkWidget             *widget,
		    struct PropertiesData *data)
{
  if (data->window)
    {
      *data->window = NULL;
      data->window = NULL;
    }

  if (data->cursor)
    {
      gdk_cursor_destroy (data->cursor);
      data->cursor = NULL;
    }

  if (data->handler)
    {
      gtk_signal_disconnect (widget, data->handler);
      data->handler = 0;
    }

  g_free (data);
}

static gint
property_query_event (GtkWidget	       *widget,
		      GdkEvent	       *event,
		      struct PropertiesData *data)
{
  GtkWidget *res_widget = NULL;

  if (!data->in_query)
    return FALSE;
  
  if (event->type == GDK_BUTTON_RELEASE)
    {
      gtk_grab_remove (widget);
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
      
      res_widget = find_widget_at_pointer ();
      if (res_widget)
	create_prop_editor (G_OBJECT (res_widget), 0);

      data->in_query = FALSE;
    }
  return FALSE;
}


static void
query_properties (GtkButton *button,
		  struct PropertiesData *data)
{
  gint failure;

  gtk_signal_connect (GTK_OBJECT (button), "event",
		      (GtkSignalFunc) property_query_event, data);


  if (!data->cursor)
    data->cursor = gdk_cursor_new (GDK_TARGET);
  
  failure = gdk_pointer_grab (GTK_WIDGET (button)->window,
			      TRUE,
			      GDK_BUTTON_RELEASE_MASK,
			      NULL,
			      data->cursor,
			      GDK_CURRENT_TIME);

  gtk_grab_add (GTK_WIDGET (button));

  data->in_query = TRUE;
}

static void
create_properties (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *label;
  struct PropertiesData *data;

  data = g_new (struct PropertiesData, 1);
  data->window = &window;
  data->in_query = FALSE;
  data->cursor = NULL;
  data->handler = 0;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      data->handler = gtk_signal_connect (GTK_OBJECT (window), "destroy",
					  GTK_SIGNAL_FUNC(destroy_properties),
					  data);

      gtk_window_set_title (GTK_WINDOW (window), "test properties");
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      vbox = gtk_vbox_new (FALSE, 1);
      gtk_container_add (GTK_CONTAINER (window), vbox);
            
      label = gtk_label_new ("This is just a dumb test to test properties.\nIf you need a generic module, get GLE.");
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
      
      button = gtk_button_new_with_label ("Query properties");
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(query_properties),
			  data);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
  
}


/*
 * Color Preview
 */

static int color_idle = 0;

gint
color_idle_func (GtkWidget *preview)
{
  static int count = 1;
  guchar buf[768];
  int i, j, k;

  for (i = 0; i < 256; i++)
    {
      for (j = 0, k = 0; j < 256; j++)
	{
	  buf[k+0] = i + count;
	  buf[k+1] = 0;
	  buf[k+2] = j + count;
	  k += 3;
	}

      gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
    }

  count += 1;

  gtk_widget_draw (preview, NULL);

  return TRUE;
}

static void
color_preview_destroy (GtkWidget  *widget,
		       GtkWidget **window)
{
  gtk_idle_remove (color_idle);
  color_idle = 0;

  *window = NULL;
}

void
create_color_preview (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *preview;
  guchar buf[768];
  int i, j, k;

  if (!window)
    {
      gtk_widget_push_colormap (gdk_rgb_get_cmap ());
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_widget_pop_colormap ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(color_preview_destroy),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      preview = gtk_preview_new (GTK_PREVIEW_COLOR);
      gtk_preview_size (GTK_PREVIEW (preview), 256, 256);
      gtk_container_add (GTK_CONTAINER (window), preview);

      for (i = 0; i < 256; i++)
	{
	  for (j = 0, k = 0; j < 256; j++)
	    {
	      buf[k+0] = i;
	      buf[k+1] = 0;
	      buf[k+2] = j;
	      k += 3;
	    }

	  gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
	}

      color_idle = gtk_idle_add ((GtkFunction) color_idle_func, preview);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Gray Preview
 */

static int gray_idle = 0;

gint
gray_idle_func (GtkWidget *preview)
{
  static int count = 1;
  guchar buf[256];
  int i, j;

  for (i = 0; i < 256; i++)
    {
      for (j = 0; j < 256; j++)
	buf[j] = i + j + count;

      gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
    }

  count += 1;

  gtk_widget_draw (preview, NULL);

  return TRUE;
}

static void
gray_preview_destroy (GtkWidget  *widget,
		      GtkWidget **window)
{
  gtk_idle_remove (gray_idle);
  gray_idle = 0;

  *window = NULL;
}

void
create_gray_preview (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *preview;
  guchar buf[256];
  int i, j;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gray_preview_destroy),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      preview = gtk_preview_new (GTK_PREVIEW_GRAYSCALE);
      gtk_preview_size (GTK_PREVIEW (preview), 256, 256);
      gtk_container_add (GTK_CONTAINER (window), preview);

      for (i = 0; i < 256; i++)
	{
	  for (j = 0; j < 256; j++)
	    buf[j] = i + j;

	  gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, 256);
	}

      gray_idle = gtk_idle_add ((GtkFunction) gray_idle_func, preview);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}


/*
 * Selection Test
 */

void
selection_test_received (GtkWidget *list, GtkSelectionData *data)
{
  GdkAtom *atoms;
  GtkWidget *list_item;
  GList *item_list;
  int i, l;

  if (data->length < 0)
    {
      g_print ("Selection retrieval failed\n");
      return;
    }
  if (data->type != GDK_SELECTION_TYPE_ATOM)
    {
      g_print ("Selection \"TARGETS\" was not returned as atoms!\n");
      return;
    }

  /* Clear out any current list items */

  gtk_list_clear_items (GTK_LIST(list), 0, -1);

  /* Add new items to list */

  atoms = (GdkAtom *)data->data;

  item_list = NULL;
  l = data->length / sizeof (GdkAtom);
  for (i = 0; i < l; i++)
    {
      char *name;
      name = gdk_atom_name (atoms[i]);
      if (name != NULL)
	{
	  list_item = gtk_list_item_new_with_label (name);
	  g_free (name);
	}
      else
	list_item = gtk_list_item_new_with_label ("(bad atom)");

      gtk_widget_show (list_item);
      item_list = g_list_append (item_list, list_item);
    }

  gtk_list_append_items (GTK_LIST (list), item_list);

  return;
}

void
selection_test_get_targets (GtkWidget *widget, GtkWidget *list)
{
  static GdkAtom targets_atom = GDK_NONE;

  if (targets_atom == GDK_NONE)
    targets_atom = gdk_atom_intern ("TARGETS", FALSE);

  gtk_selection_convert (list, GDK_SELECTION_PRIMARY, targets_atom,
			 GDK_CURRENT_TIME);
}

void
create_selection_test (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *scrolled_win;
  GtkWidget *list;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Selection Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      /* Create the list */

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox,
			  TRUE, TRUE, 0);

      label = gtk_label_new ("Gets available targets for current selection");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC, 
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
      gtk_widget_set_usize (scrolled_win, 100, 200);

      list = gtk_list_new ();
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_win), list);

      gtk_signal_connect (GTK_OBJECT(list), "selection_received",
			  GTK_SIGNAL_FUNC (selection_test_received), NULL);

      /* .. And create some buttons */
      button = gtk_button_new_with_label ("Get Targets");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area),
			  button, TRUE, TRUE, 0);

      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (selection_test_get_targets), list);

      button = gtk_button_new_with_label ("Quit");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area),
			  button, TRUE, TRUE, 0);

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Gamma Curve
 */

void
create_gamma_curve (void)
{
  static GtkWidget *window = NULL, *curve;
  static int count = 0;
  gfloat vec[256];
  gint max;
  gint i;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      curve = gtk_gamma_curve_new ();
      gtk_container_add (GTK_CONTAINER (window), curve);
      gtk_widget_show (curve);
    }

  max = 127 + (count % 2)*128;
  gtk_curve_set_range (GTK_CURVE (GTK_GAMMA_CURVE (curve)->curve),
		       0, max, 0, max);
  for (i = 0; i < max; ++i)
    vec[i] = (127 / sqrt (max)) * sqrt (i);
  gtk_curve_set_vector (GTK_CURVE (GTK_GAMMA_CURVE (curve)->curve),
			max, vec);

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else if (count % 4 == 3)
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  ++count;
}

/*
 * Test scrolling
 */

static int scroll_test_pos = 0.0;

static gint
scroll_test_expose (GtkWidget *widget, GdkEventExpose *event,
		    GtkAdjustment *adj)
{
  gint i,j;
  gint imin, imax, jmin, jmax;
  
  imin = (event->area.x) / 10;
  imax = (event->area.x + event->area.width + 9) / 10;

  jmin = ((int)adj->value + event->area.y) / 10;
  jmax = ((int)adj->value + event->area.y + event->area.height + 9) / 10;

  gdk_window_clear_area (widget->window,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);

  for (i=imin; i<imax; i++)
    for (j=jmin; j<jmax; j++)
      if ((i+j) % 2)
	gdk_draw_rectangle (widget->window, 
			    widget->style->black_gc,
			    TRUE,
			    10*i, 10*j - (int)adj->value, 1+i%10, 1+j%10);

  return TRUE;
}

static gint
scroll_test_scroll (GtkWidget *widget, GdkEventScroll *event,
		    GtkAdjustment *adj)
{
  gdouble new_value = adj->value + ((event->direction == GDK_SCROLL_UP) ?
				    -adj->page_increment / 2:
				    adj->page_increment / 2);
  new_value = CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
  gtk_adjustment_set_value (adj, new_value);  
  
  return TRUE;
}

static void
scroll_test_configure (GtkWidget *widget, GdkEventConfigure *event,
		       GtkAdjustment *adj)
{
  adj->page_increment = 0.9 * widget->allocation.height;
  adj->page_size = widget->allocation.height;

  gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
}

static void
scroll_test_adjustment_changed (GtkAdjustment *adj, GtkWidget *widget)
{
  /* gint source_min = (int)adj->value - scroll_test_pos; */
  gint dy;

  dy = scroll_test_pos - (int)adj->value;
  scroll_test_pos = adj->value;

  if (!GTK_WIDGET_DRAWABLE (widget))
    return;
  gdk_window_scroll (widget->window, 0, dy);
  gdk_window_process_updates (widget->window, FALSE);
}


void
create_scroll_test (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *hbox;
  GtkWidget *drawing_area;
  GtkWidget *scrollbar;
  GtkWidget *button;
  GtkAdjustment *adj;
  GdkGeometry geometry;
  GdkWindowHints geometry_mask;
  
  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Scroll Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox,
			  TRUE, TRUE, 0);
      gtk_widget_show (hbox);

      drawing_area = gtk_drawing_area_new ();
      gtk_drawing_area_size (GTK_DRAWING_AREA (drawing_area), 200, 200);
      gtk_box_pack_start (GTK_BOX (hbox), drawing_area, TRUE, TRUE, 0);
      gtk_widget_show (drawing_area);

      gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK | GDK_SCROLL_MASK);

      adj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 1000.0, 1.0, 180.0, 200.0));
      scroll_test_pos = 0.0;

      scrollbar = gtk_vscrollbar_new (adj);
      gtk_box_pack_start (GTK_BOX (hbox), scrollbar, FALSE, FALSE, 0);
      gtk_widget_show (scrollbar);

      gtk_signal_connect (GTK_OBJECT (drawing_area), "expose_event",
			  GTK_SIGNAL_FUNC (scroll_test_expose), adj);
      gtk_signal_connect (GTK_OBJECT (drawing_area), "configure_event",
			  GTK_SIGNAL_FUNC (scroll_test_configure), adj);
      gtk_signal_connect (GTK_OBJECT (drawing_area), "scroll_event",
			  GTK_SIGNAL_FUNC (scroll_test_scroll), adj);
      
      gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
			  GTK_SIGNAL_FUNC (scroll_test_adjustment_changed),
			  drawing_area);
      
      /* .. And create some buttons */

      button = gtk_button_new_with_label ("Quit");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area),
			  button, TRUE, TRUE, 0);

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));
      gtk_widget_show (button);

      /* Set up gridded geometry */

      geometry_mask = GDK_HINT_MIN_SIZE | 
	               GDK_HINT_BASE_SIZE | 
	               GDK_HINT_RESIZE_INC;

      geometry.min_width = 20;
      geometry.min_height = 20;
      geometry.base_width = 0;
      geometry.base_height = 0;
      geometry.width_inc = 10;
      geometry.height_inc = 10;
      
      gtk_window_set_geometry_hints (GTK_WINDOW (window),
			       drawing_area, &geometry, geometry_mask);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Timeout Test
 */

static int timer = 0;

gint
timeout_test (GtkWidget *label)
{
  static int count = 0;
  static char buffer[32];

  sprintf (buffer, "count: %d", ++count);
  gtk_label_set_text (GTK_LABEL (label), buffer);

  return TRUE;
}

void
start_timeout_test (GtkWidget *widget,
		    GtkWidget *label)
{
  if (!timer)
    {
      timer = gtk_timeout_add (100, (GtkFunction) timeout_test, label);
    }
}

void
stop_timeout_test (GtkWidget *widget,
		   gpointer   data)
{
  if (timer)
    {
      gtk_timeout_remove (timer);
      timer = 0;
    }
}

void
destroy_timeout_test (GtkWidget  *widget,
		      GtkWidget **window)
{
  stop_timeout_test (NULL, NULL);

  *window = NULL;
}

void
create_timeout_test (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_timeout_test),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Timeout Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      label = gtk_label_new ("count: 0");
      gtk_misc_set_padding (GTK_MISC (label), 10, 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  label, TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("start");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(start_timeout_test),
			  label);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("stop");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(stop_timeout_test),
			  NULL);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Idle Test
 */

static int idle_id = 0;

static gint
idle_test (GtkWidget *label)
{
  static int count = 0;
  static char buffer[32];

  sprintf (buffer, "count: %d", ++count);
  gtk_label_set_text (GTK_LABEL (label), buffer);

  return TRUE;
}

static void
start_idle_test (GtkWidget *widget,
		 GtkWidget *label)
{
  if (!idle_id)
    {
      idle_id = gtk_idle_add ((GtkFunction) idle_test, label);
    }
}

static void
stop_idle_test (GtkWidget *widget,
		gpointer   data)
{
  if (idle_id)
    {
      gtk_idle_remove (idle_id);
      idle_id = 0;
    }
}

static void
destroy_idle_test (GtkWidget  *widget,
		   GtkWidget **window)
{
  stop_idle_test (NULL, NULL);

  *window = NULL;
}

static void
toggle_idle_container (GtkObject *button,
		       GtkContainer *container)
{
  gtk_container_set_resize_mode (container, GPOINTER_TO_INT (gtk_object_get_user_data (button)));
}

static void
create_idle_test (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *container;

  if (!window)
    {
      GtkWidget *button2;
      GtkWidget *frame;
      GtkWidget *box;

      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_idle_test),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Idle Test");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      label = gtk_label_new ("count: 0");
      gtk_misc_set_padding (GTK_MISC (label), 10, 10);
      gtk_widget_show (label);
      
      container =
	gtk_widget_new (GTK_TYPE_HBOX,
			"visible", TRUE,
			/* "GtkContainer::child", gtk_widget_new (GTK_TYPE_HBOX,
			 * "GtkWidget::visible", TRUE,
			 */
			 "child", label,
			/* NULL), */
			NULL);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), 
			  container, TRUE, TRUE, 0);

      frame =
	gtk_widget_new (GTK_TYPE_FRAME,
			"border_width", 5,
			"label", "Label Container",
			"visible", TRUE,
			"parent", GTK_DIALOG (window)->vbox,
			NULL);
      box =
	gtk_widget_new (GTK_TYPE_VBOX,
			"visible", TRUE,
			"parent", frame,
			NULL);
      button =
	g_object_connect (gtk_widget_new (GTK_TYPE_RADIO_BUTTON,
					  "label", "Resize-Parent",
					  "user_data", (void*)GTK_RESIZE_PARENT,
					  "visible", TRUE,
					  "parent", box,
					  NULL),
			  "signal::clicked", toggle_idle_container, container,
			  NULL);
      button = gtk_widget_new (GTK_TYPE_RADIO_BUTTON,
			       "label", "Resize-Queue",
			       "user_data", (void*)GTK_RESIZE_QUEUE,
			       "group", button,
			       "visible", TRUE,
			       "parent", box,
			       NULL);
      g_object_connect (button,
			"signal::clicked", toggle_idle_container, container,
			NULL);
      button2 = gtk_widget_new (GTK_TYPE_RADIO_BUTTON,
				"label", "Resize-Immediate",
				"user_data", (void*)GTK_RESIZE_IMMEDIATE,
				NULL);
      g_object_connect (button2,
			"signal::clicked", toggle_idle_container, container,
			NULL);
      g_object_set (button2,
		    "group", button,
		    "visible", TRUE,
		    "parent", box,
		    NULL);

      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("start");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(start_idle_test),
			  label);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("stop");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(stop_idle_test),
			  NULL);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * rc file test
 */

void
reload_all_rc_files (void)
{
  static GdkAtom atom_rcfiles = GDK_NONE;

  GdkEventClient sev;
  int i;
  
  if (!atom_rcfiles)
    atom_rcfiles = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);

  for(i = 0; i < 5; i++)
    sev.data.l[i] = 0;
  sev.data_format = 32;
  sev.message_type = atom_rcfiles;
  gdk_event_send_clientmessage_toall ((GdkEvent *) &sev);
}

void
create_rc_file (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(destroy_idle_test),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Reload Rc file");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      button = gtk_button_new_with_label ("Reload");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(gtk_rc_reparse_all), NULL);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Reload All");
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC(reload_all_rc_files), NULL);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Test of recursive mainloop
 */

void
mainloop_destroyed (GtkWidget *w, GtkWidget **window)
{
  *window = NULL;
  gtk_main_quit ();
}

void
create_mainloop (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_dialog_new ();

      gtk_window_set_title (GTK_WINDOW (window), "Test Main Loop");

      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(mainloop_destroyed),
			  &window);

      label = gtk_label_new ("In recursive main loop...");
      gtk_misc_set_padding (GTK_MISC(label), 20, 20);

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), label,
			  TRUE, TRUE, 0);
      gtk_widget_show (label);

      button = gtk_button_new_with_label ("Leave");
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), button, 
			  FALSE, TRUE, 0);

      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_destroy),
				 GTK_OBJECT (window));

      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);

      gtk_widget_show (button);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    {
      gtk_widget_show (window);

      g_print ("create_mainloop: start\n");
      gtk_main ();
      g_print ("create_mainloop: done\n");
    }
  else
    gtk_widget_destroy (window);
}

gboolean
layout_expose_handler (GtkWidget *widget, GdkEventExpose *event)
{
  GtkLayout *layout;

  gint i,j;
  gint imin, imax, jmin, jmax;

  layout = GTK_LAYOUT (widget);

  if (event->window != layout->bin_window)
    return FALSE;
  
  imin = (event->area.x) / 10;
  imax = (event->area.x + event->area.width + 9) / 10;

  jmin = (event->area.y) / 10;
  jmax = (event->area.y + event->area.height + 9) / 10;

  for (i=imin; i<imax; i++)
    for (j=jmin; j<jmax; j++)
      if ((i+j) % 2)
	gdk_draw_rectangle (layout->bin_window,
			    widget->style->black_gc,
			    TRUE,
			    10*i, 10*j, 
			    1+i%10, 1+j%10);
  
  return FALSE;
}

void create_layout (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *layout;
  GtkWidget *scrolledwindow;
  GtkWidget *button;

  if (!window)
    {
      gchar buf[16];

      gint i, j;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      gtk_window_set_title (GTK_WINDOW (window), "Layout");
      gtk_widget_set_usize (window, 200, 200);

      scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
					   GTK_SHADOW_IN);
      gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (scrolledwindow),
					 GTK_CORNER_TOP_RIGHT);

      gtk_container_add (GTK_CONTAINER (window), scrolledwindow);
      
      layout = gtk_layout_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (scrolledwindow), layout);

      /* We set step sizes here since GtkLayout does not set
       * them itself.
       */
      GTK_LAYOUT (layout)->hadjustment->step_increment = 10.0;
      GTK_LAYOUT (layout)->vadjustment->step_increment = 10.0;
      
      gtk_widget_set_events (layout, GDK_EXPOSURE_MASK);
      gtk_signal_connect (GTK_OBJECT (layout), "expose_event",
			  GTK_SIGNAL_FUNC (layout_expose_handler), NULL);
      
      gtk_layout_set_size (GTK_LAYOUT (layout), 1600, 128000);
      
      for (i=0 ; i < 16 ; i++)
	for (j=0 ; j < 16 ; j++)
	  {
	    sprintf(buf, "Button %d, %d", i, j);
	    if ((i + j) % 2)
	      button = gtk_button_new_with_label (buf);
	    else
	      button = gtk_label_new (buf);

	    gtk_layout_put (GTK_LAYOUT (layout), button,
			    j*100, i*100);
	  }

      for (i=16; i < 1280; i++)
	{
	  sprintf(buf, "Button %d, %d", i, 0);
	  if (i % 2)
	    button = gtk_button_new_with_label (buf);
	  else
	    button = gtk_label_new (buf);

	  gtk_layout_put (GTK_LAYOUT (layout), button,
			  0, i*100);
	}
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

void
create_styles (void)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *entry;
  GtkWidget *vbox;
  static GdkColor red =    { 0, 0xffff, 0,      0      };
  static GdkColor green =  { 0, 0,      0xffff, 0      };
  static GdkColor blue =   { 0, 0,      0,      0xffff };
  static GdkColor yellow = { 0, 0xffff, 0xffff, 0      };
  static GdkColor cyan =   { 0, 0     , 0xffff, 0xffff };
  PangoFontDescription *font_desc;

  GtkRcStyle *rc_style;

  if (!window)
    {
      window = gtk_dialog_new ();
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			  &window);

      
      button = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC(gtk_widget_destroy),
				 GTK_OBJECT (window));
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), 
			  button, TRUE, TRUE, 0);
      gtk_widget_show (button);

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox, FALSE, FALSE, 0);
      
      label = gtk_label_new ("Font:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      font_desc = pango_font_description_from_string ("Helvetica,Sans Oblique 18");

      button = gtk_button_new_with_label ("Some Text");
      gtk_widget_modify_font (GTK_BIN (button)->child, font_desc);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      label = gtk_label_new ("Foreground:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Some Text");
      gtk_widget_modify_fg (GTK_BIN (button)->child, GTK_STATE_NORMAL, &red);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      label = gtk_label_new ("Background:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Some Text");
      gtk_widget_modify_bg (button, GTK_STATE_NORMAL, &green);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

      label = gtk_label_new ("Text:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "Some Text");
      gtk_widget_modify_text (entry, GTK_STATE_NORMAL, &blue);
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

      label = gtk_label_new ("Base:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry), "Some Text");
      gtk_widget_modify_base (entry, GTK_STATE_NORMAL, &yellow);
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

      label = gtk_label_new ("Multiple:");
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Some Text");

      rc_style = gtk_rc_style_new ();

      rc_style->font_desc = pango_font_description_copy (font_desc);
      rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_FG | GTK_RC_BG;
      rc_style->color_flags[GTK_STATE_PRELIGHT] = GTK_RC_FG | GTK_RC_BG;
      rc_style->color_flags[GTK_STATE_ACTIVE] = GTK_RC_FG | GTK_RC_BG;
      rc_style->fg[GTK_STATE_NORMAL] = yellow;
      rc_style->bg[GTK_STATE_NORMAL] = blue;
      rc_style->fg[GTK_STATE_PRELIGHT] = blue;
      rc_style->bg[GTK_STATE_PRELIGHT] = yellow;
      rc_style->fg[GTK_STATE_ACTIVE] = red;
      rc_style->bg[GTK_STATE_ACTIVE] = cyan;
      rc_style->xthickness = 5;
      rc_style->ythickness = 5;

      gtk_widget_modify_style (button, rc_style);
      gtk_widget_modify_style (GTK_BIN (button)->child, rc_style);

      g_object_unref (G_OBJECT (rc_style));
      
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    }
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);
}

/*
 * Main Window and Exit
 */

void
do_exit (GtkWidget *widget, GtkWidget *window)
{
  gtk_widget_destroy (window);
  gtk_main_quit ();
}

struct {
  char *label;
  void (*func) ();
  gboolean do_not_benchmark;
} buttons[] =
{
  { "big windows", create_big_windows },
  { "button box", create_button_box },
  { "buttons", create_buttons },
  { "check buttons", create_check_buttons },
  { "clist", create_clist},
  { "color selection", create_color_selection },
  { "ctree", create_ctree },
  { "cursors", create_cursors },
  { "dialog", create_dialog },
  { "entry", create_entry },
  { "event watcher", create_event_watcher },
  { "file selection", create_file_selection },
  { "flipping", create_flipping },
  { "focus", create_focus },
  { "font selection", create_font_selection },
  { "gamma curve", create_gamma_curve, TRUE },
  { "handle box", create_handle_box },
  { "image from drawable", create_get_image },
  { "image", create_image },
  { "item factory", create_item_factory },
  { "labels", create_labels },
  { "layout", create_layout },
  { "list", create_list },
  { "menus", create_menus },
  { "message dialog", create_message_dialog },
  { "modal window", create_modal_window, TRUE },
  { "notebook", create_notebook },
  { "panes", create_panes },
  { "pixmap", create_pixmap },
  { "preview color", create_color_preview, TRUE },
  { "preview gray", create_gray_preview, TRUE },
  { "progress bar", create_progress_bar },
  { "properties", create_properties },
  { "radio buttons", create_radio_buttons },
  { "range controls", create_range_controls },
  { "rc file", create_rc_file },
  { "reparent", create_reparent },
  { "rulers", create_rulers },
  { "saved position", create_saved_position },
  { "scrolled windows", create_scrolled_windows },
  { "shapes", create_shapes },
  { "size groups", create_size_groups },
  { "spinbutton", create_spins },
  { "statusbar", create_statusbar },
  { "styles", create_styles },
  { "test idle", create_idle_test },
  { "test mainloop", create_mainloop, TRUE },
  { "test scrolling", create_scroll_test },
  { "test selection", create_selection_test },
  { "test timeout", create_timeout_test },
  { "text", create_text },
  { "toggle buttons", create_toggle_buttons },
  { "toolbar", create_toolbar },
  { "tooltips", create_tooltips },
  { "tree", create_tree_mode_window},
  { "WM hints", create_wmhints },
  { "window sizing", create_window_sizing },
  { "window states", create_window_states }
};
int nbuttons = sizeof (buttons) / sizeof (buttons[0]);

void
create_main_window (void)
{
  GtkWidget *window;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *scrolled_window;
  GtkWidget *button;
  GtkWidget *label;
  gchar buffer[64];
  GtkWidget *separator;
  GdkGeometry geometry;
  int i;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (window, "main window");
  gtk_widget_set_uposition (window, 20, 20);
  gtk_window_set_default_size (GTK_WINDOW (window), -1, 400);

  geometry.min_width = -1;
  geometry.min_height = -1;
  geometry.max_width = -1;
  geometry.max_height = G_MAXSHORT;
  gtk_window_set_geometry_hints (GTK_WINDOW (window), NULL,
				 &geometry,
				 GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC(gtk_main_quit),
		      NULL);
  gtk_signal_connect (GTK_OBJECT (window), "delete-event",
		      GTK_SIGNAL_FUNC (gtk_false),
		      NULL);

  box1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), box1);

  if (gtk_micro_version > 0)
    sprintf (buffer,
	     "Gtk+ v%d.%d.%d",
	     gtk_major_version,
	     gtk_minor_version,
	     gtk_micro_version);
  else
    sprintf (buffer,
	     "Gtk+ v%d.%d",
	     gtk_major_version,
	     gtk_minor_version);

  label = gtk_label_new (buffer);
  gtk_box_pack_start (GTK_BOX (box1), label, FALSE, FALSE, 0);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 10);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
     		                  GTK_POLICY_NEVER, 
                                  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (box1), scrolled_window, TRUE, TRUE, 0);

  box2 = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), box2);
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (box2),
				       gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
  gtk_widget_show (box2);

  for (i = 0; i < nbuttons; i++)
    {
      button = gtk_button_new_with_label (buttons[i].label);
      if (buttons[i].func)
        gtk_signal_connect (GTK_OBJECT (button), 
			    "clicked", 
			    GTK_SIGNAL_FUNC(buttons[i].func),
			    NULL);
      else
        gtk_widget_set_sensitive (button, FALSE);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
    }

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

  box2 = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
  gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

  button = gtk_button_new_with_mnemonic ("_Close");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (do_exit),
		      window);
  gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (button);

  gtk_widget_show_all (window);
}

static void
test_init ()
{
  if (file_exists ("../gdk-pixbuf/.libs/libpixbufloader-pnm.so"))
    {
      putenv ("GDK_PIXBUF_MODULEDIR=../gdk-pixbuf/.libs");
      putenv ("GTK_IM_MODULE_FILE=../modules/input/gtk.immodules");
    }
}

static char *
pad (const char *str, int to)
{
  static char buf[256];
  int len = strlen (str);
  int i;

  for (i = 0; i < to; i++)
    buf[i] = ' ';

  buf[to] = '\0';

  memcpy (buf, str, len);

  return buf;
}

static void
bench_iteration (void (* fn) ())
{
  fn (); /* on */
  while (g_main_iteration (FALSE));
  fn (); /* off */
  while (g_main_iteration (FALSE));
}

void
do_real_bench (void (* fn) (), char *name, int num)
{
  GTimeVal tv0, tv1;
  double dt_first;
  double dt;
  int n;
  static gboolean printed_headers = FALSE;

  if (!printed_headers) {
    g_print ("Test                 Iters      First      Other\n");
    g_print ("-------------------- ----- ---------- ----------\n");
    printed_headers = TRUE;
  }

  g_get_current_time (&tv0);
  bench_iteration (fn); 
  g_get_current_time (&tv1);

  dt_first = ((double)tv1.tv_sec - tv0.tv_sec) * 1000.0
	+ (tv1.tv_usec - tv0.tv_usec) / 1000.0;

  g_get_current_time (&tv0);
  for (n = 0; n < num - 1; n++)
    bench_iteration (fn); 
  g_get_current_time (&tv1);
  dt = ((double)tv1.tv_sec - tv0.tv_sec) * 1000.0
	+ (tv1.tv_usec - tv0.tv_usec) / 1000.0;

  g_print ("%s %5d ", pad (name, 20), num);
  if (num > 1)
    g_print ("%10.1f %10.1f\n", dt_first, dt/(num-1));
  else
    g_print ("%10.1f\n", dt_first);
}

void
do_bench (char* what, int num)
{
  int i;
  void (* fn) ();
  fn = NULL;

  if (g_strcasecmp (what, "ALL") == 0)
    {
      for (i = 0; i < nbuttons; i++)
	{
	  if (!buttons[i].do_not_benchmark)
	    do_real_bench (buttons[i].func, buttons[i].label, num);
	}

      return;
    }
  else
    {
      for (i = 0; i < nbuttons; i++)
	{
	  if (strcmp (buttons[i].label, what) == 0)
	    {
	      fn = buttons[i].func;
	      break;
	    }
	}
      
      if (!fn)
	g_print ("Can't bench: \"%s\" not found.\n", what);
      else
	do_real_bench (fn, buttons[i].label, num);
    }
}

void 
usage (void)
{
  fprintf (stderr, "Usage: testgtk [--bench ALL|<bench>[:<count>]]\n");
  exit (1);
}

int
main (int argc, char *argv[])
{
  GtkBindingSet *binding_set;
  int i;
  gboolean done_benchmarks = FALSE;

  srand (time (NULL));

  test_init ();

  /* Check to see if we are being run from the correct
   * directory.
   */
  if (file_exists ("testgtkrc"))
    gtk_rc_add_default_file ("testgtkrc");

  gtk_init (&argc, &argv);

  /*  benchmarking
   */
  for (i = 1; i < argc; i++)
    {
      if (strncmp (argv[i], "--bench", strlen("--bench")) == 0)
        {
          int num = 1;
	  char *nextarg;
	  char *what;
	  char *count;
	  
	  nextarg = strchr (argv[i], '=');
	  if (nextarg)
	    nextarg++;
	  else
	    {
	      i++;
	      if (i == argc)
		usage ();
	      nextarg = argv[i];
	    }

	  count = strchr (nextarg, ':');
	  if (count)
	    {
	      what = g_strndup (nextarg, count - nextarg);
	      count++;
	      num = atoi (count);
	      if (num <= 0)
		usage ();
	    }
	  else
	    what = g_strdup (nextarg);

          do_bench (what, num ? num : 1);
	  done_benchmarks = TRUE;
        }
      else
	usage ();
    }
  if (done_benchmarks)
    return 0;

  /* bindings test
   */
  binding_set = gtk_binding_set_by_class (gtk_type_class (GTK_TYPE_WIDGET));
  gtk_binding_entry_add_signal (binding_set,
				'9', GDK_CONTROL_MASK | GDK_RELEASE_MASK,
				"debug_msg",
				1,
				GTK_TYPE_STRING, "GtkWidgetClass <ctrl><release>9 test");

  create_main_window ();

  gtk_main ();

  if (1)
    {
      while (g_main_pending ())
	g_main_iteration (FALSE);
#if 0
      sleep (1);
      while (g_main_pending ())
	g_main_iteration (FALSE);
#endif
    }
  return 0;
}
