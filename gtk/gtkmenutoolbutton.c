/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
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
#include "gtkalias.h"
#include "gtkmenutoolbutton.h"
#include "gtkintl.h"
#include "gtktogglebutton.h"
#include "gtkarrow.h"
#include "gtkhbox.h"
#include "gtkvbox.h"
#include "gtkmenu.h"
#include "gtkmain.h"


#define GTK_MENU_TOOL_BUTTON_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GTK_TYPE_MENU_TOOL_BUTTON, GtkMenuToolButtonPrivate))

struct _GtkMenuToolButtonPrivate
{
  GtkWidget *button;
  GtkWidget *arrow;
  GtkWidget *arrow_button;
  GtkWidget *box;
  GtkMenu   *menu;
};

static void gtk_menu_tool_button_init       (GtkMenuToolButton      *button);
static void gtk_menu_tool_button_class_init (GtkMenuToolButtonClass *klass);
static void gtk_menu_tool_button_finalize   (GObject                *object);

enum
{
  MENU_ACTIVATED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_MENU,
  LAST_PROP
};

static gint signals[LAST_SIGNAL];

static GObjectClass *parent_class = NULL;

GType
gtk_menu_tool_button_get_type (void)
{
  static GType type = 0;

  if (type == 0)
    {
      static const GTypeInfo info =
	{
	  sizeof (GtkMenuToolButtonClass),
	  (GBaseInitFunc) 0,
	  (GBaseFinalizeFunc) 0,
	  (GClassInitFunc) gtk_menu_tool_button_class_init,
	  (GClassFinalizeFunc) 0,
	  NULL,
	  sizeof (GtkMenuToolButton),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) gtk_menu_tool_button_init
	};

      type = g_type_register_static (GTK_TYPE_TOOL_BUTTON,
                                     "GtkMenuToolButton",
                                     &info, 0);
    }

  return type;
}

static gboolean
gtk_menu_tool_button_set_tooltip (GtkToolItem *tool_item,
                                  GtkTooltips *tooltips,
                                  const char  *tip_text,
                                  const char  *tip_private)
{
  GtkMenuToolButton *button;

  g_return_val_if_fail (GTK_IS_MENU_TOOL_BUTTON (tool_item), FALSE);

  button = GTK_MENU_TOOL_BUTTON (tool_item);
  gtk_tooltips_set_tip (tooltips, button->priv->button, tip_text, tip_private);

  return TRUE;
}

static void
gtk_menu_tool_button_construct_contents (GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv;
  GtkWidget *box;
  GtkOrientation orientation;

  priv = GTK_MENU_TOOL_BUTTON_GET_PRIVATE (button);

  orientation = gtk_tool_item_get_orientation (GTK_TOOL_ITEM (button));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      box = gtk_hbox_new (FALSE, 0);
      gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_DOWN, GTK_SHADOW_NONE);
    }
  else
    {
      box = gtk_vbox_new (FALSE, 0);
      gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
    }

  if (priv->button && priv->button->parent)
    {
      g_object_ref (priv->button);
      gtk_container_remove (GTK_CONTAINER (priv->button->parent),
                            priv->button);
      gtk_container_add (GTK_CONTAINER (box), priv->button);
      g_object_unref (priv->button);
    }

  if (priv->arrow_button && priv->arrow_button->parent)
    {
      g_object_ref (priv->arrow_button);
      gtk_container_remove (GTK_CONTAINER (priv->arrow_button->parent),
                            priv->arrow_button);
      gtk_box_pack_end (GTK_BOX (box), priv->arrow_button,
                        FALSE, FALSE, 0);
      g_object_unref (priv->arrow_button);
    }

  if (priv->box)
    {
      /* Note: we are not destroying the button and the arrow_button
       * here because they were removed from their container above
       */
      gtk_widget_destroy (priv->box);
    }

  priv->box = box;

  gtk_container_add (GTK_CONTAINER (button), priv->box);
  gtk_widget_show_all (priv->box);

  gtk_widget_queue_resize (GTK_WIDGET (button));
}

