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

#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtktogglebutton.h"
#include "gtkintl.h"

#define DEFAULT_LEFT_POS  4
#define DEFAULT_TOP_POS   4
#define DEFAULT_SPACING   7

enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_INCONSISTENT,
  PROP_DRAW_INDICATOR
};


static void gtk_toggle_button_class_init    (GtkToggleButtonClass *klass);
static void gtk_toggle_button_init          (GtkToggleButton      *toggle_button);
static void gtk_toggle_button_paint         (GtkWidget            *widget,
					     GdkRectangle         *area);
static void gtk_toggle_button_size_allocate (GtkWidget            *widget,
					     GtkAllocation        *allocation);
static gint gtk_toggle_button_expose        (GtkWidget            *widget,
					     GdkEventExpose       *event);
static void gtk_toggle_button_pressed       (GtkButton            *button);
static void gtk_toggle_button_released      (GtkButton            *button);
static void gtk_toggle_button_clicked       (GtkButton            *button);
static void gtk_toggle_button_set_property  (GObject              *object,
					     guint                 prop_id,
					     const GValue         *value,
					     GParamSpec           *pspec);
static void gtk_toggle_button_get_property  (GObject              *object,
					     guint                 prop_id,
					     GValue               *value,
					     GParamSpec           *pspec);
static void gtk_toggle_button_realize       (GtkWidget            *widget);
static void gtk_toggle_button_unrealize     (GtkWidget            *widget);
static void gtk_toggle_button_map           (GtkWidget            *widget);
static void gtk_toggle_button_unmap         (GtkWidget            *widget);
static void gtk_toggle_button_update_state  (GtkButton            *button);

static guint toggle_button_signals[LAST_SIGNAL] = { 0 };
static GtkContainerClass *parent_class = NULL;

GtkType
gtk_toggle_button_get_type (void)
{
  static GtkType toggle_button_type = 0;

  if (!toggle_button_type)
    {
      static const GtkTypeInfo toggle_button_info =
      {
	"GtkToggleButton",
	sizeof (GtkToggleButton),
	sizeof (GtkToggleButtonClass),
	(GtkClassInitFunc) gtk_toggle_button_class_init,
	(GtkObjectInitFunc) gtk_toggle_button_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      toggle_button_type = gtk_type_unique (GTK_TYPE_BUTTON, &toggle_button_info);
    }

  return toggle_button_type;
}

static void
gtk_toggle_button_class_init (GtkToggleButtonClass *class)
{
  GtkObjectClass *object_class;
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkButtonClass *button_class;

  object_class = (GtkObjectClass*) class;
  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;
  button_class = (GtkButtonClass*) class;

  parent_class = gtk_type_class (GTK_TYPE_BUTTON);


  gobject_class->set_property = gtk_toggle_button_set_property;
  gobject_class->get_property = gtk_toggle_button_get_property;

  widget_class->size_allocate = gtk_toggle_button_size_allocate;
  widget_class->expose_event = gtk_toggle_button_expose;
  widget_class->realize = gtk_toggle_button_realize;
  widget_class->unrealize = gtk_toggle_button_unrealize;
  widget_class->map = gtk_toggle_button_map;
  widget_class->unmap = gtk_toggle_button_unmap;

  button_class->pressed = gtk_toggle_button_pressed;
  button_class->released = gtk_toggle_button_released;
  button_class->clicked = gtk_toggle_button_clicked;
  button_class->enter = gtk_toggle_button_update_state;
  button_class->leave = gtk_toggle_button_update_state;

  class->toggled = NULL;

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
							 _("Active"),
							 _("If the toggle button should be pressed in or not"),
							 FALSE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_INCONSISTENT,
                                   g_param_spec_boolean ("inconsistent",
							 _("Inconsistent"),
							 _("If the toggle button is in an \"in between\" state."),
							 FALSE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_INDICATOR,
                                   g_param_spec_boolean ("draw_indicator",
							 _("Draw Indicator"),
							 _("If the toggle part of the button is displayed"),
							 FALSE,
							 G_PARAM_READWRITE));

  toggle_button_signals[TOGGLED] =
    gtk_signal_new ("toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkToggleButtonClass, toggled),
                    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
}

static void
gtk_toggle_button_init (GtkToggleButton *toggle_button)
{
  toggle_button->active = FALSE;
  toggle_button->draw_indicator = FALSE;
  GTK_WIDGET_UNSET_FLAGS (toggle_button, GTK_NO_WINDOW);
}


GtkWidget*
gtk_toggle_button_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_toggle_button_get_type ()));
}

