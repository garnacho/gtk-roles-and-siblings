/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAspectFrame: Ensure that the child window has a specified aspect ratio
 *    or, if obey_child, has the same aspect ratio as its requested size
 *
 *     Copyright Owen Taylor                          4/9/97
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
#include "gtkaspectframe.h"

static void gtk_aspect_frame_class_init    (GtkAspectFrameClass *klass);
static void gtk_aspect_frame_init          (GtkAspectFrame      *aspect_frame);
static void gtk_aspect_frame_draw          (GtkWidget      *widget,
					    GdkRectangle   *area);
static void gtk_aspect_frame_paint         (GtkWidget      *widget,
					    GdkRectangle   *area);
static gint gtk_aspect_frame_expose        (GtkWidget      *widget,
					    GdkEventExpose *event);
static void gtk_aspect_frame_size_allocate (GtkWidget         *widget,
					    GtkAllocation     *allocation);

#define MAX_RATIO 10000.0
#define MIN_RATIO 0.0001

guint
gtk_aspect_frame_get_type ()
{
  static guint aspect_frame_type = 0;

  if (!aspect_frame_type)
    {
      GtkTypeInfo aspect_frame_info =
      {
	"GtkAspectFrame",
	sizeof (GtkAspectFrame),
	sizeof (GtkAspectFrameClass),
	(GtkClassInitFunc) gtk_aspect_frame_class_init,
	(GtkObjectInitFunc) gtk_aspect_frame_init,
	(GtkArgFunc) NULL,
      };

      aspect_frame_type = gtk_type_unique (gtk_frame_get_type (), &aspect_frame_info);
    }

  return aspect_frame_type;
}

static void
gtk_aspect_frame_class_init (GtkAspectFrameClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->draw = gtk_aspect_frame_draw;
  widget_class->expose_event = gtk_aspect_frame_expose;
  widget_class->size_allocate = gtk_aspect_frame_size_allocate;
}

static void
gtk_aspect_frame_init (GtkAspectFrame *aspect_frame)
{
  aspect_frame->xalign = 0.5;
  aspect_frame->yalign = 0.5;
  aspect_frame->ratio = 1.0;
  aspect_frame->obey_child = 1;
  aspect_frame->center_allocation.x = -1;
  aspect_frame->center_allocation.y = -1;
  aspect_frame->center_allocation.width = 1;
  aspect_frame->center_allocation.height = 1;
}

GtkWidget*
gtk_aspect_frame_new (const gchar *label,
		      gfloat xalign,
		      gfloat yalign,
		      gfloat ratio,
		      gint   obey_child)
{
  GtkAspectFrame *aspect_frame;

  aspect_frame = gtk_type_new (gtk_aspect_frame_get_type ());

  aspect_frame->xalign = CLAMP (xalign, 0.0, 1.0);
  aspect_frame->yalign = CLAMP (yalign, 0.0, 1.0);
  aspect_frame->ratio = CLAMP (ratio, MIN_RATIO, MAX_RATIO);
  aspect_frame->obey_child = obey_child;

  gtk_frame_set_label (GTK_FRAME(aspect_frame), label);

  return GTK_WIDGET (aspect_frame);
}

void
gtk_aspect_frame_set (GtkAspectFrame *aspect_frame,
		      gfloat        xalign,
		      gfloat        yalign,
		      gfloat        ratio,
		      gint          obey_child)
{
  g_return_if_fail (aspect_frame != NULL);
  g_return_if_fail (GTK_IS_ASPECT_FRAME (aspect_frame));

  xalign = CLAMP (xalign, 0.0, 1.0);
  yalign = CLAMP (yalign, 0.0, 1.0);
  ratio = CLAMP (ratio, MIN_RATIO, MAX_RATIO);

  if ((aspect_frame->xalign != xalign) ||
      (aspect_frame->yalign != yalign) ||
      (aspect_frame->ratio != ratio) ||
      (aspect_frame->obey_child != obey_child))
    {
      aspect_frame->xalign = xalign;
      aspect_frame->yalign = yalign;
      aspect_frame->ratio = ratio;
      aspect_frame->obey_child = obey_child;

      gtk_widget_size_allocate (GTK_WIDGET (aspect_frame), &(GTK_WIDGET (aspect_frame)->allocation));
      gtk_widget_queue_draw (GTK_WIDGET (aspect_frame));
    }
}

