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
#include "gtkvpaned.h"
#include "gtkmain.h"
#include "gtksignal.h"

static void gtk_vpaned_class_init       (GtkVPanedClass *klass);
static void gtk_vpaned_init             (GtkVPaned      *vpaned);
static void gtk_vpaned_size_request     (GtkWidget      *widget,
					 GtkRequisition *requisition);
static void gtk_vpaned_size_allocate    (GtkWidget          *widget,
					 GtkAllocation      *allocation);
static void gtk_vpaned_draw             (GtkWidget    *widget,
					 GdkRectangle *area);
static void gtk_vpaned_xor_line         (GtkPaned *paned);
static gint gtk_vpaned_button_press     (GtkWidget *widget,
					 GdkEventButton *event);
static gint gtk_vpaned_button_release   (GtkWidget *widget,
					 GdkEventButton *event);
static gint gtk_vpaned_motion           (GtkWidget *widget,
					 GdkEventMotion *event);

guint
gtk_vpaned_get_type ()
{
  static guint vpaned_type = 0;

  if (!vpaned_type)
    {
      GtkTypeInfo vpaned_info =
      {
	"GtkVPaned",
	sizeof (GtkVPaned),
	sizeof (GtkVPanedClass),
	(GtkClassInitFunc) gtk_vpaned_class_init,
	(GtkObjectInitFunc) gtk_vpaned_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      vpaned_type = gtk_type_unique (gtk_paned_get_type (), &vpaned_info);
    }

  return vpaned_type;
}

static void
gtk_vpaned_class_init (GtkVPanedClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->size_request = gtk_vpaned_size_request;
  widget_class->size_allocate = gtk_vpaned_size_allocate;
  widget_class->draw = gtk_vpaned_draw;
  widget_class->button_press_event = gtk_vpaned_button_press;
  widget_class->button_release_event = gtk_vpaned_button_release;
  widget_class->motion_notify_event = gtk_vpaned_motion;
}

static void
gtk_vpaned_init (GtkVPaned *vpaned)
{
}

GtkWidget*
gtk_vpaned_new ()
{
  GtkVPaned *vpaned;

  vpaned = gtk_type_new (gtk_vpaned_get_type ());

  return GTK_WIDGET (vpaned);
}

static void
gtk_vpaned_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkPaned *paned;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VPANED (widget));
  g_return_if_fail (requisition != NULL);

  paned = GTK_PANED (widget);
  requisition->width = 0;
  requisition->height = 0;

  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
    {
      gtk_widget_size_request (paned->child1, &paned->child1->requisition);

      requisition->height = paned->child1->requisition.height;
      requisition->width = paned->child1->requisition.width;
    }

  if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    {
      gtk_widget_size_request (paned->child2, &paned->child2->requisition);

      requisition->width = MAX (requisition->width,
				paned->child2->requisition.width);
      requisition->height += paned->child2->requisition.height;
    }

  requisition->height += GTK_CONTAINER (paned)->border_width * 2 + paned->gutter_size;
  requisition->width += GTK_CONTAINER (paned)->border_width * 2;
}

static void
gtk_vpaned_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkPaned *paned;
  GtkAllocation child1_allocation;
  GtkAllocation child2_allocation;
  guint16 border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VPANED (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;

  paned = GTK_PANED (widget);
  border_width = GTK_CONTAINER (widget)->border_width;

  if (!paned->position_set)
    {
      if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
	paned->child1_size = paned->child1->requisition.height;
      else
	paned->child1_size = 0;
    }
  else
    paned->child1_size = CLAMP (paned->child1_size, 0,
				allocation->height - paned->gutter_size
				- 2 * GTK_CONTAINER (paned)->border_width);

  /* Move the handle before the children so we don't get extra expose events */

  paned->handle_xpos = allocation->width - border_width - 2 * paned->handle_size;
  paned->handle_ypos = paned->child1_size + border_width + paned->gutter_size / 2 - paned->handle_size / 2;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      
      gdk_window_move (paned->handle, paned->handle_xpos, paned->handle_ypos);
    }

  if (GTK_WIDGET_MAPPED (widget))
    {
      gdk_window_clear_area (widget->window,
			     paned->groove_rectangle.x,
			     paned->groove_rectangle.y,
			     paned->groove_rectangle.width,
			     paned->groove_rectangle.height);
    }
  
  child1_allocation.width = child2_allocation.width = MAX (0, allocation->width - border_width * 2);
  child1_allocation.height = paned->child1_size;
  child1_allocation.x = child2_allocation.x = border_width;
  child1_allocation.y = border_width;
  
  paned->groove_rectangle.y = child1_allocation.y 
    + child1_allocation.height + paned->gutter_size / 2 - 1;
  paned->groove_rectangle.x = 0;
  paned->groove_rectangle.height = 2;
  paned->groove_rectangle.width = allocation->width;
  
  child2_allocation.y = paned->groove_rectangle.y + paned->gutter_size / 2 + 1;
  child2_allocation.height = MAX (0, allocation->height 
    - child2_allocation.y - border_width);
  
  /* Now allocate the childen, making sure, when resizing not to
   * overlap the windows */
  if (GTK_WIDGET_MAPPED(widget) &&
      paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child1->allocation.height < child1_allocation.height)
    {
      if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
	gtk_widget_size_allocate (paned->child2, &child2_allocation);
      gtk_widget_size_allocate (paned->child1, &child1_allocation);      
    }
  else
    {
      if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
	gtk_widget_size_allocate (paned->child1, &child1_allocation);
      if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
	gtk_widget_size_allocate (paned->child2, &child2_allocation);
    }
}

