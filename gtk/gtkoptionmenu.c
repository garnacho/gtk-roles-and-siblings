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

#include "gtkintl.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkoptionmenu.h"
#include "gtksignal.h"
#include "gdk/gdkkeysyms.h"


#define CHILD_LEFT_SPACING        5
#define CHILD_RIGHT_SPACING       1
#define CHILD_TOP_SPACING         1
#define CHILD_BOTTOM_SPACING      1

typedef struct _GtkOptionMenuProps GtkOptionMenuProps;

struct _GtkOptionMenuProps
{
  gboolean interior_focus;
  GtkRequisition indicator_size;
  GtkBorder indicator_spacing;
};

static GtkOptionMenuProps default_props = {
  FALSE,
  { 7, 13 },
  { 7, 5, 2, 2 }		/* Left, right, top, bottom */
};

static void gtk_option_menu_class_init      (GtkOptionMenuClass *klass);
static void gtk_option_menu_init            (GtkOptionMenu      *option_menu);
static void gtk_option_menu_destroy         (GtkObject          *object);
static void gtk_option_menu_size_request    (GtkWidget          *widget,
					     GtkRequisition     *requisition);
static void gtk_option_menu_size_allocate   (GtkWidget          *widget,
					     GtkAllocation      *allocation);
static void gtk_option_menu_paint           (GtkWidget          *widget,
					     GdkRectangle       *area);
static gint gtk_option_menu_expose          (GtkWidget          *widget,
					     GdkEventExpose     *event);
static gint gtk_option_menu_button_press    (GtkWidget          *widget,
					     GdkEventButton     *event);
static gint gtk_option_menu_key_press	    (GtkWidget          *widget,
					     GdkEventKey        *event);
static void gtk_option_menu_selection_done  (GtkMenuShell       *menu_shell,
					     GtkOptionMenu      *option_menu);
static void gtk_option_menu_update_contents (GtkOptionMenu      *option_menu);
static void gtk_option_menu_remove_contents (GtkOptionMenu      *option_menu);
static void gtk_option_menu_calc_size       (GtkOptionMenu      *option_menu);
static void gtk_option_menu_position        (GtkMenu            *menu,
					     gint               *x,
					     gint               *y,
					     gint               *scroll_offet,
					     gpointer            user_data);
static void gtk_option_menu_show_all        (GtkWidget          *widget);
static void gtk_option_menu_hide_all        (GtkWidget          *widget);
static gboolean gtk_option_menu_mnemonic_activate (GtkWidget    *widget,
						   gboolean      group_cycling);
static GtkType gtk_option_menu_child_type   (GtkContainer       *container);
static gint gtk_option_menu_scroll_event    (GtkWidget          *widget,
					     GdkEventScroll     *event);

enum
{
  CHANGED,
  LAST_SIGNAL
};

static GtkButtonClass *parent_class = NULL;
static guint           signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_option_menu_get_type (void)
{
  static GtkType option_menu_type = 0;

  if (!option_menu_type)
    {
      static const GtkTypeInfo option_menu_info =
      {
	"GtkOptionMenu",
	sizeof (GtkOptionMenu),
	sizeof (GtkOptionMenuClass),
	(GtkClassInitFunc) gtk_option_menu_class_init,
	(GtkObjectInitFunc) gtk_option_menu_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      option_menu_type = gtk_type_unique (gtk_button_get_type (), &option_menu_info);
    }

  return option_menu_type;
}

static void
gtk_option_menu_class_init (GtkOptionMenuClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkButtonClass *button_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  button_class = (GtkButtonClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_button_get_type ());

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkOptionMenuClass, changed),
                  NULL, NULL,
                  gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  
  object_class->destroy = gtk_option_menu_destroy;
  
  widget_class->size_request = gtk_option_menu_size_request;
  widget_class->size_allocate = gtk_option_menu_size_allocate;
  widget_class->expose_event = gtk_option_menu_expose;
  widget_class->button_press_event = gtk_option_menu_button_press;
  widget_class->key_press_event = gtk_option_menu_key_press;
  widget_class->scroll_event = gtk_option_menu_scroll_event;
  widget_class->show_all = gtk_option_menu_show_all;
  widget_class->hide_all = gtk_option_menu_hide_all;
  widget_class->mnemonic_activate = gtk_option_menu_mnemonic_activate;

  container_class->child_type = gtk_option_menu_child_type;

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("indicator_size",
							       _("Indicator Size"),
							       _("Size of dropdown indicator"),
							       GTK_TYPE_REQUISITION,
							       G_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("indicator_spacing",
							       _("Indicator Spacing"),
							       _("Spacing around indicator"),
							       GTK_TYPE_BORDER,
							       G_PARAM_READABLE));
}