static void
gtk_menu_tool_button_toolbar_reconfigured (GtkToolItem *toolitem)
{
  gtk_menu_tool_button_construct_contents (GTK_MENU_TOOL_BUTTON (toolitem));

  /* chain up */
  GTK_TOOL_ITEM_CLASS (parent_class)->toolbar_reconfigured (toolitem);
}

static void
gtk_menu_tool_button_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_MENU:
      gtk_menu_tool_button_set_menu (button, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_tool_button_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_MENU:
      g_value_set_object (value, button->priv->menu);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_tool_button_class_init (GtkMenuToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkToolItemClass *toolitem_class;
  GtkToolButtonClass *toolbutton_class;

  parent_class = g_type_class_peek_parent (klass);

  object_class = (GObjectClass *)klass;
  toolitem_class = (GtkToolItemClass *)klass;
  toolbutton_class = (GtkToolButtonClass *)klass;

  object_class->set_property = gtk_menu_tool_button_set_property;
  object_class->get_property = gtk_menu_tool_button_get_property;
  object_class->finalize = gtk_menu_tool_button_finalize;
  toolitem_class->set_tooltip = gtk_menu_tool_button_set_tooltip;
  toolitem_class->toolbar_reconfigured = gtk_menu_tool_button_toolbar_reconfigured;

  signals[MENU_ACTIVATED] =
    g_signal_new ("menu-activated",
                  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuToolButtonClass, menu_activated),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_MENU,
                                   g_param_spec_object ("menu",
                                                        P_("Menu"),
                                                        P_("The dropdown menu"),
                                                        GTK_TYPE_MENU,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_type_class_add_private (object_class, sizeof (GtkMenuToolButtonPrivate));
}

static void
button_state_changed_cb (GtkWidget         *widget,
                         GtkStateType       previous_state,
                         GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv;
  GtkWidget *b;
  GtkStateType state = GTK_WIDGET_STATE (widget);

  priv = GTK_MENU_TOOL_BUTTON_GET_PRIVATE (button);

  b = (widget == priv->arrow_button) ? priv->button : priv->arrow_button;

  g_signal_handlers_block_by_func (G_OBJECT (b),
                                   G_CALLBACK (button_state_changed_cb),
                                   button);

  if (state == GTK_STATE_PRELIGHT &&
      previous_state != GTK_STATE_ACTIVE)
    {
      gtk_widget_set_state (b, state);
    }
  else if (state == GTK_STATE_NORMAL)
    {
      gtk_widget_set_state (b, state);
    }
  else if (state == GTK_STATE_ACTIVE)
    {
      gtk_widget_set_state (b, GTK_STATE_NORMAL);
    }

  g_signal_handlers_unblock_by_func (G_OBJECT (b),
                                     G_CALLBACK (button_state_changed_cb),
                                     button);
}

static void
menu_position_func (GtkMenu           *menu,
                    int               *x,
                    int               *y,
                    gboolean          *push_in,
                    GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv;
  GtkRequisition req;
  GtkRequisition menu_req;
  GtkOrientation orientation;
  GtkTextDirection direction;

  priv = GTK_MENU_TOOL_BUTTON_GET_PRIVATE (button);

  gdk_window_get_origin (GTK_BUTTON (priv->arrow_button)->event_window, x, y);
  gtk_widget_size_request (priv->arrow_button, &req);
  gtk_widget_size_request (GTK_WIDGET (priv->menu), &menu_req);

  orientation = gtk_tool_item_get_orientation (GTK_TOOL_ITEM (button));
  direction = gtk_widget_get_direction (GTK_WIDGET (priv->arrow_button));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (direction == GTK_TEXT_DIR_LTR)
        *x += priv->arrow_button->allocation.width - req.width;
      else
        *x += req.width - menu_req.width;
      *y += priv->arrow_button->allocation.height;
    }
  else 
    {
      if (direction == GTK_TEXT_DIR_LTR)
        *x += priv->arrow_button->allocation.width;
      else 
        *x -= menu_req.width;
      *y += priv->arrow_button->allocation.height - req.height;
    }

  *push_in = TRUE;
}

static void
popup_menu_under_arrow (GtkMenuToolButton *button,
                        GdkEventButton    *event)
{
  GtkMenuToolButtonPrivate *priv;

  priv = GTK_MENU_TOOL_BUTTON_GET_PRIVATE (button);

  g_signal_emit (button, signals[MENU_ACTIVATED], 0);

  if (!priv->menu)
    return;

  gtk_menu_popup (priv->menu, NULL, NULL,
                  (GtkMenuPositionFunc) menu_position_func,
                  button,
                  event ? event->button : 0,
                  event ? event->time : gtk_get_current_event_time ());
}

static void
arrow_button_toggled_cb (GtkToggleButton   *togglebutton,
                         GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv;

  priv = GTK_MENU_TOOL_BUTTON_GET_PRIVATE (button);

  if (!priv->menu)
    return;

  if (gtk_toggle_button_get_active (togglebutton) &&
      !GTK_WIDGET_VISIBLE (priv->menu))
    {
      /* we get here only when the menu is activated by a key
       * press, so that we can select the first menu item */
      popup_menu_under_arrow (button, NULL);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->menu), FALSE);
    }
}

