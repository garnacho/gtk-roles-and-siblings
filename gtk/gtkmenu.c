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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include <ctype.h>
#include <string.h> /* memset */
#include "gdk/gdkkeysyms.h"
#include "gtkaccelmap.h"
#include "gtkbindings.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtksignal.h"
#include "gtkwindow.h"
#include "gtkhbox.h"
#include "gtkvscrollbar.h"
#include "gtksettings.h"
#include "gtkintl.h"


#define MENU_ITEM_CLASS(w)   GTK_MENU_ITEM_GET_CLASS (w)
#define	MENU_NEEDS_RESIZE(m) GTK_MENU_SHELL (m)->menu_flag

#define SUBMENU_NAV_REGION_PADDING 2
#define SUBMENU_NAV_HYSTERESIS_TIMEOUT 333

#define MENU_SCROLL_STEP 10
#define MENU_SCROLL_ARROW_HEIGHT 16
#define MENU_SCROLL_FAST_ZONE 4
#define MENU_SCROLL_TIMEOUT1 150
#define MENU_SCROLL_TIMEOUT2 50

typedef struct _GtkMenuAttachData	GtkMenuAttachData;

struct _GtkMenuAttachData
{
  GtkWidget *attach_widget;
  GtkMenuDetachFunc detacher;
};

enum {
  PROP_0,
  PROP_TEAROFF_TITLE
};

static void     gtk_menu_class_init        (GtkMenuClass     *klass);
static void     gtk_menu_init              (GtkMenu          *menu);
static void     gtk_menu_set_property      (GObject      *object,
					    guint         prop_id,
					    const GValue *value,
					    GParamSpec   *pspec);
static void     gtk_menu_get_property      (GObject     *object,
					    guint        prop_id,
					    GValue      *value,
					    GParamSpec  *pspec);
static void     gtk_menu_destroy           (GtkObject        *object);
static void     gtk_menu_finalize          (GObject          *object);
static void     gtk_menu_realize           (GtkWidget        *widget);
static void     gtk_menu_unrealize         (GtkWidget        *widget);
static void     gtk_menu_size_request      (GtkWidget        *widget,
					    GtkRequisition   *requisition);
static void     gtk_menu_size_allocate     (GtkWidget        *widget,
					    GtkAllocation    *allocation);
static void     gtk_menu_paint             (GtkWidget        *widget,
					    GdkEventExpose   *expose);
static void     gtk_menu_show              (GtkWidget        *widget);
static gboolean gtk_menu_expose            (GtkWidget        *widget,
					    GdkEventExpose   *event);
static gboolean gtk_menu_key_press         (GtkWidget        *widget,
					    GdkEventKey      *event);
static gboolean gtk_menu_motion_notify     (GtkWidget        *widget,
					    GdkEventMotion   *event);
static gboolean gtk_menu_enter_notify      (GtkWidget        *widget,
					    GdkEventCrossing *event);
static gboolean gtk_menu_leave_notify      (GtkWidget        *widget,
					    GdkEventCrossing *event);
static void     gtk_menu_scroll_to         (GtkMenu          *menu,
					    gint              offset);
static void     gtk_menu_stop_scrolling    (GtkMenu          *menu);
static gboolean gtk_menu_scroll_timeout    (gpointer          data);
static void     gtk_menu_scroll_item_visible (GtkMenuShell    *menu_shell,
					      GtkWidget       *menu_item);
static void     gtk_menu_select_item       (GtkMenuShell     *menu_shell,
					    GtkWidget        *menu_item);
static void     gtk_menu_real_insert       (GtkMenuShell     *menu_shell,
					    GtkWidget        *child,
					    gint              position);
static void     gtk_menu_scrollbar_changed (GtkAdjustment    *adjustment,
					    GtkMenu          *menu);
static void     gtk_menu_handle_scrolling  (GtkMenu          *menu,
					    gboolean         enter);
static void     gtk_menu_set_tearoff_hints (GtkMenu          *menu,
					    gint             width);

static void     gtk_menu_stop_navigating_submenu       (GtkMenu          *menu);
static gboolean gtk_menu_stop_navigating_submenu_cb    (gpointer          user_data);
static gboolean gtk_menu_navigating_submenu            (GtkMenu          *menu,
							gint              event_x,
							gint              event_y);
static void     gtk_menu_set_submenu_navigation_region (GtkMenu          *menu,
							GtkMenuItem      *menu_item,
							GdkEventCrossing *event);
 
static void gtk_menu_deactivate	    (GtkMenuShell      *menu_shell);
static void gtk_menu_show_all       (GtkWidget         *widget);
static void gtk_menu_hide_all       (GtkWidget         *widget);
static void gtk_menu_position       (GtkMenu           *menu);
static void gtk_menu_reparent       (GtkMenu           *menu, 
				     GtkWidget         *new_parent, 
				     gboolean           unrealize);
static void gtk_menu_remove         (GtkContainer      *menu,
				     GtkWidget         *widget);

static void gtk_menu_update_title   (GtkMenu           *menu);

static void       menu_grab_transfer_window_destroy (GtkMenu *menu);
static GdkWindow *menu_grab_transfer_window_get     (GtkMenu *menu);

static void _gtk_menu_refresh_accel_paths (GtkMenu *menu,
					   gboolean group_changed);

static GtkMenuShellClass *parent_class = NULL;
static const gchar	 *attach_data_key = "gtk-menu-attach-data";

