/* gtktoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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
#include "gtktoolitem.h"
#include "gtkmarshalers.h"
#include "gtktoolbar.h"
#include "gtkseparatormenuitem.h"
#include "gtkintl.h"
#include "gtkmain.h"

#include <string.h>

enum {
  CREATE_MENU_PROXY,
  TOOLBAR_RECONFIGURED,
  SET_TOOLTIP,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_VISIBLE_HORIZONTAL,
  PROP_VISIBLE_VERTICAL,
  PROP_IS_IMPORTANT
};

#define GTK_TOOL_ITEM_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_TOOL_ITEM, GtkToolItemPrivate))

struct _GtkToolItemPrivate
{
  gchar *tip_text;
  gchar *tip_private;

  guint visible_horizontal : 1;
  guint visible_vertical : 1;
  guint homogeneous : 1;
  guint expand : 1;
  guint use_drag_window : 1;
  guint is_important : 1;

  GdkWindow *drag_window;
  
  gchar *menu_item_id;
  GtkWidget *menu_item;
};
  
static void gtk_tool_item_init       (GtkToolItem *toolitem);
static void gtk_tool_item_class_init (GtkToolItemClass *class);
static void gtk_tool_item_finalize    (GObject *object);
static void gtk_tool_item_parent_set   (GtkWidget   *toolitem,
				        GtkWidget   *parent);
static void gtk_tool_item_set_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec);
static void gtk_tool_item_get_property (GObject         *object,
					guint            prop_id,
					GValue          *value,
					GParamSpec      *pspec);
static void gtk_tool_item_property_notify (GObject      *object,
					   GParamSpec   *pspec);
static void gtk_tool_item_realize       (GtkWidget      *widget);
static void gtk_tool_item_unrealize     (GtkWidget      *widget);
static void gtk_tool_item_map           (GtkWidget      *widget);
static void gtk_tool_item_unmap         (GtkWidget      *widget);
static void gtk_tool_item_size_request  (GtkWidget      *widget,
					 GtkRequisition *requisition);
static void gtk_tool_item_size_allocate (GtkWidget      *widget,
					 GtkAllocation  *allocation);
static gboolean gtk_tool_item_real_set_tooltip (GtkToolItem *tool_item,
						GtkTooltips *tooltips,
						const gchar *tip_text,
						const gchar *tip_private);

static gboolean gtk_tool_item_create_menu_proxy (GtkToolItem *item);


static GObjectClass *parent_class = NULL;
static guint         toolitem_signals[LAST_SIGNAL] = { 0 };

GType
gtk_tool_item_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
	{
	  sizeof (GtkToolItemClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gtk_tool_item_class_init,
	  (GClassFinalizeFunc) NULL,
	  NULL,
        
	  sizeof (GtkToolItem),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) gtk_tool_item_init,
	};

      type = g_type_register_static (GTK_TYPE_BIN,
				     "GtkToolItem",
				     &type_info, 0);
    }
  return type;
}

static void
gtk_tool_item_class_init (GtkToolItemClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  parent_class = g_type_class_peek_parent (klass);
  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  
  object_class->set_property = gtk_tool_item_set_property;
  object_class->get_property = gtk_tool_item_get_property;
  object_class->finalize = gtk_tool_item_finalize;
  object_class->notify = gtk_tool_item_property_notify;

  widget_class->realize       = gtk_tool_item_realize;
  widget_class->unrealize     = gtk_tool_item_unrealize;
  widget_class->map           = gtk_tool_item_map;
  widget_class->unmap         = gtk_tool_item_unmap;
  widget_class->size_request  = gtk_tool_item_size_request;
  widget_class->size_allocate = gtk_tool_item_size_allocate;
  widget_class->parent_set    = gtk_tool_item_parent_set;

  klass->create_menu_proxy = gtk_tool_item_create_menu_proxy;
  klass->set_tooltip       = gtk_tool_item_real_set_tooltip;
  
  g_object_class_install_property (object_class,
				   PROP_VISIBLE_HORIZONTAL,
				   g_param_spec_boolean ("visible_horizontal",
							 P_("Visible when horizontal"),
							 P_("Whether the toolbar item is visible when the toolbar is in a horizontal orientation."),
							 TRUE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_VISIBLE_VERTICAL,
				   g_param_spec_boolean ("visible_vertical",
							 P_("Visible when vertical"),
							 P_("Whether the toolbar item is visible when the toolbar is in a vertical orientation."),
							 TRUE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
 				   PROP_IS_IMPORTANT,
 				   g_param_spec_boolean ("is_important",
 							 P_("Is important"),
 							 P_("Whether the toolbar item is considered important. When TRUE, toolbar buttons show text in GTK_TOOLBAR_BOTH_HORIZ mode"),
 							 FALSE,
 							 G_PARAM_READWRITE));

/**
 * GtkToolItem::create-menu-proxy:
 * @toolitem: the object the signal was emitted on
 *
 * This signal is emitted when the toolbar is displaying an overflow menu.
 * In response the tool item should either 
 * <itemizedlist>
 * <listitem> call gtk_tool_item_set_proxy_menu_item() with a %NULL
 * pointer and return %TRUE to indicate that the item should not appear
 * in the overflow menu
 * </listitem>
 * <listitem> call gtk_tool_item_set_proxy_menu_item() with a new menu
 * item and return %TRUE, or 
 * </listitem>
 * <listitem> return %FALSE to indicate that the signal was not
 * handled by the item. This means that
 * the item will not appear in the overflow menu unless a later handler
 * installs a menu item.
 * </listitem>
 * </itemizedlist>
 * 
 * Return value: %TRUE if the signal was handled, %FALSE if not
 **/
  toolitem_signals[CREATE_MENU_PROXY] =
    g_signal_new ("create_menu_proxy",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkToolItemClass, create_menu_proxy),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

