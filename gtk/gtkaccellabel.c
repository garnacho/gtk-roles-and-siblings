/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAccelLabel: GtkLabel with accelerator monitoring facilities.
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <string.h>
#include <ctype.h>
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkaccellabel.h"
#include "gtkintl.h"

enum {
  PROP_0,
  PROP_ACCEL_OBJECT
};

static void     gtk_accel_label_class_init   (GtkAccelLabelClass *klass);
static void     gtk_accel_label_init         (GtkAccelLabel      *accel_label);
static void     gtk_accel_label_set_property (GObject            *object,
					      guint               prop_id,
					      const GValue       *value,
					      GParamSpec         *pspec);
static void     gtk_accel_label_get_property (GObject            *object,
					      guint               prop_id,
					      GValue             *value,
					      GParamSpec         *pspec);
static void     gtk_accel_label_destroy      (GtkObject          *object);
static void     gtk_accel_label_finalize     (GObject            *object);
static void     gtk_accel_label_size_request (GtkWidget          *widget,
					      GtkRequisition     *requisition);
static gboolean gtk_accel_label_expose_event (GtkWidget          *widget,
					      GdkEventExpose     *event);
static gboolean gtk_accel_label_refetch_idle (GtkAccelLabel      *accel_label);

static GtkAccelLabelClass *accel_label_class = NULL;
static GtkLabelClass *parent_class = NULL;


GtkType
gtk_accel_label_get_type (void)
{
  static GtkType accel_label_type = 0;
  
  if (!accel_label_type)
    {
      static const GtkTypeInfo accel_label_info =
      {
	"GtkAccelLabel",
	sizeof (GtkAccelLabel),
	sizeof (GtkAccelLabelClass),
	(GtkClassInitFunc) gtk_accel_label_class_init,
	(GtkObjectInitFunc) gtk_accel_label_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };
      
      accel_label_type = gtk_type_unique (GTK_TYPE_LABEL, &accel_label_info);
    }
  
  return accel_label_type;
}

static void
gtk_accel_label_class_init (GtkAccelLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkMiscClass *misc_class;
  GtkLabelClass *label_class;
  
  accel_label_class = class;
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  misc_class = (GtkMiscClass*) class;
  label_class = (GtkLabelClass*) class;
  
  parent_class = gtk_type_class (GTK_TYPE_LABEL);
  
  gobject_class->finalize = gtk_accel_label_finalize;
  gobject_class->set_property = gtk_accel_label_set_property;
  gobject_class->get_property = gtk_accel_label_get_property;
  
  object_class->destroy = gtk_accel_label_destroy;
  
  g_object_class_install_property (G_OBJECT_CLASS(object_class),
                                   PROP_ACCEL_OBJECT,
                                   g_param_spec_object ("accel_object",
                                                        _("Accelerator object"),
                                                        _("The object monitored by this accelerator label"),
                                                        G_TYPE_OBJECT,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));

   
  widget_class->size_request = gtk_accel_label_size_request;
  widget_class->expose_event = gtk_accel_label_expose_event;

  class->signal_quote1 = g_strdup ("<:");
  class->signal_quote2 = g_strdup (":>");
  class->mod_name_shift = g_strdup ("Shift");
  class->mod_name_control = g_strdup ("Ctrl");
  class->mod_name_alt = g_strdup ("Alt");
  class->mod_separator = g_strdup ("+");
  class->accel_seperator = g_strdup (" / ");
  class->latin1_to_char = TRUE;
}

