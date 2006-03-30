/* GTK - The GIMP Toolkit
 * gtkrecentchooser.c - Abstract interface for recent file selectors GUIs
 *
 * Copyright (C) 2006, Emmanuele Bassi
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

#include "config.h"

#include "gtkrecentchooser.h"
#include "gtkrecentchooserprivate.h"
#include "gtkrecentmanager.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkalias.h"


enum
{
  ITEM_ACTIVATED,
  SELECTION_CHANGED,

  LAST_SIGNAL
};

static void gtk_recent_chooser_class_init (gpointer g_iface);

static guint chooser_signals[LAST_SIGNAL] = { 0, };

GType
gtk_recent_chooser_get_type (void)
{
  static GType chooser_type = 0;
  
  if (!chooser_type)
    {
      static const GTypeInfo chooser_info =
      {
        sizeof (GtkRecentChooserIface),
        NULL, /* base_init */
        NULL, /* base_finalize */
        (GClassInitFunc) gtk_recent_chooser_class_init,
      };
      
      chooser_type = g_type_register_static (G_TYPE_INTERFACE,
      					     I_("GtkRecentChooser"),
      					     &chooser_info, 0);
      					     
      g_type_interface_add_prerequisite (chooser_type, GTK_TYPE_OBJECT);
    }
  
  return chooser_type;
}

