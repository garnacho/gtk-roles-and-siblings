/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkSpinButton widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <locale.h>
#include "gdk/gdkkeysyms.h"
#include "gtkspinbutton.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtksettings.h"
#include "gtkintl.h"

#define MIN_SPIN_BUTTON_WIDTH              30
#define ARROW_SIZE                         11
#define SPIN_BUTTON_INITIAL_TIMER_DELAY    200
#define SPIN_BUTTON_TIMER_DELAY            20
#define MAX_TIMER_CALLS                    5
#define EPSILON                            1e-5
#define	MAX_DIGITS			   20

enum {
  PROP_0,
  PROP_ADJUSTMENT,
  PROP_CLIMB_RATE,
  PROP_DIGITS,
  PROP_SNAP_TO_TICKS,
  PROP_NUMERIC,
  PROP_WRAP,
  PROP_UPDATE_POLICY,
  PROP_VALUE
};

/* Signals */
enum
{
  INPUT,
  OUTPUT,
  VALUE_CHANGED,
  LAST_SIGNAL
};

static void gtk_spin_button_class_init     (GtkSpinButtonClass *klass);
static void gtk_spin_button_init           (GtkSpinButton      *spin_button);
static void gtk_spin_button_finalize       (GObject            *object);
static void gtk_spin_button_set_property   (GObject         *object,
					    guint            prop_id,
					    const GValue    *value,
					    GParamSpec      *pspec);
static void gtk_spin_button_get_property   (GObject         *object,
					    guint            prop_id,
					    GValue          *value,
					    GParamSpec      *pspec);
static void gtk_spin_button_map            (GtkWidget          *widget);
static void gtk_spin_button_unmap          (GtkWidget          *widget);
static void gtk_spin_button_realize        (GtkWidget          *widget);
static void gtk_spin_button_unrealize      (GtkWidget          *widget);
static void gtk_spin_button_size_request   (GtkWidget          *widget,
					    GtkRequisition     *requisition);
static void gtk_spin_button_size_allocate  (GtkWidget          *widget,
					    GtkAllocation      *allocation);
static gint gtk_spin_button_expose         (GtkWidget          *widget,
					    GdkEventExpose     *event);
static gint gtk_spin_button_button_press   (GtkWidget          *widget,
					    GdkEventButton     *event);
static gint gtk_spin_button_button_release (GtkWidget          *widget,
					    GdkEventButton     *event);
static gint gtk_spin_button_motion_notify  (GtkWidget          *widget,
					    GdkEventMotion     *event);
static gint gtk_spin_button_enter_notify   (GtkWidget          *widget,
					    GdkEventCrossing   *event);
static gint gtk_spin_button_leave_notify   (GtkWidget          *widget,
					    GdkEventCrossing   *event);
static gint gtk_spin_button_focus_out      (GtkWidget          *widget,
					    GdkEventFocus      *event);
static void gtk_spin_button_draw_arrow     (GtkSpinButton      *spin_button, 
					    guint               arrow);
static gint gtk_spin_button_timer          (GtkSpinButton      *spin_button);
static void gtk_spin_button_value_changed  (GtkAdjustment      *adjustment,
					    GtkSpinButton      *spin_button); 
static gint gtk_spin_button_key_press      (GtkWidget          *widget,
					    GdkEventKey        *event);
static gint gtk_spin_button_key_release    (GtkWidget          *widget,
					    GdkEventKey        *event);
static gint gtk_spin_button_scroll         (GtkWidget          *widget,
					    GdkEventScroll     *event);
static void gtk_spin_button_activate       (GtkEntry           *entry);
static void gtk_spin_button_snap           (GtkSpinButton      *spin_button,
					    gdouble             val);
static void gtk_spin_button_insert_text    (GtkEntry           *entry,
					    const gchar        *new_text,
					    gint                new_text_length,
					    gint               *position);
static void gtk_spin_button_real_spin      (GtkSpinButton      *spin_button,
					    gdouble             step);
static gint gtk_spin_button_default_input  (GtkSpinButton      *spin_button,
					    gdouble            *new_val);
static gint gtk_spin_button_default_output (GtkSpinButton      *spin_button);
static gint spin_button_get_shadow_type    (GtkSpinButton      *spin_button);


static GtkEntryClass *parent_class = NULL;
static guint spinbutton_signals[LAST_SIGNAL] = {0};