/**
 * GtkToolItem::toolbar-reconfigured:
 * @toolitem: the object the signal was emitted on
 *
 * This signal is emitted when some property of the toolbar that the
 * item is a child of changes. For custom subclasses of #GtkToolItem,
 * the default handler of this signal use the functions
 * <itemizedlist>
 * <listitem>gtk_toolbar_get_orientation()</listitem>
 * <listitem>gtk_toolbar_get_style()</listitem>
 * <listitem>gtk_toolbar_get_icon_size()</listitem>
 * <listitem>gtk_toolbar_get_relief_style()</listitem>
 * </itemizedlist>
 * to find out what the toolbar should look like and change
 * themselves accordingly.
 **/
  toolitem_signals[TOOLBAR_RECONFIGURED] =
    g_signal_new ("toolbar_reconfigured",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkToolItemClass, toolbar_reconfigured),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
/**
 * GtkToolItem::set-tooltip:
 * @toolitem: the object the signal was emitted on
 * @tooltips: the #GtkTooltips
 * @tip_text: the tooltip text
 * @tip_private: the tooltip private text
 *
 * This signal is emitted when the toolitem's tooltip changes.
 * Application developers can use gtk_tool_item_set_tooltip() to
 * set the item's tooltip.
 *
 * Return value: %TRUE if the signal was handled, %FALSE if not
 **/
  toolitem_signals[SET_TOOLTIP] =
    g_signal_new ("set_tooltip",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkToolItemClass, set_tooltip),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_STRING_STRING,
		  G_TYPE_BOOLEAN, 3,
		  GTK_TYPE_TOOLTIPS,
		  G_TYPE_STRING,
		  G_TYPE_STRING);		  

  g_type_class_add_private (object_class, sizeof (GtkToolItemPrivate));
}

