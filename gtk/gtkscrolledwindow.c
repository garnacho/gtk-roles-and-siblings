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
#include "gtkscrolledwindow.h"
#include "gtksignal.h"


#define SCROLLBAR_SPACING(w) (GTK_SCROLLED_WINDOW_CLASS (GTK_OBJECT (w)->klass)->scrollbar_spacing)

enum {
  ARG_0,
  ARG_VIEWPORT,
  ARG_HSCROLLBAR_POLICY,
  ARG_VSCROLLBAR_POLICY
};


static void gtk_scrolled_window_class_init         (GtkScrolledWindowClass *klass);
static void gtk_scrolled_window_init               (GtkScrolledWindow      *scrolled_window);
static void gtk_scrolled_window_set_arg		   (GtkObject              *object,
						    GtkArg                 *arg,
						    guint                   arg_id);
static void gtk_scrolled_window_get_arg		   (GtkObject              *object,
						    GtkArg                 *arg,
						    guint                   arg_id);
static void gtk_scrolled_window_destroy            (GtkObject              *object);
static void gtk_scrolled_window_finalize           (GtkObject              *object);
static void gtk_scrolled_window_map                (GtkWidget              *widget);
static void gtk_scrolled_window_unmap              (GtkWidget              *widget);
static void gtk_scrolled_window_draw               (GtkWidget              *widget,
						    GdkRectangle           *area);
static void gtk_scrolled_window_size_request       (GtkWidget              *widget,
						    GtkRequisition         *requisition);
static void gtk_scrolled_window_size_allocate      (GtkWidget              *widget,
						    GtkAllocation          *allocation);
static void gtk_scrolled_window_add                (GtkContainer           *container,
						    GtkWidget              *widget);
static void gtk_scrolled_window_remove             (GtkContainer           *container,
						    GtkWidget              *widget);
static void gtk_scrolled_window_forall             (GtkContainer           *container,
						    gboolean		    include_internals,
						    GtkCallback             callback,
						    gpointer                callback_data);
static void gtk_scrolled_window_viewport_allocate  (GtkWidget              *widget,
						    GtkAllocation          *allocation);
static void gtk_scrolled_window_adjustment_changed (GtkAdjustment          *adjustment,
						    gpointer                data);


static GtkContainerClass *parent_class = NULL;