GtkType
gtk_spin_button_get_type (void)
{
  static guint spin_button_type = 0;

  if (!spin_button_type)
    {
      static const GtkTypeInfo spin_button_info =
      {
	"GtkSpinButton",
	sizeof (GtkSpinButton),
	sizeof (GtkSpinButtonClass),
	(GtkClassInitFunc) gtk_spin_button_class_init,
	(GtkObjectInitFunc) gtk_spin_button_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      spin_button_type = gtk_type_unique (GTK_TYPE_ENTRY, &spin_button_info);
    }
  return spin_button_type;
}

static void
gtk_spin_button_class_init (GtkSpinButtonClass *class)
{
  GObjectClass     *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass   *object_class;
  GtkWidgetClass   *widget_class;
  GtkEntryClass    *entry_class;

  object_class   = (GtkObjectClass*)   class;
  widget_class   = (GtkWidgetClass*)   class;
  entry_class    = (GtkEntryClass*)    class; 

  parent_class = gtk_type_class (GTK_TYPE_ENTRY);

  gobject_class->finalize = gtk_spin_button_finalize;

  gobject_class->set_property = gtk_spin_button_set_property;
  gobject_class->get_property = gtk_spin_button_get_property;

  widget_class->map = gtk_spin_button_map;
  widget_class->unmap = gtk_spin_button_unmap;
  widget_class->realize = gtk_spin_button_realize;
  widget_class->unrealize = gtk_spin_button_unrealize;
  widget_class->size_request = gtk_spin_button_size_request;
  widget_class->size_allocate = gtk_spin_button_size_allocate;
  widget_class->expose_event = gtk_spin_button_expose;
  widget_class->scroll_event = gtk_spin_button_scroll;
  widget_class->button_press_event = gtk_spin_button_button_press;
  widget_class->button_release_event = gtk_spin_button_button_release;
  widget_class->motion_notify_event = gtk_spin_button_motion_notify;
  widget_class->key_press_event = gtk_spin_button_key_press;
  widget_class->key_release_event = gtk_spin_button_key_release;
  widget_class->enter_notify_event = gtk_spin_button_enter_notify;
  widget_class->leave_notify_event = gtk_spin_button_leave_notify;
  widget_class->focus_out_event = gtk_spin_button_focus_out;

  entry_class->insert_text = gtk_spin_button_insert_text;
  entry_class->activate = gtk_spin_button_activate;

  class->input = NULL;
  class->output = NULL;

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
                                                        _("Adjustment"),
                                                        _("The adjustment that holds the value of the spinbutton"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_CLIMB_RATE,
                                   g_param_spec_double ("climb_rate",
							_("Climb Rate"),
							_("The acceleration rate when you hold down a button"),
							0.0,
							G_MAXDOUBLE,
							0.0,
							G_PARAM_READWRITE));  
  
  g_object_class_install_property (gobject_class,
                                   PROP_DIGITS,
                                   g_param_spec_uint ("digits",
						      _("Digits"),
						      _("The number of decimal places to display"),
						      0,
						      MAX_DIGITS,
						      0,
						      G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SNAP_TO_TICKS,
                                   g_param_spec_boolean ("snap_to_ticks",
							 _("Snap to Ticks"),
							 _("Whether erroneous values are automatically changed to a spin button's nearest step increment"),
							 FALSE,
							 G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_NUMERIC,
                                   g_param_spec_boolean ("numeric",
							 _("Numeric"),
							 _("Whether non-numeric characters should be ignored"),
							 FALSE,
							 G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_WRAP,
                                   g_param_spec_boolean ("wrap",
							 _("Wrap"),
							 _("Whether a spin button should wrap upon reaching its limits"),
							 FALSE,
							 G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_UPDATE_POLICY,
                                   g_param_spec_enum ("update_policy",
						      _("Update Policy"),
						      _("Whether the spin button should update always, or only when the value is legal"),
						      GTK_TYPE_SPIN_BUTTON_UPDATE_POLICY,
						      GTK_UPDATE_ALWAYS,
						      G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_VALUE,
                                   g_param_spec_double ("value",
							_("Value"),
							_("Reads the current value, or sets a new value"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE,
							0.0,
							G_PARAM_READWRITE));  
  
  gtk_widget_class_install_style_property_parser (widget_class,
						  g_param_spec_enum ("shadow_type", "Shadow Type", NULL,
								     GTK_TYPE_SHADOW_TYPE,
								     GTK_SHADOW_IN,
								     G_PARAM_READABLE),
						  gtk_rc_property_parse_enum);
  spinbutton_signals[INPUT] =
    gtk_signal_new ("input",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkSpinButtonClass, input),
		    gtk_marshal_INT__POINTER,
		    GTK_TYPE_INT, 1, GTK_TYPE_POINTER);

  spinbutton_signals[OUTPUT] =
    g_signal_newc ("output",
		   G_TYPE_FROM_CLASS(object_class),
		   G_SIGNAL_RUN_LAST,
		   G_STRUCT_OFFSET(GtkSpinButtonClass, output),
		   _gtk_boolean_handled_accumulator, NULL,
		   gtk_marshal_BOOLEAN__VOID,
		   G_TYPE_BOOLEAN, 0);

  spinbutton_signals[VALUE_CHANGED] =
    gtk_signal_new ("value_changed",
		    GTK_RUN_LAST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkSpinButtonClass, value_changed),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
}

static void
gtk_spin_button_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkSpinButton *spin_button;

  spin_button = GTK_SPIN_BUTTON (object);
  
  switch (prop_id)
    {
      GtkAdjustment *adjustment;

    case PROP_ADJUSTMENT:
      adjustment = GTK_ADJUSTMENT (g_value_get_object (value));
      if (!adjustment)
	adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      gtk_spin_button_set_adjustment (spin_button, adjustment);
      break;
    case PROP_CLIMB_RATE:
      gtk_spin_button_configure (spin_button,
				 spin_button->adjustment,
				 g_value_get_double (value),
				 spin_button->digits);
      break;
    case PROP_DIGITS:
      gtk_spin_button_configure (spin_button,
				 spin_button->adjustment,
				 spin_button->climb_rate,
				 g_value_get_uint (value));
      break;
    case PROP_SNAP_TO_TICKS:
      gtk_spin_button_set_snap_to_ticks (spin_button, g_value_get_boolean (value));
      break;
    case PROP_NUMERIC:
      gtk_spin_button_set_numeric (spin_button, g_value_get_boolean (value));
      break;
    case PROP_WRAP:
      gtk_spin_button_set_wrap (spin_button, g_value_get_boolean (value));
      break;
    case PROP_UPDATE_POLICY:
      gtk_spin_button_set_update_policy (spin_button, g_value_get_enum (value));
      break;
    case PROP_VALUE:
      gtk_spin_button_set_value (spin_button, g_value_get_double (value));
      break;
    default:
      break;
    }
}

static void
gtk_spin_button_get_property (GObject      *object,
			      guint         prop_id,
			      GValue       *value,
			      GParamSpec   *pspec)
{
  GtkSpinButton *spin_button;

  spin_button = GTK_SPIN_BUTTON (object);
  
  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, G_OBJECT (spin_button->adjustment));
      break;
    case PROP_CLIMB_RATE:
      g_value_set_double (value, spin_button->climb_rate);
      break;
    case PROP_DIGITS:
      g_value_set_uint (value, spin_button->digits);
      break;
    case PROP_SNAP_TO_TICKS:
      g_value_set_boolean (value, spin_button->snap_to_ticks);
      break;
    case PROP_NUMERIC:
      g_value_set_boolean (value, spin_button->numeric);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, spin_button->wrap);
      break;
    case PROP_UPDATE_POLICY:
      g_value_set_enum (value, spin_button->update_policy);
      break;
     case PROP_VALUE:
       g_value_set_double (value, spin_button->adjustment->value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_spin_button_init (GtkSpinButton *spin_button)
{
  spin_button->adjustment = NULL;
  spin_button->panel = NULL;
  spin_button->timer = 0;
  spin_button->ev_time = 0;
  spin_button->climb_rate = 0.0;
  spin_button->timer_step = 0.0;
  spin_button->update_policy = GTK_UPDATE_ALWAYS;
  spin_button->in_child = 2;
  spin_button->click_child = 2;
  spin_button->button = 0;
  spin_button->need_timer = FALSE;
  spin_button->timer_calls = 0;
  spin_button->digits = 0;
  spin_button->numeric = FALSE;
  spin_button->wrap = FALSE;
  spin_button->snap_to_ticks = FALSE;
  gtk_spin_button_set_adjustment (spin_button,
	  (GtkAdjustment*) gtk_adjustment_new (0, 0, 0, 0, 0, 0));
}

static void
gtk_spin_button_finalize (GObject *object)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (object));

  gtk_object_unref (GTK_OBJECT (GTK_SPIN_BUTTON (object)->adjustment));
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_spin_button_map (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  if (GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_CLASS (parent_class)->map (widget);
      gdk_window_show (GTK_SPIN_BUTTON (widget)->panel);
    }
}

static void
gtk_spin_button_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  if (GTK_WIDGET_MAPPED (widget))
    {
      gdk_window_hide (GTK_SPIN_BUTTON (widget)->panel);
      GTK_WIDGET_CLASS (parent_class)->unmap (widget);
    }
}

static void
gtk_spin_button_realize (GtkWidget *widget)
{
  GtkSpinButton *spin_button;
  GdkWindowAttr attributes;
  gint attributes_mask;
  guint real_width;
  gint return_val;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));
  
  spin_button = GTK_SPIN_BUTTON (widget);

  real_width = widget->allocation.width;
  widget->allocation.width -= ARROW_SIZE + 2 * widget->style->xthickness;
  gtk_widget_set_events (widget, gtk_widget_get_events (widget) |
			 GDK_KEY_RELEASE_MASK);
  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  widget->allocation.width = real_width;
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK 
    | GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK 
    | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  attributes.x = (widget->allocation.x + widget->allocation.width - ARROW_SIZE -
		  2 * widget->style->xthickness);
  attributes.y = widget->allocation.y + (widget->allocation.height -
					 widget->requisition.height) / 2;
  attributes.width = ARROW_SIZE + 2 * widget->style->xthickness;
  attributes.height = widget->requisition.height;
  
  spin_button->panel = gdk_window_new (gtk_widget_get_parent_window (widget), 
				       &attributes, attributes_mask);
  gdk_window_set_user_data (spin_button->panel, widget);

  gtk_style_set_background (widget->style, spin_button->panel, GTK_STATE_NORMAL);

  return_val = FALSE;
  gtk_signal_emit (GTK_OBJECT (spin_button), spinbutton_signals[OUTPUT],
		   &return_val);
  if (return_val == FALSE)
    gtk_spin_button_default_output (spin_button);
}

static void
gtk_spin_button_unrealize (GtkWidget *widget)
{
  GtkSpinButton *spin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  spin = GTK_SPIN_BUTTON (widget);

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);

  if (spin->panel)
    {
      gdk_window_set_user_data (spin->panel, NULL);
      gdk_window_destroy (spin->panel);
      spin->panel = NULL;
    }
}

