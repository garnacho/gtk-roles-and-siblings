/* GTK - The GIMP Toolkit
 * gtksizegroup.c: 
 * Copyright (C) 2001 Red Hat Software
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

#include <config.h>
#include "gtkcontainer.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtksizegroup.h"

enum {
  PROP_0,
  PROP_MODE
};

static void gtk_size_group_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec);
static void gtk_size_group_get_property (GObject      *object,
					 guint         prop_id,
					 GValue       *value,
					 GParamSpec   *pspec);

static void add_group_to_closure  (GtkSizeGroup      *group,
				   GtkSizeGroupMode   mode,
				   GSList           **groups,
				   GSList           **widgets);
static void add_widget_to_closure (GtkWidget         *widget,
				   GtkSizeGroupMode   mode,
				   GSList           **groups,
				   GSList           **widgets);

static GQuark size_groups_quark;
static const gchar size_groups_tag[] = "gtk-size-groups";

static GSList *
get_size_groups (GtkWidget *widget)
{
  if (!size_groups_quark)
    size_groups_quark = g_quark_from_string (size_groups_tag);
  
  return g_object_get_qdata (G_OBJECT (widget), size_groups_quark);
}

static void
set_size_groups (GtkWidget *widget,
		 GSList    *groups)
{
  if (!size_groups_quark)
    size_groups_quark = g_quark_from_string (size_groups_tag);

  g_object_set_qdata (G_OBJECT (widget), size_groups_quark, groups);
}

static void
add_group_to_closure (GtkSizeGroup    *group,
		      GtkSizeGroupMode mode,
		      GSList         **groups,
		      GSList         **widgets)
{
  GSList *tmp_widgets;
  
  *groups = g_slist_prepend (*groups, group);

  tmp_widgets = group->widgets;
  while (tmp_widgets)
    {
      GtkWidget *tmp_widget = tmp_widgets->data;
      
      if (!g_slist_find (*widgets, tmp_widget))
	add_widget_to_closure (tmp_widget, mode, groups, widgets);
      
      tmp_widgets = tmp_widgets->next;
    }
}

static void
add_widget_to_closure (GtkWidget       *widget,
		       GtkSizeGroupMode mode,
		       GSList         **groups,
		       GSList         **widgets)
{
  GSList *tmp_groups;

  *widgets = g_slist_prepend (*widgets, widget);

  tmp_groups = get_size_groups (widget);
  while (tmp_groups)
    {
      GtkSizeGroup *tmp_group = tmp_groups->data;
      
      if ((tmp_group->mode == GTK_SIZE_GROUP_BOTH || tmp_group->mode == mode) &&
	  !g_slist_find (*groups, tmp_group))
	add_group_to_closure (tmp_group, mode, groups, widgets);

      tmp_groups = tmp_groups->next;
    }
}

static void
real_queue_resize (GtkWidget *widget)
{
  GTK_PRIVATE_SET_FLAG (widget, GTK_ALLOC_NEEDED);
  GTK_PRIVATE_SET_FLAG (widget, GTK_REQUEST_NEEDED);
  
  if (widget->parent)
    _gtk_container_queue_resize (GTK_CONTAINER (widget->parent));
  else if (GTK_WIDGET_TOPLEVEL (widget) && GTK_IS_CONTAINER (widget))
    _gtk_container_queue_resize (GTK_CONTAINER (widget));
}

static void
reset_group_sizes (GSList *groups)
{
  GSList *tmp_list = groups;
  while (tmp_list)
    {
      GtkSizeGroup *tmp_group = tmp_list->data;

      tmp_group->have_width = FALSE;
      tmp_group->have_height = FALSE;
      
      tmp_list = tmp_list->next;
    }
}

static void
queue_resize_on_widget (GtkWidget *widget,
			gboolean   check_siblings)
{
  GtkWidget *parent = widget;
  GSList *tmp_list;

  while (parent)
    {
      GSList *widget_groups;
      GSList *groups;
      GSList *widgets;
      
      if (widget == parent && !check_siblings)
	{
	  real_queue_resize (widget);
	  parent = parent->parent;
	  continue;
	}
      
      widget_groups = get_size_groups (parent);
      if (!widget_groups)
	{
	  if (widget == parent)
	    real_queue_resize (widget);

	  parent = parent->parent;
	  continue;
	}

      groups = NULL;
      widgets = NULL;
	  
      add_widget_to_closure (parent, GTK_SIZE_GROUP_HORIZONTAL, &groups, &widgets);
      reset_group_sizes (groups);
	      
      tmp_list = widgets;
      while (tmp_list)
	{
	  if (tmp_list->data == parent)
	    {
	      if (widget == parent)
		real_queue_resize (parent);
	    }
	  else
	    queue_resize_on_widget (tmp_list->data, FALSE);

	  tmp_list = tmp_list->next;
	}
      
      g_slist_free (widgets);
      g_slist_free (groups);
	      
      groups = NULL;
      widgets = NULL;
	      
      add_widget_to_closure (parent, GTK_SIZE_GROUP_VERTICAL, &groups, &widgets);
      reset_group_sizes (groups);
	      
      tmp_list = widgets;
      while (tmp_list)
	{
	  if (tmp_list->data == parent)
	    {
	      if (widget == parent)
		real_queue_resize (parent);
	    }
	  else
	    queue_resize_on_widget (tmp_list->data, FALSE);

	  tmp_list = tmp_list->next;
	}
      
      g_slist_free (widgets);
      g_slist_free (groups);
      
      parent = parent->parent;
    }
}

static void
queue_resize_on_group (GtkSizeGroup *size_group)
{
  if (size_group->widgets)
    queue_resize_on_widget (size_group->widgets->data, TRUE);
}

static void
gtk_size_group_class_init (GtkSizeGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_size_group_set_property;
  gobject_class->get_property = gtk_size_group_get_property;
  
  g_object_class_install_property (gobject_class,
				   PROP_MODE,
				   g_param_spec_enum ("mode",
						      P_("Mode"),
						      P_("The directions in which the size group effects the requested sizes"
							" of its component widgets"),
						      GTK_TYPE_SIZE_GROUP_MODE,
						      GTK_SIZE_GROUP_HORIZONTAL,
						      G_PARAM_READWRITE));
}

static void
gtk_size_group_init (GtkSizeGroup *size_group)
{
  size_group->widgets = NULL;
  size_group->mode = GTK_SIZE_GROUP_HORIZONTAL;
  size_group->have_width = 0;
  size_group->have_height = 0;
}

GType
gtk_size_group_get_type (void)
{
  static GType size_group_type = 0;

  if (!size_group_type)
    {
      static const GTypeInfo size_group_info =
      {
	sizeof (GtkSizeGroupClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_size_group_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkSizeGroup),
	16,		/* n_preallocs */
	(GInstanceInitFunc) gtk_size_group_init,
      };

      size_group_type = g_type_register_static (G_TYPE_OBJECT, "GtkSizeGroup",
						&size_group_info, 0);
    }

  return size_group_type;
}

