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

#define GTK_MENU_INTERNALS

#include "gdk/gdkkeysyms.h"
#include "gtkbindings.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenubar.h"
#include "gtkmenuitem.h"
#include "gtkmenushell.h"
#include "gtktearoffmenuitem.h"
#include "gtkwindow.h"

#define MENU_SHELL_TIMEOUT   500

enum {
  DEACTIVATE,
  SELECTION_DONE,
  MOVE_CURRENT,
  ACTIVATE_CURRENT,
  CANCEL,
  CYCLE_FOCUS,
  LAST_SIGNAL
};

typedef void (*GtkMenuShellSignal1) (GtkObject           *object,
				     GtkMenuDirectionType arg1,
				     gpointer             data);
typedef void (*GtkMenuShellSignal2) (GtkObject *object,
				     gboolean   arg1,
				     gpointer   data);

/* Terminology:
 * 
 * A menu item can be "selected", this means that it is displayed
 * in the prelight state, and if it has a submenu, that submenu
 * will be popped up. 
 * 
 * A menu is "active" when it is visible onscreen and the user
 * is selecting from it. A menubar is not active until the user
 * clicks on one of its menuitems. When a menu is active,
 * passing the mouse over a submenu will pop it up.
 *
 * menu_shell->active_menu_item, is however, not an "active"
 * menu item (there is no such thing) but rather, the selected
 * menu item in that MenuShell, if there is one.
 *
 * There is also is a concept of the current menu and a current
 * menu item. The current menu item is the selected menu item
 * that is furthest down in the heirarchy. (Every active menu_shell
 * does not necessarily contain a selected menu item, but if
 * it does, then menu_shell->parent_menu_shell must also contain
 * a selected menu item. The current menu is the menu that 
 * contains the current menu_item. It will always have a GTK
 * grab and receive all key presses.
 *
 *
 * Action signals:
 *
 *  ::move_current (GtkMenuDirection *dir)
 *     Moves the current menu item in direction 'dir':
 *
 *       GTK_MENU_DIR_PARENT: To the parent menu shell
 *       GTK_MENU_DIR_CHILD: To the child menu shell (if this item has
 *          a submenu.
 *       GTK_MENU_DIR_NEXT/PREV: To the next or previous item
 *          in this menu.
 * 
 *     As a a bit of a hack to get movement between menus and
 *     menubars working, if submenu_placement is different for
 *     the menu and its MenuShell then the following apply:
 * 
 *       - For 'parent' the current menu is not just moved to
 *         the parent, but moved to the previous entry in the parent
 *       - For 'child', if there is no child, then current is
 *         moved to the next item in the parent.
 *
 * 
 *  ::activate_current (GBoolean *force_hide)
 *     Activate the current item. If 'force_hide' is true, hide
 *     the current menu item always. Otherwise, only hide
 *     it if menu_item->klass->hide_on_activate is true.
 *
 *  ::cancel ()
 *     Cancels the current selection
 */

static void gtk_menu_shell_class_init        (GtkMenuShellClass *klass);
static void gtk_menu_shell_init              (GtkMenuShell      *menu_shell);
static void gtk_menu_shell_realize           (GtkWidget         *widget);
static gint gtk_menu_shell_button_press      (GtkWidget         *widget,
					      GdkEventButton    *event);
static gint gtk_menu_shell_button_release    (GtkWidget         *widget,
					      GdkEventButton    *event);
static gint gtk_menu_shell_key_press         (GtkWidget	        *widget,
					      GdkEventKey       *event);