GtkType
gtk_menu_get_type (void)
{
  static GtkType menu_type = 0;
  
  if (!menu_type)
    {
      static const GtkTypeInfo menu_info =
      {
	"GtkMenu",
	sizeof (GtkMenu),
	sizeof (GtkMenuClass),
	(GtkClassInitFunc) gtk_menu_class_init,
	(GtkObjectInitFunc) gtk_menu_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      menu_type = gtk_type_unique (gtk_menu_shell_get_type (), &menu_info);
    }
  
  return menu_type;
}

static void
gtk_menu_class_init (GtkMenuClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
  GtkMenuShellClass *menu_shell_class = GTK_MENU_SHELL_CLASS (class);
  GtkBindingSet *binding_set;
  
  parent_class = g_type_class_peek_parent (class);
  
  gobject_class->finalize = gtk_menu_finalize;
  gobject_class->set_property = gtk_menu_set_property;
  gobject_class->get_property = gtk_menu_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_TEAROFF_TITLE,
                                   g_param_spec_string ("tearoff-title",
                                                        _("Tearoff Title"),
                                                        _("A title that may be displayed by the window manager when this menu is torn-off."),
                                                        "",
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
  object_class->destroy = gtk_menu_destroy;
  
  widget_class->realize = gtk_menu_realize;
  widget_class->unrealize = gtk_menu_unrealize;
  widget_class->size_request = gtk_menu_size_request;
  widget_class->size_allocate = gtk_menu_size_allocate;
  widget_class->show = gtk_menu_show;
  widget_class->expose_event = gtk_menu_expose;
  widget_class->key_press_event = gtk_menu_key_press;
  widget_class->motion_notify_event = gtk_menu_motion_notify;
  widget_class->show_all = gtk_menu_show_all;
  widget_class->hide_all = gtk_menu_hide_all;
  widget_class->enter_notify_event = gtk_menu_enter_notify;
  widget_class->leave_notify_event = gtk_menu_leave_notify;

  container_class->remove = gtk_menu_remove;
  
  menu_shell_class->submenu_placement = GTK_LEFT_RIGHT;
  menu_shell_class->deactivate = gtk_menu_deactivate;
  menu_shell_class->select_item = gtk_menu_select_item;
  menu_shell_class->insert = gtk_menu_real_insert;

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Up, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Up, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Down, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Down, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Left, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Left, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Right, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);
  gtk_binding_entry_add_signal (binding_set,
				GDK_KP_Right, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);

  gtk_settings_install_property (g_param_spec_boolean ("gtk-can-change-accels",
						       _("Can change accelerators"),
						       _("Whether menu accelerators can be changed by pressing a key over the menu item."),
						       FALSE,
						       G_PARAM_READWRITE));
}


static void 
gtk_menu_set_property (GObject      *object,
		       guint         prop_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
  GtkMenu *menu;
  
  menu = GTK_MENU (object);
  
  switch (prop_id)
    {
    case PROP_TEAROFF_TITLE:
      gtk_menu_set_title (menu, g_value_get_string (value));
      break;	  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_menu_get_property (GObject     *object,
		       guint        prop_id,
		       GValue      *value,
		       GParamSpec  *pspec)
{
  GtkMenu *menu;
  
  menu = GTK_MENU (object);
  
  switch (prop_id)
    {
    case PROP_TEAROFF_TITLE:
      g_value_set_string (value, gtk_menu_get_title (menu));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_menu_window_event (GtkWidget *window,
		       GdkEvent  *event,
		       GtkWidget *menu)
{
  gboolean handled = FALSE;

  gtk_widget_ref (window);
  gtk_widget_ref (menu);

  switch (event->type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      handled = gtk_widget_event (menu, event);
      break;
    default:
      break;
    }

  gtk_widget_unref (window);
  gtk_widget_unref (menu);

  return handled;
}

static void
gtk_menu_init (GtkMenu *menu)
{
  menu->parent_menu_item = NULL;
  menu->old_active_menu_item = NULL;
  menu->accel_group = NULL;
  menu->position_func = NULL;
  menu->position_func_data = NULL;
  menu->toggle_size = 0;

  menu->toplevel = g_object_connect (gtk_widget_new (GTK_TYPE_WINDOW,
						     "type", GTK_WINDOW_POPUP,
						     "child", menu,
						     NULL),
				     "signal::event", gtk_menu_window_event, menu,
				     "signal::destroy", gtk_widget_destroyed, &menu->toplevel,
				     NULL);
  gtk_window_set_policy (GTK_WINDOW (menu->toplevel),
			 FALSE, FALSE, TRUE);
  gtk_window_set_mnemonic_modifier (GTK_WINDOW (menu->toplevel), 0);

  /* Refloat the menu, so that reference counting for the menu isn't
   * affected by it being a child of the toplevel
   */
  GTK_WIDGET_SET_FLAGS (menu, GTK_FLOATING);
  menu->needs_destruction_ref_count = TRUE;

  menu->view_window = NULL;
  menu->bin_window = NULL;

  menu->scroll_offset = 0;
  menu->scroll_step  = 0;
  menu->timeout_id = 0;
  menu->scroll_fast = FALSE;
  
  menu->tearoff_window = NULL;
  menu->tearoff_hbox = NULL;
  menu->torn_off = FALSE;
  menu->tearoff_active = FALSE;
  menu->tearoff_adjustment = NULL;
  menu->tearoff_scrollbar = NULL;

  menu->upper_arrow_visible = FALSE;
  menu->lower_arrow_visible = FALSE;
  menu->upper_arrow_prelight = FALSE;
  menu->lower_arrow_prelight = FALSE;
  
  MENU_NEEDS_RESIZE (menu) = TRUE;
}

static void
gtk_menu_destroy (GtkObject *object)
{
  GtkMenu *menu;
  GtkMenuAttachData *data;

  g_return_if_fail (GTK_IS_MENU (object));

  menu = GTK_MENU (object);

  gtk_menu_stop_scrolling (menu);
  
  data = gtk_object_get_data (object, attach_data_key);
  if (data)
    gtk_menu_detach (menu);
  
  gtk_menu_stop_navigating_submenu (menu);

  if (menu->old_active_menu_item)
    {
      gtk_widget_unref (menu->old_active_menu_item);
      menu->old_active_menu_item = NULL;
    }

  /* Add back the reference count for being a child */
  if (menu->needs_destruction_ref_count)
    {
      menu->needs_destruction_ref_count = FALSE;
      gtk_object_ref (object);
    }
  
  if (menu->accel_group)
    {
      g_object_unref (menu->accel_group);
      menu->accel_group = NULL;
    }

  if (menu->toplevel)
    gtk_widget_destroy (menu->toplevel);
  if (menu->tearoff_window)
    gtk_widget_destroy (menu->tearoff_window);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_menu_finalize (GObject *object)
{
  GtkMenu *menu = GTK_MENU (object);

  g_free (menu->accel_path);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
gtk_menu_attach_to_widget (GtkMenu	       *menu,
			   GtkWidget	       *attach_widget,
			   GtkMenuDetachFunc	detacher)
{
  GtkMenuAttachData *data;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_WIDGET (attach_widget));
  g_return_if_fail (detacher != NULL);
  
  /* keep this function in sync with gtk_widget_set_parent()
   */
  
  data = gtk_object_get_data (GTK_OBJECT (menu), attach_data_key);
  if (data)
    {
      g_warning ("gtk_menu_attach_to_widget(): menu already attached to %s",
		 gtk_type_name (GTK_OBJECT_TYPE (data->attach_widget)));
      return;
    }
  
  gtk_object_ref (GTK_OBJECT (menu));
  gtk_object_sink (GTK_OBJECT (menu));
  
  data = g_new (GtkMenuAttachData, 1);
  data->attach_widget = attach_widget;
  data->detacher = detacher;
  gtk_object_set_data (GTK_OBJECT (menu), attach_data_key, data);
  
  if (GTK_WIDGET_STATE (menu) != GTK_STATE_NORMAL)
    gtk_widget_set_state (GTK_WIDGET (menu), GTK_STATE_NORMAL);
  
  /* we don't need to set the style here, since
   * we are a toplevel widget.
   */

  /* Fallback title for menu comes from attach widget */
  gtk_menu_update_title (menu);
}

GtkWidget*
gtk_menu_get_attach_widget (GtkMenu *menu)
{
  GtkMenuAttachData *data;
  
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);
  
  data = gtk_object_get_data (GTK_OBJECT (menu), attach_data_key);
  if (data)
    return data->attach_widget;
  return NULL;
}

void
gtk_menu_detach (GtkMenu *menu)
{
  GtkMenuAttachData *data;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  
  /* keep this function in sync with gtk_widget_unparent()
   */
  data = gtk_object_get_data (GTK_OBJECT (menu), attach_data_key);
  if (!data)
    {
      g_warning ("gtk_menu_detach(): menu is not attached");
      return;
    }
  gtk_object_remove_data (GTK_OBJECT (menu), attach_data_key);
  
  data->detacher (data->attach_widget, menu);
  
  if (GTK_WIDGET_REALIZED (menu))
    gtk_widget_unrealize (GTK_WIDGET (menu));
  
  g_free (data);
  
  /* Fallback title for menu comes from attach widget */
  gtk_menu_update_title (menu);

  gtk_widget_unref (GTK_WIDGET (menu));
}

static void 
gtk_menu_remove (GtkContainer *container,
		 GtkWidget    *widget)
{
  GtkMenu *menu;
  g_return_if_fail (GTK_IS_MENU (container));
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));

  menu = GTK_MENU (container);

  /* Clear out old_active_menu_item if it matches the item we are removing
   */
  if (menu->old_active_menu_item == widget)
    {
      gtk_widget_unref (menu->old_active_menu_item);
      menu->old_active_menu_item = NULL;
    }

  GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}


GtkWidget*
gtk_menu_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_menu_get_type ()));
}

static void
gtk_menu_real_insert (GtkMenuShell     *menu_shell,
		      GtkWidget        *child,
		      gint              position)
{
  if (GTK_WIDGET_REALIZED (menu_shell))
    gtk_widget_set_parent_window (child, GTK_MENU (menu_shell)->bin_window);
  
  GTK_MENU_SHELL_CLASS (parent_class)->insert (menu_shell, child, position);
}

static void
gtk_menu_tearoff_bg_copy (GtkMenu *menu)
{
  GtkWidget *widget;
  gint width, height;

  widget = GTK_WIDGET (menu);

  if (menu->torn_off)
    {
      GdkPixmap *pixmap;
      GdkGC *gc;
      GdkGCValues gc_values;

      menu->tearoff_active = FALSE;
      menu->saved_scroll_offset = menu->scroll_offset;
      
      gc_values.subwindow_mode = GDK_INCLUDE_INFERIORS;
      gc = gdk_gc_new_with_values (widget->window,
				   &gc_values, GDK_GC_SUBWINDOW);
      
      gdk_window_get_size (menu->tearoff_window->window, &width, &height);
      
      pixmap = gdk_pixmap_new (menu->tearoff_window->window,
			       width,
			       height,
			       -1);

      gdk_draw_pixmap (pixmap, gc,
		       menu->tearoff_window->window,
		       0, 0, 0, 0, -1, -1);
      gdk_gc_unref (gc);

      gtk_widget_set_usize (menu->tearoff_window,
			    width,
			    height);

      gdk_window_set_back_pixmap (menu->tearoff_window->window, pixmap, FALSE);
      gdk_pixmap_unref (pixmap);
    }
}

static gboolean
popup_grab_on_window (GdkWindow *window,
		      guint32    activate_time)
{
  if ((gdk_pointer_grab (window, TRUE,
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			 GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
			 GDK_POINTER_MOTION_MASK,
			 NULL, NULL, activate_time) == 0))
    {
      if (gdk_keyboard_grab (window, TRUE,
			     activate_time) == 0)
	return TRUE;
      else
	{
	  gdk_pointer_ungrab (activate_time);
	  return FALSE;
	}
    }

  return FALSE;
}

void
gtk_menu_popup (GtkMenu		    *menu,
		GtkWidget	    *parent_menu_shell,
		GtkWidget	    *parent_menu_item,
		GtkMenuPositionFunc  func,
		gpointer	     data,
		guint		     button,
		guint32		     activate_time)
{
  GtkWidget *widget;
  GtkWidget *xgrab_shell;
  GtkWidget *parent;
  GdkEvent *current_event;
  GtkMenuShell *menu_shell;

  g_return_if_fail (GTK_IS_MENU (menu));
  
  widget = GTK_WIDGET (menu);
  menu_shell = GTK_MENU_SHELL (menu);
  
  menu_shell->parent_menu_shell = parent_menu_shell;

  /* Find the last viewable ancestor, and make an X grab on it
   */
  parent = GTK_WIDGET (menu);
  xgrab_shell = NULL;
  while (parent)
    {
      gboolean viewable = TRUE;
      GtkWidget *tmp = parent;
      
      while (tmp)
	{
	  if (!GTK_WIDGET_MAPPED (tmp))
	    {
	      viewable = FALSE;
	      break;
	    }
	  tmp = tmp->parent;
	}
      
      if (viewable)
	xgrab_shell = parent;
      
      parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }

  /* We want to receive events generated when we map the menu; unfortunately,
   * since there is probably already an implicit grab in place from the
   * button that the user used to pop up the menu, we won't receive then --
   * in particular, the EnterNotify when the menu pops up under the pointer.
   *
   * If we are grabbing on a parent menu shell, no problem; just grab on
   * that menu shell first before popping up the window with owner_events = TRUE.
   *
   * When grabbing on the menu itself, things get more convuluted - we
   * we do an explicit grab on a specially created window with
   * owner_events = TRUE, which we override further down with a grab
   * on the menu. (We can't grab on the menu until it is mapped; we
   * probably could just leave the grab on the other window, with a
   * little reorganization of the code in gtkmenu*).
   */
  if (xgrab_shell && xgrab_shell != widget)
    {
      if (popup_grab_on_window (xgrab_shell->window, activate_time))
	GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
    }
  else
    {
      GdkWindow *transfer_window;

      xgrab_shell = widget;
      transfer_window = menu_grab_transfer_window_get (menu);
      if (popup_grab_on_window (transfer_window, activate_time))
	GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
    }

  if (!GTK_MENU_SHELL (xgrab_shell)->have_xgrab)
    {
      /* We failed to make our pointer/keyboard grab. Rather than leaving the user
       * with a stuck up window, we just abort here. Presumably the user will
       * try again.
       */
      menu_shell->parent_menu_shell = NULL;
      return;
    }

  menu_shell->active = TRUE;
  menu_shell->button = button;

  /* If we are popping up the menu from something other than, a button
   * press then, as a heuristic, we ignore enter events for the menu
   * until we get a MOTION_NOTIFY.  
   */

  current_event = gtk_get_current_event ();
  if (current_event)
    {
      if ((current_event->type != GDK_BUTTON_PRESS) &&
	  (current_event->type != GDK_ENTER_NOTIFY))
	menu_shell->ignore_enter = TRUE;

      gdk_event_free (current_event);
    }

  if (menu->torn_off)
    {
      gtk_menu_tearoff_bg_copy (menu);

      gtk_menu_reparent (menu, menu->toplevel, FALSE);
    }
  
  menu->parent_menu_item = parent_menu_item;
  menu->position_func = func;
  menu->position_func_data = data;
  menu_shell->activate_time = activate_time;

  gtk_menu_position (menu);

  /* We need to show the menu _here_ because code expects to be
   * able to tell if the menu is onscreen by looking at the
   * GTK_WIDGET_VISIBLE (menu)
   */
  gtk_widget_show (GTK_WIDGET (menu));
  gtk_widget_show (menu->toplevel);

  gtk_menu_scroll_to (menu, menu->scroll_offset);

  if (xgrab_shell == widget)
    popup_grab_on_window (widget->window, activate_time); /* Should always succeed */

  gtk_grab_add (GTK_WIDGET (menu));
}

