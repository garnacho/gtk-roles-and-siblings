/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
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

#include <string.h>

#include "gtkbutton.h"
#include "gtktogglebutton.h"
#include "gtkradiobutton.h"
#include "gtklabel.h"
#include "gtkvbox.h"
#include "gtkhbox.h"
#include "gtktoolbar.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkimage.h"
#include "gtksettings.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"


#define DEFAULT_IPADDING 0
#define DEFAULT_SPACE_SIZE  5
#define DEFAULT_SPACE_STYLE GTK_TOOLBAR_SPACE_LINE

#define DEFAULT_ICON_SIZE GTK_ICON_SIZE_LARGE_TOOLBAR
#define DEFAULT_TOOLBAR_STYLE GTK_TOOLBAR_BOTH

#define SPACE_LINE_DIVISION 10
#define SPACE_LINE_START    3
#define SPACE_LINE_END      7

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_TOOLBAR_STYLE
};

enum {
  ORIENTATION_CHANGED,
  STYLE_CHANGED,
  LAST_SIGNAL
};

typedef struct _GtkToolbarChildSpace GtkToolbarChildSpace;
struct _GtkToolbarChildSpace
{
  GtkToolbarChild child;

  gint alloc_x, alloc_y;
};

static void gtk_toolbar_class_init               (GtkToolbarClass *class);
static void gtk_toolbar_init                     (GtkToolbar      *toolbar);
static void gtk_toolbar_set_property             (GObject         *object,
						  guint            prop_id,
						  const GValue    *value,
						  GParamSpec      *pspec);
static void gtk_toolbar_get_property             (GObject         *object,
						  guint            prop_id,
						  GValue          *value,
						  GParamSpec      *pspec);
static void gtk_toolbar_destroy                  (GtkObject       *object);
static gint gtk_toolbar_expose                   (GtkWidget       *widget,
						  GdkEventExpose  *event);
static void gtk_toolbar_size_request             (GtkWidget       *widget,
				                  GtkRequisition  *requisition);
static void gtk_toolbar_size_allocate            (GtkWidget       *widget,
				                  GtkAllocation   *allocation);
static void gtk_toolbar_style_set                (GtkWidget       *widget,
                                                  GtkStyle        *prev_style);
static gboolean gtk_toolbar_focus                (GtkWidget       *widget,
                                                  GtkDirectionType dir);
static void gtk_toolbar_screen_changed           (GtkWidget       *widget,
						  GdkScreen       *previous_screen);
static void gtk_toolbar_show_all                 (GtkWidget       *widget);
static void gtk_toolbar_hide_all                 (GtkWidget       *widget);
static void gtk_toolbar_add                      (GtkContainer    *container,
				                  GtkWidget       *widget);
static void gtk_toolbar_remove                   (GtkContainer    *container,
						  GtkWidget       *widget);
static void gtk_toolbar_forall                   (GtkContainer    *container,
						  gboolean	   include_internals,
				                  GtkCallback      callback,
				                  gpointer         callback_data);

static void gtk_real_toolbar_orientation_changed (GtkToolbar      *toolbar,
						  GtkOrientation   orientation);
static void gtk_real_toolbar_style_changed       (GtkToolbar      *toolbar,
						  GtkToolbarStyle  style);

static GtkWidget * gtk_toolbar_internal_insert_element (GtkToolbar          *toolbar,
                                                        GtkToolbarChildType  type,
                                                        GtkWidget           *widget,
                                                        const char          *text,
                                                        const char          *tooltip_text,
                                                        const char          *tooltip_private_text,
                                                        GtkWidget           *icon,
                                                        GtkSignalFunc        callback,
                                                        gpointer             user_data,
                                                        gint                 position);

static GtkWidget * gtk_toolbar_internal_insert_item (GtkToolbar    *toolbar,
                                                     const char    *text,
                                                     const char    *tooltip_text,
                                                     const char    *tooltip_private_text,
                                                     GtkWidget     *icon,
                                                     GtkSignalFunc  callback,
                                                     gpointer       user_data,
                                                     gint           position);

static void        gtk_toolbar_update_button_relief (GtkToolbar *toolbar);

static GtkReliefStyle       get_button_relief (GtkToolbar *toolbar);
static gint                 get_space_size    (GtkToolbar *toolbar);
static GtkToolbarSpaceStyle get_space_style   (GtkToolbar *toolbar);


static GtkContainerClass *parent_class;

static guint toolbar_signals[LAST_SIGNAL] = { 0 };


GType
gtk_toolbar_get_type (void)
{
  static GType toolbar_type = 0;

  if (!toolbar_type)
    {
      static const GTypeInfo toolbar_info =
      {
	sizeof (GtkToolbarClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_toolbar_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkToolbar),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_toolbar_init,
      };

      toolbar_type = g_type_register_static (GTK_TYPE_CONTAINER, "GtkToolbar",
					     &toolbar_info, 0);
    }

  return toolbar_type;
}

