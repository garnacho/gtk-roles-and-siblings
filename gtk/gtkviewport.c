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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "gtksignal.h"
#include "gtkviewport.h"

enum {
  ARG_0,
  ARG_HADJUSTMENT,
  ARG_VADJUSTMENT,
  ARG_SHADOW_TYPE
};



static void gtk_viewport_class_init               (GtkViewportClass *klass);
static void gtk_viewport_init                     (GtkViewport      *viewport);
static void gtk_viewport_finalize                 (GtkObject        *object);
static void gtk_viewport_set_arg		  (GtkObject        *object,
						   GtkArg           *arg,
						   guint             arg_id);
static void gtk_viewport_get_arg		  (GtkObject        *object,
						   GtkArg           *arg,
						   guint             arg_id);
static void gtk_viewport_map                      (GtkWidget        *widget);
static void gtk_viewport_unmap                    (GtkWidget        *widget);
static void gtk_viewport_realize                  (GtkWidget        *widget);
static void gtk_viewport_unrealize                (GtkWidget        *widget);
static void gtk_viewport_paint                    (GtkWidget        *widget,
						   GdkRectangle     *area);
static void gtk_viewport_draw                     (GtkWidget        *widget,
						   GdkRectangle     *area);
static gint gtk_viewport_expose                   (GtkWidget        *widget,
						   GdkEventExpose   *event);
static void gtk_viewport_add                      (GtkContainer     *container,
						   GtkWidget        *widget);
static void gtk_viewport_size_request             (GtkWidget        *widget,
						   GtkRequisition   *requisition);
static void gtk_viewport_size_allocate            (GtkWidget        *widget,
						   GtkAllocation    *allocation);
static void gtk_viewport_adjustment_changed       (GtkAdjustment    *adjustment,
						   gpointer          data);
static void gtk_viewport_adjustment_value_changed (GtkAdjustment    *adjustment,
						   gpointer          data);

static GtkBinClass *parent_class;

