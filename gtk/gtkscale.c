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

#include <math.h>
#include "gtkintl.h"
#include "gtkscale.h"
#include "gtkmarshal.h"

enum {
  PROP_0,
  PROP_DIGITS,
  PROP_DRAW_VALUE,
  PROP_VALUE_POS
};

enum {
  FORMAT_VALUE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void gtk_scale_class_init   (GtkScaleClass *klass);
static void gtk_scale_init         (GtkScale      *scale);
static void gtk_scale_set_property (GObject       *object,
				    guint          prop_id,
				    const GValue  *value,
				    GParamSpec    *pspec);
static void gtk_scale_get_property (GObject       *object,
				    guint          prop_id,
				    GValue        *value,
				    GParamSpec    *pspec);
static void gtk_scale_map          (GtkWidget     *widget);
static void gtk_scale_unmap        (GtkWidget     *widget);

static void gtk_scale_draw_background (GtkRange      *range);


static GtkRangeClass *parent_class = NULL;


GtkType
gtk_scale_get_type (void)
{
  static GtkType scale_type = 0;

  if (!scale_type)
    {
      static const GtkTypeInfo scale_info =
      {
	"GtkScale",
	sizeof (GtkScale),
	sizeof (GtkScaleClass),
	(GtkClassInitFunc) gtk_scale_class_init,
	(GtkObjectInitFunc) gtk_scale_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      scale_type = gtk_type_unique (GTK_TYPE_RANGE, &scale_info);
    }

  return scale_type;
}

gboolean
single_string_accumulator (GSignalInvocationHint *ihint,
                           GValue                *return_accu,
                           const GValue          *handler_return,
                           gpointer               dummy)
{
  gboolean continue_emission;
  gchar *str;
  
  str = g_value_get_string (handler_return);
  g_value_set_string (return_accu, str);
  continue_emission = str == NULL;
  
  return continue_emission;
}

static void
gtk_scale_class_init (GtkScaleClass *class)
{
  GObjectClass   *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;

  gobject_class = G_OBJECT_CLASS (class);
  object_class = (GtkObjectClass*) class;
  range_class = (GtkRangeClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  parent_class = gtk_type_class (GTK_TYPE_RANGE);
  
  gobject_class->set_property = gtk_scale_set_property;
  gobject_class->get_property = gtk_scale_get_property;

  widget_class->map = gtk_scale_map;
  widget_class->unmap = gtk_scale_unmap;
  
  range_class->draw_background = gtk_scale_draw_background;

  signals[FORMAT_VALUE] =
    g_signal_newc ("format_value",
		   G_TYPE_FROM_CLASS (object_class),
		   G_SIGNAL_RUN_LAST,
		   G_STRUCT_OFFSET (GtkScaleClass, format_value),
		   single_string_accumulator, NULL,
		   gtk_marshal_STRING__DOUBLE,
		   G_TYPE_STRING, 1,
		   G_TYPE_DOUBLE);

  g_object_class_install_property (gobject_class,
                                   PROP_DIGITS,
                                   g_param_spec_int ("digits",
						     _("Digits"),
						     _("The number of decimal places that are displayed in the value"),
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_VALUE,
                                   g_param_spec_boolean ("draw_value",
							 _("Draw Value"),
							 _("Whether the current value is displayed as a string next to the slider"),
							 FALSE,
							 G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_VALUE_POS,
                                   g_param_spec_enum ("value_pos",
						      _("Value Position"),
						      _("The position in which the current value is displayed"),
						      GTK_TYPE_POSITION_TYPE,
						      GTK_POS_LEFT,
						      G_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("slider_length",
							     _("Slider Length"),
							     _("Length of scale's slider"),
							     0,
							     G_MAXINT,
							     31,
							     G_PARAM_READABLE));
  class->value_spacing = 2;
  class->draw_value = NULL;
}

static void
gtk_scale_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkScale *scale;

  scale = GTK_SCALE (object);

  switch (prop_id)
    {
    case PROP_DIGITS:
      gtk_scale_set_digits (scale, g_value_get_int (value));
      break;
    case PROP_DRAW_VALUE:
      gtk_scale_set_draw_value (scale, g_value_get_boolean (value));
      break;
    case PROP_VALUE_POS:
      gtk_scale_set_value_pos (scale, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_get_property (GObject      *object,
			guint         prop_id,
			GValue       *value,
			GParamSpec   *pspec)
{
  GtkScale *scale;

  scale = GTK_SCALE (object);

  switch (prop_id)
    {
    case PROP_DIGITS:
      g_value_set_int (value, GTK_RANGE (scale)->digits);
      break;
    case PROP_DRAW_VALUE:
      g_value_set_boolean (value, scale->draw_value);
      break;
    case PROP_VALUE_POS:
      g_value_set_enum (value, scale->value_pos);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_init (GtkScale *scale)
{
  GTK_WIDGET_SET_FLAGS (scale, GTK_CAN_FOCUS);
  GTK_WIDGET_SET_FLAGS (scale, GTK_NO_WINDOW);
  GTK_RANGE (scale)->digits = 1;
  scale->draw_value = TRUE;
  scale->value_pos = GTK_POS_TOP;
}

static void
gtk_scale_map (GtkWidget *widget)
{
  GtkRange *range;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCALE (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  range = GTK_RANGE (widget);

  gdk_window_show (range->trough);
}

static void
gtk_scale_unmap (GtkWidget *widget)
{
  GtkRange *range;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCALE (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  range = GTK_RANGE (widget);

  if (GTK_WIDGET_NO_WINDOW (widget))
     gtk_widget_queue_clear (widget);

  gdk_window_hide (range->trough);
}

void
gtk_scale_set_digits (GtkScale *scale,
		      gint      digits)
{
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_SCALE (scale));

  digits = CLAMP (digits, -1, 16);

  if (GTK_RANGE (scale)->digits != digits)
    {
      GTK_RANGE (scale)->digits = digits;

      gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify (G_OBJECT (scale), "digits");
    }
}

void
gtk_scale_set_draw_value (GtkScale *scale,
			  gboolean  draw_value)
{
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_SCALE (scale));

  draw_value = draw_value != FALSE;

  if (scale->draw_value != draw_value)
    {
      scale->draw_value = draw_value;

      gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify (G_OBJECT (scale), "draw_value");
    }
}

void
gtk_scale_set_value_pos (GtkScale        *scale,
			 GtkPositionType  pos)
{
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_SCALE (scale));

  if (scale->value_pos != pos)
    {
      scale->value_pos = pos;

      if (GTK_WIDGET_VISIBLE (scale) && GTK_WIDGET_MAPPED (scale))
	gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify (G_OBJECT (scale), "value_pos");
    }
}

void
gtk_scale_get_value_size (GtkScale *scale,
			  gint     *width,
			  gint     *height)
{
  GtkRange *range;

  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_SCALE (scale));

  if (scale->draw_value)
    {
      PangoLayout *layout;
      PangoRectangle logical_rect;
      gchar *txt;
      
      range = GTK_RANGE (scale);

      layout = gtk_widget_create_pango_layout (GTK_WIDGET (scale), NULL);

      txt = _gtk_scale_format_value (scale, range->adjustment->lower);
      pango_layout_set_text (layout, txt, -1);
      g_free (txt);
      
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      if (width)
	*width = logical_rect.width;
      if (height)
	*height = logical_rect.height;

      txt = _gtk_scale_format_value (scale, range->adjustment->upper);
      pango_layout_set_text (layout, txt, -1);
      g_free (txt);
      
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      if (width)
	*width = MAX (*width, logical_rect.width);
      if (height)
	*height = MAX (*height, logical_rect.height);

      g_object_unref (G_OBJECT (layout));
    }
  else
    {
      if (width)
	*width = 0;
      if (height)
	*height = 0;
    }

}

gint
gtk_scale_get_value_width (GtkScale *scale)
{
  gint width;

  g_return_val_if_fail (scale != NULL, 0);
  g_return_val_if_fail (GTK_IS_SCALE (scale), 0);
  
  gtk_scale_get_value_size (scale, &width, NULL);

  return width;
}

void
gtk_scale_draw_value (GtkScale *scale)
{
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_SCALE (scale));

  if (GTK_SCALE_GET_CLASS (scale)->draw_value)
    GTK_SCALE_GET_CLASS (scale)->draw_value (scale);
}


static void
gtk_scale_draw_background (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_SCALE (range));

  gtk_scale_draw_value (GTK_SCALE (range));
}

/**
 * _gtk_scale_format_value:
 * @scale: a #GtkScale
 * @value: adjustment value
 * 
 * Emits "format_value" signal to format the value, if no user
 * signal handlers, falls back to a default format.
 * 
 * Return value: formatted value
 **/
gchar*
_gtk_scale_format_value (GtkScale *scale,
                         gdouble   value)
{
  gchar *fmt = NULL;

  g_signal_emit (G_OBJECT (scale),
                 signals[FORMAT_VALUE],
                 0,
                 value,
                 &fmt);

  if (fmt)
    return fmt;
  else
    return g_strdup_printf ("%0.*f", GTK_RANGE (scale)->digits,
                            value);
}