static void
gtk_toolbar_class_init (GtkToolbarClass *class)
{
  GObjectClass   *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (class);
  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = g_type_class_peek_parent (class);

  object_class->destroy = gtk_toolbar_destroy;
  gobject_class->set_property = gtk_toolbar_set_property;
  gobject_class->get_property = gtk_toolbar_get_property;

  widget_class->expose_event = gtk_toolbar_expose;
  widget_class->size_request = gtk_toolbar_size_request;
  widget_class->size_allocate = gtk_toolbar_size_allocate;
  widget_class->style_set = gtk_toolbar_style_set;
  widget_class->show_all = gtk_toolbar_show_all;
  widget_class->hide_all = gtk_toolbar_hide_all;
  widget_class->focus = gtk_toolbar_focus;
  widget_class->screen_changed = gtk_toolbar_screen_changed;
  
  container_class->add = gtk_toolbar_add;
  container_class->remove = gtk_toolbar_remove;
  container_class->forall = gtk_toolbar_forall;
  
  class->orientation_changed = gtk_real_toolbar_orientation_changed;
  class->style_changed = gtk_real_toolbar_style_changed;

  toolbar_signals[ORIENTATION_CHANGED] =
    g_signal_new ("orientation_changed",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToolbarClass, orientation_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_ORIENTATION);
  toolbar_signals[STYLE_CHANGED] =
    g_signal_new ("style_changed",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToolbarClass, style_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TOOLBAR_STYLE);
  
  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
 						      _("Orientation"),
 						      _("The orientation of the toolbar"),
 						      GTK_TYPE_ORIENTATION,
 						      GTK_ORIENTATION_HORIZONTAL,
 						      G_PARAM_READWRITE));
 
   g_object_class_install_property (gobject_class,
                                    PROP_TOOLBAR_STYLE,
                                    g_param_spec_enum ("toolbar_style",
 						      _("Toolbar Style"),
 						      _("How to draw the toolbar"),
 						      GTK_TYPE_TOOLBAR_STYLE,
 						      GTK_TOOLBAR_ICONS,
 						      G_PARAM_READWRITE));


  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("space_size",
							     _("Spacer size"),
							     _("Size of spacers"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_SPACE_SIZE,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("internal_padding",
							     _("Internal padding"),
							     _("Amount of border space between the toolbar shadow and the buttons"),
							     0,
							     G_MAXINT,
                                                             DEFAULT_IPADDING,
                                                             G_PARAM_READABLE));
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("space_style",
							     _("Space style"),
							     _("Whether spacers are vertical lines or just blank"),
                                                              GTK_TYPE_TOOLBAR_SPACE_STYLE,
                                                              DEFAULT_SPACE_STYLE,
                                                              
                                                              G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_enum ("button_relief",
							     _("Button relief"),
							     _("Type of bevel around toolbar buttons"),
                                                              GTK_TYPE_RELIEF_STYLE,
                                                              GTK_RELIEF_NONE,
                                                              G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow_type",
                                                              _("Shadow type"),
                                                              _("Style of bevel around the toolbar"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_OUT,
                                                              G_PARAM_READABLE));

  gtk_settings_install_property (g_param_spec_enum ("gtk-toolbar-style",
                                                    _("Toolbar style"),
                                                    _("Whether default toolbars have text only, text and icons, icons only, etc."),
                                                    GTK_TYPE_TOOLBAR_STYLE,
                                                    DEFAULT_TOOLBAR_STYLE,
                                                    G_PARAM_READWRITE));

  gtk_settings_install_property (g_param_spec_enum ("gtk-toolbar-icon-size",
                                                    _("Toolbar icon size"),
                                                    _("Size of icons in default toolbars"),
                                                    GTK_TYPE_ICON_SIZE,
                                                    DEFAULT_ICON_SIZE,
                                                    G_PARAM_READWRITE));  
}

static void
style_change_notify (GtkToolbar *toolbar)
{
  if (!toolbar->style_set)
    {
      /* pretend it was set, then unset, thus reverting to new default */
      toolbar->style_set = TRUE; 
      gtk_toolbar_unset_style (toolbar);
    }
}

static void
icon_size_change_notify (GtkToolbar *toolbar)
{
  if (!toolbar->icon_size_set)
    {
      /* pretend it was set, then unset, thus reverting to new default */
      toolbar->icon_size_set = TRUE; 
      gtk_toolbar_unset_icon_size (toolbar);
    }
}

static GtkSettings *
toolbar_get_settings (GtkToolbar *toolbar)
{
  return g_object_get_data (G_OBJECT (toolbar), "gtk-toolbar-settings");
}

static void
gtk_toolbar_screen_changed (GtkWidget *widget,
			    GdkScreen *previous_screen)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkSettings *old_settings = toolbar_get_settings (toolbar);
  GtkSettings *settings;

  if (gtk_widget_has_screen (GTK_WIDGET (toolbar)))
    settings = gtk_widget_get_settings (GTK_WIDGET (toolbar));
  else
    settings = NULL;

  if (settings == old_settings)
    return;

  if (old_settings)
    {
      g_signal_handler_disconnect (old_settings, toolbar->style_set_connection);
      g_signal_handler_disconnect (old_settings, toolbar->icon_size_connection);

      g_object_unref (old_settings);
    }
  
  if (settings)
    {
      toolbar->style_set_connection =
	g_signal_connect_swapped (settings,
				  "notify::gtk-toolbar-style",
				  G_CALLBACK (style_change_notify),
				  toolbar);
      
      toolbar->icon_size_connection =
	g_signal_connect_swapped (settings,
				  "notify::gtk-toolbar-icon-size",
				   G_CALLBACK (icon_size_change_notify),
				   toolbar);


      g_object_ref (settings);
      g_object_set_data (G_OBJECT (toolbar), "gtk-toolbar-settings", settings);
    }
  else
    g_object_set_data (G_OBJECT (toolbar), "gtk-toolbar-settings", NULL);

  style_change_notify (toolbar);
  icon_size_change_notify (toolbar);
}

static void
gtk_toolbar_init (GtkToolbar *toolbar)
{
  GTK_WIDGET_SET_FLAGS (toolbar, GTK_NO_WINDOW);
  GTK_WIDGET_UNSET_FLAGS (toolbar, GTK_CAN_FOCUS);

  toolbar->num_children = 0;
  toolbar->children     = NULL;
  toolbar->orientation  = GTK_ORIENTATION_HORIZONTAL;
  toolbar->icon_size    = DEFAULT_ICON_SIZE;
  toolbar->style        = DEFAULT_TOOLBAR_STYLE;
  toolbar->tooltips     = gtk_tooltips_new ();
  g_object_ref (toolbar->tooltips);
  gtk_object_sink (GTK_OBJECT (toolbar->tooltips));
  
  toolbar->button_maxw  = 0;
  toolbar->button_maxh  = 0;

  toolbar->style_set = FALSE;
  toolbar->icon_size_set = FALSE;
}

static void
gtk_toolbar_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (object);
  
  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_toolbar_set_orientation (toolbar, g_value_get_enum (value));
      break;
    case PROP_TOOLBAR_STYLE:
      gtk_toolbar_set_style (toolbar, g_value_get_enum (value));
      break;
    }
}