static int
compute_double_length (double val, int digits)
{
  int a;
  int extra;

  a = 1;
  if (fabs (val) > 1.0)
    a = floor (log10 (fabs (val))) + 1;  

  extra = 0;
  
  /* The dot: */
  if (digits > 0)
    extra++;

  /* The sign: */
  if (val < 0)
    extra++;

  return a + digits + extra;
}

static void
gtk_spin_button_size_request (GtkWidget      *widget,
			      GtkRequisition *requisition)
{
  GtkEntry *entry;
  GtkSpinButton *spin_button;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (requisition != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

  entry = GTK_ENTRY (widget);
  spin_button = GTK_SPIN_BUTTON (widget);
  
  GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

  if (entry->width_chars < 0)
    {
      PangoContext *context;
      PangoFontMetrics metrics;
      gint width;
      gint w;
      int string_len;

      context = gtk_widget_get_pango_context (widget);
      pango_context_get_metrics (context,
				 widget->style->font_desc,
				 pango_context_get_language (context),
				 &metrics);
      
      /* Get max of MIN_SPIN_BUTTON_WIDTH, size of upper, size of lower */
      
      width = MIN_SPIN_BUTTON_WIDTH;

      string_len = compute_double_length (spin_button->adjustment->upper,
                                          spin_button->digits);
      w = MIN (string_len, 10) * PANGO_PIXELS (metrics.approximate_digit_width);
      width = MAX (width, w);
      string_len = compute_double_length (spin_button->adjustment->lower,
					  spin_button->adjustment->step_increment);
      w = MIN (string_len, 10) * PANGO_PIXELS (metrics.approximate_digit_width);
      width = MAX (width, w);
      
      requisition->width = width + ARROW_SIZE + 2 * widget->style->xthickness;
    }
  else
    requisition->width += ARROW_SIZE + 2 * widget->style->xthickness;
}

static void
gtk_spin_button_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));
  g_return_if_fail (allocation != NULL);

  child_allocation = *allocation;
  if (child_allocation.width > ARROW_SIZE + 2 * widget->style->xthickness)
    child_allocation.width -= ARROW_SIZE + 2 * widget->style->xthickness;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    child_allocation.x += ARROW_SIZE + 2 * widget->style->xthickness;

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, &child_allocation);

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      child_allocation.width = ARROW_SIZE + 2 * widget->style->xthickness;
      child_allocation.height = widget->requisition.height;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	child_allocation.x = (allocation->x + allocation->width - ARROW_SIZE - 
			      2 * widget->style->xthickness);
      else
	child_allocation.x = allocation->x;      

      child_allocation.y = allocation->y + (allocation->height - widget->requisition.height) / 2;

      gdk_window_move_resize (GTK_SPIN_BUTTON (widget)->panel, 
			      child_allocation.x,
			      child_allocation.y,
			      child_allocation.width,
			      child_allocation.height); 
    }
}

static gint
gtk_spin_button_expose (GtkWidget      *widget,
			GdkEventExpose *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      GtkShadowType shadow_type;

      /* FIXME this seems like really broken code -
       * why aren't we looking at event->window
       * and acting accordingly?
       */

      shadow_type = spin_button_get_shadow_type (spin);
      if (shadow_type != GTK_SHADOW_NONE)
	gtk_paint_box (widget->style, spin->panel,
		       GTK_STATE_NORMAL, shadow_type,
		       &event->area, widget, "spinbutton",
		       0, 0, 
		       ARROW_SIZE + 2 * widget->style->xthickness,
		       widget->requisition.height); 
      else
	 {
	    gdk_window_set_back_pixmap (spin->panel, NULL, TRUE);
	    gdk_window_clear_area (spin->panel,
                                   event->area.x, event->area.y,
                                   event->area.width, event->area.height);
	 }
       gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
       gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);

       GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
    }

  return FALSE;
}

static void
gtk_spin_button_draw_arrow (GtkSpinButton *spin_button, 
			    guint          arrow)
{
  GtkShadowType spin_shadow_type;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GtkWidget *widget;
  gint x;
  gint y;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  
  widget = GTK_WIDGET (spin_button);
  spin_shadow_type = spin_button_get_shadow_type (spin_button);

  if (GTK_WIDGET_DRAWABLE (spin_button))
    {
      if (!spin_button->wrap &&
	  (((arrow == GTK_ARROW_UP &&
	  (spin_button->adjustment->upper - spin_button->adjustment->value
	   <= EPSILON))) ||
	  ((arrow == GTK_ARROW_DOWN &&
	  (spin_button->adjustment->value - spin_button->adjustment->lower
	   <= EPSILON)))))
	{
	  shadow_type = GTK_SHADOW_ETCHED_IN;
	  state_type = GTK_STATE_NORMAL;
	}
      else
	{
	  if (spin_button->in_child == arrow)
	    {
	      if (spin_button->click_child == arrow)
		state_type = GTK_STATE_ACTIVE;
	      else
		state_type = GTK_STATE_PRELIGHT;
	    }
	  else
	    state_type = GTK_STATE_NORMAL;
	  
	  if (spin_button->click_child == arrow)
	    shadow_type = GTK_SHADOW_IN;
	  else
	    shadow_type = GTK_SHADOW_OUT;
	}
      if (arrow == GTK_ARROW_UP)
	{
	  if (spin_shadow_type != GTK_SHADOW_NONE)
	    {
	      x = widget->style->xthickness;
	      y = widget->style->ythickness;
	    }
	  else
	    {
	      x = widget->style->xthickness - 1;
	      y = widget->style->ythickness - 1;
	    }
	  gtk_paint_arrow (widget->style, spin_button->panel,
			   state_type, shadow_type, 
			   NULL, widget, "spinbutton",
			   arrow, TRUE, 
			   x, y, ARROW_SIZE, widget->requisition.height / 2 
			   - widget->style->ythickness);
	}
      else
	{
	  if (spin_shadow_type != GTK_SHADOW_NONE)
	    {
	      x = widget->style->xthickness;
	      y = widget->requisition.height / 2;
	    }
	  else
	    {
	      x = widget->style->xthickness - 1;
	      y = widget->requisition.height / 2 + 1;
	    }
	  gtk_paint_arrow (widget->style, spin_button->panel,
			   state_type, shadow_type, 
			   NULL, widget, "spinbutton",
			   arrow, TRUE, 
			   x, y, ARROW_SIZE, widget->requisition.height / 2 
			   - widget->style->ythickness);
	}
    }
}