static GtkType
gtk_option_menu_child_type (GtkContainer       *container)
{
  return GTK_TYPE_NONE;
}

static void
gtk_option_menu_init (GtkOptionMenu *option_menu)
{
  GTK_WIDGET_SET_FLAGS (option_menu, GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS (option_menu, GTK_CAN_DEFAULT | GTK_RECEIVES_DEFAULT);

  option_menu->menu = NULL;
  option_menu->menu_item = NULL;
  option_menu->width = 0;
  option_menu->height = 0;
}

GtkWidget*
gtk_option_menu_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_option_menu_get_type ()));
}

GtkWidget*
gtk_option_menu_get_menu (GtkOptionMenu *option_menu)
{
  g_return_val_if_fail (GTK_IS_OPTION_MENU (option_menu), NULL);

  return option_menu->menu;
}

static void
gtk_option_menu_detacher (GtkWidget     *widget,
			  GtkMenu	*menu)
{
  GtkOptionMenu *option_menu;

  g_return_if_fail (GTK_IS_OPTION_MENU (widget));

  option_menu = GTK_OPTION_MENU (widget);
  g_return_if_fail (option_menu->menu == (GtkWidget*) menu);

  gtk_option_menu_remove_contents (option_menu);
  gtk_signal_disconnect_by_data (GTK_OBJECT (option_menu->menu),
				 option_menu);
  
  option_menu->menu = NULL;
}

void
gtk_option_menu_set_menu (GtkOptionMenu *option_menu,
			  GtkWidget     *menu)
{
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));
  g_return_if_fail (GTK_IS_MENU (menu));

  if (option_menu->menu != menu)
    {
      gtk_option_menu_remove_menu (option_menu);

      option_menu->menu = menu;
      gtk_menu_attach_to_widget (GTK_MENU (menu),
				 GTK_WIDGET (option_menu),
				 gtk_option_menu_detacher);

      gtk_option_menu_calc_size (option_menu);

      gtk_signal_connect_after (GTK_OBJECT (option_menu->menu), "selection_done",
				G_CALLBACK (gtk_option_menu_selection_done),
				option_menu);
      gtk_signal_connect_object (GTK_OBJECT (option_menu->menu), "size_request",
				 (GtkSignalFunc) gtk_option_menu_calc_size,
				 GTK_OBJECT (option_menu));

      if (GTK_WIDGET (option_menu)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (option_menu));

      gtk_option_menu_update_contents (option_menu);
    }
}

void
gtk_option_menu_remove_menu (GtkOptionMenu *option_menu)
{
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  if (option_menu->menu)
    gtk_menu_detach (GTK_MENU (option_menu->menu));
}

void
gtk_option_menu_set_history (GtkOptionMenu *option_menu,
			     guint          index)
{
  GtkWidget *menu_item;

  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  if (option_menu->menu)
    {
      gtk_menu_set_active (GTK_MENU (option_menu->menu), index);
      menu_item = gtk_menu_get_active (GTK_MENU (option_menu->menu));

      if (menu_item != option_menu->menu_item)
        gtk_option_menu_update_contents (option_menu);
    }
}

