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

#include "gtknotebook.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtklabel.h"
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkbindings.h"


#define TAB_OVERLAP    2
#define TAB_CURVATURE  1
#define ARROW_SIZE     12
#define ARROW_SPACING  0
#define NOTEBOOK_INIT_SCROLL_DELAY (200)
#define NOTEBOOK_SCROLL_DELAY      (100)


enum {
  SWITCH_PAGE,
  FOCUS_TAB,
  SELECT_PAGE,
  CHANGE_CURRENT_PAGE,
  MOVE_FOCUS_OUT,
  LAST_SIGNAL
};

enum {
  STEP_PREV,
  STEP_NEXT
};

enum {
  PROP_0,
  PROP_TAB_POS,
  PROP_SHOW_TABS,
  PROP_SHOW_BORDER,
  PROP_SCROLLABLE,
  PROP_TAB_BORDER,
  PROP_TAB_HBORDER,
  PROP_TAB_VBORDER,
  PROP_PAGE,
  PROP_ENABLE_POPUP,
  PROP_HOMOGENEOUS
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_TAB_LABEL,
  CHILD_PROP_MENU_LABEL,
  CHILD_PROP_POSITION,
  CHILD_PROP_TAB_EXPAND,
  CHILD_PROP_TAB_FILL,
  CHILD_PROP_TAB_PACK
};

#define GTK_NOTEBOOK_PAGE(_glist_)         ((GtkNotebookPage *)((GList *)(_glist_))->data)

struct _GtkNotebookPage
{
  GtkWidget *child;
  GtkWidget *tab_label;
  GtkWidget *menu_label;
  GtkWidget *last_focus_child;	/* Last descendant of the page that had focus */

  guint default_menu : 1;	/* If true, we create the menu label ourself */
  guint default_tab  : 1;	/* If true, we create the tab label ourself */
  guint expand       : 1;
  guint fill         : 1;
  guint pack         : 1;

  GtkRequisition requisition;
  GtkAllocation allocation;

  guint mnemonic_activate_signal;
};

#ifdef G_DISABLE_CHECKS
#define CHECK_FIND_CHILD(notebook, child)                           \
 gtk_notebook_find_child (notebook, child, G_STRLOC)
#else
#define CHECK_FIND_CHILD(notebook, child)                           \
 gtk_notebook_find_child (notebook, child, NULL)
#endif
 
/*** GtkNotebook Methods ***/
static void gtk_notebook_class_init          (GtkNotebookClass *klass);
static void gtk_notebook_init                (GtkNotebook      *notebook);

static gboolean gtk_notebook_select_page         (GtkNotebook      *notebook,
						  gboolean          move_focus);
static gboolean gtk_notebook_focus_tab           (GtkNotebook      *notebook,
						  GtkNotebookTab    type);
static void     gtk_notebook_change_current_page (GtkNotebook      *notebook,
						  gint              offset);
static void     gtk_notebook_move_focus_out      (GtkNotebook      *notebook,
						  GtkDirectionType  direction_type);

/*** GtkObject Methods ***/
static void gtk_notebook_destroy             (GtkObject        *object);
static void gtk_notebook_set_property	     (GObject         *object,
					      guint            prop_id,
					      const GValue    *value,
					      GParamSpec      *pspec);
static void gtk_notebook_get_property	     (GObject         *object,
					      guint            prop_id,
					      GValue          *value,
					      GParamSpec      *pspec);

/*** GtkWidget Methods ***/
static void gtk_notebook_map                 (GtkWidget        *widget);
static void gtk_notebook_unmap               (GtkWidget        *widget);
static void gtk_notebook_realize             (GtkWidget        *widget);
static void gtk_notebook_unrealize           (GtkWidget        *widget);
static void gtk_notebook_size_request        (GtkWidget        *widget,
					      GtkRequisition   *requisition);
static void gtk_notebook_size_allocate       (GtkWidget        *widget,
					      GtkAllocation    *allocation);
static gint gtk_notebook_expose              (GtkWidget        *widget,
					      GdkEventExpose   *event);