static gint
gtk_spin_button_enter_notify (GtkWidget        *widget,
			      GdkEventCrossing *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (event->window == spin->panel)
    {
      gint x;
      gint y;

      gdk_window_get_pointer (spin->panel, &x, &y, NULL);

      if (y <= widget->requisition.height / 2)
	{
	  spin->in_child = GTK_ARROW_UP;
	  if (spin->click_child == 2) 
	    gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	}
      else
	{
	  spin->in_child = GTK_ARROW_DOWN;
	  if (spin->click_child == 2) 
	    gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	}
    }
  return FALSE;
}

static gint
gtk_spin_button_leave_notify (GtkWidget        *widget,
			      GdkEventCrossing *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (event->window == spin->panel && spin->click_child == 2)
    {
      if (spin->in_child == GTK_ARROW_UP) 
	{
	  spin->in_child = 2;
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	}
      else
	{
	  spin->in_child = 2;
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	}
    }
  return FALSE;
}

static gint
gtk_spin_button_focus_out (GtkWidget     *widget,
			   GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_ENTRY (widget)->editable)
    gtk_spin_button_update (GTK_SPIN_BUTTON (widget));

  return GTK_WIDGET_CLASS (parent_class)->focus_out_event (widget, event);
}

static gint
gtk_spin_button_scroll (GtkWidget      *widget,
			GdkEventScroll *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (event->direction == GDK_SCROLL_UP)
    {
      if (!GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);
      gtk_spin_button_real_spin (spin, spin->adjustment->step_increment);
    }
  else if (event->direction == GDK_SCROLL_DOWN)
    {
      if (!GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);
      gtk_spin_button_real_spin (spin, -spin->adjustment->step_increment); 
    }
  else
    return FALSE;

  return TRUE;
}

static gint
gtk_spin_button_button_press (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (!spin->button)
    {
      if (event->window == spin->panel)
	{
	  if (!GTK_WIDGET_HAS_FOCUS (widget))
	    gtk_widget_grab_focus (widget);
	  gtk_grab_add (widget);
	  spin->button = event->button;
	  
	  if (GTK_ENTRY (widget)->editable)
	    gtk_spin_button_update (spin);
	  
	  if (event->y <= widget->requisition.height / 2)
	    {
	      spin->click_child = GTK_ARROW_UP;
	      if (event->button == 1)
		{
		 gtk_spin_button_real_spin (spin, 
					    spin->adjustment->step_increment);
		  if (!spin->timer)
		    {
		      spin->timer_step = spin->adjustment->step_increment;
		      spin->need_timer = TRUE;
		      spin->timer = gtk_timeout_add 
			(SPIN_BUTTON_INITIAL_TIMER_DELAY, 
			 (GtkFunction) gtk_spin_button_timer, (gpointer) spin);
		    }
		}
	      else if (event->button == 2)
		{
		 gtk_spin_button_real_spin (spin, 
					    spin->adjustment->page_increment);
		  if (!spin->timer) 
		    {
		      spin->timer_step = spin->adjustment->page_increment;
		      spin->need_timer = TRUE;
		      spin->timer = gtk_timeout_add 
			(SPIN_BUTTON_INITIAL_TIMER_DELAY, 
			 (GtkFunction) gtk_spin_button_timer, (gpointer) spin);
		    }
		}
	      gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	    }
	  else 
	    {
	      spin->click_child = GTK_ARROW_DOWN;
	      if (event->button == 1)
		{
		  gtk_spin_button_real_spin (spin,
					     -spin->adjustment->step_increment);
		  if (!spin->timer)
		    {
		      spin->timer_step = spin->adjustment->step_increment;
		      spin->need_timer = TRUE;
		      spin->timer = gtk_timeout_add 
			(SPIN_BUTTON_INITIAL_TIMER_DELAY, 
			 (GtkFunction) gtk_spin_button_timer, (gpointer) spin);
		    }
		}      
	      else if (event->button == 2)
		{
		  gtk_spin_button_real_spin (spin,
					     -spin->adjustment->page_increment);
		  if (!spin->timer) 
		    {
		      spin->timer_step = spin->adjustment->page_increment;
		      spin->need_timer = TRUE;
		      spin->timer = gtk_timeout_add 
			(SPIN_BUTTON_INITIAL_TIMER_DELAY, 
			 (GtkFunction) gtk_spin_button_timer, (gpointer) spin);
		    }
		}
	      gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	    }
	  return TRUE;
	}
      else
	return GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
    }
  return FALSE;
}

static gint
gtk_spin_button_button_release (GtkWidget      *widget,
				GdkEventButton *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);

  if (event->button == spin->button)
    {
      guint click_child;

      if (spin->timer)
	{
	  gtk_timeout_remove (spin->timer);
	  spin->timer = 0;
	  spin->timer_calls = 0;
	  spin->need_timer = FALSE;
	}

      if (event->button == 3)
	{
	  if (event->y >= 0 && event->x >= 0 && 
	      event->y <= widget->requisition.height &&
	      event->x <= ARROW_SIZE + 2 * widget->style->xthickness)
	    {
	      if (spin->click_child == GTK_ARROW_UP &&
		  event->y <= widget->requisition.height / 2)
		{
		  gdouble diff;

		  diff = spin->adjustment->upper - spin->adjustment->value;
		  if (diff > EPSILON)
		    gtk_spin_button_real_spin (spin, diff);
		}
	      else if (spin->click_child == GTK_ARROW_DOWN &&
		       event->y > widget->requisition.height / 2)
		{
		  gdouble diff;

		  diff = spin->adjustment->value - spin->adjustment->lower;
		  if (diff > EPSILON)
		    gtk_spin_button_real_spin (spin, -diff);
		}
	    }
	}		  
      gtk_grab_remove (widget);
      click_child = spin->click_child;
      spin->click_child = 2;
      spin->button = 0;
      gtk_spin_button_draw_arrow (spin, click_child);
      return TRUE;
    }
  else
    return GTK_WIDGET_CLASS (parent_class)->button_release_event (widget, event);

  return FALSE;
}

static gint
gtk_spin_button_motion_notify (GtkWidget      *widget,
			       GdkEventMotion *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  spin = GTK_SPIN_BUTTON (widget);
  
  if (spin->button)
    return FALSE;

  if (event->window == spin->panel)
    {
      gint y;

      y = event->y;
      if (event->is_hint)
	gdk_window_get_pointer (spin->panel, NULL, &y, NULL);

      if (y <= widget->requisition.height / 2 && 
	  spin->in_child == GTK_ARROW_DOWN)
	{
	  spin->in_child = GTK_ARROW_UP;
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	}
      else if (y > widget->requisition.height / 2 && 
	  spin->in_child == GTK_ARROW_UP)
	{
	  spin->in_child = GTK_ARROW_DOWN;
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_UP);
	  gtk_spin_button_draw_arrow (spin, GTK_ARROW_DOWN);
	}
      return FALSE;
    }
	  
  return GTK_WIDGET_CLASS (parent_class)->motion_notify_event (widget, event);
}

