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

#include "gtkcheckbutton.h"
#include "gtkintl.h"
#include "gtklabel.h"


#define INDICATOR_SIZE     13
#define INDICATOR_SPACING  2


static void gtk_check_button_class_init          (GtkCheckButtonClass *klass);
static void gtk_check_button_init                (GtkCheckButton      *check_button);
static void gtk_check_button_size_request        (GtkWidget           *widget,
						  GtkRequisition      *requisition);
static void gtk_check_button_size_allocate       (GtkWidget           *widget,
						  GtkAllocation       *allocation);
static gint gtk_check_button_expose              (GtkWidget           *widget,
						  GdkEventExpose      *event);
static void gtk_check_button_paint               (GtkWidget           *widget,
						  GdkRectangle        *area);
static void gtk_check_button_draw_indicator      (GtkCheckButton      *check_button,
						  GdkRectangle        *area);
static void gtk_real_check_button_draw_indicator (GtkCheckButton      *check_button,
						  GdkRectangle        *area);

static GtkToggleButtonClass *parent_class = NULL;


GtkType
gtk_check_button_get_type (void)
{
  static GtkType check_button_type = 0;
  
  if (!check_button_type)
    {
      static const GtkTypeInfo check_button_info =
      {
	"GtkCheckButton",
	sizeof (GtkCheckButton),
	sizeof (GtkCheckButtonClass),
	(GtkClassInitFunc) gtk_check_button_class_init,
	(GtkObjectInitFunc) gtk_check_button_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      check_button_type = gtk_type_unique (GTK_TYPE_TOGGLE_BUTTON, &check_button_info);
    }
  
  return check_button_type;
}

static void
gtk_check_button_class_init (GtkCheckButtonClass *class)
{
  GtkWidgetClass *widget_class;
  
  widget_class = (GtkWidgetClass*) class;
  parent_class = gtk_type_class (gtk_toggle_button_get_type ());
  
  widget_class->size_request = gtk_check_button_size_request;
  widget_class->size_allocate = gtk_check_button_size_allocate;
  widget_class->expose_event = gtk_check_button_expose;

  class->draw_indicator = gtk_real_check_button_draw_indicator;

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("indicator_size",
							     _("Indicator Size"),
							     _("Size of check or radio indicator"),
							     0,
							     G_MAXINT,
							     INDICATOR_SIZE,
							     G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("indicator_spacing",
							     _("Indicator Spacing"),
							     _("Spacing around check or radio indicator"),
							     0,
							     G_MAXINT,
							     INDICATOR_SPACING,
							     G_PARAM_READABLE));
}

static void
gtk_check_button_init (GtkCheckButton *check_button)
{
  GTK_WIDGET_SET_FLAGS (check_button, GTK_NO_WINDOW);
  GTK_WIDGET_UNSET_FLAGS (check_button, GTK_RECEIVES_DEFAULT);
  GTK_TOGGLE_BUTTON (check_button)->draw_indicator = TRUE;
}

GtkWidget*
gtk_check_button_new (void)
{
  return gtk_widget_new (GTK_TYPE_CHECK_BUTTON, NULL);
}


GtkWidget*
gtk_check_button_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON, "label", label, NULL);
}

/**
 * gtk_check_button_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkCheckButton
 *
 * Creates a new #GtkCheckButton containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the check button.
 **/
GtkWidget*
gtk_check_button_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON, "label", label, "use_underline", TRUE, NULL);
}


/* This should only be called when toggle_button->draw_indicator
 * is true.
 */
static void
gtk_check_button_paint (GtkWidget    *widget,
			GdkRectangle *area)
{
  GtkCheckButton *check_button = GTK_CHECK_BUTTON (widget);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gint border_width;
      gint interior_focus;
      gint focus_width;
      gint focus_pad;
	  
      gtk_widget_style_get (widget,
			    "interior-focus", &interior_focus,
			    "focus-line-width", &focus_width,
			    "focus-padding", &focus_pad,
			    NULL);

      gtk_check_button_draw_indicator (check_button, area);
      
      border_width = GTK_CONTAINER (widget)->border_width;
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  if (interior_focus)
	    {
	      GtkWidget *child = GTK_BIN (widget)->child;
	      
	      if (child && GTK_WIDGET_VISIBLE (child))
		gtk_paint_focus (widget->style, widget->window, GTK_WIDGET_STATE (widget),
				 NULL, widget, "checkbutton",
				 child->allocation.x - focus_width - focus_pad,
				 child->allocation.y - focus_width - focus_pad,
				 child->allocation.width + 2 * (focus_width + focus_pad),
				 child->allocation.height + 2 * (focus_width + focus_pad));
	    }
	  else
	    gtk_paint_focus (widget->style, widget->window, GTK_WIDGET_STATE (widget), 
			     NULL, widget, "checkbutton",
			     border_width + widget->allocation.x,
			     border_width + widget->allocation.y,
			     widget->allocation.width - 2 * border_width,
			     widget->allocation.height - 2 * border_width);
	}
    }
}

