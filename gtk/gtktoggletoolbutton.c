/* gtktoggletoolbutton.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
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
#include "gtktoggletoolbutton.h"
#include "gtkcheckmenuitem.h"
#include "gtklabel.h"
#include "gtktogglebutton.h"
#include "gtkstock.h"
#include "gtkintl.h"
#include "gtkradiotoolbutton.h"

#define MENU_ID "gtk-toggle-tool-button-menu-id"

enum {
  TOGGLED,
  LAST_SIGNAL
};

#define GTK_TOGGLE_TOOL_BUTTON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_TOGGLE_TOOL_BUTTON, GtkToggleToolButtonPrivate))

struct _GtkToggleToolButtonPrivate
{
  guint active : 1;
};
  
static void gtk_toggle_tool_button_init       (GtkToggleToolButton      *button);
static void gtk_toggle_tool_button_class_init (GtkToggleToolButtonClass *klass);

static gboolean gtk_toggle_tool_button_create_menu_proxy (GtkToolItem *button);

static void button_toggled      (GtkWidget           *widget,
				 GtkToggleToolButton *button);
static void menu_item_activated (GtkWidget           *widget,
				 GtkToggleToolButton *button);

static GObjectClass *parent_class = NULL;
static guint         toggle_signals[LAST_SIGNAL] = { 0 };

GType
gtk_toggle_tool_button_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
	{
	  sizeof (GtkToggleToolButtonClass),
	  (GBaseInitFunc) 0,
	  (GBaseFinalizeFunc) 0,
	  (GClassInitFunc) gtk_toggle_tool_button_class_init,
	  (GClassFinalizeFunc) 0,
	  NULL,
	  sizeof (GtkToggleToolButton),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) gtk_toggle_tool_button_init
	};

      type = g_type_register_static (GTK_TYPE_TOOL_BUTTON,
				     "GtkToggleToolButton", &type_info, 0);
    }
  return type;
}


static void
gtk_toggle_tool_button_class_init (GtkToggleToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkToolItemClass *toolitem_class;
  GtkToolButtonClass *toolbutton_class;

  parent_class = g_type_class_peek_parent (klass);

  object_class = (GObjectClass *)klass;
  toolitem_class = (GtkToolItemClass *)klass;
  toolbutton_class = (GtkToolButtonClass *)klass;

  toolitem_class->create_menu_proxy = gtk_toggle_tool_button_create_menu_proxy;
  toolbutton_class->button_type = GTK_TYPE_TOGGLE_BUTTON;
  
/**
 * GtkToggleToolButton::toggled:
 * @toggle_tool_button: the object that emitted the signal
 *
 * Emitted whenever the toggle tool button changes state.
 **/
  toggle_signals[TOGGLED] =
    g_signal_new ("toggled",
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToggleToolButtonClass, toggled),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (object_class, sizeof (GtkToggleToolButtonPrivate));
}

static void
gtk_toggle_tool_button_init (GtkToggleToolButton *button)
{
  GtkToolButton *tool_button = GTK_TOOL_BUTTON (button);
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (_gtk_tool_button_get_button (tool_button));

  button->priv = GTK_TOGGLE_TOOL_BUTTON_GET_PRIVATE (button);

  /* If the real button is a radio button, it may have been
   * active at the time it was created.
   */
  button->priv->active = gtk_toggle_button_get_active (toggle_button);
    
  g_signal_connect_object (toggle_button,
			   "toggled", G_CALLBACK (button_toggled), button, 0);
}

static gboolean
gtk_toggle_tool_button_create_menu_proxy (GtkToolItem *item)
{
  GtkToolButton *tool_button = GTK_TOOL_BUTTON (item);
  GtkToggleToolButton *toggle_tool_button = GTK_TOGGLE_TOOL_BUTTON (item);
  GtkWidget *menu_item = NULL;
  GtkStockItem stock_item;
  gboolean use_mnemonic = TRUE;
  const char *label;

  GtkWidget *label_widget = gtk_tool_button_get_label_widget (tool_button);
  const gchar *label_text = gtk_tool_button_get_label (tool_button);
  const gchar *stock_id = gtk_tool_button_get_stock_id (tool_button);

  if (label_widget && GTK_IS_LABEL (label_widget))
    {
      label = gtk_label_get_label (GTK_LABEL (label_widget));
      use_mnemonic = gtk_label_get_use_underline (GTK_LABEL (label_widget));
    }
  else if (label_text)
    {
      label = label_text;
      use_mnemonic = gtk_tool_button_get_use_underline (tool_button);
    }
  else if (stock_id && gtk_stock_lookup (stock_id, &stock_item))
    {
      label = stock_item.label;
    }
  else
    {
      label = "";
    }
  
  if (use_mnemonic)
    menu_item = gtk_check_menu_item_new_with_mnemonic (label);
  else
    menu_item = gtk_check_menu_item_new_with_label (label);

  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
				  toggle_tool_button->priv->active);

  if (GTK_IS_RADIO_TOOL_BUTTON (toggle_tool_button))
    {
      gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (menu_item),
					     TRUE);
    }

  g_signal_connect_closure_by_id (menu_item,
				  g_signal_lookup ("activate", G_OBJECT_TYPE (menu_item)), 0,
				  g_cclosure_new_object (G_CALLBACK (menu_item_activated),
							 G_OBJECT (toggle_tool_button)),
				  FALSE);

  gtk_tool_item_set_proxy_menu_item (item, MENU_ID, menu_item);
  
  return TRUE;
}