static void
gtk_size_group_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
  GtkSizeGroup *size_group = GTK_SIZE_GROUP (object);

  switch (prop_id)
    {
    case PROP_MODE:
      gtk_size_group_set_mode (size_group, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_size_group_get_property (GObject      *object,
			     guint         prop_id,
			     GValue       *value,
			     GParamSpec   *pspec)
{
  GtkSizeGroup *size_group = GTK_SIZE_GROUP (object);

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, size_group->mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_size_group_new:
 * @mode: the mode for the new size group.
 * 
 * Create a new #GtkSizeGroup.
 
 * Return value: a newly created #GtkSizeGroup
 **/
GtkSizeGroup *
gtk_size_group_new (GtkSizeGroupMode  mode)
{
  GtkSizeGroup *size_group = g_object_new (GTK_TYPE_SIZE_GROUP, NULL);

  size_group->mode = mode;

  return size_group;
}

/**
 * gtk_size_group_set_mode:
 * @size_group: a #GtkSizeGroup
 * @mode: the mode to set for the size group.
 * 
 * Sets the #GtkSizeGroupMode of the size group. The mode of the size
 * group determines whether the widgets in the size group should
 * all have the same horizontal requisition (%GTK_SIZE_GROUP_MODE_HORIZONTAL)
 * all have the same vertical requisition (%GTK_SIZE_GROUP_MODE_VERTICAL),
 * or should all have the same requisition in both directions
 * (%GTK_SIZE_GROUP_MODE_BOTH).
 **/
void
gtk_size_group_set_mode (GtkSizeGroup     *size_group,
			 GtkSizeGroupMode  mode)
{
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));

  if (size_group->mode != mode)
    {
      if (size_group->mode != GTK_SIZE_GROUP_NONE)
	queue_resize_on_group (size_group);
      size_group->mode = mode;
      if (size_group->mode != GTK_SIZE_GROUP_NONE)
	queue_resize_on_group (size_group);

      g_object_notify (G_OBJECT (size_group), "mode");
    }
}

/**
 * gtk_size_group_get_mode:
 * @size_group: a #GtkSizeGroup
 * 
 * Gets the current mode of the size group. See gtk_size_group_set_mode().
 * 
 * Return value: the current mode of the size group.
 **/
GtkSizeGroupMode
gtk_size_group_get_mode (GtkSizeGroup *size_group)
{
  g_return_val_if_fail (GTK_IS_SIZE_GROUP (size_group), GTK_SIZE_GROUP_BOTH);

  return size_group->mode;
}

static void
gtk_size_group_widget_destroyed (GtkWidget    *widget,
				 GtkSizeGroup *size_group)
{
  gtk_size_group_remove_widget (size_group, widget);
}

/**
 * gtk_size_group_add_widget:
 * @size_group: a #GtkSizeGroup
 * @widget: the #GtkWidget to add
 * 
 * Adds a widget to a #GtkSizeGroup. In the future, the requisition
 * of the widget will be determined as the maximum of its requisition
 * and the requisition of the other widgets in the size group.
 * Whether this applies horizontally, vertically, or in both directions
 * depends on the mode of the size group. See gtk_size_group_set_mode().
 **/
void
gtk_size_group_add_widget (GtkSizeGroup     *size_group,
			   GtkWidget        *widget)
{
  GSList *groups;
  
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  
  groups = get_size_groups (widget);

  if (!g_slist_find (groups, widget))
    {
      groups = g_slist_prepend (groups, size_group);
      set_size_groups (widget, groups);

      size_group->widgets = g_slist_prepend (size_group->widgets, widget);

      g_signal_connect (widget, "destroy",
			G_CALLBACK (gtk_size_group_widget_destroyed),
			size_group);

      g_object_ref (size_group);
    }
  
  queue_resize_on_group (size_group);
}

/**
 * gtk_size_group_remove_widget:
 * @size_group: a #GtkSizeGrup
 * @widget: the #GtkWidget to remove
 * 
 * Removes a widget from a #GtkSizeGroup.
 **/
void
gtk_size_group_remove_widget (GtkSizeGroup     *size_group,
			      GtkWidget        *widget)
{
  GSList *groups;
  
  g_return_if_fail (GTK_IS_SIZE_GROUP (size_group));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (g_slist_find (size_group->widgets, widget));

  g_signal_handlers_disconnect_by_func (widget,
					gtk_size_group_widget_destroyed,
					size_group);
  
  groups = get_size_groups (widget);
  groups = g_slist_remove (groups, size_group);
  set_size_groups (widget, groups);

  size_group->widgets = g_slist_remove (size_group->widgets, widget);
  queue_resize_on_group (size_group);
  gtk_widget_queue_resize (widget);

  g_object_unref (size_group);
}

static gint
get_base_dimension (GtkWidget        *widget,
		    GtkSizeGroupMode  mode)
{
  GtkWidgetAuxInfo *aux_info = _gtk_widget_get_aux_info (widget, FALSE);

  if (mode == GTK_SIZE_GROUP_HORIZONTAL)
    {
      if (aux_info && aux_info->width > 0)
	return aux_info->width;
      else
	return widget->requisition.width;
    }
  else
    {
      if (aux_info && aux_info->height > 0)
	return aux_info->height;
      else
	return widget->requisition.height;
    }
}

static void
do_size_request (GtkWidget *widget)
{
  if (GTK_WIDGET_REQUEST_NEEDED (widget))
    {
      gtk_widget_ensure_style (widget);
      g_signal_emit_by_name (widget,
			     "size_request",
			     &widget->requisition);
      
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_REQUEST_NEEDED);
    }
}