GtkType
gtk_viewport_get_type (void)
{
  static GtkType viewport_type = 0;

  if (!viewport_type)
    {
      GtkTypeInfo viewport_info =
      {
	"GtkViewport",
	sizeof (GtkViewport),
	sizeof (GtkViewportClass),
	(GtkClassInitFunc) gtk_viewport_class_init,
	(GtkObjectInitFunc) gtk_viewport_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      viewport_type = gtk_type_unique (GTK_TYPE_BIN, &viewport_info);
    }

  return viewport_type;
}

static void
gtk_viewport_class_init (GtkViewportClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;
  parent_class = (GtkBinClass*) gtk_type_class (GTK_TYPE_BIN);

  gtk_object_add_arg_type ("GtkViewport::hadjustment",
			   GTK_TYPE_ADJUSTMENT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
			   ARG_HADJUSTMENT);
  gtk_object_add_arg_type ("GtkViewport::vadjustment",
			   GTK_TYPE_ADJUSTMENT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
			   ARG_VADJUSTMENT);
  gtk_object_add_arg_type ("GtkViewport::shadow_type",
			   GTK_TYPE_SHADOW_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_SHADOW_TYPE);

  object_class->set_arg = gtk_viewport_set_arg;
  object_class->get_arg = gtk_viewport_get_arg;
  object_class->finalize = gtk_viewport_finalize;
  
  widget_class->map = gtk_viewport_map;
  widget_class->unmap = gtk_viewport_unmap;
  widget_class->realize = gtk_viewport_realize;
  widget_class->unrealize = gtk_viewport_unrealize;
  widget_class->draw = gtk_viewport_draw;
  widget_class->expose_event = gtk_viewport_expose;
  widget_class->size_request = gtk_viewport_size_request;
  widget_class->size_allocate = gtk_viewport_size_allocate;

  container_class->add = gtk_viewport_add;
}

static void
gtk_viewport_set_arg (GtkObject        *object,
		      GtkArg           *arg,
		      guint             arg_id)
{
  GtkViewport *viewport;

  viewport = GTK_VIEWPORT (object);

  switch (arg_id)
    {
      GtkAdjustment *adjustment;

    case ARG_HADJUSTMENT:
      g_return_if_fail (viewport->hadjustment == NULL);
      adjustment = GTK_VALUE_POINTER (*arg);
      if (!adjustment)
	adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      gtk_viewport_set_hadjustment (viewport, adjustment);
      break;
    case ARG_VADJUSTMENT:
      g_return_if_fail (viewport->vadjustment == NULL);
      adjustment = GTK_VALUE_POINTER (*arg);
      if (!adjustment)
	adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      gtk_viewport_set_vadjustment (viewport, adjustment);
      break;
    case ARG_SHADOW_TYPE:
      gtk_viewport_set_shadow_type (viewport, GTK_VALUE_ENUM (*arg));
    default:
      break;
    }
}

static void
gtk_viewport_get_arg (GtkObject        *object,
		      GtkArg           *arg,
		      guint             arg_id)
{
  GtkViewport *viewport;

  viewport = GTK_VIEWPORT (object);

  switch (arg_id)
    {
    case ARG_HADJUSTMENT:
      GTK_VALUE_POINTER (*arg) = viewport->hadjustment;
      break;
    case ARG_VADJUSTMENT:
      GTK_VALUE_POINTER (*arg) = viewport->vadjustment;
      break;
    case ARG_SHADOW_TYPE:
      GTK_VALUE_ENUM (*arg) = viewport->shadow_type;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_viewport_init (GtkViewport *viewport)
{
  GTK_WIDGET_UNSET_FLAGS (viewport, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (viewport, GTK_BASIC);

  gtk_container_set_resize_mode (GTK_CONTAINER (viewport), GTK_RESIZE_QUEUE);
  
  viewport->shadow_type = GTK_SHADOW_IN;
  viewport->view_window = NULL;
  viewport->bin_window = NULL;
  viewport->hadjustment = NULL;
  viewport->vadjustment = NULL;
}

GtkWidget*
gtk_viewport_new (GtkAdjustment *hadjustment,
		  GtkAdjustment *vadjustment)
{
  GtkViewport *viewport;

  viewport = gtk_type_new (gtk_viewport_get_type ());

  if (!hadjustment)
    hadjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  if (!vadjustment)
    vadjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  gtk_viewport_set_hadjustment (viewport, hadjustment);
  gtk_viewport_set_vadjustment (viewport, vadjustment);

  return GTK_WIDGET (viewport);
}

static void
gtk_viewport_finalize (GtkObject *object)
{
  GtkViewport *viewport = GTK_VIEWPORT (object);

  gtk_object_unref (GTK_OBJECT (viewport->hadjustment));
  gtk_object_unref (GTK_OBJECT (viewport->vadjustment));

  GTK_OBJECT_CLASS(parent_class)->finalize (object);
}

GtkAdjustment*
gtk_viewport_get_hadjustment (GtkViewport *viewport)
{
  g_return_val_if_fail (viewport != NULL, NULL);
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), NULL);

  return viewport->hadjustment;
}

GtkAdjustment*
gtk_viewport_get_vadjustment (GtkViewport *viewport)
{
  g_return_val_if_fail (viewport != NULL, NULL);
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), NULL);

  return viewport->vadjustment;
}