GtkWidget*
gtk_toggle_button_new_with_label (const gchar *label)
{
  GtkWidget *toggle_button;
  GtkWidget *label_widget;

  toggle_button = gtk_toggle_button_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.5, 0.5);

  gtk_container_add (GTK_CONTAINER (toggle_button), label_widget);
  gtk_widget_show (label_widget);

  return toggle_button;
}

/**
 * gtk_toggle_button_new_with_mnemonic:
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkToggleButton
 *
 * Creates a new #GtkToggleButton containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the button.
 **/
GtkWidget*
gtk_toggle_button_new_with_mnemonic (const gchar *label)
{
  GtkWidget *toggle_button;
  GtkWidget *label_widget;

  toggle_button = gtk_toggle_button_new ();
  label_widget = gtk_label_new_with_mnemonic (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.5, 0.5);

  gtk_container_add (GTK_CONTAINER (toggle_button), label_widget);
  gtk_widget_show (label_widget);

  return toggle_button;
}

static void
gtk_toggle_button_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  GtkToggleButton *tb;

  tb = GTK_TOGGLE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_toggle_button_set_active (tb, g_value_get_boolean (value));
      break;
    case PROP_INCONSISTENT:
      gtk_toggle_button_set_inconsistent (tb, g_value_get_boolean (value));
      break;
    case PROP_DRAW_INDICATOR:
      gtk_toggle_button_set_mode (tb, g_value_get_boolean (value));
      break;
    default:
      break;
    }
}

static void
gtk_toggle_button_get_property (GObject      *object,
				guint         prop_id,
				GValue       *value,
				GParamSpec   *pspec)
{
  GtkToggleButton *tb;

  tb = GTK_TOGGLE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, tb->active);
      break;
    case PROP_INCONSISTENT:
      g_value_set_boolean (value, tb->inconsistent);
      break;
    case PROP_DRAW_INDICATOR:
      g_value_set_boolean (value, tb->draw_indicator);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gtk_toggle_button_set_mode (GtkToggleButton *toggle_button,
			    gboolean         draw_indicator)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  widget = GTK_WIDGET (toggle_button);

  draw_indicator = draw_indicator ? TRUE : FALSE;

  if (toggle_button->draw_indicator != draw_indicator)
    {
      if (GTK_WIDGET_REALIZED (toggle_button))
	{
	  gboolean visible = GTK_WIDGET_VISIBLE (toggle_button);

	  if (visible)
	    gtk_widget_hide (widget);

	  gtk_widget_unrealize (widget);
	  toggle_button->draw_indicator = draw_indicator;

	  if (toggle_button->draw_indicator)
	    GTK_WIDGET_SET_FLAGS (toggle_button, GTK_NO_WINDOW);
	  else
	    GTK_WIDGET_UNSET_FLAGS (toggle_button, GTK_NO_WINDOW);
	  
	  gtk_widget_realize (widget);

	  if (visible)
	    gtk_widget_show (widget);
	}
      else
	{
	  toggle_button->draw_indicator = draw_indicator;

	  if (toggle_button->draw_indicator)
	    GTK_WIDGET_SET_FLAGS (toggle_button, GTK_NO_WINDOW);
	  else
	    GTK_WIDGET_UNSET_FLAGS (toggle_button, GTK_NO_WINDOW);
	}

      if (GTK_WIDGET_VISIBLE (toggle_button))
	gtk_widget_queue_resize (GTK_WIDGET (toggle_button));

      g_object_notify (G_OBJECT (toggle_button), "draw_indicator");
    }
}

/**
 * gtk_toggle_button_get_mode:
 * @toggle_button: a #GtkToggleButton
 *
 * Retrieves whether the button is displayed as a separate indicator
 * and label. See gtk_toggle_button_set_mode().
 *
 * Return value: %TRUE if the togglebutton is drawn as a separate indicator
 *   and label.
 **/
gboolean
gtk_toggle_button_get_mode (GtkToggleButton *toggle_button)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);

  return toggle_button->draw_indicator;
}