static gint
compute_base_dimension (GtkWidget        *widget,
			GtkSizeGroupMode  mode)
{
  do_size_request (widget);

  return get_base_dimension (widget, mode);
}

static gint
compute_dimension (GtkWidget        *widget,
		   GtkSizeGroupMode  mode)
{
  GSList *widgets = NULL;
  GSList *groups = NULL;
  GSList *tmp_list;
  gint result = 0;

  add_widget_to_closure (widget, mode, &groups, &widgets);

  g_slist_foreach (widgets, (GFunc)g_object_ref, NULL);
  
  if (!groups)
    {
      result = compute_base_dimension (widget, mode);
    }
  else
    {
      GtkSizeGroup *group = groups->data;

      if (mode == GTK_SIZE_GROUP_HORIZONTAL && group->have_width)
	result = group->requisition.width;
      else if (mode == GTK_SIZE_GROUP_VERTICAL && group->have_height)
	result = group->requisition.height;
      else
	{
	  tmp_list = widgets;
	  while (tmp_list)
	    {
	      GtkWidget *tmp_widget = tmp_list->data;

	      gint dimension = compute_base_dimension (tmp_widget, mode);

	      if (dimension > result)
		result = dimension;
	      
	      tmp_list = tmp_list->next;
	    }

	  tmp_list = groups;
	  while (tmp_list)
	    {
	      GtkSizeGroup *tmp_group = tmp_list->data;

	      if (mode == GTK_SIZE_GROUP_HORIZONTAL)
		{
		  tmp_group->have_width = TRUE;
		  tmp_group->requisition.width = result;
		}
	      else
		{
		  tmp_group->have_height = TRUE;
		  tmp_group->requisition.height = result;
		}
	      
	      tmp_list = tmp_list->next;
	    }
	}
    }

  g_slist_foreach (widgets, (GFunc)g_object_unref, NULL);
  
  g_slist_free (widgets);
  g_slist_free (groups);

  return result;
}