void
gtk_menu_popdown (GtkMenu *menu)
{
  GtkMenuShell *menu_shell;

  g_return_if_fail (GTK_IS_MENU (menu));
  
  menu_shell = GTK_MENU_SHELL (menu);
  
  menu_shell->parent_menu_shell = NULL;
  menu_shell->active = FALSE;
  menu_shell->ignore_enter = FALSE;

  gtk_menu_stop_scrolling (menu);
  
  gtk_menu_stop_navigating_submenu (menu);
  
  if (menu_shell->active_menu_item)
    {
      if (menu->old_active_menu_item)
	gtk_widget_unref (menu->old_active_menu_item);
      menu->old_active_menu_item = menu_shell->active_menu_item;
      gtk_widget_ref (menu->old_active_menu_item);
    }

  gtk_menu_shell_deselect (menu_shell);
  
  /* The X Grab, if present, will automatically be removed when we hide
   * the window */
  gtk_widget_hide (menu->toplevel);

  if (menu->torn_off)
    {
      gint width, height;
      gdk_window_get_size (menu->tearoff_window->window, &width, &height);
      gtk_widget_set_usize (menu->tearoff_window,
			    -1,
			    height);
      
      if (GTK_BIN (menu->toplevel)->child) 
	{
	  gtk_menu_reparent (menu, menu->tearoff_hbox, TRUE);
	} 
      else
	{
	  /* We popped up the menu from the tearoff, so we need to 
	   * release the grab - we aren't actually hiding the menu.
	   */
	  if (menu_shell->have_xgrab)
	    {
	      gdk_pointer_ungrab (GDK_CURRENT_TIME);
	      gdk_keyboard_ungrab (GDK_CURRENT_TIME);
	    }
	}

      /* gtk_menu_popdown is called each time a menu item is selected from
       * a torn off menu. Only scroll back to the saved position if the
       * non-tearoff menu was popped down.
       */
      if (!menu->tearoff_active)
	gtk_menu_scroll_to (menu, menu->saved_scroll_offset);
      menu->tearoff_active = TRUE;
    }
  else
    gtk_widget_hide (GTK_WIDGET (menu));

  menu_shell->have_xgrab = FALSE;
  gtk_grab_remove (GTK_WIDGET (menu));

  menu_grab_transfer_window_destroy (menu);
}

GtkWidget*
gtk_menu_get_active (GtkMenu *menu)
{
  GtkWidget *child;
  GList *children;
  
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);
  
  if (!menu->old_active_menu_item)
    {
      child = NULL;
      children = GTK_MENU_SHELL (menu)->children;
      
      while (children)
	{
	  child = children->data;
	  children = children->next;
	  
	  if (GTK_BIN (child)->child)
	    break;
	  child = NULL;
	}
      
      menu->old_active_menu_item = child;
      if (menu->old_active_menu_item)
	gtk_widget_ref (menu->old_active_menu_item);
    }
  
  return menu->old_active_menu_item;
}

void
gtk_menu_set_active (GtkMenu *menu,
		     guint    index)
{
  GtkWidget *child;
  GList *tmp_list;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  
  tmp_list = g_list_nth (GTK_MENU_SHELL (menu)->children, index);
  if (tmp_list)
    {
      child = tmp_list->data;
      if (GTK_BIN (child)->child)
	{
	  if (menu->old_active_menu_item)
	    gtk_widget_unref (menu->old_active_menu_item);
	  menu->old_active_menu_item = child;
	  gtk_widget_ref (menu->old_active_menu_item);
	}
    }
}

void
gtk_menu_set_accel_group (GtkMenu	*menu,
			  GtkAccelGroup *accel_group)
{
  g_return_if_fail (GTK_IS_MENU (menu));
  
  if (menu->accel_group != accel_group)
    {
      if (menu->accel_group)
	gtk_accel_group_unref (menu->accel_group);
      menu->accel_group = accel_group;
      if (menu->accel_group)
	gtk_accel_group_ref (menu->accel_group);
      _gtk_menu_refresh_accel_paths (menu, TRUE);
    }
}

GtkAccelGroup*
gtk_menu_get_accel_group (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  return menu->accel_group;
}

/**
 * gtk_menu_set_accel_path
 * @menu:       a valid #GtkMenu
 * @accel_path: a valid accelerator path
 *
 * Sets an accelerator path for this menu from which accelerator paths
 * for its immediate children, its menu items, can be constructed.
 * The main purpose of this function is to spare the programmer the
 * inconvenience of having to call gtk_menu_item_set_accel_path() on
 * each menu item that should support runtime user changable accelerators.
 * Instead, by just calling gtk_menu_set_accel_path() on their parent,
 * each menu item of this menu, that contains a label describing its purpose,
 * automatically gets an accel path assigned. For example, a menu containing
 * menu items "New" and "Exit", will, after 
 * <literal>gtk_menu_set_accel_path (menu, "&lt;Gnumeric-Sheet&gt;/File");</literal>
 * has been called, assign its items the accel paths:
 * <literal>"&lt;Gnumeric-Sheet&gt;/File/New"</literal> and <literal>"&lt;Gnumeric-Sheet&gt;/File/Exit"</literal>.
 * Assigning accel paths to menu items then enables the user to change
 * their accelerators at runtime. More details about accelerator paths
 * and their default setups can be found at gtk_accel_map_add_entry().
 */
void
gtk_menu_set_accel_path (GtkMenu     *menu,
			 const gchar *accel_path)
{
  g_return_if_fail (GTK_IS_MENU (menu));
  if (accel_path)
    g_return_if_fail (accel_path[0] == '<' && strchr (accel_path, '/')); /* simplistic check */

  g_free (menu->accel_path);
  menu->accel_path = g_strdup (accel_path);
  if (menu->accel_path)
    _gtk_menu_refresh_accel_paths (menu, FALSE);
}

typedef struct {
  GtkMenu *menu;
  gboolean group_changed;
} AccelPropagation;

static void
refresh_accel_paths_foreach (GtkWidget *widget,
			     gpointer   data)
{
  AccelPropagation *prop = data;

  if (GTK_IS_MENU_ITEM (widget))	/* should always be true */
    _gtk_menu_item_refresh_accel_path (GTK_MENU_ITEM (widget),
				       prop->menu->accel_path,
				       prop->menu->accel_group,
				       prop->group_changed);
}

static void
_gtk_menu_refresh_accel_paths (GtkMenu *menu,
			       gboolean group_changed)
{
  g_return_if_fail (GTK_IS_MENU (menu));
      
  if (menu->accel_path && menu->accel_group)
    {
      AccelPropagation prop;

      prop.menu = menu;
      prop.group_changed = group_changed;
      gtk_container_foreach (GTK_CONTAINER (menu),
			     refresh_accel_paths_foreach,
			     &prop);
    }
}

void
gtk_menu_reposition (GtkMenu *menu)
{
  g_return_if_fail (GTK_IS_MENU (menu));

  if (GTK_WIDGET_DRAWABLE (menu) && !menu->torn_off)
    gtk_menu_position (menu);
}

static void
gtk_menu_scrollbar_changed (GtkAdjustment *adjustment,
			    GtkMenu       *menu)
{
  g_return_if_fail (GTK_IS_MENU (menu));

  if (adjustment->value != menu->scroll_offset)
    gtk_menu_scroll_to (menu, adjustment->value);
}

static void
gtk_menu_set_tearoff_hints (GtkMenu *menu,
			    gint     width)
{
  GdkGeometry geometry_hints;
  
  if (!menu->tearoff_window)
    return;

  if (GTK_WIDGET_VISIBLE (menu->tearoff_scrollbar))
    {
      gtk_widget_size_request (menu->tearoff_scrollbar, NULL);
      width += menu->tearoff_scrollbar->requisition.width;
    }

  geometry_hints.min_width = width;
  geometry_hints.max_width = width;
    
  geometry_hints.min_height = 0;
  geometry_hints.max_height = GTK_WIDGET (menu)->requisition.height;
  gtk_window_set_geometry_hints (GTK_WINDOW (menu->tearoff_window),
				 NULL,
				 &geometry_hints,
				 GDK_HINT_MAX_SIZE|GDK_HINT_MIN_SIZE);
}