static gint gtk_menu_shell_enter_notify      (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static gint gtk_menu_shell_leave_notify      (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static void gtk_menu_shell_add               (GtkContainer      *container,
					      GtkWidget         *widget);
static void gtk_menu_shell_remove            (GtkContainer      *container,
					      GtkWidget         *widget);
static void gtk_menu_shell_forall            (GtkContainer      *container,
					      gboolean		 include_internals,
					      GtkCallback        callback,
					      gpointer           callback_data);
static void gtk_menu_shell_real_insert       (GtkMenuShell *menu_shell,
					      GtkWidget    *child,
					      gint          position);
static void gtk_real_menu_shell_deactivate   (GtkMenuShell      *menu_shell);
static gint gtk_menu_shell_is_item           (GtkMenuShell      *menu_shell,
					      GtkWidget         *child);
static GtkWidget *gtk_menu_shell_get_item    (GtkMenuShell      *menu_shell,
					      GdkEvent          *event);
static GType    gtk_menu_shell_child_type  (GtkContainer      *container);
static void gtk_menu_shell_real_select_item  (GtkMenuShell      *menu_shell,
					      GtkWidget         *menu_item);
static void gtk_menu_shell_select_submenu_first (GtkMenuShell   *menu_shell); 

static void gtk_real_menu_shell_move_current (GtkMenuShell      *menu_shell,
					      GtkMenuDirectionType direction);
static void gtk_real_menu_shell_activate_current (GtkMenuShell      *menu_shell,
						  gboolean           force_hide);
static void gtk_real_menu_shell_cancel           (GtkMenuShell      *menu_shell);
static void gtk_real_menu_shell_cycle_focus      (GtkMenuShell      *menu_shell,
						  GtkDirectionType   dir);

static GtkContainerClass *parent_class = NULL;
static guint menu_shell_signals[LAST_SIGNAL] = { 0 };


GType
gtk_menu_shell_get_type (void)
{
  static GType menu_shell_type = 0;

  if (!menu_shell_type)
    {
      static const GTypeInfo menu_shell_info =
      {
	sizeof (GtkMenuShellClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_menu_shell_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkMenuShell),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_menu_shell_init,
	NULL,		/* value_table */
      };

      menu_shell_type =
	g_type_register_static (GTK_TYPE_CONTAINER, "GtkMenuShell",
				&menu_shell_info, G_TYPE_FLAG_ABSTRACT);
    }

  return menu_shell_type;
}

static void
gtk_menu_shell_class_init (GtkMenuShellClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  GtkBindingSet *binding_set;

  object_class = (GObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  parent_class = g_type_class_peek_parent (klass);

  widget_class->realize = gtk_menu_shell_realize;
  widget_class->button_press_event = gtk_menu_shell_button_press;
  widget_class->button_release_event = gtk_menu_shell_button_release;
  widget_class->key_press_event = gtk_menu_shell_key_press;
  widget_class->enter_notify_event = gtk_menu_shell_enter_notify;
  widget_class->leave_notify_event = gtk_menu_shell_leave_notify;

  container_class->add = gtk_menu_shell_add;
  container_class->remove = gtk_menu_shell_remove;
  container_class->forall = gtk_menu_shell_forall;
  container_class->child_type = gtk_menu_shell_child_type;

  klass->submenu_placement = GTK_TOP_BOTTOM;
  klass->deactivate = gtk_real_menu_shell_deactivate;
  klass->selection_done = NULL;
  klass->move_current = gtk_real_menu_shell_move_current;
  klass->activate_current = gtk_real_menu_shell_activate_current;
  klass->cancel = gtk_real_menu_shell_cancel;
  klass->select_item = gtk_menu_shell_real_select_item;
  klass->insert = gtk_menu_shell_real_insert;

  menu_shell_signals[DEACTIVATE] =
    g_signal_new ("deactivate",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkMenuShellClass, deactivate),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  menu_shell_signals[SELECTION_DONE] =
    g_signal_new ("selection-done",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkMenuShellClass, selection_done),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  menu_shell_signals[MOVE_CURRENT] =
    g_signal_new ("move_current",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkMenuShellClass, move_current),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1, 
		  GTK_TYPE_MENU_DIRECTION_TYPE);
  menu_shell_signals[ACTIVATE_CURRENT] =
    g_signal_new ("activate_current",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkMenuShellClass, activate_current),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE, 1, 
		  G_TYPE_BOOLEAN);
  menu_shell_signals[CANCEL] =
    g_signal_new ("cancel",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkMenuShellClass, cancel),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  menu_shell_signals[CYCLE_FOCUS] =
    _gtk_binding_signal_new ("cycle_focus",
			     G_OBJECT_CLASS_TYPE (object_class),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_CALLBACK (gtk_real_menu_shell_cycle_focus),
			     NULL, NULL,
			     _gtk_marshal_VOID__ENUM,
			     G_TYPE_NONE, 1,
			     GTK_TYPE_DIRECTION_TYPE);


  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Escape, 0,
				"cancel", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Return, 0,
				"activate_current", 1,
				G_TYPE_BOOLEAN,
				TRUE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Enter, 0,
				"activate_current", 1,
				G_TYPE_BOOLEAN,
				TRUE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_space, 0,
				"activate_current", 1,
				G_TYPE_BOOLEAN,
				FALSE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Space, 0,
				"activate_current", 1,
				G_TYPE_BOOLEAN,
				FALSE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_F10, 0,
				"cycle_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_FORWARD);
  gtk_binding_entry_add_signal (binding_set,
				GDK_F10, GDK_SHIFT_MASK,
				"cycle_focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, GTK_DIR_TAB_BACKWARD);
}

static GType
gtk_menu_shell_child_type (GtkContainer     *container)
{
  return GTK_TYPE_MENU_ITEM;
}

static void
gtk_menu_shell_init (GtkMenuShell *menu_shell)
{
  menu_shell->children = NULL;
  menu_shell->active_menu_item = NULL;
  menu_shell->parent_menu_shell = NULL;
  menu_shell->active = FALSE;
  menu_shell->have_grab = FALSE;
  menu_shell->have_xgrab = FALSE;
  menu_shell->ignore_leave = FALSE;
  menu_shell->button = 0;
  menu_shell->menu_flag = 0;
  menu_shell->activate_time = 0;
}

void
gtk_menu_shell_append (GtkMenuShell *menu_shell,
		       GtkWidget    *child)
{
  gtk_menu_shell_insert (menu_shell, child, -1);
}

void
gtk_menu_shell_prepend (GtkMenuShell *menu_shell,
			GtkWidget    *child)
{
  gtk_menu_shell_insert (menu_shell, child, 0);
}

void
gtk_menu_shell_insert (GtkMenuShell *menu_shell,
		       GtkWidget    *child,
		       gint          position)
{
  GtkMenuShellClass *class;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  class = GTK_MENU_SHELL_GET_CLASS (menu_shell);

  if (class->insert)
    class->insert (menu_shell, child, position);
}

static void
gtk_menu_shell_real_insert (GtkMenuShell *menu_shell,
			    GtkWidget    *child,
			    gint          position)
{
  menu_shell->children = g_list_insert (menu_shell->children, child, position);

  gtk_widget_set_parent (child, GTK_WIDGET (menu_shell));
}

void
gtk_menu_shell_deactivate (GtkMenuShell *menu_shell)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  g_signal_emit (menu_shell, menu_shell_signals[DEACTIVATE], 0);
}

static void
gtk_menu_shell_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (GTK_IS_MENU_SHELL (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

void
_gtk_menu_shell_activate (GtkMenuShell *menu_shell)
{
  if (!menu_shell->active)
    {
      gtk_grab_add (GTK_WIDGET (menu_shell));
      menu_shell->have_grab = TRUE;
      menu_shell->active = TRUE;
    }
}

static gint
gtk_menu_shell_button_press (GtkWidget      *widget,
			     GdkEventButton *event)
{
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;

  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  menu_shell = GTK_MENU_SHELL (widget);

  if (menu_shell->parent_menu_shell)
    {
      return gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
    }
  else if (!menu_shell->active || !menu_shell->button)
    {
      _gtk_menu_shell_activate (menu_shell);
      
      menu_shell->button = event->button;

      menu_item = gtk_menu_shell_get_item (menu_shell, (GdkEvent *)event);

      if (menu_item && _gtk_menu_item_is_selectable (menu_item))
	{
	  if ((menu_item->parent == widget) &&
	      (menu_item != menu_shell->active_menu_item))
	    {
	      if (GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement == GTK_TOP_BOTTOM)
		g_object_set_data (G_OBJECT (menu_shell),
				   "gtk-menushell-just-activated",
				   GUINT_TO_POINTER (1));
	      gtk_menu_shell_select_item (menu_shell, menu_item);
	    }
	}
    }
  else
    {
      widget = gtk_get_event_widget ((GdkEvent*) event);
      if (widget == GTK_WIDGET (menu_shell))
	{
	  gtk_menu_shell_deactivate (menu_shell);
	  g_signal_emit (menu_shell, menu_shell_signals[SELECTION_DONE], 0);
	}
    }

  return TRUE;
}

static gint
gtk_menu_shell_button_release (GtkWidget      *widget,
			       GdkEventButton *event)
{
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;
  gint deactivate;

  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  menu_shell = GTK_MENU_SHELL (widget);
  if (menu_shell->active)
    {
      gboolean deactivate_immediately = FALSE;

      if (menu_shell->button && (event->button != menu_shell->button))
	{
	  menu_shell->button = 0;
	  if (menu_shell->parent_menu_shell)
	    return gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	}

      menu_shell->button = 0;
      menu_item = gtk_menu_shell_get_item (menu_shell, (GdkEvent*) event);

      deactivate = TRUE;

      if (menu_item
	  && GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement == GTK_TOP_BOTTOM)
	{
	  if (g_object_get_data (G_OBJECT (menu_shell), "gtk-menushell-just-activated"))
	    g_object_set_data (G_OBJECT (menu_shell), "gtk-menushell-just-activated", NULL);
	  else
	    deactivate_immediately = TRUE;
	}

      if ((event->time - menu_shell->activate_time) > MENU_SHELL_TIMEOUT)
	{
	  if (deactivate_immediately)
	    {
	      gtk_menu_shell_deactivate (menu_shell);
	      return TRUE;
	    }
	    
	  if (menu_item && (menu_shell->active_menu_item == menu_item) &&
	      _gtk_menu_item_is_selectable (menu_item))
	    {
	      if (GTK_MENU_ITEM (menu_item)->submenu == NULL)
		{
		  gtk_menu_shell_activate_item (menu_shell, menu_item, TRUE);
		  return TRUE;
		}
	    }
	  else if (menu_item && !_gtk_menu_item_is_selectable (menu_item))
	    deactivate = FALSE;
	  else if (menu_shell->parent_menu_shell)
	    {
	      menu_shell->active = TRUE;
	      gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	      return TRUE;
	    }
	}
      else
	{
	  /* We only ever want to prevent deactivation on the first
           * press/release. Setting the time to zero is a bit of a
	   * hack, since we could be being triggered in the first
	   * few fractions of a second after a server time wraparound.
	   * the chances of that happening are ~1/10^6, without
	   * serious harm if we lose.
	   */
	  menu_shell->activate_time = 0;
	  deactivate = FALSE;
	}
      
      /* If the button click was very fast, or we ended up on a submenu,
       * leave the menu up
       */
      if (!deactivate || 
	  (menu_item && (menu_shell->active_menu_item == menu_item)))
	{
	  deactivate = FALSE;
	  menu_shell->ignore_leave = TRUE;
	}
      else
	deactivate = TRUE;

      if (deactivate)
	{
	  gtk_menu_shell_deactivate (menu_shell);
	  g_signal_emit (menu_shell, menu_shell_signals[SELECTION_DONE], 0);
	}
    }

  return TRUE;
}

static gint
gtk_menu_shell_key_press (GtkWidget	*widget,
			  GdkEventKey *event)
{
  GtkMenuShell *menu_shell;
  GtkWidget *toplevel;
  
  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
      
  menu_shell = GTK_MENU_SHELL (widget);

  if (!menu_shell->active_menu_item && menu_shell->parent_menu_shell)
    return gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent *)event);
  
  if (_gtk_bindings_activate_event (GTK_OBJECT (widget), event))
    return TRUE;

  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_IS_WINDOW (toplevel) &&
      _gtk_window_activate_key (GTK_WINDOW (toplevel), event))
    return TRUE;

  return FALSE;
}