static gint gtk_notebook_button_press        (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint gtk_notebook_button_release      (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint gtk_notebook_enter_notify        (GtkWidget        *widget,
					      GdkEventCrossing *event);
static gint gtk_notebook_leave_notify        (GtkWidget        *widget,
					      GdkEventCrossing *event);
static gint gtk_notebook_motion_notify       (GtkWidget        *widget,
					      GdkEventMotion   *event);
static gint gtk_notebook_focus_in            (GtkWidget        *widget,
					      GdkEventFocus    *event);
static void gtk_notebook_draw_focus          (GtkWidget        *widget);
static gint gtk_notebook_focus               (GtkWidget        *widget,
					      GtkDirectionType  direction);

/*** GtkContainer Methods ***/
static void gtk_notebook_set_child_property  (GtkContainer     *container,
					      GtkWidget        *child,
					      guint             property_id,
					      const GValue     *value,
					      GParamSpec       *pspec);
static void gtk_notebook_get_child_property  (GtkContainer     *container,
					      GtkWidget        *child,
					      guint             property_id,
					      GValue           *value,
					      GParamSpec       *pspec);
static void gtk_notebook_add                 (GtkContainer     *container,
					      GtkWidget        *widget);
static void gtk_notebook_remove              (GtkContainer     *container,
					      GtkWidget        *widget);
static void gtk_notebook_set_focus_child     (GtkContainer     *container,
					      GtkWidget        *child);
static GType gtk_notebook_child_type       (GtkContainer     *container);
static void gtk_notebook_forall              (GtkContainer     *container,
					      gboolean		include_internals,
					      GtkCallback       callback,
					      gpointer          callback_data);

/*** GtkNotebook Private Functions ***/
static void gtk_notebook_redraw_tabs         (GtkNotebook      *notebook);
static void gtk_notebook_redraw_arrows       (GtkNotebook      *notebook);
static void gtk_notebook_real_remove         (GtkNotebook      *notebook,
					      GList            *list,
					      gboolean		destroying);
static void gtk_notebook_update_labels       (GtkNotebook      *notebook);
static gint gtk_notebook_timer               (GtkNotebook      *notebook);
static gint gtk_notebook_page_compare        (gconstpointer     a,
					      gconstpointer     b);
static GList* gtk_notebook_find_child        (GtkNotebook      *notebook,
					      GtkWidget        *child,
					      const gchar      *function);
static gint  gtk_notebook_real_page_position (GtkNotebook      *notebook,
					      GList            *list);
static GList * gtk_notebook_search_page      (GtkNotebook      *notebook,
					      GList            *list,
					      gint              direction,
					      gboolean          find_visible);

/*** GtkNotebook Drawing Functions ***/
static void gtk_notebook_paint               (GtkWidget        *widget,
					      GdkRectangle     *area);
static void gtk_notebook_draw_tab            (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      GdkRectangle     *area);
static void gtk_notebook_draw_arrow          (GtkNotebook      *notebook,
					      guint             arrow);

/*** GtkNotebook Size Allocate Functions ***/
static void gtk_notebook_pages_allocate      (GtkNotebook      *notebook);
static void gtk_notebook_page_allocate       (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      GtkAllocation    *allocation);
static void gtk_notebook_calc_tabs           (GtkNotebook      *notebook,
			                      GList            *start,
					      GList           **end,
					      gint             *tab_space,
					      guint             direction);

/*** GtkNotebook Page Switch Methods ***/
static void gtk_notebook_real_switch_page    (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      guint             page_num);

/*** GtkNotebook Page Switch Functions ***/
static void gtk_notebook_switch_page         (GtkNotebook      *notebook,
					      GtkNotebookPage  *page,
					      gint              page_num);
static gint gtk_notebook_page_select         (GtkNotebook      *notebook,
					      gboolean          move_focus);
static void gtk_notebook_switch_focus_tab    (GtkNotebook      *notebook,
                                              GList            *new_child);
static void gtk_notebook_menu_switch_page    (GtkWidget        *widget,
					      GtkNotebookPage  *page);

/*** GtkNotebook Menu Functions ***/
static void gtk_notebook_menu_item_create    (GtkNotebook      *notebook,
					      GList            *list);
static void gtk_notebook_menu_label_unparent (GtkWidget        *widget,
					      gpointer          data);
static void gtk_notebook_menu_detacher       (GtkWidget        *widget,
					      GtkMenu          *menu);

/*** GtkNotebook Private Setters ***/
static void gtk_notebook_set_homogeneous_tabs_internal (GtkNotebook *notebook,
							gboolean     homogeneous);
static void gtk_notebook_set_tab_border_internal       (GtkNotebook *notebook,
							guint        border_width);
static void gtk_notebook_set_tab_hborder_internal      (GtkNotebook *notebook,
							guint        tab_hborder);
static void gtk_notebook_set_tab_vborder_internal      (GtkNotebook *notebook,
							guint        tab_vborder);

static gboolean focus_tabs_in  (GtkNotebook      *notebook);
static gboolean focus_child_in (GtkNotebook      *notebook,
				GtkDirectionType  direction);

static GtkContainerClass *parent_class = NULL;
static guint notebook_signals[LAST_SIGNAL] = { 0 };

GType
gtk_notebook_get_type (void)
{
  static GType notebook_type = 0;

  if (!notebook_type)
    {
      static const GTypeInfo notebook_info =
      {
	sizeof (GtkNotebookClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_notebook_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkNotebook),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_notebook_init,
      };

      notebook_type = g_type_register_static (GTK_TYPE_CONTAINER, "GtkNotebook",
					      &notebook_info, 0);
    }

  return notebook_type;
}

static void
add_tab_bindings (GtkBindingSet    *binding_set,
		  GdkModifierType   modifiers,
		  GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set, GDK_Tab, modifiers,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Tab, modifiers,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
add_arrow_bindings (GtkBindingSet    *binding_set,
		    guint             keysym,
		    GtkDirectionType  direction)
{
  guint keypad_keysym = keysym - GDK_Left + GDK_KP_Left;
  
  gtk_binding_entry_add_signal (binding_set, keysym, GDK_CONTROL_MASK,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, keypad_keysym, GDK_CONTROL_MASK,
                                "move_focus_out", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
gtk_notebook_class_init (GtkNotebookClass *class)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);
  GtkBindingSet *binding_set;
  
  parent_class = g_type_class_peek_parent (class);

  gobject_class->set_property = gtk_notebook_set_property;
  gobject_class->get_property = gtk_notebook_get_property;
  object_class->destroy = gtk_notebook_destroy;

  widget_class->map = gtk_notebook_map;
  widget_class->unmap = gtk_notebook_unmap;
  widget_class->realize = gtk_notebook_realize;
  widget_class->unrealize = gtk_notebook_unrealize;
  widget_class->size_request = gtk_notebook_size_request;
  widget_class->size_allocate = gtk_notebook_size_allocate;
  widget_class->expose_event = gtk_notebook_expose;
  widget_class->button_press_event = gtk_notebook_button_press;
  widget_class->button_release_event = gtk_notebook_button_release;
  widget_class->enter_notify_event = gtk_notebook_enter_notify;
  widget_class->leave_notify_event = gtk_notebook_leave_notify;
  widget_class->motion_notify_event = gtk_notebook_motion_notify;
  widget_class->focus_in_event = gtk_notebook_focus_in;
  widget_class->focus = gtk_notebook_focus;
  
  container_class->add = gtk_notebook_add;
  container_class->remove = gtk_notebook_remove;
  container_class->forall = gtk_notebook_forall;
  container_class->set_focus_child = gtk_notebook_set_focus_child;
  container_class->get_child_property = gtk_notebook_get_child_property;
  container_class->set_child_property = gtk_notebook_set_child_property;
  container_class->child_type = gtk_notebook_child_type;

  class->switch_page = gtk_notebook_real_switch_page;

  class->focus_tab = gtk_notebook_focus_tab;
  class->select_page = gtk_notebook_select_page;
  class->change_current_page = gtk_notebook_change_current_page;
  class->move_focus_out = gtk_notebook_move_focus_out;
  
  g_object_class_install_property (gobject_class,
				   PROP_PAGE,
				   g_param_spec_int ("page",
 						     _("Page"),
 						     _("The index of the current page"),
 						     0,
 						     G_MAXINT,
 						     0,
 						     G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_TAB_POS,
				   g_param_spec_enum ("tab_pos",
 						      _("Tab Position"),
 						      _("Which side of the notebook holds the tabs"),
 						      GTK_TYPE_POSITION_TYPE,
 						      GTK_POS_TOP,
 						      G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_TAB_BORDER,
				   g_param_spec_uint ("tab_border",
 						      _("Tab Border"),
 						      _("Width of the border around the tab labels"),
 						      0,
 						      G_MAXUINT,
 						      2,
 						      G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
				   PROP_TAB_HBORDER,
				   g_param_spec_uint ("tab_hborder",
 						      _("Horizontal Tab Border"),
 						      _("Width of the horizontal border of tab labels"),
 						      0,
 						      G_MAXUINT,
 						      2,
 						      G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_TAB_VBORDER,
				   g_param_spec_uint ("tab_vborder",
 						      _("Vertical Tab Border"),
 						      _("Width of the vertical border of tab labels"),
 						      0,
 						      G_MAXUINT,
 						      2,
 						      G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_TABS,
				   g_param_spec_boolean ("show_tabs",
 							 _("Show Tabs"),
 							 _("Whether tabs should be shown or not"),
 							 TRUE,
 							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_BORDER,
				   g_param_spec_boolean ("show_border",
 							 _("Show Border"),
 							 _("Whether the border should be shown or not"),
 							 TRUE,
 							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SCROLLABLE,
				   g_param_spec_boolean ("scrollable",
 							 _("Scrollable"),
 							 _("If TRUE, scroll arrows are added if there are too many tabs to fit"),
 							 FALSE,
 							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_ENABLE_POPUP,
				   g_param_spec_boolean ("enable_popup",
 							 _("Enable Popup"),
 							 _("If TRUE, pressing the right mouse button on the notebook pops up a menu that you can use to go to a page"),
 							 FALSE,
 							 G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HOMOGENEOUS,
				   g_param_spec_boolean ("homogeneous",
 							 _("Homogeneous"),
 							 _("Whether tabs should have homogeneous sizes"),
 							 FALSE,
							 G_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_TAB_LABEL,
					      g_param_spec_string ("tab_label", 
								   _("Tab label"),
								   _("The string displayed on the childs tab label"),
								   NULL,
								   G_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_MENU_LABEL,
					      g_param_spec_string ("menu_label", 
								   _("Menu label"), 
								   _("The string displayed in the childs menu entry"),
								   NULL,
								   G_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_POSITION,
					      g_param_spec_int ("position", 
								_("Position"), 
								_("The index of the child in the parent"),
								-1, G_MAXINT, 0,
								G_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_TAB_EXPAND,
					      g_param_spec_boolean ("tab_expand", 
								    _("Tab expand"), 
								    _("Whether to expand the childs tab or not"),
								    TRUE,
								    G_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_TAB_FILL,
					      g_param_spec_boolean ("tab_fill", 
								    _("Tab fill"), 
								    _("Wheather the childs tab should fill the allocated area or not"),
								    TRUE,
								    G_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_TAB_PACK,
					      g_param_spec_enum ("tab_pack", 
								 _("Tab pack type"),
								 _("A GtkPackType indicating whether the child is packed with reference to the start or end of the parent"),
								 GTK_TYPE_PACK_TYPE, GTK_PACK_START,
								 G_PARAM_READWRITE));
  
  notebook_signals[SWITCH_PAGE] =
    g_signal_new ("switch_page",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkNotebookClass, switch_page),
		  NULL, NULL,
		  _gtk_marshal_VOID__POINTER_UINT,
		  G_TYPE_NONE, 2,
		  G_TYPE_POINTER,
		  G_TYPE_UINT);
  notebook_signals[FOCUS_TAB] = 
    g_signal_new ("focus_tab",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, focus_tab),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__ENUM,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_NOTEBOOK_TAB);
  notebook_signals[SELECT_PAGE] = 
    g_signal_new ("select_page",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, select_page),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_BOOLEAN);
  notebook_signals[CHANGE_CURRENT_PAGE] = 
    g_signal_new ("change_current_page",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, change_current_page),
                  NULL, NULL,
                  _gtk_marshal_VOID__INT,
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);
  notebook_signals[MOVE_FOCUS_OUT] =
    g_signal_new ("move_focus_out",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkNotebookClass, move_focus_out),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_DIRECTION_TYPE);
  
  
  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_space, 0,
                                "select_page", 1, 
                                G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KP_Space, 0,
                                "select_page", 1, 
                                G_TYPE_BOOLEAN, FALSE);
  
  gtk_binding_entry_add_signal (binding_set,
                                GDK_Home, 0,
                                "focus_tab", 1, 
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_FIRST);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KP_Home, 0,
                                "focus_tab", 1, 
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_FIRST);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_End, 0,
                                "focus_tab", 1, 
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_LAST);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KP_End, 0,
                                "focus_tab", 1, 
                                GTK_TYPE_NOTEBOOK_TAB, GTK_NOTEBOOK_TAB_LAST);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_Page_Up, GDK_CONTROL_MASK,
                                "change_current_page", 1,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_Page_Down, GDK_CONTROL_MASK,
                                "change_current_page", 1,
                                G_TYPE_INT, 1);

  add_arrow_bindings (binding_set, GDK_Up, GTK_DIR_UP);
  add_arrow_bindings (binding_set, GDK_Down, GTK_DIR_DOWN);
  add_arrow_bindings (binding_set, GDK_Left, GTK_DIR_LEFT);
  add_arrow_bindings (binding_set, GDK_Right, GTK_DIR_RIGHT);

  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
}

static void
gtk_notebook_init (GtkNotebook *notebook)
{
  GTK_WIDGET_SET_FLAGS (notebook, GTK_CAN_FOCUS);
  GTK_WIDGET_SET_FLAGS (notebook, GTK_NO_WINDOW);

  notebook->cur_page = NULL;
  notebook->children = NULL;
  notebook->first_tab = NULL;
  notebook->focus_tab = NULL;
  notebook->event_window = NULL;
  notebook->menu = NULL;

  notebook->tab_hborder = 2;
  notebook->tab_vborder = 2;

  notebook->show_tabs = TRUE;
  notebook->show_border = TRUE;
  notebook->tab_pos = GTK_POS_TOP;
  notebook->scrollable = FALSE;
  notebook->in_child = 0;
  notebook->click_child = 0;
  notebook->button = 0;
  notebook->need_timer = 0;
  notebook->child_has_focus = FALSE;
  notebook->have_visible_child = FALSE;
  notebook->focus_out = FALSE;
}

static gboolean
gtk_notebook_select_page (GtkNotebook *notebook,
                          gboolean     move_focus)
{
  if (gtk_widget_is_focus (GTK_WIDGET (notebook)))
    {
      gtk_notebook_page_select (notebook, move_focus);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_notebook_focus_tab (GtkNotebook       *notebook,
                        GtkNotebookTab     type)
{
  GList *list;

  if (gtk_widget_is_focus (GTK_WIDGET (notebook)))
    {
      switch (type)
	{
	case GTK_NOTEBOOK_TAB_FIRST:
	  list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, TRUE);
	  if (list)
	    gtk_notebook_switch_focus_tab (notebook, list);
	  break;
	case GTK_NOTEBOOK_TAB_LAST:
	  list = gtk_notebook_search_page (notebook, NULL, STEP_PREV, TRUE);
	  if (list)
	    gtk_notebook_switch_focus_tab (notebook, list);
	  break;
	}

      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_notebook_change_current_page (GtkNotebook *notebook,
				  gint         offset)
{
  GList *current = NULL;
  
  if (notebook->cur_page)
    current = g_list_find (notebook->children, notebook->cur_page);

  while (offset != 0)
    {
      current = gtk_notebook_search_page (notebook, current, offset < 0 ? STEP_PREV : STEP_NEXT, TRUE);
      offset += offset < 0 ? 1 : -1;
    }

  if (current)
    gtk_notebook_switch_page (notebook, current->data, -1);
  else
    gdk_display_beep (gtk_widget_get_display (GTK_WIDGET (notebook)));
}

static GtkDirectionType
get_effective_direction (GtkNotebook      *notebook,
			 GtkDirectionType  direction)
{
  /* Remap the directions into the effective direction it would be for a
   * GTK_POS_TOP notebook
   */
#define D(rest) GTK_DIR_##rest

  static const GtkDirectionType translate_direction[4][6] = {
    /* LEFT */   { D(TAB_FORWARD),  D(TAB_BACKWARD), D(LEFT), D(RIGHT), D(UP),   D(DOWN) },
    /* RIGHT */  { D(TAB_BACKWARD), D(TAB_FORWARD),  D(LEFT), D(RIGHT), D(DOWN), D(UP)   },
    /* TOP */    { D(TAB_FORWARD),  D(TAB_BACKWARD), D(UP),   D(DOWN),  D(LEFT), D(RIGHT) },
    /* BOTTOM */ { D(TAB_BACKWARD), D(TAB_FORWARD),  D(DOWN), D(UP),    D(LEFT), D(RIGHT) },
  };

#undef D

  return translate_direction[notebook->tab_pos][direction];
}

static void
gtk_notebook_move_focus_out (GtkNotebook      *notebook,
			     GtkDirectionType  direction_type)
{
  GtkDirectionType effective_direction = get_effective_direction (notebook, direction_type);
  GtkWidget *toplevel;
  
  if (GTK_CONTAINER (notebook)->focus_child && effective_direction == GTK_DIR_UP)
    if (focus_tabs_in (notebook))
      return;
  
  if (gtk_widget_is_focus (GTK_WIDGET (notebook)) && effective_direction == GTK_DIR_DOWN)
    if (focus_child_in (notebook, GTK_DIR_TAB_FORWARD))
      return;

  /* At this point, we know we should be focusing out of the notebook entirely. We
   * do this by setting a flag, then propagating the focus motion to the notebook.
   */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (notebook));
  if (!GTK_WIDGET_TOPLEVEL (toplevel))
    return;

  g_object_ref (notebook);
  
  notebook->focus_out = TRUE;
  g_signal_emit_by_name (toplevel, "move_focus", direction_type);
  notebook->focus_out = FALSE;
  
  g_object_unref (notebook);

}

/**
 * gtk_notebook_new:
 * 
 * Creates a new #GtkNotebook widget with no pages.

 * Return value: the newly created #GtkNotebook
 **/
GtkWidget*
gtk_notebook_new (void)
{
  return g_object_new (GTK_TYPE_NOTEBOOK, NULL);
}

/* Private GtkObject Methods :
 * 
 * gtk_notebook_destroy
 * gtk_notebook_set_arg
 * gtk_notebook_get_arg
 */
static void
gtk_notebook_destroy (GtkObject *object)
{
  GList *children;
  GtkNotebook *notebook = GTK_NOTEBOOK (object);
  
  if (notebook->menu)
    gtk_notebook_popup_disable (notebook);

  children = notebook->children;
  while (children)
    {
      GList *child = children;
      children = child->next;
      
      gtk_notebook_real_remove (notebook, child, TRUE);
    }
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_notebook_set_property (GObject         *object,
			   guint            prop_id,
			   const GValue    *value,
			   GParamSpec      *pspec)
{
  GtkNotebook *notebook;

  notebook = GTK_NOTEBOOK (object);

  switch (prop_id)
    {
    case PROP_SHOW_TABS:
      gtk_notebook_set_show_tabs (notebook, g_value_get_boolean (value));
      break;
    case PROP_SHOW_BORDER:
      gtk_notebook_set_show_border (notebook, g_value_get_boolean (value));
      break;
    case PROP_SCROLLABLE:
      gtk_notebook_set_scrollable (notebook, g_value_get_boolean (value));
      break;
    case PROP_ENABLE_POPUP:
      if (g_value_get_boolean (value))
	gtk_notebook_popup_enable (notebook);
      else
	gtk_notebook_popup_disable (notebook);
      break;
    case PROP_HOMOGENEOUS:
      gtk_notebook_set_homogeneous_tabs_internal (notebook, g_value_get_boolean (value));
      break;  
    case PROP_PAGE:
      gtk_notebook_set_current_page (notebook, g_value_get_int (value));
      break;
    case PROP_TAB_POS:
      gtk_notebook_set_tab_pos (notebook, g_value_get_enum (value));
      break;
    case PROP_TAB_BORDER:
      gtk_notebook_set_tab_border_internal (notebook, g_value_get_uint (value));
      break;
    case PROP_TAB_HBORDER:
      gtk_notebook_set_tab_hborder_internal (notebook, g_value_get_uint (value));
      break;
    case PROP_TAB_VBORDER:
      gtk_notebook_set_tab_vborder_internal (notebook, g_value_get_uint (value));
      break;
    default:
      break;
    }
}

static void
gtk_notebook_get_property (GObject         *object,
			   guint            prop_id,
			   GValue          *value,
			   GParamSpec      *pspec)
{
  GtkNotebook *notebook;

  notebook = GTK_NOTEBOOK (object);

  switch (prop_id)
    {
    case PROP_SHOW_TABS:
      g_value_set_boolean (value, notebook->show_tabs);
      break;
    case PROP_SHOW_BORDER:
      g_value_set_boolean (value, notebook->show_border);
      break;
    case PROP_SCROLLABLE:
      g_value_set_boolean (value, notebook->scrollable);
      break;
    case PROP_ENABLE_POPUP:
      g_value_set_boolean (value, notebook->menu != NULL);
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, notebook->homogeneous);
      break;
    case PROP_PAGE:
      g_value_set_int (value, gtk_notebook_get_current_page (notebook));
      break;
    case PROP_TAB_POS:
      g_value_set_enum (value, notebook->tab_pos);
      break;
    case PROP_TAB_HBORDER:
      g_value_set_uint (value, notebook->tab_hborder);
      break;
    case PROP_TAB_VBORDER:
      g_value_set_uint (value, notebook->tab_vborder);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Private GtkWidget Methods :
 * 
 * gtk_notebook_map
 * gtk_notebook_unmap
 * gtk_notebook_realize
 * gtk_notebook_size_request
 * gtk_notebook_size_allocate
 * gtk_notebook_expose
 * gtk_notebook_button_press
 * gtk_notebook_button_release
 * gtk_notebook_enter_notify
 * gtk_notebook_leave_notify
 * gtk_notebook_motion_notify
 * gtk_notebook_focus_in
 * gtk_notebook_focus_out
 * gtk_notebook_draw_focus
 * gtk_notebook_style_set
 */
static gboolean
gtk_notebook_get_event_window_position (GtkNotebook  *notebook,
					GdkRectangle *rectangle)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  gint border_width = GTK_CONTAINER (notebook)->border_width;
  GtkNotebookPage *visible_page = NULL;
  GList *tmp_list;

  for (tmp_list = notebook->children; tmp_list; tmp_list = tmp_list->next)
    {
      GtkNotebookPage *page = tmp_list->data;
      if (GTK_WIDGET_VISIBLE (page->child))
	{
	  visible_page = page;
	  break;
	}
    }

  if (notebook->show_tabs && visible_page)
    {
      if (rectangle)
	{
	  rectangle->x = widget->allocation.x + border_width;
	  rectangle->y = widget->allocation.y + border_width;
	  
	  switch (notebook->tab_pos)
	    {
	    case GTK_POS_TOP:
	    case GTK_POS_BOTTOM:
	      rectangle->width = widget->allocation.width - 2 * border_width;
	      rectangle->height = visible_page->requisition.height;
	      if (notebook->tab_pos == GTK_POS_BOTTOM)
		rectangle->y += widget->allocation.height - 2 * border_width - rectangle->height;
	      break;
	    case GTK_POS_LEFT:
	    case GTK_POS_RIGHT:
	      rectangle->width = visible_page->requisition.width;
	      rectangle->height = widget->allocation.height - 2 * border_width;
	      if (notebook->tab_pos == GTK_POS_RIGHT)
		rectangle->x += widget->allocation.width - 2 * border_width - rectangle->width;
	      break;
	    }
	}

      return TRUE;
    }
  else
    {
      if (rectangle)
	{
	  rectangle->x = rectangle->y = 0;
	  rectangle->width = rectangle->height = 10;
	}
    }

  return FALSE;
}

static void
gtk_notebook_map (GtkWidget *widget)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  notebook = GTK_NOTEBOOK (widget);

  if (notebook->cur_page && 
      GTK_WIDGET_VISIBLE (notebook->cur_page->child) &&
      !GTK_WIDGET_MAPPED (notebook->cur_page->child))
    gtk_widget_map (notebook->cur_page->child);

  if (notebook->scrollable)
    gtk_notebook_pages_allocate (notebook);
  else
    {
      children = notebook->children;

      while (children)
	{
	  page = children->data;
	  children = children->next;

	  if (page->tab_label &&
	      GTK_WIDGET_VISIBLE (page->tab_label) &&
	      !GTK_WIDGET_MAPPED (page->tab_label))
	    gtk_widget_map (page->tab_label);
	}
    }

  if (gtk_notebook_get_event_window_position (notebook, NULL))
    gdk_window_show_unraised (notebook->event_window);
}

static void
gtk_notebook_unmap (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  gdk_window_hide (GTK_NOTEBOOK (widget)->event_window);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
gtk_notebook_realize (GtkWidget *widget)
{
  GtkNotebook *notebook;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkRectangle event_window_pos;

  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  notebook = GTK_NOTEBOOK (widget);
  GTK_WIDGET_SET_FLAGS (notebook, GTK_REALIZED);

  gtk_notebook_get_event_window_position (notebook, &event_window_pos);
  
  widget->window = gtk_widget_get_parent_window (widget);
  g_object_ref (widget->window);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = event_window_pos.x;
  attributes.y = event_window_pos.y;
  attributes.width = event_window_pos.width;
  attributes.height = event_window_pos.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  notebook->event_window = gdk_window_new (gtk_widget_get_parent_window (widget), 
					   &attributes, attributes_mask);
  gdk_window_set_user_data (notebook->event_window, notebook);

  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
gtk_notebook_unrealize (GtkWidget *widget)
{
  GtkNotebook *notebook;

  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  notebook = GTK_NOTEBOOK (widget);

  gdk_window_set_user_data (notebook->event_window, NULL);
  gdk_window_destroy (notebook->event_window);
  notebook->event_window = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_notebook_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPage *page;
  GList *children;
  GtkRequisition child_requisition;
  gboolean switch_page = FALSE;
  gint vis_pages;
  gint focus_width;

  gtk_widget_style_get (widget, "focus-line-width", &focus_width, NULL);
  
  widget->requisition.width = 0;
  widget->requisition.height = 0;

  for (children = notebook->children, vis_pages = 0; children;
       children = children->next)
    {
      page = children->data;

      if (GTK_WIDGET_VISIBLE (page->child))
	{
	  vis_pages++;
	  gtk_widget_size_request (page->child, &child_requisition);
	  
	  widget->requisition.width = MAX (widget->requisition.width,
					   child_requisition.width);
	  widget->requisition.height = MAX (widget->requisition.height,
					    child_requisition.height);

	  if (notebook->menu && page->menu_label->parent &&
	      !GTK_WIDGET_VISIBLE (page->menu_label->parent))
	    gtk_widget_show (page->menu_label->parent);
	}
      else
	{
	  if (page == notebook->cur_page)
	    switch_page = TRUE;
	  if (notebook->menu && page->menu_label->parent &&
	      GTK_WIDGET_VISIBLE (page->menu_label->parent))
	    gtk_widget_hide (page->menu_label->parent);
	}
    }

  if (notebook->show_border || notebook->show_tabs)
    {
      widget->requisition.width += widget->style->xthickness * 2;
      widget->requisition.height += widget->style->ythickness * 2;

      if (notebook->show_tabs)
	{
	  gint tab_width = 0;
	  gint tab_height = 0;
	  gint tab_max = 0;
	  gint padding;
	  
	  for (children = notebook->children; children;
	       children = children->next)
	    {
	      page = children->data;
	      
	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  if (!GTK_WIDGET_VISIBLE (page->tab_label))
		    gtk_widget_show (page->tab_label);

		  gtk_widget_size_request (page->tab_label,
					   &child_requisition);

		  page->requisition.width = 
		    child_requisition.width +
		    2 * widget->style->xthickness;
		  page->requisition.height = 
		    child_requisition.height +
		    2 * widget->style->ythickness;
		  
		  switch (notebook->tab_pos)
		    {
		    case GTK_POS_TOP:
		    case GTK_POS_BOTTOM:
		      page->requisition.height += 2 * (notebook->tab_vborder +
						       focus_width);
		      tab_height = MAX (tab_height, page->requisition.height);
		      tab_max = MAX (tab_max, page->requisition.width);
		      break;
		    case GTK_POS_LEFT:
		    case GTK_POS_RIGHT:
		      page->requisition.width += 2 * (notebook->tab_hborder +
						      focus_width);
		      tab_width = MAX (tab_width, page->requisition.width);
		      tab_max = MAX (tab_max, page->requisition.height);
		      break;
		    }
		}
	      else if (GTK_WIDGET_VISIBLE (page->tab_label))
		gtk_widget_hide (page->tab_label);
	    }

	  children = notebook->children;

	  if (vis_pages)
	    {
	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
		  if (tab_height == 0)
		    break;

		  if (notebook->scrollable && vis_pages > 1 && 
		      widget->requisition.width < tab_width)
		    tab_height = MAX (tab_height, ARROW_SIZE);

		  padding = 2 * (TAB_CURVATURE + focus_width +
				 notebook->tab_hborder) - TAB_OVERLAP;
		  tab_max += padding;
		  while (children)
		    {
		      page = children->data;
		      children = children->next;
		  
		      if (!GTK_WIDGET_VISIBLE (page->child))
			continue;

		      if (notebook->homogeneous)
			page->requisition.width = tab_max;
		      else
			page->requisition.width += padding;

		      tab_width += page->requisition.width;
		      page->requisition.height = tab_height;
		    }

		  if (notebook->scrollable && vis_pages > 1 &&
		      widget->requisition.width < tab_width)
		    tab_width = tab_max + 2 * (ARROW_SIZE + ARROW_SPACING);

                  if (notebook->homogeneous && !notebook->scrollable)
                    widget->requisition.width = MAX (widget->requisition.width,
                                                     vis_pages * tab_max +
                                                     TAB_OVERLAP);
                  else
                    widget->requisition.width = MAX (widget->requisition.width,
                                                     tab_width + TAB_OVERLAP);

		  widget->requisition.height += tab_height;
		  break;
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
		  if (tab_width == 0)
		    break;

		  if (notebook->scrollable && vis_pages > 1 && 
		      widget->requisition.height < tab_height)
		    tab_width = MAX (tab_width, ARROW_SPACING +2 * ARROW_SIZE);

		  padding = 2 * (TAB_CURVATURE + focus_width +
				 notebook->tab_vborder) - TAB_OVERLAP;
		  tab_max += padding;

		  while (children)
		    {
		      page = children->data;
		      children = children->next;

		      if (!GTK_WIDGET_VISIBLE (page->child))
			continue;

		      page->requisition.width   = tab_width;

		      if (notebook->homogeneous)
			page->requisition.height = tab_max;
		      else
			page->requisition.height += padding;

		      tab_height += page->requisition.height;
		    }

		  if (notebook->scrollable && vis_pages > 1 && 
		      widget->requisition.height < tab_height)
		    tab_height = tab_max + ARROW_SIZE + ARROW_SPACING;

		  widget->requisition.width += tab_width;

                  if (notebook->homogeneous && !notebook->scrollable)
                    widget->requisition.height =
		      MAX (widget->requisition.height,
			   vis_pages * tab_max + TAB_OVERLAP);
                  else
                    widget->requisition.height =
		      MAX (widget->requisition.height,
			   tab_height + TAB_OVERLAP);

		  if (!notebook->homogeneous || notebook->scrollable)
		    vis_pages = 1;
		  widget->requisition.height = MAX (widget->requisition.height,
						    vis_pages * tab_max +
						    TAB_OVERLAP);
		  break;
		}
	    }
	}
      else
	{
	  for (children = notebook->children; children;
	       children = children->next)
	    {
	      page = children->data;
	      
	      if (page->tab_label && GTK_WIDGET_VISIBLE (page->tab_label))
		gtk_widget_hide (page->tab_label);
	    }
	}
    }

  widget->requisition.width += GTK_CONTAINER (widget)->border_width * 2;
  widget->requisition.height += GTK_CONTAINER (widget)->border_width * 2;

  if (switch_page)
    {
      if (vis_pages)
	{
	  for (children = notebook->children; children;
	       children = children->next)
	    {
	      page = children->data;
	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  gtk_notebook_switch_page (notebook, page, -1);
		  break;
		}
	    }
	}
      else if (GTK_WIDGET_VISIBLE (widget))
	{
	  widget->requisition.width = GTK_CONTAINER (widget)->border_width * 2;
	  widget->requisition.height= GTK_CONTAINER (widget)->border_width * 2;
	}
    }
  if (vis_pages && !notebook->cur_page)
    {
      children = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, TRUE);
      if (children)
	{
	  notebook->first_tab = children;
	  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (children),-1);
	}
    }
}

static void
gtk_notebook_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  gint vis_pages = 0;

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      GdkRectangle position;

      if (gtk_notebook_get_event_window_position (notebook, &position))
	{
	  gdk_window_move_resize (notebook->event_window,
				  position.x, position.y,
				  position.width, position.height);
	  gdk_window_show_unraised (notebook->event_window);
	}
      else
	gdk_window_hide (notebook->event_window);
    }

  if (notebook->children)
    {
      gint border_width = GTK_CONTAINER (widget)->border_width;
      GtkNotebookPage *page;
      GtkAllocation child_allocation;
      GList *children;
      
      child_allocation.x = widget->allocation.x + border_width;
      child_allocation.y = widget->allocation.y + border_width;
      child_allocation.width = MAX (1, allocation->width - border_width * 2);
      child_allocation.height = MAX (1, allocation->height - border_width * 2);

      if (notebook->show_tabs || notebook->show_border)
	{
	  child_allocation.x += widget->style->xthickness;
	  child_allocation.y += widget->style->ythickness;
	  child_allocation.width = MAX (1, child_allocation.width -
					widget->style->xthickness * 2);
	  child_allocation.height = MAX (1, child_allocation.height -
					 widget->style->ythickness * 2);

	  if (notebook->show_tabs && notebook->children && notebook->cur_page)
	    {
	      switch (notebook->tab_pos)
		{
		case GTK_POS_TOP:
		  child_allocation.y += notebook->cur_page->requisition.height;
		case GTK_POS_BOTTOM:
		  child_allocation.height =
		    MAX (1, child_allocation.height -
			 notebook->cur_page->requisition.height);
		  break;
		case GTK_POS_LEFT:
		  child_allocation.x += notebook->cur_page->requisition.width;
		case GTK_POS_RIGHT:
		  child_allocation.width =
		    MAX (1, child_allocation.width -
			 notebook->cur_page->requisition.width);
		  break;
		}
	    }
	}

      children = notebook->children;
      while (children)
	{
	  page = children->data;
	  children = children->next;
	  
	  if (GTK_WIDGET_VISIBLE (page->child))
	    {
	      gtk_widget_size_allocate (page->child, &child_allocation);
	      vis_pages++;
	    }
	}

      gtk_notebook_pages_allocate (notebook);
    }

  if ((vis_pages != 0) != notebook->have_visible_child)
    {
      notebook->have_visible_child = (vis_pages != 0);
      if (notebook->show_tabs)
	gtk_widget_queue_draw (widget);
    }
}

static gint
gtk_notebook_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  GtkNotebook *notebook;
  GdkRectangle child_area;
   
  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      notebook = GTK_NOTEBOOK (widget);

      gtk_notebook_paint (widget, &event->area);
      if (notebook->show_tabs)
	{
	  if (notebook->cur_page && 
	      gtk_widget_intersect (notebook->cur_page->tab_label, 
				    &event->area, &child_area))
	    gtk_notebook_draw_focus (widget);
	}

      
      if (notebook->cur_page)
	gtk_container_propagate_expose (GTK_CONTAINER (notebook),
					notebook->cur_page->child,
					event);
    }

  return FALSE;
}

static gboolean
gtk_notebook_show_arrows (GtkNotebook *notebook)
{
  gboolean show_arrow = FALSE;
  GList *children;
  
  if (!notebook->scrollable)
    return FALSE;

  children = notebook->children;
  while (children)
    {
      GtkNotebookPage *page = children->data;

      if (page->tab_label && !gtk_widget_get_child_visible (page->tab_label))
	show_arrow = TRUE;

      children = children->next;
    }

  return show_arrow;
}

static void
gtk_notebook_get_arrow_rect (GtkNotebook  *notebook,
			     GdkRectangle *rectangle)
{
  GdkRectangle event_window_pos;

  if (gtk_notebook_get_event_window_position (notebook, &event_window_pos))
    {
      rectangle->width = 2 * ARROW_SIZE + ARROW_SPACING;
      rectangle->height = ARROW_SIZE;

      switch (notebook->tab_pos)
	{
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  rectangle->x = event_window_pos.x + (event_window_pos.width - rectangle->width) / 2;
	  rectangle->y = event_window_pos.y + event_window_pos.height - rectangle->height;
	  break;
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  rectangle->x = event_window_pos.x + event_window_pos.width - rectangle->width;
	  rectangle->y = event_window_pos.y + (event_window_pos.height - rectangle->height) / 2;
	  break;
	}
    }
}

static GtkArrowType
gtk_notebook_get_arrow (GtkNotebook *notebook,
			gint         x,
			gint         y)
{
  GdkRectangle arrow_rect;
  GdkRectangle event_window_pos;

  if (gtk_notebook_show_arrows (notebook))
    {
      gtk_notebook_get_event_window_position (notebook, &event_window_pos);
      gtk_notebook_get_arrow_rect (notebook, &arrow_rect);
      
      x -= arrow_rect.x;
      y -= arrow_rect.y;
      
      if (y >= 0 && y < arrow_rect.height)
	{
	  if (x >= 0 && x < ARROW_SIZE + ARROW_SPACING / 2)
	    return GTK_ARROW_LEFT;
	  else if (x >= ARROW_SIZE + ARROW_SPACING / 2 && x < arrow_rect.width)
	    return GTK_ARROW_RIGHT;
	}
    }
      
  return 0;
}

static void
gtk_notebook_do_arrow (GtkNotebook    *notebook,
		       GtkArrowType    arrow)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  GtkDirectionType dir;
  
  if (!notebook->focus_tab ||
      gtk_notebook_search_page (notebook, notebook->focus_tab,
				arrow == GTK_ARROW_LEFT ? STEP_PREV : STEP_NEXT,
				TRUE))
    {
      if (notebook->tab_pos == GTK_POS_LEFT ||
	  notebook->tab_pos == GTK_POS_RIGHT)
	dir = (arrow == GTK_ARROW_LEFT) ? GTK_DIR_UP : GTK_DIR_DOWN;
      else
	dir = (arrow == GTK_ARROW_LEFT) ? GTK_DIR_LEFT : GTK_DIR_RIGHT;
      gtk_widget_child_focus (widget, dir);
    }
}

static gboolean
gtk_notebook_arrow_button_press (GtkNotebook    *notebook,
				 GtkArrowType    arrow,
				 GdkEventButton *event)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  
  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);
  
  notebook->button = event->button;
  notebook->click_child = arrow;
  
  if (event->button == 1)
    {
      gtk_notebook_do_arrow (notebook, arrow);
      
      if (!notebook->timer)
	{
	  notebook->timer = gtk_timeout_add 
	    (NOTEBOOK_INIT_SCROLL_DELAY, 
	     (GtkFunction) gtk_notebook_timer, (gpointer) notebook);
	  notebook->need_timer = TRUE;
	}
    }
  else if (event->button == 2)
    gtk_notebook_page_select (notebook, TRUE);
  else if (event->button == 3)
    gtk_notebook_switch_focus_tab (notebook,
				   gtk_notebook_search_page (notebook,
							     NULL,
							     arrow == GTK_ARROW_LEFT ? STEP_NEXT : STEP_PREV,
							     TRUE));
  gtk_notebook_redraw_arrows (notebook);

  return TRUE;
}