static void
gtk_menu_update_title (GtkMenu *menu)
{
  if (menu->tearoff_window)
    {
      const gchar *title;
      GtkWidget *attach_widget;

      title = gtk_menu_get_title (menu);
      if (!title)
	{
	  attach_widget = gtk_menu_get_attach_widget (menu);
	  if (GTK_IS_MENU_ITEM (attach_widget))
	    {
	      GtkWidget *child = GTK_BIN (attach_widget)->child;
	      if (GTK_IS_LABEL (child))
		title = gtk_label_get_text (GTK_LABEL (child));
	    }
	}
      
      if (title)
	gtk_window_set_title (GTK_WINDOW (menu->tearoff_window), title);
    }
}

void       
gtk_menu_set_tearoff_state (GtkMenu  *menu,
			    gboolean  torn_off)
{
  gint width, height;
  
  g_return_if_fail (GTK_IS_MENU (menu));

  if (menu->torn_off != torn_off)
    {
      menu->torn_off = torn_off;
      menu->tearoff_active = torn_off;
      
      if (menu->torn_off)
	{
	  if (GTK_WIDGET_VISIBLE (menu))
	    gtk_menu_popdown (menu);

	  if (!menu->tearoff_window)
	    {
	      menu->tearoff_window = g_object_connect (gtk_widget_new (GTK_TYPE_WINDOW,
								       "type", GTK_WINDOW_TOPLEVEL,
								       NULL),
						       "signal::destroy", gtk_widget_destroyed, &menu->tearoff_window,
						       NULL);
	      gtk_window_set_type_hint (GTK_WINDOW (menu->tearoff_window),
					GDK_WINDOW_TYPE_HINT_MENU);
	      gtk_window_set_mnemonic_modifier (GTK_WINDOW (menu->tearoff_window), 0);
	      gtk_widget_set_app_paintable (menu->tearoff_window, TRUE);
	      gtk_signal_connect (GTK_OBJECT (menu->tearoff_window),  
				  "event",
				  GTK_SIGNAL_FUNC (gtk_menu_window_event), 
				  GTK_OBJECT (menu));

	      gtk_menu_update_title (menu);

	      gtk_widget_realize (menu->tearoff_window);
	      
	      gdk_window_set_decorations (menu->tearoff_window->window, 
					  GDK_DECOR_ALL |
					  GDK_DECOR_RESIZEH |
					  GDK_DECOR_MINIMIZE |
					  GDK_DECOR_MAXIMIZE);
	      gtk_window_set_resizable (GTK_WINDOW (menu->tearoff_window), FALSE);

	      menu->tearoff_hbox = gtk_hbox_new (FALSE, FALSE);
	      gtk_container_add (GTK_CONTAINER (menu->tearoff_window), menu->tearoff_hbox);

	      gdk_window_get_size (GTK_WIDGET (menu)->window, &width, &height);
	      menu->tearoff_adjustment =
		GTK_ADJUSTMENT (gtk_adjustment_new (0,
						    0,
						    GTK_WIDGET (menu)->requisition.height,
						    MENU_SCROLL_STEP,
						    height/2,
						    height));
	      g_object_connect (GTK_OBJECT (menu->tearoff_adjustment),
				"signal::value_changed", gtk_menu_scrollbar_changed, menu,
				NULL);
	      menu->tearoff_scrollbar = gtk_vscrollbar_new (menu->tearoff_adjustment);

	      gtk_box_pack_end (GTK_BOX (menu->tearoff_hbox),
				menu->tearoff_scrollbar,
				FALSE, FALSE, 0);
	      
	      if (menu->tearoff_adjustment->upper > height)
		gtk_widget_show (menu->tearoff_scrollbar);
	      
	      gtk_widget_show (menu->tearoff_hbox);
	    }
	  
	  gtk_menu_reparent (menu, menu->tearoff_hbox, FALSE);

	  gdk_window_get_size (GTK_WIDGET (menu)->window, &width, NULL);

	  /* Update menu->requisition
	   */
	  gtk_widget_size_request (GTK_WIDGET (menu), NULL);
  
	  gtk_menu_set_tearoff_hints (menu, width);
	    
	  gtk_widget_realize (menu->tearoff_window);
	  gtk_menu_position (menu);
	  
	  gtk_widget_show (GTK_WIDGET (menu));
	  gtk_widget_show (menu->tearoff_window);

	  gtk_menu_scroll_to (menu, 0);

	}
      else
	{
	  gtk_widget_hide (menu->tearoff_window);
	  gtk_menu_reparent (menu, menu->toplevel, FALSE);
	}
    }
}

/**
 * gtk_menu_get_tearoff_state:
 * @menu: a #GtkMenu
 *
 * Returns whether the menu is torn off. See
 * gtk_menu_set_tearoff_state ().
 *
 * Return value: %TRUE if the menu is currently torn off.
 **/
gboolean
gtk_menu_get_tearoff_state (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), FALSE);

  return menu->torn_off;
}

/**
 * gtk_menu_set_title:
 * @menu: a #GtkMenu
 * @title: a string containing the title for the menu.
 * 
 * Sets the title string for the menu.  The title is displayed when the menu
 * is shown as a tearoff menu.
 **/
void       
gtk_menu_set_title (GtkMenu     *menu,
		    const gchar *title)
{
  g_return_if_fail (GTK_IS_MENU (menu));

  if (title)
    g_object_set_data_full (G_OBJECT (menu), "gtk-menu-title",
			    g_strdup (title), (GtkDestroyNotify) g_free);
  else
    g_object_set_data (G_OBJECT (menu), "gtk-menu-title", NULL);
    
  gtk_menu_update_title (menu);
  g_object_notify (G_OBJECT (menu), "tearoff_title");
}

/**
 * gtk_menu_get_title:
 * @menu: a #GtkMenu
 *
 * Returns the title of the menu. See gtk_menu_set_title().
 *
 * Return value: the title of the menu, or %NULL if the menu has no
 *               title set on it.
 **/
G_CONST_RETURN gchar *
gtk_menu_get_title (GtkMenu *menu)
{
  g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

  return gtk_object_get_data (GTK_OBJECT (menu), "gtk-menu-title");
}

void
gtk_menu_reorder_child (GtkMenu   *menu,
                        GtkWidget *child,
                        gint       position)
{
  GtkMenuShell *menu_shell;
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));
  menu_shell = GTK_MENU_SHELL (menu);
  if (g_list_find (menu_shell->children, child))
    {   
      menu_shell->children = g_list_remove (menu_shell->children, child);
      menu_shell->children = g_list_insert (menu_shell->children, child, position);   
      if (GTK_WIDGET_VISIBLE (menu_shell))
        gtk_widget_queue_resize (GTK_WIDGET (menu_shell));
    }   
}

static void
gtk_menu_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;
  GtkMenu *menu;
  GtkWidget *child;
  GList *children;

  g_return_if_fail (GTK_IS_MENU (widget));

  menu = GTK_MENU (widget);
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK | GDK_KEY_PRESS_MASK |
			    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK );
  
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);
  
  border_width = GTK_CONTAINER (widget)->border_width;
  
  attributes.x = border_width + widget->style->xthickness;
  attributes.y = border_width + widget->style->ythickness;
  attributes.width = MAX (1, widget->allocation.width - attributes.x * 2);
  attributes.height = MAX (1, widget->allocation.height - attributes.y * 2);

  if (menu->upper_arrow_visible)
    {
      attributes.y += MENU_SCROLL_ARROW_HEIGHT;
      attributes.height -= MENU_SCROLL_ARROW_HEIGHT;
    }
  if (menu->lower_arrow_visible)
    attributes.height -= MENU_SCROLL_ARROW_HEIGHT;

  menu->view_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (menu->view_window, menu);

  attributes.x = 0;
  attributes.y = 0;
  attributes.height = MAX (1, widget->requisition.height - (border_width + widget->style->ythickness) * 2);
  
  menu->bin_window = gdk_window_new (menu->view_window, &attributes, attributes_mask);
  gdk_window_set_user_data (menu->bin_window, menu);

  children = GTK_MENU_SHELL (menu)->children;
  while (children)
    {
      child = children->data;
      children = children->next;
	  
      gtk_widget_set_parent_window (child, menu->bin_window);
    }
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, menu->bin_window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, menu->view_window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gtk_menu_scroll_item_visible (GTK_MENU_SHELL (widget),
				GTK_MENU_SHELL (widget)->active_menu_item);

  gdk_window_show (menu->bin_window);
  gdk_window_show (menu->view_window);
}

/* See notes in gtk_menu_popup() for information about the "grab transfer window"
 */
static GdkWindow *
menu_grab_transfer_window_get (GtkMenu *menu)
{
  GdkWindow *window = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-window");
  if (!window)
    {
      GdkWindowAttr attributes;
      gint attributes_mask;
      
      attributes.x = -100;
      attributes.y = -100;
      attributes.width = 10;
      attributes.height = 10;
      attributes.window_type = GDK_WINDOW_TEMP;
      attributes.wclass = GDK_INPUT_ONLY;
      attributes.override_redirect = TRUE;
      attributes.event_mask = 0;

      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;
      
      window = gdk_window_new (NULL, &attributes, attributes_mask);
      gdk_window_set_user_data (window, menu);

      gdk_window_show (window);

      g_object_set_data (G_OBJECT (menu), "gtk-menu-transfer-window", window);
    }

  return window;
}