static void
gtk_toolbar_get_property (GObject      *object,
			  guint         prop_id,
			  GValue       *value,
			  GParamSpec   *pspec)
{
  GtkToolbar *toolbar = GTK_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, toolbar->orientation);
      break;
    case PROP_TOOLBAR_STYLE:
      g_value_set_enum (value, toolbar->style);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkWidget*
gtk_toolbar_new (void)
{
  GtkToolbar *toolbar;

  toolbar = g_object_new (GTK_TYPE_TOOLBAR, NULL);

  return GTK_WIDGET (toolbar);
}

static void
gtk_toolbar_destroy (GtkObject *object)
{
  GtkToolbar *toolbar;
  GList *children;

  g_return_if_fail (GTK_IS_TOOLBAR (object));

  toolbar = GTK_TOOLBAR (object);

  if (toolbar->tooltips)
    {
      g_object_unref (toolbar->tooltips);
      toolbar->tooltips = NULL;
    }

  for (children = toolbar->children; children; children = children->next)
    {
      GtkToolbarChild *child;

      child = children->data;

      if (child->type != GTK_TOOLBAR_CHILD_SPACE)
	{
	  g_object_ref (child->widget);
	  gtk_widget_unparent (child->widget);
	  gtk_widget_destroy (child->widget);
	  g_object_unref (child->widget);
	}

      g_free (child);
    }
  g_list_free (toolbar->children);
  toolbar->children = NULL;
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_toolbar_paint_space_line (GtkWidget       *widget,
			      GdkRectangle    *area,
			      GtkToolbarChild *child)
{
  GtkToolbar *toolbar;
  GtkToolbarChildSpace *child_space;
  gint space_size;
  
  g_return_if_fail (GTK_IS_TOOLBAR (widget));
  g_return_if_fail (child != NULL);
  g_return_if_fail (child->type == GTK_TOOLBAR_CHILD_SPACE);

  toolbar = GTK_TOOLBAR (widget);

  child_space = (GtkToolbarChildSpace *) child;
  space_size = get_space_size (toolbar);
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_paint_vline (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget), area, widget,
		     "toolbar",
		     child_space->alloc_y + toolbar->button_maxh *
		     SPACE_LINE_START / SPACE_LINE_DIVISION,
		     child_space->alloc_y + toolbar->button_maxh *
		     SPACE_LINE_END / SPACE_LINE_DIVISION,
		     child_space->alloc_x +
		     (space_size -
		      widget->style->xthickness) / 2);
  else
    gtk_paint_hline (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget), area, widget,
		     "toolbar",
		     child_space->alloc_x + toolbar->button_maxw *
		     SPACE_LINE_START / SPACE_LINE_DIVISION,
		     child_space->alloc_x + toolbar->button_maxw *
		     SPACE_LINE_END / SPACE_LINE_DIVISION,
		     child_space->alloc_y +
		     (space_size -
		      widget->style->ythickness) / 2);
}

static gint
gtk_toolbar_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  gint border_width;
  
  g_return_val_if_fail (GTK_IS_TOOLBAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  border_width = GTK_CONTAINER (widget)->border_width;
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      GtkShadowType shadow_type;

      toolbar = GTK_TOOLBAR (widget);

      gtk_widget_style_get (widget, "shadow_type", &shadow_type, NULL);
      
      gtk_paint_box (widget->style,
		     widget->window,
                     GTK_WIDGET_STATE (widget),
                     shadow_type,
		     &event->area, widget, "toolbar",
		     widget->allocation.x + border_width,
                     widget->allocation.y + border_width,
		     widget->allocation.width - border_width,
                     widget->allocation.height - border_width);
      
      for (children = toolbar->children; children; children = children->next)
	{
	  child = children->data;

	  if (child->type == GTK_TOOLBAR_CHILD_SPACE)
	    {
	      if (get_space_style (toolbar) == GTK_TOOLBAR_SPACE_LINE)
		gtk_toolbar_paint_space_line (widget, &event->area, child);
	    }
	  else
	    gtk_container_propagate_expose (GTK_CONTAINER (widget),
					    child->widget,
					    event);
	}
    }

  return FALSE;
}

static void
gtk_toolbar_size_request (GtkWidget      *widget,
			  GtkRequisition *requisition)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  gint nbuttons;
  gint button_maxw, button_maxh;
  gint widget_maxw, widget_maxh;
  GtkRequisition child_requisition;
  gint space_size;
  gint ipadding;
  
  g_return_if_fail (GTK_IS_TOOLBAR (widget));
  g_return_if_fail (requisition != NULL);

  toolbar = GTK_TOOLBAR (widget);

  requisition->width = GTK_CONTAINER (toolbar)->border_width * 2;
  requisition->height = GTK_CONTAINER (toolbar)->border_width * 2;
  nbuttons = 0;
  button_maxw = 0;
  button_maxh = 0;
  widget_maxw = 0;
  widget_maxh = 0;

  space_size = get_space_size (toolbar);
  
  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      switch (child->type)
	{
	case GTK_TOOLBAR_CHILD_SPACE:
	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    requisition->width += space_size;
	  else
	    requisition->height += space_size;

	  break;

	case GTK_TOOLBAR_CHILD_BUTTON:
	case GTK_TOOLBAR_CHILD_RADIOBUTTON:
	case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
	  if (GTK_WIDGET_VISIBLE (child->widget))
	    {              
	      gtk_widget_size_request (child->widget, &child_requisition);

	      nbuttons++;
	      button_maxw = MAX (button_maxw, child_requisition.width);
	      button_maxh = MAX (button_maxh, child_requisition.height);
	    }

	  break;

	case GTK_TOOLBAR_CHILD_WIDGET:
	  if (GTK_WIDGET_VISIBLE (child->widget))
	    {
	      gtk_widget_size_request (child->widget, &child_requisition);

	      widget_maxw = MAX (widget_maxw, child_requisition.width);
	      widget_maxh = MAX (widget_maxh, child_requisition.height);

	      if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
		requisition->width += child_requisition.width;
	      else
		requisition->height += child_requisition.height;
	    }

	  break;

	default:
	  g_assert_not_reached ();
	}
    }

  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      requisition->width += nbuttons * button_maxw;
      requisition->height += MAX (button_maxh, widget_maxh);
    }
  else
    {
      requisition->width += MAX (button_maxw, widget_maxw);
      requisition->height += nbuttons * button_maxh;
    }

  /* Extra spacing */
  gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
  
  requisition->width += 2 * ipadding;
  requisition->height += 2 * ipadding;
  
  toolbar->button_maxw = button_maxw;
  toolbar->button_maxh = button_maxh;
}