GtkType
gtk_scrolled_window_get_type (void)
{
  static GtkType scrolled_window_type = 0;

  if (!scrolled_window_type)
    {
      GtkTypeInfo scrolled_window_info =
      {
	"GtkScrolledWindow",
	sizeof (GtkScrolledWindow),
	sizeof (GtkScrolledWindowClass),
	(GtkClassInitFunc) gtk_scrolled_window_class_init,
	(GtkObjectInitFunc) gtk_scrolled_window_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      scrolled_window_type = gtk_type_unique (GTK_TYPE_CONTAINER, &scrolled_window_info);
    }

  return scrolled_window_type;
}

static void
gtk_scrolled_window_class_init (GtkScrolledWindowClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;
  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  gtk_object_add_arg_type ("GtkScrolledWindow::viewport",
			   GTK_TYPE_VIEWPORT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
			   ARG_VIEWPORT);
  gtk_object_add_arg_type ("GtkScrolledWindow::hscrollbar_policy",
			   GTK_TYPE_POLICY_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_HSCROLLBAR_POLICY);
  gtk_object_add_arg_type ("GtkScrolledWindow::vscrollbar_policy",
			   GTK_TYPE_POLICY_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_VSCROLLBAR_POLICY);

  object_class->set_arg = gtk_scrolled_window_set_arg;
  object_class->get_arg = gtk_scrolled_window_get_arg;
  object_class->destroy = gtk_scrolled_window_destroy;
  object_class->finalize = gtk_scrolled_window_finalize;

  widget_class->map = gtk_scrolled_window_map;
  widget_class->unmap = gtk_scrolled_window_unmap;
  widget_class->draw = gtk_scrolled_window_draw;
  widget_class->size_request = gtk_scrolled_window_size_request;
  widget_class->size_allocate = gtk_scrolled_window_size_allocate;

  container_class->add = gtk_scrolled_window_add;
  container_class->remove = gtk_scrolled_window_remove;
  container_class->forall = gtk_scrolled_window_forall;

  class->scrollbar_spacing = 5;
}

static void
gtk_scrolled_window_set_arg (GtkObject        *object,
			     GtkArg           *arg,
			     guint             arg_id)
{
  GtkScrolledWindow *scrolled_window;

  scrolled_window = GTK_SCROLLED_WINDOW (object);

  switch (arg_id)
    {
      GtkWidget *viewport;

    case ARG_VIEWPORT:
      viewport = GTK_VALUE_POINTER (*arg);
      gtk_container_add (GTK_CONTAINER (scrolled_window), viewport);
    case ARG_HSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
				      GTK_VALUE_ENUM (*arg),
				      scrolled_window->vscrollbar_policy);
      break;
    case ARG_VSCROLLBAR_POLICY:
      gtk_scrolled_window_set_policy (scrolled_window,
				      scrolled_window->hscrollbar_policy,
				      GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_scrolled_window_get_arg (GtkObject        *object,
			     GtkArg           *arg,
			     guint             arg_id)
{
  GtkScrolledWindow *scrolled_window;

  scrolled_window = GTK_SCROLLED_WINDOW (object);

  switch (arg_id)
    {
    case ARG_VIEWPORT:
      GTK_VALUE_POINTER (*arg) = scrolled_window->viewport;
      break;
    case ARG_HSCROLLBAR_POLICY:
      GTK_VALUE_ENUM (*arg) = scrolled_window->hscrollbar_policy;
      break;
    case ARG_VSCROLLBAR_POLICY:
      GTK_VALUE_ENUM (*arg) = scrolled_window->vscrollbar_policy;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_scrolled_window_init (GtkScrolledWindow *scrolled_window)
{
  GTK_WIDGET_SET_FLAGS (scrolled_window, GTK_NO_WINDOW);

  gtk_container_set_resize_mode (GTK_CONTAINER (scrolled_window), GTK_RESIZE_QUEUE);

  scrolled_window->hscrollbar = NULL;
  scrolled_window->vscrollbar = NULL;
  scrolled_window->hscrollbar_policy = GTK_POLICY_ALWAYS;
  scrolled_window->vscrollbar_policy = GTK_POLICY_ALWAYS;
  scrolled_window->window_placement = GTK_CORNER_TOP_LEFT;
  scrolled_window->autogenerated_viewport = FALSE;
}

GtkWidget*
gtk_scrolled_window_new (GtkAdjustment *hadjustment,
			 GtkAdjustment *vadjustment)
{
  GtkWidget *scrolled_window;

  scrolled_window = gtk_type_new (GTK_TYPE_SCROLLED_WINDOW);

  gtk_scrolled_window_construct (GTK_SCROLLED_WINDOW (scrolled_window), hadjustment, vadjustment);

  return scrolled_window;
}

void
gtk_scrolled_window_construct (GtkScrolledWindow *scrolled_window,
			       GtkAdjustment     *hadjustment,
			       GtkAdjustment     *vadjustment)
{
  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));
  g_return_if_fail (scrolled_window->hscrollbar == NULL);
  g_return_if_fail (scrolled_window->vscrollbar == NULL);

  scrolled_window->hscrollbar = gtk_hscrollbar_new (hadjustment);
  scrolled_window->vscrollbar = gtk_vscrollbar_new (vadjustment);

  hadjustment =
    gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
  vadjustment =
    gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));

  gtk_signal_connect (GTK_OBJECT (hadjustment), "changed",
		      (GtkSignalFunc) gtk_scrolled_window_adjustment_changed,
		      (gpointer) scrolled_window);
  gtk_signal_connect (GTK_OBJECT (vadjustment), "changed",
		      (GtkSignalFunc) gtk_scrolled_window_adjustment_changed,
		      (gpointer) scrolled_window);

  gtk_widget_set_parent (scrolled_window->hscrollbar, GTK_WIDGET (scrolled_window));
  gtk_widget_set_parent (scrolled_window->vscrollbar, GTK_WIDGET (scrolled_window));

  gtk_widget_show (scrolled_window->hscrollbar);
  gtk_widget_show (scrolled_window->vscrollbar);
  
  gtk_widget_ref (scrolled_window->hscrollbar);
  gtk_widget_ref (scrolled_window->vscrollbar);
}

GtkAdjustment*
gtk_scrolled_window_get_hadjustment (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (scrolled_window != NULL, NULL);
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return gtk_range_get_adjustment (GTK_RANGE (scrolled_window->hscrollbar));
}

GtkAdjustment*
gtk_scrolled_window_get_vadjustment (GtkScrolledWindow *scrolled_window)
{
  g_return_val_if_fail (scrolled_window != NULL, NULL);
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window), NULL);

  return gtk_range_get_adjustment (GTK_RANGE (scrolled_window->vscrollbar));
}