/**
 * gtk_option_menu_get_history:
 * @option_menu: a #GtkOptionMenu
 * 
 * Retrieves the index of the currently selected menu item. The menu
 * items are numbered from top to bottom, starting with 0. 
 * 
 * Return value: index of the selected menu item, or -1 if there are no menu items
 **/
gint
gtk_option_menu_get_history (GtkOptionMenu *option_menu)
{
  GtkWidget *active_widget;
  
  g_return_val_if_fail (GTK_IS_OPTION_MENU (option_menu), -1);

  if (option_menu->menu)
    {
      active_widget = gtk_menu_get_active (GTK_MENU (option_menu->menu));

      if (active_widget)
	return g_list_index (GTK_MENU_SHELL (option_menu->menu)->children,
                             active_widget);
      else
	return -1;
    }
  else
    return -1;
}

static void
gtk_option_menu_destroy (GtkObject *object)
{
  GtkOptionMenu *option_menu;

  g_return_if_fail (GTK_IS_OPTION_MENU (object));

  option_menu = GTK_OPTION_MENU (object);

  if (option_menu->menu)
    gtk_widget_destroy (option_menu->menu);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_option_menu_get_props (GtkOptionMenu       *option_menu,
			   GtkOptionMenuProps  *props)
{
  GtkRequisition *indicator_size;
  GtkBorder *indicator_spacing;
  
  gtk_widget_style_get (GTK_WIDGET (option_menu),
			"indicator_size", &indicator_size,
			"indicator_spacing", &indicator_spacing,
			"interior_focus", &props->interior_focus,
			NULL);

  if (indicator_size)
    props->indicator_size = *indicator_size;
  else
    props->indicator_size = default_props.indicator_size;

  if (indicator_spacing)
    props->indicator_spacing = *indicator_spacing;
  else
    props->indicator_spacing = default_props.indicator_spacing;

  g_free (indicator_size);
  g_free (indicator_spacing);
}

static void
gtk_option_menu_size_request (GtkWidget      *widget,
			      GtkRequisition *requisition)
{
  GtkOptionMenu *option_menu = GTK_OPTION_MENU (widget);
  GtkOptionMenuProps props;
  gint tmp;
  GtkRequisition child_requisition = { 0, 0 };
      
  gtk_option_menu_get_props (option_menu, &props);
 
  if (GTK_BIN (option_menu)->child && GTK_WIDGET_VISIBLE (GTK_BIN (option_menu)->child))
    {
      gtk_widget_size_request (GTK_BIN (option_menu)->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
  
  requisition->width = ((GTK_CONTAINER (widget)->border_width +
			 GTK_WIDGET (widget)->style->xthickness) * 2 +
			MAX (child_requisition.width, option_menu->width) +
 			props.indicator_size.width +
 			props.indicator_spacing.left + props.indicator_spacing.right +
			CHILD_LEFT_SPACING + CHILD_RIGHT_SPACING + 2);
  requisition->height = ((GTK_CONTAINER (widget)->border_width +
			  GTK_WIDGET (widget)->style->ythickness) * 2 +
			 MAX (child_requisition.height, option_menu->height) +
			 CHILD_TOP_SPACING + CHILD_BOTTOM_SPACING + 2);

  tmp = (requisition->height - option_menu->height +
	 props.indicator_size.height + props.indicator_spacing.top + props.indicator_spacing.bottom);
  requisition->height = MAX (requisition->height, tmp);
}

static void
gtk_option_menu_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  GtkWidget *child;
  GtkAllocation child_allocation;
  GtkOptionMenuProps props;
  gint border_width;
    
  gtk_option_menu_get_props (GTK_OPTION_MENU (widget), &props);
  border_width = GTK_CONTAINER (widget)->border_width;

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x + border_width, allocation->y + border_width,
			    allocation->width - border_width * 2, allocation->height - border_width * 2);

  child = GTK_BIN (widget)->child;
  if (child && GTK_WIDGET_VISIBLE (child))
    {
      child_allocation.x = GTK_WIDGET (widget)->style->xthickness + 1;
      child_allocation.y = GTK_CONTAINER (widget)->border_width + 1;
      child_allocation.width = MAX (1, (gint)allocation->width - child_allocation.x * 2 - border_width * 2 -
				    props.indicator_size.width - props.indicator_spacing.left - props.indicator_spacing.right -
				    CHILD_LEFT_SPACING - CHILD_RIGHT_SPACING - 2);
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2 - border_width * 2 -
				     CHILD_TOP_SPACING - CHILD_BOTTOM_SPACING - 2);
      child_allocation.x += CHILD_LEFT_SPACING;
      child_allocation.y += CHILD_TOP_SPACING;

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static void
gtk_option_menu_paint (GtkWidget    *widget,
		       GdkRectangle *area)
{
  GdkRectangle button_area;
  GtkOptionMenuProps props;
  gint border_width;

  g_return_if_fail (GTK_IS_OPTION_MENU (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      border_width = GTK_CONTAINER (widget)->border_width;
      gtk_option_menu_get_props (GTK_OPTION_MENU (widget), &props);

      button_area.x = 0;
      button_area.y = 0;
      button_area.width = widget->allocation.width - 2 * border_width;
      button_area.height = widget->allocation.height - 2 * border_width;

      if (!props.interior_focus)
	{
	  button_area.x += 1;
	  button_area.y += 1;
	  button_area.width -= 2;
	  button_area.height -= 2;

	  /* This is evil, and should be elimated here and in the button
	   * code. The point is to clear the focus, and make it
	   * sort of transparent if it isn't there.
	   */
	  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
	  gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);
	}

      gtk_paint_box (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget), GTK_SHADOW_OUT,
		     area, widget, "optionmenu",
		     button_area.x, button_area.y,
		     button_area.width, button_area.height);
      
      gtk_paint_tab (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget), GTK_SHADOW_OUT,
		     area, widget, "optionmenutab",
		     button_area.x + button_area.width - 
		     props.indicator_size.width - props.indicator_spacing.right -
		     widget->style->xthickness,
		     button_area.y + (button_area.height - props.indicator_size.height) / 2,
		     props.indicator_size.width, props.indicator_size.height);
      
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  if (props.interior_focus)
	    {
	      button_area.x += widget->style->xthickness + 1;
	      button_area.y += widget->style->ythickness + 1;
	      button_area.width -= 2 * (widget->style->xthickness + 1)
		+ props.indicator_spacing.left + props.indicator_spacing.right + props.indicator_size.width;
	      button_area.height -= 2 * (widget->style->ythickness + 1);
	    }
	  else
	    {
	      button_area.x -= 1;
	      button_area.y -= 1;
	      button_area.width += 2;
	      button_area.height += 2;
	    }
	    
	  gtk_paint_focus (widget->style, widget->window,
			   area, widget, "button",
			   button_area.x, 
			   button_area.y, 
			   button_area.width - 1,
			   button_area.height - 1);
	}
    }
}