static void
gtk_toolbar_size_allocate (GtkWidget     *widget,
			   GtkAllocation *allocation)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;
  GtkToolbarChildSpace *child_space;
  GtkAllocation alloc;
  GtkRequisition child_requisition;
  gint x_border_width, y_border_width;
  gint space_size;
  gint ipadding;
  GtkTextDirection direction;
  gint ltr_x = 0;		/* Quiet GCC */
  
  g_return_if_fail (GTK_IS_TOOLBAR (widget));
  g_return_if_fail (allocation != NULL);

  toolbar = GTK_TOOLBAR (widget);
  widget->allocation = *allocation;
  
  direction = gtk_widget_get_direction (widget);

  x_border_width = GTK_CONTAINER (toolbar)->border_width;
  y_border_width = GTK_CONTAINER (toolbar)->border_width;

  gtk_widget_style_get (widget, "internal_padding", &ipadding, NULL);
  
  x_border_width += ipadding;
  y_border_width += ipadding;
  
  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
    ltr_x = allocation->x + x_border_width;
  else
    alloc.y = allocation->y + y_border_width;

  space_size = get_space_size (toolbar);
  
  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      switch (child->type)
	{
	case GTK_TOOLBAR_CHILD_SPACE:

	  child_space = (GtkToolbarChildSpace *) child;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      if (direction == GTK_TEXT_DIR_LTR)
		child_space->alloc_x = ltr_x;
	      else
		child_space->alloc_x = allocation->width - ltr_x - space_size;
	      child_space->alloc_y = allocation->y + (allocation->height - toolbar->button_maxh) / 2;
	      ltr_x += space_size;
	    }
	  else
	    {
	      child_space->alloc_x = allocation->x + (allocation->width - toolbar->button_maxw) / 2;
	      child_space->alloc_y = alloc.y;
	      alloc.y += space_size;
	    }

	  break;

	case GTK_TOOLBAR_CHILD_BUTTON:
	case GTK_TOOLBAR_CHILD_RADIOBUTTON:
	case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
	  if (!GTK_WIDGET_VISIBLE (child->widget))
	    break;

	  alloc.width = toolbar->button_maxw;
	  alloc.height = toolbar->button_maxh;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL) 
	    {
	      if (direction == GTK_TEXT_DIR_LTR)
		alloc.x = ltr_x;
	      else
		alloc.x = allocation->width - ltr_x - alloc.width;
	      alloc.y = allocation->y + (allocation->height - toolbar->button_maxh) / 2;
	    }
	  else
	    alloc.x = allocation->x + (allocation->width - toolbar->button_maxw) / 2;
	  
	  gtk_widget_size_allocate (child->widget, &alloc);

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    ltr_x += toolbar->button_maxw;
	  else
	    alloc.y += toolbar->button_maxh;

	  break;

	case GTK_TOOLBAR_CHILD_WIDGET:
	  if (!GTK_WIDGET_VISIBLE (child->widget))
	    break;

	  gtk_widget_get_child_requisition (child->widget, &child_requisition);
	  
	  alloc.width = child_requisition.width;
	  alloc.height = child_requisition.height;

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL) 
	    {
	      if (direction == GTK_TEXT_DIR_LTR)
		alloc.x = ltr_x;
	      else
		alloc.x = allocation->width - ltr_x - alloc.width;
	      alloc.y = allocation->y + (allocation->height - child_requisition.height) / 2;
	    }
	  else
	    alloc.x = allocation->x + (allocation->width - child_requisition.width) / 2;

	  gtk_widget_size_allocate (child->widget, &alloc);

	  if (toolbar->orientation == GTK_ORIENTATION_HORIZONTAL)
	    ltr_x += child_requisition.width;
	  else
	    alloc.y += child_requisition.height;

	  break;

	default:
	  g_assert_not_reached ();
	}
    }
}

static void
gtk_toolbar_style_set (GtkWidget  *widget,
                       GtkStyle   *prev_style)
{
  if (prev_style)
    gtk_toolbar_update_button_relief (GTK_TOOLBAR (widget));
}

static gboolean
gtk_toolbar_focus (GtkWidget       *widget,
                   GtkDirectionType dir)
{
  /* Focus can't go in toolbars */
  
  return FALSE;
}

static void
child_show_all (GtkWidget *widget)
{
  /* Don't show our own children, since that would
   * show labels we may intend to hide in icons-only mode
   */
  if (!g_object_get_data (G_OBJECT (widget),
                          "gtk-toolbar-is-child"))
    gtk_widget_show_all (widget);
}

static void
gtk_toolbar_show_all (GtkWidget *widget)
{
  gtk_container_foreach (GTK_CONTAINER (widget),
			 (GtkCallback) child_show_all,
			 NULL);
  gtk_widget_show (widget);
}

static void
child_hide_all (GtkWidget *widget)
{
  /* Don't hide our own children, since that would also hide
   * widgets that won't be shown again by gtk_toolbar_show_all().
   */
  if (!g_object_get_data (G_OBJECT (widget),
                          "gtk-toolbar-is-child"))
    gtk_widget_hide_all (widget);
}

static void
gtk_toolbar_hide_all (GtkWidget *widget)
{
  gtk_container_foreach (GTK_CONTAINER (widget),
			 (GtkCallback) child_hide_all,
			 NULL);
  gtk_widget_hide (widget);
}

static void
gtk_toolbar_add (GtkContainer *container,
		 GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (widget != NULL);

  gtk_toolbar_append_widget (GTK_TOOLBAR (container), widget, NULL, NULL);
}