static void
menu_grab_transfer_window_destroy (GtkMenu *menu)
{
  GdkWindow *window = g_object_get_data (G_OBJECT (menu), "gtk-menu-transfer-window");
  if (window)
    {
      gdk_window_set_user_data (window, NULL);
      gdk_window_destroy (window);
      g_object_set_data (G_OBJECT (menu), "gtk-menu-transfer-window", NULL);
    }
}

static void
gtk_menu_unrealize (GtkWidget *widget)
{
  GtkMenu *menu;

  g_return_if_fail (GTK_IS_MENU (widget));

  menu = GTK_MENU (widget);

  menu_grab_transfer_window_destroy (menu);

  gdk_window_set_user_data (menu->view_window, NULL);
  gdk_window_destroy (menu->view_window);
  menu->view_window = NULL;

  gdk_window_set_user_data (menu->bin_window, NULL);
  gdk_window_destroy (menu->bin_window);
  menu->bin_window = NULL;

  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_menu_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  GtkMenu *menu;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  guint max_toggle_size;
  guint max_accel_width;
  GtkRequisition child_requisition;
  
  g_return_if_fail (GTK_IS_MENU (widget));
  g_return_if_fail (requisition != NULL);
  
  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);
  
  requisition->width = 0;
  requisition->height = 0;
  
  max_toggle_size = 0;
  max_accel_width = 0;
  
  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      
      if (GTK_WIDGET_VISIBLE (child))
	{
	  gint toggle_size;

          /* It's important to size_request the child
           * before doing the toggle size request, in
           * case the toggle size request depends on the size
           * request of a child of the child (e.g. for ImageMenuItem)
           */
          
	  GTK_MENU_ITEM (child)->show_submenu_indicator = TRUE;
	  gtk_widget_size_request (child, &child_requisition);
	  
	  requisition->width = MAX (requisition->width, child_requisition.width);
	  requisition->height += child_requisition.height;

	  gtk_menu_item_toggle_size_request (GTK_MENU_ITEM (child), &toggle_size);
	  max_toggle_size = MAX (max_toggle_size, toggle_size);
	  max_accel_width = MAX (max_accel_width, GTK_MENU_ITEM (child)->accelerator_width);
	}
    }
  
  requisition->width += max_toggle_size + max_accel_width;
  requisition->width += (GTK_CONTAINER (menu)->border_width +
			 widget->style->xthickness) * 2;
  requisition->height += (GTK_CONTAINER (menu)->border_width +
			  widget->style->ythickness) * 2;
  
  menu->toggle_size = max_toggle_size;

  /* If the requested width was different than the allocated width, we need to change
   * the geometry hints for the tear off window so that the window can actually be resized.
   * Don't resize the tearoff if it is not active, because it won't redraw (it is only a background pixmap).
   */
  if ((requisition->width != GTK_WIDGET (menu)->allocation.width) && menu->tearoff_active)
    gtk_menu_set_tearoff_hints (menu, requisition->width);
}

static void
gtk_menu_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkMenu *menu;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GtkAllocation child_allocation;
  GList *children;
  gint x, y;
  gint width, height;

  g_return_if_fail (GTK_IS_MENU (widget));
  g_return_if_fail (allocation != NULL);
  
  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget);

  widget->allocation = *allocation;

  x = GTK_CONTAINER (menu)->border_width + widget->style->xthickness;
  y = GTK_CONTAINER (menu)->border_width + widget->style->ythickness;
  
  width = MAX (1, allocation->width - x * 2);
  height = MAX (1, allocation->height - y * 2);
  
  if (menu->upper_arrow_visible && !menu->tearoff_active)
    {
      y += MENU_SCROLL_ARROW_HEIGHT;
      height -= MENU_SCROLL_ARROW_HEIGHT;
    }
  
  if (menu->lower_arrow_visible && !menu->tearoff_active)
    height -= MENU_SCROLL_ARROW_HEIGHT;
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gdk_window_move_resize (menu->view_window,
			      x,
			      y,
			      width,
			      height);
    }

  if (menu_shell->children)
    {
      child_allocation.x = 0;
      child_allocation.y = 0;
      child_allocation.width = width;
      
      children = menu_shell->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;
	  
	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      GtkRequisition child_requisition;
	      gtk_widget_get_child_requisition (child, &child_requisition);
	      
	      child_allocation.height = child_requisition.height;

	      gtk_menu_item_toggle_size_allocate (GTK_MENU_ITEM (child),
						  menu->toggle_size);
	      gtk_widget_size_allocate (child, &child_allocation);
	      gtk_widget_queue_draw (child);
	      
	      child_allocation.y += child_allocation.height;
	    }
	}
      
      /* Resize the item window */
      if (GTK_WIDGET_REALIZED (widget))
	{
	  gdk_window_resize (menu->bin_window,
			     child_allocation.width,
			     child_allocation.y);
	}


      if (menu->tearoff_active)
	{
	  if (allocation->height >= widget->requisition.height)
	    {
	      if (GTK_WIDGET_VISIBLE (menu->tearoff_scrollbar))
		{
		  gtk_widget_hide (menu->tearoff_scrollbar);
		  gtk_menu_set_tearoff_hints (menu, allocation->width);
		  gtk_widget_set_usize (menu->tearoff_window, -1, allocation->height);

		  gtk_menu_scroll_to (menu, 0);
		}
	    }
	  else
	    {
	      menu->tearoff_adjustment->upper = widget->requisition.height;
	      menu->tearoff_adjustment->page_size = allocation->height;
	      
	      if (menu->tearoff_adjustment->value + menu->tearoff_adjustment->page_size >
		  menu->tearoff_adjustment->upper)
		{
		  gint value;
		  value = menu->tearoff_adjustment->upper - menu->tearoff_adjustment->page_size;
		  if (value < 0)
		    value = 0;
		  gtk_menu_scroll_to (menu, value);
		}
	      
	      gtk_adjustment_changed (menu->tearoff_adjustment);
	      
	      if (!GTK_WIDGET_VISIBLE (menu->tearoff_scrollbar))
		{
		  gtk_widget_show (menu->tearoff_scrollbar);
		  gtk_menu_set_tearoff_hints (menu, allocation->width);
		  gtk_widget_set_usize (menu->tearoff_window, -1, allocation->height);
		}
	    }
	}
    }
}

static void
gtk_menu_paint (GtkWidget      *widget,
		GdkEventExpose *event)
{
  GtkMenu *menu;
  gint width, height;
  gint border_x, border_y;
  
  g_return_if_fail (GTK_IS_MENU (widget));

  menu = GTK_MENU (widget);
  
  border_x = GTK_CONTAINER (widget)->border_width + widget->style->xthickness;
  border_y = GTK_CONTAINER (widget)->border_width + widget->style->ythickness;
  gdk_window_get_size (widget->window, &width, &height);

  if (event->window == widget->window)
    {
      gtk_paint_box (widget->style,
		     widget->window,
		     GTK_STATE_NORMAL,
		     GTK_SHADOW_OUT,
		     NULL, widget, "menu",
		     0, 0, -1, -1);
      if (menu->upper_arrow_visible && !menu->tearoff_active)
	{
	  gtk_paint_box (widget->style,
			 widget->window,
			 menu->upper_arrow_prelight ?
			 GTK_STATE_PRELIGHT : GTK_STATE_NORMAL,
			 GTK_SHADOW_OUT,
			 NULL, widget, "menu",
			 border_x,
			 border_y,
			 width - 2*border_x,
			 MENU_SCROLL_ARROW_HEIGHT);
	  
	  gtk_paint_arrow (widget->style,
			   widget->window,
			   menu->upper_arrow_prelight ?
			   GTK_STATE_PRELIGHT : GTK_STATE_NORMAL,
			   GTK_SHADOW_OUT,
			   NULL, widget, "menu",
			   GTK_ARROW_UP,
			   TRUE,
			   width / 2 - MENU_SCROLL_ARROW_HEIGHT / 2 + 1,
			   2 * border_y + 1,
			   MENU_SCROLL_ARROW_HEIGHT - 2 * border_y - 2,
			   MENU_SCROLL_ARROW_HEIGHT - 2 * border_y - 2);
	}
  
      if (menu->lower_arrow_visible && !menu->tearoff_active)
	{
	  gtk_paint_box (widget->style,
			 widget->window,
			 menu->lower_arrow_prelight ?
			 GTK_STATE_PRELIGHT : GTK_STATE_NORMAL,
			 GTK_SHADOW_OUT,
			 NULL, widget, "menu",
			 border_x,
			 height - border_y - MENU_SCROLL_ARROW_HEIGHT + 1,
			 width - 2*border_x,
			 MENU_SCROLL_ARROW_HEIGHT);
	  
	  gtk_paint_arrow (widget->style,
			   widget->window,
			   menu->lower_arrow_prelight ?
			   GTK_STATE_PRELIGHT : GTK_STATE_NORMAL,
			   GTK_SHADOW_OUT,
			   NULL, widget, "menu",
			   GTK_ARROW_DOWN,
			   TRUE,
			   width / 2 - MENU_SCROLL_ARROW_HEIGHT / 2 + 1,
			   height - MENU_SCROLL_ARROW_HEIGHT + 1,
			   MENU_SCROLL_ARROW_HEIGHT - 2 * border_y - 2,
			   MENU_SCROLL_ARROW_HEIGHT - 2 * border_y - 2);
	}
    }
  else if (event->window == menu->view_window)
    {
      gint menu_height;
      gint top_pos;
      
      if (menu->scroll_offset < 0)
	gtk_paint_box (widget->style,
		       menu->view_window,
		       GTK_STATE_ACTIVE,
		       GTK_SHADOW_IN,
		       NULL, widget, "menu",
		       0, 0,
		       -1,
		       -menu->scroll_offset);

      menu_height = widget->requisition.height - 2*border_y;
      top_pos = height - 2*border_y - (menu->upper_arrow_visible ? MENU_SCROLL_ARROW_HEIGHT : 0);

      if (menu_height - menu->scroll_offset < top_pos)
	gtk_paint_box (widget->style,
		       menu->view_window,
		       GTK_STATE_ACTIVE,
		       GTK_SHADOW_IN,
		       NULL, widget, "menu",
		       0,
		       menu_height - menu->scroll_offset,
		       -1,
		       top_pos - (menu_height - menu->scroll_offset));
    }
}