static gint
gtk_option_menu_expose (GtkWidget      *widget,
			GdkEventExpose *event)
{
  g_return_val_if_fail (GTK_IS_OPTION_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_option_menu_paint (widget, &event->area);


      /* The following code tries to draw the child in two places at
       * once. It fails miserably for several reasons
       *
       * - If the child is not no-window, removing generates
       *   more expose events. Bad, bad, bad.
       * 
       * - Even if the child is no-window, removing it now (properly)
       *   clears the space where it was, so it does no good
       */
      
#if 0
      remove_child = FALSE;
      child = GTK_BUTTON (widget)->child;

      if (!child)
	{
	  if (!GTK_OPTION_MENU (widget)->menu)
	    return FALSE;
	  gtk_option_menu_update_contents (GTK_OPTION_MENU (widget));
	  child = GTK_BUTTON (widget)->child;
	  if (!child)
	    return FALSE;
	  remove_child = TRUE;
	}

      child_event = *event;

      if (GTK_WIDGET_NO_WINDOW (child) &&
	  gtk_widget_intersect (child, &event->area, &child_event.area))
	gtk_widget_event (child, (GdkEvent*) &child_event);

      if (remove_child)
	gtk_option_menu_remove_contents (GTK_OPTION_MENU (widget));
#else
      if (GTK_BIN (widget)->child)
	gtk_container_propagate_expose (GTK_CONTAINER (widget),
					GTK_BIN (widget)->child,
					event);
#endif /* 0 */
    }

  return FALSE;
}

