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

#include "gtkeventbox.h"


static void gtk_event_box_class_init               (GtkEventBoxClass *klass);
static void gtk_event_box_init                     (GtkEventBox      *event_box);
static void gtk_event_box_realize                  (GtkWidget        *widget);
static void gtk_event_box_size_request             (GtkWidget        *widget,
						    GtkRequisition   *requisition);
static void gtk_event_box_size_allocate            (GtkWidget        *widget,
						    GtkAllocation    *allocation);
static void gtk_event_box_paint                    (GtkWidget         *widget,
						    GdkRectangle      *area);
static gint gtk_event_box_expose                   (GtkWidget         *widget,
						   GdkEventExpose     *event);


static GtkBinClass *parent_class = NULL;

GType
gtk_event_box_get_type (void)
{
  static GType event_box_type = 0;

  if (!event_box_type)
    {
      static const GTypeInfo event_box_info =
      {
	sizeof (GtkEventBoxClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_event_box_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkEventBox),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_event_box_init,
      };

      event_box_type = g_type_register_static (GTK_TYPE_BIN, "GtkEventBox",
					       &event_box_info, 0);
    }

  return event_box_type;
}

static void
gtk_event_box_class_init (GtkEventBoxClass *class)
{
  GtkWidgetClass *widget_class;

  parent_class = g_type_class_peek_parent (class);
  
  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = gtk_event_box_realize;
  widget_class->size_request = gtk_event_box_size_request;
  widget_class->size_allocate = gtk_event_box_size_allocate;
  widget_class->expose_event = gtk_event_box_expose;
}

static void
gtk_event_box_init (GtkEventBox *event_box)
{
  GTK_WIDGET_UNSET_FLAGS (event_box, GTK_NO_WINDOW);
}

GtkWidget*
gtk_event_box_new (void)
{
  return g_object_new (GTK_TYPE_EVENT_BOX, NULL);
}

static void
gtk_event_box_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;
  
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - 2*border_width;
  attributes.height = widget->allocation.height - 2*border_width;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget)
			| GDK_BUTTON_MOTION_MASK
			| GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_EXPOSURE_MASK
			| GDK_ENTER_NOTIFY_MASK
			| GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_event_box_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkBin *bin = GTK_BIN (widget);

  requisition->width = GTK_CONTAINER (widget)->border_width * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
gtk_event_box_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;

  widget->allocation = *allocation;
  bin = GTK_BIN (widget);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = MAX (allocation->width - GTK_CONTAINER (widget)->border_width * 2, 0);
  child_allocation.height = MAX (allocation->height - GTK_CONTAINER (widget)->border_width * 2, 0);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + GTK_CONTAINER (widget)->border_width,
			      allocation->y + GTK_CONTAINER (widget)->border_width,
			      child_allocation.width,
			      child_allocation.height);
    }
  
  if (bin->child)
    {
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
gtk_event_box_paint (GtkWidget    *widget,
		     GdkRectangle *area)
{
  if (!GTK_WIDGET_APP_PAINTABLE (widget))
    gtk_paint_flat_box (widget->style, widget->window,
			widget->state, GTK_SHADOW_NONE,
			area, widget, "eventbox",
			0, 0, -1, -1);
}

static gint
gtk_event_box_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_event_box_paint (widget, &event->area);
      
      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }

  return FALSE;
}