static gboolean
gtk_menu_expose (GtkWidget	*widget,
		 GdkEventExpose *event)
{
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_paint (widget, event);
      
      (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
    }
  
  return FALSE;
}

static void
gtk_menu_show (GtkWidget *widget)
{
  GtkMenu *menu = GTK_MENU (widget);

  _gtk_menu_refresh_accel_paths (menu, FALSE);

  GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static gboolean
gtk_menu_key_press (GtkWidget	*widget,
		    GdkEventKey *event)
{
  GtkMenuShell *menu_shell;
  GtkMenu *menu;
  gboolean delete = FALSE;
  gboolean can_change_accels;
  gchar *accel = NULL;
  guint accel_key, accel_mods;
  
  g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
      
  menu_shell = GTK_MENU_SHELL (widget);
  menu = GTK_MENU (widget);
  
  gtk_menu_stop_navigating_submenu (menu);

  if (GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event))
    return TRUE;
    
  g_object_get (G_OBJECT (gtk_settings_get_default ()),
                "gtk-menu-bar-accel",
                &accel,
                NULL);

  if (accel)
    {
      guint keyval = 0;
      GdkModifierType mods = 0;
      gboolean handled = FALSE;
      
      gtk_accelerator_parse (accel, &keyval, &mods);

      if (keyval == 0)
        g_warning ("Failed to parse menu bar accelerator '%s'\n", accel);

      /* FIXME this is wrong, needs to be in the global accel resolution
       * thing, to properly consider i18n etc., but that probably requires
       * AccelGroup changes etc.
       */
      if (event->keyval == keyval &&
          (mods & event->state) == mods)
        {
          gtk_signal_emit_by_name (GTK_OBJECT (menu), "cancel");
        }

      g_free (accel);

      if (handled)
        return TRUE;
    }
  
  switch (event->keyval)
    {
    case GDK_Delete:
    case GDK_KP_Delete:
    case GDK_BackSpace:
      delete = TRUE;
      break;
    default:
      break;
    }

  g_object_get (G_OBJECT (gtk_settings_get_default ()),
		"gtk-can-change-accels",
		&can_change_accels,
		NULL);
  
  accel_key = event->keyval;
  accel_mods = event->state & gtk_accelerator_get_default_mod_mask ();

  /* Modify the accelerators */
  if (can_change_accels &&
      menu_shell->active_menu_item &&
      GTK_BIN (menu_shell->active_menu_item)->child &&			/* no seperators */
      GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu == NULL &&	/* no submenus */
      (delete || gtk_accelerator_valid (event->keyval, event->state)))
    {
      GtkWidget *menu_item = menu_shell->active_menu_item;
      gboolean replace_accels = TRUE;
      const gchar *path;

      path = GTK_MENU_ITEM (menu_item)->accel_path;
      if (!path)
	path = _gtk_widget_get_accel_path (menu_item);
      if (!path)
	{
	  /* can't change accelerators on menu_items without paths
	   * (basically, those items are accelerator-locked).
	   */
	  /* g_print("item has no path, menu prefix: %s\n", menu->accel_path); */
	  gdk_beep ();
	}
      else
	{
	  gboolean changed;

	  /* For the keys that act to delete the current setting, we delete
	   * the current setting if there is one, otherwise, we set the
	   * key as the accelerator.
	   */
	  if (delete)
	    {
	      GtkAccelKey key;
	      
	      if (gtk_accel_map_lookup_entry (path, &key) &&
		  (key.accel_key || key.accel_mods))
		{
		  accel_key = 0;
		  accel_mods = 0;
		}
	    }
	  changed = gtk_accel_map_change_entry (path, accel_key, accel_mods, replace_accels);

	  if (!changed)
	    {
	      /* we failed, probably because this key is in use and
	       * locked already
	       */
	      /* g_print("failed to change\n"); */
	      gdk_beep ();
	    }
	}
    }
  
  return TRUE;
}

static gboolean
gtk_menu_motion_notify  (GtkWidget	   *widget,
			 GdkEventMotion    *event)
{
  GtkWidget *menu_item;
  GtkMenu *menu;
  GtkMenuShell *menu_shell;

  gboolean need_enter;


  if (GTK_IS_MENU (widget))
    gtk_menu_handle_scrolling (GTK_MENU (widget), TRUE);
  
  /* We received the event for one of two reasons:
   *
   * a) We are the active menu, and did gtk_grab_add()
   * b) The widget is a child of ours, and the event was propagated
   *
   * Since for computation of navigation regions, we want the menu which
   * is the parent of the menu item, for a), we need to find that menu,
   * which may be different from 'widget'.
   */
  menu_item = gtk_get_event_widget ((GdkEvent*) event);
  if (!menu_item || !GTK_IS_MENU_ITEM (menu_item) ||
      !_gtk_menu_item_is_selectable (menu_item) ||
      !GTK_IS_MENU (menu_item->parent))
    return FALSE;

  menu_shell = GTK_MENU_SHELL (menu_item->parent);
  menu = GTK_MENU (menu_shell);
  
  need_enter = (menu->navigation_region != NULL || menu_shell->ignore_enter);

  /* Check to see if we are within an active submenu's navigation region
   */
  if (gtk_menu_navigating_submenu (menu, event->x_root, event->y_root))
    return TRUE; 

  if (need_enter)
    {
      /* The menu is now sensitive to enter events on its items, but
       * was previously sensitive.  So we fake an enter event.
       */
      gint width, height;
      
      menu_shell->ignore_enter = FALSE; 
      
      gdk_window_get_size (event->window, &width, &height);
      if (event->x >= 0 && event->x < width &&
	  event->y >= 0 && event->y < height)
	{
	  GdkEvent send_event;

	  memset (&send_event, 0, sizeof (send_event));
	  send_event.crossing.type = GDK_ENTER_NOTIFY;
	  send_event.crossing.window = event->window;
	  send_event.crossing.time = event->time;
	  send_event.crossing.send_event = TRUE;
	  send_event.crossing.x_root = event->x_root;
	  send_event.crossing.y_root = event->y_root;
	  send_event.crossing.x = event->x;
	  send_event.crossing.y = event->y;

	  /* We send the event to 'widget', the currently active menu,
	   * instead of 'menu', the menu that the pointer is in. This
	   * will ensure that the event will be ignored unless the
	   * menuitem is a child of the active menu or some parent
	   * menu of the active menu.
	   */
	  return gtk_widget_event (widget, &send_event);
	}
    }

  return FALSE;
}

static gboolean
gtk_menu_scroll_timeout (gpointer  data)
{
  GtkMenu *menu;
  GtkWidget *widget;
  gint offset;
  gint view_width, view_height;

  GDK_THREADS_ENTER ();

  menu = GTK_MENU (data);
  widget = GTK_WIDGET (menu);

  offset = menu->scroll_offset + menu->scroll_step;

  /* If we scroll upward and the non-visible top part
   * is smaller than the scroll arrow it would be
   * pretty stupid to show the arrow and taking more
   * screen space than just scrolling to the top.
   */
  if ((menu->scroll_step < 0) && (offset < MENU_SCROLL_ARROW_HEIGHT))
    offset = 0;

  /* Don't scroll over the top if we weren't before: */
  if ((menu->scroll_offset >= 0) && (offset < 0))
    offset = 0;

  gdk_window_get_size (widget->window, &view_width, &view_height);

  /* Don't scroll past the bottom if we weren't before: */
  if (menu->scroll_offset > 0)
    view_height -= MENU_SCROLL_ARROW_HEIGHT;
  
  if ((menu->scroll_offset + view_height <= widget->requisition.height) &&
      (offset + view_height > widget->requisition.height))
    offset = widget->requisition.height - view_height;

  gtk_menu_scroll_to (menu, offset);

  GDK_THREADS_LEAVE ();

  return TRUE;
}

