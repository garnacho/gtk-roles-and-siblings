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
#include <stdio.h>
#include "gtkhscale.h"
#include "gtksignal.h"
#include "gdk/gdkkeysyms.h"


#define SCALE_CLASS(w)  GTK_SCALE_CLASS (GTK_OBJECT (w)->klass)
#define RANGE_CLASS(w)  GTK_RANGE_CLASS (GTK_OBJECT (w)->klass)


static void gtk_hscale_class_init    (GtkHScaleClass *klass);
static void gtk_hscale_init          (GtkHScale      *hscale);
static void gtk_hscale_realize       (GtkWidget      *widget);
static void gtk_hscale_size_request  (GtkWidget      *widget,
				      GtkRequisition *requisition);
static void gtk_hscale_size_allocate (GtkWidget      *widget,
				      GtkAllocation  *allocation);
static void gtk_hscale_pos_trough    (GtkHScale      *hscale,
				      gint           *x,
				      gint           *y,
				      gint           *w,
				      gint           *h);
static void gtk_hscale_draw_slider   (GtkRange       *range);
static void gtk_hscale_draw_value    (GtkScale       *scale);
static gint gtk_hscale_trough_keys   (GtkRange *range,
				      GdkEventKey *key,
				      GtkScrollType *scroll,
				      GtkTroughType *pos);

guint
gtk_hscale_get_type (void)
{
  static guint hscale_type = 0;

  if (!hscale_type)
    {
      GtkTypeInfo hscale_info =
      {
	"GtkHScale",
	sizeof (GtkHScale),
	sizeof (GtkHScaleClass),
	(GtkClassInitFunc) gtk_hscale_class_init,
	(GtkObjectInitFunc) gtk_hscale_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      hscale_type = gtk_type_unique (gtk_scale_get_type (), &hscale_info);
    }

  return hscale_type;
}

static void
gtk_hscale_class_init (GtkHScaleClass *class)
{
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;
  GtkScaleClass *scale_class;

  widget_class = (GtkWidgetClass*) class;
  range_class = (GtkRangeClass*) class;
  scale_class = (GtkScaleClass*) class;

  widget_class->realize = gtk_hscale_realize;
  widget_class->size_request = gtk_hscale_size_request;
  widget_class->size_allocate = gtk_hscale_size_allocate;

  range_class->slider_update = gtk_range_default_hslider_update;
  range_class->trough_click = gtk_range_default_htrough_click;
  range_class->motion = gtk_range_default_hmotion;
  range_class->draw_slider = gtk_hscale_draw_slider;
  range_class->trough_keys = gtk_hscale_trough_keys;

  scale_class->draw_value = gtk_hscale_draw_value;
}

static void
gtk_hscale_init (GtkHScale *hscale)
{
}

GtkWidget*
gtk_hscale_new (GtkAdjustment *adjustment)
{
  GtkHScale *hscale;

  hscale = gtk_type_new (gtk_hscale_get_type ());

  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  gtk_range_set_adjustment (GTK_RANGE (hscale), adjustment);

  return GTK_WIDGET (hscale);
}


static void
gtk_hscale_realize (GtkWidget *widget)
{
  GtkRange *range;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint x, y, w, h;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HSCALE (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  range = GTK_RANGE (widget);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);

  gtk_hscale_pos_trough (GTK_HSCALE (widget), &x, &y, &w, &h);
  attributes.x = x;
  attributes.y = y;
  attributes.width = w;
  attributes.height = h;
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  range->trough = gdk_window_new (widget->window, &attributes, attributes_mask);

  attributes.width = SCALE_CLASS (range)->slider_length;
  attributes.height = RANGE_CLASS (range)->slider_width;
  attributes.event_mask |= (GDK_BUTTON_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK);

  range->slider = gdk_window_new (range->trough, &attributes, attributes_mask);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_user_data (widget->window, widget);
  gdk_window_set_user_data (range->trough, widget);
  gdk_window_set_user_data (range->slider, widget);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, range->trough, GTK_STATE_ACTIVE);
  gtk_style_set_background (widget->style, range->slider, GTK_STATE_NORMAL);

  gtk_range_slider_update (GTK_RANGE (widget));

  gdk_window_show (range->slider);
  gdk_window_show (range->trough);
}