static gint
gtk_option_menu_button_press (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkOptionMenu *option_menu;
  GtkWidget *menu_item;

  g_return_val_if_fail (GTK_IS_OPTION_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  option_menu = GTK_OPTION_MENU (widget);

  if ((event->type == GDK_BUTTON_PRESS) &&
      (event->button == 1))
    {
      gtk_option_menu_remove_contents (option_menu);
      gtk_menu_popup (GTK_MENU (option_menu->menu), NULL, NULL,
		      gtk_option_menu_position, option_menu,
		      event->button, event->time);
      menu_item = gtk_menu_get_active (GTK_MENU (option_menu->menu));
      if (menu_item)
	gtk_menu_shell_select_item (GTK_MENU_SHELL (option_menu->menu), menu_item);
      return TRUE;
    }

  return FALSE;
}

static gint
gtk_option_menu_key_press (GtkWidget   *widget,
			   GdkEventKey *event)
{
  GtkOptionMenu *option_menu;
  GtkWidget *menu_item;

  g_return_val_if_fail (GTK_IS_OPTION_MENU (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  option_menu = GTK_OPTION_MENU (widget);

  switch (event->keyval)
    {
    case GDK_KP_Space:
    case GDK_space:
      gtk_option_menu_remove_contents (option_menu);
      gtk_menu_popup (GTK_MENU (option_menu->menu), NULL, NULL,
		      gtk_option_menu_position, option_menu,
		      0, event->time);
      menu_item = gtk_menu_get_active (GTK_MENU (option_menu->menu));
      if (menu_item)
	gtk_menu_shell_select_item (GTK_MENU_SHELL (option_menu->menu), menu_item);
      return TRUE;
    }
  
  return FALSE;
}

static void
gtk_option_menu_selection_done (GtkMenuShell  *menu_shell,
				GtkOptionMenu *option_menu)
{
  g_return_if_fail (menu_shell != NULL);
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  gtk_option_menu_update_contents (option_menu);
}

static void
gtk_option_menu_changed (GtkOptionMenu *option_menu)
{
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  g_signal_emit (G_OBJECT (option_menu), signals[CHANGED], 0);
}

static void
gtk_option_menu_select_first_sensitive (GtkOptionMenu *option_menu)
{
  if (option_menu->menu)
    {
      GList *children = GTK_MENU_SHELL (option_menu->menu)->children;
      gint index = 0;

      while (children)
	{
	  if (GTK_WIDGET_SENSITIVE (children->data))
	    {
	      gtk_option_menu_set_history (option_menu, index);
	      return;
	    }
	  
	  children = children->next;
	  index++;
	}
    }
}

static void
gtk_option_menu_item_state_changed_cb (GtkWidget      *widget,
				       GtkStateType    previous_state,
				       GtkOptionMenu  *option_menu)
{
  GtkWidget *child = GTK_BIN (option_menu)->child;

  if (child && GTK_WIDGET_SENSITIVE (child) != GTK_WIDGET_IS_SENSITIVE (widget))
    gtk_widget_set_sensitive (child, GTK_WIDGET_IS_SENSITIVE (widget));
}

static void
gtk_option_menu_item_destroy_cb (GtkWidget     *widget,
				 GtkOptionMenu *option_menu)
{
  GtkWidget *child = GTK_BIN (option_menu)->child;

  if (child)
    {
      gtk_widget_ref (child);
      gtk_option_menu_remove_contents (option_menu);
      gtk_widget_destroy (child);
      gtk_widget_unref (child);

      gtk_option_menu_select_first_sensitive (option_menu);
    }
}

static void
gtk_option_menu_update_contents (GtkOptionMenu *option_menu)
{
  GtkWidget *child;
  GtkRequisition child_requisition;

  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  if (option_menu->menu)
    {
      GtkWidget *old_item = option_menu->menu_item;
      
      gtk_option_menu_remove_contents (option_menu);

      option_menu->menu_item = gtk_menu_get_active (GTK_MENU (option_menu->menu));
      if (option_menu->menu_item)
	{
	  gtk_widget_ref (option_menu->menu_item);
	  child = GTK_BIN (option_menu->menu_item)->child;
	  if (child)
	    {
	      if (!GTK_WIDGET_IS_SENSITIVE (option_menu->menu_item))
		gtk_widget_set_sensitive (child, FALSE);
	      gtk_widget_reparent (child, GTK_WIDGET (option_menu));
	    }

	  gtk_signal_connect (GTK_OBJECT (option_menu->menu_item), "state_changed",
			      GTK_SIGNAL_FUNC (gtk_option_menu_item_state_changed_cb), option_menu);
	  gtk_signal_connect (GTK_OBJECT (option_menu->menu_item), "destroy",
			      GTK_SIGNAL_FUNC (gtk_option_menu_item_destroy_cb), option_menu);

	  gtk_widget_size_request (child, &child_requisition);
	  gtk_widget_size_allocate (GTK_WIDGET (option_menu),
				    &(GTK_WIDGET (option_menu)->allocation));

	  if (GTK_WIDGET_DRAWABLE (option_menu))
	    gtk_widget_queue_draw (GTK_WIDGET (option_menu));
	}

      if (old_item != option_menu->menu_item)
        gtk_option_menu_changed (option_menu);
    }
}

static void
gtk_option_menu_remove_contents (GtkOptionMenu *option_menu)
{
  GtkWidget *child;
  
  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  if (option_menu->menu_item)
    {
      child = GTK_BIN (option_menu)->child;
  
      if (child)
	{
	  gtk_widget_set_sensitive (child, TRUE);
	  gtk_widget_reparent (child, option_menu->menu_item);
	}

      gtk_signal_disconnect_by_func (GTK_OBJECT (option_menu->menu_item),
				     GTK_SIGNAL_FUNC (gtk_option_menu_item_state_changed_cb),
				     option_menu);				     
      gtk_signal_disconnect_by_func (GTK_OBJECT (option_menu->menu_item),
				     GTK_SIGNAL_FUNC (gtk_option_menu_item_destroy_cb),
				     option_menu);   
      
      gtk_widget_unref (option_menu->menu_item);
      option_menu->menu_item = NULL;
    }
}

static void
gtk_option_menu_calc_size (GtkOptionMenu *option_menu)
{
  GtkWidget *child;
  GList *children;
  GtkRequisition child_requisition;
  gint old_width = option_menu->width;
  gint old_height = option_menu->height;

  g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));

  option_menu->width = 0;
  option_menu->height = 0;

  if (option_menu->menu)
    {
      children = GTK_MENU_SHELL (option_menu->menu)->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      gtk_widget_size_request (child, &child_requisition);

	      option_menu->width = MAX (option_menu->width, child_requisition.width);
	      option_menu->height = MAX (option_menu->height, child_requisition.height);
	    }
	}
    }

  if (old_width != option_menu->width || old_height != option_menu->height)
    gtk_widget_queue_resize (GTK_WIDGET (option_menu));
}