static void
gtk_menu_handle_scrolling (GtkMenu *menu, gboolean enter)
{
  GtkMenuShell *menu_shell;
  gint width, height;
  gint x, y;
  gint border;
  GdkRectangle rect;
  gboolean in_arrow;
  gboolean scroll_fast = FALSE;

  menu_shell = GTK_MENU_SHELL (menu);

  gdk_window_get_pointer (GTK_WIDGET (menu)->window, &x, &y, NULL);
  gdk_window_get_size (GTK_WIDGET (menu)->window, &width, &height);

  border = GTK_CONTAINER (menu)->border_width + GTK_WIDGET (menu)->style->ythickness;

  if (menu->upper_arrow_visible && !menu->tearoff_active)
    {
      rect.x = 0;
      rect.y = 0;
      rect.width = width;
      rect.height = MENU_SCROLL_ARROW_HEIGHT + border;
      
      in_arrow = FALSE;
      if ((x >= rect.x) && (x < rect.x + rect.width) &&
	  (y >= rect.y) && (y < rect.y + rect.height))
	{
	  in_arrow = TRUE;
	  scroll_fast = (y < rect.y + MENU_SCROLL_FAST_ZONE);
	}
	
      if (enter && in_arrow &&
	  (!menu->upper_arrow_prelight || menu->scroll_fast != scroll_fast))
	{
	  menu->upper_arrow_prelight = TRUE;
	  menu->scroll_fast = scroll_fast;
	  gdk_window_invalidate_rect (GTK_WIDGET (menu)->window, &rect, FALSE);
	  
	  /* Deselect the active item so that any submenus are poped down */
	  gtk_menu_shell_deselect (menu_shell);

	  gtk_menu_stop_scrolling (menu);
	  menu->scroll_step = -MENU_SCROLL_STEP;
	  menu->timeout_id = g_timeout_add ((scroll_fast) ? MENU_SCROLL_TIMEOUT2 : MENU_SCROLL_TIMEOUT1,
					    gtk_menu_scroll_timeout,
					    menu);
	}
      else if (!enter && !in_arrow && menu->upper_arrow_prelight)
	{
	  menu->upper_arrow_prelight = FALSE;
	  gdk_window_invalidate_rect (GTK_WIDGET (menu)->window, &rect, FALSE);
	  
	  gtk_menu_stop_scrolling (menu);
	}
    }
  
  if (menu->lower_arrow_visible && !menu->tearoff_active)
    {
      rect.x = 0;
      rect.y = height - border - MENU_SCROLL_ARROW_HEIGHT;
      rect.width = width;
      rect.height = MENU_SCROLL_ARROW_HEIGHT + border;

      in_arrow = FALSE;
      if ((x >= rect.x) && (x < rect.x + rect.width) &&
	  (y >= rect.y) && (y < rect.y + rect.height))
	{
	  in_arrow = TRUE;
	  scroll_fast = (y > rect.y + rect.height - MENU_SCROLL_FAST_ZONE);
	}

      if (enter && in_arrow &&
	  (!menu->lower_arrow_prelight || menu->scroll_fast != scroll_fast))
	{
	  menu->lower_arrow_prelight = TRUE;
	  menu->scroll_fast = scroll_fast;
	  gdk_window_invalidate_rect (GTK_WIDGET (menu)->window, &rect, FALSE);

	  /* Deselect the active item so that any submenus are poped down */
	  gtk_menu_shell_deselect (menu_shell);

	  gtk_menu_stop_scrolling (menu);
	  menu->scroll_step = MENU_SCROLL_STEP;
	  menu->timeout_id = g_timeout_add ((scroll_fast) ? MENU_SCROLL_TIMEOUT2 : MENU_SCROLL_TIMEOUT1,
					    gtk_menu_scroll_timeout,
					    menu);
	}
      else if (!enter && !in_arrow && menu->lower_arrow_prelight)
	{
	  menu->lower_arrow_prelight = FALSE;
	  gdk_window_invalidate_rect (GTK_WIDGET (menu)->window, &rect, FALSE);
	  
	  gtk_menu_stop_scrolling (menu);
	}
    }
}

static gboolean
gtk_menu_enter_notify (GtkWidget        *widget,
		       GdkEventCrossing *event)
{
  GtkWidget *menu_item;

  if (widget && GTK_IS_MENU (widget))
    gtk_menu_handle_scrolling (GTK_MENU (widget), TRUE);
      
  /* If this is a faked enter (see gtk_menu_motion_notify), 'widget'
   * will not correspond to the event widget's parent.  Check to see
   * if we are in the parent's navigation region.
   */
  menu_item = gtk_get_event_widget ((GdkEvent*) event);
  if (menu_item && GTK_IS_MENU_ITEM (menu_item) && GTK_IS_MENU (menu_item->parent) &&
      gtk_menu_navigating_submenu (GTK_MENU (menu_item->parent), event->x_root, event->y_root))
    return TRUE;

  return GTK_WIDGET_CLASS (parent_class)->enter_notify_event (widget, event); 
}

static gboolean
gtk_menu_leave_notify (GtkWidget        *widget,
		       GdkEventCrossing *event)
{
  GtkMenuShell *menu_shell;
  GtkMenu *menu;
  GtkMenuItem *menu_item;
  GtkWidget *event_widget;

  menu = GTK_MENU (widget);
  menu_shell = GTK_MENU_SHELL (widget); 
  
  if (gtk_menu_navigating_submenu (menu, event->x_root, event->y_root))
    return TRUE; 

  gtk_menu_handle_scrolling (menu, FALSE);
  
  event_widget = gtk_get_event_widget ((GdkEvent*) event);
  
  if (!event_widget || !GTK_IS_MENU_ITEM (event_widget))
    return TRUE;
  
  menu_item = GTK_MENU_ITEM (event_widget); 
  
  /* Here we check to see if we're leaving an active menu item with a submenu, 
   * in which case we enter submenu navigation mode. 
   */
  if (menu_shell->active_menu_item != NULL
      && menu_item->submenu != NULL
      && menu_item->submenu_placement == GTK_LEFT_RIGHT)
    {
      if (menu_item->submenu->window != NULL) 
	{
	  gtk_menu_set_submenu_navigation_region (menu, menu_item, event);
	  return TRUE;
	}
    }
  
  return GTK_WIDGET_CLASS (parent_class)->leave_notify_event (widget, event); 
}

static void 
gtk_menu_stop_navigating_submenu (GtkMenu *menu)
{
  if (menu->navigation_region) 
    {
      gdk_region_destroy (menu->navigation_region);
      menu->navigation_region = NULL;
    }
  
  if (menu->navigation_timeout)
    {
      gtk_timeout_remove (menu->navigation_timeout);
      menu->navigation_timeout = 0;
    }
}

/* When the timeout is elapsed, the navigation region is destroyed
 * and the menuitem under the pointer (if any) is selected.
 */
static gboolean
gtk_menu_stop_navigating_submenu_cb (gpointer user_data)
{
  GtkMenu *menu = user_data;
  GdkWindow *child_window;

  GDK_THREADS_ENTER ();

  gtk_menu_stop_navigating_submenu (menu);
  
  if (GTK_WIDGET_REALIZED (menu))
    {
      child_window = gdk_window_get_pointer (menu->bin_window, NULL, NULL, NULL);

      if (child_window)
	{
	  GdkEventCrossing send_event;

	  memset (&send_event, 0, sizeof (send_event));
	  send_event.window = child_window;
	  send_event.type = GDK_ENTER_NOTIFY;
	  send_event.time = GDK_CURRENT_TIME; /* Bogus */
	  send_event.send_event = TRUE;

	  GTK_WIDGET_CLASS (parent_class)->enter_notify_event (GTK_WIDGET (menu), &send_event); 
	}
    }

  GDK_THREADS_LEAVE ();

  return FALSE; 
}

static gboolean
gtk_menu_navigating_submenu (GtkMenu *menu,
			     gint     event_x,
			     gint     event_y)
{
  if (menu->navigation_region)
    {
      if (gdk_region_point_in (menu->navigation_region, event_x, event_y))
	return TRUE;
      else
	{
	  gtk_menu_stop_navigating_submenu (menu);
	  return FALSE;
	}
    }
  return FALSE;
}

static void
gtk_menu_set_submenu_navigation_region (GtkMenu          *menu,
					GtkMenuItem      *menu_item,
					GdkEventCrossing *event)
{
  gint submenu_left = 0;
  gint submenu_right = 0;
  gint submenu_top = 0;
  gint submenu_bottom = 0;
  gint width = 0;
  gint height = 0;
  GdkPoint point[3];
  GtkWidget *event_widget;

  g_return_if_fail (menu_item->submenu != NULL);
  g_return_if_fail (event != NULL);
  
  event_widget = gtk_get_event_widget ((GdkEvent*) event);
  
  gdk_window_get_origin (menu_item->submenu->window, &submenu_left, &submenu_top);
  gdk_window_get_size (menu_item->submenu->window, &width, &height);
  submenu_right = submenu_left + width;
  submenu_bottom = submenu_top + height;
  
  gdk_window_get_size (event_widget->window, &width, &height);
  
  if (event->x >= 0 && event->x < width)
    {
      /* Set navigation region */
      /* We fudge/give a little padding in case the user
       * ``misses the vertex'' of the triangle/is off by a pixel or two.
       */ 
      if (menu_item->submenu_direction == GTK_DIRECTION_RIGHT)
	point[0].x = event->x_root - SUBMENU_NAV_REGION_PADDING; 
      else                             
	point[0].x = event->x_root + SUBMENU_NAV_REGION_PADDING;  
      
      /* Exiting the top or bottom? */ 
      if (event->y < 0)
        { /* top */
	  point[0].y = event->y_root + SUBMENU_NAV_REGION_PADDING;
	  point[1].y = submenu_top;

	  if (point[0].y <= point[1].y)
	    return;
	}
      else
        { /* bottom */
	  point[0].y = event->y_root - SUBMENU_NAV_REGION_PADDING; 
	  point[1].y = submenu_bottom;

	  if (point[0].y >= point[1].y)
	    return;
	}
      
      /* Submenu is to the left or right? */ 
      if (menu_item->submenu_direction == GTK_DIRECTION_RIGHT)
	point[1].x = submenu_left;  /* right */
      else
	point[1].x = submenu_right; /* left */
      
      point[2].x = point[1].x;
      point[2].y = point[0].y;

      gtk_menu_stop_navigating_submenu (menu);
      
      menu->navigation_region = gdk_region_polygon (point, 3, GDK_WINDING_RULE);

      menu->navigation_timeout = gtk_timeout_add (SUBMENU_NAV_HYSTERESIS_TIMEOUT, 
						  gtk_menu_stop_navigating_submenu_cb, menu);
    }
}

static void
gtk_menu_deactivate (GtkMenuShell *menu_shell)
{
  GtkWidget *parent;
  
  g_return_if_fail (GTK_IS_MENU (menu_shell));
  
  parent = menu_shell->parent_menu_shell;
  
  menu_shell->activate_time = 0;
  gtk_menu_popdown (GTK_MENU (menu_shell));
  
  if (parent)
    gtk_menu_shell_deactivate (GTK_MENU_SHELL (parent));
}