void
_gtk_check_button_get_props (GtkCheckButton *check_button,
			     gint           *indicator_size,
			     gint           *indicator_spacing)
{
  GtkWidget *widget =  GTK_WIDGET (check_button);

  if (indicator_size)
    gtk_widget_style_get (widget, "indicator_size", indicator_size, NULL);

  if (indicator_spacing)
    gtk_widget_style_get (widget, "indicator_spacing", indicator_spacing, NULL);
}

static void
gtk_check_button_size_request (GtkWidget      *widget,
			       GtkRequisition *requisition)
{
  GtkToggleButton *toggle_button;
  
  g_return_if_fail (GTK_IS_CHECK_BUTTON (widget));
  g_return_if_fail (requisition != NULL);
  
  toggle_button = GTK_TOGGLE_BUTTON (widget);
  
  if (toggle_button->draw_indicator)
    {
      GtkWidget *child;
      gint temp;
      gint indicator_size;
      gint indicator_spacing;
      gint border_width = GTK_CONTAINER (widget)->border_width;
      gint focus_width;
      gint focus_pad;

      gtk_widget_style_get (GTK_WIDGET (widget),
			    "focus-line-width", &focus_width,
			    "focus-padding", &focus_pad,
			    NULL);
 
      requisition->width = border_width * 2;
      requisition->height = border_width * 2;

      child = GTK_BIN (widget)->child;
      if (child && GTK_WIDGET_VISIBLE (child))
	{
	  GtkRequisition child_requisition;
	  
	  gtk_widget_size_request (child, &child_requisition);

	  requisition->width += child_requisition.width;
	  requisition->height += child_requisition.height;
	}
      
      _gtk_check_button_get_props (GTK_CHECK_BUTTON (widget),
 				   &indicator_size, &indicator_spacing);
      
      requisition->width += (indicator_size + indicator_spacing * 3 + 2 * (focus_width + focus_pad));
      
      temp = indicator_size + indicator_spacing * 2;
      requisition->height = MAX (requisition->height, temp) + 2 * (focus_width + focus_pad);
    }
  else
    (* GTK_WIDGET_CLASS (parent_class)->size_request) (widget, requisition);
}

static void
gtk_check_button_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  GtkCheckButton *check_button;
  GtkToggleButton *toggle_button;
  GtkButton *button;
  GtkAllocation child_allocation;

  g_return_if_fail (GTK_IS_CHECK_BUTTON (widget));
  g_return_if_fail (allocation != NULL);
  
  button = GTK_BUTTON (widget);
  check_button = GTK_CHECK_BUTTON (widget);
  toggle_button = GTK_TOGGLE_BUTTON (widget);

  if (toggle_button->draw_indicator)
    {
      gint indicator_size;
      gint indicator_spacing;
      gint focus_width;
      gint focus_pad;
      
      _gtk_check_button_get_props (check_button, &indicator_size, &indicator_spacing);
      gtk_widget_style_get (widget,
			    "focus-line-width", &focus_width,
			    "focus-padding", &focus_pad,
			    NULL);
						    
      widget->allocation = *allocation;
      if (GTK_WIDGET_REALIZED (widget))
	gdk_window_move_resize (button->event_window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);
      
      if (GTK_BIN (button)->child && GTK_WIDGET_VISIBLE (GTK_BIN (button)->child))
	{
	  GtkRequisition child_requisition;
 	  gint border_width = GTK_CONTAINER (widget)->border_width;

	  gtk_widget_get_child_requisition (GTK_BIN (button)->child, &child_requisition);
 
	  child_allocation.width = MIN (child_requisition.width,
					allocation->width -
					((border_width + focus_width + focus_pad) * 2
					 + indicator_size + indicator_spacing * 3));

	  child_allocation.height = MIN (child_requisition.height,
					 allocation->height - (border_width + focus_width + focus_pad) * 2);
	  child_allocation.x = (border_width + indicator_size + indicator_spacing * 3 +
				widget->allocation.x + focus_width + focus_pad);
	  child_allocation.y = widget->allocation.y +
		  (allocation->height - child_allocation.height) / 2;

	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	    child_allocation.x = allocation->x + allocation->width
	      - (child_allocation.x - allocation->x + child_allocation.width);
	  
	  gtk_widget_size_allocate (GTK_BIN (button)->child, &child_allocation);
	}
    }
  else
    (* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);
}