void
gtk_viewport_set_hadjustment (GtkViewport   *viewport,
			      GtkAdjustment *adjustment)
{
  g_return_if_fail (viewport != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));
  g_return_if_fail (adjustment != NULL);

  if (viewport->hadjustment != adjustment)
    {
      if (viewport->hadjustment)
	{
	  gtk_signal_disconnect_by_data (GTK_OBJECT (viewport->hadjustment),
					 (gpointer) viewport);
	  gtk_object_unref (GTK_OBJECT (viewport->hadjustment));
	}

      viewport->hadjustment = adjustment;
      gtk_object_ref (GTK_OBJECT (viewport->hadjustment));
      gtk_object_sink (GTK_OBJECT (viewport->hadjustment));
      
      gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
			  gtk_viewport_adjustment_changed,
			  viewport);
      gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			  gtk_viewport_adjustment_value_changed,
			  viewport);

      gtk_viewport_adjustment_changed (adjustment, viewport);
    }
}

void
gtk_viewport_set_vadjustment (GtkViewport   *viewport,
			      GtkAdjustment *adjustment)
{
  g_return_if_fail (viewport != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));
  g_return_if_fail (adjustment != NULL);

  if (viewport->vadjustment != adjustment)
    {
      if (viewport->vadjustment)
	{
	  gtk_signal_disconnect_by_data (GTK_OBJECT (viewport->vadjustment),
					 (gpointer) viewport);
	  gtk_object_unref (GTK_OBJECT (viewport->vadjustment));
	}

      viewport->vadjustment = adjustment;
      gtk_object_ref (GTK_OBJECT (viewport->vadjustment));
      gtk_object_sink (GTK_OBJECT (viewport->vadjustment));
      
      gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
			  gtk_viewport_adjustment_changed,
			  viewport);
      gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			  gtk_viewport_adjustment_value_changed,
			  viewport);

      gtk_viewport_adjustment_changed (adjustment, viewport);
    }
}

void
gtk_viewport_set_shadow_type (GtkViewport   *viewport,
			      GtkShadowType  type)
{
  g_return_if_fail (viewport != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));

  if ((GtkShadowType) viewport->shadow_type != type)
    {
      viewport->shadow_type = type;

      if (GTK_WIDGET_VISIBLE (viewport))
	{
	  gtk_widget_size_allocate (GTK_WIDGET (viewport), &(GTK_WIDGET (viewport)->allocation));
	  gtk_widget_queue_draw (GTK_WIDGET (viewport));
	}
    }
}


static void
gtk_viewport_map (GtkWidget *widget)
{
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  bin = GTK_BIN (widget);

  gdk_window_show (widget->window);

  if (bin->child &&
      GTK_WIDGET_VISIBLE (bin->child) &&
      !GTK_WIDGET_MAPPED (bin->child))
    gtk_widget_map (bin->child);
}

static void
gtk_viewport_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  
  gdk_window_hide (widget->window);
}

static void
gtk_viewport_realize (GtkWidget *widget)
{
  GtkBin *bin;
  GtkViewport *viewport;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint event_mask;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (widget));

  border_width = GTK_CONTAINER (widget)->border_width;

  bin = GTK_BIN (widget);
  viewport = GTK_VIEWPORT (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes.event_mask = event_mask;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, viewport);

  if (viewport->shadow_type != GTK_SHADOW_NONE)
    {
      attributes.x = widget->style->klass->xthickness;
      attributes.y = widget->style->klass->ythickness;
    }
  else
    {
      attributes.x = 0;
      attributes.y = 0;
    }

  attributes.width = MAX (1, widget->allocation.width - attributes.x * 2 - border_width * 2);
  attributes.height = MAX (1, widget->allocation.height - attributes.y * 2 - border_width * 2);
  attributes.event_mask = 0;

  viewport->view_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (viewport->view_window, viewport);

  attributes.x = 0;
  attributes.y = 0;

  if (bin->child)
    {
      attributes.width = viewport->hadjustment->upper;
      attributes.height = viewport->vadjustment->upper;
    }
  
  attributes.event_mask = event_mask;

  viewport->bin_window = gdk_window_new (viewport->view_window, &attributes, attributes_mask);
  gdk_window_set_user_data (viewport->bin_window, viewport);

  if (bin->child)
    gtk_widget_set_parent_window (bin->child, viewport->bin_window);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, viewport->bin_window, GTK_STATE_NORMAL);
  
  gdk_window_show (viewport->bin_window);
  gdk_window_show (viewport->view_window);
}