static gint
gtk_menu_shell_enter_notify (GtkWidget        *widget,
			     GdkEventCrossing *event)
{
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;

  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  menu_shell = GTK_MENU_SHELL (widget);

  if (menu_shell->active)
    {
      menu_item = gtk_get_event_widget ((GdkEvent*) event);

      if (!menu_item ||
	  (GTK_IS_MENU_ITEM (menu_item) && 
	   !_gtk_menu_item_is_selectable (menu_item)))
	return TRUE;
      
      if ((menu_item->parent == widget) &&
	  (menu_shell->active_menu_item != menu_item) &&
	  GTK_IS_MENU_ITEM (menu_item))
	{
	  if (menu_shell->ignore_enter)
	    return TRUE;
	  
	  if ((event->detail != GDK_NOTIFY_INFERIOR) &&
	      (GTK_WIDGET_STATE (menu_item) != GTK_STATE_PRELIGHT))
	    {
	      gtk_menu_shell_select_item (menu_shell, menu_item);
	    }
	}
      else if (menu_shell->parent_menu_shell)
	{
	  gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	}
    }

  return TRUE;
}

static gint
gtk_menu_shell_leave_notify (GtkWidget        *widget,
			     GdkEventCrossing *event)
{
  GtkMenuShell *menu_shell;
  GtkMenuItem *menu_item;
  GtkWidget *event_widget;

  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_VISIBLE (widget))
    {
      menu_shell = GTK_MENU_SHELL (widget);
      event_widget = gtk_get_event_widget ((GdkEvent*) event);

      if (!event_widget || !GTK_IS_MENU_ITEM (event_widget))
	return TRUE;

      menu_item = GTK_MENU_ITEM (event_widget);

      if (menu_shell->ignore_leave)
	{
	  menu_shell->ignore_leave = FALSE;
	  return TRUE;
	}

      if (!_gtk_menu_item_is_selectable (event_widget))
	return TRUE;

      if ((menu_shell->active_menu_item == event_widget) &&
	  (menu_item->submenu == NULL))
	{
	  if ((event->detail != GDK_NOTIFY_INFERIOR) &&
	      (GTK_WIDGET_STATE (menu_item) != GTK_STATE_NORMAL))
	    {
	      gtk_menu_shell_deselect (menu_shell);
	    }
	}
      else if (menu_shell->parent_menu_shell)
	{
	  gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	}
    }

  return TRUE;
}