static void
gtk_tool_item_init (GtkToolItem *toolitem)
{
  GTK_WIDGET_UNSET_FLAGS (toolitem, GTK_CAN_FOCUS);  

  toolitem->priv = GTK_TOOL_ITEM_GET_PRIVATE (toolitem);

  toolitem->priv->visible_horizontal = TRUE;
  toolitem->priv->visible_vertical = TRUE;
  toolitem->priv->homogeneous = FALSE;
  toolitem->priv->expand = FALSE;
}

static void
gtk_tool_item_finalize (GObject *object)
{
  GtkToolItem *item = GTK_TOOL_ITEM (object);

  if (item->priv->menu_item_id)
    g_free (item->priv->menu_item_id);
  
  if (item->priv->menu_item)
    g_object_unref (item->priv->menu_item);
  
  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_tool_item_parent_set   (GtkWidget   *toolitem,
			    GtkWidget   *prev_parent)
{
  _gtk_tool_item_toolbar_reconfigured (GTK_TOOL_ITEM (toolitem));
}

static void
gtk_tool_item_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_HORIZONTAL:
      gtk_tool_item_set_visible_horizontal (toolitem, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE_VERTICAL:
      gtk_tool_item_set_visible_vertical (toolitem, g_value_get_boolean (value));
      break;
    case PROP_IS_IMPORTANT:
      gtk_tool_item_set_is_important (toolitem, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_tool_item_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_HORIZONTAL:
      g_value_set_boolean (value, toolitem->priv->visible_horizontal);
      break;
    case PROP_VISIBLE_VERTICAL:
      g_value_set_boolean (value, toolitem->priv->visible_vertical);
      break;
    case PROP_IS_IMPORTANT:
      g_value_set_boolean (value, toolitem->priv->is_important);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_tool_item_property_notify (GObject    *object,
			       GParamSpec *pspec)
{
  GtkToolItem *tool_item = GTK_TOOL_ITEM (object);

  if (tool_item->priv->menu_item && strcmp (pspec->name, "sensitive") == 0)
    gtk_widget_set_sensitive (tool_item->priv->menu_item,
			      GTK_WIDGET_SENSITIVE (tool_item));
}

static void
create_drag_window (GtkToolItem *toolitem)
{
  GtkWidget *widget;
  GdkWindowAttr attributes;
  gint attributes_mask, border_width;

  g_return_if_fail (toolitem->priv->use_drag_window == TRUE);

  widget = GTK_WIDGET (toolitem);
  border_width = GTK_CONTAINER (toolitem)->border_width;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  toolitem->priv->drag_window = gdk_window_new (gtk_widget_get_parent_window (widget),
					  &attributes, attributes_mask);
  gdk_window_set_user_data (toolitem->priv->drag_window, toolitem);
}

static void
gtk_tool_item_realize (GtkWidget *widget)
{
  GtkToolItem *toolitem;

  toolitem = GTK_TOOL_ITEM (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  widget->window = gtk_widget_get_parent_window (widget);
  g_object_ref (widget->window);

  if (toolitem->priv->use_drag_window)
    create_drag_window(toolitem);

  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
destroy_drag_window (GtkToolItem *toolitem)
{
  if (toolitem->priv->drag_window)
    {
      gdk_window_set_user_data (toolitem->priv->drag_window, NULL);
      gdk_window_destroy (toolitem->priv->drag_window);
      toolitem->priv->drag_window = NULL;
    }
}

static void
gtk_tool_item_unrealize (GtkWidget *widget)
{
  GtkToolItem *toolitem;

  toolitem = GTK_TOOL_ITEM (widget);

  destroy_drag_window (toolitem);
  
  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
gtk_tool_item_map (GtkWidget *widget)
{
  GtkToolItem *toolitem;

  toolitem = GTK_TOOL_ITEM (widget);
  GTK_WIDGET_CLASS (parent_class)->map (widget);
  if (toolitem->priv->drag_window)
    gdk_window_show (toolitem->priv->drag_window);
}

static void
gtk_tool_item_unmap (GtkWidget *widget)
{
  GtkToolItem *toolitem;

  toolitem = GTK_TOOL_ITEM (widget);
  if (toolitem->priv->drag_window)
    gdk_window_hide (toolitem->priv->drag_window);
  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
gtk_tool_item_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkWidget *child = GTK_BIN (widget)->child;

  if (child && GTK_WIDGET_VISIBLE (child))
    {
      gtk_widget_size_request (child, requisition);
    }
  else
    {
      requisition->height = 0;
      requisition->width = 0;
    }
  
  requisition->width += (GTK_CONTAINER (widget)->border_width) * 2;
  requisition->height += (GTK_CONTAINER (widget)->border_width) * 2;
}

static void
gtk_tool_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (widget);
  GtkAllocation child_allocation;
  gint border_width;
  GtkWidget *child = GTK_BIN (widget)->child;

  widget->allocation = *allocation;
  border_width = GTK_CONTAINER (widget)->border_width;

  if (toolitem->priv->drag_window)
    gdk_window_move_resize (toolitem->priv->drag_window,
                            widget->allocation.x + border_width,
                            widget->allocation.y + border_width,
                            widget->allocation.width - border_width * 2,
                            widget->allocation.height - border_width * 2);
  
  if (child && GTK_WIDGET_VISIBLE (child))
    {
      child_allocation.x = allocation->x + border_width;
      child_allocation.y = allocation->y + border_width;
      child_allocation.width = allocation->width - 2 * border_width;
      child_allocation.height = allocation->height - 2 * border_width;
      
      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static gboolean
gtk_tool_item_create_menu_proxy (GtkToolItem *item)
{
  return FALSE;
}

/**
 * gtk_tool_item_new:
 * 
 * Creates a new #GtkToolItem
 * 
 * Return value: the new #GtkToolItem
 * 
 * Since: 2.4
 **/
GtkToolItem *
gtk_tool_item_new (void)
{
  GtkToolItem *item;

  item = g_object_new (GTK_TYPE_TOOL_ITEM, NULL);

  return item;
}

/**
 * gtk_tool_item_get_icon_size:
 * @tool_item: a #GtkToolItem:
 * 
 * Returns the icon size used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out what size icons
 * they should use.
 * 
 * Return value: a #GtkIconSize indicating the icon size used for @tool_item
 * 
 * Since: 2.4
 **/
GtkIconSize
gtk_tool_item_get_icon_size (GtkToolItem *tool_item)
{
  GtkWidget *parent;

  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_ICON_SIZE_LARGE_TOOLBAR);

  parent = GTK_WIDGET (tool_item)->parent;
  if (!parent || !GTK_IS_TOOLBAR (parent))
    return GTK_ICON_SIZE_LARGE_TOOLBAR;

  return gtk_toolbar_get_icon_size (GTK_TOOLBAR (parent));
}

/**
 * gtk_tool_item_get_orientation:
 * @tool_item: a #GtkToolItem: 
 * 
 * Returns the orientation used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function to find out what size icons
 * they should use.
 * 
 * Return value: a #GtkOrientation indicating the orientation
 * used for @tool_item
 * 
 * Since: 2.4
 **/
GtkOrientation
gtk_tool_item_get_orientation (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_ORIENTATION_HORIZONTAL);

  parent = GTK_WIDGET (tool_item)->parent;
  if (!parent || !GTK_IS_TOOLBAR (parent))
    return GTK_ORIENTATION_HORIZONTAL;

  return gtk_toolbar_get_orientation (GTK_TOOLBAR (parent));
}

/**
 * gtk_tool_item_get_toolbar_style:
 * @tool_item: a #GtkToolItem: 
 * 
 * Returns the toolbar style used for @tool_item. Custom subclasses of
 * #GtkToolItem should call this function in the handler of the
 * GtkToolItem::toolbar_reconfigured signal to find out in what style
 * the toolbar is displayed and change themselves accordingly 
 *
 * Possibilities are:
 * <itemizedlist>
 * <listitem> GTK_TOOLBAR_BOTH, meaning the tool item should show
 * both an icon and a label, stacked vertically </listitem>
 * <listitem> GTK_TOOLBAR_ICONS, meaning the toolbar shows
 * only icons </listitem>
 * <listitem> GTK_TOOLBAR_TEXT, meaning the tool item should only
 * show text</listitem>
 * <listitem> GTK_TOOLBAR_BOTH_HORIZ, meaning the tool item should show
 * both an icon and a label, arranged horizontally (however, note the 
 * #GtkToolButton::has_text_horizontally that makes tool buttons not
 * show labels when the toolbar style is GTK_TOOLBAR_BOTH_HORIZ.
 * </listitem>
 * </itemizedlist>
 * 
 * Return value: A #GtkToolbarStyle indicating the toolbar style used
 * for @tool_item.
 * 
 * Since: 2.4
 **/
GtkToolbarStyle
gtk_tool_item_get_toolbar_style (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_TOOLBAR_ICONS);

  parent = GTK_WIDGET (tool_item)->parent;
  if (!parent || !GTK_IS_TOOLBAR (parent))
    return GTK_TOOLBAR_ICONS;

  return gtk_toolbar_get_style (GTK_TOOLBAR (parent));
}

/**
 * gtk_tool_item_get_relief_style:
 * @tool_item: a #GtkToolItem: 
 * 
 * Returns the relief style of @tool_item. See gtk_button_set_relief_style().
 * Custom subclasses of #GtkToolItem should call this function in the handler
 * of the #GtkToolItem::toolbar_reconfigured signal to find out the
 * relief style of buttons.
 * 
 * Return value: a #GtkReliefStyle indicating the relief style used
 * for @tool_item.
 * 
 * Since: 2.4
 **/
GtkReliefStyle 
gtk_tool_item_get_relief_style (GtkToolItem *tool_item)
{
  GtkWidget *parent;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), GTK_RELIEF_NONE);

  parent = GTK_WIDGET (tool_item)->parent;
  if (!parent || !GTK_IS_TOOLBAR (parent))
    return GTK_RELIEF_NONE;

  return gtk_toolbar_get_relief_style (GTK_TOOLBAR (parent));
}

/**
 * gtk_tool_item_set_expand:
 * @tool_item: a #GtkToolItem: 
 * @expand: Whether @tool_item is allocated extra space
 * 
 * Sets whether @tool_item is allocated extra space when there
 * is more room on the toolbar then needed for the items. The
 * effect is that the item gets bigger when the toolbar gets bigger
 * and smaller when the toolbar gets smaller.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_expand (GtkToolItem *tool_item,
			  gboolean     expand)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));
    
  expand = expand != FALSE;

  if (tool_item->priv->expand != expand)
    {
      tool_item->priv->expand = expand;
      gtk_widget_child_notify (GTK_WIDGET (tool_item), "expand");
      gtk_widget_queue_resize (GTK_WIDGET (tool_item));
    }
}

/**
 * gtk_tool_item_get_expand:
 * @tool_item: a #GtkToolItem: 
 * 
 * Returns whether @tool_item is allocated extra space.
 * See gtk_tool_item_set_expand().
 * 
 * Return value: %TRUE if @tool_item is allocated extra space.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_expand (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  return tool_item->priv->expand;
}

/**
 * gtk_tool_item_set_homogeneous:
 * @tool_item: a #GtkToolItem: 
 * @homogeneous: whether @tool_item is the same size as other homogeneous items
 * 
 * Sets whether @tool_item is to be allocated the same size as other
 * homogeneous items. The effect is that all homogeneous items will have
 * the same width as the widest of the items.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_homogeneous (GtkToolItem *tool_item,
			       gboolean     homogeneous)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));
    
  homogeneous = homogeneous != FALSE;

  if (tool_item->priv->homogeneous != homogeneous)
    {
      tool_item->priv->homogeneous = homogeneous;
      gtk_widget_child_notify (GTK_WIDGET (tool_item), "homogeneous");
      gtk_widget_queue_resize (GTK_WIDGET (tool_item));
    }
}

/**
 * gtk_tool_item_get_homogeneous:
 * @tool_item: a #GtkToolItem: 
 * 
 * Returns whether @tool_item is the same size as other homogeneous
 * items. See gtk_tool_item_set_homogeneous().
 * 
 * Return value: %TRUE if the item is the same size as other homogeneous
 * item.s
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_homogeneous (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  return tool_item->priv->homogeneous;
}

/**
 * gtk_tool_item_get_is_important:
 * @tool_item: a #GtkToolItem
 * 
 * Returns whether @tool_item is considered important. See
 * gtk_tool_item_set_is_important()
 * 
 * Return value: %TRUE if @tool_item is considered important.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_is_important (GtkToolItem *tool_item)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), FALSE);

  return tool_item->priv->is_important;
}

/**
 * gtk_tool_item_set_is_important:
 * @tool_item: a #GtkToolItem
 * @is_important: whether the tool item should be considered important
 * 
 * Sets whether @tool_item should be considered important. The #GtkToolButton
 * class uses this property to determine whether to show or hide its label
 * when the toolbar style is %GTK_TOOLBAR_BOTH_HORIZ. The result is that
 * only tool buttons with the "is_important" property set have labels, an
 * effect known as "priority text"
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_is_important (GtkToolItem *tool_item, gboolean is_important)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  is_important = is_important != FALSE;

  if (is_important != tool_item->priv->is_important)
    {
      tool_item->priv->is_important = is_important;

      gtk_widget_queue_resize (GTK_WIDGET (tool_item));

      g_object_notify (G_OBJECT (tool_item), "is_important");
    }
}

static gboolean
gtk_tool_item_real_set_tooltip (GtkToolItem *tool_item,
				GtkTooltips *tooltips,
				const gchar *tip_text,
				const gchar *tip_private)
{
  GtkWidget *child = GTK_BIN (tool_item)->child;

  if (!child)
    return FALSE;

  gtk_tooltips_set_tip (tooltips, child, tip_text, tip_private);

  return TRUE;
}

/**
 * gtk_tool_item_set_tooltip:
 * @tool_item: a #GtkToolItem: 
 * @tooltips: The #GtkTooltips object to be used
 * @tip_text: text to be used as tooltip text for @tool_item
 * @tip_private: text to be used as private tooltip text
 *
 * Sets the #GtkTooltips object to be used for @tool_item, the
 * text to be displayed as tooltip on the item and the private text
 * to be used. See gtk_tooltips_set_tip().
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_tooltip (GtkToolItem *tool_item,
			   GtkTooltips *tooltips,
			   const gchar *tip_text,
			   const gchar *tip_private)
{
  gboolean retval;
  
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  g_signal_emit (tool_item, toolitem_signals[SET_TOOLTIP], 0,
		 tooltips, tip_text, tip_private, &retval);
}

/**
 * gtk_tool_item_set_use_drag_window:
 * @toolitem: a #GtkToolItem 
 * @use_drag_window: Whether @toolitem has a drag window.
 * 
 * Sets whether @toolitem has a drag window. When %TRUE the
 * toolitem can be used as a drag source through gtk_drag_source_set().
 * When @toolitem has a drag window it will intercept all events,
 * even those that would otherwise be sent to a child of @toolitem.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_use_drag_window (GtkToolItem *toolitem,
				   gboolean     use_drag_window)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (toolitem));

  use_drag_window = use_drag_window != FALSE;

  if (toolitem->priv->use_drag_window != use_drag_window)
    {
      toolitem->priv->use_drag_window = use_drag_window;
      
      if (use_drag_window)
	{
	  if (!toolitem->priv->drag_window && GTK_WIDGET_REALIZED (toolitem))
	    {
	      create_drag_window(toolitem);
	      if (GTK_WIDGET_MAPPED (toolitem))
		gdk_window_show (toolitem->priv->drag_window);
	    }
	}
      else
	{
	  destroy_drag_window (toolitem);
	}
    }
}

/**
 * gtk_tool_item_get_use_drag_window:
 * @toolitem: a #GtkToolItem 
 * 
 * Returns whether @toolitem has a drag window. See
 * gtk_tool_item_set_use_drag_window().
 * 
 * Return value: %TRUE if @toolitem uses a drag window.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_use_drag_window (GtkToolItem *toolitem)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (toolitem), FALSE);

  return toolitem->priv->use_drag_window;
}

/**
 * gtk_tool_item_set_visible_horizontal:
 * @toolitem: a #GtkToolItem
 * @visible_horizontal: Whether @toolitem is visible when in horizontal mode
 * 
 * Sets whether @toolitem is visible when the toolbar is docked horizontally.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_visible_horizontal (GtkToolItem *toolitem,
				      gboolean     visible_horizontal)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (toolitem));

  visible_horizontal = visible_horizontal != FALSE;

  if (toolitem->priv->visible_horizontal != visible_horizontal)
    {
      toolitem->priv->visible_horizontal = visible_horizontal;

      g_object_notify (G_OBJECT (toolitem), "visible_horizontal");

      gtk_widget_queue_resize (GTK_WIDGET (toolitem));
    }
}

/**
 * gtk_tool_item_get_visible_horizontal:
 * @toolitem: a #GtkToolItem 
 * 
 * Returns whether the @toolitem is visible on toolbars that are
 * docked horizontally.
 * 
 * Return value: %TRUE if @toolitem is visible on toolbars that are
 * docked horizontally.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_visible_horizontal (GtkToolItem *toolitem)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (toolitem), FALSE);

  return toolitem->priv->visible_horizontal;
}

/**
 * gtk_tool_item_set_visible_vertical:
 * @toolitem: a #GtkToolItem 
 * @visible_vertical: whether @toolitem is visible when the toolbar
 * is in vertical mode
 *
 * Sets whether @toolitem is visible when the toolbar is docked
 * vertically. Some tool items, such as text entries, are too wide to be
 * useful on a vertically docked toolbar. If @visible_vertical is %FALSE
 * @toolitem will not appear on toolbars that are docked vertically.
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_visible_vertical (GtkToolItem *toolitem,
				    gboolean     visible_vertical)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (toolitem));

  visible_vertical = visible_vertical != FALSE;

  if (toolitem->priv->visible_vertical != visible_vertical)
    {
      toolitem->priv->visible_vertical = visible_vertical;

      g_object_notify (G_OBJECT (toolitem), "visible_vertical");

      gtk_widget_queue_resize (GTK_WIDGET (toolitem));
    }
}

/**
 * gtk_tool_item_get_visible_vertical:
 * @toolitem: a #GtkToolItem 
 * 
 * Returns whether @toolitem is visible when the toolbar is docked vertically.
 * See gtk_tool_item_set_visible_vertical().
 * 
 * Return value: Whether @toolitem is visible when the toolbar is docked vertically
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_item_get_visible_vertical (GtkToolItem *toolitem)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (toolitem), FALSE);

  return toolitem->priv->visible_vertical;
}

/**
 * gtk_tool_item_retrieve_proxy_menu_item:
 * @tool_item: a #GtkToolItem: 
 * 
 * Returns the #GtkMenuItem that was last set by
 * gtk_tool_item_set_proxy_menu_item(), ie. the #GtkMenuItem
 * that is going to appear in the overflow menu.
 * 
 * Return value: The #GtkMenuItem that is going to appear in the
 * overflow menu for @tool_item.
 * 
 * Since: 2.4
 **/
GtkWidget *
gtk_tool_item_retrieve_proxy_menu_item (GtkToolItem *tool_item)
{
  gboolean retval;
  
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), NULL);

  g_signal_emit (tool_item, toolitem_signals[CREATE_MENU_PROXY], 0, &retval);
  
  return tool_item->priv->menu_item;
}