void
gtk_toggle_button_set_active (GtkToggleButton *toggle_button,
			      gboolean         is_active)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  is_active = is_active != FALSE;

  if (toggle_button->active != is_active)
    gtk_button_clicked (GTK_BUTTON (toggle_button));
}


gboolean
gtk_toggle_button_get_active (GtkToggleButton *toggle_button)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);

  return (toggle_button->active) ? TRUE : FALSE;
}


void
gtk_toggle_button_toggled (GtkToggleButton *toggle_button)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  gtk_signal_emit (GTK_OBJECT (toggle_button), toggle_button_signals[TOGGLED]);
}

/**
 * gtk_toggle_button_set_inconsistent:
 * @toggle_button: a #GtkToggleButton
 * @setting: %TRUE if state is inconsistent
 *
 * If the user has selected a range of elements (such as some text or
 * spreadsheet cells) that are affected by a toggle button, and the
 * current values in that range are inconsistent, you may want to
 * display the toggle in an "in between" state. This function turns on
 * "in between" display.  Normally you would turn off the inconsistent
 * state again if the user toggles the toggle button. This has to be
 * done manually, gtk_toggle_button_set_inconsistent() only affects
 * visual appearance, it doesn't affect the semantics of the button.
 * 
 **/
void
gtk_toggle_button_set_inconsistent (GtkToggleButton *toggle_button,
                                    gboolean         setting)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));
  
  setting = setting != FALSE;

  if (setting != toggle_button->inconsistent)
    {
      toggle_button->inconsistent = setting;
      
      gtk_toggle_button_update_state (GTK_BUTTON (toggle_button));
      gtk_widget_queue_draw (GTK_WIDGET (toggle_button));

      g_object_notify (G_OBJECT (toggle_button), "inconsistent");      
    }
}

/**
 * gtk_toggle_button_get_inconsistent:
 * @toggle_button: a #GtkToggleButton
 * 
 * Gets the value set by gtk_toggle_button_set_inconsistent().
 * 
 * Return value: %TRUE if the button is displayed as inconsistent, %FALSE otherwise
 **/
gboolean
gtk_toggle_button_get_inconsistent (GtkToggleButton *toggle_button)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);

  return toggle_button->inconsistent;
}

static void
gtk_toggle_button_paint (GtkWidget    *widget,
			 GdkRectangle *area)
{
  GtkButton *button;
  GtkToggleButton *toggle_button;
  GtkShadowType shadow_type;
  GtkStateType state_type;
  gint width, height;
  gboolean interior_focus;
  gint x, y;

  button = GTK_BUTTON (widget);
  toggle_button = GTK_TOGGLE_BUTTON (widget);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_widget_style_get (widget, "interior_focus", &interior_focus, NULL);
      
      x = 0;
      y = 0;
      width = widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2;
      height = widget->allocation.height - GTK_CONTAINER (widget)->border_width * 2;

      gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
      gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

      if (GTK_WIDGET_HAS_DEFAULT (widget) &&
          GTK_BUTTON (widget)->relief == GTK_RELIEF_NORMAL)
        {
          gtk_paint_box (widget->style, widget->window,
                         GTK_STATE_NORMAL, GTK_SHADOW_IN,
                         area, widget, "togglebuttondefault",
                         x, y, width, height);
        }

      if (GTK_WIDGET_CAN_DEFAULT (widget))
        {
          x += widget->style->xthickness;
          y += widget->style->ythickness;
          width -= 2 * x + DEFAULT_SPACING;
          height -= 2 * y + DEFAULT_SPACING;
          x += DEFAULT_LEFT_POS;
          y += DEFAULT_TOP_POS;
        }

      if (GTK_WIDGET_HAS_FOCUS (widget) && !interior_focus)
	{
	  x += 1;
	  y += 1;
	  width -= 2;
	  height -= 2;
	}

      state_type = GTK_WIDGET_STATE (widget);
      
      if (toggle_button->inconsistent)
        {
          if (state_type == GTK_STATE_ACTIVE)
            state_type = GTK_STATE_NORMAL;
          shadow_type = GTK_SHADOW_ETCHED_IN;
        }
      else
	shadow_type = button->depressed ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

      if (button->relief != GTK_RELIEF_NONE ||
	  (GTK_WIDGET_STATE(widget) != GTK_STATE_NORMAL &&
	   GTK_WIDGET_STATE(widget) != GTK_STATE_INSENSITIVE))
	gtk_paint_box (widget->style, widget->window,
                       state_type,
		       shadow_type, area, widget, "togglebutton",
		       x, y, width, height);
      
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  if (interior_focus)
	    {
	      x += widget->style->xthickness + 1;
	      y += widget->style->xthickness + 1;
	      width -= 2 * (widget->style->xthickness + 1);
	      height -= 2 * (widget->style->ythickness + 1);
	    }
	  else
	    {
	      x -= 1;
	      y -= 1;
	      width += 2;
	      height += 2;
	    }

	  gtk_paint_focus (widget->style, widget->window,
			   area, widget, "togglebutton",
			   x, y, width - 1, height - 1);
	}
    }
}

