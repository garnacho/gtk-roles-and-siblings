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

#include "gtkalignment.h"
#include "gtkintl.h"

enum {
  PROP_0,

  PROP_XALIGN,
  PROP_YALIGN,
  PROP_XSCALE,
  PROP_YSCALE,
  
  PROP_LAST
};


static void gtk_alignment_class_init    (GtkAlignmentClass *klass);
static void gtk_alignment_init          (GtkAlignment      *alignment);
static void gtk_alignment_size_request  (GtkWidget         *widget,
					 GtkRequisition    *requisition);
static void gtk_alignment_size_allocate (GtkWidget         *widget,
					 GtkAllocation     *allocation);
static void gtk_alignment_set_property (GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);
static void gtk_alignment_get_property (GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);

GType
gtk_alignment_get_type (void)
{
  static GType alignment_type = 0;

  if (!alignment_type)
    {
      static const GTypeInfo alignment_info =
      {
	sizeof (GtkAlignmentClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_alignment_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkAlignment),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_alignment_init,
      };

      alignment_type = g_type_register_static (GTK_TYPE_BIN, "GtkAlignment",
					       &alignment_info, 0);
    }

  return alignment_type;
}

static void
gtk_alignment_class_init (GtkAlignmentClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  gobject_class->set_property = gtk_alignment_set_property;
  gobject_class->get_property = gtk_alignment_get_property;

  widget_class->size_request = gtk_alignment_size_request;
  widget_class->size_allocate = gtk_alignment_size_allocate;
  
  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float("xalign",
                                                      _("Horizontal alignment"),
                                                      _("Horizontal position of child in available space. 0.0 is left aligned, 1.0 is right aligned"),
                                                      0.0,
                                                      1.0,
                                                      0.5,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
   
  g_object_class_install_property (gobject_class,
                                   PROP_YALIGN,
                                   g_param_spec_float("yalign",
                                                      _("Vertical alignment"),
                                                      _("Vertical position of child in available space. 0.0 is top aligned, 1.0 is bottom aligned"),
                                                      0.0,
                                                      1.0,
						      0.5,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_XSCALE,
                                   g_param_spec_float("xscale",
                                                      _("Horizontal scale"),
                                                      _("If available horizontal space is bigger than needed for the child, how much of it to use for the child. 0.0 means none, 1.0 means all"),
                                                      0.0,
                                                      1.0,
                                                      1.0,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_YSCALE,
                                   g_param_spec_float("yscale",
                                                      _("Vertical scale"),
                                                      _("If available vertical space is bigger than needed for the child, how much of it to use for the child. 0.0 means none, 1.0 means all"),
                                                      0.0,
                                                      1.0,
                                                      1.0,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gtk_alignment_init (GtkAlignment *alignment)
{
  GTK_WIDGET_SET_FLAGS (alignment, GTK_NO_WINDOW);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (alignment), FALSE);

  alignment->xalign = 0.5;
  alignment->yalign = 0.5;
  alignment->xscale = 1.0;
  alignment->yscale = 1.0;
}

GtkWidget*
gtk_alignment_new (gfloat xalign,
		   gfloat yalign,
		   gfloat xscale,
		   gfloat yscale)
{
  GtkAlignment *alignment;

  alignment = g_object_new (GTK_TYPE_ALIGNMENT, NULL);

  alignment->xalign = CLAMP (xalign, 0.0, 1.0);
  alignment->yalign = CLAMP (yalign, 0.0, 1.0);
  alignment->xscale = CLAMP (xscale, 0.0, 1.0);
  alignment->yscale = CLAMP (yscale, 0.0, 1.0);

  return GTK_WIDGET (alignment);
}

static void
gtk_alignment_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
  GtkAlignment *alignment;
  
  alignment = GTK_ALIGNMENT (object);
  
  switch (prop_id)
    {
    case PROP_XALIGN:
      gtk_alignment_set (alignment,
			 g_value_get_float (value),
			 alignment->yalign,
			 alignment->xscale,
			 alignment->yscale);
      break;
    case PROP_YALIGN:
      gtk_alignment_set (alignment,
			 alignment->xalign,
			 g_value_get_float (value),
			 alignment->xscale,
			 alignment->yscale);
      break;
    case PROP_XSCALE:
      gtk_alignment_set (alignment,
			 alignment->xalign,
			 alignment->yalign,
			 g_value_get_float (value),
			 alignment->yscale);
      break;
    case PROP_YSCALE:
      gtk_alignment_set (alignment,
			 alignment->xalign,
			 alignment->yalign,
			 alignment->xscale,
			 g_value_get_float (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_alignment_get_property (GObject         *object,
			    guint            prop_id,
			    GValue          *value,
			    GParamSpec      *pspec)
{
  GtkAlignment *alignment;
  
  alignment = GTK_ALIGNMENT (object);
   
  switch (prop_id)
    {
    case PROP_XALIGN:
      g_value_set_float(value, alignment->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float(value, alignment->yalign);
      break;
    case PROP_XSCALE:
      g_value_set_float(value, alignment->xscale);
      break;
    case PROP_YSCALE:
      g_value_set_float(value, alignment->yscale);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gtk_alignment_set (GtkAlignment *alignment,
		   gfloat        xalign,
		   gfloat        yalign,
		   gfloat        xscale,
		   gfloat        yscale)
{
  g_return_if_fail (GTK_IS_ALIGNMENT (alignment));

  xalign = CLAMP (xalign, 0.0, 1.0);
  yalign = CLAMP (yalign, 0.0, 1.0);
  xscale = CLAMP (xscale, 0.0, 1.0);
  yscale = CLAMP (yscale, 0.0, 1.0);

  if (   (alignment->xalign != xalign)
      || (alignment->yalign != yalign)
      || (alignment->xscale != xscale)
      || (alignment->yscale != yscale))
    {
      g_object_freeze_notify (G_OBJECT (alignment));
      if (alignment->xalign != xalign)
        {
           alignment->xalign = xalign;
           g_object_notify (G_OBJECT (alignment), "xalign");
        }
      if (alignment->yalign != yalign)
        {
           alignment->yalign = yalign;
           g_object_notify (G_OBJECT (alignment), "yalign");
        }
      if (alignment->xscale != xscale)
        {
           alignment->xscale = xscale;
           g_object_notify (G_OBJECT (alignment), "xscale");
        }
      if (alignment->yscale != yscale)
        {
           alignment->yscale = yscale;
           g_object_notify (G_OBJECT (alignment), "yscale");
        }
      g_object_thaw_notify (G_OBJECT (alignment));

      if (GTK_BIN (alignment)->child)
        gtk_widget_queue_resize (GTK_BIN (alignment)->child);
      gtk_widget_queue_draw (GTK_WIDGET (alignment));
    }
}


static void
gtk_alignment_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkAlignment *alignment;
  GtkBin *bin;

  alignment = GTK_ALIGNMENT (widget);
  bin = GTK_BIN (widget);

  requisition->width = GTK_CONTAINER (widget)->border_width * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
gtk_alignment_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkAlignment *alignment;
  GtkBin *bin;
  GtkAllocation child_allocation;
  GtkRequisition child_requisition;
  gint width, height;
  gint x, y;

  widget->allocation = *allocation;
  alignment = GTK_ALIGNMENT (widget);
  bin = GTK_BIN (widget);
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_get_child_requisition (bin->child, &child_requisition);
      
      x = GTK_CONTAINER (alignment)->border_width;
      y = GTK_CONTAINER (alignment)->border_width;
      width = MAX (allocation->width - 2 * x, 0);
      height = MAX (allocation->height - 2 * y, 0);
      
      if (width > child_requisition.width)
	child_allocation.width = (child_requisition.width *
				  (1.0 - alignment->xscale) +
				  width * alignment->xscale);
      else
	child_allocation.width = width;
      
      if (height > child_requisition.height)
	child_allocation.height = (child_requisition.height *
				   (1.0 - alignment->yscale) +
				   height * alignment->yscale);
      else
	child_allocation.height = height;

      child_allocation.x = alignment->xalign * (width - child_allocation.width) + allocation->x + x;
      child_allocation.y = alignment->yalign * (height - child_allocation.height) + allocation->y + y;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}