static gboolean
arrow_button_button_press_event_cb (GtkWidget         *widget,
                                    GdkEventButton    *event,
                                    GtkMenuToolButton *button)
{
  popup_menu_under_arrow (button, event);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

  return TRUE;
}

static void
gtk_menu_tool_button_init (GtkMenuToolButton *button)
{
  GtkWidget *box;
  GtkWidget *arrow;
  GtkWidget *arrow_button;
  GtkWidget *real_button;

  button->priv = GTK_MENU_TOOL_BUTTON_GET_PRIVATE (button);

  gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (button), FALSE);

  box = gtk_hbox_new (FALSE, 0);

  real_button = GTK_BIN (button)->child;
  g_object_ref (real_button);
  gtk_container_remove (GTK_CONTAINER (button), real_button);
  gtk_container_add (GTK_CONTAINER (box), real_button);
  g_object_unref (real_button);

  arrow_button = gtk_toggle_button_new ();
  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_button_set_relief (GTK_BUTTON (arrow_button), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER (arrow_button), arrow);
  gtk_box_pack_end (GTK_BOX (box), arrow_button,
                    FALSE, FALSE, 0);

  /* the arrow button is insentive until we set a menu */
  gtk_widget_set_sensitive (arrow_button, FALSE);

  gtk_widget_show_all (box);

  gtk_container_add (GTK_CONTAINER (button), box);

  button->priv->button = real_button;
  button->priv->arrow = arrow;
  button->priv->arrow_button = arrow_button;
  button->priv->box = box;

  g_signal_connect (real_button, "state_changed",
                    G_CALLBACK (button_state_changed_cb), button);
  g_signal_connect (arrow_button, "state_changed",
                    G_CALLBACK (button_state_changed_cb), button);
  g_signal_connect (arrow_button, "toggled",
                    G_CALLBACK (arrow_button_toggled_cb), button);
  g_signal_connect (arrow_button, "button_press_event",
                    G_CALLBACK (arrow_button_button_press_event_cb), button);
}

static void
gtk_menu_tool_button_finalize (GObject *object)
{
  GtkMenuToolButton *button;

  button = GTK_MENU_TOOL_BUTTON (object);

  if (button->priv->menu)
    g_object_unref (button->priv->menu);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtk_menu_tool_button_new:
 * @icon_widget: a widget that will be used as icon widget, or %NULL
 * @label: a string that will be used as label, or %NULL
 *
 * Creates a new #GtkMenuToolButton using @icon_widget as icon and
 * @label as label.
 *
 * Return value: the new #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkToolItem *
gtk_menu_tool_button_new (GtkWidget   *icon_widget,
                          const gchar *label)
{
  GtkMenuToolButton *button;

  button = g_object_new (GTK_TYPE_MENU_TOOL_BUTTON, NULL);

  if (label)
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (button), label);

  if (icon_widget)
    gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (button), icon_widget);

  return GTK_TOOL_ITEM (button);
}