static void
gtk_recent_chooser_class_init (gpointer g_iface)
{
  GType iface_type = G_TYPE_FROM_INTERFACE (g_iface);
  
  /**
   * GtkRecentChooser::selection-changed
   * @chooser: the object which received the signal
   *
   * This signal is emitted when there is a change in the set of
   * selected recently used resources.  This can happen when a user
   * modifies the selection with the mouse or the keyboard, or when
   * explicitely calling functions to change the selection.
   *
   * Since: 2.10
   */
  chooser_signals[SELECTION_CHANGED] =
    g_signal_new (I_("selection-changed"),
                  iface_type,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkRecentChooserIface, selection_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
   
  /**
   * GtkRecentChooser::item-activated
   * @chooser: the object which received the signal
   *
   * This signal is emitted when the user "activates" a recent item
   * in the recent chooser.  This can happen by double-clicking on an item
   * in the recently used resources list, or by pressing
   * <keycap>Enter</keycap>.
   *
   * Since: 2.10
   */
  chooser_signals[ITEM_ACTIVATED] =
    g_signal_new (I_("item-activated"),
                  iface_type,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkRecentChooserIface, item_activated),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
 
  g_object_interface_install_property (g_iface,
  				       g_param_spec_object ("recent-manager",
  				       			    P_("Recent Manager"),
  				       			    P_("The RecentManager object to use"),
  				       			    GTK_TYPE_RECENT_MANAGER,
  				       			    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_interface_install_property (g_iface,
   				       g_param_spec_boolean ("show-private",
   							     P_("Show Private"),
   							     P_("Whether the private items should be displayed"),
   							     FALSE,
   							     G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
  				       g_param_spec_boolean ("show-tips",
  				       			     P_("Show Tooltips"),
  				       			     P_("Whether there should be a tooltip on the item"),
  				       			     FALSE,
  				       			     G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
  				       g_param_spec_boolean ("show-icons",
  				       			     P_("Show Icons"),
  				       			     P_("Whether there should be an icon near the item"),
  				       			     TRUE,
  				       			     G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
  				       g_param_spec_boolean ("show-not-found",
  				       			     P_("Show Not Found"),
  				       			     P_("Whether the items pointing to unavailable resources should be displayed"),
  				       			     FALSE,
  				       			     G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
   				       g_param_spec_boolean ("select-multiple",
   							     P_("Select Multiple"),
   							     P_("Whether to allow multiple items to be selected"),
   							     FALSE,
   							     G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
		  		       g_param_spec_boolean ("local-only",
					       		     P_("Local only"),
							     P_("Whether the selected resource(s) should be limited to local file: URIs"),
							     TRUE,
							     G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
   				       g_param_spec_int ("limit",
   							 P_("Limit"),
   							 P_("The maximum number of items to be displayed"),
   							 -1,
   							 G_MAXINT,
   							 -1,
   							 G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
		  		       g_param_spec_enum ("sort-type",
					       		  P_("Sort Type"),
							  P_("The sorting order of the items displayed"),
							  GTK_TYPE_RECENT_SORT_TYPE,
							  GTK_RECENT_SORT_NONE,
							  G_PARAM_READWRITE));
  g_object_interface_install_property (g_iface,
  				       g_param_spec_object ("filter",
  				       			    P_("Filter"),
  				       			    P_("The current filter for selecting which resources are displayed"),
  				       			    GTK_TYPE_RECENT_FILTER,
  				       			    G_PARAM_READWRITE));
}

GQuark
gtk_recent_chooser_error_quark (void)
{
  static GQuark error_quark = 0;
  if (!error_quark)
    error_quark = g_quark_from_static_string ("gtk-recent-chooser-error-quark");
  return error_quark;
}

/**
 * _gtk_recent_chooser_get_recent_manager:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the #GtkRecentManager used by @chooser.
 *
 * Return value: the recent manager for @chooser.
 *
 * Since: 2.10
 */
GtkRecentManager *
_gtk_recent_chooser_get_recent_manager (GtkRecentChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->get_recent_manager (chooser);
}

/**
 * gtk_recent_chooser_set_show_private:
 * @chooser: a #GtkRecentChooser
 * @show_private: %TRUE to show private items, %FALSE otherwise
 *
 * Whether to show recently used resources marked registered as private.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_show_private (GtkRecentChooser *chooser,
				     gboolean          show_private)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "show-private", show_private, NULL);
}

/**
 * gtk_recent_chooser_get_show_private:
 * @chooser: a #GtkRecentChooser
 *
 * Returns whether @chooser should display recently used resources
 * registered as private.
 *
 * Return value: %TRUE if the recent chooser should show private items,
 *   %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_show_private (GtkRecentChooser *chooser)
{
  gboolean show_private;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "show-private", &show_private, NULL);
  
  return show_private;
}

/**
 * gtk_recent_chooser_set_show_not_found:
 * @chooser: a #GtkRecentChooser
 * @show_not_found: whether to show the local items we didn't find
 *
 * Sets whether @chooser should display the recently used resources that
 * it didn't find.  This only applies to local resources.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_show_not_found (GtkRecentChooser *chooser,
				       gboolean          show_not_found)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "show-not-found", show_not_found, NULL);
}

/**
 * gtk_recent_chooser_get_show_not_found:
 * @chooser: a #GtkRecentChooser
 *
 * Retrieves whether @chooser should show the recently used resources that
 * were not found.
 *
 * Return value: %TRUE if the resources not found should be displayed, and
 *   %FALSE otheriwse.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_show_not_found (GtkRecentChooser *chooser)
{
  gboolean show_not_found;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "show-not-found", &show_not_found, NULL);
  
  return show_not_found;
}

/**
 * gtk_recent_chooser_set_show_icons:
 * @chooser: a #GtkRecentChooser
 * @show_icons: whether to show an icon near the resource
 *
 * Sets whether @chooser should show an icon near the resource when
 * displaying it.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_show_icons (GtkRecentChooser *chooser,
				   gboolean          show_icons)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "show-icons", show_icons, NULL);
}

/**
 * gtk_recent_chooser_get_show_icons:
 * @chooser: a #GtkRecentChooser
 *
 * Retrieves whether @chooser should show an icon near the resource.
 *
 * Return value: %TRUE if the icons should be displayed, %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_show_icons (GtkRecentChooser *chooser)
{
  gboolean show_icons;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "show-icons", &show_icons, NULL);
  
  return show_icons;
}

/**
 * gtk_recent_chooser_set_select_multiple:
 * @chooser: a #GtkRecentChooser
 * @select_multiple: %TRUE if @chooser can select more than one item
 *
 * Sets whether @chooser can select multiple items.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_select_multiple (GtkRecentChooser *chooser,
					gboolean          select_multiple)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "select-multiple", select_multiple, NULL);
}

/**
 * gtk_recent_chooser_get_select_multiple:
 * @chooser: a #GtkRecentChooser
 *
 * Gets whether @chooser can select multiple items.
 *
 * Return value: %TRUE if @chooser can select more than one item.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_select_multiple (GtkRecentChooser *chooser)
{
  gboolean select_multiple;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "select-multiple", &select_multiple, NULL);
  
  return select_multiple;
}

/**
 * gtk_recent_chooser_set_local_only:
 * @chooser: a #GtkRecentChooser
 * @local_only: %TRUE if only local files can be shown
 * 
 * Sets whether only local resources, that is resources using the file:// URI
 * scheme, should be shown in the recently used resources selector.  If
 * @local_only is %TRUE (the default) then the shown resources are guaranteed
 * to be accessible through the operating system native file system.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_local_only (GtkRecentChooser *chooser,
				   gboolean          local_only)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));

  g_object_set (chooser, "local-only", local_only, NULL);
}

/**
 * gtk_recent_chooser_get_local_only:
 * @chooser: a #GtkRecentChooser
 *
 * Gets whether only local resources should be shown in the recently used
 * resources selector.  See gtk_recent_chooser_set_local_only()
 *
 * Return value: %TRUE if only local resources should be shown.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_local_only (GtkRecentChooser *chooser)
{
  gboolean local_only;

  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);

  g_object_get (chooser, "local-only", &local_only, NULL);

  return local_only;
}

/**
 * gtk_recent_chooser_set_limit:
 * @chooser: a #GtkRecentChooser
 * @limit: a positive integer, or -1 for all items
 *
 * Sets the number of items that should be returned by
 * gtk_recent_chooser_get_items() and gtk_recent_chooser_get_uris().
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_limit (GtkRecentChooser *chooser,
			      gint              limit)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "limit", limit, NULL);
}

/**
 * gtk_recent_chooser_get_limit:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the number of items returned by gtk_recent_chooser_get_items()
 * and gtk_recent_chooser_get_uris().
 *
 * Return value: A positive integer, or -1 meaning that all items are
 *   returned.
 *
 * Since: 2.10
 */
gint
gtk_recent_chooser_get_limit (GtkRecentChooser *chooser)
{
  gint limit;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), 10);
  
  g_object_get (chooser, "limit", &limit, NULL);
  
  return limit;
}

/**
 * gtk_recent_chooser_set_show_tips:
 * @chooser: a #GtkRecentChooser
 * @show_tips: %TRUE if tooltips should be shown
 *
 * Sets whether to show a tooltips on the widget.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_show_tips (GtkRecentChooser *chooser,
				  gboolean          show_tips)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "show-tips", show_tips, NULL);
}

/**
 * gtk_recent_chooser_get_show_tips:
 * @chooser: a #GtkRecentChooser
 *
 * Gets whether @chooser should display tooltips.
 *
 * Return value: %TRUE if the recent chooser should show tooltips,
 *   %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_show_tips (GtkRecentChooser *chooser)
{
  gboolean show_tips;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "show-tips", &show_tips, NULL);
  
  return show_tips;
}

/**
 * gtk_recent_chooser_set_show_numbers:
 * @chooser: a #GtkRecentChooser
 * @show_numbers: %TRUE to show numbers, %FALSE otherwise
 *
 * Whether to show recently used resources prepended by a unique number.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_show_numbers (GtkRecentChooser *chooser,
				     gboolean          show_numbers)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "show-numbers", show_numbers, NULL);
}

/**
 * gtk_recent_chooser_get_show_numbers:
 * @chooser: a #GtkRecentChooser
 *
 * Returns whether @chooser should display recently used resources
 * prepended by a unique number.
 *
 * Return value: %TRUE if the recent chooser should show display numbers,
 *   %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_get_show_numbers (GtkRecentChooser *chooser)
{
  gboolean show_numbers;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  g_object_get (chooser, "show-numbers", &show_numbers, NULL);
  
  return show_numbers;
}


/**
 * gtk_recent_chooser_set_sort_type:
 * @chooser: a #GtkRecentChooser
 * @sort_type: sort order that the chooser should use
 *
 * Changes the sorting order of the recently used resources list displayed by
 * @chooser.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_sort_type (GtkRecentChooser  *chooser,
				  GtkRecentSortType  sort_type)
{  
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (chooser, "sort-type", sort_type, NULL);
}

/**
 * gtk_recent_chooser_get_sort_type:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the value set by gtk_recent_chooser_set_sort_type().
 *
 * Return value: the sorting order of the @chooser.
 *
 * Since: 2.10
 */
GtkRecentSortType
gtk_recent_chooser_get_sort_type (GtkRecentChooser *chooser)
{
  GtkRecentSortType sort_type;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), GTK_RECENT_SORT_NONE);
  
  g_object_get (chooser, "sort-type", &sort_type, NULL);

  return sort_type;
}

/**
 * gtk_recent_chooser_set_sort_func:
 * @chooser: a #GtkRecentChooser
 * @sort_func: the comparison function
 * @sort_data: user data to pass to @sort_func, or %NULL
 * @data_destroy: destroy notifier for @sort_data, or %NULL
 *
 * Sets the comparison function used when sorting to be @sort_func.  If
 * the @chooser has the sort type set to #GTK_RECENT_SORT_CUSTOM then
 * the chooser will sort using this function.
 *
 * To the comparison function will be passed two #GtkRecentInfo structs and
 * @sort_data;  @sort_func should return a positive integer if the first
 * item comes before the second, zero if the two items are equal and
 * a negative integer if the first item comes after the second.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_sort_func  (GtkRecentChooser  *chooser,
				   GtkRecentSortFunc  sort_func,
				   gpointer           sort_data,
				   GDestroyNotify     data_destroy)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->set_sort_func (chooser,
  							 sort_func,
  							 sort_data,
  							 data_destroy);
}

/**
 * gtk_recent_chooser_set_current_uri:
 * @chooser: a #GtkRecentChooser
 * @uri: a URI
 * @error: return location for a #GError, or %NULL
 *
 * Sets @uri as the current URI for @chooser.
 *
 * Return value: %TRUE if the URI was found.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_set_current_uri (GtkRecentChooser  *chooser,
				    const gchar       *uri,
				    GError           **error)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->set_current_uri (chooser, uri, error);
}

/**
 * gtk_recent_chooser_get_current_uri:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the URI currently selected by @chooser.
 *
 * Return value: a newly allocated string holding a URI.
 *
 * Since: 2.10
 */
gchar *
gtk_recent_chooser_get_current_uri (GtkRecentChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->get_current_uri (chooser);
}

/**
 * gtk_recent_chooser_get_current_item:
 * @chooser: a #GtkRecentChooser
 * 
 * Gets the #GtkRecentInfo currently selected by @chooser.
 *
 * Return value: a #GtkRecentInfo.  Use gtk_recent_info_unref() when
 *   when you have finished using it.
 *
 * Since: 2.10
 */
GtkRecentInfo *
gtk_recent_chooser_get_current_item (GtkRecentChooser *chooser)
{
  GtkRecentManager *manager;
  GtkRecentInfo *retval;
  gchar *uri;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  uri = gtk_recent_chooser_get_current_uri (chooser);
  if (!uri)
    return NULL;
  
  manager = _gtk_recent_chooser_get_recent_manager (chooser);
  retval = gtk_recent_manager_lookup_item (manager, uri, NULL);
  g_free (uri);
  
  return retval;
}

/**
 * gtk_recent_chooser_select_uri:
 * @chooser: a #GtkRecentChooser
 * @uri: a URI
 * @error: return location for a #GError, or %NULL
 *
 * Selects @uri inside @chooser.
 *
 * Return value: %TRUE if @uri was found.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_chooser_select_uri (GtkRecentChooser  *chooser,
			       const gchar       *uri,
			       GError           **error)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), FALSE);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->select_uri (chooser, uri, error);
}

/**
 * gtk_recent_chooser_unselect_uri:
 * @chooser: a #GtkRecentChooser
 * @uri: a URI
 *
 * Unselects @uri inside @chooser.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_unselect_uri (GtkRecentChooser *chooser,
				 const gchar      *uri)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->unselect_uri (chooser, uri);
}

/**
 * gtk_recent_chooser_select_all:
 * @chooser: a #GtkRecentChooser
 *
 * Selects all the items inside @chooser, if the @chooser supports
 * multiple selection.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_select_all (GtkRecentChooser *chooser)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->select_all (chooser);
}

/**
 * gtk_recent_chooser_unselect_all:
 * @chooser: a #GtkRecentChooser
 *
 * Unselects all the items inside @chooser.
 * 
 * Since: 2.10
 */
void
gtk_recent_chooser_unselect_all (GtkRecentChooser *chooser)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->unselect_all (chooser);
}

/**
 * gtk_recent_chooser_get_items:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the list of recently used resources in form of #GtkRecentInfo objects.
 *
 * The return value of this function is affected by the "sort-type" and
 * "limit" properties of @chooser.
 *
 * Return value: A newly allocated list of #GtkRecentInfo objects.  You should
 *   use gtk_recent_info_unref() on every item of the list, and then free
 *   the list itself using g_list_free().
 *
 * Since: 2.10
 */
GList *
gtk_recent_chooser_get_items (GtkRecentChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->get_items (chooser);
}

/**
 * gtk_recent_chooser_get_uris:
 * @chooser: a #GtkRecentChooser
 * @length: return location for a the length of the URI list, or %NULL
 *
 * Gets the URI of the recently used resources.
 *
 * The return value of this function is affected by the "sort-type" and "limit"
 * properties of @chooser.
 *
 * Since the returned array is %NULL terminated, @length may be %NULL.
 * 
 * Return value: A newly allocated, %NULL terminated array of strings. Use
 *   g_strfreev() to free it.
 *
 * Since: 2.10
 */
gchar **
gtk_recent_chooser_get_uris (GtkRecentChooser *chooser,
                             gsize            *length)
{
  GList *items, *l;
  gchar **retval;
  gsize n_items, i;
  
  items = gtk_recent_chooser_get_items (chooser);
  if (!items)
    return NULL;
  
  n_items = g_list_length (items);
  retval = g_new0 (gchar *, n_items + 1);
  
  for (l = items, i = 0; l != NULL; l = l->next)
    {
      GtkRecentInfo *info = (GtkRecentInfo *) l->data;
      const gchar *uri;
      
      g_assert (info != NULL);
      
      uri = gtk_recent_info_get_uri (info);
      g_assert (uri != NULL);
      
      retval[i++] = g_strdup (uri);
    }
  retval[i] = NULL;
  
  if (length)
    *length = i;
  
  g_list_foreach (items,
  		  (GFunc) gtk_recent_info_unref,
  		  NULL);
  g_list_free (items);
  
  return retval;
}

/**
 * gtk_recent_chooser_add_filter:
 * @chooser: a #GtkRecentChooser
 * @filter: a #GtkRecentFilter
 *
 * Adds @filter to the list of #GtkRecentFilter objects held by @chooser.
 *
 * If no previous filter objects were defined, this function will call
 * gtk_recent_chooser_set_filter().
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_add_filter (GtkRecentChooser *chooser,
			       GtkRecentFilter  *filter)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->add_filter (chooser, filter);
}

/**
 * gtk_recent_chooser_remove_filter:
 * @chooser: a #GtkRecentChooser
 * @filter: a #GtkRecentFilter
 *
 * Removes @filter from the list of #GtkRecentFilter objects held by @chooser.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_remove_filter (GtkRecentChooser *chooser,
				  GtkRecentFilter  *filter)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  GTK_RECENT_CHOOSER_GET_IFACE (chooser)->remove_filter (chooser, filter);
}

/**
 * gtk_recent_chooser_list_filters:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the #GtkRecentFilter objects held by @chooser.
 *
 * Return value: A singly linked list of #GtkRecentFilter objects.  You
 *   should just free the returned list using g_slist_free().
 *
 * Since: 2.10
 */
GSList *
gtk_recent_chooser_list_filters (GtkRecentChooser *chooser)
{
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  return GTK_RECENT_CHOOSER_GET_IFACE (chooser)->list_filters (chooser);
}

/**
 * gtk_recent_chooser_set_filter:
 * @chooser: a #GtkRecentChooser
 * @filter: a #GtkRecentFilter
 *
 * Sets @filter as the current #GtkRecentFilter object used by @chooser
 * to affect the displayed recently used resources.
 *
 * Since: 2.10
 */
void
gtk_recent_chooser_set_filter (GtkRecentChooser *chooser,
			       GtkRecentFilter  *filter)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_object_set (G_OBJECT (chooser), "filter", filter, NULL);
}

/**
 * gtk_recent_chooser_get_filter:
 * @chooser: a #GtkRecentChooser
 *
 * Gets the #GtkRecentFilter object currently used by @chooser to affect
 * the display of the recently used resources.
 *
 * Return value: a #GtkRecentFilter object.
 *
 * Since: 2.10
 */
GtkRecentFilter *
gtk_recent_chooser_get_filter (GtkRecentChooser *chooser)
{
  GtkRecentFilter *filter;
  
  g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);
  
  g_object_get (G_OBJECT (chooser), "filter", &filter, NULL);
  
  /* we need this hack because g_object_get() increases the refcount
   * of the returned object; see also gtk_file_chooser_get_filter()
   * inside gtkfilechooser.c
   */
  if (filter)
    g_object_unref (filter);
  
  return filter;
}

void
_gtk_recent_chooser_item_activated (GtkRecentChooser *chooser)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));
  
  g_signal_emit (chooser, chooser_signals[ITEM_ACTIVATED], 0);
}

void
_gtk_recent_chooser_selection_changed (GtkRecentChooser *chooser)
{
  g_return_if_fail (GTK_IS_RECENT_CHOOSER (chooser));

  g_signal_emit (chooser, chooser_signals[SELECTION_CHANGED], 0);
}

#define __GTK_RECENT_CHOOSER_C__
#include "gtkaliasdef.c"