void
gtk_scrolled_window_set_policy (GtkScrolledWindow *scrolled_window,
				GtkPolicyType      hscrollbar_policy,
				GtkPolicyType      vscrollbar_policy)
{
  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if ((scrolled_window->hscrollbar_policy != hscrollbar_policy) ||
      (scrolled_window->vscrollbar_policy != vscrollbar_policy))
    {
      scrolled_window->hscrollbar_policy = hscrollbar_policy;
      scrolled_window->vscrollbar_policy = vscrollbar_policy;

      if (GTK_WIDGET (scrolled_window)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

void
gtk_scrolled_window_set_placement (GtkScrolledWindow *scrolled_window,
				   GtkCornerType      window_placement)
{
  g_return_if_fail (scrolled_window != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

  if (scrolled_window->window_placement != window_placement)
    {
      scrolled_window->window_placement = window_placement;

      if (GTK_WIDGET (scrolled_window)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (scrolled_window));
    }
}

static void
gtk_scrolled_window_destroy (GtkObject *object)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (object));

  scrolled_window = GTK_SCROLLED_WINDOW (object);

  gtk_widget_destroy (scrolled_window->viewport);
  gtk_widget_destroy (scrolled_window->hscrollbar);
  gtk_widget_destroy (scrolled_window->vscrollbar);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_scrolled_window_finalize (GtkObject *object)
{
  GtkScrolledWindow *scrolled_window;

  scrolled_window = GTK_SCROLLED_WINDOW (object);
  gtk_widget_unref (scrolled_window->viewport);
  gtk_widget_unref (scrolled_window->hscrollbar);
  gtk_widget_unref (scrolled_window->vscrollbar);

  GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_scrolled_window_map (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));

  if (!GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
      scrolled_window = GTK_SCROLLED_WINDOW (widget);

      if (scrolled_window->viewport &&
	  GTK_WIDGET_VISIBLE (scrolled_window->viewport) &&
	  !GTK_WIDGET_MAPPED (scrolled_window->viewport))
	gtk_widget_map (scrolled_window->viewport);

      if (GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar) &&
	  !GTK_WIDGET_MAPPED (scrolled_window->hscrollbar))
	gtk_widget_map (scrolled_window->hscrollbar);

      if (GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar) &&
	  !GTK_WIDGET_MAPPED (scrolled_window->vscrollbar))
	gtk_widget_map (scrolled_window->vscrollbar);
    }
}

static void
gtk_scrolled_window_unmap (GtkWidget *widget)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));

  if (GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
      scrolled_window = GTK_SCROLLED_WINDOW (widget);

      if (scrolled_window->viewport &&
	  GTK_WIDGET_MAPPED (scrolled_window->viewport))
	gtk_widget_unmap (scrolled_window->viewport);

      if (GTK_WIDGET_MAPPED (scrolled_window->hscrollbar))
	gtk_widget_unmap (scrolled_window->hscrollbar);

      if (GTK_WIDGET_MAPPED (scrolled_window->vscrollbar))
	gtk_widget_unmap (scrolled_window->vscrollbar);
    }
}

static void
gtk_scrolled_window_draw (GtkWidget    *widget,
			  GdkRectangle *area)
{
  GtkScrolledWindow *scrolled_window;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      scrolled_window = GTK_SCROLLED_WINDOW (widget);

      if (scrolled_window->viewport &&
	  gtk_widget_intersect (scrolled_window->viewport, area, &child_area))
	gtk_widget_draw (scrolled_window->viewport, &child_area);

      if (gtk_widget_intersect (scrolled_window->hscrollbar, area, &child_area))
	gtk_widget_draw (scrolled_window->hscrollbar, &child_area);

      if (gtk_widget_intersect (scrolled_window->vscrollbar, area, &child_area))
	gtk_widget_draw (scrolled_window->vscrollbar, &child_area);
    }
}