static void
gtk_toolbar_remove (GtkContainer *container,
		    GtkWidget    *widget)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;

  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (widget != NULL);

  toolbar = GTK_TOOLBAR (container);

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if ((child->type != GTK_TOOLBAR_CHILD_SPACE) && (child->widget == widget))
	{
	  gboolean was_visible;

	  was_visible = GTK_WIDGET_VISIBLE (widget);
	  gtk_widget_unparent (widget);

	  toolbar->children = g_list_remove_link (toolbar->children, children);
	  g_free (child);
	  g_list_free (children);
	  toolbar->num_children--;

	  if (was_visible && GTK_WIDGET_VISIBLE (container))
	    gtk_widget_queue_resize (GTK_WIDGET (container));

	  break;
	}
    }
}

static void
gtk_toolbar_forall (GtkContainer *container,
		    gboolean	  include_internals,
		    GtkCallback   callback,
		    gpointer      callback_data)
{
  GtkToolbar *toolbar;
  GList *children;
  GtkToolbarChild *child;

  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (callback != NULL);

  toolbar = GTK_TOOLBAR (container);

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if (child->type != GTK_TOOLBAR_CHILD_SPACE)
	(*callback) (child->widget, callback_data);
    }
}

GtkWidget *
gtk_toolbar_append_item (GtkToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

GtkWidget *
gtk_toolbar_prepend_item (GtkToolbar    *toolbar,
			  const char    *text,
			  const char    *tooltip_text,
			  const char    *tooltip_private_text,
			  GtkWidget     *icon,
			  GtkSignalFunc  callback,
			  gpointer       user_data)
{
  return gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
				     NULL, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     0);
}

static GtkWidget *
gtk_toolbar_internal_insert_item (GtkToolbar    *toolbar,
                                  const char    *text,
                                  const char    *tooltip_text,
                                  const char    *tooltip_private_text,
                                  GtkWidget     *icon,
                                  GtkSignalFunc  callback,
                                  gpointer       user_data,
                                  gint           position)
{
  return gtk_toolbar_internal_insert_element (toolbar, GTK_TOOLBAR_CHILD_BUTTON,
                                              NULL, text,
                                              tooltip_text, tooltip_private_text,
                                              icon, callback, user_data,
                                              position);
}
     
GtkWidget *
gtk_toolbar_insert_item (GtkToolbar    *toolbar,
			 const char    *text,
			 const char    *tooltip_text,
			 const char    *tooltip_private_text,
			 GtkWidget     *icon,
			 GtkSignalFunc  callback,
			 gpointer       user_data,
			 gint           position)
{
  return gtk_toolbar_internal_insert_item (toolbar, 
                                           text, tooltip_text, tooltip_private_text,
                                           icon, callback, user_data,
                                           position);
}

/**
 * gtk_toolbar_set_icon_size:
 * @toolbar: A #GtkToolbar
 * @icon_size: The #GtkIconSize that stock icons in the toolbar shall have.
 *
 * This function sets the size of stock icons in the toolbar. You
 * can call it both before you add the icons and after they've been
 * added. The size you set will override user preferences for the default
 * icon size.
 **/
void
gtk_toolbar_set_icon_size (GtkToolbar  *toolbar,
			   GtkIconSize  icon_size)
{
  GList *children;
  GtkToolbarChild *child;
  GtkImage *image;
  gchar *stock_id;

  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  toolbar->icon_size_set = TRUE;
  
  if (toolbar->icon_size == icon_size)
    return;
  
  toolbar->icon_size = icon_size;

  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;
      if ((child->type == GTK_TOOLBAR_CHILD_BUTTON ||
	   child->type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON ||
	   child->type == GTK_TOOLBAR_CHILD_RADIOBUTTON) &&
	  GTK_IS_IMAGE (child->icon))
	{
	  image = GTK_IMAGE (child->icon);
	  if (gtk_image_get_storage_type (image) == GTK_IMAGE_STOCK)
	    {
	      gtk_image_get_stock (image, &stock_id, NULL);
	      stock_id = g_strdup (stock_id);
	      gtk_image_set_from_stock (image,
					stock_id,
					icon_size);
	      g_free (stock_id);
	    }
	}
    }
  
  gtk_widget_queue_resize (GTK_WIDGET (toolbar));
}

/**
 * gtk_toolbar_get_icon_size:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves the icon size fo the toolbar. See gtk_toolbar_set_icon_size().
 *
 * Return value: the current icon size for the icons on the toolbar.
 **/
GtkIconSize
gtk_toolbar_get_icon_size (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), DEFAULT_ICON_SIZE);

  return toolbar->icon_size;
}

/**
 * gtk_toolbar_unset_icon_size:
 * @toolbar: a #GtkToolbar
 * 
 * Unsets toolbar icon size set with gtk_toolbar_set_icon_size(), so that
 * user preferences will be used to determine the icon size.
 **/
void
gtk_toolbar_unset_icon_size (GtkToolbar  *toolbar)
{
  GtkIconSize size;

  if (toolbar->icon_size_set)
    {
      GtkSettings *settings = toolbar_get_settings (toolbar);

      if (settings)
	g_object_get (settings,
		      "gtk-toolbar-icon-size", &size,
		      NULL);
      else
	size = DEFAULT_ICON_SIZE;

      if (size != toolbar->icon_size)
        gtk_toolbar_set_icon_size (toolbar, size);

      toolbar->icon_size_set = FALSE;
    }
}

static gchar *
elide_underscores (const gchar *original)
{
  gchar *q, *result;
  const gchar *p;
  gboolean last_underscore;

  q = result = g_malloc (strlen (original) + 1);
  last_underscore = FALSE;
  
  for (p = original; *p; p++)
    {
      if (!last_underscore && *p == '_')
	last_underscore = TRUE;
      else
	{
	  last_underscore = FALSE;
	  *q++ = *p;
	}
    }
  
  *q = '\0';
  
  return result;
}

/**
 * gtk_toolbar_insert_stock:
 * @toolbar: A #GtkToolbar
 * @stock_id: The id of the stock item you want to insert
 * @tooltip_text: The text in the tooltip of the toolbar button
 * @tooltip_private_text: The private text of the tooltip
 * @callback: The callback called when the toolbar button is clicked.
 * @user_data: user data passed to callback
 * @position: The position the button shall be inserted at.
 *            -1 means at the end.
 *
 * Inserts a stock item at the specified position of the toolbar.  If
 * @stock_id is not a known stock item ID, it's inserted verbatim,
 * except that underscores used to mark mnemonics are removed.
 *
 * Returns: the inserted widget
 */