static void
gtk_option_menu_position (GtkMenu  *menu,
			  gint     *x,
			  gint     *y,
			  gboolean *push_in,
			  gpointer  user_data)
{
  GtkOptionMenu *option_menu;
  GtkWidget *active;
  GtkWidget *child;
  GtkRequisition requisition;
  GList *children;
  gint screen_width;
  gint menu_xpos;
  gint menu_ypos;
  gint menu_width;

  g_return_if_fail (GTK_IS_OPTION_MENU (user_data));

  option_menu = GTK_OPTION_MENU (user_data);

  gtk_widget_get_child_requisition (GTK_WIDGET (menu), &requisition);
  menu_width = requisition.width;

  active = gtk_menu_get_active (GTK_MENU (option_menu->menu));
  gdk_window_get_origin (GTK_WIDGET (option_menu)->window, &menu_xpos, &menu_ypos);

  menu_ypos += GTK_WIDGET (option_menu)->allocation.height / 2 - 2;

  if (active != NULL)
    {
      gtk_widget_get_child_requisition (active, &requisition);
      menu_ypos -= requisition.height / 2;
    }

  children = GTK_MENU_SHELL (option_menu->menu)->children;
  while (children)
    {
      child = children->data;

      if (active == child)
	break;

      if (GTK_WIDGET_VISIBLE (child))
	{
	  gtk_widget_get_child_requisition (child, &requisition);
	  menu_ypos -= requisition.height;
	}

      children = children->next;
    }

  screen_width = gdk_screen_width ();
  
  if (menu_xpos < 0)
    menu_xpos = 0;
  else if ((menu_xpos + menu_width) > screen_width)
    menu_xpos -= ((menu_xpos + menu_width) - screen_width);

  *x = menu_xpos;
  *y = menu_ypos;
  *push_in = TRUE;
}