/**
 * gtk_tool_item_get_proxy_menu_item:
 * @tool_item: a #GtkToolItem: 
 * @menu_item_id: a string used to identify the menu item
 * 
 * If @menu_item_id matches the string passed to
 * gtk_tool_item_set_proxy_menu_item() return the corresponding #GtkMenuItem.
 *
 * Custom subclasses of #GtkToolItem should use this function to update
 * their menu item when the #GtkToolItem changes. That the
 * @menu_item_id<!-- -->s must match ensures that a #GtkToolItem will not
 * inadvertently change a menu item that they did not create.
 * 
 * Return value: The #GtkMenuItem passed to
 * gtk_tool_item_set_proxy_menu_item(), if the @menu_item_id<!-- -->s match.
 * 
 * Since: 2.4
 **/
GtkWidget *
gtk_tool_item_get_proxy_menu_item (GtkToolItem *tool_item,
				   const gchar *menu_item_id)
{
  g_return_val_if_fail (GTK_IS_TOOL_ITEM (tool_item), NULL);
  g_return_val_if_fail (menu_item_id != NULL, NULL);

  if (tool_item->priv->menu_item_id && strcmp (tool_item->priv->menu_item_id, menu_item_id) == 0)
    return tool_item->priv->menu_item;

  return NULL;
}