GtkWidget*
gtk_toolbar_insert_stock (GtkToolbar      *toolbar,
			  const gchar     *stock_id,
			  const char      *tooltip_text,
			  const char      *tooltip_private_text,
			  GtkSignalFunc    callback,
			  gpointer         user_data,
			  gint             position)
{
  GtkStockItem item;
  GtkWidget *image = NULL;
  const gchar *label;
  gchar *label_no_mnemonic;
  GtkWidget *retval;

  if (gtk_stock_lookup (stock_id, &item))
    {
      image = gtk_image_new_from_stock (stock_id, toolbar->icon_size);
      label = item.label;
    }
  else
    label = stock_id;

  label_no_mnemonic = elide_underscores (label);
  
  retval =  gtk_toolbar_internal_insert_item (toolbar,
	   				      label_no_mnemonic,
					      tooltip_text,
					      tooltip_private_text,
					      image,
					      callback,
					      user_data,
					      position);

  g_free (label_no_mnemonic);

  return retval;
}



void
gtk_toolbar_append_space (GtkToolbar *toolbar)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

void
gtk_toolbar_prepend_space (GtkToolbar *toolbar)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      0);
}

void
gtk_toolbar_insert_space (GtkToolbar *toolbar,
			  gint        position)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_SPACE,
			      NULL, NULL,
			      NULL, NULL,
			      NULL, NULL, NULL,
			      position);
}

/**
 * gtk_toolbar_remove_space:
 * @toolbar: a #GtkToolbar.
 * @position: the index of the space to remove.
 * 
 * Removes a space from the specified position.
 **/
void
gtk_toolbar_remove_space (GtkToolbar *toolbar,
                          gint        position)
{
  GList *children;
  GtkToolbarChild *child;
  gint i;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));
  
  i = 0;
  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;

      if (i == position)
        {
          if (child->type == GTK_TOOLBAR_CHILD_SPACE)
            {
              toolbar->children = g_list_remove_link (toolbar->children, children);
              g_free (child);
              g_list_free (children);
              toolbar->num_children--;
              
              gtk_widget_queue_resize (GTK_WIDGET (toolbar));
            }
          else
            {
              g_warning ("Toolbar position %d is not a space", position);
            }

          return;
        }

      ++i;
    }

  g_warning ("Toolbar position %d doesn't exist", position);
}

/**
 * gtk_toolbar_append_widget:
 * @toolbar: a #GtkToolbar.
 * @widget: a #GtkWidget to add to the toolbar. 
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * 
 * Adds a widget to the end of the given toolbar.
 **/ 
void
gtk_toolbar_append_widget (GtkToolbar  *toolbar,
			   GtkWidget   *widget,
			   const gchar *tooltip_text,
			   const gchar *tooltip_private_text)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      toolbar->num_children);
}

/**
 * gtk_toolbar_prepend_widget:
 * @toolbar: a #GtkToolbar.
 * @widget: a #GtkWidget to add to the toolbar. 
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * 
 * Adds a widget to the beginning of the given toolbar.
 **/ 
void
gtk_toolbar_prepend_widget (GtkToolbar  *toolbar,
			    GtkWidget   *widget,
			    const gchar *tooltip_text,
			    const gchar *tooltip_private_text)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      0);
}

/**
 * gtk_toolbar_insert_widget:
 * @toolbar: a #GtkToolbar.
 * @widget: a #GtkWidget to add to the toolbar. 
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * @position: the number of widgets to insert this widget after.
 * 
 * Inserts a widget in the toolbar at the given position.
 **/ 
void
gtk_toolbar_insert_widget (GtkToolbar *toolbar,
			   GtkWidget  *widget,
			   const char *tooltip_text,
			   const char *tooltip_private_text,
			   gint        position)
{
  gtk_toolbar_insert_element (toolbar, GTK_TOOLBAR_CHILD_WIDGET,
			      widget, NULL,
			      tooltip_text, tooltip_private_text,
			      NULL, NULL, NULL,
			      position);
}

GtkWidget*
gtk_toolbar_append_element (GtkToolbar          *toolbar,
			    GtkToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data)
{
  return gtk_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data,
				     toolbar->num_children);
}

GtkWidget *
gtk_toolbar_prepend_element (GtkToolbar          *toolbar,
			     GtkToolbarChildType  type,
			     GtkWidget           *widget,
			     const char          *text,
			     const char          *tooltip_text,
			     const char          *tooltip_private_text,
			     GtkWidget           *icon,
			     GtkSignalFunc        callback,
			     gpointer             user_data)
{
  return gtk_toolbar_insert_element (toolbar, type, widget, text,
				     tooltip_text, tooltip_private_text,
				     icon, callback, user_data, 0);
}

/**
 * gtk_toolbar_insert_element:
 * @toolbar: a #GtkToolbar.
 * @type: a value of type #GtkToolbarChildType that determines what @widget
 *   will be.
 * @widget: a #GtkWidget, or %NULL. 
 * @text: the element's label.
 * @tooltip_text: the element's tooltip.
 * @tooltip_private_text: used for context-sensitive help about this toolbar element.
 * @icon: a #GtkWidget that provides pictorial representation of the element's function.
 * @callback: the function to be executed when the button is pressed.
 * @user_data: any data you wish to pass to the callback.
 * @position: the number of widgets to insert this element after.
 *
 * Inserts a new element in the toolbar at the given position. 
 *
 * If @type == %GTK_TOOLBAR_CHILD_WIDGET, @widget is used as the new element.
 * If @type == %GTK_TOOLBAR_CHILD_RADIOBUTTON, @widget is used to determine
 * the radio group for the new element. In all other cases, @widget must
 * be %NULL.
 *
 * Return value: the new toolbar element as a #GtkWidget.
 **/