static void
gtk_menu_position (GtkMenu *menu)
{
  GtkWidget *widget;
  GtkRequisition requisition;
  gint x, y;
  gint screen_width;
  gint screen_height;
  gint scroll_offset;
  gint menu_height;
  gboolean push_in;
  
  g_return_if_fail (GTK_IS_MENU (menu));

  widget = GTK_WIDGET (menu);

  screen_width = gdk_screen_width ();
  screen_height = gdk_screen_height ();

  gdk_window_get_pointer (NULL, &x, &y, NULL);

  /* We need the requisition to figure out the right place to
   * popup the menu. In fact, we always need to ask here, since
   * if a size_request was queued while we weren't popped up,
   * the requisition won't have been recomputed yet.
   */
  gtk_widget_size_request (widget, &requisition);

  push_in = FALSE;
  
  if (menu->position_func)
    (* menu->position_func) (menu, &x, &y, &push_in, menu->position_func_data);
  else
    {
      x = CLAMP (x - 2, 0, MAX (0, screen_width - requisition.width));
      y = CLAMP (y - 2, 0, MAX (0, screen_height - requisition.height));
    }

  scroll_offset = 0;

  if (push_in)
    {
      menu_height = GTK_WIDGET (menu)->requisition.height;

      if (y + menu_height > screen_height)
	{
	  scroll_offset -= y + menu_height - screen_height;
	  y = screen_height - menu_height;
	}
  
      if (y < 0)
	{
	  scroll_offset -= y;
	  y = 0;
	}
    }

  if (y + requisition.height > screen_height)
    requisition.height = screen_height - y;
  
  if (y < 0)
    {
      scroll_offset -= y;
      requisition.height -= -y;
      y = 0;
    }

  if (scroll_offset > 0)
    scroll_offset += MENU_SCROLL_ARROW_HEIGHT;
  
  gtk_window_move (GTK_WINDOW (GTK_MENU_SHELL (menu)->active ? menu->toplevel : menu->tearoff_window), 
		   x, y);
  gtk_widget_set_usize (GTK_MENU_SHELL (menu)->active ?
			menu->toplevel : menu->tearoff_hbox,
			-1, requisition.height);
  menu->scroll_offset = scroll_offset;
}

static void
gtk_menu_stop_scrolling (GtkMenu *menu)
{
  if (menu->timeout_id)
    {
      g_source_remove (menu->timeout_id);
      menu->timeout_id = 0;
    }
}


static void
gtk_menu_scroll_to (GtkMenu *menu,
		    gint    offset)
{
  GtkWidget *widget;
  gint x, y;
  gint view_width, view_height;
  gint border_width;
  gboolean last_visible;
  gint menu_height;

  widget = GTK_WIDGET (menu);

  if (menu->tearoff_active &&
      menu->tearoff_adjustment &&
      (menu->tearoff_adjustment->value != offset))
    {
      menu->tearoff_adjustment->value = offset;
      gtk_adjustment_value_changed (menu->tearoff_adjustment);
    }
  
  /* Scroll the menu: */
  gdk_window_move (menu->bin_window, 0, -offset);

  /* Move/resize the viewport according to arrows: */
  gdk_window_get_size (widget->window, &view_width, &view_height);

  border_width = GTK_CONTAINER (menu)->border_width;
  view_width -= (border_width + widget->style->xthickness) * 2;
  view_height -= (border_width + widget->style->ythickness) * 2;
  menu_height = widget->requisition.height - (border_width + widget->style->ythickness) * 2;

  x = border_width + widget->style->xthickness;
  y = border_width + widget->style->ythickness;
  
  if (!menu->tearoff_active)
    {
      last_visible = menu->upper_arrow_visible;
      menu->upper_arrow_visible = (offset > 0);

      if (menu->upper_arrow_visible)
	view_height -= MENU_SCROLL_ARROW_HEIGHT;
      
      if ( (last_visible != menu->upper_arrow_visible) &&
	   !menu->upper_arrow_visible)
	{
	  menu->upper_arrow_prelight = FALSE;
	  
	  /* If we hid the upper arrow, possibly remove timeout */
	  if (menu->scroll_step < 0)
	    gtk_menu_stop_scrolling (menu);
	}
      
      last_visible = menu->lower_arrow_visible;
      menu->lower_arrow_visible = (view_height + offset < menu_height);
      
      if (menu->lower_arrow_visible)
	view_height -= MENU_SCROLL_ARROW_HEIGHT;
      
      if ( (last_visible != menu->lower_arrow_visible) &&
	   !menu->lower_arrow_visible)
	{
	  menu->lower_arrow_prelight = FALSE;
	  
	  /* If we hid the lower arrow, possibly remove timeout */
	  if (menu->scroll_step > 0)
	    gtk_menu_stop_scrolling (menu);
	}
      
      if (menu->upper_arrow_visible)
	y += MENU_SCROLL_ARROW_HEIGHT;
    }
  
  gdk_window_move_resize (menu->view_window,
			  x,
			  y,
			  view_width,
			  view_height);

  menu->scroll_offset = offset;
}

static void
gtk_menu_scroll_item_visible (GtkMenuShell    *menu_shell,
			      GtkWidget       *menu_item)
{
  GtkMenu *menu;
  GtkWidget *child;
  GList *children;
  GtkRequisition child_requisition;
  gint child_offset, child_height;
  gint width, height;
  gint y;
  gint arrow_height;
  gboolean last_child = 0;
  
  menu = GTK_MENU (menu_shell);

  /* We need to check if the selected item fully visible.
   * If not we need to scroll the menu so that it becomes fully
   * visible.
   */

  child = NULL;
  child_offset = 0;
  child_height = 0;
  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      
      if (GTK_WIDGET_VISIBLE (child))
	{
	  gtk_widget_size_request (child, &child_requisition);
	  child_offset += child_height;
	  child_height = child_requisition.height;
	}
      
      if (child == menu_item)
	{
	  last_child = (children == NULL);
	  break;
	}
    }

  if (child == menu_item)
    {
      y = menu->scroll_offset;
      gdk_window_get_size (GTK_WIDGET (menu)->window, &width, &height);

      height -= 2*GTK_CONTAINER (menu)->border_width + 2*GTK_WIDGET (menu)->style->ythickness;
      
      if (child_offset + child_height <= y)
	{
	  /* Ignore the enter event we might get if the pointer is on the menu
	   */
	  menu_shell->ignore_enter = TRUE;
	  gtk_menu_scroll_to (menu, child_offset);
	}
      else
	{
	  arrow_height = 0;
	  if (menu->upper_arrow_visible && !menu->tearoff_active)
	    arrow_height += MENU_SCROLL_ARROW_HEIGHT;
	  if (menu->lower_arrow_visible && !menu->tearoff_active)
	    arrow_height += MENU_SCROLL_ARROW_HEIGHT;
	  
	  if (child_offset >= y + height - arrow_height)
	    {
	      arrow_height = 0;
	      if (!last_child && !menu->tearoff_active)
		arrow_height += MENU_SCROLL_ARROW_HEIGHT;
	      
	      y = child_offset + child_height - height + arrow_height;
	      if ((y > 0) && !menu->tearoff_active)
		{
		  /* Need upper arrow */
		  arrow_height += MENU_SCROLL_ARROW_HEIGHT;
		  y = child_offset + child_height - height + arrow_height;
		}
	      /* Ignore the enter event we might get if the pointer is on the menu
	       */
	      menu_shell->ignore_enter = TRUE;
	      gtk_menu_scroll_to (menu, y);
	    }
	}    
      
    }
}

static void
gtk_menu_select_item (GtkMenuShell  *menu_shell,
		      GtkWidget     *menu_item)
{
  GtkMenu *menu = GTK_MENU (menu_shell);

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (menu)))
    gtk_menu_scroll_item_visible (menu_shell, menu_item);

  GTK_MENU_SHELL_CLASS (parent_class)->select_item (menu_shell, menu_item);
}


/* Reparent the menu, taking care of the refcounting
 *
 * If unrealize is true we force a unrealize while reparenting the parent.
 * This can help eliminate flicker in some cases.
 *
 * What happens is that when the menu is unrealized and then re-realized,
 * the allocations are as follows:
 *
 *  parent - 1x1 at (0,0) 
 *  child1 - 100x20 at (0,0)
 *  child2 - 100x20 at (0,20)
 *  child3 - 100x20 at (0,40)
 *
 * That is, the parent is small but the children are full sized. Then,
 * when the queued_resize gets processed, the parent gets resized to
 * full size. 
 *
 * But in order to eliminate flicker when scrolling, gdkgeometry-x11.c
 * contains the following logic:
 * 
 * - if a move or resize operation on a window would change the clip 
 *   region on the children, then before the window is resized
 *   the background for children is temporarily set to None, the
 *   move/resize done, and the background for the children restored.
 *
 * So, at the point where the parent is resized to final size, the
 * background for the children is temporarily None, and thus they
 * are not cleared to the background color and the previous background
 * (the image of the menu) is left in place.
 */
static void 
gtk_menu_reparent (GtkMenu      *menu, 
		   GtkWidget    *new_parent, 
		   gboolean      unrealize)
{
  GtkObject *object = GTK_OBJECT (menu);
  GtkWidget *widget = GTK_WIDGET (menu);
  gboolean was_floating = GTK_OBJECT_FLOATING (object);

  gtk_object_ref (object);
  gtk_object_sink (object);

  if (unrealize)
    {
      gtk_object_ref (object);
      gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      gtk_object_unref (object);
    }
  else
    gtk_widget_reparent (GTK_WIDGET (menu), new_parent);
  
  if (was_floating)
    GTK_OBJECT_SET_FLAGS (object, GTK_FLOATING);
  else
    gtk_object_unref (object);
}

static void
gtk_menu_show_all (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_MENU (widget));

  /* Show children, but not self. */
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_show_all, NULL);
}


static void
gtk_menu_hide_all (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_MENU (widget));

  /* Hide children, but not self. */
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_hide_all, NULL);
}