/**
 * gtk_tool_item_set_proxy_menu_item:
 * @tool_item: a #GtkToolItem:
 * @menu_item_id: a string used to identify @menu_item
 * @menu_item: a #GtkMenuItem to be used in the overflow menu
 * 
 * Sets the #GtkMenuItem used in the toolbar overflow menu. The
 * @menu_item_id is used to identify the caller of this function and
 * should also be used with gtk_tool_item_get_proxy_menu_item().
 * 
 * Since: 2.4
 **/
void
gtk_tool_item_set_proxy_menu_item (GtkToolItem *tool_item,
				   const gchar *menu_item_id,
				   GtkWidget   *menu_item)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));
  g_return_if_fail (menu_item == NULL || GTK_IS_MENU_ITEM (menu_item));
  g_return_if_fail (menu_item_id != NULL);

  if (tool_item->priv->menu_item_id)
    g_free (tool_item->priv->menu_item_id);
      
  tool_item->priv->menu_item_id = g_strdup (menu_item_id);

  if (tool_item->priv->menu_item != menu_item)
    {
      if (tool_item->priv->menu_item)
	g_object_unref (G_OBJECT (tool_item->priv->menu_item));
      
      if (menu_item)
	{
	  g_object_ref (menu_item);
	  gtk_object_sink (GTK_OBJECT (menu_item));

	  gtk_widget_set_sensitive (menu_item,
				    GTK_WIDGET_SENSITIVE (tool_item));
	}
      
      tool_item->priv->menu_item = menu_item;
    }
}

/**
 * _gtk_tool_item_toolbar_reconfigured:
 * @tool_item: a #GtkToolItem: 
 * 
 * Emits the signal #GtkToolItem::toolbar_reconfigured on @tool_item. This
 * internal function is called by #GtkToolbar when some aspect of its
 * configuration changes.
 * 
 * Since: 2.4
 **/
void
_gtk_tool_item_toolbar_reconfigured (GtkToolItem *tool_item)
{
  g_return_if_fail (GTK_IS_TOOL_ITEM (tool_item));

  g_signal_emit (tool_item, toolitem_signals[TOOLBAR_RECONFIGURED], 0);
  
  gtk_widget_queue_resize (GTK_WIDGET (tool_item));
}