static void
gtk_menu_shell_add (GtkContainer *container,
		    GtkWidget    *widget)
{
  gtk_menu_shell_append (GTK_MENU_SHELL (container), widget);
}

static void
gtk_menu_shell_remove (GtkContainer *container,
		       GtkWidget    *widget)
{
  GtkMenuShell *menu_shell;
  gint was_visible;
  
  g_return_if_fail (GTK_IS_MENU_SHELL (container));
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  
  was_visible = GTK_WIDGET_VISIBLE (widget);
  menu_shell = GTK_MENU_SHELL (container);
  menu_shell->children = g_list_remove (menu_shell->children, widget);
  
  if (widget == menu_shell->active_menu_item)
    {
      gtk_item_deselect (GTK_ITEM (menu_shell->active_menu_item));
      menu_shell->active_menu_item = NULL;
    }

  gtk_widget_unparent (widget);
  
  /* queue resize regardless of GTK_WIDGET_VISIBLE (container),
   * since that's what is needed by toplevels.
   */
  if (was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_menu_shell_forall (GtkContainer *container,
		       gboolean      include_internals,
		       GtkCallback   callback,
		       gpointer      callback_data)
{
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;

  g_return_if_fail (GTK_IS_MENU_SHELL (container));
  g_return_if_fail (callback != NULL);

  menu_shell = GTK_MENU_SHELL (container);

  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      (* callback) (child, callback_data);
    }
}


static void
gtk_real_menu_shell_deactivate (GtkMenuShell *menu_shell)
{
  if (menu_shell->active)
    {
      menu_shell->button = 0;
      menu_shell->active = FALSE;

      if (menu_shell->active_menu_item)
	{
	  gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
	  menu_shell->active_menu_item = NULL;
	}

      if (menu_shell->have_grab)
	{
	  menu_shell->have_grab = FALSE;
	  gtk_grab_remove (GTK_WIDGET (menu_shell));
	}
      if (menu_shell->have_xgrab)
	{
	  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (menu_shell));
	  
	  menu_shell->have_xgrab = FALSE;
	  gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);
	  gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);
	}
    }
}

