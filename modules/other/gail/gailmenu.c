/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkcombobox.h>
#include "gailmenu.h"

static void gail_menu_class_init (GailMenuClass *klass);

static void	  gail_menu_real_initialize     (AtkObject *obj,
                                                 gpointer  data);

static AtkObject* gail_menu_get_parent          (AtkObject *accessible);
static gint       gail_menu_get_index_in_parent (AtkObject *accessible);

static GailMenuShell *parent_class = NULL;

GType
gail_menu_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo tinfo =
      {
        sizeof (GailMenuClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) gail_menu_class_init, /* class init */
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (GailMenu), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL, /* instance init */
        NULL /* value table */
      };

      type = g_type_register_static (GAIL_TYPE_MENU_SHELL,
                                     "GailMenu", &tinfo, 0);
    }

  return type;
}

static void
gail_menu_class_init (GailMenuClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_parent = gail_menu_get_parent;
  class->get_index_in_parent = gail_menu_get_index_in_parent;
  class->initialize = gail_menu_real_initialize;

  parent_class = g_type_class_peek_parent (klass);
}

AtkObject*
gail_menu_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_MENU (widget), NULL);

  object = g_object_new (GAIL_TYPE_MENU, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  g_object_set_data (G_OBJECT (accessible), "atk-component-layer",
		     GINT_TO_POINTER (ATK_LAYER_POPUP));
  return accessible;
}

static void
gail_menu_real_initialize (AtkObject *obj,
                           gpointer  data)
{
  ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_MENU;
}

static AtkObject*
gail_menu_get_parent (AtkObject *accessible)
{
  AtkObject *parent;

  parent = accessible->accessible_parent;

  if (parent != NULL)
    {
      g_return_val_if_fail (ATK_IS_OBJECT (parent), NULL);
    }
  else
    {
      GtkWidget *widget, *parent_widget;

      widget = GTK_ACCESSIBLE (accessible)->widget;
      if (widget == NULL)
        {
          /*
           * State is defunct
           */
          return NULL;
        }
      g_return_val_if_fail (GTK_IS_MENU (widget), NULL);

      /*
       * If the menu is attached to a menu item or a button (Gnome Menu)
       * report the menu item as parent.
       */
      parent_widget = gtk_menu_get_attach_widget (GTK_MENU (widget));

      if (!GTK_IS_MENU_ITEM (parent_widget) && !GTK_IS_BUTTON (parent_widget) && !GTK_IS_COMBO_BOX (parent_widget) && !GTK_IS_OPTION_MENU (parent_widget))
        parent_widget = widget->parent;

      if (parent_widget == NULL)
        return NULL;

      parent = gtk_widget_get_accessible (parent_widget);
      atk_object_set_parent (accessible, parent);
    }
  return parent;
}

static gint
gail_menu_get_index_in_parent (AtkObject *accessible)
{
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (accessible)->widget;

  if (widget == NULL)
    {
      /*
       * State is defunct
       */
      return -1;
    }
  g_return_val_if_fail (GTK_IS_MENU (widget), -1);

  if (gtk_menu_get_attach_widget (GTK_MENU (widget)))
    {
      return 0;
    }
  return ATK_OBJECT_CLASS (parent_class)->get_index_in_parent (accessible);
}