static void
gtk_accel_label_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkAccelLabel  *accel_label;

  accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCEL_OBJECT:
      gtk_accel_label_set_accel_object (accel_label, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void gtk_accel_label_get_property (GObject    *object,
					  guint       prop_id,
					  GValue     *value,
					  GParamSpec *pspec)
{
  GtkAccelLabel  *accel_label;

  accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCEL_OBJECT:
       g_value_set_object (value, accel_label->accel_object);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accel_label_init (GtkAccelLabel *accel_label)
{
  accel_label->queue_id = 0;
  accel_label->accel_padding = 3;
  accel_label->accel_object = NULL;
  accel_label->accel_string = NULL;
  
  gtk_accel_label_refetch (accel_label);
}

GtkWidget*
gtk_accel_label_new (const gchar *string)
{
  GtkAccelLabel *accel_label;
  
  g_return_val_if_fail (string != NULL, NULL);
  
  accel_label = gtk_type_new (GTK_TYPE_ACCEL_LABEL);
  
  gtk_label_set_text (GTK_LABEL (accel_label), string);
  
  return GTK_WIDGET (accel_label);
}

static void
gtk_accel_label_destroy (GtkObject *object)
{
  GtkAccelLabel *accel_label;
  
  g_return_if_fail (GTK_IS_ACCEL_LABEL (object));
  
  accel_label = GTK_ACCEL_LABEL (object);

  gtk_accel_label_set_accel_object (accel_label, NULL);
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_accel_label_finalize (GObject *object)
{
  GtkAccelLabel *accel_label;
  
  g_return_if_fail (GTK_IS_ACCEL_LABEL (object));
  
  accel_label = GTK_ACCEL_LABEL (object);
  
  g_free (accel_label->accel_string);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtk_accel_label_get_accel_object:
 * @accel_label: a #GtkAccelLabel
 *
 * Fetches the widget monitored by this accelerator label. See
 * gtk_accel_label_set_accel_object().
 *
 * Return value: the object monitored by the accelerator label,
 *               or %NULL.
 **/
GObject *
gtk_accel_label_get_accel_object (GtkAccelLabel *accel_label)
{
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), NULL);

  return accel_label->accel_object;
}

guint
gtk_accel_label_get_accel_width (GtkAccelLabel *accel_label)
{
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), 0);
  
  return (accel_label->accel_string_width +
	  (accel_label->accel_string_width ? accel_label->accel_padding : 0));
}

static void
gtk_accel_label_size_request (GtkWidget	     *widget,
			      GtkRequisition *requisition)
{
  GtkAccelLabel *accel_label;
  PangoLayout *layout;
  gint width;
  
  g_return_if_fail (GTK_IS_ACCEL_LABEL (widget));
  g_return_if_fail (requisition != NULL);
  
  accel_label = GTK_ACCEL_LABEL (widget);
  
  if (GTK_WIDGET_CLASS (parent_class)->size_request)
    GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

  layout = gtk_widget_create_pango_layout (widget, accel_label->accel_string);
  pango_layout_get_pixel_size (layout, &width, NULL);
  accel_label->accel_string_width = width;
  
  g_object_unref (G_OBJECT (layout));
}

static gboolean 
gtk_accel_label_expose_event (GtkWidget      *widget,
			      GdkEventExpose *event)
{
  GtkMisc *misc;
  GtkAccelLabel *accel_label;
  PangoLayout *layout;
  
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  accel_label = GTK_ACCEL_LABEL (widget);
  misc = GTK_MISC (accel_label);
	  
  if (GTK_WIDGET_DRAWABLE (accel_label))
    {
      guint ac_width;
      
      ac_width = gtk_accel_label_get_accel_width (accel_label);
      
      if (widget->allocation.width >= widget->requisition.width + ac_width)
	{
	  guint x;
	  guint y;
	  
	  widget->allocation.width -= ac_width;
	  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
	    GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
	  widget->allocation.width += ac_width;
	  
	  x = widget->allocation.x + widget->allocation.width - misc->xpad - ac_width;
	  
	  y = (widget->allocation.y * (1.0 - misc->yalign) +
	       (widget->allocation.y + widget->allocation.height -
		(widget->requisition.height - misc->ypad * 2)) *
	       misc->yalign) + 1.5;
	  
	  layout = gtk_widget_create_pango_layout (widget, accel_label->accel_string);

          gtk_paint_layout (widget->style,
                            widget->window,
                            GTK_WIDGET_STATE (widget),
			    FALSE,
                            &event->area,
                            widget,
                            "accellabel",
                            x, y,
                            layout);                            

          g_object_unref (G_OBJECT (layout));
	}
      else
	{
	  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
	    GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
	}
    }
  
  return TRUE;
}

static void
gtk_accel_label_queue_refetch (GtkAccelLabel *accel_label)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));

  if (accel_label->queue_id == 0)
    accel_label->queue_id = gtk_idle_add_priority (G_PRIORITY_HIGH_IDLE,
						   (GtkFunction) gtk_accel_label_refetch_idle,
						   accel_label);
}

