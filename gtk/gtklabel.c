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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <math.h>
#include <string.h>
#include "gtklabel.h"
#include "gdk/gdkkeysyms.h"
#include "gdk/gdki18n.h"
#include <pango/pango.h>


enum {
  ARG_0,
  ARG_LABEL,
  ARG_PATTERN,
  ARG_JUSTIFY,
  ARG_WRAP
};

static void gtk_label_class_init        (GtkLabelClass    *klass);
static void gtk_label_init              (GtkLabel         *label);
static void gtk_label_set_arg           (GtkObject        *object,
					 GtkArg           *arg,
					 guint             arg_id);
static void gtk_label_get_arg           (GtkObject        *object,
					 GtkArg           *arg,
					 guint             arg_id);
static void gtk_label_finalize          (GObject          *object);
static void gtk_label_size_request      (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void gtk_label_style_set         (GtkWidget        *widget,
					 GtkStyle         *previous_style);
static void gtk_label_direction_changed (GtkWidget        *widget,
					 GtkTextDirection  previous_dir);
static gint gtk_label_expose            (GtkWidget        *widget,
					 GdkEventExpose   *event);

static GtkMiscClass *parent_class = NULL;

GtkType
gtk_label_get_type (void)
{
  static GtkType label_type = 0;
  
  if (!label_type)
    {
      static const GTypeInfo label_info =
      {
	sizeof (GtkLabelClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_label_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkLabel),
	32,             /* n_preallocs */
	(GInstanceInitFunc) gtk_label_init,
      };

      label_type = g_type_register_static (GTK_TYPE_MISC, "GtkLabel", &label_info);
    }
  
  return label_type;
}

static void
gtk_label_class_init (GtkLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  parent_class = gtk_type_class (GTK_TYPE_MISC);
  
  gtk_object_add_arg_type ("GtkLabel::label", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_LABEL);
  gtk_object_add_arg_type ("GtkLabel::pattern", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_PATTERN);
  gtk_object_add_arg_type ("GtkLabel::justify", GTK_TYPE_JUSTIFICATION, GTK_ARG_READWRITE, ARG_JUSTIFY);
  gtk_object_add_arg_type ("GtkLabel::wrap", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_WRAP);
  
  gobject_class->finalize = gtk_label_finalize;

  object_class->set_arg = gtk_label_set_arg;
  object_class->get_arg = gtk_label_get_arg;
  
  widget_class->size_request = gtk_label_size_request;
  widget_class->style_set = gtk_label_style_set;
  widget_class->direction_changed = gtk_label_direction_changed;
  widget_class->expose_event = gtk_label_expose;
}

static void
gtk_label_set_arg (GtkObject	  *object,
		   GtkArg	  *arg,
		   guint	   arg_id)
{
  GtkLabel *label;
  
  label = GTK_LABEL (object);
  
  switch (arg_id)
    {
    case ARG_LABEL:
      gtk_label_set_text (label, GTK_VALUE_STRING (*arg));
      break;
    case ARG_PATTERN:
      gtk_label_set_pattern (label, GTK_VALUE_STRING (*arg));
      break;
    case ARG_JUSTIFY:
      gtk_label_set_justify (label, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_WRAP:
      gtk_label_set_line_wrap (label, GTK_VALUE_BOOL (*arg));
      break;	  
    default:
      break;
    }
}

static void
gtk_label_get_arg (GtkObject	  *object,
		   GtkArg	  *arg,
		   guint	   arg_id)
{
  GtkLabel *label;
  
  label = GTK_LABEL (object);
  
  switch (arg_id)
    {
    case ARG_LABEL:
      GTK_VALUE_STRING (*arg) = g_strdup (label->label);
      break;
    case ARG_PATTERN:
      GTK_VALUE_STRING (*arg) = g_strdup (label->pattern);
      break;
    case ARG_JUSTIFY:
      GTK_VALUE_ENUM (*arg) = label->jtype;
      break;
    case ARG_WRAP:
      GTK_VALUE_BOOL (*arg) = label->wrap;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_label_init (GtkLabel *label)
{
  GTK_WIDGET_SET_FLAGS (label, GTK_NO_WINDOW);
  
  label->label = NULL;
  label->pattern = NULL;

  label->jtype = GTK_JUSTIFY_CENTER;
  label->wrap = FALSE;

  label->layout = NULL;
  
  gtk_label_set_text (label, "");
}

GtkWidget*
gtk_label_new (const gchar *str)
{
  GtkLabel *label;
  
  label = gtk_type_new (GTK_TYPE_LABEL);

  if (str && *str)
    gtk_label_set_text (label, str);
  
  return GTK_WIDGET (label);
}

static inline void
gtk_label_set_text_internal (GtkLabel *label,
			     gchar    *str)
{
  g_free (label->label);

  label->label = str;
  if (label->layout)
    pango_layout_set_text (label->layout, str, -1);
  
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

void
gtk_label_set_text (GtkLabel    *label,
		    const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_text_internal (label, g_strdup (str ? str : ""));
}

/**
 * gtk_label_get_text:
 * @label: a #GtkLabel
 * 
 * Fetches the text from a label widget
 * 
 * Return value: the text in the label widget. This value must
 * be freed with g_free().
 **/
gchar *
gtk_label_get_text (GtkLabel *label)
{
  g_return_val_if_fail (label != NULL, NULL);
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return g_strdup (label->label);
}

void
gtk_label_set_pattern (GtkLabel	   *label,
		       const gchar *pattern)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  g_free (label->pattern);
  label->pattern = g_strdup (pattern);

  gtk_widget_queue_resize (GTK_WIDGET (label));
}

void
gtk_label_set_justify (GtkLabel        *label,
		       GtkJustification jtype)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (jtype >= GTK_JUSTIFY_LEFT && jtype <= GTK_JUSTIFY_FILL);
  
  if ((GtkJustification) label->jtype != jtype)
    {
      label->jtype = jtype;

      if (label->layout)
	{
	  /* No real need to be this drastic, but easier than duplicating the code */
          g_object_unref (G_OBJECT (label->layout));
	  label->layout = NULL;
	}
      
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

void
gtk_label_set_line_wrap (GtkLabel *label,
			 gboolean  wrap)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  wrap = wrap != FALSE;
  
  if (label->wrap != wrap)
    {
      label->wrap = wrap;

      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

void
gtk_label_get (GtkLabel *label,
	       gchar   **str)
{
  g_return_if_fail (label != NULL);
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);
  
  *str = label->label;
}

static void
gtk_label_finalize (GObject *object)
{
  GtkLabel *label;
  
  g_return_if_fail (GTK_IS_LABEL (object));
  
  label = GTK_LABEL (object);
  
  g_free (label->label);
  g_free (label->pattern);

  if (label->layout)
    g_object_unref (G_OBJECT (label->layout));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static PangoAttrList *
gtk_label_pattern_to_attrs (GtkLabel *label)
{
  PangoAttrList *attrs = pango_attr_list_new ();

  if (label->pattern)
    {
      const char *start;
      const char *p = label->label;
      const char *q = label->pattern;

      while (1)
	{
	  while (*p && *q && *q != '_')
	    {
	      p = g_utf8_next_char (p);
	      q++;
	    }
	  start = p;
	  while (*p && *q && *q == '_')
	    {
	      p = g_utf8_next_char (p);
	      q++;
	    }

	  if (p > start)
	    {
	      PangoAttribute *attr = pango_attr_underline_new (PANGO_UNDERLINE_LOW);
	      attr->start_index = start - label->label;
	      attr->end_index = p - label->label;

	      pango_attr_list_insert (attrs, attr);
	    }
	  else
	    break;
	}
    }

  return attrs;
}

static void
gtk_label_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkLabel *label;
  PangoRectangle logical_rect;
  
  g_return_if_fail (GTK_IS_LABEL (widget));
  g_return_if_fail (requisition != NULL);
  
  label = GTK_LABEL (widget);

  /*
   * There are a number of conditions which will necessitate re-filling
   * our text:
   *
   *     1. text changed.
   *     2. justification changed either from to to GTK_JUSTIFY_FILL.
   *     3. font changed.
   *
   * These have been detected elsewhere, and label->words will be zero,
   * if one of the above has occured.
   *
   * Additionally, though, if GTK_JUSTIFY_FILL, we need to re-fill if:
   *
   *     4. gtk_widget_set_usize has changed the requested width.
   *     5. gtk_misc_set_padding has changed xpad.
   *     6.  maybe others?...
   *
   * Too much of a pain to detect all these case, so always re-fill.  I
   * don't think it's really that slow.
   */

  requisition->width = label->misc.xpad;
  requisition->height = label->misc.ypad;

  if (!label->layout)
    {
      PangoAlignment align = PANGO_ALIGN_LEFT; /* Quiet gcc */
      PangoAttrList *attrs = gtk_label_pattern_to_attrs (label);

      label->layout = gtk_widget_create_pango_layout (widget);
      pango_layout_set_attributes (label->layout, attrs);
      pango_attr_list_unref (attrs);

      pango_layout_set_text (label->layout, label->label, -1);

      switch (label->jtype)
	{
	case GTK_JUSTIFY_LEFT:
	  align = PANGO_ALIGN_LEFT;
	  break;
	case GTK_JUSTIFY_RIGHT:
	  align = PANGO_ALIGN_RIGHT;
	  break;
	case GTK_JUSTIFY_CENTER:
	  align = PANGO_ALIGN_LEFT;
	  break;
	case GTK_JUSTIFY_FILL:
	  /* FIXME: This just doesn't work to do this */
	  align = PANGO_ALIGN_LEFT;
	  pango_layout_set_justify (label->layout, TRUE);
	  break;
	default:
	  g_assert_not_reached();
	}

      pango_layout_set_alignment (label->layout, align);
    }

  if (label->wrap)
    {
      GtkWidgetAuxInfo *aux_info;
      gint longest_paragraph;
      gint width, height;
      gint real_width;

      aux_info = gtk_object_get_data (GTK_OBJECT (widget), "gtk-aux-info");
      if (aux_info && aux_info->width > 0)
	{
	  pango_layout_set_width (label->layout, aux_info->width * PANGO_SCALE);
	  pango_layout_get_extents (label->layout, NULL, &logical_rect);

	  requisition->width += aux_info->width;
	  requisition->height += logical_rect.height / PANGO_SCALE;
	}
      else
	{
	  pango_layout_set_width (label->layout, -1);
	  pango_layout_get_extents (label->layout, NULL, &logical_rect);
      
	  width = logical_rect.width;
	  height = logical_rect.height;
	  
	  /* Try to guess a reasonable maximum width
	   */
	  longest_paragraph = width;

	  width = MIN (width,
		       PANGO_SCALE * gdk_string_width (GTK_WIDGET (label)->style->font,
						"This long string gives a good enough length for any line to have."));
	  width = MIN (width,
		       PANGO_SCALE * (gdk_screen_width () + 1) / 2);

	  pango_layout_set_width (label->layout, width);
	  pango_layout_get_extents (label->layout, NULL, &logical_rect);
	  real_width = logical_rect.width;
	  height = logical_rect.height;
	  
	  /* Unfortunately, the above may leave us with a very unbalanced looking paragraph,
	   * so we try short search for a narrower width that leaves us with the same height
	   */
	  if (longest_paragraph > 0)
	    {
	      gint nlines, perfect_width;

	      nlines = pango_layout_get_line_count (label->layout);
	      perfect_width = (longest_paragraph + nlines - 1) / nlines;
	      
	      if (perfect_width < width)
		{
		  pango_layout_set_width (label->layout, perfect_width);
		  pango_layout_get_extents (label->layout, NULL, &logical_rect);
		  
		  if (logical_rect.height <= height)
		    {
		      width = perfect_width;
		      real_width = logical_rect.width;
		      height = logical_rect.height;
		    }
		  else
		    {
		      gint mid_width = (perfect_width + width) / 2;

		      if (mid_width > perfect_width)
			{
			  pango_layout_set_width (label->layout, mid_width);
			  pango_layout_get_extents (label->layout, NULL, &logical_rect);

			  if (logical_rect.height <= height)
			    {
			      width = mid_width;
			      real_width = logical_rect.width;
			      height = logical_rect.height;
			    }
			}
		    }
		}
	    }
	  pango_layout_set_width (label->layout, width);

	  requisition->width += real_width / PANGO_SCALE;
	  requisition->height += height / PANGO_SCALE;
	}
    }
  else				/* !label->wrap */
    {
      pango_layout_set_width (label->layout, -1);
      pango_layout_get_extents (label->layout, NULL, &logical_rect);

      requisition->width += logical_rect.width / PANGO_SCALE;
      requisition->height += logical_rect.height / PANGO_SCALE;
    }
}

static void 
gtk_label_style_set (GtkWidget *widget,
		     GtkStyle  *previous_style)
{
  GtkLabel *label;

  g_return_if_fail (GTK_IS_LABEL (widget));
  
  label = GTK_LABEL (widget);

  if (previous_style && label->layout)
    {
      g_object_unref (G_OBJECT (label->layout));
      label->layout = NULL;
    }
}

static void 
gtk_label_direction_changed (GtkWidget        *widget,
			     GtkTextDirection previous_dir)
{
  GtkLabel *label = GTK_LABEL (widget);

  if (label->layout)
    {
      g_object_unref (G_OBJECT (label->layout));
      label->layout = NULL;
    }

  GTK_WIDGET_CLASS (parent_class)->direction_changed (widget, previous_dir);
}

#if 0
static void
gtk_label_paint_word (GtkLabel     *label,
		      gint          x,
		      gint          y,
		      GtkLabelWord *word,
		      GdkRectangle *area)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GtkLabelULine *uline;
  gchar *tmp_str;
  
  tmp_str = gdk_wcstombs (word->beginning);
  if (tmp_str)
    {
      gtk_paint_string (widget->style, widget->window, widget->state,
			area, widget, "label", 
			x + word->x,
			y + word->y,
			tmp_str);
      g_free (tmp_str);
    }
  
  for (uline = word->uline; uline; uline = uline->next)
    gtk_paint_hline (widget->style, widget->window, 
		     widget->state, area,
		     widget, "label", 
		     x + uline->x1, x + uline->x2, y + uline->y);
}
#endif

static gint
gtk_label_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkLabel *label;
  GtkMisc *misc;
  gint x, y;
  gfloat xalign;
  
  g_return_val_if_fail (GTK_IS_LABEL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  label = GTK_LABEL (widget);
  
  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget) &&
      label->label && (*label->label != '\0'))
    {
      misc = GTK_MISC (widget);
      
      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = misc->xalign;
      else
	xalign = 1. - misc->xalign;

      /*
       * GC Clipping
       */
      gdk_gc_set_clip_rectangle (widget->style->white_gc, &event->area);
      gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], &event->area);
      
      x = floor (widget->allocation.x + (gint)misc->xpad
		 + ((widget->allocation.width - widget->requisition.width) * xalign)
		 + 0.5);
      
      y = floor (widget->allocation.y + (gint)misc->ypad 
		 + ((widget->allocation.height - widget->requisition.height) * misc->yalign)
		 + 0.5);

      gdk_draw_layout (widget->window, widget->style->fg_gc [widget->state], x, y, label->layout);
      
      gdk_gc_set_clip_rectangle (widget->style->white_gc, NULL);
      gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], NULL);
    }

  return TRUE;
}