static gint
gtk_check_button_expose (GtkWidget      *widget,
			 GdkEventExpose *event)
{
  GtkCheckButton *check_button;
  GtkToggleButton *toggle_button;
  GtkBin *bin;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  check_button = GTK_CHECK_BUTTON (widget);
  toggle_button = GTK_TOGGLE_BUTTON (widget);
  bin = GTK_BIN (widget);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (toggle_button->draw_indicator)
	{
	  gtk_check_button_paint (widget, &event->area);
	  
	  if (bin->child)
	    gtk_container_propagate_expose (GTK_CONTAINER (widget),
					    bin->child,
					    event);
	}
      else if (GTK_WIDGET_CLASS (parent_class)->expose_event)
	(* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }
  
  return FALSE;
}


static void
gtk_check_button_draw_indicator (GtkCheckButton *check_button,
				 GdkRectangle   *area)
{
  GtkCheckButtonClass *class;
  
  g_return_if_fail (GTK_IS_CHECK_BUTTON (check_button));
  
  class = GTK_CHECK_BUTTON_GET_CLASS (check_button);
  
  if (class->draw_indicator)
    (* class->draw_indicator) (check_button, area);
}

static void
gtk_real_check_button_draw_indicator (GtkCheckButton *check_button,
				      GdkRectangle   *area)
{
  GtkWidget *widget;
  GtkToggleButton *toggle_button;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GdkRectangle restrict_area;
  GdkRectangle new_area;
  gint width, height;
  gint x, y;
  gint indicator_size;
  gint indicator_spacing;
  gint focus_width;
  gint focus_pad;
  gboolean interior_focus;
  GdkWindow *window;
  
  g_return_if_fail (GTK_IS_CHECK_BUTTON (check_button));
  
  widget = GTK_WIDGET (check_button);
  toggle_button = GTK_TOGGLE_BUTTON (check_button);
  
  if (GTK_WIDGET_DRAWABLE (check_button))
    {
      window = widget->window;

      gtk_widget_style_get (widget, "interior_focus", &interior_focus,
			    "focus-line-width", &focus_width, 
			    "focus-padding", &focus_pad, NULL);

      _gtk_check_button_get_props (check_button, &indicator_size, &indicator_spacing);
						    
      state_type = GTK_WIDGET_STATE (widget);
      if (state_type != GTK_STATE_NORMAL &&
	  state_type != GTK_STATE_PRELIGHT)
	state_type = GTK_STATE_NORMAL;
      
      restrict_area.x = widget->allocation.x + GTK_CONTAINER (widget)->border_width;
      restrict_area.y = widget->allocation.y + GTK_CONTAINER (widget)->border_width;
      restrict_area.width = widget->allocation.width - ( 2 * GTK_CONTAINER (widget)->border_width);
      restrict_area.height = widget->allocation.height - ( 2 * GTK_CONTAINER (widget)->border_width);
      
      if (gdk_rectangle_intersect (area, &restrict_area, &new_area))
	{
	  if (state_type != GTK_STATE_NORMAL)
	    gtk_paint_flat_box (widget->style, window, state_type, 
				GTK_SHADOW_ETCHED_OUT, 
				area, widget, "checkbutton",
				new_area.x, new_area.y,
				new_area.width, new_area.height);
	}
      
      x = widget->allocation.x + indicator_spacing + GTK_CONTAINER (widget)->border_width;
      y = widget->allocation.y + (widget->allocation.height - indicator_size) / 2;
      width = indicator_size;
      height = indicator_size;

      if (!interior_focus)
	x += focus_width + focus_pad;      

      state_type = GTK_WIDGET_STATE (widget) == GTK_STATE_ACTIVE ? GTK_STATE_NORMAL : GTK_WIDGET_STATE (widget);
      if (GTK_TOGGLE_BUTTON (widget)->inconsistent)
	shadow_type = GTK_SHADOW_ETCHED_IN;
      else if (GTK_TOGGLE_BUTTON (widget)->active)
	shadow_type = GTK_SHADOW_IN;
      else
	shadow_type = GTK_SHADOW_OUT;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	x = widget->allocation.x + widget->allocation.width - (width + x - widget->allocation.x);

      gtk_paint_check (widget->style, window,
		       state_type, shadow_type,
		       area, widget, "checkbutton",
		       x, y, width, height);
    }
}