static void
gtk_aspect_frame_paint (GtkWidget    *widget,
			GdkRectangle *area)
{
  GtkFrame *frame;
  GtkStateType state;
  gint height_extra;
  gint label_area_width;
  gint x, y;
  GtkAllocation *allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ASPECT_FRAME (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      frame = GTK_FRAME (widget);
      allocation = &GTK_ASPECT_FRAME(widget)->center_allocation;

      state = widget->state;
      if (!GTK_WIDGET_IS_SENSITIVE (widget))
        state = GTK_STATE_INSENSITIVE;

      height_extra = frame->label_height - widget->style->klass->xthickness;
      height_extra = MAX (height_extra, 0);

      x = GTK_CONTAINER (frame)->border_width;
      y = GTK_CONTAINER (frame)->border_width;

      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, frame->shadow_type,
		       allocation->x + x,
		       allocation->y + y + height_extra / 2,
		       allocation->width - x * 2,
		       allocation->height - y * 2 - height_extra / 2);

      if (frame->label)
	{
	  label_area_width = (allocation->width +
			      GTK_CONTAINER (frame)->border_width * 2 -
			      widget->style->klass->xthickness * 2);

	  x = ((label_area_width - frame->label_width) * frame->label_xalign +
	       GTK_CONTAINER (frame)->border_width + widget->style->klass->xthickness);
	  y = (GTK_CONTAINER (frame)->border_width + widget->style->font->ascent);

	  gdk_window_clear_area (widget->window,
				 allocation->x + x + 2,
				 allocation->y + GTK_CONTAINER (frame)->border_width,
				 frame->label_width - 4, frame->label_height);
	  gtk_draw_string (widget->style, widget->window, state,
			   allocation->x + x + 3,
			   allocation->y + y,
			   frame->label);
	}
    }
}

/* the only modification to the next two routines is to call
   gtk_aspect_frame_paint instead of gtk_frame_paint */

static void
gtk_aspect_frame_draw (GtkWidget    *widget,
		       GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ASPECT_FRAME (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      gtk_aspect_frame_paint (widget, area);

      if (bin->child && gtk_widget_intersect (bin->child, area, &child_area))
	gtk_widget_draw (bin->child, &child_area);
    }
}

static gint
gtk_aspect_frame_expose (GtkWidget      *widget,
			 GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      gtk_aspect_frame_paint (widget, &event->area);

      child_event = *event;
      if (bin->child &&
	  GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
    }

  return FALSE;
}

static void
gtk_aspect_frame_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkFrame *frame;
  GtkAspectFrame *aspect_frame;
  GtkBin *bin;

  GtkAllocation child_allocation;
  gint x,y;
  gint width,height;
  gdouble ratio;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ASPECT_FRAME (widget));
  g_return_if_fail (allocation != NULL);

  aspect_frame = GTK_ASPECT_FRAME (widget);
  frame = GTK_FRAME (widget);
  bin = GTK_BIN (widget);

  if (GTK_WIDGET_MAPPED (widget) &&
      ((widget->allocation.x != allocation->x) ||
       (widget->allocation.y != allocation->y) ||
       (widget->allocation.width != allocation->width) ||
       (widget->allocation.height != allocation->height)) &&
      (widget->allocation.width != 0) &&
      (widget->allocation.height != 0))
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);

  widget->allocation = *allocation;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      if (aspect_frame->obey_child)
	{
	  if (bin->child->requisition.height != 0)
	    {
	      ratio = (gdouble)bin->child->requisition.width /
		bin->child->requisition.height;
	      if (ratio < MIN_RATIO) ratio = MIN_RATIO;
	    }
	  else
	    if (bin->child->requisition.height != 0)
	      ratio = MAX_RATIO;
	  else
	    ratio = 1.0;
	}
      else
	ratio = aspect_frame->ratio;

      x = (GTK_CONTAINER (frame)->border_width +
	   GTK_WIDGET (frame)->style->klass->xthickness);
      width = allocation->width - x * 2;

      y = (GTK_CONTAINER (frame)->border_width +
			    MAX (frame->label_height, GTK_WIDGET (frame)->style->klass->ythickness));
      height = (allocation->height - y -
				 GTK_CONTAINER (frame)->border_width -
				 GTK_WIDGET (frame)->style->klass->ythickness);

      if (ratio * height > width)
	{
	  child_allocation.width = width;
	  child_allocation.height = width/ratio;
	}
      else
	{
	  child_allocation.width = ratio*height;
	  child_allocation.height = height;
	}

      child_allocation.x = aspect_frame->xalign * (width - child_allocation.width) + allocation->x + x;
      child_allocation.y = aspect_frame->yalign * (height - child_allocation.height) + allocation->y + y;

      aspect_frame->center_allocation.width = child_allocation.width + 2*x;
      aspect_frame->center_allocation.x = child_allocation.x - x;
      aspect_frame->center_allocation.height = child_allocation.height + y +
				 GTK_CONTAINER (frame)->border_width +
				 GTK_WIDGET (frame)->style->klass->ythickness;
      aspect_frame->center_allocation.y = child_allocation.y - y;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}