static gboolean
get_widget_coordinates (GtkWidget *widget,
			GdkEvent  *event,
			gint      *x,
			gint      *y)
{
  GdkWindow *window = ((GdkEventAny *)event)->window;
  gdouble tx, ty;

  if (!gdk_event_get_coords (event, &tx, &ty))
    return FALSE;

  while (window && window != widget->window)
    {
      gint window_x, window_y;
      
      gdk_window_get_position (window, &window_x, &window_y);
      tx += window_x;
      ty += window_y;

      window = gdk_window_get_parent (window);
    }

  if (window)
    {
      *x = tx;
      *y = ty;

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_notebook_button_press (GtkWidget      *widget,
			   GdkEventButton *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkNotebookPage *page;
  GList *children;
  GtkArrowType arrow;
  gint num;
  gint x, y;

  if (event->type != GDK_BUTTON_PRESS || !notebook->children ||
      notebook->button)
    return FALSE;

  if (!get_widget_coordinates (widget, (GdkEvent *)event, &x, &y))
    return FALSE;

  arrow = gtk_notebook_get_arrow (notebook, x, y);
  if (arrow)
    return gtk_notebook_arrow_button_press (notebook, arrow, event);

  if (event->button == 3 && notebook->menu)
    {
      gtk_menu_popup (GTK_MENU (notebook->menu), NULL, NULL, 
		      NULL, NULL, 3, event->time);
      return TRUE;
    }

  if (event->button != 1)
    return FALSE;
  
  num = 0;
  children = notebook->children;
  while (children)
    {
      page = children->data;
      
      if (GTK_WIDGET_VISIBLE (page->child) &&
	  page->tab_label && GTK_WIDGET_MAPPED (page->tab_label) &&
	  (x >= page->allocation.x) &&
	  (y >= page->allocation.y) &&
	  (x <= (page->allocation.x + page->allocation.width)) &&
	  (y <= (page->allocation.y + page->allocation.height)))
	{
	  gboolean page_changed = page != notebook->cur_page;
	  gboolean was_focus = gtk_widget_is_focus (widget);
	  
	  gtk_notebook_switch_focus_tab (notebook, children);
	  gtk_widget_grab_focus (widget);
	  
	  if (page_changed && !was_focus)
	    gtk_widget_child_focus (page->child, GTK_DIR_TAB_FORWARD);
	  
	  break;
	}
      children = children->next;
      num++;
    }
  if (!children && !GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);
  
  return TRUE;
}

static gint
gtk_notebook_button_release (GtkWidget      *widget,
			     GdkEventButton *event)
{
  GtkNotebook *notebook;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  notebook = GTK_NOTEBOOK (widget);

  if (event->button == notebook->button)
    {
      guint click_child;

      if (notebook->timer)
	{
	  gtk_timeout_remove (notebook->timer);
	  notebook->timer = 0;
	  notebook->need_timer = FALSE;
	}
      click_child = notebook->click_child;
      notebook->click_child = 0;
      notebook->button = 0;
      gtk_notebook_redraw_arrows (notebook);

      return TRUE;
    }
  else
    return FALSE;
}

static gint
gtk_notebook_enter_notify (GtkWidget        *widget,
			   GdkEventCrossing *event)
{
  GtkNotebook *notebook;
  GtkArrowType arrow;
  gint x, y;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  notebook = GTK_NOTEBOOK (widget);

  if (!get_widget_coordinates (widget, (GdkEvent *)event, &x, &y))
    return FALSE;

  arrow = gtk_notebook_get_arrow (notebook, x, y);

  if (arrow != notebook->in_child)
    {
      notebook->in_child = arrow;
      gtk_notebook_redraw_arrows (notebook);

      return TRUE;
    }

  return TRUE;
}

static gint
gtk_notebook_leave_notify (GtkWidget        *widget,
			   GdkEventCrossing *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkArrowType arrow;
  gint x, y;

  if (!get_widget_coordinates (widget, (GdkEvent *)event, &x, &y))
    return FALSE;

  arrow = gtk_notebook_get_arrow (notebook, x, y);

  if (notebook->in_child)
    {
      notebook->in_child = 0;
      gtk_notebook_redraw_arrows (notebook);
    }

  return TRUE;
}

static gint
gtk_notebook_motion_notify (GtkWidget      *widget,
			    GdkEventMotion *event)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkArrowType arrow;
  gint x, y;
  
  if (notebook->button)
    return FALSE;

  if (!get_widget_coordinates (widget, (GdkEvent *)event, &x, &y))
    return FALSE;

  arrow = gtk_notebook_get_arrow (notebook, x, y);

  if (arrow != notebook->in_child)
    {
      notebook->in_child = arrow;
      gtk_notebook_redraw_arrows (notebook);
    }
  
  return TRUE;
}

static gint
gtk_notebook_focus_in (GtkWidget     *widget,
		       GdkEventFocus *event)
{
  GTK_NOTEBOOK (widget)->child_has_focus = FALSE;

  return (* GTK_WIDGET_CLASS (parent_class)->focus_in_event) (widget, event);
}

static void
gtk_notebook_draw_focus (GtkWidget *widget)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);

  if (GTK_WIDGET_DRAWABLE (widget) && notebook->show_tabs &&
      notebook->focus_tab)
    {
      GtkNotebookPage *page;
      GdkRectangle area;
      gint focus_width;
      
      gtk_widget_style_get (widget, "focus-line-width", &focus_width, NULL);

      page = notebook->focus_tab->data;

      area.x = page->tab_label->allocation.x - focus_width;
      area.y = page->tab_label->allocation.y - focus_width;
      area.width = page->tab_label->allocation.width + 2 * focus_width;
      area.height = page->tab_label->allocation.height + 2 * focus_width;

      gtk_notebook_draw_tab (GTK_NOTEBOOK (widget), page, &area);
    }
}

/* Private GtkContainer Methods :
 * 
 * gtk_notebook_set_child_arg
 * gtk_notebook_get_child_arg
 * gtk_notebook_add
 * gtk_notebook_remove
 * gtk_notebook_focus
 * gtk_notebook_set_focus_child
 * gtk_notebook_child_type
 * gtk_notebook_forall
 */