guint      
gtk_label_parse_uline (GtkLabel    *label,
		       const gchar *str)
{
  guint accel_key = GDK_VoidSymbol;

  gchar *new_str;
  gchar *pattern;
  const gchar *src;
  gchar *dest, *pattern_dest;
  gboolean underscore;
      
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_VoidSymbol);
  g_return_val_if_fail (str != NULL, GDK_VoidSymbol);

  /* Convert text to wide characters */

  new_str = g_new (gchar, strlen (str) + 1);
  pattern = g_new (gchar, g_utf8_strlen (str, -1) + 1);
  
  underscore = FALSE;

  src = str;
  dest = new_str;
  pattern_dest = pattern;
  
  while (*src)
    {
      gunichar c;
      gchar *next_src;

      c = g_utf8_get_char (src);
      if (c == (gunichar)-1)
	{
	  g_warning ("Invalid input string");
	  g_free (new_str);
	  g_free (pattern);
	  return GDK_VoidSymbol;
	}
      next_src = g_utf8_next_char (src);
      
      if (underscore)
	{
	  if (c == '_')
	    *pattern_dest++ = ' ';
	  else
	    {
	      *pattern_dest++ = '_';
	      if (accel_key == GDK_VoidSymbol)
		accel_key = gdk_keyval_to_lower (c);
	    }

	  while (src < next_src)
	    *dest++ = *src++;
	  
	  underscore = FALSE;
	}
      else
	{
	  if (c == '_')
	    {
	      underscore = TRUE;
	      src = next_src;
	    }
	  else
	    {
	      while (src < next_src)
		*dest++ = *src++;
	  
	      *pattern_dest++ = ' ';
	    }
	}
    }
  *dest = 0;
  *pattern_dest = 0;
  
  gtk_label_set_text_internal (label, new_str);
  gtk_label_set_pattern (label, pattern);
  
  g_free (pattern);
  
  return accel_key;
}