static gint
gtk_menu_shell_is_item (GtkMenuShell *menu_shell,
			GtkWidget    *child)
{
  GtkWidget *parent;

  g_return_val_if_fail (GTK_IS_MENU_SHELL (menu_shell), FALSE);
  g_return_val_if_fail (child != NULL, FALSE);

  parent = child->parent;
  while (parent && GTK_IS_MENU_SHELL (parent))
    {
      if (parent == (GtkWidget*) menu_shell)
	return TRUE;
      parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }

  return FALSE;
}

static GtkWidget*
gtk_menu_shell_get_item (GtkMenuShell *menu_shell,
			 GdkEvent     *event)
{
  GtkWidget *menu_item;

  menu_item = gtk_get_event_widget ((GdkEvent*) event);
  
  while (menu_item && !GTK_IS_MENU_ITEM (menu_item))
    menu_item = menu_item->parent;

  if (menu_item && gtk_menu_shell_is_item (menu_shell, menu_item))
    return menu_item;
  else
    return NULL;
}

/* Handlers for action signals */

void
gtk_menu_shell_select_item (GtkMenuShell *menu_shell,
			    GtkWidget    *menu_item)
{
  GtkMenuShellClass *class;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  class = GTK_MENU_SHELL_GET_CLASS (menu_shell);

  if (class->select_item &&
      !(menu_shell->active &&
	menu_shell->active_menu_item == menu_item))
    class->select_item (menu_shell, menu_item);
}