/**
 * gtk_menu_tool_button_new_from_stock:
 * @stock_id: the name of a stock item
 *
 * Creates a new #GtkMenuToolButton.
 * The new #GtkMenuToolButton will contain an icon and label from
 * the stock item indicated by @stock_id.
 *
 * Return value: the new #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkToolItem *
gtk_menu_tool_button_new_from_stock (const gchar *stock_id)
{
  GtkMenuToolButton *button;

  g_return_val_if_fail (stock_id != NULL, NULL);

  button = g_object_new (GTK_TYPE_MENU_TOOL_BUTTON,
			 "stock_id", stock_id,
			 NULL);

  return GTK_TOOL_ITEM (button);
}

/* Callback for the "deactivate" signal on the pop-up menu.
 * This is used so that we unset the state of the toggle button
 * when the pop-up menu disappears. 
 */
static int
menu_deactivate_cb (GtkMenuShell      *menu_shell,
		    GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv = button->priv;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->arrow_button), FALSE);

  return TRUE;
}

/**
 * gtk_menu_tool_button_set_menu:
 * @button: a #GtkMenuToolButton
 * @menu: the #GtkMenu associated with #GtkMenuToolButton
 *
 * Sets the #GtkMenu that is popped up when the user clicks on the arrow.
 * If @menu is NULL, the arrow button becomes insensitive.
 *
 * Since: 2.6
 **/
void
gtk_menu_tool_button_set_menu (GtkMenuToolButton *button,
                               GtkWidget         *menu)
{
  GtkMenuToolButtonPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_TOOL_BUTTON (button));
  g_return_if_fail (GTK_IS_MENU (menu) || menu == NULL);

  priv = button->priv;

  if (priv->menu != GTK_MENU (menu))
    {
      if (priv->menu && GTK_WIDGET_VISIBLE (priv->menu))
        gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->menu));

      if (priv->menu)
        g_object_unref (priv->menu);

      priv->menu = GTK_MENU (menu);

      if (priv->menu)
        {
          g_object_ref (priv->menu);
          gtk_object_sink (GTK_OBJECT (priv->menu));

          gtk_widget_set_sensitive (priv->arrow_button, TRUE);

          g_signal_connect (button->priv->menu, "deactivate",
                            G_CALLBACK (menu_deactivate_cb), button);
        }
      else
       gtk_widget_set_sensitive (priv->arrow_button, FALSE);
    }

  g_object_notify (G_OBJECT (button), "menu");
}

/**
 * gtk_menu_tool_button_get_menu:
 * @button: a #GtkMenuToolButton
 * @menu: the #GtkMenu associated with #GtkMenuToolButton
 *
 * Gets the #GtkMenu associated with #GtkMenuToolButton.
 *
 * Return value: the #GtkMenu associated with #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkWidget *
gtk_menu_tool_button_get_menu (GtkMenuToolButton *button)
{
  g_return_val_if_fail (GTK_IS_MENU_TOOL_BUTTON (button), NULL);

  return GTK_WIDGET (button->priv->menu);
}

/**
 * gtk_menu_tool_set_arrow_tooltip:
 * @button: a #GtkMenuToolButton
 * @tooltips: the #GtkTooltips object to be used
 * @tip_text: text to be used as tooltip text for tool_item
 * @tip_private: text to be used as private tooltip text
 *
 * Sets the #GtkTooltips object to be used for arrow button which
 * pops up the menu. See gtk_tool_item_set_tooltip() for setting
 * a tooltip on the whole #GtkMenuToolButton.
 *
 * Since: 2.6
 **/
void
gtk_menu_tool_button_set_arrow_tooltip (GtkMenuToolButton *button,
                                        GtkTooltips       *tooltips,
                                        const gchar       *tip_text,
                                        const gchar       *tip_private)
{
  g_return_if_fail (GTK_IS_MENU_TOOL_BUTTON (button));

  gtk_tooltips_set_tip (tooltips, button->priv->arrow_button, tip_text, tip_private);
}