static void
gtk_vpaned_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  GtkPaned *paned;
  GdkRectangle child_area;
  guint16 border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PANED (widget));

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      paned = GTK_PANED (widget);
      border_width = GTK_CONTAINER (paned)->border_width;

      if (paned->child1 &&
	  gtk_widget_intersect (paned->child1, area, &child_area))
        gtk_widget_draw (paned->child1, &child_area);
      if (paned->child2 &&
	  gtk_widget_intersect (paned->child2, area, &child_area))
        gtk_widget_draw (paned->child2, &child_area);

      gdk_draw_line (widget->window,
		     widget->style->dark_gc[widget->state],
		     0,
		     border_width + paned->child1_size + paned->gutter_size / 2 - 1,
		     widget->allocation.width - 1,
		     border_width + paned->child1_size + paned->gutter_size / 2 - 1);
      gdk_draw_line (widget->window,
		     widget->style->light_gc[widget->state],
		     0,
		     border_width + paned->child1_size + paned->gutter_size / 2,
		     widget->allocation.width - 1,
		     border_width + paned->child1_size + paned->gutter_size / 2);
    }
}

static void
gtk_vpaned_xor_line (GtkPaned *paned)
{
  GtkWidget *widget;
  GdkGCValues values;
  guint16 ypos;

  widget = GTK_WIDGET(paned);

  if (!paned->xor_gc)
    {
      values.foreground = widget->style->white;
      values.function = GDK_XOR;
      values.subwindow_mode = GDK_INCLUDE_INFERIORS;
      paned->xor_gc = gdk_gc_new_with_values (widget->window,
					      &values,
					      GDK_GC_FOREGROUND |
					      GDK_GC_FUNCTION |
					      GDK_GC_SUBWINDOW);
    }

  ypos = paned->child1_size
    + GTK_CONTAINER (paned)->border_width + paned->gutter_size / 2;

  gdk_draw_line (widget->window, paned->xor_gc,
		 0,
		 ypos,
		 widget->allocation.width - 1,
		 ypos);
}

static gint
gtk_vpaned_button_press (GtkWidget *widget, GdkEventButton *event)
{
  GtkPaned *paned;

  g_return_val_if_fail (widget != NULL,FALSE);
  g_return_val_if_fail (GTK_IS_PANED (widget),FALSE);

  paned = GTK_PANED (widget);

  if (!paned->in_drag &&
      (event->window == paned->handle) && (event->button == 1))
    {
      paned->in_drag = TRUE;
      /* We need a server grab here, not gtk_grab_add(), since
       * we don't want to pass events on to the widget's children */
      gdk_pointer_grab (paned->handle, FALSE,
			GDK_POINTER_MOTION_HINT_MASK 
			| GDK_BUTTON1_MOTION_MASK 
			| GDK_BUTTON_RELEASE_MASK,
			NULL, NULL, event->time);
      paned->child1_size += event->y - paned->handle_size / 2;
      paned->child1_size = CLAMP (paned->child1_size, 0,
                                  widget->allocation.height - paned->gutter_size
                                  - 2 * GTK_CONTAINER (paned)->border_width);
      gtk_vpaned_xor_line (paned);
    }

  return TRUE;
}

static gint
gtk_vpaned_button_release (GtkWidget *widget, GdkEventButton *event)
{
  GtkPaned *paned;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PANED (widget), FALSE);

  paned = GTK_PANED (widget);

  if (paned->in_drag && (event->button == 1))
    {
      gtk_vpaned_xor_line (paned);
      paned->in_drag = FALSE;
      paned->position_set = TRUE;
      gdk_pointer_ungrab (event->time);
      gtk_widget_queue_resize (GTK_WIDGET (paned));
    }

  return TRUE;
}

static gint
gtk_vpaned_motion (GtkWidget *widget, GdkEventMotion *event)
{
  GtkPaned *paned;
  gint y;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PANED (widget), FALSE);

  if (event->is_hint || event->window != widget->window)
      gtk_widget_get_pointer(widget, NULL, &y);
  else
      y = event->y;

  paned = GTK_PANED (widget);

  if (paned->in_drag)
    {
      gtk_vpaned_xor_line (paned);
      paned->child1_size = y - GTK_CONTAINER (paned)->border_width -
	paned->gutter_size/2;
      paned->child1_size = CLAMP (paned->child1_size, 0,
                                  widget->allocation.height - paned->gutter_size
                                  - 2 * GTK_CONTAINER (paned)->border_width);
      gtk_vpaned_xor_line (paned);
    }

  return TRUE;
}