void _gtk_menu_item_set_placement (GtkMenuItem         *menu_item,
				   GtkSubmenuPlacement  placement);

static void
gtk_menu_shell_real_select_item (GtkMenuShell *menu_shell,
				 GtkWidget    *menu_item)
{
  gtk_menu_shell_deselect (menu_shell);

  if (!_gtk_menu_item_is_selectable (menu_item))
    return;

  menu_shell->active_menu_item = menu_item;
  _gtk_menu_item_set_placement (GTK_MENU_ITEM (menu_shell->active_menu_item),
			       GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement);
  gtk_menu_item_select (GTK_MENU_ITEM (menu_shell->active_menu_item));

  /* This allows the bizarre radio buttons-with-submenus-display-history
   * behavior
   */
  if (GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu)
    gtk_widget_activate (menu_shell->active_menu_item);
}

void
gtk_menu_shell_deselect (GtkMenuShell *menu_shell)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  if (menu_shell->active_menu_item)
    {
      gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
      menu_shell->active_menu_item = NULL;
    }
}

void
gtk_menu_shell_activate_item (GtkMenuShell      *menu_shell,
			      GtkWidget         *menu_item,
			      gboolean           force_deactivate)
{
  GSList *slist, *shells = NULL;
  gboolean deactivate = force_deactivate;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  if (!deactivate)
    deactivate = GTK_MENU_ITEM_GET_CLASS (menu_item)->hide_on_activate;

  g_object_ref (menu_shell);

  if (deactivate)
    {
      GtkMenuShell *parent_menu_shell = menu_shell;

      do
	{
	  g_object_ref (parent_menu_shell);
	  shells = g_slist_prepend (shells, parent_menu_shell);
	  parent_menu_shell = (GtkMenuShell*) parent_menu_shell->parent_menu_shell;
	}
      while (parent_menu_shell);
      shells = g_slist_reverse (shells);

      gtk_menu_shell_deactivate (menu_shell);
  
      /* flush the x-queue, so any grabs are removed and
       * the menu is actually taken down
       */
      gdk_display_sync (gtk_widget_get_display (menu_item));
    }

  gtk_widget_activate (menu_item);

  for (slist = shells; slist; slist = slist->next)
    {
      g_signal_emit (slist->data, menu_shell_signals[SELECTION_DONE], 0);
      g_object_unref (slist->data);
    }
  g_slist_free (shells);

  g_object_unref (menu_shell);
}