static void
gtk_notebook_set_child_property (GtkContainer    *container,
				 GtkWidget       *child,
				 guint            property_id,
				 const GValue    *value,
				 GParamSpec      *pspec)
{
  gboolean expand;
  gboolean fill;
  GtkPackType pack_type;

  /* not finding child's page is valid for menus or labels */
  if (!gtk_notebook_find_child (GTK_NOTEBOOK (container), child, NULL))
    return;

  switch (property_id)
    {
    case CHILD_PROP_TAB_LABEL:
      /* a NULL pointer indicates a default_tab setting, otherwise
       * we need to set the associated label
       */
      gtk_notebook_set_tab_label_text (GTK_NOTEBOOK (container), child,
				       g_value_get_string (value));
      break;
    case CHILD_PROP_MENU_LABEL:
      gtk_notebook_set_menu_label_text (GTK_NOTEBOOK (container), child,
					g_value_get_string (value));
      break;
    case CHILD_PROP_POSITION:
      gtk_notebook_reorder_child (GTK_NOTEBOOK (container), child,
				  g_value_get_int (value));
      break;
    case CHILD_PROP_TAB_EXPAND:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					    &expand, &fill, &pack_type);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
					  g_value_get_boolean (value),
					  fill, pack_type);
      break;
    case CHILD_PROP_TAB_FILL:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					    &expand, &fill, &pack_type);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
					  expand,
					  g_value_get_boolean (value),
					  pack_type);
      break;
    case CHILD_PROP_TAB_PACK:
      gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					    &expand, &fill, &pack_type);
      gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (container), child,
					  expand, fill,
					  g_value_get_enum (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_notebook_get_child_property (GtkContainer    *container,
				 GtkWidget       *child,
				 guint            property_id,
				 GValue          *value,
				 GParamSpec      *pspec)
{
  GList *list;
  GtkNotebook *notebook;
  GtkWidget *label;
  gboolean expand;
  gboolean fill;
  GtkPackType pack_type;

  notebook = GTK_NOTEBOOK (container);

  /* not finding child's page is valid for menus or labels */
  list = gtk_notebook_find_child (notebook, child, NULL);
  if (!list)
    {
      /* nothing to set on labels or menus */
      g_param_value_set_default (pspec, value);
      return;
    }

  switch (property_id)
    {
    case CHILD_PROP_TAB_LABEL:
      label = gtk_notebook_get_tab_label (notebook, child);

      if (label && GTK_IS_LABEL (label))
	g_value_set_string (value, GTK_LABEL (label)->label);
      else
	g_value_set_string (value, NULL);
      break;
    case CHILD_PROP_MENU_LABEL:
      label = gtk_notebook_get_menu_label (notebook, child);

      if (label && GTK_IS_LABEL (label))
	g_value_set_string (value, GTK_LABEL (label)->label);
      else
	g_value_set_string (value, NULL);
      break;
    case CHILD_PROP_POSITION:
      g_value_set_int (value, g_list_position (notebook->children, list));
      break;
    case CHILD_PROP_TAB_EXPAND:
	gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					      &expand, NULL, NULL);
	g_value_set_boolean (value, expand);
      break;
    case CHILD_PROP_TAB_FILL:
	gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					      NULL, &fill, NULL);
	g_value_set_boolean (value, fill);
      break;
    case CHILD_PROP_TAB_PACK:
	gtk_notebook_query_tab_label_packing (GTK_NOTEBOOK (container), child,
					      NULL, NULL, &pack_type);
	g_value_set_enum (value, pack_type);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_notebook_add (GtkContainer *container,
		  GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (container));

  gtk_notebook_insert_page_menu (GTK_NOTEBOOK (container), widget, 
				 NULL, NULL, -1);
}

static void
gtk_notebook_remove (GtkContainer *container,
		     GtkWidget    *widget)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;
  guint page_num;

  g_return_if_fail (GTK_IS_NOTEBOOK (container));
  g_return_if_fail (widget != NULL);

  notebook = GTK_NOTEBOOK (container);

  children = notebook->children;
  page_num = 0;
  while (children)
    {
      page = children->data;
      if (page->child == widget)
	{
	  gtk_notebook_real_remove (notebook, children, FALSE);
	  break;
	}
      page_num++;
      children = children->next;
    }
}

static gboolean
focus_tabs_in (GtkNotebook *notebook)
{
  if (notebook->show_tabs && notebook->cur_page)
    {
      gtk_widget_grab_focus (GTK_WIDGET (notebook));

      gtk_notebook_switch_focus_tab (notebook,
				     g_list_find (notebook->children,
						  notebook->cur_page));

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
focus_tabs_move (GtkNotebook     *notebook,
		 GtkDirectionType direction,
		 gint             search_direction)
{
  GList *new_page;

  new_page = gtk_notebook_search_page (notebook, notebook->focus_tab,
				       search_direction, TRUE);
  if (new_page)
    gtk_notebook_switch_focus_tab (notebook, new_page);
  else
    gdk_display_beep (gtk_widget_get_display (GTK_WIDGET (notebook)));
  
  return TRUE;
}

static gboolean
focus_child_in (GtkNotebook     *notebook,
		GtkDirectionType direction)
{
  if (notebook->cur_page)
    return gtk_widget_child_focus (notebook->cur_page->child, direction);
  else
    return FALSE;
}

/* Focus in the notebook can either be on the pages, or on
 * the tabs.
 */
static gint
gtk_notebook_focus (GtkWidget        *widget,
		    GtkDirectionType  direction)
{
  GtkWidget *old_focus_child;
  GtkNotebook *notebook;
  GtkDirectionType effective_direction;

  gboolean widget_is_focus;
  GtkContainer *container;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (widget), FALSE);

  container = GTK_CONTAINER (widget);
  notebook = GTK_NOTEBOOK (container);

  if (notebook->focus_out)
    {
      notebook->focus_out = FALSE; /* Clear this to catch the wrap-around case */
      return FALSE;
    }

  widget_is_focus = gtk_widget_is_focus (widget);
  old_focus_child = container->focus_child; 

  effective_direction = get_effective_direction (notebook, direction);

  if (old_focus_child)		/* Focus on page child */
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
        return TRUE;
      
      switch (effective_direction)
	{
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_UP:
	  /* Focus onto the tabs */
	  return focus_tabs_in (notebook);
	case GTK_DIR_DOWN:
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
	  return FALSE;
	}
    }
  else if (widget_is_focus)	/* Focus was on tabs */
    {
      switch (effective_direction)
	{
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_UP:
	  return FALSE;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
	  /* We use TAB_FORWARD rather than direction so that we focus a more
	   * predictable widget for the user; users may be using arrow focusing
	   * in this situation even if they don't usually use arrow focusing.
	   */
	  return focus_child_in (notebook, GTK_DIR_TAB_FORWARD);
	case GTK_DIR_LEFT:
	  return focus_tabs_move (notebook, direction, STEP_PREV);
	case GTK_DIR_RIGHT:
	  return focus_tabs_move (notebook, direction, STEP_NEXT);
	}
    }
  else /* Focus was not on widget */
    {
      switch (effective_direction)
	{
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
	  if (focus_tabs_in (notebook))
	    return TRUE;
	  if (focus_child_in (notebook, direction))
	    return TRUE;
	  return FALSE;
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_UP:
	  if (focus_child_in (notebook, direction))
	    return TRUE;
	  if (focus_tabs_in (notebook))
	    return TRUE;
	  return FALSE;
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
	  return focus_child_in (notebook, direction);
	}
    }

  g_assert_not_reached ();
  return FALSE;
}  

static void
gtk_notebook_set_focus_child (GtkContainer *container,
			      GtkWidget    *child)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (container);
  GtkWidget *page_child;
  GtkWidget *toplevel;

  /* If the old focus widget was within a page of the notebook,
   * (child may either be NULL or not in this case), record it
   * for future use if we switch to the page with a mnemonic.
   */

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));
  if (toplevel && GTK_WIDGET_TOPLEVEL (toplevel))
    {
      page_child = GTK_WINDOW (toplevel)->focus_widget; 
      while (page_child)
	{
	  if (page_child->parent == GTK_WIDGET (container))
	    {
	      GList *list = gtk_notebook_find_child (notebook, page_child, NULL);
	      if (list != NULL) 
		{
		  GtkNotebookPage *page = list->data;
	      
		  if (page->last_focus_child)
		    g_object_remove_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);
		  
		  page->last_focus_child = GTK_WINDOW (toplevel)->focus_widget;
		  g_object_add_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);
	      
		  break;
		}
	    }

	  page_child = page_child->parent;
	}
    }
  
  if (child)
    {
      g_return_if_fail (GTK_IS_WIDGET (child));

      notebook->child_has_focus = TRUE;
      if (!notebook->focus_tab)
	{
	  GList *children;
	  GtkNotebookPage *page;

	  children = notebook->children;
	  while (children)
	    {
	      page = children->data;
	      if (page->child == child || page->tab_label == child)
		gtk_notebook_switch_focus_tab (notebook, children);
	      children = children->next;
	    }
	}
    }

  parent_class->set_focus_child (container, child);
}

static void
gtk_notebook_forall (GtkContainer *container,
		     gboolean      include_internals,
		     GtkCallback   callback,
		     gpointer      callback_data)
{
  GtkNotebook *notebook;
  GList *children;

  g_return_if_fail (GTK_IS_NOTEBOOK (container));
  g_return_if_fail (callback != NULL);

  notebook = GTK_NOTEBOOK (container);

  children = notebook->children;
  while (children)
    {
      GtkNotebookPage *page;
      
      page = children->data;
      children = children->next;
      (* callback) (page->child, callback_data);
      if (include_internals)
	{
	  if (page->tab_label)
	    (* callback) (page->tab_label, callback_data);
	  if (page->menu_label)
	    (* callback) (page->menu_label, callback_data);
	}
    }
}

static GType
gtk_notebook_child_type (GtkContainer     *container)
{
  return GTK_TYPE_WIDGET;
}

/* Private GtkNotebook Functions:
 *
 * gtk_notebook_redraw_tabs
 * gtk_notebook_real_remove
 * gtk_notebook_update_labels
 * gtk_notebook_timer
 * gtk_notebook_page_compare
 * gtk_notebook_real_page_position
 * gtk_notebook_search_page
 */
static void
gtk_notebook_redraw_tabs (GtkNotebook *notebook)
{
  GtkWidget *widget;
  GtkNotebookPage *page;
  GdkRectangle redraw_rect;
  gint border;

  widget = GTK_WIDGET (notebook);
  border = GTK_CONTAINER (notebook)->border_width;

  if (!GTK_WIDGET_MAPPED (notebook) || !notebook->first_tab)
    return;

  page = notebook->first_tab->data;

  redraw_rect.x = border;
  redraw_rect.y = border;

  switch (notebook->tab_pos)
    {
    case GTK_POS_BOTTOM:
      redraw_rect.y = (widget->allocation.height - border -
		       page->allocation.height -
		       widget->style->ythickness);
      if (page != notebook->cur_page)
	redraw_rect.y -= widget->style->ythickness;
      /* fall through */
    case GTK_POS_TOP:
      redraw_rect.width = widget->allocation.width - 2 * border;
      redraw_rect.height = (page->allocation.height +
			    widget->style->ythickness);
      if (page != notebook->cur_page)
	redraw_rect.height += widget->style->ythickness;
      break;
    case GTK_POS_RIGHT:
      redraw_rect.x = (widget->allocation.width - border -
		       page->allocation.width -
		       widget->style->xthickness);
      if (page != notebook->cur_page)
	redraw_rect.x -= widget->style->xthickness;
      /* fall through */
    case GTK_POS_LEFT:
      redraw_rect.width = (page->allocation.width +
			   widget->style->xthickness);
      redraw_rect.height = widget->allocation.height - 2 * border;
      if (page != notebook->cur_page)
	redraw_rect.width += widget->style->xthickness;
      break;
    }

  redraw_rect.x += widget->allocation.x;
  redraw_rect.y += widget->allocation.y;

  gdk_window_invalidate_rect (widget->window, &redraw_rect, TRUE);
}

static void
gtk_notebook_redraw_arrows (GtkNotebook *notebook)
{
  if (GTK_WIDGET_MAPPED (notebook) && gtk_notebook_show_arrows (notebook))
    {
      GdkRectangle rect;

      gtk_notebook_get_arrow_rect (notebook, &rect);
      gdk_window_invalidate_rect (GTK_WIDGET (notebook)->window, &rect, FALSE);
    }
}

static gint
gtk_notebook_timer (GtkNotebook *notebook)
{
  gboolean retval = FALSE;

  GDK_THREADS_ENTER ();

  if (notebook->timer)
    {
      gtk_notebook_do_arrow (notebook, notebook->click_child);

      if (notebook->need_timer) 
	{
	  notebook->need_timer = FALSE;
	  notebook->timer = gtk_timeout_add (NOTEBOOK_SCROLL_DELAY,
					     (GtkFunction) gtk_notebook_timer, 
					     (gpointer) notebook);
	}
      else
	retval = TRUE;
    }

  GDK_THREADS_LEAVE ();

  return retval;
}

static gint
gtk_notebook_page_compare (gconstpointer a,
			   gconstpointer b)
{
  return (((GtkNotebookPage *) a)->child != b);
}

static GList*
gtk_notebook_find_child (GtkNotebook *notebook,
			 GtkWidget   *child,
			 const gchar *function)
{
  GList *list = g_list_find_custom (notebook->children, child,
				    gtk_notebook_page_compare);

#ifndef G_DISABLE_CHECKS
  if (!list && function)
    g_warning ("%s: unable to find child %p in notebook %p",
	       function, child, notebook);
#endif

  return list;
}

static void
gtk_notebook_remove_tab_label (GtkNotebook     *notebook,
			       GtkNotebookPage *page)
{
  if (page->tab_label)
    {
      if (page->mnemonic_activate_signal)
	g_signal_handler_disconnect (page->tab_label,
				     page->mnemonic_activate_signal);
      page->mnemonic_activate_signal = 0;

      gtk_widget_set_state (page->tab_label, GTK_STATE_NORMAL);
      gtk_widget_unparent (page->tab_label);
    }
}

static void
gtk_notebook_real_remove (GtkNotebook *notebook,
			  GList       *list,
			  gboolean     destroying)
{
  GtkNotebookPage *page;
  GList * next_list;
  gint need_resize = FALSE;

  next_list = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);
  if (!next_list)
    next_list = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);

  if (notebook->cur_page == list->data)
    { 
      notebook->cur_page = NULL;
      if (next_list && !destroying)
	gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (next_list), -1);
    }

  if (list == notebook->first_tab)
    notebook->first_tab = next_list;
  if (list == notebook->focus_tab && !destroying)
    gtk_notebook_switch_focus_tab (notebook, next_list);

  page = list->data;
  
  if (GTK_WIDGET_VISIBLE (page->child) && GTK_WIDGET_VISIBLE (notebook))
    need_resize = TRUE;

  gtk_widget_unparent (page->child);

  gtk_notebook_remove_tab_label (notebook, page);
  
  if (notebook->menu)
    {
      gtk_container_remove (GTK_CONTAINER (notebook->menu), 
			    page->menu_label->parent);
      gtk_widget_queue_resize (notebook->menu);
    }
  if (!page->default_menu)
    g_object_unref (page->menu_label);
  
  notebook->children = g_list_remove_link (notebook->children, list);
  g_list_free (list);

  if (page->last_focus_child)
    {
      g_object_remove_weak_pointer (G_OBJECT (page->last_focus_child), (gpointer *)&page->last_focus_child);
      page->last_focus_child = NULL;
    }
  
  g_free (page);

  gtk_notebook_update_labels (notebook);
  if (need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

static void
gtk_notebook_update_labels (GtkNotebook *notebook)
{
  GtkNotebookPage *page;
  GList *list;
  gchar string[32];
  gint page_num = 1;

  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    {
      page = list->data;
      g_snprintf (string, sizeof(string), _("Page %u"), page_num++);
      if (notebook->show_tabs)
	{
	  if (page->default_tab)
	    {
	      if (!page->tab_label)
		{
		  page->tab_label = gtk_label_new (string);
		  gtk_widget_set_parent (page->tab_label,
					 GTK_WIDGET (notebook));
		}
	      else
		gtk_label_set_text (GTK_LABEL (page->tab_label), string);
	    }

	  if (GTK_WIDGET_VISIBLE (page->child) &&
	      !GTK_WIDGET_VISIBLE (page->tab_label))
	    gtk_widget_show (page->tab_label);
	  else if (!GTK_WIDGET_VISIBLE (page->child) &&
		   GTK_WIDGET_VISIBLE (page->tab_label))
	    gtk_widget_hide (page->tab_label);
	}
      if (notebook->menu && page->default_menu)
	{
	  if (page->tab_label && GTK_IS_LABEL (page->tab_label))
	    gtk_label_set_text (GTK_LABEL (page->menu_label),
			   GTK_LABEL (page->tab_label)->label);
	  else
	    gtk_label_set_text (GTK_LABEL (page->menu_label), string);
	}
    }  
}

static gint
gtk_notebook_real_page_position (GtkNotebook *notebook,
				 GList       *list)
{
  GList *work;
  gint count_start;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);
  g_return_val_if_fail (list != NULL, -1);

  for (work = notebook->children, count_start = 0;
       work && work != list; work = work->next)
    if (GTK_NOTEBOOK_PAGE (work)->pack == GTK_PACK_START)
      count_start++;

  if (!work)
    return -1;

  if (GTK_NOTEBOOK_PAGE (list)->pack == GTK_PACK_START)
    return count_start;

  return (count_start + g_list_length (list) - 1);
}

static GList *
gtk_notebook_search_page (GtkNotebook *notebook,
			  GList       *list,
			  gint         direction,
			  gboolean     find_visible)
{
  GtkNotebookPage *page = NULL;
  GList *old_list = NULL;
  gint flag = 0;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  switch (direction)
    {
    case STEP_PREV:
      flag = GTK_PACK_END;
      break;

    case STEP_NEXT:
      flag = GTK_PACK_START;
      break;
    }

  if (list)
    page = list->data;

  if (!page || page->pack == flag)
    {
      if (list)
	{
	  old_list = list;
	  list = list->next;
	}
      else
	list = notebook->children;

      while (list)
	{
	  page = list->data;
	  if (page->pack == flag &&
	      (!find_visible || GTK_WIDGET_VISIBLE (page->child)))
	    return list;
	  old_list = list;
	  list = list->next;
	}
      list = old_list;
    }
  else
    {
      old_list = list;
      list = list->prev;
    }
  while (list)
    {
      page = list->data;
      if (page->pack != flag &&
	  (!find_visible || GTK_WIDGET_VISIBLE (page->child)))
	return list;
      old_list = list;
      list = list->prev;
    }
  return NULL;
}