void
gtk_accel_label_set_accel_object (GtkAccelLabel *accel_label,
				  GObject	*accel_object)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  g_return_if_fail (accel_object == NULL || G_IS_OBJECT (accel_object));
    
  if (accel_object != accel_label->accel_object)
    {
      if (accel_label->accel_object)
	{
	  g_signal_handlers_disconnect_by_func (accel_label->accel_object,
						G_CALLBACK (gtk_accel_label_queue_refetch),
						accel_object);
	  g_object_unref (accel_label->accel_object);
	}
      if (accel_label->queue_id)
	{
	  gtk_idle_remove (accel_label->queue_id);
	  accel_label->queue_id = 0;
	}
      accel_label->accel_object = accel_object;
      if (accel_label->accel_object)
	{
	  g_object_ref (accel_label->accel_object);
	  g_signal_connect_object (accel_label->accel_object,
				   "add-accelerator",
				   G_CALLBACK (gtk_accel_label_queue_refetch),
				   accel_label,
				   G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	  g_signal_connect_object (accel_label->accel_object,
				   "remove-accelerator",
				   G_CALLBACK (gtk_accel_label_queue_refetch),
				   accel_label,
				   G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	}
       g_object_notify (G_OBJECT (accel_label), "accel_object");
    }
}

static gboolean
gtk_accel_label_refetch_idle (GtkAccelLabel *accel_label)
{
  gboolean retval;

  GDK_THREADS_ENTER ();
  retval = gtk_accel_label_refetch (accel_label);
  GDK_THREADS_LEAVE ();

  return retval;
}

gboolean
gtk_accel_label_refetch (GtkAccelLabel *accel_label)
{
  GtkAccelLabelClass *class;

  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), FALSE);

  class = GTK_ACCEL_LABEL_GET_CLASS (accel_label);
  
  g_free (accel_label->accel_string);
  accel_label->accel_string = NULL;
  
  if (accel_label->accel_object)
    {
      GtkAccelEntry *entry = NULL;
      GSList *slist;
      
      slist = gtk_accel_group_entries_from_object (accel_label->accel_object);
      for (; slist; slist = slist->next)
	{
	  entry = slist->data;
	  if (entry->accel_flags & GTK_ACCEL_VISIBLE)
	    {
	      GString *gstring;
	      gboolean had_mod;
	      
	      gstring = g_string_new (accel_label->accel_string);
	      if (gstring->len)
		g_string_append (gstring, class->accel_seperator);
	      else
		g_string_append (gstring, "   ");

	      if (entry->accel_flags & GTK_ACCEL_SIGNAL_VISIBLE)
		{
		  g_string_append (gstring, class->signal_quote1);
		  g_string_append (gstring, gtk_signal_name (entry->signal_id));
		  g_string_append (gstring, class->signal_quote2);
		}
	      
	      had_mod = FALSE;
	      if (entry->accelerator_mods & GDK_SHIFT_MASK)
		{
		  g_string_append (gstring, class->mod_name_shift);
		  had_mod = TRUE;
		}
	      if (entry->accelerator_mods & GDK_CONTROL_MASK)
		{
		  if (had_mod)
		    g_string_append (gstring, class->mod_separator);
		  g_string_append (gstring, class->mod_name_control);
		  had_mod = TRUE;
		}
	      if (entry->accelerator_mods & GDK_MOD1_MASK)
		{
		  if (had_mod)
		    g_string_append (gstring, class->mod_separator);
		  g_string_append (gstring, class->mod_name_alt);
		  had_mod = TRUE;
		}
	      
	      if (had_mod)
		g_string_append (gstring, class->mod_separator);
	      if (entry->accelerator_key < 0x80 ||
		  (entry->accelerator_key > 0x80 &&
		   entry->accelerator_key <= 0xff &&
		   class->latin1_to_char))
		{
		  switch (entry->accelerator_key)
		    {
		    case ' ':
		      g_string_append (gstring, "Space");
		      break;
		    case '\\':
		      g_string_append (gstring, "Backslash");
		      break;
		    default:
		      g_string_append_c (gstring, toupper (entry->accelerator_key));
		      break;
		    }
		}
	      else
		{
		  gchar *tmp;
		  
		  tmp = gtk_accelerator_name (entry->accelerator_key, 0);
		  if (tmp[0] != 0 && tmp[1] == 0)
		    tmp[0] = toupper (tmp[0]);
		  g_string_append (gstring, tmp);
		  g_free (tmp);
		}

	      g_free (accel_label->accel_string);
	      accel_label->accel_string = gstring->str;
	      g_string_free (gstring, FALSE);
	    }
	}
    }

  if (!accel_label->accel_string)
    accel_label->accel_string = g_strdup ("");

  if (accel_label->queue_id)
    {
      gtk_idle_remove (accel_label->queue_id);
      accel_label->queue_id = 0;
    }

  gtk_widget_queue_resize (GTK_WIDGET (accel_label));

  return FALSE;
}
