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

#include "gtkbbox.h"
#include "gtkintl.h"

enum {
  PROP_0,
  PROP_LAYOUT_STYLE,
  PROP_LAST
};

static void gtk_button_box_class_init    (GtkButtonBoxClass   *klass);
static void gtk_button_box_init          (GtkButtonBox        *box);
static void gtk_button_box_set_property  (GObject         *object,
					  guint            prop_id,
					  const GValue    *value,
					  GParamSpec      *pspec);
static void gtk_button_box_get_property  (GObject         *object,
					  guint            prop_id,
					  GValue          *value,
					  GParamSpec      *pspec);

#define DEFAULT_CHILD_MIN_WIDTH 85
#define DEFAULT_CHILD_MIN_HEIGHT 27
#define DEFAULT_CHILD_IPAD_X 4
#define DEFAULT_CHILD_IPAD_Y 0

GtkType
gtk_button_box_get_type (void)
{
  static GtkType button_box_type = 0;

  if (!button_box_type)
    {
      static const GtkTypeInfo button_box_info =
      {
	"GtkButtonBox",
	sizeof (GtkButtonBox),
	sizeof (GtkButtonBoxClass),
	(GtkClassInitFunc) gtk_button_box_class_init,
	(GtkObjectInitFunc) gtk_button_box_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      button_box_type = gtk_type_unique (gtk_box_get_type (), &button_box_info);
    }

  return button_box_type;
}

static void
gtk_button_box_class_init (GtkButtonBoxClass *class)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_button_box_set_property;
  gobject_class->get_property = gtk_button_box_get_property;
  
  /* FIXME we need to override the "spacing" property on GtkBox once
   * libgobject allows that.
   */

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child_min_width",
							     _("Minimum child width"),
							     _("Minimum width of buttons inside the box"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_MIN_WIDTH,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child_min_height",
							     _("Minimum child height"),
							     _("Minimum height of buttons inside the box"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_MIN_HEIGHT,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child_internal_pad_x",
							     _("Child internal width padding"),
							     _("Amount to increase child's size on either side"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_IPAD_X,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child_internal_pad_y",
							     _("Child internal height padding"),
							     _("Amount to increase child's size on the top and bottom"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_CHILD_IPAD_Y,
							     G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_LAYOUT_STYLE,
                                   g_param_spec_enum ("layout_style",
                                                      _("Layout style"),
                                                      _("How to layout the buttons in the box. Possible values are default, spread, edge, start and end"),
						      GTK_TYPE_BUTTON_BOX_STYLE,
						      GTK_BUTTONBOX_DEFAULT_STYLE,
                                                      G_PARAM_READWRITE));
}

static void
gtk_button_box_init (GtkButtonBox *button_box)
{
  GTK_BOX (button_box)->spacing = 0;
  button_box->child_min_width = GTK_BUTTONBOX_DEFAULT;
  button_box->child_min_height = GTK_BUTTONBOX_DEFAULT;
  button_box->child_ipad_x = GTK_BUTTONBOX_DEFAULT;
  button_box->child_ipad_y = GTK_BUTTONBOX_DEFAULT;
  button_box->layout_style = GTK_BUTTONBOX_DEFAULT_STYLE;
}

static void
gtk_button_box_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
  switch (prop_id) 
    {
    case PROP_LAYOUT_STYLE:
      gtk_button_box_set_layout (GTK_BUTTON_BOX (object),
				 g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_box_get_property (GObject         *object,
			     guint            prop_id,
			     GValue          *value,
			     GParamSpec      *pspec)
{
  switch (prop_id)
    {
    case PROP_LAYOUT_STYLE:
      g_value_set_enum (value, GTK_BUTTON_BOX (object)->layout_style);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/* set per widget values for spacing, child size and child internal padding */

void gtk_button_box_set_child_size (GtkButtonBox *widget, gint width, gint height)
{
  widget->child_min_width = width;
  widget->child_min_height = height;
}

void gtk_button_box_set_child_ipadding (GtkButtonBox *widget,
					gint ipad_x, gint ipad_y)
{
  widget->child_ipad_x = ipad_x;
  widget->child_ipad_y = ipad_y;
}

void gtk_button_box_set_layout (GtkButtonBox *widget, 
				GtkButtonBoxStyle layout_style)
{
  g_return_if_fail (layout_style >= GTK_BUTTONBOX_DEFAULT_STYLE &&
		    layout_style <= GTK_BUTTONBOX_END);

  if (widget->layout_style != layout_style)
    {
      widget->layout_style = layout_style;
      g_object_notify (G_OBJECT (widget), "layout_style");
      gtk_widget_queue_resize (GTK_WIDGET (widget));
    }
}


/* get per widget values for spacing, child size and child internal padding */

void gtk_button_box_get_child_size (GtkButtonBox *widget,
				     gint *width, gint *height)
{
  *width  = widget->child_min_width;
  *height = widget->child_min_height;
}

void gtk_button_box_get_child_ipadding (GtkButtonBox *widget,
					 gint* ipad_x, gint *ipad_y)
{
  *ipad_x = widget->child_ipad_x;
  *ipad_y = widget->child_ipad_y;
}

GtkButtonBoxStyle gtk_button_box_get_layout (GtkButtonBox *widget)
{
  return widget->layout_style;
}



/* Ask children how much space they require and round up 
   to match minimum size and internal padding.
   Returns the size each single child should have. */
void
_gtk_button_box_child_requisition (GtkWidget *widget,
                                   int *nvis_children,
                                   int *width,
                                   int *height)
{
  GtkButtonBox *bbox;
  GtkBoxChild *child;
  GList *children;
  gint nchildren;
  gint needed_width;
  gint needed_height;
  GtkRequisition child_requisition;
  gint ipad_w;
  gint ipad_h;
  gint width_default;
  gint height_default;
  gint ipad_x_default;
  gint ipad_y_default;
  
  gint child_min_width;
  gint child_min_height;
  gint ipad_x;
  gint ipad_y;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  bbox = GTK_BUTTON_BOX (widget);

  gtk_widget_style_get (widget,
                        "child_min_width",
                        &width_default,
                        "child_min_height",
                        &height_default,
                        "child_internal_pad_x",
                        &ipad_x_default,
                        "child_internal_pad_y",
                        &ipad_y_default, NULL);
  
  child_min_width = bbox->child_min_width   != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_min_width : width_default;
  child_min_height = bbox->child_min_height !=GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_min_height : height_default;
  ipad_x = bbox->child_ipad_x != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_ipad_x : ipad_x_default;
  ipad_y = bbox->child_ipad_y != GTK_BUTTONBOX_DEFAULT
	  ? bbox->child_ipad_y : ipad_y_default;

  nchildren = 0;
  children = GTK_BOX(bbox)->children;
  needed_width = child_min_width;
  needed_height = child_min_height;  
  ipad_w = ipad_x * 2;
  ipad_h = ipad_y * 2;
  
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  nchildren += 1;
	  gtk_widget_size_request (child->widget, &child_requisition);
	  if (child_requisition.width + ipad_w > needed_width)
		  needed_width = child_requisition.width + ipad_w;
	  if (child_requisition.height + ipad_h > needed_height)
		  needed_height = child_requisition.height + ipad_h;
	}
    }
  
  *nvis_children = nchildren;
  *width = needed_width;
  *height = needed_height;
}