static void
gtk_hscale_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkScale *scale;
  gint value_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HSCALE (widget));
  g_return_if_fail (requisition != NULL);

  scale = GTK_SCALE (widget);

  requisition->width = (SCALE_CLASS (scale)->slider_length +
			widget->style->klass->xthickness) * 2;
  requisition->height = (RANGE_CLASS (scale)->slider_width +
			 widget->style->klass->ythickness * 2);

  if (scale->draw_value)
    {
      value_width = gtk_scale_value_width (scale);

      if ((scale->value_pos == GTK_POS_LEFT) ||
	  (scale->value_pos == GTK_POS_RIGHT))
	{
	  requisition->width += value_width + SCALE_CLASS (scale)->value_spacing;
	  if (requisition->height < (widget->style->font->ascent + widget->style->font->descent))
	    requisition->height = widget->style->font->ascent + widget->style->font->descent;
	}
      else if ((scale->value_pos == GTK_POS_TOP) ||
	       (scale->value_pos == GTK_POS_BOTTOM))
	{
	  if (requisition->width < value_width)
	    requisition->width = value_width;
	  requisition->height += widget->style->font->ascent + widget->style->font->descent;
	}
    }
}

static void
gtk_hscale_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkRange *range;
  GtkScale *scale;
  gint width, height;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HSCALE (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      range = GTK_RANGE (widget);
      scale = GTK_SCALE (widget);

      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gtk_hscale_pos_trough (GTK_HSCALE (widget), &x, &y, &width, &height);

      gdk_window_move_resize (range->trough, x, y, width, height);
      gtk_range_slider_update (GTK_RANGE (widget));
    }
}

static void
gtk_hscale_pos_trough (GtkHScale *hscale,
		       gint      *x,
		       gint      *y,
		       gint      *w,
		       gint      *h)
{
  GtkWidget *widget;
  GtkScale *scale;

  g_return_if_fail (hscale != NULL);
  g_return_if_fail (GTK_IS_HSCALE (hscale));
  g_return_if_fail ((x != NULL) && (y != NULL) && (w != NULL) && (h != NULL));

  widget = GTK_WIDGET (hscale);
  scale = GTK_SCALE (hscale);

  *w = widget->allocation.width;
  *h = (RANGE_CLASS (scale)->slider_width +
	widget->style->klass->ythickness * 2);

  if (scale->draw_value)
    {
      *x = 0;
      *y = 0;

      switch (scale->value_pos)
	{
	case GTK_POS_LEFT:
	  *x += gtk_scale_value_width (scale) + SCALE_CLASS (scale)->value_spacing;
	  *y = (widget->allocation.height - *h) / 2;
	  *w -= *x;
	  break;
	case GTK_POS_RIGHT:
	  *w -= gtk_scale_value_width (scale) + SCALE_CLASS (scale)->value_spacing;
	  *y = (widget->allocation.height - *h) / 2;
	  break;
	case GTK_POS_TOP:
	  *y = (widget->style->font->ascent + widget->style->font->descent +
		(widget->allocation.height - widget->requisition.height) / 2);
	  break;
	case GTK_POS_BOTTOM:
	  *y = (widget->allocation.height - widget->requisition.height) / 2;
	  break;
	}
    }
  else
    {
      *x = 0;
      *y = (widget->allocation.height - *h) / 2;
    }
  *x += 1;
  *w -= 2;
}

static void
gtk_hscale_draw_slider (GtkRange *range)
{
  GtkStateType state_type;
  gint width, height;

  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_HSCALE (range));

  if (range->slider)
    {
      if ((range->in_child == RANGE_CLASS (range)->slider) ||
          (range->click_child == RANGE_CLASS (range)->slider))
        state_type = GTK_STATE_PRELIGHT;
      else
        state_type = GTK_STATE_NORMAL;

      gtk_style_set_background (GTK_WIDGET (range)->style, range->slider, state_type);
      gdk_window_clear (range->slider);

      gdk_window_get_size (range->slider, &width, &height);
      gtk_draw_vline (GTK_WIDGET (range)->style, range->slider,
		      state_type, 1, height - 2, width / 2);

      gtk_draw_shadow (GTK_WIDGET (range)->style, range->slider,
                       state_type, GTK_SHADOW_OUT,
                       0, 0, -1, -1);
    }
}