/* Private GtkNotebook Drawing Functions:
 *
 * gtk_notebook_paint
 * gtk_notebook_draw_tab
 * gtk_notebook_draw_arrow
 */
static void
gtk_notebook_paint (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkNotebook *notebook;
  GtkNotebookPage *page;
  GList *children;
  gboolean showarrow;
  gint width, height;
  gint x, y;
  gint border_width = GTK_CONTAINER (widget)->border_width;
  gint gap_x = 0, gap_width = 0;
   
  g_return_if_fail (GTK_IS_NOTEBOOK (widget));
  g_return_if_fail (area != NULL);

  if (!GTK_WIDGET_DRAWABLE (widget))
    return;

  notebook = GTK_NOTEBOOK (widget);

  if ((!notebook->show_tabs && !notebook->show_border) ||
      !notebook->cur_page || !GTK_WIDGET_VISIBLE (notebook->cur_page->child))
    return;

  x = widget->allocation.x + border_width;
  y = widget->allocation.y + border_width;
  width = widget->allocation.width - border_width * 2;
  height = widget->allocation.height - border_width * 2;

  if (notebook->show_border && (!notebook->show_tabs || !notebook->children))
    {
      gtk_paint_box (widget->style, widget->window,
		     GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		     area, widget, "notebook",
		     x, y, width, height);
      return;
    }


  if (!GTK_WIDGET_MAPPED (notebook->cur_page->tab_label))
    {
      page = notebook->first_tab->data;

      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  y += page->allocation.height + widget->style->ythickness;
	case GTK_POS_BOTTOM:
	  height -= page->allocation.height + widget->style->ythickness;
	  break;
	case GTK_POS_LEFT:
	  x += page->allocation.width + widget->style->xthickness;
	case GTK_POS_RIGHT:
	  width -= page->allocation.width + widget->style->xthickness;
	  break;
	}
      gtk_paint_box (widget->style, widget->window,
		     GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		     area, widget, "notebook",
		     x, y, width, height);
    }
  else
    {
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  y += notebook->cur_page->allocation.height;
	case GTK_POS_BOTTOM:
	  height -= notebook->cur_page->allocation.height;
	  break;
	case GTK_POS_LEFT:
	  x += notebook->cur_page->allocation.width;
	case GTK_POS_RIGHT:
	  width -= notebook->cur_page->allocation.width;
	  break;
	}

      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  gap_x = (notebook->cur_page->allocation.x - widget->allocation.x - border_width);
	  gap_width = notebook->cur_page->allocation.width;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  gap_x = (notebook->cur_page->allocation.y - widget->allocation.y - border_width);
	  gap_width = notebook->cur_page->allocation.height;
	  break;
	}
      gtk_paint_box_gap(widget->style, widget->window,
			GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			area, widget, "notebook",
			x, y, width, height,
			notebook->tab_pos, gap_x, gap_width);
    }

  showarrow = FALSE;
  children = gtk_notebook_search_page (notebook, NULL, STEP_PREV, TRUE);
  while (children)
    {
      page = children->data;
      children = gtk_notebook_search_page (notebook, children,
					   STEP_PREV, TRUE);
      if (!GTK_WIDGET_VISIBLE (page->child))
	continue;
      if (!GTK_WIDGET_MAPPED (page->tab_label))
	showarrow = TRUE;
      else if (page != notebook->cur_page)
	gtk_notebook_draw_tab (notebook, page, area);
    }

  if (showarrow && notebook->scrollable) 
    {
      gtk_notebook_draw_arrow (notebook, GTK_ARROW_LEFT);
      gtk_notebook_draw_arrow (notebook, GTK_ARROW_RIGHT);
    }
  gtk_notebook_draw_tab (notebook, notebook->cur_page, area);
}

static void
gtk_notebook_draw_tab (GtkNotebook     *notebook,
		       GtkNotebookPage *page,
		       GdkRectangle    *area)
{
  GdkRectangle child_area;
  GdkRectangle page_area;
  GtkStateType state_type;
  GtkPositionType gap_side;
  
  g_return_if_fail (notebook != NULL);
  g_return_if_fail (page != NULL);
  g_return_if_fail (area != NULL);

  if (!GTK_WIDGET_MAPPED (page->tab_label) ||
      (page->allocation.width == 0) || (page->allocation.height == 0))
    return;

  page_area.x = page->allocation.x;
  page_area.y = page->allocation.y;
  page_area.width = page->allocation.width;
  page_area.height = page->allocation.height;

  if (gdk_rectangle_intersect (&page_area, area, &child_area))
    {
      GtkWidget *widget;

      widget = GTK_WIDGET (notebook);
      gap_side = 0;
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  gap_side = GTK_POS_BOTTOM;
	  break;
	case GTK_POS_BOTTOM:
	  gap_side = GTK_POS_TOP;
	  break;
	case GTK_POS_LEFT:
	  gap_side = GTK_POS_RIGHT;
	  break;
	case GTK_POS_RIGHT:
	  gap_side = GTK_POS_LEFT;
	  break;
	}
      
      if (notebook->cur_page == page)
	state_type = GTK_STATE_NORMAL;
      else 
	state_type = GTK_STATE_ACTIVE;
      gtk_paint_extension(widget->style, widget->window,
			  state_type, GTK_SHADOW_OUT,
			  area, widget, "tab",
			  page_area.x, page_area.y,
			  page_area.width, page_area.height,
			  gap_side);
      if ((GTK_WIDGET_HAS_FOCUS (widget)) &&
	  notebook->focus_tab && (notebook->focus_tab->data == page))
	{
          gint focus_width;
	  
	  gtk_widget_style_get (widget, "focus-line-width", &focus_width, NULL);
	  
	  gtk_paint_focus (widget->style, widget->window, GTK_WIDGET_STATE (widget),
			   area, widget, "tab",
			   page->tab_label->allocation.x - focus_width,
			   page->tab_label->allocation.y - focus_width,
			   page->tab_label->allocation.width + 2 * focus_width,
			   page->tab_label->allocation.height + 2 * focus_width);
	}
      if (gtk_widget_intersect (page->tab_label, area, &child_area) &&
          GTK_WIDGET_DRAWABLE (page->tab_label))
        {
          GdkEvent *expose_event = gdk_event_new (GDK_EXPOSE);

          /* This is a lame hack since all this code needs rewriting anyhow */
          
          expose_event->expose.window = g_object_ref (page->tab_label->window);
          expose_event->expose.area = child_area;
          expose_event->expose.region = gdk_region_rectangle (&child_area);
          expose_event->expose.send_event = TRUE;
          expose_event->expose.count = 0;

	  gtk_container_propagate_expose (GTK_CONTAINER (notebook), page->tab_label, (GdkEventExpose *)expose_event);

	  gdk_event_free (expose_event);
        }
    }
}

static void
gtk_notebook_draw_arrow (GtkNotebook *notebook,
			 guint        arrow)
{
  GtkStateType state_type;
  GtkShadowType shadow_type;
  GtkWidget *widget;
  GdkRectangle arrow_rect;

  gtk_notebook_get_arrow_rect (notebook, &arrow_rect);

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  widget = GTK_WIDGET(notebook);

  if (GTK_WIDGET_DRAWABLE (notebook))
    {
      if (notebook->in_child == arrow)
        {
          if (notebook->click_child == arrow)
            state_type = GTK_STATE_ACTIVE;
          else
            state_type = GTK_STATE_PRELIGHT;
        }
      else
        state_type = GTK_STATE_NORMAL;

      if (notebook->click_child == arrow)
        shadow_type = GTK_SHADOW_IN;
      else
        shadow_type = GTK_SHADOW_OUT;

      if (arrow == GTK_ARROW_LEFT)
	{
	  if (notebook->focus_tab &&
	      !gtk_notebook_search_page (notebook, notebook->focus_tab,
					 STEP_PREV, TRUE))
	    {
	      shadow_type = GTK_SHADOW_ETCHED_IN;
	      state_type = GTK_STATE_INSENSITIVE;
	    }

	  if (notebook->tab_pos == GTK_POS_LEFT ||
	      notebook->tab_pos == GTK_POS_RIGHT)
	    arrow = GTK_ARROW_UP;

	  gtk_paint_arrow (widget->style, widget->window, state_type, 
			   shadow_type, NULL, GTK_WIDGET(notebook), "notebook",
			   arrow, TRUE, 
			   arrow_rect.x, arrow_rect.y, ARROW_SIZE, ARROW_SIZE);
	}
      else
	{
	  if (notebook->focus_tab &&
	      !gtk_notebook_search_page (notebook, notebook->focus_tab,
					 STEP_NEXT, TRUE))
	    {
	      shadow_type = GTK_SHADOW_ETCHED_IN;
	      state_type = GTK_STATE_INSENSITIVE;
	    }

	  if (notebook->tab_pos == GTK_POS_LEFT ||
	      notebook->tab_pos == GTK_POS_RIGHT)
	    arrow = GTK_ARROW_DOWN;

	   gtk_paint_arrow (widget->style, widget->window, state_type, 
			    shadow_type, NULL, GTK_WIDGET(notebook), "notebook",
			    arrow, TRUE, arrow_rect.x + ARROW_SIZE + ARROW_SPACING,
			    arrow_rect.y, ARROW_SIZE, ARROW_SIZE);
	}
    }
}

/* Private GtkNotebook Size Allocate Functions:
 *
 * gtk_notebook_pages_allocate
 * gtk_notebook_page_allocate
 * gtk_notebook_calc_tabs
 */
static void
gtk_notebook_pages_allocate (GtkNotebook   *notebook)
{
  GtkWidget    *widget = GTK_WIDGET (notebook);
  GtkContainer *container = GTK_CONTAINER (notebook);
  GtkNotebookPage *page = NULL;
  GtkAllocation *allocation = &widget->allocation;
  GtkAllocation child_allocation;
  GList *children = NULL;
  GList *last_child = NULL;
  gboolean showarrow = FALSE;
  gint tab_space = 0; 
  gint delta; 
  gint x = 0;
  gint y = 0;
  gint i;
  gint n = 1;
  gint old_fill = 0;
  gint new_fill = 0;

  if (!notebook->show_tabs || !notebook->children || !notebook->cur_page)
    return;

  child_allocation.x = widget->allocation.x + container->border_width;
  child_allocation.y = widget->allocation.y + container->border_width;

  switch (notebook->tab_pos)
    {
    case GTK_POS_BOTTOM:
      child_allocation.y = (widget->allocation.y +
			    allocation->height -
			    notebook->cur_page->requisition.height -
			    container->border_width);
      /* fall through */
    case GTK_POS_TOP:
      child_allocation.height = notebook->cur_page->requisition.height;
      break;
      
    case GTK_POS_RIGHT:
      child_allocation.x = (widget->allocation.x +
			    allocation->width -
			    notebook->cur_page->requisition.width -
			    container->border_width);
      /* fall through */
    case GTK_POS_LEFT:
      child_allocation.width = notebook->cur_page->requisition.width;
      break;
    }
  
  if (notebook->scrollable) 
    {
      GList *focus_tab;
      
      children = notebook->children;
      
      if (notebook->focus_tab)
	focus_tab = notebook->focus_tab;
      else if (notebook->first_tab)
	focus_tab = notebook->first_tab;
      else
	focus_tab = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, TRUE);

      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  while (children)
	    {
	      page = children->data;
	      children = children->next;

	      if (GTK_WIDGET_VISIBLE (page->child))
		tab_space += page->requisition.width;
	    }
	  if (tab_space >
	      allocation->width - 2 * container->border_width - TAB_OVERLAP) 
	    {
	      showarrow = TRUE;
	      page = focus_tab->data; 

	      tab_space = (allocation->width - TAB_OVERLAP -
			   page->requisition.width -
			   2 * (container->border_width + ARROW_SPACING +
				ARROW_SIZE));
	      x = (allocation->width - 2 * ARROW_SIZE - ARROW_SPACING -
		   container->border_width);

	      page = notebook->children->data;
	      if (notebook->tab_pos == GTK_POS_TOP)
		y = (container->border_width +
		     (page->requisition.height - ARROW_SIZE) / 2);
	      else
		y = (allocation->height - container->border_width - 
		     ARROW_SIZE - (page->requisition.height - ARROW_SIZE) / 2);
	    }
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  while (children)
	    {
	      page = children->data;
	      children = children->next;

	      if (GTK_WIDGET_VISIBLE (page->child))
		tab_space += page->requisition.height;
	    }
	  if (tab_space >
	      (allocation->height - 2 * container->border_width - TAB_OVERLAP))
	    {
	      showarrow = TRUE;
	      page = focus_tab->data; 
	      tab_space = (allocation->height -
			   ARROW_SIZE - ARROW_SPACING - TAB_OVERLAP -
			   2 * container->border_width -
			   page->requisition.height);
	      y = allocation->height - container->border_width - ARROW_SIZE;  

	      page = notebook->children->data;
	      if (notebook->tab_pos == GTK_POS_LEFT)
		x = (container->border_width +
		     (page->requisition.width -
		      (2 * ARROW_SIZE - ARROW_SPACING)) / 2); 
	      else
		x = (allocation->width - container->border_width -
		     (2 * ARROW_SIZE - ARROW_SPACING) -
		     (page->requisition.width -
		      (2 * ARROW_SIZE - ARROW_SPACING)) / 2);
	    }
	  break;
	}
      if (showarrow) /* first_tab <- focus_tab */
	{ 
	  if (tab_space <= 0)
	    {
	      notebook->first_tab = focus_tab;
	      last_child = gtk_notebook_search_page (notebook, focus_tab,
						     STEP_NEXT, TRUE);
	    }
	  else
	    {
	      children = NULL;
	      if (notebook->first_tab && notebook->first_tab != focus_tab)
		{
		  /* Is first_tab really predecessor of focus_tab  ? */
		  page = notebook->first_tab->data;
		  if (GTK_WIDGET_VISIBLE (page->child))
		    for (children = focus_tab;
			 children && children != notebook->first_tab;
			 children = gtk_notebook_search_page (notebook,
							      children,
							      STEP_PREV,
							      TRUE));
		}
	      if (!children)
		notebook->first_tab = focus_tab;
	      else
		gtk_notebook_calc_tabs (notebook,
					gtk_notebook_search_page (notebook,
								  focus_tab,
								  STEP_PREV,
								  TRUE), 
					&(notebook->first_tab), &tab_space,
					STEP_PREV);

	      if (tab_space <= 0)
		{
		  notebook->first_tab =
		    gtk_notebook_search_page (notebook, notebook->first_tab,
					      STEP_NEXT, TRUE);
		  if (!notebook->first_tab)
		    notebook->first_tab = focus_tab;
		  last_child = gtk_notebook_search_page (notebook, focus_tab,
							 STEP_NEXT, TRUE); 
		}
	      else /* focus_tab -> end */   
		{
		  if (!notebook->first_tab)
		    notebook->first_tab = gtk_notebook_search_page (notebook,
								    NULL,
								    STEP_NEXT,
								    TRUE);
		  children = NULL;
		  gtk_notebook_calc_tabs (notebook,
					  gtk_notebook_search_page (notebook,
								    focus_tab,
								    STEP_NEXT,
								    TRUE),
					  &children, &tab_space, STEP_NEXT);

		  if (tab_space <= 0) 
		    last_child = children;
		  else /* start <- first_tab */
		    {
		      last_child = NULL;
		      children = NULL;
		      gtk_notebook_calc_tabs
			(notebook,
			 gtk_notebook_search_page (notebook,
						   notebook->first_tab,
						   STEP_PREV,
						   TRUE),
			 &children, &tab_space, STEP_PREV);
		      notebook->first_tab = gtk_notebook_search_page(notebook,
								     children,
								     STEP_NEXT,
								     TRUE);
		    }
		}
	    }

	  if (tab_space < 0) 
	    {
	      tab_space = -tab_space;
	      n = 0;
	      for (children = notebook->first_tab;
		   children && children != last_child;
		   children = gtk_notebook_search_page (notebook, children,
							STEP_NEXT, TRUE))
		n++;
	    }
	  else 
	    tab_space = 0;

	  /*unmap all non-visible tabs*/
	  for (children = gtk_notebook_search_page (notebook, NULL,
						    STEP_NEXT, TRUE);
	       children && children != notebook->first_tab;
	       children = gtk_notebook_search_page (notebook, children,
						    STEP_NEXT, TRUE))
	    {
	      page = children->data;
	      if (page->tab_label)
		gtk_widget_set_child_visible (page->tab_label, FALSE);
	    }
	  for (children = last_child; children;
	       children = gtk_notebook_search_page (notebook, children,
						    STEP_NEXT, TRUE))
	    {
	      page = children->data;
	      if (page->tab_label)
		gtk_widget_set_child_visible (page->tab_label, FALSE);
	    }
	}
      else /* !showarrow */
	{
	  notebook->first_tab = gtk_notebook_search_page (notebook, NULL,
							  STEP_NEXT, TRUE);
	  tab_space = 0;
	}
    }

  if (!showarrow)
    {
      gint c = 0;

      n = 0;
      children = notebook->children;
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  while (children)
	    {
	      page = children->data;
	      children = children->next;

	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  c++;
		  tab_space += page->requisition.width;
		  if (page->expand)
		    n++;
		}
	    }
	  tab_space -= allocation->width;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  while (children)
	    {
	      page = children->data;
	      children = children->next;

	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  c++;
		  tab_space += page->requisition.height;
		  if (page->expand)
		    n++;
		}
	    }
	  tab_space -= allocation->height;
	}
      tab_space += 2 * container->border_width + TAB_OVERLAP;
      tab_space *= -1;
      notebook->first_tab = gtk_notebook_search_page (notebook, NULL,
						      STEP_NEXT, TRUE);
      if (notebook->homogeneous && n)
	n = c;
    }
  
  children = notebook->first_tab;
  i = 1;
  while (children)
    {
      if (children == last_child)
	{
	  /* FIXME double check */
	  goto done;
	}

      page = children->data;
      if (!showarrow && page->pack != GTK_PACK_START)
	break;
      children = gtk_notebook_search_page (notebook, children, STEP_NEXT,TRUE);
      
      delta = 0;
      if (n && (showarrow || page->expand || notebook->homogeneous))
	{
	  new_fill = (tab_space * i++) / n;
	  delta = new_fill - old_fill;
	  old_fill = new_fill;
	}
      
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  child_allocation.width = (page->requisition.width +
				    TAB_OVERLAP + delta);
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  child_allocation.height = (page->requisition.height +
				     TAB_OVERLAP + delta);
	  break;
	}

      gtk_notebook_page_allocate (notebook, page, &child_allocation);
	  
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  child_allocation.x += child_allocation.width - TAB_OVERLAP;
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  child_allocation.y += child_allocation.height - TAB_OVERLAP;
	  break;
	}

      if (page->tab_label)
	gtk_widget_set_child_visible (page->tab_label, TRUE);
    }

  if (children)
    {
      children = notebook->children;
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  child_allocation.x = (allocation->x + allocation->width -
				container->border_width);
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  child_allocation.y = (allocation->y + allocation->height -
				container->border_width);
	  break;
	}

      while (children != last_child)
	{
	  page = children->data;
	  children = children->next;

	  if (page->pack != GTK_PACK_END || !GTK_WIDGET_VISIBLE (page->child))
	    continue;

	  delta = 0;
	  if (n && (page->expand || notebook->homogeneous))
	    {
	      new_fill = (tab_space * i++) / n;
	      delta = new_fill - old_fill;
	      old_fill = new_fill;
	    }

	  switch (notebook->tab_pos)
	    {
	    case GTK_POS_TOP:
	    case GTK_POS_BOTTOM:
	      child_allocation.width = (page->requisition.width +
					TAB_OVERLAP + delta);
	      child_allocation.x -= child_allocation.width;
	      break;
	    case GTK_POS_LEFT:
	    case GTK_POS_RIGHT:
	      child_allocation.height = (page->requisition.height +
					 TAB_OVERLAP + delta);
	      child_allocation.y -= child_allocation.height;
	      break;
	    }

	  gtk_notebook_page_allocate (notebook, page, &child_allocation);

	  switch (notebook->tab_pos)
	    {
	    case GTK_POS_TOP:
	    case GTK_POS_BOTTOM:
	      child_allocation.x += TAB_OVERLAP;
	      break;
	    case GTK_POS_LEFT:
	    case GTK_POS_RIGHT:
	      child_allocation.y += TAB_OVERLAP;
	      break;
	    }

	  if (page->tab_label)
	    gtk_widget_set_child_visible (page->tab_label, TRUE);
	}
    }

 done:
  gtk_notebook_redraw_tabs (notebook);  
}