static gint
gtk_spin_button_timer (GtkSpinButton *spin_button)
{
  gboolean retval = FALSE;
  
  GDK_THREADS_ENTER ();

  if (spin_button->timer)
    {
      if (spin_button->click_child == GTK_ARROW_UP)
	gtk_spin_button_real_spin (spin_button,	spin_button->timer_step);
      else
	gtk_spin_button_real_spin (spin_button,	-spin_button->timer_step);

      if (spin_button->need_timer)
	{
	  spin_button->need_timer = FALSE;
	  spin_button->timer = gtk_timeout_add 
	    (SPIN_BUTTON_TIMER_DELAY, (GtkFunction) gtk_spin_button_timer, 
	     (gpointer) spin_button);
	}
      else 
	{
	  if (spin_button->climb_rate > 0.0 && spin_button->timer_step 
	      < spin_button->adjustment->page_increment)
	    {
	      if (spin_button->timer_calls < MAX_TIMER_CALLS)
		spin_button->timer_calls++;
	      else 
		{
		  spin_button->timer_calls = 0;
		  spin_button->timer_step += spin_button->climb_rate;
		}
	    }
	  retval = TRUE;
	}
    }

  GDK_THREADS_LEAVE ();

  return retval;
}

static void
gtk_spin_button_value_changed (GtkAdjustment *adjustment,
			       GtkSpinButton *spin_button)
{
  gint return_val;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  return_val = FALSE;
  gtk_signal_emit (GTK_OBJECT (spin_button), spinbutton_signals[OUTPUT],
		   &return_val);
  if (return_val == FALSE)
    gtk_spin_button_default_output (spin_button);

  gtk_signal_emit (GTK_OBJECT (spin_button), 
		   spinbutton_signals[VALUE_CHANGED]);

  gtk_spin_button_draw_arrow (spin_button, GTK_ARROW_UP);
  gtk_spin_button_draw_arrow (spin_button, GTK_ARROW_DOWN);
  
  g_object_notify (G_OBJECT (spin_button), "value");
}

static gint
gtk_spin_button_key_press (GtkWidget     *widget,
			   GdkEventKey   *event)
{
  GtkSpinButton *spin;
  gint key;
  gboolean key_repeat = FALSE;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  spin = GTK_SPIN_BUTTON (widget);
  key = event->keyval;

  key_repeat = (event->time == spin->ev_time);

  if (GTK_ENTRY (widget)->editable &&
      (key == GDK_Up || key == GDK_Down || 
       key == GDK_Page_Up || key == GDK_Page_Down))
    gtk_spin_button_update (spin);

  switch (key)
    {
    case GDK_Up:

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), 
					"key_press_event");
	  if (!key_repeat)
	    spin->timer_step = spin->adjustment->step_increment;

	 gtk_spin_button_real_spin (spin, spin->timer_step);

	  if (key_repeat)
	    {
	      if (spin->climb_rate > 0.0 && spin->timer_step
		  < spin->adjustment->page_increment)
		{
		  if (spin->timer_calls < MAX_TIMER_CALLS)
		    spin->timer_calls++;
		  else 
		    {
		      spin->timer_calls = 0;
		      spin->timer_step += spin->climb_rate;
		    }
		}
	    }
	  return TRUE;
	}
      return FALSE;

    case GDK_Down:

      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), 
					"key_press_event");
	  if (!key_repeat)
	    spin->timer_step = spin->adjustment->step_increment;

	 gtk_spin_button_real_spin (spin, -spin->timer_step);

	  if (key_repeat)
	    {
	      if (spin->climb_rate > 0.0 && spin->timer_step
		  < spin->adjustment->page_increment)
		{
		  if (spin->timer_calls < MAX_TIMER_CALLS)
		    spin->timer_calls++;
		  else 
		    {
		      spin->timer_calls = 0;
		      spin->timer_step += spin->climb_rate;
		    }
		}
	    }
	  return TRUE;
	}
      return FALSE;

    case GDK_Page_Up:

      if (event->state & GDK_CONTROL_MASK)
	{
	  gdouble diff = spin->adjustment->upper - spin->adjustment->value;
	  if (diff > EPSILON)
	    gtk_spin_button_real_spin (spin, diff);
	}
      else
	gtk_spin_button_real_spin (spin, spin->adjustment->page_increment);
      return TRUE;

    case GDK_Page_Down:

      if (event->state & GDK_CONTROL_MASK)
	{
	  gdouble diff = spin->adjustment->value - spin->adjustment->lower;
	  if (diff > EPSILON)
	    gtk_spin_button_real_spin (spin, -diff);
	}
      else
	gtk_spin_button_real_spin (spin, -spin->adjustment->page_increment);
      return TRUE;

    default:
      break;
    }

  return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
}

static gint
gtk_spin_button_key_release (GtkWidget   *widget,
			     GdkEventKey *event)
{
  GtkSpinButton *spin;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (widget), FALSE);
  
  spin = GTK_SPIN_BUTTON (widget);
  
  spin->ev_time = event->time;
  return TRUE;
}

static void
gtk_spin_button_snap (GtkSpinButton *spin_button,
		      gdouble        val)
{
  gdouble inc;
  gdouble tmp;
  
  inc = spin_button->adjustment->step_increment;
  tmp = (val - spin_button->adjustment->lower) / inc;
  if (tmp - floor (tmp) < ceil (tmp) - tmp)
    val = spin_button->adjustment->lower + floor (tmp) * inc;
  else
    val = spin_button->adjustment->lower + ceil (tmp) * inc;

  if (fabs (val - spin_button->adjustment->value) > EPSILON)
    gtk_adjustment_set_value (spin_button->adjustment, val);
  else
    {
      gint return_val = FALSE;
      gtk_signal_emit (GTK_OBJECT (spin_button), spinbutton_signals[OUTPUT],
		       &return_val);
      if (return_val == FALSE)
	gtk_spin_button_default_output (spin_button);
    }
}

static void
gtk_spin_button_activate (GtkEntry *entry)
{
  if (entry->editable)
    gtk_spin_button_update (GTK_SPIN_BUTTON (entry));
}