static void
gtk_toggle_button_size_allocate (GtkWidget     *widget,
				 GtkAllocation *allocation)
{
  if (!GTK_WIDGET_NO_WINDOW (widget) &&
      GTK_WIDGET_CLASS (parent_class)->size_allocate)
    GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
}

static gint
gtk_toggle_button_expose (GtkWidget      *widget,
			  GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      GtkWidget *child = GTK_BIN (widget)->child;

      gtk_toggle_button_paint (widget, &event->area);

      if (child)
	gtk_container_propagate_expose (GTK_CONTAINER (widget), child, event);
    }
  
  return TRUE;
}

static void
gtk_toggle_button_pressed (GtkButton *button)
{
  button->button_down = TRUE;

  gtk_toggle_button_update_state (button);
}

static void
gtk_toggle_button_released (GtkButton *button)
{
  if (button->button_down)
    {
      button->button_down = FALSE;

      if (button->in_button)
	gtk_button_clicked (button);

      gtk_toggle_button_update_state (button);
    }
}

static void
gtk_toggle_button_clicked (GtkButton *button)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (button);
  toggle_button->active = !toggle_button->active;

  gtk_toggle_button_toggled (toggle_button);

  gtk_toggle_button_update_state (button);

  g_object_notify (G_OBJECT (toggle_button), "active");
}

static void
gtk_toggle_button_realize (GtkWidget *widget)
{
  GtkToggleButton *toggle_button;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;
  
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));
  
  toggle_button = GTK_TOGGLE_BUTTON (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  
  border_width = GTK_CONTAINER (widget)->border_width;
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      attributes.wclass = GDK_INPUT_ONLY;
      attributes_mask = GDK_WA_X | GDK_WA_Y;

      widget->window = gtk_widget_get_parent_window (widget);
      gdk_window_ref (widget->window);
      
      toggle_button->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
						    &attributes, attributes_mask);
      gdk_window_set_user_data (toggle_button->event_window, toggle_button);
    }
  else
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				       &attributes, attributes_mask);
      gdk_window_set_user_data (widget->window, toggle_button);
    }

  widget->style = gtk_style_attach (widget->style, widget->window);

  if (!GTK_WIDGET_NO_WINDOW (widget))
    gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}
  
static void
gtk_toggle_button_unrealize (GtkWidget *widget)
{
  GtkToggleButton *toggle_button;
  
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

  toggle_button = GTK_TOGGLE_BUTTON (widget);
  
  if (GTK_WIDGET_NO_WINDOW (widget))
    {
      gdk_window_set_user_data (toggle_button->event_window, NULL);
      gdk_window_destroy (toggle_button->event_window);
      toggle_button->event_window = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_toggle_button_map (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

  if (GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_show (GTK_TOGGLE_BUTTON (widget)->event_window);

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static void
gtk_toggle_button_unmap (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

  if (GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_hide (GTK_TOGGLE_BUTTON (widget)->event_window);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
gtk_toggle_button_update_state (GtkButton *button)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (button);
  gboolean depressed;
  GtkStateType new_state;

  if (toggle_button->inconsistent)
    depressed = FALSE;
  else if (button->in_button && button->button_down)
    depressed = !toggle_button->active;
  else
    depressed = toggle_button->active;
      
  if (!button->button_down && button->in_button)
    new_state = GTK_STATE_PRELIGHT;
  else
    new_state = depressed ? GTK_STATE_ACTIVE: GTK_STATE_NORMAL;

  _gtk_button_set_depressed (button, depressed); 
  gtk_widget_set_state (GTK_WIDGET (toggle_button), new_state);
}