static void
gtk_notebook_page_allocate (GtkNotebook     *notebook,
			    GtkNotebookPage *page,
			    GtkAllocation   *allocation)
{
  GtkWidget *widget = GTK_WIDGET (notebook);
  GtkAllocation child_allocation;
  GtkRequisition tab_requisition;
  gint xthickness;
  gint ythickness;
  gint padding;
  gint focus_width;

  gtk_widget_style_get (widget, "focus-line-width", &focus_width, NULL);
  
  xthickness = widget->style->xthickness;
  ythickness = widget->style->ythickness;

  /* If the size of the notebook tabs change, we need to queue
   * a redraw on the tab area
   */
  if ((allocation->width != page->allocation.width) ||
      (allocation->height != page->allocation.height))
    {
      gint x, y, width, height, border_width;

      border_width = GTK_CONTAINER (notebook)->border_width;

      switch (notebook->tab_pos)
       {
       case GTK_POS_TOP:
         width = widget->allocation.width;
         height = MAX (page->allocation.height, allocation->height) + ythickness;
         x = 0;                              
         y = border_width;
         break;

       case GTK_POS_BOTTOM:
         width = widget->allocation.width + xthickness;
         height = MAX (page->allocation.height, allocation->height) + ythickness;
         x = 0;                              
         y = widget->allocation.height - height - border_width;
         break;

       case GTK_POS_LEFT:
         width = MAX (page->allocation.width, allocation->width) + xthickness;
         height = widget->allocation.height;
         x = border_width;
         y = 0;
         break;

       case GTK_POS_RIGHT:
       default:                /* quiet gcc */
         width = MAX (page->allocation.width, allocation->width) + xthickness;
         height = widget->allocation.height;
         x = widget->allocation.width - width - border_width;
         y = 0;
         break;
       }

      gtk_widget_queue_draw_area (widget, x, y, width, height);
    }

  page->allocation = *allocation;
  gtk_widget_get_child_requisition (page->tab_label, &tab_requisition);

  if (notebook->cur_page != page)
    {
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  page->allocation.y += ythickness;
	case GTK_POS_BOTTOM:
	  if (page->allocation.height > ythickness)
	    page->allocation.height -= ythickness;
	  break;
	case GTK_POS_LEFT:
	  page->allocation.x += xthickness;
	case GTK_POS_RIGHT:
	  if (page->allocation.width > xthickness)
	    page->allocation.width -= xthickness;
	  break;
	}
    }

  switch (notebook->tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      padding = TAB_CURVATURE + focus_width + notebook->tab_hborder;
      if (page->fill)
	{
	  child_allocation.x = (xthickness + focus_width +
				notebook->tab_hborder);
	  child_allocation.width = MAX (1, (page->allocation.width -
					    2 * child_allocation.x));
	  child_allocation.x += page->allocation.x;
	}
      else
	{
	  child_allocation.x = (page->allocation.x +
				(page->allocation.width -
				 tab_requisition.width) / 2);
	  child_allocation.width = tab_requisition.width;
	}
      child_allocation.y = (notebook->tab_vborder + focus_width +
			    page->allocation.y);
      if (notebook->tab_pos == GTK_POS_TOP)
	child_allocation.y += ythickness;
      child_allocation.height = MAX (1, (((gint) page->allocation.height) - ythickness -
					 2 * (notebook->tab_vborder + focus_width)));
      break;
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      padding = TAB_CURVATURE + focus_width + notebook->tab_vborder;
      if (page->fill)
	{
	  child_allocation.y = ythickness + padding;
	  child_allocation.height = MAX (1, (page->allocation.height -
					     2 * child_allocation.y));
	  child_allocation.y += page->allocation.y;
	}
      else
	{
	  child_allocation.y = (page->allocation.y + (page->allocation.height -
						      tab_requisition.height) / 2);
	  child_allocation.height = tab_requisition.height;
	}
      child_allocation.x = page->allocation.x + notebook->tab_hborder + focus_width;
      if (notebook->tab_pos == GTK_POS_LEFT)
	child_allocation.x += xthickness;
      child_allocation.width = MAX (1, (((gint) page->allocation.width) - xthickness -
					2 * (notebook->tab_hborder + focus_width)));
      break;
    }

  if (page->tab_label)
    gtk_widget_size_allocate (page->tab_label, &child_allocation);
}

static void 
gtk_notebook_calc_tabs (GtkNotebook  *notebook, 
			GList        *start, 
                        GList       **end,
			gint         *tab_space,
                        guint         direction)
{
  GtkNotebookPage *page = NULL;
  GList *children;
  GList *last_list = NULL;
  gboolean pack;

  if (!start)
    return;

  children = start;
  pack = GTK_NOTEBOOK_PAGE (start)->pack;
  if (pack == GTK_PACK_END)
    direction = (direction == STEP_PREV) ? STEP_NEXT : STEP_PREV;

  while (1)
    {
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	case GTK_POS_BOTTOM:
	  while (children)
	    {
	      page = children->data;
	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  if (page->pack == pack)
		    {
		      *tab_space -= page->requisition.width;
		      if (*tab_space < 0 || children == *end)
			{
			  if (*tab_space < 0) 
			    {
			      *tab_space = - (*tab_space +
					      page->requisition.width);
			      *end = children;
			    }
			  return;
			}
		    }
		  last_list = children;
		}
	      if (direction == STEP_NEXT)
		children = children->next;
	      else
		children = children->prev;
	    }
	  break;
	case GTK_POS_LEFT:
	case GTK_POS_RIGHT:
	  while (children)
	    {
	      page = children->data;
	      if (GTK_WIDGET_VISIBLE (page->child))
		{
		  if (page->pack == pack)
		    {
		      *tab_space -= page->requisition.height;
		      if (*tab_space < 0 || children == *end)
			{
			  if (*tab_space < 0)
			    {
			      *tab_space = - (*tab_space +
					      page->requisition.height);
			      *end = children;
			    }
			  return;
			}
		    }
		  last_list = children;
		}
	      if (direction == STEP_NEXT)
		children = children->next;
	      else
		children = children->prev;
	    }
	  break;
	}
      if (direction == STEP_PREV)
	return;
      pack = (pack == GTK_PACK_END) ? GTK_PACK_START : GTK_PACK_END;
      direction = STEP_PREV;
      children = last_list;
    }
}

static void
gtk_notebook_update_tab_states (GtkNotebook *notebook)
{
  GList *list;

  for (list = notebook->children; list != NULL; list = list->next)
    {
      GtkNotebookPage *page = list->data;
      
      if (page->tab_label)
	{
	  if (page == notebook->cur_page)
	    gtk_widget_set_state (page->tab_label, GTK_STATE_NORMAL);
	  else
	    gtk_widget_set_state (page->tab_label, GTK_STATE_ACTIVE);
	}
    }
}

/* Private GtkNotebook Page Switch Methods:
 *
 * gtk_notebook_real_switch_page
 */
static void
gtk_notebook_real_switch_page (GtkNotebook     *notebook,
			       GtkNotebookPage *page,
			       guint            page_num)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (page != NULL);

  if (notebook->cur_page == page || !GTK_WIDGET_VISIBLE (page->child))
    return;

  if (notebook->cur_page)
    gtk_widget_set_child_visible (notebook->cur_page->child, FALSE);
  
  notebook->cur_page = page;

  if (!notebook->focus_tab ||
      notebook->focus_tab->data != (gpointer) notebook->cur_page)
    notebook->focus_tab = 
      g_list_find (notebook->children, notebook->cur_page);

  gtk_widget_set_child_visible (notebook->cur_page->child, TRUE);

  /* If the focus was on the previous page, move it to the first
   * element on the new page, if possible, or if not, to the
   * notebook itself.
   */
  if (notebook->child_has_focus)
    {
      if (notebook->cur_page->last_focus_child &&
	  gtk_widget_is_ancestor (notebook->cur_page->last_focus_child, notebook->cur_page->child))
	gtk_widget_grab_focus (notebook->cur_page->last_focus_child);
      else
	if (!gtk_widget_child_focus (notebook->cur_page->child, GTK_DIR_TAB_FORWARD))
	  gtk_widget_grab_focus (GTK_WIDGET (notebook));
    }
  
  gtk_notebook_update_tab_states (notebook);
  gtk_widget_queue_resize (GTK_WIDGET (notebook));
  g_object_notify (G_OBJECT (notebook), "page");
}

/* Private GtkNotebook Page Switch Functions:
 *
 * gtk_notebook_switch_page
 * gtk_notebook_page_select
 * gtk_notebook_switch_focus_tab
 * gtk_notebook_menu_switch_page
 */
static void
gtk_notebook_switch_page (GtkNotebook     *notebook,
			  GtkNotebookPage *page,
			  gint             page_num)
{ 
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (page != NULL);
 
  if (notebook->cur_page == page)
    return;

  if (page_num < 0)
    page_num = g_list_index (notebook->children, page);

  g_signal_emit (notebook,
		 notebook_signals[SWITCH_PAGE],
		 0,
		 page,
		 page_num);
}

static gint
gtk_notebook_page_select (GtkNotebook *notebook,
			  gboolean     move_focus)
{
  GtkNotebookPage *page;
  GtkDirectionType dir = GTK_DIR_DOWN; /* Quiet GCC */

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  if (!notebook->focus_tab)
    return FALSE;

  page = notebook->focus_tab->data;
  gtk_notebook_switch_page (notebook, page, -1);

  if (move_focus)
    {
      switch (notebook->tab_pos)
	{
	case GTK_POS_TOP:
	  dir = GTK_DIR_DOWN;
	  break;
	case GTK_POS_BOTTOM:
	  dir = GTK_DIR_UP;
	  break;
	case GTK_POS_LEFT:
	  dir = GTK_DIR_RIGHT;
	  break;
	case GTK_POS_RIGHT:
	  dir = GTK_DIR_LEFT;
	  break;
	}

      if (gtk_widget_child_focus (page->child, dir))
        return TRUE;
    }
  return FALSE;
}

static void
gtk_notebook_switch_focus_tab (GtkNotebook *notebook, 
			       GList       *new_child)
{
  GList *old_child;
  GtkNotebookPage *old_page = NULL;
  GtkNotebookPage *page;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->focus_tab == new_child)
    return;

  old_child = notebook->focus_tab;
  notebook->focus_tab = new_child;

  if (notebook->scrollable)
    gtk_notebook_redraw_arrows (notebook);

  if (!notebook->show_tabs || !notebook->focus_tab)
    return;

  if (old_child)
    old_page = old_child->data;

  page = notebook->focus_tab->data;
  if (GTK_WIDGET_MAPPED (page->tab_label))
    gtk_notebook_redraw_tabs (notebook);
  else
    gtk_notebook_pages_allocate (notebook);
  
  gtk_notebook_switch_page (notebook, page,
			    g_list_index (notebook->children, page));
}

static void
gtk_notebook_menu_switch_page (GtkWidget       *widget,
			       GtkNotebookPage *page)
{
  GtkNotebook *notebook;
  GList *children;
  guint page_num;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (page != NULL);

  notebook = GTK_NOTEBOOK (gtk_menu_get_attach_widget
			   (GTK_MENU (widget->parent)));

  if (notebook->cur_page == page)
    return;

  page_num = 0;
  children = notebook->children;
  while (children && children->data != page)
    {
      children = children->next;
      page_num++;
    }

  g_signal_emit (notebook,
		 notebook_signals[SWITCH_PAGE],
		 0,
		 page,
		 page_num);
}

/* Private GtkNotebook Menu Functions:
 *
 * gtk_notebook_menu_item_create
 * gtk_notebook_menu_label_unparent
 * gtk_notebook_menu_detacher
 */
static void
gtk_notebook_menu_item_create (GtkNotebook *notebook, 
			       GList       *list)
{	
  GtkNotebookPage *page;
  GtkWidget *menu_item;

  page = list->data;
  if (page->default_menu)
    {
      if (page->tab_label && GTK_IS_LABEL (page->tab_label))
	page->menu_label = gtk_label_new (GTK_LABEL (page->tab_label)->label);
      else
	page->menu_label = gtk_label_new ("");
      gtk_misc_set_alignment (GTK_MISC (page->menu_label), 0.0, 0.5);
    }

  gtk_widget_show (page->menu_label);
  menu_item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu_item), page->menu_label);
  gtk_menu_shell_insert (GTK_MENU_SHELL (notebook->menu), menu_item,
			 gtk_notebook_real_page_position (notebook, list));
  g_signal_connect (menu_item, "activate",
		    G_CALLBACK (gtk_notebook_menu_switch_page), page);
  if (GTK_WIDGET_VISIBLE (page->child))
    gtk_widget_show (menu_item);
}

static void
gtk_notebook_menu_label_unparent (GtkWidget *widget, 
				  gpointer  data)
{
  gtk_widget_unparent (GTK_BIN(widget)->child);
  GTK_BIN(widget)->child = NULL;
}

static void
gtk_notebook_menu_detacher (GtkWidget *widget,
			    GtkMenu   *menu)
{
  GtkNotebook *notebook;

  g_return_if_fail (GTK_IS_NOTEBOOK (widget));

  notebook = GTK_NOTEBOOK (widget);
  g_return_if_fail (notebook->menu == (GtkWidget*) menu);

  notebook->menu = NULL;
}

/* Private GtkNotebook Setter Functions:
 *
 * gtk_notebook_set_homogeneous_tabs_internal
 * gtk_notebook_set_tab_border_internal
 * gtk_notebook_set_tab_hborder_internal
 * gtk_notebook_set_tab_vborder_internal
 */
static void
gtk_notebook_set_homogeneous_tabs_internal (GtkNotebook *notebook,
				            gboolean     homogeneous)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (homogeneous == notebook->homogeneous)
    return;

  notebook->homogeneous = homogeneous;
  gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify (G_OBJECT (notebook), "homogeneous");
}