/* There are two activatable widgets, a toggle button and a menu item.
 *
 * If a widget is activated and the state of the tool button is the same as
 * the new state of the activated widget, then the other widget was the one
 * that was activated by the user and updated the tool button's state.
 *
 * If the state of the tool button is not the same as the new state of the
 * activated widget, then the activation was activated by the user, and the
 * widget needs to make sure the tool button is updated before the other
 * widget is activated. This will make sure the other widget a tool button
 * in a state that matches its own new state.
 */
static void
menu_item_activated (GtkWidget           *menu_item,
		     GtkToggleToolButton *toggle_tool_button)
{
  GtkToolButton *tool_button = GTK_TOOL_BUTTON (toggle_tool_button);
  gboolean menu_active = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menu_item));

  if (toggle_tool_button->priv->active != menu_active)
    {
      toggle_tool_button->priv->active = menu_active;

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (_gtk_tool_button_get_button (tool_button)),
				    toggle_tool_button->priv->active);

      g_signal_emit (G_OBJECT (toggle_tool_button), toggle_signals[TOGGLED], 0);
    }
}

static void
button_toggled (GtkWidget           *widget,
		GtkToggleToolButton *toggle_tool_button)
{
  gboolean toggle_active = GTK_TOGGLE_BUTTON (widget)->active;

  if (toggle_tool_button->priv->active != toggle_active)
    {
      GtkWidget *menu_item;
      
      toggle_tool_button->priv->active = toggle_active;
       
      if ((menu_item =
	   gtk_tool_item_get_proxy_menu_item (GTK_TOOL_ITEM (toggle_tool_button), MENU_ID)))
	{
	  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
					  toggle_tool_button->priv->active);
	}

      g_signal_emit (G_OBJECT (toggle_tool_button), toggle_signals[TOGGLED], 0);
    }
}

/**
 * gtk_toggle_tool_button_new:
 * 
 * Returns a new #GtkToggleToolButton
 * 
 * Return value: a newly created #GtkToggleToolButton
 * 
 * Since: 2.4
 **/
GtkToolItem *
gtk_toggle_tool_button_new (void)
{
  GtkToolButton *button;

  button = g_object_new (GTK_TYPE_TOGGLE_TOOL_BUTTON,
			 NULL);
  
  return GTK_TOOL_ITEM (button);
}

/**
 * gtk_toggle_tool_button_new_from_stock:
 * @stock_id: the name of the stock item 
 *
 * Creates a new #GtkToggleToolButton containing the image and text from a
 * stock item. Some stock ids have preprocessor macros like #GTK_STOCK_OK
 * and #GTK_STOCK_APPLY.
 *
 * It is an error if @stock_id is not a name of a stock item.
 * 
 * Return value: A new #GtkToggleToolButton
 * 
 * Since: 2.4
 **/
GtkToolItem *
gtk_toggle_tool_button_new_from_stock (const gchar *stock_id)
{
  GtkToolButton *button;

  g_return_val_if_fail (stock_id != NULL, NULL);
  
  button = g_object_new (GTK_TYPE_TOGGLE_TOOL_BUTTON,
			 "stock_id", stock_id,
			 NULL);
  
  return GTK_TOOL_ITEM (button);
}

/**
 * gtk_toggle_tool_button_set_active:
 * @button: a #GtkToggleToolButton
 * @is_active: whether @button should be active
 * 
 * Sets the status of the toggle tool button. Set to %TRUE if you
 * want the GtkToggleButton to be 'pressed in', and %FALSE to raise it.
 * This action causes the toggled signal to be emitted.
 * 
 * Since: 2.4
 **/
void
gtk_toggle_tool_button_set_active (GtkToggleToolButton *button,
				   gboolean is_active)
{
  g_return_if_fail (GTK_IS_TOGGLE_TOOL_BUTTON (button));

  is_active = is_active != FALSE;

  if (button->priv->active != is_active)
    gtk_button_clicked (GTK_BUTTON (_gtk_tool_button_get_button (GTK_TOOL_BUTTON (button))));
}

/**
 * gtk_toggle_tool_button_get_active:
 * @button: a #GtkToggleToolButton
 * 
 * Queries a #GtkToggleToolButton and returns its current state.
 * Returns %TRUE if the toggle button is pressed in and %FALSE if it is raised.
 * 
 * Return value: %TRUE if the toggle tool button is pressed in, %FALSE if not
 * 
 * Since: 2.4
 **/
gboolean
gtk_toggle_tool_button_get_active (GtkToggleToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_TOOL_BUTTON (button), FALSE);

  return button->priv->active;
}