GtkWidget *
gtk_toolbar_insert_element (GtkToolbar          *toolbar,
			    GtkToolbarChildType  type,
			    GtkWidget           *widget,
			    const char          *text,
			    const char          *tooltip_text,
			    const char          *tooltip_private_text,
			    GtkWidget           *icon,
			    GtkSignalFunc        callback,
			    gpointer             user_data,
			    gint                 position)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), NULL);
  if (type == GTK_TOOLBAR_CHILD_WIDGET)
    {
      g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
    }
  else if (type != GTK_TOOLBAR_CHILD_RADIOBUTTON)
    g_return_val_if_fail (widget == NULL, NULL);
  
  return gtk_toolbar_internal_insert_element (toolbar, type, widget, text,
                                              tooltip_text, tooltip_private_text,
                                              icon, callback, user_data,
                                              position);
}

static void
set_child_packing_and_visibility(GtkToolbar      *toolbar,
                                 GtkToolbarChild *child)
{
  GtkWidget *box;
  gboolean   expand;

  box = gtk_bin_get_child (GTK_BIN (child->widget));

  g_return_if_fail (GTK_IS_BOX (box));

  if (child->label)
    {
      expand = (toolbar->style != GTK_TOOLBAR_BOTH);

      gtk_box_set_child_packing (GTK_BOX (box), child->label,
                                 expand, expand, 0, GTK_PACK_END);

      if (toolbar->style != GTK_TOOLBAR_ICONS)
        gtk_widget_show (child->label);
      else
        gtk_widget_hide (child->label);
    }

  if (child->icon)
    {
      expand = (toolbar->style != GTK_TOOLBAR_BOTH_HORIZ);

      gtk_box_set_child_packing (GTK_BOX (box), child->icon,
                                 expand, expand, 0, GTK_PACK_END);

      if (toolbar->style != GTK_TOOLBAR_TEXT)
        gtk_widget_show (child->icon);
      else
        gtk_widget_hide (child->icon);
    }
}

static GtkWidget *
gtk_toolbar_internal_insert_element (GtkToolbar          *toolbar,
                                     GtkToolbarChildType  type,
                                     GtkWidget           *widget,
                                     const char          *text,
                                     const char          *tooltip_text,
                                     const char          *tooltip_private_text,
                                     GtkWidget           *icon,
                                     GtkSignalFunc        callback,
                                     gpointer             user_data,
                                     gint                 position)
{
  GtkToolbarChild *child;
  GtkWidget *box;

  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), NULL);
  if (type == GTK_TOOLBAR_CHILD_WIDGET)
    {
      g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
    }
  else if (type != GTK_TOOLBAR_CHILD_RADIOBUTTON)
    g_return_val_if_fail (widget == NULL, NULL);

  if (type == GTK_TOOLBAR_CHILD_SPACE)
    child = (GtkToolbarChild *) g_new (GtkToolbarChildSpace, 1);
  else
    child = g_new (GtkToolbarChild, 1);

  child->type = type;
  child->icon = NULL;
  child->label = NULL;

  switch (type)
    {
    case GTK_TOOLBAR_CHILD_SPACE:
      child->widget = NULL;
      ((GtkToolbarChildSpace *) child)->alloc_x =
	((GtkToolbarChildSpace *) child)->alloc_y = 0;
      break;

    case GTK_TOOLBAR_CHILD_WIDGET:
      child->widget = widget;
      break;

    case GTK_TOOLBAR_CHILD_BUTTON:
    case GTK_TOOLBAR_CHILD_TOGGLEBUTTON:
    case GTK_TOOLBAR_CHILD_RADIOBUTTON:
      if (type == GTK_TOOLBAR_CHILD_BUTTON)
	{
	  child->widget = gtk_button_new ();
	  gtk_button_set_relief (GTK_BUTTON (child->widget), get_button_relief (toolbar));
	}
      else if (type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON)
	{
	  child->widget = gtk_toggle_button_new ();
	  gtk_button_set_relief (GTK_BUTTON (child->widget), get_button_relief (toolbar));
	  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (child->widget),
				      FALSE);
	}
      else
	{
	  child->widget = gtk_radio_button_new (widget
						? gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget))
						: NULL);
	  gtk_button_set_relief (GTK_BUTTON (child->widget), get_button_relief (toolbar));
	  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (child->widget), FALSE);
	}

      GTK_WIDGET_UNSET_FLAGS (child->widget, GTK_CAN_FOCUS);

      if (callback)
	g_signal_connect (child->widget, "clicked",
			  callback, user_data);

      if (toolbar->style == GTK_TOOLBAR_BOTH_HORIZ)
	  box = gtk_hbox_new (FALSE, 0);
      else
	  box = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (child->widget), box);
      gtk_widget_show (box);

      if (text)
	{
	  child->label = gtk_label_new (text);
          gtk_container_add (GTK_CONTAINER (box), child->label);
	}

      if (icon)
	{
	  child->icon = GTK_WIDGET (icon);
          gtk_container_add (GTK_CONTAINER (box), child->icon);
	}

      set_child_packing_and_visibility (toolbar, child);

      /* Mark child as ours */
      g_object_set_data (G_OBJECT (child->widget),
                         "gtk-toolbar-is-child",
                         GINT_TO_POINTER (TRUE));

      gtk_widget_show (child->widget);
      break;

    default:
      g_assert_not_reached ();
    }

  if ((type != GTK_TOOLBAR_CHILD_SPACE) && tooltip_text)
    gtk_tooltips_set_tip (toolbar->tooltips, child->widget,
			  tooltip_text, tooltip_private_text);

  toolbar->children = g_list_insert (toolbar->children, child, position);
  toolbar->num_children++;

  if (type != GTK_TOOLBAR_CHILD_SPACE)
    gtk_widget_set_parent (child->widget, GTK_WIDGET (toolbar));
  else
    gtk_widget_queue_resize (GTK_WIDGET (toolbar));

  return child->widget;
}

void
gtk_toolbar_set_orientation (GtkToolbar     *toolbar,
			     GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  g_signal_emit (toolbar, toolbar_signals[ORIENTATION_CHANGED], 0, orientation);
}

/**
 * gtk_toolbar_get_orientation:
 * @toolbar: a #GtkToolbar
 * 
 * Retrieves the current orientation of the toolbar. See
 * gtk_toolbar_set_orientation().
 *
 * Return value: the orientation
 **/