static void
gtk_option_menu_show_all (GtkWidget *widget)
{
  GtkContainer *container;
  GtkOptionMenu *option_menu;
  
  g_return_if_fail (GTK_IS_OPTION_MENU (widget));
  container = GTK_CONTAINER (widget);
  option_menu = GTK_OPTION_MENU (widget);

  gtk_widget_show (widget);
  gtk_container_foreach (container, (GtkCallback) gtk_widget_show_all, NULL);
  if (option_menu->menu)
    gtk_widget_show_all (option_menu->menu);
  if (option_menu->menu_item)
    gtk_widget_show_all (option_menu->menu_item);
}


static void
gtk_option_menu_hide_all (GtkWidget *widget)
{
  GtkContainer *container;

  g_return_if_fail (GTK_IS_OPTION_MENU (widget));
  container = GTK_CONTAINER (widget);

  gtk_widget_hide (widget);
  gtk_container_foreach (container, (GtkCallback) gtk_widget_hide_all, NULL);
}

static gboolean
gtk_option_menu_mnemonic_activate (GtkWidget *widget,
				   gboolean   group_cycling)
{
  gtk_widget_grab_focus (widget);
  return TRUE;
}

static gint
gtk_option_menu_scroll_event (GtkWidget          *widget,
			      GdkEventScroll     *event)
{
  GtkOptionMenu *option_menu = GTK_OPTION_MENU (widget);
  gint index;
  gint n_children;
  gint index_dir;
  GList *l;
  GtkMenuItem *item;
    
  index = gtk_option_menu_get_history (option_menu);

  if (index != -1)
    {
      n_children = g_list_length (GTK_MENU_SHELL (option_menu->menu)->children);
      
      if (event->direction == GDK_SCROLL_UP)
	index_dir = -1;
      else
	index_dir = 1;


      while (TRUE)
	{
	  index += index_dir;

	  if (index < 0)
	    break;
	  if (index >= n_children)
	    break;

	  l = g_list_nth (GTK_MENU_SHELL (option_menu->menu)->children, index);
	  item = GTK_MENU_ITEM (l->data);
	  if (GTK_WIDGET_VISIBLE (item) && GTK_WIDGET_IS_SENSITIVE (item))
	    {
	      gtk_option_menu_set_history (option_menu, index);
	      break;
	    }
	      
	}
    }

  return TRUE;
}

