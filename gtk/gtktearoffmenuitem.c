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

#include <config.h>
#include "gtkmenu.h"
#include "gtktearoffmenuitem.h"

#define ARROW_SIZE 10
#define TEAR_LENGTH 5
#define BORDER_SPACING  3

static void gtk_tearoff_menu_item_class_init (GtkTearoffMenuItemClass *klass);
static void gtk_tearoff_menu_item_init       (GtkTearoffMenuItem      *tearoff_menu_item);
static void gtk_tearoff_menu_item_size_request (GtkWidget             *widget,
				                GtkRequisition        *requisition);
static gint gtk_tearoff_menu_item_expose     (GtkWidget             *widget,
					      GdkEventExpose        *event);
static void gtk_tearoff_menu_item_activate   (GtkMenuItem           *menu_item);
static gint gtk_tearoff_menu_item_delete_cb  (GtkMenuItem           *menu_item,
					      GdkEventAny           *event);

GType
gtk_tearoff_menu_item_get_type (void)
{
  static GType tearoff_menu_item_type = 0;

  if (!tearoff_menu_item_type)
    {
      static const GTypeInfo tearoff_menu_item_info =
      {
        sizeof (GtkTearoffMenuItemClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_tearoff_menu_item_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkTearoffMenuItem),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_tearoff_menu_item_init,
      };

      tearoff_menu_item_type =
	g_type_register_static (GTK_TYPE_MENU_ITEM, "GtkTearoffMenuItem",
			        &tearoff_menu_item_info, 0);
    }

  return tearoff_menu_item_type;
}

GtkWidget*
gtk_tearoff_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_TEAROFF_MENU_ITEM, NULL);
}

static void
gtk_tearoff_menu_item_class_init (GtkTearoffMenuItemClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  menu_item_class = (GtkMenuItemClass*) klass;

  widget_class->expose_event = gtk_tearoff_menu_item_expose;
  widget_class->size_request = gtk_tearoff_menu_item_size_request;

  menu_item_class->activate = gtk_tearoff_menu_item_activate;
}

static void
gtk_tearoff_menu_item_init (GtkTearoffMenuItem *tearoff_menu_item)
{
  tearoff_menu_item->torn_off = FALSE;
}

static void
gtk_tearoff_menu_item_size_request (GtkWidget      *widget,
				    GtkRequisition *requisition)
{
  GtkTearoffMenuItem *tearoff;

  tearoff = GTK_TEAROFF_MENU_ITEM (widget);
  
  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->xthickness +
			BORDER_SPACING) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width +
			 widget->style->ythickness) * 2;

  if (tearoff->torn_off)
    {
      requisition->height += ARROW_SIZE;
    }
  else
    {
      requisition->height += widget->style->ythickness + 4;
    }
}