GtkOrientation
gtk_toolbar_get_orientation (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL);

  return toolbar->orientation;
}

void
gtk_toolbar_set_style (GtkToolbar      *toolbar,
		       GtkToolbarStyle  style)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  toolbar->style_set = TRUE;
  g_signal_emit (toolbar, toolbar_signals[STYLE_CHANGED], 0, style);
}

/**
 * gtk_toolbar_get_style:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves whether the toolbar has text, icons, or both . See
 * gtk_toolbar_set_style().
 
 * Return value: the current style of @toolbar
 **/
GtkToolbarStyle
gtk_toolbar_get_style (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), DEFAULT_TOOLBAR_STYLE);

  return toolbar->style;
}

/**
 * gtk_toolbar_unset_style:
 * @toolbar: a #GtkToolbar
 * 
 * Unsets a toolbar style set with gtk_toolbar_set_style(), so that
 * user preferences will be used to determine the toolbar style.
 **/
void
gtk_toolbar_unset_style (GtkToolbar *toolbar)
{
  GtkToolbarStyle style;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->style_set)
    {
      GtkSettings *settings = toolbar_get_settings (toolbar);

      if (settings)
	g_object_get (settings,
		      "gtk-toolbar-style", &style,
		      NULL);
      else
	style = DEFAULT_TOOLBAR_STYLE;

      if (style != toolbar->style)
        g_signal_emit (toolbar, toolbar_signals[STYLE_CHANGED], 0, style);
      
      toolbar->style_set = FALSE;
    }
}

void
gtk_toolbar_set_tooltips (GtkToolbar *toolbar,
			  gboolean    enable)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (enable)
    gtk_tooltips_enable (toolbar->tooltips);
  else
    gtk_tooltips_disable (toolbar->tooltips);
}

/**
 * gtk_toolbar_get_tooltips:
 * @toolbar: a #GtkToolbar
 *
 * Retrieves whether tooltips are enabled. See
 * gtk_toolbar_set_tooltips().
 *
 * Return value: %TRUE if tooltips are enabled
 **/
gboolean
gtk_toolbar_get_tooltips (GtkToolbar *toolbar)
{
  g_return_val_if_fail (GTK_IS_TOOLBAR (toolbar), FALSE);

  return toolbar->tooltips->enabled;
}

static void
gtk_toolbar_update_button_relief (GtkToolbar *toolbar)
{
  GList *children;
  GtkToolbarChild *child;
  GtkReliefStyle relief;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  relief = get_button_relief (toolbar);
  
  for (children = toolbar->children; children; children = children->next)
    {
      child = children->data;
      if (child->type == GTK_TOOLBAR_CHILD_BUTTON ||
          child->type == GTK_TOOLBAR_CHILD_RADIOBUTTON ||
          child->type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON)
        gtk_button_set_relief (GTK_BUTTON (child->widget), relief);
    }
}

static void
gtk_real_toolbar_orientation_changed (GtkToolbar     *toolbar,
				      GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->orientation != orientation)
    {
      toolbar->orientation = orientation;
      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "orientation");
    }
}

static void
gtk_real_toolbar_style_changed (GtkToolbar      *toolbar,
				GtkToolbarStyle  style)
{
  GList *children;
  GtkToolbarChild *child;
  GtkWidget* box;
  
  g_return_if_fail (GTK_IS_TOOLBAR (toolbar));

  if (toolbar->style != style)
    {
      toolbar->style = style;

      for (children = toolbar->children; children; children = children->next)
	{
	  child = children->data;

	  if (child->type == GTK_TOOLBAR_CHILD_BUTTON ||
	      child->type == GTK_TOOLBAR_CHILD_RADIOBUTTON ||
	      child->type == GTK_TOOLBAR_CHILD_TOGGLEBUTTON)
            {
              box = gtk_bin_get_child (GTK_BIN (child->widget));

              if (style == GTK_TOOLBAR_BOTH && GTK_IS_HBOX (box))
                {
                  GtkWidget *vbox;

                  vbox = gtk_vbox_new (FALSE, 0);

                  if (child->label)
                    gtk_widget_reparent (child->label, vbox);
                  if (child->icon)
                    gtk_widget_reparent (child->icon, vbox);

                  gtk_widget_destroy (box);
                  gtk_container_add (GTK_CONTAINER (child->widget), vbox);

                  gtk_widget_show (vbox);
                }
              else if (style == GTK_TOOLBAR_BOTH_HORIZ && GTK_IS_VBOX (box))
                {
                  GtkWidget *hbox;

                  hbox = gtk_hbox_new (FALSE, 0);

                  if (child->label)
                    gtk_widget_reparent (child->label, hbox);
                  if (child->icon)
                    gtk_widget_reparent (child->icon, hbox);

                  gtk_widget_destroy (box);
                  gtk_container_add (GTK_CONTAINER (child->widget), hbox);

                  gtk_widget_show (hbox);
                }

              set_child_packing_and_visibility (toolbar, child);
            }
	}

      gtk_widget_queue_resize (GTK_WIDGET (toolbar));
      g_object_notify (G_OBJECT (toolbar), "toolbar_style");
    }
}


static GtkReliefStyle
get_button_relief (GtkToolbar *toolbar)
{
  GtkReliefStyle button_relief = GTK_RELIEF_NORMAL;

  gtk_widget_ensure_style (GTK_WIDGET (toolbar));
  gtk_widget_style_get (GTK_WIDGET (toolbar),
			"button_relief", &button_relief,
                        NULL);

  return button_relief;
}

static gint
get_space_size (GtkToolbar *toolbar)
{
  gint space_size = DEFAULT_SPACE_SIZE;

  gtk_widget_style_get (GTK_WIDGET (toolbar),
                        "space_size", &space_size,
                        NULL);

  return space_size;
}

static GtkToolbarSpaceStyle
get_space_style (GtkToolbar *toolbar)
{
  GtkToolbarSpaceStyle space_style = DEFAULT_SPACE_STYLE;

  gtk_widget_style_get (GTK_WIDGET (toolbar),
                        "space_style", &space_style,
                        NULL);


  return space_style;  
}