static gint
get_dimension (GtkWidget        *widget,
	       GtkSizeGroupMode  mode)
{
  GSList *widgets = NULL;
  GSList *groups = NULL;
  gint result = 0;

  add_widget_to_closure (widget, mode, &groups, &widgets);

  if (!groups)
    {
      result = get_base_dimension (widget, mode);
    }
  else
    {
      GtkSizeGroup *group = groups->data;

      if (mode == GTK_SIZE_GROUP_HORIZONTAL && group->have_width)
	result = group->requisition.width;
      else if (mode == GTK_SIZE_GROUP_VERTICAL && group->have_height)
	result = group->requisition.height;
    }

  g_slist_free (widgets);
  g_slist_free (groups);

  return result;
}

static void
get_fast_child_requisition (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkWidgetAuxInfo *aux_info = _gtk_widget_get_aux_info (widget, FALSE);
  
  *requisition = widget->requisition;
  
  if (aux_info)
    {
      if (aux_info->width > 0)
	requisition->width = aux_info->width;
      if (aux_info && aux_info->height > 0)
	requisition->height = aux_info->height;
    }
}

/**
 * _gtk_size_group_get_child_requisition:
 * @widget: a #GtkWidget
 * @requisition: location to store computed requisition.
 * 
 * Retrieve the "child requisition" of the widget, taking account grouping
 * of the widget's requisition with other widgets.
 **/
void
_gtk_size_group_get_child_requisition (GtkWidget      *widget,
				       GtkRequisition *requisition)
{
  if (requisition)
    {
      if (get_size_groups (widget))
	{
	  requisition->width = get_dimension (widget, GTK_SIZE_GROUP_HORIZONTAL);
	  requisition->height = get_dimension (widget, GTK_SIZE_GROUP_VERTICAL);

	  /* Only do the full computation if we actually have size groups */
	}
      else
	get_fast_child_requisition (widget, requisition);
    }
}

/**
 * _gtk_size_group_compute_requisition:
 * @widget: a #GtkWidget
 * @requisition: location to store computed requisition.
 * 
 * Compute the requisition of a widget taking into account grouping of
 * the widget's requisition with other widgets.
 **/
void
_gtk_size_group_compute_requisition (GtkWidget      *widget,
				     GtkRequisition *requisition)
{
  gint width;
  gint height;

  if (get_size_groups (widget))
    {
      /* Only do the full computation if we actually have size groups */
      
      width = compute_dimension (widget, GTK_SIZE_GROUP_HORIZONTAL);
      height = compute_dimension (widget, GTK_SIZE_GROUP_VERTICAL);

      if (requisition)
	{
	  requisition->width = width;
	  requisition->height = height;
	}
    }
  else
    {
      do_size_request (widget);
      
      if (requisition)
	get_fast_child_requisition (widget, requisition);
    }
}

/**
 * _gtk_size_group_queue_resize:
 * @widget: a #GtkWidget
 * 
 * Queue a resize on a widget, and on all other widgets grouped with this widget.
 **/
void
_gtk_size_group_queue_resize (GtkWidget *widget)
{
  queue_resize_on_widget (widget, TRUE);
}