static void
gtk_viewport_unrealize (GtkWidget *widget)
{
  GtkViewport *viewport;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (widget));

  viewport = GTK_VIEWPORT (widget);

  gdk_window_set_user_data (viewport->view_window, NULL);
  gdk_window_destroy (viewport->view_window);
  viewport->view_window = NULL;

  gdk_window_set_user_data (viewport->bin_window, NULL);
  gdk_window_destroy (viewport->bin_window);
  viewport->bin_window = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_viewport_paint (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkViewport *viewport;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      viewport = GTK_VIEWPORT (widget);

      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, viewport->shadow_type,
		       0, 0, -1, -1);
    }
}

static void
gtk_viewport_draw (GtkWidget    *widget,
		   GdkRectangle *area)
{
  GtkViewport *viewport;
  GtkBin *bin;
  GdkRectangle tmp_area;
  GdkRectangle child_area;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      viewport = GTK_VIEWPORT (widget);
      bin = GTK_BIN (widget);

      border_width = GTK_CONTAINER (widget)->border_width;
      
      tmp_area = *area;
      tmp_area.x -= border_width;
      tmp_area.y -= border_width;
      
      gtk_viewport_paint (widget, &tmp_area);

      if (bin->child)
	{
	  tmp_area.x += viewport->hadjustment->value - widget->style->klass->xthickness;
	  tmp_area.y += viewport->vadjustment->value - widget->style->klass->ythickness;

	  if (gtk_widget_intersect (bin->child, &tmp_area, &child_area))
	    gtk_widget_draw (bin->child, &child_area);
	}
    }
}

static gint
gtk_viewport_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  GtkViewport *viewport;
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_VIEWPORT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      viewport = GTK_VIEWPORT (widget);
      bin = GTK_BIN (widget);

      if (event->window == widget->window)
	gtk_viewport_paint (widget, &event->area);

      child_event = *event;
      if ((event->window == viewport->bin_window) &&
	  (bin->child != NULL) &&
	  GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
    }

  return FALSE;
}

static void
gtk_viewport_add (GtkContainer *container,
		  GtkWidget    *child)
{
  GtkBin *bin;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (container));
  g_return_if_fail (child != NULL);

  bin = GTK_BIN (container);
  g_return_if_fail (bin->child == NULL);

  gtk_widget_set_parent_window (child, GTK_VIEWPORT (bin)->bin_window);

  GTK_CONTAINER_CLASS (parent_class)->add (container, child);
}

static void
gtk_viewport_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkViewport *viewport;
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (widget));
  g_return_if_fail (requisition != NULL);

  viewport = GTK_VIEWPORT (widget);
  bin = GTK_BIN (widget);

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			GTK_WIDGET (widget)->style->klass->xthickness) * 2 + 5;

  requisition->height = (GTK_CONTAINER (widget)->border_width * 2 +
			 GTK_WIDGET (widget)->style->klass->ythickness) * 2 + 5;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    gtk_widget_size_request (bin->child, &bin->child->requisition);
}