static void
gtk_tearoff_menu_item_paint (GtkWidget   *widget,
			     GdkRectangle *area)
{
  GtkMenuItem *menu_item;
  GtkTearoffMenuItem *tearoff_item;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;
  gint right_max;
  GtkArrowType arrow_type;
  GtkTextDirection direction;
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      menu_item = GTK_MENU_ITEM (widget);
      tearoff_item = GTK_TEAROFF_MENU_ITEM (widget);

      direction = gtk_widget_get_direction (widget);

      x = widget->allocation.x + GTK_CONTAINER (menu_item)->border_width;
      y = widget->allocation.y + GTK_CONTAINER (menu_item)->border_width;
      width = widget->allocation.width - GTK_CONTAINER (menu_item)->border_width * 2;
      height = widget->allocation.height - GTK_CONTAINER (menu_item)->border_width * 2;
      right_max = x + width;

      if (widget->state == GTK_STATE_PRELIGHT)
	{
	  gint selected_shadow_type;
	  
	  gtk_widget_style_get (widget,
				"selected_shadow_type", &selected_shadow_type,
				NULL);
	  gtk_paint_box (widget->style,
			 widget->window,
			 GTK_STATE_PRELIGHT,
			 selected_shadow_type,
			 area, widget, "menuitem",
			 x, y, width, height);
	}
      else
	gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

      if (tearoff_item->torn_off)
	{
	  gint arrow_x;

	  if (widget->state == GTK_STATE_PRELIGHT)
	    shadow_type = GTK_SHADOW_IN;
	  else
	    shadow_type = GTK_SHADOW_OUT;

	  if (menu_item->toggle_size > ARROW_SIZE)
	    {
	      if (direction == GTK_TEXT_DIR_LTR) {
		arrow_x = x + (menu_item->toggle_size - ARROW_SIZE)/2;
		arrow_type = GTK_ARROW_LEFT;
	      }
	      else {
		arrow_x = x + width - menu_item->toggle_size + (menu_item->toggle_size - ARROW_SIZE)/2; 
		arrow_type = GTK_ARROW_RIGHT;	    
	      }
	      x += menu_item->toggle_size + BORDER_SPACING;
	    }
	  else
	    {
	      if (direction == GTK_TEXT_DIR_LTR) {
		arrow_x = ARROW_SIZE / 2;
		arrow_type = GTK_ARROW_LEFT;
	      }
	      else {
		arrow_x = x + width - 2 * ARROW_SIZE + ARROW_SIZE / 2; 
		arrow_type = GTK_ARROW_RIGHT;	    
	      }
	      x += 2 * ARROW_SIZE;
	    }


	  gtk_paint_arrow (widget->style, widget->window,
			   widget->state, shadow_type,
			   NULL, widget, "tearoffmenuitem",
			   arrow_type, FALSE,
			   arrow_x, y + height / 2 - 5, 
			   ARROW_SIZE, ARROW_SIZE);
	}

      while (x < right_max)
	{
	  gint x1, x2;

	  if (direction == GTK_TEXT_DIR_LTR) {
	    x1 = x;
	    x2 = MIN (x + TEAR_LENGTH, right_max);
	  }
	  else {
	    x1 = right_max - x;
	    x2 = MAX (right_max - x - TEAR_LENGTH, 0);
	  }
	  
	  gtk_paint_hline (widget->style, widget->window, GTK_STATE_NORMAL,
			   NULL, widget, "tearoffmenuitem",
			   x1, x2, y + (height - widget->style->ythickness) / 2);
	  x += 2 * TEAR_LENGTH;
	}
    }
}

static gint
gtk_tearoff_menu_item_expose (GtkWidget      *widget,
			    GdkEventExpose *event)
{
  gtk_tearoff_menu_item_paint (widget, &event->area);

  return FALSE;
}

static gint
gtk_tearoff_menu_item_delete_cb (GtkMenuItem *menu_item, GdkEventAny *event)
{
  gtk_tearoff_menu_item_activate (menu_item);
  return TRUE;
}

static void
gtk_tearoff_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkTearoffMenuItem *tearoff_menu_item = GTK_TEAROFF_MENU_ITEM (menu_item);

  tearoff_menu_item->torn_off = !tearoff_menu_item->torn_off;
  gtk_widget_queue_resize (GTK_WIDGET (menu_item));

  if (GTK_IS_MENU (GTK_WIDGET (menu_item)->parent))
    {
      GtkMenu *menu = GTK_MENU (GTK_WIDGET (menu_item)->parent);
      gboolean need_connect;
      
      	need_connect = (tearoff_menu_item->torn_off && !menu->tearoff_window);

	gtk_menu_set_tearoff_state (GTK_MENU (GTK_WIDGET (menu_item)->parent),
				    tearoff_menu_item->torn_off);

	if (need_connect)
	  g_signal_connect_swapped (menu->tearoff_window,
				    "delete_event",
				    G_CALLBACK (gtk_tearoff_menu_item_delete_cb),
				    menu_item);
    }
}