/* Distance should be +/- 1 */
static void
gtk_menu_shell_move_selected (GtkMenuShell  *menu_shell, 
			      gint           distance)
{
  if (menu_shell->active_menu_item)
    {
      GList *node = g_list_find (menu_shell->children,
				 menu_shell->active_menu_item);
      GList *start_node = node;
      
      if (distance > 0)
	{
	  node = node->next;
	  while (node != start_node && 
		 (!node || !_gtk_menu_item_is_selectable (node->data)))
	    {
	      if (!node)
		node = menu_shell->children;
	      else
		node = node->next;
	    }
	}
      else
	{
	  node = node->prev;
	  while (node != start_node &&
		 (!node || !_gtk_menu_item_is_selectable (node->data)))
	    {
	      if (!node)
		node = g_list_last (menu_shell->children);
	      else
		node = node->prev;
	    }
	}
      
      if (node)
	gtk_menu_shell_select_item (menu_shell, node->data);
    }
}

/**
 * gtk_menu_shell_select_first:
 * @menu_shell: a #GtkMenuShell
 * @search_sensitive: if %TRUE, search for the first selectable
 *                    menu item, otherwise select nothing if
 *                    the first item isn't sensitive. This
 *                    should be %FALSE if the menu is being
 *                    popped up initially.
 * 
 * Select the first visible or selectable child of the menu shell;
 * don't select tearoff items unless the only item is a tearoff
 * item.
 *
 * Since: 2.2
 **/
void
gtk_menu_shell_select_first (GtkMenuShell *menu_shell,
			     gboolean      search_sensitive)
{
  GtkWidget *to_select = NULL;
  GList *tmp_list;

  tmp_list = menu_shell->children;
  while (tmp_list)
    {
      GtkWidget *child = tmp_list->data;
      
      if ((!search_sensitive && GTK_WIDGET_VISIBLE (child)) ||
	  _gtk_menu_item_is_selectable (child))
	{
	  to_select = child;
	  if (!GTK_IS_TEAROFF_MENU_ITEM (child))
	    break;
	}
      
      tmp_list = tmp_list->next;
    }

  if (to_select)
    gtk_menu_shell_select_item (menu_shell, to_select);
}

void
_gtk_menu_shell_select_last (GtkMenuShell *menu_shell,
			     gboolean      search_sensitive)
{
  GtkWidget *to_select = NULL;
  GList *tmp_list;

  tmp_list = g_list_last (menu_shell->children);
  while (tmp_list)
    {
      GtkWidget *child = tmp_list->data;
      
      if ((!search_sensitive && GTK_WIDGET_VISIBLE (child)) ||
	  _gtk_menu_item_is_selectable (child))
	{
	  to_select = child;
	  if (!GTK_IS_TEAROFF_MENU_ITEM (child))
	    break;
	}
      
      tmp_list = tmp_list->prev;
    }

  if (to_select)
    gtk_menu_shell_select_item (menu_shell, to_select);
}

static void
gtk_menu_shell_select_submenu_first (GtkMenuShell     *menu_shell)
{
  GtkMenuItem *menu_item;

  menu_item = GTK_MENU_ITEM (menu_shell->active_menu_item); 
  
  if (menu_item->submenu)
    gtk_menu_shell_select_first (GTK_MENU_SHELL (menu_item->submenu), TRUE);
}