static void
gtk_spin_button_insert_text (GtkEntry    *entry,
			     const gchar *new_text,
			     gint         new_text_length,
			     gint        *position)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkSpinButton *spin = GTK_SPIN_BUTTON (editable);
 
  if (spin->numeric)
    {
      struct lconv *lc;
      gboolean sign;
      gint dotpos = -1;
      gint i;
      GdkWChar pos_sign;
      GdkWChar neg_sign;
      gint entry_length;

      entry_length = entry->text_length;

      lc = localeconv ();

      if (*(lc->negative_sign))
	neg_sign = *(lc->negative_sign);
      else 
	neg_sign = '-';

      if (*(lc->positive_sign))
	pos_sign = *(lc->positive_sign);
      else 
	pos_sign = '+';

      for (sign=0, i=0; i<entry_length; i++)
	if ((entry->text[i] == neg_sign) ||
	    (entry->text[i] == pos_sign))
	  {
	    sign = 1;
	    break;
	  }

      if (sign && !(*position))
	return;

      for (dotpos=-1, i=0; i<entry_length; i++)
	if (entry->text[i] == *(lc->decimal_point))
	  {
	    dotpos = i;
	    break;
	  }

      if (dotpos > -1 && *position > dotpos &&
	  (gint)spin->digits - entry_length
	    + dotpos - new_text_length + 1 < 0)
	return;

      for (i = 0; i < new_text_length; i++)
	{
	  if (new_text[i] == neg_sign || new_text[i] == pos_sign)
	    {
	      if (sign || (*position) || i)
		return;
	      sign = TRUE;
	    }
	  else if (new_text[i] == *(lc->decimal_point))
	    {
	      if (!spin->digits || dotpos > -1 || 
 		  (new_text_length - 1 - i + entry_length
		    - *position > (gint)spin->digits)) 
		return;
	      dotpos = *position + i;
	    }
	  else if (new_text[i] < 0x30 || new_text[i] > 0x39)
	    return;
	}
    }

  GTK_ENTRY_CLASS (parent_class)->insert_text (entry, new_text,
					       new_text_length, position);
}

static void
gtk_spin_button_real_spin (GtkSpinButton *spin_button,
			   gdouble        increment)
{
  GtkAdjustment *adj;
  gdouble new_value = 0.0;

  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  
  adj = spin_button->adjustment;

  new_value = adj->value + increment;

  if (increment > 0)
    {
      if (spin_button->wrap)
	{
	  if (fabs (adj->value - adj->upper) < EPSILON)
	    new_value = adj->lower;
	  else if (new_value > adj->upper)
	    new_value = adj->upper;
	}
      else
	new_value = MIN (new_value, adj->upper);
    }
  else if (increment < 0) 
    {
      if (spin_button->wrap)
	{
	  if (fabs (adj->value - adj->lower) < EPSILON)
	    new_value = adj->upper;
	  else if (new_value < adj->lower)
	    new_value = adj->lower;
	}
      else
	new_value = MAX (new_value, adj->lower);
    }

  if (fabs (new_value - adj->value) > EPSILON)
    gtk_adjustment_set_value (adj, new_value);
}

static gint
gtk_spin_button_default_input (GtkSpinButton *spin_button,
			       gdouble       *new_val)
{
  gchar *err = NULL;

  *new_val = strtod (gtk_entry_get_text (GTK_ENTRY (spin_button)), &err);
  if (*err)
    return GTK_INPUT_ERROR;
  else
    return FALSE;
}

static gint
gtk_spin_button_default_output (GtkSpinButton *spin_button)
{
  gchar *buf = g_strdup_printf ("%0.*f", spin_button->digits, spin_button->adjustment->value);

  if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin_button))))
    gtk_entry_set_text (GTK_ENTRY (spin_button), buf);
  g_free (buf);
  return FALSE;
}


/***********************************************************
 ***********************************************************
 ***                  Public interface                   ***
 ***********************************************************
 ***********************************************************/


void
gtk_spin_button_configure (GtkSpinButton  *spin_button,
			   GtkAdjustment  *adjustment,
			   gdouble         climb_rate,
			   guint           digits)
{
  g_return_if_fail (spin_button != NULL);
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (adjustment)
    gtk_spin_button_set_adjustment (spin_button, adjustment);
  else
    adjustment = spin_button->adjustment;

  if (spin_button->digits != digits) 
    {
      spin_button->digits = digits;
      g_object_notify (G_OBJECT (spin_button), "digits");
    }

  if (spin_button->climb_rate != climb_rate)
    {
      spin_button->climb_rate = climb_rate;
      g_object_notify (G_OBJECT (spin_button), "climb_rate");
    }

  gtk_adjustment_value_changed (adjustment);
}

GtkWidget *
gtk_spin_button_new (GtkAdjustment *adjustment,
		     gdouble        climb_rate,
		     guint          digits)
{
  GtkSpinButton *spin;

  if (adjustment)
    g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), NULL);

  spin = gtk_type_new (GTK_TYPE_SPIN_BUTTON);

  gtk_spin_button_configure (spin, adjustment, climb_rate, digits);

  return GTK_WIDGET (spin);
}

/**
 * gtk_spin_button_new_with_range:
 * @min: Minimum allowable value
 * @max: Maximum allowable value
 * @step: Increment added or subtracted by spinning the widget
 * 
 * This is a convenience constructor that allows creation of a numeric 
 * #GtkSpinButton without manually creating an adjustment. The value is 
 * initially set to the minimum value and a page increment of 10 * @step
 * is the default. The precision of the spin button is equivalent to the 
 * precision of @step.
 * 
 * Return value: the newly instantiated spin button
 **/
GtkWidget *
gtk_spin_button_new_with_range (gdouble min,
				gdouble max,
				gdouble step)
{
  GtkObject *adj;
  GtkSpinButton *spin;
  gint digits;

  g_return_val_if_fail (min < max, NULL);
  g_return_val_if_fail (step != 0.0, NULL);

  spin = gtk_type_new (GTK_TYPE_SPIN_BUTTON);

  adj = gtk_adjustment_new (min, min, max, step, 10 * step, step);

  if (fabs (step) >= 1.0 || step == 0.0)
    digits = 0;
  else {
    digits = abs ((gint) floor (log10 (fabs (step))));
    if (digits > MAX_DIGITS)
      digits = MAX_DIGITS;
  }

  gtk_spin_button_configure (spin, GTK_ADJUSTMENT (adj), step, digits);

  gtk_spin_button_set_numeric (spin, TRUE);

  return GTK_WIDGET (spin);
}

/* Callback used when the spin button's adjustment changes.  We need to redraw
 * the arrows when the adjustment's range changes, and reevaluate our size request.
 */
static void
adjustment_changed_cb (GtkAdjustment *adjustment, gpointer data)
{
  GtkSpinButton *spin_button;

  spin_button = GTK_SPIN_BUTTON (data);

  gtk_widget_queue_resize (GTK_WIDGET (spin_button));
}

/**
 * gtk_spin_button_set_adjustment:
 * @spin_button: a #GtkSpinButton
 * @adjustment: a #GtkAdjustment to replace the existing adjustment
 * 
 * Replaces the #GtkAdjustment associated with @spin_button.
 **/
void
gtk_spin_button_set_adjustment (GtkSpinButton *spin_button,
				GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (spin_button->adjustment != adjustment)
    {
      if (spin_button->adjustment)
        {
          gtk_signal_disconnect_by_data (GTK_OBJECT (spin_button->adjustment),
                                         (gpointer) spin_button);
          gtk_object_unref (GTK_OBJECT (spin_button->adjustment));
        }
      spin_button->adjustment = adjustment;
      if (adjustment)
        {
          gtk_object_ref (GTK_OBJECT (adjustment));
	  gtk_object_sink (GTK_OBJECT (adjustment));
          gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			      (GtkSignalFunc) gtk_spin_button_value_changed,
			      (gpointer) spin_button);
	  gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
			      (GtkSignalFunc) adjustment_changed_cb,
			      (gpointer) spin_button);
        }

      gtk_widget_queue_resize (GTK_WIDGET (spin_button));
    }

  g_object_notify (G_OBJECT (spin_button), "adjustment");
}