static void
gtk_hscale_draw_value (GtkScale *scale)
{
  GtkStateType state_type;
  gchar buffer[32];
  gint text_width;
  gint width, height;
  gint x, y;

  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_HSCALE (scale));

  if (scale->draw_value)
    {
      gdk_window_get_size (GTK_WIDGET (scale)->window, &width, &height);
      gdk_window_clear_area (GTK_WIDGET (scale)->window, 1, 1, width - 2, height - 2);

      sprintf (buffer, "%0.*f", GTK_RANGE (scale)->digits, GTK_RANGE (scale)->adjustment->value);
      text_width = gdk_string_measure (GTK_WIDGET (scale)->style->font, buffer);

      switch (scale->value_pos)
	{
	case GTK_POS_LEFT:
	  gdk_window_get_position (GTK_RANGE (scale)->trough, &x, &y);
	  gdk_window_get_size (GTK_RANGE (scale)->trough, &width, &height);

	  x -= SCALE_CLASS (scale)->value_spacing + text_width;
	  y += ((height -
		 (GTK_WIDGET (scale)->style->font->ascent +
		  GTK_WIDGET (scale)->style->font->descent)) / 2 +
		GTK_WIDGET (scale)->style->font->ascent);
	  break;
	case GTK_POS_RIGHT:
	  gdk_window_get_position (GTK_RANGE (scale)->trough, &x, &y);
	  gdk_window_get_size (GTK_RANGE (scale)->trough, &width, &height);

	  x += width + SCALE_CLASS (scale)->value_spacing;
	  y += ((height -
		 (GTK_WIDGET (scale)->style->font->ascent +
		  GTK_WIDGET (scale)->style->font->descent)) / 2 +
		GTK_WIDGET (scale)->style->font->ascent);
	  break;
	case GTK_POS_TOP:
	  gdk_window_get_position (GTK_RANGE (scale)->slider, &x, NULL);
	  gdk_window_get_position (GTK_RANGE (scale)->trough, NULL, &y);
	  gdk_window_get_size (GTK_RANGE (scale)->slider, &width, NULL);
	  gdk_window_get_size (GTK_RANGE (scale)->trough, NULL, &height);

	  x += (width - text_width) / 2;
	  y -= GTK_WIDGET (scale)->style->font->descent;
	  break;
	case GTK_POS_BOTTOM:
	  gdk_window_get_position (GTK_RANGE (scale)->slider, &x, NULL);
	  gdk_window_get_position (GTK_RANGE (scale)->trough, NULL, &y);
	  gdk_window_get_size (GTK_RANGE (scale)->slider, &width, NULL);
	  gdk_window_get_size (GTK_RANGE (scale)->trough, NULL, &height);

	  x += (width - text_width) / 2;
	  y += height + GTK_WIDGET (scale)->style->font->ascent;
	  break;
	}

      state_type = GTK_STATE_NORMAL;
      if (!GTK_WIDGET_IS_SENSITIVE (scale))
	state_type = GTK_STATE_INSENSITIVE;

      gtk_draw_string (GTK_WIDGET (scale)->style,
		       GTK_WIDGET (scale)->window,
		       state_type, x, y, buffer);
    }
}

static gint
gtk_hscale_trough_keys(GtkRange *range,
		       GdkEventKey *key,
		       GtkScrollType *scroll,
		       GtkTroughType *pos)
{
  gint return_val = FALSE;
  switch (key->keyval)
    {
    case GDK_Left:
      return_val = TRUE;
      if (key->state & GDK_CONTROL_MASK)
	*scroll = GTK_SCROLL_PAGE_BACKWARD;
      else
	*scroll = GTK_SCROLL_STEP_BACKWARD;
      break;
    case GDK_Right:
      return_val = TRUE;
      if (key->state & GDK_CONTROL_MASK)
	*scroll = GTK_SCROLL_PAGE_FORWARD;
      else
	*scroll = GTK_SCROLL_STEP_FORWARD;
      break;
    case GDK_Home:
      return_val = TRUE;
      *pos = GTK_TROUGH_START;
      break;
    case GDK_End:
      return_val = TRUE;
      *pos = GTK_TROUGH_END;
      break;
    }
  return return_val;
}