static void
gtk_real_menu_shell_move_current (GtkMenuShell      *menu_shell,
				  GtkMenuDirectionType direction)
{
  GtkMenuShell *parent_menu_shell = NULL;
  gboolean had_selection;

  had_selection = menu_shell->active_menu_item != NULL;

  if (menu_shell->parent_menu_shell)
    parent_menu_shell = GTK_MENU_SHELL (menu_shell->parent_menu_shell);
  
  switch (direction)
    {
    case GTK_MENU_DIR_PARENT:
      if (parent_menu_shell)
	{
	  if (GTK_MENU_SHELL_GET_CLASS (parent_menu_shell)->submenu_placement == 
		       GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement)
	    gtk_menu_shell_deselect (menu_shell);
	  else 
	    {
	      gtk_menu_shell_move_selected (parent_menu_shell, -1);
	      gtk_menu_shell_select_submenu_first (parent_menu_shell); 
	    }
	}
      /* If there is no parent and the submenu is in the opposite direction
       * to the menu, then make the PARENT direction wrap around to
       * the bottom of the submenu.
       */
      else if (menu_shell->active_menu_item &&
	       _gtk_menu_item_is_selectable (menu_shell->active_menu_item) &&
	       GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu)
	{
	  GtkMenuShell *submenu = GTK_MENU_SHELL (GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu);

	  if (GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement !=
	      GTK_MENU_SHELL_GET_CLASS (submenu)->submenu_placement)
	    _gtk_menu_shell_select_last (submenu, TRUE);
	}
      break;
      
    case GTK_MENU_DIR_CHILD:
      if (menu_shell->active_menu_item &&
	  _gtk_menu_item_is_selectable (menu_shell->active_menu_item) &&
	  GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu)
	{
	  gtk_menu_shell_select_submenu_first (menu_shell);
	}
      else
	{
	  /* Try to find a menu running the opposite direction */
	  while (parent_menu_shell && 
		 (GTK_MENU_SHELL_GET_CLASS (parent_menu_shell)->submenu_placement ==
		  GTK_MENU_SHELL_GET_CLASS (menu_shell)->submenu_placement))
            {
              GtkWidget *tmp_widget = parent_menu_shell->parent_menu_shell;

              if (tmp_widget)
                parent_menu_shell = GTK_MENU_SHELL (tmp_widget);
              else
                parent_menu_shell = NULL;
            }

	  if (parent_menu_shell)
	    {
	      gtk_menu_shell_move_selected (parent_menu_shell, 1);
	      gtk_menu_shell_select_submenu_first (parent_menu_shell);
	    }
	}
      break;
      
    case GTK_MENU_DIR_PREV:
      gtk_menu_shell_move_selected (menu_shell, -1);
      if (!had_selection &&
	  !menu_shell->active_menu_item &&
	  menu_shell->children)
	_gtk_menu_shell_select_last (menu_shell, TRUE);
      break;
    case GTK_MENU_DIR_NEXT:
      gtk_menu_shell_move_selected (menu_shell, 1);
      if (!had_selection &&
	  !menu_shell->active_menu_item &&
	  menu_shell->children)
	gtk_menu_shell_select_first (menu_shell, TRUE);
      break;
    }
  
}

static void
gtk_real_menu_shell_activate_current (GtkMenuShell      *menu_shell,
				      gboolean           force_hide)
{
  if (menu_shell->active_menu_item &&
      _gtk_menu_item_is_selectable (menu_shell->active_menu_item) &&
      GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu == NULL)
    {
      gtk_menu_shell_activate_item (menu_shell,
				    menu_shell->active_menu_item,
				    force_hide);
    }
}

static void
gtk_real_menu_shell_cancel (GtkMenuShell      *menu_shell)
{
  /* Unset the active menu item so gtk_menu_popdown() doesn't see it.
   */
  gtk_menu_shell_deselect (menu_shell);
  
  gtk_menu_shell_deactivate (menu_shell);
  g_signal_emit (menu_shell, menu_shell_signals[SELECTION_DONE], 0);
}

static void
gtk_real_menu_shell_cycle_focus (GtkMenuShell      *menu_shell,
				 GtkDirectionType   dir)
{
  while (menu_shell && !GTK_IS_MENU_BAR (menu_shell))
    {
      if (menu_shell->parent_menu_shell)
	menu_shell = GTK_MENU_SHELL (menu_shell->parent_menu_shell);
      else
	menu_shell = NULL;
    }

  if (menu_shell)
    _gtk_menu_bar_cycle_focus (GTK_MENU_BAR (menu_shell), dir);
}

gint
_gtk_menu_shell_get_popup_delay (GtkMenuShell *menu_shell)
{
  GtkMenuShellClass *klass = GTK_MENU_SHELL_GET_CLASS (menu_shell);
  
  if (klass->get_popup_delay)
    {
      return klass->get_popup_delay (menu_shell);
    }
  else
    {
      gint popup_delay;
      GtkWidget *widget = GTK_WIDGET (menu_shell);
      
      g_object_get (G_OBJECT (gtk_widget_get_settings (widget)),
		    "gtk-menu-popup-delay", &popup_delay,
		    NULL);
      
      return popup_delay;
    }
}
