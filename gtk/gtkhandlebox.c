/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "gtksignal.h"
#include "gtkhandlebox.h"
#include <gdk/gdkx.h>

#define DRAG_HANDLE_SIZE 10

static void gtk_handle_box_class_init    (GtkHandleBoxClass *klass);
static void gtk_handle_box_init          (GtkHandleBox      *handle_box);
static void gtk_handle_box_realize       (GtkWidget        *widget);
static void gtk_handle_box_size_request  (GtkWidget *widget,
					  GtkRequisition   *requisition);
static void gtk_handle_box_size_allocate (GtkWidget *widget,
					  GtkAllocation *allocation);
static void gtk_handle_box_paint         (GtkWidget *widget,
					  GdkRectangle *area);
static void gtk_handle_box_draw          (GtkWidget *widget,
					  GdkRectangle *area);
static gint gtk_handle_box_expose        (GtkWidget *widget,
					  GdkEventExpose *event);
static gint gtk_handle_box_button_changed(GtkWidget *widget,
					  GdkEventButton *event);
static gint gtk_handle_box_motion        (GtkWidget *widget,
					  GdkEventMotion *event);


guint
gtk_handle_box_get_type ()
{
  static guint handle_box_type = 0;

  if (!handle_box_type)
    {
      GtkTypeInfo handle_box_info =
      {
	"GtkHandleBox",
	sizeof (GtkHandleBox),
	sizeof (GtkHandleBoxClass),
	(GtkClassInitFunc) gtk_handle_box_class_init,
	(GtkObjectInitFunc) gtk_handle_box_init,
	(GtkArgFunc) NULL,
      };

      handle_box_type = gtk_type_unique (gtk_event_box_get_type (), &handle_box_info);
    }

  return handle_box_type;
}

static void
gtk_handle_box_class_init (GtkHandleBoxClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;
  widget_class->realize = gtk_handle_box_realize;
  widget_class->size_request = gtk_handle_box_size_request;
  widget_class->size_allocate = gtk_handle_box_size_allocate;
  widget_class->draw = gtk_handle_box_draw;
  widget_class->expose_event = gtk_handle_box_expose;
  widget_class->button_press_event = gtk_handle_box_button_changed;
  widget_class->button_release_event = gtk_handle_box_button_changed;
  widget_class->motion_notify_event = gtk_handle_box_motion;
}

static void
gtk_handle_box_init (GtkHandleBox *handle_box)
{
  GTK_WIDGET_UNSET_FLAGS (handle_box, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (handle_box, GTK_BASIC);
  handle_box->is_being_dragged = FALSE;
  handle_box->real_parent = NULL;
}

GtkWidget*
gtk_handle_box_new ()
{
  return GTK_WIDGET ( gtk_type_new (gtk_handle_box_get_type ()));
}

static void
gtk_handle_box_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
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

  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_handle_box_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);

  requisition->width = DRAG_HANDLE_SIZE;
  requisition->height = DRAG_HANDLE_SIZE;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &bin->child->requisition);

      requisition->width += bin->child->requisition.width;
      if(bin->child->requisition.height > requisition->height)
	requisition->height = bin->child->requisition.height;
    }
}

static void
gtk_handle_box_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  bin = GTK_BIN (widget);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = allocation->width - DRAG_HANDLE_SIZE;
  child_allocation.height = allocation->height;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + DRAG_HANDLE_SIZE,
			      allocation->y,
			      child_allocation.width,
			      child_allocation.height);
    }
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void gtk_handle_box_paint(GtkWidget *widget,
				 GdkRectangle *area)
{
  gint startx, endx, x;
  gint line_y2;
  g_print("painting %dx%d+%d+%d\n",
	  area->x, area->y, area->width, area->height);

  
  startx = 1; endx = DRAG_HANDLE_SIZE;
  if(area->x > startx)
    startx = area->x;
  if((area->x + area->width) < endx)
    endx = area->x + area->width;
  line_y2 = area->y + area->height;
  for(x = startx; x < DRAG_HANDLE_SIZE; x += 3)
    gtk_draw_vline(widget->style, widget->window,
		   GTK_WIDGET_STATE(widget),
		   area->y, line_y2,
		   x);
}

static void
gtk_handle_box_draw (GtkWidget    *widget,
		   GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);
      
      gtk_handle_box_paint(widget, area);
      if (bin->child)
	{
	  if (gtk_widget_intersect (bin->child, area, &child_area))
	    gtk_widget_draw (bin->child, &child_area);
	}
    }
}

static gint
gtk_handle_box_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);
      gtk_handle_box_paint(widget, &event->area);

      child_event = *event;
      if (bin->child &&
	  GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
    }

  return FALSE;
}

static gint dragoff_x, dragoff_y;

static gint gtk_handle_box_button_changed(GtkWidget *widget,
					  GdkEventButton *event)
{
  GtkHandleBox *hb;
  gint rootx, rooty;
  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_HANDLE_BOX(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  hb = GTK_HANDLE_BOX(widget);
  if(event->button == 1)
    {
      if(event->type == GDK_BUTTON_PRESS
	 && event->x < DRAG_HANDLE_SIZE)
	{
	  hb->is_being_dragged = TRUE;
	  dragoff_x = event->x;
	  dragoff_y = event->y;
	  if(!hb->real_parent) {
	    hb->real_parent = widget->parent;
	    gdk_window_set_override_redirect(widget->window, TRUE);
	    rootx = event->x_root - event->x;
	    rooty = event->y_root - event->y;
	    gdk_window_reparent(widget->window, GDK_ROOT_PARENT(),
				rootx, rooty);
	    g_print("Reparenting to %dx%d (%dx%d)\n", rootx, rooty,
		    (gint)event->x_root, (gint)event->y_root);
	  } else
	    gdk_window_raise(widget->window);
	}
      else if(event->type == GDK_BUTTON_RELEASE)
	{
	  hb->is_being_dragged = FALSE;
	}
    }
  return TRUE;
}

static gint gtk_handle_box_motion        (GtkWidget *widget,
					  GdkEventMotion *event)
{
  GtkHandleBox *hb;
  gint newx, newy;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_HANDLE_BOX(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  hb = GTK_HANDLE_BOX(widget);
  if(hb->is_being_dragged) {
    newx = event->x_root - dragoff_x;
    newy = event->y_root - dragoff_y;
    if(newx < 0) newx = 0;
    if(newy < 0) newy = 0;
    gdk_window_move(widget->window, newx,
		    newy);
  }
  return TRUE;
}