static void
gtk_notebook_set_tab_border_internal (GtkNotebook *notebook,
				      guint        border_width)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  notebook->tab_hborder = border_width;
  notebook->tab_vborder = border_width;

  if (GTK_WIDGET_VISIBLE (notebook) && notebook->show_tabs)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_freeze_notify (G_OBJECT (notebook));
  g_object_notify (G_OBJECT (notebook), "tab_hborder");
  g_object_notify (G_OBJECT (notebook), "tab_vborder");
  g_object_thaw_notify (G_OBJECT (notebook));
}

static void
gtk_notebook_set_tab_hborder_internal (GtkNotebook *notebook,
				       guint        tab_hborder)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->tab_hborder == tab_hborder)
    return;

  notebook->tab_hborder = tab_hborder;

  if (GTK_WIDGET_VISIBLE (notebook) && notebook->show_tabs)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify (G_OBJECT (notebook), "tab_hborder");
}

static void
gtk_notebook_set_tab_vborder_internal (GtkNotebook *notebook,
				       guint        tab_vborder)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->tab_vborder == tab_vborder)
    return;

  notebook->tab_vborder = tab_vborder;

  if (GTK_WIDGET_VISIBLE (notebook) && notebook->show_tabs)
    gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify (G_OBJECT (notebook), "tab_vborder");
}

/* Public GtkNotebook Page Insert/Remove Methods :
 *
 * gtk_notebook_append_page
 * gtk_notebook_append_page_menu
 * gtk_notebook_prepend_page
 * gtk_notebook_prepend_page_menu
 * gtk_notebook_insert_page
 * gtk_notebook_insert_page_menu
 * gtk_notebook_remove_page
 */
/**
 * gtk_notebook_append_page:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 * 
 * Appends a page to @notebook.
 **/
void
gtk_notebook_append_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label));
  
  gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, -1);
}

/**
 * gtk_notebook_append_page_menu:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 * @menu_label: the widget to use as a label for the page-switch
 *              menu, if that is enabled. If %NULL, and @tab_label
 *              is a #GtkLabel or %NULL, then the menu label will be
 *              a newly created label with the same text as @tab_label;
 *              If @tab_label is not a #GtkLabel, @menu_label must be
 *              specified if the page-switch menu is to be used.
 * 
 * Appends a page to @notebook, specifying the widget to use as the
 * label in the popup menu.
 **/
void
gtk_notebook_append_page_menu (GtkNotebook *notebook,
			       GtkWidget   *child,
			       GtkWidget   *tab_label,
			       GtkWidget   *menu_label)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label));
  g_return_if_fail (menu_label == NULL || GTK_IS_WIDGET (menu_label));
  
  gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, -1);
}

/**
 * gtk_notebook_prepend_page:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 *
 * Prepends a page to @notebook.
 **/
void
gtk_notebook_prepend_page (GtkNotebook *notebook,
			   GtkWidget   *child,
			   GtkWidget   *tab_label)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label));
  
  gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, 0);
}

/**
 * gtk_notebook_prepend_page_menu:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 * @menu_label: the widget to use as a label for the page-switch
 *              menu, if that is enabled. If %NULL, and @tab_label
 *              is a #GtkLabel or %NULL, then the menu label will be
 *              a newly created label with the same text as @tab_label;
 *              If @tab_label is not a #GtkLabel, @menu_label must be
 *              specified if the page-switch menu is to be used.
 * 
 * Prepends a page to @notebook, specifying the widget to use as the
 * label in the popup menu.
 **/
void
gtk_notebook_prepend_page_menu (GtkNotebook *notebook,
				GtkWidget   *child,
				GtkWidget   *tab_label,
				GtkWidget   *menu_label)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label));
  g_return_if_fail (menu_label == NULL || GTK_IS_WIDGET (menu_label));
  
  gtk_notebook_insert_page_menu (notebook, child, tab_label, menu_label, 0);
}

/**
 * gtk_notebook_insert_page:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 * @position: the index (starting at 0) at which to insert the page,
 *            or -1 to append the page after all other pages.
 * 
 * Insert a page into @notebook at the given position
 **/
void
gtk_notebook_insert_page (GtkNotebook *notebook,
			  GtkWidget   *child,
			  GtkWidget   *tab_label,
			  gint         position)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label));
  
  gtk_notebook_insert_page_menu (notebook, child, tab_label, NULL, position);
}


static gint
gtk_notebook_page_compare_tab (gconstpointer a,
			       gconstpointer b)
{
  return (((GtkNotebookPage *) a)->tab_label != b);
}

static gboolean
gtk_notebook_mnemonic_activate_switch_page (GtkWidget *child,
					    gboolean overload,
					    gpointer data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (data);
  GList *list;
  
  list = g_list_find_custom (notebook->children, child,
			     gtk_notebook_page_compare_tab);
  if (list)
    {
      GtkNotebookPage *page = list->data;

      gtk_widget_grab_focus (GTK_WIDGET (notebook));	/* Do this first to avoid focusing new page */
      gtk_notebook_switch_page (notebook, page,  -1);
      focus_tabs_in (notebook);
    }

  return TRUE;
}

/**
 * gtk_notebook_insert_page_menu:
 * @notebook: a #GtkNotebook
 * @child: the #GtkWidget to use as the contents of the page.
 * @tab_label: the #GtkWidget to be used as the label for the page,
 *             or %NULL to use the default label, 'page N'.
 * @menu_label: the widget to use as a label for the page-switch
 *              menu, if that is enabled. If %NULL, and @tab_label
 *              is a #GtkLabel or %NULL, then the menu label will be
 *              a newly created label with the same text as @tab_label;
 *              If @tab_label is not a #GtkLabel, @menu_label must be
 *              specified if the page-switch menu is to be used.
 * @position: the index (starting at 0) at which to insert the page,
 *            or -1 to append the page after all other pages.
 * 
 * Insert a page into @notebook at the given position, specifying
 * the widget to use as the label in the popup menu.
 **/
void
gtk_notebook_insert_page_menu (GtkNotebook *notebook,
			       GtkWidget   *child,
			       GtkWidget   *tab_label,
			       GtkWidget   *menu_label,
			       gint         position)
{
  GtkNotebookPage *page;
  gint nchildren;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (tab_label == NULL || GTK_IS_WIDGET (tab_label));
  g_return_if_fail (menu_label == NULL || GTK_IS_WIDGET (menu_label));

  gtk_widget_freeze_child_notify (child);
  
  page = g_new (GtkNotebookPage, 1);
  page->child = child;
  page->last_focus_child = NULL;
  page->requisition.width = 0;
  page->requisition.height = 0;
  page->allocation.x = 0;
  page->allocation.y = 0;
  page->allocation.width = 0;
  page->allocation.height = 0;
  page->default_menu = FALSE;
  page->default_tab = FALSE;
  page->mnemonic_activate_signal = 0;
   
  nchildren = g_list_length (notebook->children);
  if ((position < 0) || (position > nchildren))
    position = nchildren;

  notebook->children = g_list_insert (notebook->children, page, position);

  if (!tab_label)
    {
      page->default_tab = TRUE;
      if (notebook->show_tabs)
	tab_label = gtk_label_new ("");
    }
  page->tab_label = tab_label;
  page->menu_label = menu_label;
  page->expand = FALSE;
  page->fill = TRUE;
  page->pack = GTK_PACK_START;

  if (!menu_label)
    page->default_menu = TRUE;
  else  
    {
      g_object_ref (page->menu_label);
      gtk_object_sink (GTK_OBJECT (page->menu_label));
    }

  if (notebook->menu)
    gtk_notebook_menu_item_create (notebook,
				   g_list_find (notebook->children, page));

  gtk_widget_set_parent (child, GTK_WIDGET (notebook));
  if (tab_label)
    gtk_widget_set_parent (tab_label, GTK_WIDGET (notebook));

  gtk_notebook_update_labels (notebook);

  if (!notebook->first_tab)
    notebook->first_tab = notebook->children;

  if (!notebook->cur_page)
    gtk_widget_set_child_visible (child, TRUE);
  else
    gtk_widget_set_child_visible (child, FALSE);
  
  if (tab_label)
    {
      if (notebook->show_tabs && GTK_WIDGET_VISIBLE (child))
	gtk_widget_show (tab_label);
      else
	gtk_widget_hide (tab_label);
    }

  if (!notebook->cur_page)
    {
      gtk_notebook_switch_page (notebook, page, 0);
      gtk_notebook_switch_focus_tab (notebook, NULL);
    }

  gtk_notebook_update_tab_states (notebook);

  if (tab_label)
    page->mnemonic_activate_signal =
      g_signal_connect (tab_label,
			"mnemonic_activate",
			G_CALLBACK (gtk_notebook_mnemonic_activate_switch_page),
			notebook);

  gtk_widget_child_notify (child, "tab_expand");
  gtk_widget_child_notify (child, "tab_fill");
  gtk_widget_child_notify (child, "tab_pack");
  gtk_widget_child_notify (child, "tab_label");
  gtk_widget_child_notify (child, "menu_label");
  gtk_widget_child_notify (child, "position");
  gtk_widget_thaw_child_notify (child);
}

/**
 * gtk_notebook_remove_page:
 * @notebook: a #GtkNotebook.
 * @page_num: the index of a notebook page, starting
 *            from 0. If -1, the last page will
 *            be removed.
 * 
 * Removes a page from the notebook given its index
 * in the notebook.
 **/
void
gtk_notebook_remove_page (GtkNotebook *notebook,
			  gint         page_num)
{
  GList *list;
  
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  
  if (page_num >= 0)
    {
      list = g_list_nth (notebook->children, page_num);
      if (list)
	gtk_notebook_real_remove (notebook, list, FALSE);
    }
  else
    {
      list = g_list_last (notebook->children);
      if (list)
	gtk_notebook_real_remove (notebook, list, FALSE);
    }
}

/* Public GtkNotebook Page Switch Methods :
 * gtk_notebook_get_current_page
 * gtk_notebook_page_num
 * gtk_notebook_set_current_page
 * gtk_notebook_next_page
 * gtk_notebook_prev_page
 */
/**
 * gtk_notebook_get_current_page:
 * @notebook: a #GtkNotebook
 * 
 * Returns the page number of the current page.
 * 
 * Return value: the index (starting from 0) of the current
 * page in the notebook. If the notebook has no pages, then
 * -1 will be returned.
 **/
gint
gtk_notebook_get_current_page (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  if (!notebook->cur_page)
    return -1;

  return g_list_index (notebook->children, notebook->cur_page);
}

/**
 * gtk_notebook_get_nth_page:
 * @notebook: a #GtkNotebook
 * @page_num: the index of a page in the noteobok, or -1
 *            to get the last page.
 * 
 * Returns the child widget contained in page number @page_num.
 * 
 * Return value: the child widget, or %NULL if @page_num is
 * out of bounds.
 **/
GtkWidget*
gtk_notebook_get_nth_page (GtkNotebook *notebook,
			   gint         page_num)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  if (page_num >= 0)
    list = g_list_nth (notebook->children, page_num);
  else
    list = g_list_last (notebook->children);

  if (list)
    {
      page = list->data;
      return page->child;
    }

  return NULL;
}

/**
 * gtk_notebook_get_n_pages:
 * @notebook: a #GtkNotebook
 * 
 * Gets the number of pages in a notebook.
 * 
 * Return value: the number of pages in the notebook.
 *
 * Since: 2.2
 **/
gint
gtk_notebook_get_n_pages (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), 0);

  return g_list_length (notebook->children);
}

/**
 * gtk_notebook_page_num:
 * @notebook: a #GtkNotebook
 * @child: a #GtkWidget
 * 
 * Finds the index of the page which contains the given child
 * widget.
 * 
 * Return value: the index of the page containing @child, or
 *   -1 if @child is not in the notebook.
 **/
gint
gtk_notebook_page_num (GtkNotebook      *notebook,
		       GtkWidget        *child)
{
  GList *children;
  gint num;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), -1);

  num = 0;
  children = notebook->children;
  while (children)
    {
      GtkNotebookPage *page =  children->data;
      
      if (page->child == child)
	return num;

      children = children->next;
      num++;
    }

  return -1;
}

/**
 * gtk_notebook_set_current_page:
 * @notebook: a #GtkNotebook
 * @page_num: index of the page to switch to, starting from 0.
 *            If negative, the last page will be used. If greater
 *            than the number of pages in the notebook, nothing
 *            will be done.
 *                
 * Switches to the page number @page_num.
 **/
void
gtk_notebook_set_current_page (GtkNotebook *notebook,
			       gint         page_num)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (page_num >= 0)
    list = g_list_nth (notebook->children, page_num);
  else
    list = g_list_last (notebook->children);

  page_num = g_list_index (notebook->children, list);

  if (list)
    gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list), page_num);
}

/**
 * gtk_notebook_next_page:
 * @notebook: a #GtkNotebook
 * 
 * Switches to the next page. Nothing happens if the current page is
 * the last page.
 **/
void
gtk_notebook_next_page (GtkNotebook *notebook)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  list = g_list_find (notebook->children, notebook->cur_page);
  if (!list)
    return;

  list = gtk_notebook_search_page (notebook, list, STEP_NEXT, TRUE);
  if (!list)
    return;

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list), -1);
}

/**
 * gtk_notebook_prev_page:
 * @notebook: a #GtkNotebook
 * 
 * Switches to the previous page. Nothing happens if the current page
 * is the first page.
 **/
void
gtk_notebook_prev_page (GtkNotebook *notebook)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  list = g_list_find (notebook->children, notebook->cur_page);
  if (!list)
    return;

  list = gtk_notebook_search_page (notebook, list, STEP_PREV, TRUE);
  if (!list)
    return;

  gtk_notebook_switch_page (notebook, GTK_NOTEBOOK_PAGE (list), -1);
}

/* Public GtkNotebook/Tab Style Functions
 *
 * gtk_notebook_set_show_border
 * gtk_notebook_set_show_tabs
 * gtk_notebook_set_tab_pos
 * gtk_notebook_set_homogeneous_tabs
 * gtk_notebook_set_tab_border
 * gtk_notebook_set_tab_hborder
 * gtk_notebook_set_tab_vborder
 * gtk_notebook_set_scrollable
 */
/**
 * gtk_notebook_set_show_border:
 * @notebook: a #GtkNotebook
 * @show_border: %TRUE if a bevel should be drawn around the notebook.
 * 
 * Sets whether a bevel will be drawn around the notebook pages.
 * This only has a visual effect when the tabs are not shown.
 * See gtk_notebook_set_show_tabs().
 **/
void
gtk_notebook_set_show_border (GtkNotebook *notebook,
			      gboolean     show_border)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->show_border != show_border)
    {
      notebook->show_border = show_border;

      if (GTK_WIDGET_VISIBLE (notebook))
	gtk_widget_queue_resize (GTK_WIDGET (notebook));
      
      g_object_notify (G_OBJECT (notebook), "show_border");
    }
}

/**
 * gtk_notebook_get_show_border:
 * @notebook: a #GtkNotebook
 *
 * Returns whether a bevel will be drawn around the notebook pages. See
 * gtk_notebook_set_show_border().
 *
 * Return value: %TRUE if the bevel is drawn
 **/
gboolean
gtk_notebook_get_show_border (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->show_border;
}

/**
 * gtk_notebook_set_show_tabs:
 * @notebook: a #GtkNotebook
 * @show_tabs: %TRUE if the tabs should be shown.
 * 
 * Sets whether to show the tabs for the notebook or not.
 **/
void
gtk_notebook_set_show_tabs (GtkNotebook *notebook,
			    gboolean     show_tabs)
{
  GtkNotebookPage *page;
  GList *children;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  show_tabs = show_tabs != FALSE;

  if (notebook->show_tabs == show_tabs)
    return;

  notebook->show_tabs = show_tabs;
  children = notebook->children;

  if (!show_tabs)
    {
      GTK_WIDGET_UNSET_FLAGS (notebook, GTK_CAN_FOCUS);
      
      while (children)
	{
	  page = children->data;
	  children = children->next;
	  if (page->default_tab)
	    {
	      gtk_widget_destroy (page->tab_label);
	      page->tab_label = NULL;
	    }
	  else
	    gtk_widget_hide (page->tab_label);
	}
    }
  else
    {
      GTK_WIDGET_SET_FLAGS (notebook, GTK_CAN_FOCUS);
      gtk_notebook_update_labels (notebook);
    }
  gtk_widget_queue_resize (GTK_WIDGET (notebook));

  g_object_notify (G_OBJECT (notebook), "show_tabs");
}

/**
 * gtk_notebook_get_show_tabs:
 * @notebook: a #GtkNotebook
 *
 * Returns whether the tabs of the notebook are shown. See
 * gtk_notebook_set_show_tabs().
 *
 * Return value: %TRUE if the tabs are shown
 **/
gboolean
gtk_notebook_get_show_tabs (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->show_tabs;
}

/**
 * gtk_notebook_set_tab_pos:
 * @notebook: a #GtkNotebook.
 * @pos: the edge to draw the tabs at.
 * 
 * Sets the edge at which the tabs for switching pages in the
 * notebook are drawn.
 **/