/**
 * gtk_spin_button_get_adjustment:
 * @spin_button: 
 * 
 * Get the adjustment associated with a #GtkSpinButton
 * 
 * Return value: the #GtkAdjustment of @spin_button
 **/
GtkAdjustment *
gtk_spin_button_get_adjustment (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), NULL);

  return spin_button->adjustment;
}

/**
 * gtk_spin_button_set_digits:
 * @spin_button: a #GtkSpinButton
 * @digits: the number of digits to be displayed for the spin button's value
 * 
 * Set the precision to be displayed by @spin_button. Up to 20 digit precision
 * is allowed.
 **/
void
gtk_spin_button_set_digits (GtkSpinButton *spin_button,
			    guint          digits)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (spin_button->digits != digits)
    {
      spin_button->digits = digits;
      gtk_spin_button_value_changed (spin_button->adjustment, spin_button);
      g_object_notify (G_OBJECT (spin_button), "digits");
      
      /* since lower/upper may have changed */
      gtk_widget_queue_resize (GTK_WIDGET (spin_button));
    }
}

/**
 * gtk_spin_button_get_digits:
 * @spin_button: a #GtkSpinButton
 *
 * Fetches the precision of @spin_button. See gtk_spin_button_set_digits().
 *
 * Returns: the current precision
 **/
guint
gtk_spin_button_get_digits (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0);

  return spin_button->digits;
}

/**
 * gtk_spin_button_set_increments:
 * @spin_button: a #GtkSpinButton
 * @step: increment applied for a button 1 press.
 * @page: increment applied for a button 2 press.
 * 
 * Sets the step and page increments for spin_button.  This affects how 
 * quickly the value changes when the spin button's arrows are activated.
 **/
void
gtk_spin_button_set_increments (GtkSpinButton *spin_button,
				gdouble        step,
				gdouble        page)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  spin_button->adjustment->step_increment = step;
  spin_button->adjustment->page_increment = page;
}

/**
 * gtk_spin_button_get_increments:
 * @psin_button: a #GtkSpinButton
 * @step: location to store step increment, or %NULL
 * @page: location to store page increment, or %NULL
 *
 * Gets the current step and page the increments used by @spin_button. See
 * gtk_spin_button_set_increments().
 **/
void
gtk_spin_button_get_increments (GtkSpinButton *spin_button,
				gdouble       *step,
				gdouble       *page)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (step)
    *step = spin_button->adjustment->step_increment;
  if (page)
    *page = spin_button->adjustment->page_increment;
}

/**
 * gtk_spin_button_set_range:
 * @spin_button: a #GtkSpinButton
 * @min: minimum allowable value
 * @max: maximum allowable value
 * 
 * Sets the minimum and maximum allowable values for @spin_button
 **/
void
gtk_spin_button_set_range (GtkSpinButton *spin_button,
			   gdouble        min,
			   gdouble        max)
{
  gdouble value;
  
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  spin_button->adjustment->lower = min;
  spin_button->adjustment->upper = max;

  value = CLAMP (spin_button->adjustment->value,
                 spin_button->adjustment->lower,
                 (spin_button->adjustment->upper - spin_button->adjustment->page_size));

  if (value != spin_button->adjustment->value)
    gtk_spin_button_set_value (spin_button, value);

  gtk_adjustment_changed (spin_button->adjustment);
}

/**
 * gtk_spin_button_get_range:
 * @spin_button: a #GtkSpinButton
 * @min: location to store minimum allowed value, or %NULL
 * @max: location to store maximum allowed value, or %NULL
 *
 * Gets the range allowed for @spin_button. See
 * gtk_spin_button_set_range().
 **/
void
gtk_spin_button_get_range (GtkSpinButton *spin_button,
			   gdouble       *min,
			   gdouble       *max)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (min)
    *min = spin_button->adjustment->lower;
  if (max)
    *max = spin_button->adjustment->upper;
}

/**
 * gtk_spin_button_get_value:
 * @spin_button: a #GtkSpinButton
 * 
 * Get the value in the @spin_button.
 * 
 * Return value: the value of @spin_button
 **/
gdouble
gtk_spin_button_get_value (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0.0);

  return spin_button->adjustment->value;
}

/**
 * gtk_spin_button_get_value_as_int:
 * @spin_button: a #GtkSpinButton
 * 
 * Get the value @spin_button represented as an integer.
 * 
 * Return value: the value of @spin_button
 **/
gint
gtk_spin_button_get_value_as_int (GtkSpinButton *spin_button)
{
  gdouble val;

  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), 0);

  val = spin_button->adjustment->value;
  if (val - floor (val) < ceil (val) - val)
    return floor (val);
  else
    return ceil (val);
}

/**
 * gtk_spin_button_set_value:
 * @spin_button: a #GtkSpinButton
 * @value: the new value
 * 
 * Set the value of @spin_button.
 **/
void 
gtk_spin_button_set_value (GtkSpinButton *spin_button, 
			   gdouble        value)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (fabs (value - spin_button->adjustment->value) > EPSILON)
    gtk_adjustment_set_value (spin_button->adjustment, value);
  else
    {
      gint return_val = FALSE;
      gtk_signal_emit (GTK_OBJECT (spin_button), spinbutton_signals[OUTPUT],
		       &return_val);
      if (return_val == FALSE)
	gtk_spin_button_default_output (spin_button);
    }
}

/**
 * gtk_spin_button_set_update_policy:
 * @spin_button: a #GtkSpinButton 
 * @policy: a #GtkSpinButtonUpdatePolicy value
 * 
 * Sets the update behavior of a spin button. This determines whether the
 * spin button is always updated or only when a valid value is set.
 **/
void
gtk_spin_button_set_update_policy (GtkSpinButton             *spin_button,
				   GtkSpinButtonUpdatePolicy  policy)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  if (spin_button->update_policy != policy)
    {
      spin_button->update_policy = policy;
      g_object_notify (G_OBJECT (spin_button), "update_policy");
    }
}

/**
 * gtk_spin_button_get_update_policy:
 * @spin_button: a #GtkSpinButton
 *
 * Gets the update behavior of a spin button. See
 * gtk_spin_button_set_update_policy().
 *
 * Return value: the current update policy
 **/
GtkSpinButtonUpdatePolicy
gtk_spin_button_get_update_policy (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), GTK_UPDATE_ALWAYS);

  return spin_button->update_policy;
}

/**
 * gtk_spin_button_set_numeric:
 * @spin_button: a #GtkSpinButton 
 * @numeric: flag indicating if only numeric entry is allowed. 
 * 
 * Sets the flag that determines if non-numeric text can be typed into
 * the spin button.
 **/
void
gtk_spin_button_set_numeric (GtkSpinButton  *spin_button,
			     gboolean        numeric)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  spin_button->numeric = (numeric != 0);

  g_object_notify (G_OBJECT (spin_button), "numeric");
}