static void
gtk_scrolled_window_size_request (GtkWidget      *widget,
				  GtkRequisition *requisition)
{
  GtkScrolledWindow *scrolled_window;
  gint extra_height;
  gint extra_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (requisition != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);

  requisition->width = 0;
  requisition->height = 0;

  if (scrolled_window->viewport &&
      GTK_WIDGET_VISIBLE (scrolled_window->viewport))
    {
      gtk_widget_size_request (scrolled_window->viewport, &scrolled_window->viewport->requisition);

      requisition->width += scrolled_window->viewport->requisition.width;
      requisition->height += scrolled_window->viewport->requisition.height;
    }

  extra_width = 0;
  extra_height = 0;

  if ((scrolled_window->hscrollbar_policy == GTK_POLICY_AUTOMATIC) ||
      GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
    {
      gtk_widget_size_request (scrolled_window->hscrollbar,
			       &scrolled_window->hscrollbar->requisition);

      requisition->width = MAX (requisition->width, scrolled_window->hscrollbar->requisition.width);
      extra_height = SCROLLBAR_SPACING (scrolled_window) + scrolled_window->hscrollbar->requisition.height;
    }

  if ((scrolled_window->vscrollbar_policy == GTK_POLICY_AUTOMATIC) ||
      GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
    {
      gtk_widget_size_request (scrolled_window->vscrollbar,
			       &scrolled_window->vscrollbar->requisition);

      requisition->height = MAX (requisition->height, scrolled_window->vscrollbar->requisition.height);
      extra_width = SCROLLBAR_SPACING (scrolled_window) + scrolled_window->vscrollbar->requisition.width;
    }

  requisition->width += GTK_CONTAINER (widget)->border_width * 2 + extra_width;
  requisition->height += GTK_CONTAINER (widget)->border_width * 2 + extra_height;
}

static void
gtk_scrolled_window_size_allocate (GtkWidget     *widget,
				   GtkAllocation *allocation)
{
  GtkScrolledWindow *scrolled_window;
  GtkAllocation viewport_allocation;
  GtkAllocation child_allocation;
  guint previous_hvis;
  guint previous_vvis;
  gint count;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (widget));
  g_return_if_fail (allocation != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);
  widget->allocation = *allocation;

  if (scrolled_window->hscrollbar_policy == GTK_POLICY_ALWAYS)
    scrolled_window->hscrollbar_visible = TRUE;
  if (scrolled_window->vscrollbar_policy == GTK_POLICY_ALWAYS)
    scrolled_window->vscrollbar_visible = TRUE;

  if (scrolled_window->viewport &&
      GTK_WIDGET_VISIBLE (scrolled_window->viewport))
    {
      count = 0;

      do {
	gtk_scrolled_window_viewport_allocate (widget, &viewport_allocation);

	child_allocation.x = viewport_allocation.x + allocation->x;
	child_allocation.y = viewport_allocation.y + allocation->y;
	child_allocation.width = viewport_allocation.width;
	child_allocation.height = viewport_allocation.height;

	previous_hvis = scrolled_window->hscrollbar_visible;
	previous_vvis = scrolled_window->vscrollbar_visible;

	gtk_widget_size_allocate (scrolled_window->viewport, &child_allocation);
	/* If, after the first iteration, the hscrollbar and the
	 * vscrollbar flip visiblity, then we need both.
	 */
	if ((count++) && 
	    (previous_hvis != scrolled_window->hscrollbar_visible) &&
	    (previous_vvis != scrolled_window->vscrollbar_visible))
	  {
	    scrolled_window->hscrollbar_visible = TRUE;
	    scrolled_window->vscrollbar_visible = TRUE;
	    break;
	  }

	count++;
      } while ((previous_hvis != scrolled_window->hscrollbar_visible) ||
	       (previous_vvis != scrolled_window->vscrollbar_visible));
    }

  if (scrolled_window->hscrollbar_visible)
    {
      if (!GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
	gtk_widget_show (scrolled_window->hscrollbar);

      child_allocation.x = viewport_allocation.x;
      if (scrolled_window->window_placement == GTK_CORNER_TOP_LEFT ||
	  scrolled_window->window_placement == GTK_CORNER_TOP_RIGHT)
	child_allocation.y = (viewport_allocation.y +
			      viewport_allocation.height +
			      SCROLLBAR_SPACING (scrolled_window));
      else
	child_allocation.y = GTK_CONTAINER (scrolled_window)->border_width;

      child_allocation.width = viewport_allocation.width;
      child_allocation.height = scrolled_window->hscrollbar->requisition.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (scrolled_window->hscrollbar, &child_allocation);
    }
  else if (GTK_WIDGET_VISIBLE (scrolled_window->hscrollbar))
    gtk_widget_hide (scrolled_window->hscrollbar);

  if (scrolled_window->vscrollbar_visible)
    {
      if (!GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
	gtk_widget_show (scrolled_window->vscrollbar);

      if (scrolled_window->window_placement == GTK_CORNER_TOP_LEFT ||
	  scrolled_window->window_placement == GTK_CORNER_BOTTOM_LEFT)
	child_allocation.x = (viewport_allocation.x +
			      viewport_allocation.width +
			      SCROLLBAR_SPACING (scrolled_window));
      else
	child_allocation.x = GTK_CONTAINER (scrolled_window)->border_width;

      child_allocation.y = viewport_allocation.y;
      child_allocation.width = scrolled_window->vscrollbar->requisition.width;
      child_allocation.height = viewport_allocation.height;
      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (scrolled_window->vscrollbar, &child_allocation);
    }
  else if (GTK_WIDGET_VISIBLE (scrolled_window->vscrollbar))
    gtk_widget_hide (scrolled_window->vscrollbar);
}

static void
gtk_scrolled_window_add (GtkContainer *container,
			 GtkWidget    *widget)
{
  GtkScrolledWindow *scrolled_window;
  GtkArgInfo *info_hadj;
  GtkArgInfo *info_vadj;
  gchar *error;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (widget != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (container);

  if (scrolled_window->viewport)
    gtk_container_remove (container, scrolled_window->viewport);

  error = gtk_object_arg_get_info (GTK_OBJECT_TYPE (widget),
				   "hadjustment", &info_hadj);
  if (!error)
    {
      error = gtk_object_arg_get_info (GTK_OBJECT_TYPE (widget),
				       "vadjustment", &info_vadj);
  
      if (!error)
	{
	  gtk_object_set (GTK_OBJECT (widget),
			  "hadjustment",
			  gtk_scrolled_window_get_hadjustment
			  (scrolled_window),
			  "vadjustment",
			  gtk_scrolled_window_get_vadjustment
			  (scrolled_window),
			  NULL);
	  scrolled_window->viewport = widget;
	  gtk_widget_set_parent (widget, GTK_WIDGET (scrolled_window));
	  gtk_widget_ref (widget);
	  scrolled_window->autogenerated_viewport = FALSE;
	}
    }

  if (error)
    {
      g_free (error);

      scrolled_window->viewport = gtk_viewport_new 
	(gtk_scrolled_window_get_hadjustment (scrolled_window),
	 gtk_scrolled_window_get_vadjustment (scrolled_window));
      gtk_widget_set_parent (scrolled_window->viewport,
			     GTK_WIDGET (scrolled_window));
      gtk_widget_ref (scrolled_window->viewport);
      gtk_widget_show (scrolled_window->viewport);
      scrolled_window->autogenerated_viewport = FALSE;

      gtk_container_add (GTK_CONTAINER (scrolled_window->viewport), widget);

      widget = scrolled_window->viewport;
    }

  if (GTK_WIDGET_VISIBLE (scrolled_window))
    {
      if (GTK_WIDGET_REALIZED (scrolled_window) &&
	  !GTK_WIDGET_REALIZED (widget))
	gtk_widget_realize (widget);

      if (GTK_WIDGET_MAPPED (scrolled_window) &&
	  !GTK_WIDGET_MAPPED (widget))
	gtk_widget_map (widget);
    }
  
  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (scrolled_window))
    gtk_widget_queue_resize (widget);
}

static void
gtk_scrolled_window_remove (GtkContainer *container,
			    GtkWidget    *widget)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (widget != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (container);
  if (scrolled_window->viewport == widget ||
      scrolled_window->hscrollbar == widget ||
      scrolled_window->vscrollbar == widget)
    {
      /* this happens during destroy */

      if (scrolled_window->viewport == widget)
	scrolled_window->autogenerated_viewport = FALSE;

      gtk_widget_unparent (widget);
    }
  else if (scrolled_window->autogenerated_viewport)
    gtk_container_remove (GTK_CONTAINER (scrolled_window->viewport), widget);
}

static void
gtk_scrolled_window_forall (GtkContainer *container,
			    gboolean	  include_internals,
			    GtkCallback   callback,
			    gpointer      callback_data)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));
  g_return_if_fail (callback != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (container);

  if (scrolled_window->viewport)
    (* callback) (scrolled_window->viewport, callback_data);
  if (include_internals)
    {
      (* callback) (scrolled_window->vscrollbar, callback_data);
      (* callback) (scrolled_window->hscrollbar, callback_data);
    }
}

static void
gtk_scrolled_window_viewport_allocate (GtkWidget     *widget,
				       GtkAllocation *allocation)
{
  GtkScrolledWindow *scrolled_window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (allocation != NULL);

  scrolled_window = GTK_SCROLLED_WINDOW (widget);

  allocation->x = GTK_CONTAINER (widget)->border_width;
  allocation->y = GTK_CONTAINER (widget)->border_width;
  allocation->width = MAX (1, widget->allocation.width - allocation->x * 2);
  allocation->height = MAX (1, widget->allocation.height - allocation->y * 2);

  if (scrolled_window->vscrollbar_visible)
    {
      if (scrolled_window->window_placement == GTK_CORNER_TOP_RIGHT ||
	  scrolled_window->window_placement == GTK_CORNER_BOTTOM_RIGHT)
	allocation->x += (scrolled_window->vscrollbar->requisition.width +
			  SCROLLBAR_SPACING (scrolled_window));

      allocation->width =
	MAX (1, allocation->width -
	     (scrolled_window->vscrollbar->requisition.width +
	      SCROLLBAR_SPACING (scrolled_window)));
    }
  if (scrolled_window->hscrollbar_visible)
    {
      if (scrolled_window->window_placement == GTK_CORNER_BOTTOM_LEFT ||
	  scrolled_window->window_placement == GTK_CORNER_BOTTOM_RIGHT)
	allocation->y += (scrolled_window->hscrollbar->requisition.height +
			  SCROLLBAR_SPACING (scrolled_window));

      allocation->height =
	MAX (1, allocation->height -
	     (scrolled_window->hscrollbar->requisition.height +
	      SCROLLBAR_SPACING (scrolled_window)));
    }
}

static void
gtk_scrolled_window_adjustment_changed (GtkAdjustment *adjustment,
					gpointer       data)
{
  GtkScrolledWindow *scrolled_win;
  gboolean visible;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  scrolled_win = GTK_SCROLLED_WINDOW (data);

  if (adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->hscrollbar)))
    {
      if (scrolled_win->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  visible = scrolled_win->hscrollbar_visible;
	  scrolled_win->hscrollbar_visible =
	    ((adjustment->upper - adjustment->lower) > adjustment->page_size);
	  if (scrolled_win->hscrollbar_visible != visible)
	    gtk_widget_queue_resize (GTK_WIDGET (scrolled_win));
	}
    }
  else if (adjustment == gtk_range_get_adjustment (GTK_RANGE (scrolled_win->vscrollbar)))
    {
      if (scrolled_win->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  visible = scrolled_win->vscrollbar_visible;
	  scrolled_win->vscrollbar_visible =
	    ((adjustment->upper - adjustment->lower) > adjustment->page_size);
	  if (scrolled_win->vscrollbar_visible != visible)
	    gtk_widget_queue_resize (GTK_WIDGET (scrolled_win));
	}
    }
  else
    {
      g_warning ("could not determine which adjustment scrollbar received change signal for");
      return;
    }
}