static void
gtk_viewport_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkViewport *viewport;
  GtkBin *bin;
  GtkAllocation child_allocation;
  gint hval, vval;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  viewport = GTK_VIEWPORT (widget);
  bin = GTK_BIN (widget);

  border_width = GTK_CONTAINER (widget)->border_width;

  child_allocation.x = 0;
  child_allocation.y = 0;

  if (viewport->shadow_type != GTK_SHADOW_NONE)
    {
      child_allocation.x = GTK_WIDGET (viewport)->style->klass->xthickness;
      child_allocation.y = GTK_WIDGET (viewport)->style->klass->ythickness;
    }

  child_allocation.width = MAX (1, allocation->width - child_allocation.x * 2 - border_width * 2);
  child_allocation.height = MAX (1, allocation->height - child_allocation.y * 2 - border_width * 2);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + border_width,
			      allocation->y + border_width,
			      allocation->width - border_width * 2,
			      allocation->height - border_width * 2);

      gdk_window_move_resize (viewport->view_window,
			      child_allocation.x,
			      child_allocation.y,
			      child_allocation.width,
			      child_allocation.height);
    }

  viewport->hadjustment->page_size = child_allocation.width;
  viewport->hadjustment->page_increment = viewport->hadjustment->page_size / 2;
  viewport->hadjustment->step_increment = 10;

  viewport->vadjustment->page_size = child_allocation.height;
  viewport->vadjustment->page_increment = viewport->vadjustment->page_size / 2;
  viewport->vadjustment->step_increment = 10;

  hval = viewport->hadjustment->value;
  vval = viewport->vadjustment->value;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      viewport->hadjustment->lower = 0;
      viewport->hadjustment->upper = MAX (bin->child->requisition.width,
					  child_allocation.width);

      hval = CLAMP (hval, 0,
		    viewport->hadjustment->upper -
		    viewport->hadjustment->page_size);

      viewport->vadjustment->lower = 0;
      viewport->vadjustment->upper = MAX (bin->child->requisition.height,
					  child_allocation.height);

      vval = CLAMP (vval, 0,
		    viewport->vadjustment->upper -
		    viewport->vadjustment->page_size);
    }

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      child_allocation.x = 0;
      child_allocation.y = 0;

      child_allocation.width = viewport->hadjustment->upper;
      child_allocation.height = viewport->vadjustment->upper;

      if (!GTK_WIDGET_REALIZED (widget))
        gtk_widget_realize (widget);

      gdk_window_resize (viewport->bin_window,
			 child_allocation.width,
			 child_allocation.height);

      child_allocation.x = 0;
      child_allocation.y = 0;
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }

  gtk_signal_emit_by_name (GTK_OBJECT (viewport->hadjustment), "changed");
  gtk_signal_emit_by_name (GTK_OBJECT (viewport->vadjustment), "changed");
  if (viewport->hadjustment->value != hval)
    {
      viewport->hadjustment->value = hval;
      gtk_signal_emit_by_name (GTK_OBJECT (viewport->hadjustment), "value_changed");
    }
  if (viewport->vadjustment->value != vval)
    {
      viewport->vadjustment->value = vval;
      gtk_signal_emit_by_name (GTK_OBJECT (viewport->vadjustment), "value_changed");
    }
}

static void
gtk_viewport_adjustment_changed (GtkAdjustment *adjustment,
				 gpointer       data)
{
  GtkViewport *viewport;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (data));

  viewport = GTK_VIEWPORT (data);
}

static void
gtk_viewport_adjustment_value_changed (GtkAdjustment *adjustment,
				       gpointer       data)
{
  GtkViewport *viewport;
  GtkBin *bin;
  GtkAllocation child_allocation;
  gint width, height;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (data));

  viewport = GTK_VIEWPORT (data);
  bin = GTK_BIN (data);

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gdk_window_get_size (viewport->view_window, &width, &height);

      child_allocation.x = 0;
      child_allocation.y = 0;

      if (viewport->hadjustment->lower != (viewport->hadjustment->upper -
					   viewport->hadjustment->page_size))
	child_allocation.x =  viewport->hadjustment->lower - viewport->hadjustment->value;

      if (viewport->vadjustment->lower != (viewport->vadjustment->upper -
					   viewport->vadjustment->page_size))
	child_allocation.y = viewport->vadjustment->lower - viewport->vadjustment->value;

      if (GTK_WIDGET_REALIZED (viewport))
	gdk_window_move (viewport->bin_window,
			 child_allocation.x,
			 child_allocation.y);
    }
}