void
gtk_notebook_set_tab_pos (GtkNotebook     *notebook,
			  GtkPositionType  pos)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->tab_pos != pos)
    {
      notebook->tab_pos = pos;
      if (GTK_WIDGET_VISIBLE (notebook))
	gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }

  g_object_notify (G_OBJECT (notebook), "tab_pos");
}

/**
 * gtk_notebook_get_tab_pos:
 * @notebook: a #GtkNotebook
 *
 * Gets the edge at which the tabs for switching pages in the
 * notebook are drawn.
 *
 * Return value: the edge at which the tabs are drawn
 **/
GtkPositionType
gtk_notebook_get_tab_pos (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), GTK_POS_TOP);

  return notebook->tab_pos;
}

/**
 * gtk_notebook_set_homogeneous_tabs:
 * @notebook: a #GtkNotebook
 * @homogeneous: %TRUE if all tabs should be the same size.
 * 
 * Sets whether the tabs must have all the same size or not.
 **/
void
gtk_notebook_set_homogeneous_tabs (GtkNotebook *notebook,
				   gboolean     homogeneous)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  gtk_notebook_set_homogeneous_tabs_internal (notebook, homogeneous);
}

/**
 * gtk_notebook_set_tab_border:
 * @notebook: a #GtkNotebook
 * @border_width: width of the border around the tab labels.
 * 
 * Sets the width the border around the tab labels
 * in a notebook. This is equivalent to calling
 * gtk_notebook_set_tab_hborder (@notebook, @border_width) followed
 * by gtk_notebook_set_tab_vborder (@notebook, @border_width).
 **/
void
gtk_notebook_set_tab_border (GtkNotebook *notebook,
			     guint        border_width)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  gtk_notebook_set_tab_border_internal (notebook, border_width);
}

/**
 * gtk_notebook_set_tab_hborder:
 * @notebook: a #GtkNotebook
 * @tab_hborder: width of the horizontal border of tab labels.
 * 
 * Sets the width of the horizontal border of tab labels.
 **/
void
gtk_notebook_set_tab_hborder (GtkNotebook *notebook,
			      guint        tab_hborder)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  gtk_notebook_set_tab_hborder_internal (notebook, tab_hborder);
}

/**
 * gtk_notebook_set_tab_vborder:
 * @notebook: a #GtkNotebook
 * @tab_vborder: width of the vertical border of tab labels.
 * 
 * Sets the width of the vertical border of tab labels.
 **/
void
gtk_notebook_set_tab_vborder (GtkNotebook *notebook,
			      guint        tab_vborder)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  gtk_notebook_set_tab_vborder_internal (notebook, tab_vborder);
}

/**
 * gtk_notebook_set_scrollable:
 * @notebook: a #GtkNotebook
 * @scrollable: %TRUE if scroll arrows should be added
 * 
 * Sets whether the tab label area will have arrows for scrolling if
 * there are too many tabs to fit in the area.
 **/
void
gtk_notebook_set_scrollable (GtkNotebook *notebook,
			     gboolean     scrollable)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  scrollable = (scrollable != FALSE);

  if (scrollable != notebook->scrollable)
    {
      notebook->scrollable = scrollable;

      if (GTK_WIDGET_VISIBLE (notebook))
	gtk_widget_queue_resize (GTK_WIDGET (notebook));

      g_object_notify (G_OBJECT (notebook), "scrollable");
    }
}

/**
 * gtk_notebook_get_scrollable:
 * @notebook: a #GtkNotebook
 *
 * Returns whether the tab label area has arrows for scrolling. See
 * gtk_notebook_set_scrollable().
 *
 * Return value: %TRUE if arrows for scrolling are present
 **/
gboolean
gtk_notebook_get_scrollable (GtkNotebook *notebook)
{
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), FALSE);

  return notebook->scrollable;
}

/* Public GtkNotebook Popup Menu Methods:
 *
 * gtk_notebook_popup_enable
 * gtk_notebook_popup_disable
 */


/**
 * gtk_notebook_popup_enable:
 * @notebook: a #GtkNotebook
 * 
 * Enables the popup menu: if the user clicks with the right mouse button on
 * the bookmarks, a menu with all the pages will be popped up.
 **/
void
gtk_notebook_popup_enable (GtkNotebook *notebook)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (notebook->menu)
    return;

  notebook->menu = gtk_menu_new ();
  for (list = gtk_notebook_search_page (notebook, NULL, STEP_NEXT, FALSE);
       list;
       list = gtk_notebook_search_page (notebook, list, STEP_NEXT, FALSE))
    gtk_notebook_menu_item_create (notebook, list);

  gtk_notebook_update_labels (notebook);
  gtk_menu_attach_to_widget (GTK_MENU (notebook->menu),
			     GTK_WIDGET (notebook),
			     gtk_notebook_menu_detacher);

  g_object_notify (G_OBJECT (notebook), "enable_popup");
}

/**
 * gtk_notebook_popup_disable:
 * @notebook: a #GtkNotebook
 * 
 * Disables the popup menu.
 **/
void       
gtk_notebook_popup_disable  (GtkNotebook *notebook)
{
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (!notebook->menu)
    return;

  gtk_container_foreach (GTK_CONTAINER (notebook->menu),
			 (GtkCallback) gtk_notebook_menu_label_unparent, NULL);
  gtk_widget_destroy (notebook->menu);

  g_object_notify (G_OBJECT (notebook), "enable_popup");
}

/* Public GtkNotebook Page Properties Functions:
 *
 * gtk_notebook_get_tab_label
 * gtk_notebook_set_tab_label
 * gtk_notebook_set_tab_label_text
 * gtk_notebook_get_menu_label
 * gtk_notebook_set_menu_label
 * gtk_notebook_set_menu_label_text
 * gtk_notebook_set_tab_label_packing
 * gtk_notebook_query_tab_label_packing
 */

/**
 * gtk_notebook_get_tab_label:
 * @notebook: a #GtkNotebook
 * @child: the page
 * 
 * Returns the tab label widget for the page @child. %NULL is returned
 * if @child is not in @notebook or if no tab label has specifically
 * been set for @child.
 * 
 * Return value: the tab label
 **/
GtkWidget *
gtk_notebook_get_tab_label (GtkNotebook *notebook,
			    GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return NULL;

  if (GTK_NOTEBOOK_PAGE (list)->default_tab)
    return NULL;

  return GTK_NOTEBOOK_PAGE (list)->tab_label;
}  

/**
 * gtk_notebook_set_tab_label:
 * @notebook: a #GtkNotebook
 * @child: the page
 * @tab_label: the tab label widget to use, or %NULL for default tab
 *             label.
 * 
 * Changes the tab label for @child. If %NULL is specified
 * for @tab_label, then the page will have the label 'page N'.
 **/
void
gtk_notebook_set_tab_label (GtkNotebook *notebook,
			    GtkWidget   *child,
			    GtkWidget   *tab_label)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return;

  /* a NULL pointer indicates a default_tab setting, otherwise
   * we need to set the associated label
   */
  page = list->data;
  
  if (page->tab_label == tab_label)
    return;
  

  gtk_notebook_remove_tab_label (notebook, page);
  
  if (tab_label)
    {
      page->default_tab = FALSE;
      page->tab_label = tab_label;
      gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
    }
  else
    {
      page->default_tab = TRUE;
      page->tab_label = NULL;

      if (notebook->show_tabs)
	{
	  gchar string[32];

	  g_snprintf (string, sizeof(string), _("Page %u"), 
		      gtk_notebook_real_page_position (notebook, list));
	  page->tab_label = gtk_label_new (string);
	  gtk_widget_set_parent (page->tab_label, GTK_WIDGET (notebook));
	}
    }

  if (page->tab_label)
    page->mnemonic_activate_signal =
      g_signal_connect (page->tab_label,
			"mnemonic_activate",
			G_CALLBACK (gtk_notebook_mnemonic_activate_switch_page),
			notebook);

  if (notebook->show_tabs && GTK_WIDGET_VISIBLE (child))
    {
      gtk_widget_show (page->tab_label);
      gtk_widget_queue_resize (GTK_WIDGET (notebook));
    }

  gtk_notebook_update_tab_states (notebook);
  gtk_widget_child_notify (child, "tab_label");
}

/**
 * gtk_notebook_set_tab_label_text:
 * @notebook: a #GtkNotebook
 * @child: the page
 * @tab_text: the label text
 * 
 * Creates a new label and sets it as the tab label for the page
 * containing @child.
 **/
void
gtk_notebook_set_tab_label_text (GtkNotebook *notebook,
				 GtkWidget   *child,
				 const gchar *tab_text)
{
  GtkWidget *tab_label = NULL;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (tab_text)
    tab_label = gtk_label_new (tab_text);
  gtk_notebook_set_tab_label (notebook, child, tab_label);
  gtk_widget_child_notify (child, "tab_label");
}

/**
 * gtk_notebook_get_tab_label_text:
 * @notebook: a #GtkNotebook
 * @child: a widget contained in a page of @notebook
 *
 * Retrieves the text of the tab label for the page containing
 *    @child.
 *
 * Returns value: the text of the tab label, or %NULL if the
 *                tab label widget is not a #GtkLabel. The
 *                string is owned by the widget and must not
 *                be freed.
 **/
G_CONST_RETURN gchar *
gtk_notebook_get_tab_label_text (GtkNotebook *notebook,
				 GtkWidget   *child)
{
  GtkWidget *tab_label;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  tab_label = gtk_notebook_get_tab_label (notebook, child);

  if (tab_label && GTK_IS_LABEL (tab_label))
    return gtk_label_get_text (GTK_LABEL (tab_label));
  else
    return NULL;
}

/**
 * gtk_notebook_get_menu_label:
 * @notebook: a #GtkNotebook
 * @child: a widget contained in a page of @notebook
 * 
 * Retrieves the menu label widget of the page containing @child.
 * 
 * Return value: the menu label, or %NULL if the
 *               notebook page does not have a menu label other
 *               than the default (the tab label).
 **/
GtkWidget*
gtk_notebook_get_menu_label (GtkNotebook *notebook,
			     GtkWidget   *child)
{
  GList *list;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return NULL;

  if (GTK_NOTEBOOK_PAGE (list)->default_menu)
    return NULL;

  return GTK_NOTEBOOK_PAGE (list)->menu_label;
}  

/**
 * gtk_notebook_set_menu_label:
 * @notebook: a #GtkNotebook
 * @child: the child widget
 * @menu_label: the menu label, or NULL for default
 * 
 * Changes the menu label for the page containing @child. 
 **/
void
gtk_notebook_set_menu_label (GtkNotebook *notebook,
			     GtkWidget   *child,
			     GtkWidget   *menu_label)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return;

  page = list->data;
  if (page->menu_label)
    {
      if (notebook->menu)
	gtk_container_remove (GTK_CONTAINER (notebook->menu), 
			      page->menu_label->parent);

      if (!page->default_menu)
	g_object_unref (page->menu_label);
    }

  if (menu_label)
    {
      page->menu_label = menu_label;
      g_object_ref (page->menu_label);
      gtk_object_sink (GTK_OBJECT(page->menu_label));
      page->default_menu = FALSE;
    }
  else
    page->default_menu = TRUE;

  if (notebook->menu)
    gtk_notebook_menu_item_create (notebook, list);
  gtk_widget_child_notify (child, "menu_label");
}

/**
 * gtk_notebook_set_menu_label_text:
 * @notebook: a #GtkNotebook
 * @child: the child widget
 * @menu_text: the label text
 * 
 * Creates a new label and sets it as the menu label of @child.
 **/
void
gtk_notebook_set_menu_label_text (GtkNotebook *notebook,
				  GtkWidget   *child,
				  const gchar *menu_text)
{
  GtkWidget *menu_label = NULL;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  if (menu_text)
    menu_label = gtk_label_new (menu_text);
  gtk_notebook_set_menu_label (notebook, child, menu_label);
  gtk_widget_child_notify (child, "menu_label");
}

/**
 * gtk_notebook_get_menu_label_text:
 * @notebook: a #GtkNotebook
 * @child: the child widget of a page of the notebook.
 *
 * Retrieves the text of the menu label for the page containing
 *    @child.
 *
 * Returns value: the text of the tab label, or %NULL if the
 *                widget does not have a menu label other than
 *                the default menu label, or the menu label widget
 *                is not a #GtkLabel. The string is owned by
 *                the widget and must not be freed.
 **/
G_CONST_RETURN gchar *
gtk_notebook_get_menu_label_text (GtkNotebook *notebook,
				  GtkWidget *child)
{
  GtkWidget *menu_label;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);
 
  menu_label = gtk_notebook_get_menu_label (notebook, child);

  if (menu_label && GTK_IS_LABEL (menu_label))
    return gtk_label_get_text (GTK_LABEL (menu_label));
  else
    return NULL;
}
  
/* Helper function called when pages are reordered
 */
static void
gtk_notebook_child_reordered (GtkNotebook     *notebook,
			      GtkNotebookPage *page)
{
  if (notebook->menu)
    {
      GtkWidget *menu_item;
      
      menu_item = page->menu_label->parent;
      gtk_container_remove (GTK_CONTAINER (menu_item), page->menu_label);
      gtk_container_remove (GTK_CONTAINER (notebook->menu), menu_item);
      gtk_notebook_menu_item_create (notebook, g_list_find (notebook->children, page));
    }

  gtk_notebook_update_tab_states (notebook);
  gtk_notebook_update_labels (notebook);
}

/**
 * gtk_notebook_set_tab_label_packing:
 * @notebook: a #GtkNotebook
 * @child: the child widget
 * @expand: whether to expand the bookmark or not
 * @fill: whether the bookmark should fill the allocated area or not
 * @pack_type: the position of the bookmark
 * 
 * Sets the packing parameters for the tab label of the page
 * containing @child. See gtk_box_pack_start() for the exact meaning
 * of the parameters.
 **/
void
gtk_notebook_set_tab_label_packing (GtkNotebook *notebook,
				    GtkWidget   *child,
				    gboolean     expand,
				    gboolean     fill,
				    GtkPackType  pack_type)
{
  GtkNotebookPage *page;
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)  
    return;

  page = list->data;
  expand = expand != FALSE;
  fill = fill != FALSE;
  if (page->pack == pack_type && page->expand == expand && page->fill == fill)
    return;

  gtk_widget_freeze_child_notify (child);
  page->expand = expand;
  gtk_widget_child_notify (child, "tab_expand");
  page->fill = fill;
  gtk_widget_child_notify (child, "tab_fill");
  if (page->pack != pack_type)
    {
      page->pack = pack_type;
      gtk_notebook_child_reordered (notebook, page);
    }
  gtk_widget_child_notify (child, "tab_pack");
  gtk_widget_child_notify (child, "position");
  if (notebook->show_tabs)
    gtk_notebook_pages_allocate (notebook);
  gtk_widget_thaw_child_notify (child);
}  

/**
 * gtk_notebook_query_tab_label_packing:
 * @notebook: a #GtkNotebook
 * @child: the page
 * @expand: location to store the expand value (or NULL)
 * @fill: location to store the fill value (or NULL)
 * @pack_type: location to store the pack_type (or NULL)
 * 
 * Query the packing attributes for the tab label of the page
 * containing @child.
 **/
void
gtk_notebook_query_tab_label_packing (GtkNotebook *notebook,
				      GtkWidget   *child,
				      gboolean    *expand,
				      gboolean    *fill,
				      GtkPackType *pack_type)
{
  GList *list;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)
    return;

  if (expand)
    *expand = GTK_NOTEBOOK_PAGE (list)->expand;
  if (fill)
    *fill = GTK_NOTEBOOK_PAGE (list)->fill;
  if (pack_type)
    *pack_type = GTK_NOTEBOOK_PAGE (list)->pack;
}

/**
 * gtk_notebook_reorder_child:
 * @notebook: a #GtkNotebook
 * @child: the child to move
 * @position: the new position, or -1 to move to the end
 * 
 * Reorders the page containing @child, so that it appears in position
 * @position. If @position is greater than or equal to the number of
 * children in the list or negative, @child will be moved to the end
 * of the list.
 **/
void
gtk_notebook_reorder_child (GtkNotebook *notebook,
			    GtkWidget   *child,
			    gint         position)
{
  GList *list, *new_list;
  GtkNotebookPage *page;
  gint old_pos;
  gint max_pos;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = CHECK_FIND_CHILD (notebook, child);
  if (!list)
    return;

  max_pos = g_list_length (notebook->children) - 1;
  if (position < 0 || position > max_pos)
    position = max_pos;

  old_pos = g_list_position (notebook->children, list);

  if (old_pos == position)
    return;

  page = list->data;
  notebook->children = g_list_delete_link (notebook->children, list);

  notebook->children = g_list_insert (notebook->children, page, position);
  new_list = g_list_nth (notebook->children, position);

  /* Fix up GList references in GtkNotebook structure */
  if (notebook->first_tab == list)
    notebook->first_tab = new_list;
  if (notebook->focus_tab == list)
    notebook->focus_tab = new_list;

  gtk_widget_freeze_child_notify (child);

  /* Move around the menu items if necesary */
  gtk_notebook_child_reordered (notebook, page);
  gtk_widget_child_notify (child, "tab_pack");
  gtk_widget_child_notify (child, "position");
  
  if (notebook->show_tabs)
    gtk_notebook_pages_allocate (notebook);

  gtk_widget_thaw_child_notify (child);
}