/**
 * gtk_spin_button_get_numeric:
 * @spin_button: a #GtkSpinButton
 *
 * Returns whether non-numeric text can be typed into the spin button.
 * See gtk_spin_button_set_numeric().
 *
 * Return value: %TRUE if only numeric text can be entered
 **/
gboolean
gtk_spin_button_get_numeric (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->numeric;
}

/**
 * gtk_spin_button_set_wrap:
 * @spin_button: a #GtkSpinButton 
 * @wrap: a flag indicating if wrapping behavior is performed.
 * 
 * Sets the flag that determines if a spin button value wraps around to the
 * opposite limit when the upper or lower limit of the range is exceeded.
 **/
void
gtk_spin_button_set_wrap (GtkSpinButton  *spin_button,
			  gboolean        wrap)
{
  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  spin_button->wrap = (wrap != 0);
  
  g_object_notify (G_OBJECT (spin_button), "wrap");
}

/**
 * gtk_spin_button_get_wrap:
 * @spin_button: a #GtkSpinButton
 *
 * Returns whether the spin button's value wraps around to the
 * opposite limit when the upper or lower limit of the range is
 * exceeded. See gtk_spin_button_set_wrap().
 *
 * Return value: %TRUE if the spin button wraps around
 **/
gboolean
gtk_spin_button_get_wrap (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->wrap;
}

/**
 * spin_button_get_shadow_type:
 * @spin_button: a #GtkSpinButton 
 * 
 * Convenience function to Get the shadow type from the underlying widget's
 * style.
 * 
 * Return value: the #GtkShadowType
 **/
static gint
spin_button_get_shadow_type (GtkSpinButton *spin_button)
{
  GtkShadowType rc_shadow_type;

  gtk_widget_style_get (GTK_WIDGET (spin_button), "shadow_type", &rc_shadow_type, NULL);

  return rc_shadow_type;
}

/**
 * gtk_spin_button_set_snap_to_ticks:
 * @spin_button: a #GtkSpinButton 
 * @snap_to_ticks: a flag indicating if invalid values should be corrected.
 * 
 * Sets the policy as to whether values are corrected to the nearest step 
 * increment when a spin button is activated after providing an invalid value.
 **/
void
gtk_spin_button_set_snap_to_ticks (GtkSpinButton *spin_button,
				   gboolean       snap_to_ticks)
{
  guint new_val;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  new_val = (snap_to_ticks != 0);

  if (new_val != spin_button->snap_to_ticks)
    {
      spin_button->snap_to_ticks = new_val;
      if (new_val && GTK_ENTRY (spin_button)->editable)
	gtk_spin_button_update (spin_button);
      
      g_object_notify (G_OBJECT (spin_button), "snap_to_ticks");
    }
}

/**
 * gtk_spin_button_get_snap_to_ticks:
 * @spin_button: a #GtkSpinButton
 *
 * Returns whether the values are corrected to the nearest step. See
 * gtk_spin_button_set_snap_to_ticks().
 *
 * Return value: %TRUE if values are snapped to the nearest step.
 **/
gboolean
gtk_spin_button_get_snap_to_ticks (GtkSpinButton *spin_button)
{
  g_return_val_if_fail (GTK_IS_SPIN_BUTTON (spin_button), FALSE);

  return spin_button->snap_to_ticks;
}

/**
 * gtk_spin_button_spin:
 * @spin_button: a #GtkSpinButton 
 * @direction: a #GtkSpinType indicating the direction to spin.
 * @increment: step increment to apply in the specified direction.
 * 
 * Increment or decrement a spin button's value in a specified direction
 * by a specified amount. 
 **/
void
gtk_spin_button_spin (GtkSpinButton *spin_button,
		      GtkSpinType    direction,
		      gdouble        increment)
{
  GtkAdjustment *adj;
  gdouble diff;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
  
  adj = spin_button->adjustment;

  /* for compatibility with the 1.0.x version of this function */
  if (increment != 0 && increment != adj->step_increment &&
      (direction == GTK_SPIN_STEP_FORWARD ||
       direction == GTK_SPIN_STEP_BACKWARD))
    {
      if (direction == GTK_SPIN_STEP_BACKWARD && increment > 0)
	increment = -increment;
      direction = GTK_SPIN_USER_DEFINED;
    }

  switch (direction)
    {
    case GTK_SPIN_STEP_FORWARD:

      gtk_spin_button_real_spin (spin_button, adj->step_increment);
      break;

    case GTK_SPIN_STEP_BACKWARD:

      gtk_spin_button_real_spin (spin_button, -adj->step_increment);
      break;

    case GTK_SPIN_PAGE_FORWARD:

      gtk_spin_button_real_spin (spin_button, adj->page_increment);
      break;

    case GTK_SPIN_PAGE_BACKWARD:

      gtk_spin_button_real_spin (spin_button, -adj->page_increment);
      break;

    case GTK_SPIN_HOME:

      diff = adj->value - adj->lower;
      if (diff > EPSILON)
	gtk_spin_button_real_spin (spin_button, -diff);
      break;

    case GTK_SPIN_END:

      diff = adj->upper - adj->value;
      if (diff > EPSILON)
	gtk_spin_button_real_spin (spin_button, diff);
      break;

    case GTK_SPIN_USER_DEFINED:

      if (increment != 0)
	gtk_spin_button_real_spin (spin_button, increment);
      break;

    default:
      break;
    }
}

/**
 * gtk_spin_button_update:
 * @spin_button: a #GtkSpinButton 
 * 
 * Manually force an update of the spin button.
 **/
void 
gtk_spin_button_update (GtkSpinButton *spin_button)
{
  gdouble val;
  gint error = 0;
  gint return_val;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));

  return_val = FALSE;
  gtk_signal_emit (GTK_OBJECT (spin_button), spinbutton_signals[INPUT],
		   &val, &return_val);
  if (return_val == FALSE)
    {
      return_val = gtk_spin_button_default_input (spin_button, &val);
      error = (return_val == GTK_INPUT_ERROR);
    }
  else if (return_val == GTK_INPUT_ERROR)
    error = 1;

  if (spin_button->update_policy == GTK_UPDATE_ALWAYS)
    {
      if (val < spin_button->adjustment->lower)
	val = spin_button->adjustment->lower;
      else if (val > spin_button->adjustment->upper)
	val = spin_button->adjustment->upper;
    }
  else if ((spin_button->update_policy == GTK_UPDATE_IF_VALID) && 
	   (error ||
	   val < spin_button->adjustment->lower ||
	   val > spin_button->adjustment->upper))
    {
      gtk_spin_button_value_changed (spin_button->adjustment, spin_button);
      return;
    }

  if (spin_button->snap_to_ticks)
    gtk_spin_button_snap (spin_button, val);
  else
    {
      if (fabs (val - spin_button->adjustment->value) > EPSILON)
	gtk_adjustment_set_value (spin_button->adjustment, val);
      else
	{
	  return_val = FALSE;
	  gtk_signal_emit (GTK_OBJECT (spin_button), spinbutton_signals[OUTPUT],
			   &return_val);
	  if (return_val == FALSE)
	    gtk_spin_button_default_output (spin_button);
	}
    }
}

