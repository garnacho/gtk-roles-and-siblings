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
#include "gtkadjustment.h"
#include "gtksignal.h"


enum {
  CHANGED,
  VALUE_CHANGED,
  LAST_SIGNAL
};


static void gtk_adjustment_class_init (GtkAdjustmentClass *klass);
static void gtk_adjustment_init       (GtkAdjustment      *adjustment);


static gint adjustment_signals[LAST_SIGNAL] = { 0 };


guint
gtk_adjustment_get_type ()
{
  static guint adjustment_type = 0;

  if (!adjustment_type)
    {
      GtkTypeInfo adjustment_info =
      {
	"GtkAdjustment",
	sizeof (GtkAdjustment),
	sizeof (GtkAdjustmentClass),
	(GtkClassInitFunc) gtk_adjustment_class_init,
	(GtkObjectInitFunc) gtk_adjustment_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL,
      };

      adjustment_type = gtk_type_unique (gtk_data_get_type (), &adjustment_info);
    }

  return adjustment_type;
}

static void
gtk_adjustment_class_init (GtkAdjustmentClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  adjustment_signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_FIRST | GTK_RUN_NO_RECURSE,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkAdjustmentClass, changed),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  adjustment_signals[VALUE_CHANGED] =
    gtk_signal_new ("value_changed",
                    GTK_RUN_FIRST | GTK_RUN_NO_RECURSE,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkAdjustmentClass, value_changed),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, adjustment_signals, LAST_SIGNAL);

  class->changed = NULL;
  class->value_changed = NULL;
}

static void
gtk_adjustment_init (GtkAdjustment *adjustment)
{
  adjustment->value = 0.0;
  adjustment->lower = 0.0;
  adjustment->upper = 0.0;
  adjustment->step_increment = 0.0;
  adjustment->page_increment = 0.0;
  adjustment->page_size = 0.0;
}

GtkObject*
gtk_adjustment_new (gfloat value,
		    gfloat lower,
		    gfloat upper,
		    gfloat step_increment,
		    gfloat page_increment,
		    gfloat page_size)
{
  GtkAdjustment *adjustment;

  adjustment = gtk_type_new (gtk_adjustment_get_type ());

  adjustment->value = value;
  adjustment->lower = lower;
  adjustment->upper = upper;
  adjustment->step_increment = step_increment;
  adjustment->page_increment = page_increment;
  adjustment->page_size = page_size;

  return GTK_OBJECT (adjustment);
}

void
gtk_adjustment_set_value (GtkAdjustment        *adjustment,
			  gfloat                value)
{
  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  adjustment->value = CLAMP (value, adjustment->lower, adjustment->upper);

  gtk_signal_emit_by_name (GTK_OBJECT (adjustment), "value_changed");
}
