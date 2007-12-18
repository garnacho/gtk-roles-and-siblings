/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#include "gailmenushell.h"

static void         gail_menu_shell_class_init          (GailMenuShellClass *klass);

static void         atk_selection_interface_init        (AtkSelectionIface  *iface);
static gboolean     gail_menu_shell_add_selection       (AtkSelection   *selection,
                                                         gint           i);
static gboolean     gail_menu_shell_clear_selection     (AtkSelection   *selection);
static AtkObject*   gail_menu_shell_ref_selection       (AtkSelection   *selection,
                                                         gint           i);
static gint         gail_menu_shell_get_selection_count (AtkSelection   *selection);
static gboolean     gail_menu_shell_is_child_selected   (AtkSelection   *selection,
                                                         gint           i);
static gboolean     gail_menu_shell_remove_selection    (AtkSelection   *selection,
                                                         gint           i);

GType
gail_menu_shell_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (GailMenuShellClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) gail_menu_shell_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (GailMenuShell), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };

    static const GInterfaceInfo atk_selection_info =
    {
        (GInterfaceInitFunc) atk_selection_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    type = g_type_register_static (GAIL_TYPE_CONTAINER,
                                   "GailMenuShell", &tinfo, 0);
    g_type_add_interface_static (type, ATK_TYPE_SELECTION,
                                 &atk_selection_info);
  }

  return type;
}

static void
gail_menu_shell_class_init (GailMenuShellClass *klass)
{
}

AtkObject*
gail_menu_shell_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), NULL);

  object = g_object_new (GAIL_TYPE_MENU_SHELL, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  if (GTK_IS_MENU_BAR (widget))
    accessible->role = ATK_ROLE_MENU_BAR;
  else
    /*
     * Accessible object for Menu is created in gailmenu.c
     */
    accessible->role = ATK_ROLE_UNKNOWN;

  return accessible;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->add_selection = gail_menu_shell_add_selection;
  iface->clear_selection = gail_menu_shell_clear_selection;
  iface->ref_selection = gail_menu_shell_ref_selection;
  iface->get_selection_count = gail_menu_shell_get_selection_count;
  iface->is_child_selected = gail_menu_shell_is_child_selected;
  iface->remove_selection = gail_menu_shell_remove_selection;
  /*
   * select_all_selection does not make sense for a menu_shell
   * so no implementation is provided.
   */
}

static gboolean
gail_menu_shell_add_selection (AtkSelection *selection,
                               gint          i)
{
  GtkMenuShell *shell;
  GList *item;
  guint length;
  GtkWidget *widget;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
  {
    /* State is defunct */
    return FALSE;
  }

  shell = GTK_MENU_SHELL (widget);
  length = g_list_length (shell->children);
  if (i < 0 || i > length)
    return FALSE;

  item = g_list_nth (shell->children, i);
  g_return_val_if_fail (item != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_ITEM(item->data), FALSE);
   
  gtk_menu_shell_select_item (shell, GTK_WIDGET (item->data));
  return TRUE;
}

static gboolean
gail_menu_shell_clear_selection (AtkSelection   *selection)
{
  GtkMenuShell *shell;
  GtkWidget *widget;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
  {
    /* State is defunct */
    return FALSE;
  }

  shell = GTK_MENU_SHELL (widget);

  gtk_menu_shell_deselect (shell);
  return TRUE;
}

static AtkObject*
gail_menu_shell_ref_selection (AtkSelection   *selection,
                               gint           i)
{
  GtkMenuShell *shell;
  AtkObject *obj;
  GtkWidget *widget;

  if (i != 0)
    return NULL;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
  {
    /* State is defunct */
    return NULL;
  }

  shell = GTK_MENU_SHELL (widget);
  
  if (shell->active_menu_item != NULL)
  {
    obj = gtk_widget_get_accessible (shell->active_menu_item);
    g_object_ref (obj);
    return obj;
  }
  else
  {
    return NULL;
  }
}

static gint
gail_menu_shell_get_selection_count (AtkSelection   *selection)
{
  GtkMenuShell *shell;
  GtkWidget *widget;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
  {
    /* State is defunct */
    return 0;
  }

  shell = GTK_MENU_SHELL (widget);

  /*
   * Identifies the currently selected menu item
   */
  if (shell->active_menu_item == NULL)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

static gboolean
gail_menu_shell_is_child_selected (AtkSelection   *selection,
                                   gint           i)
{
  GtkMenuShell *shell;
  gint j;
  GtkWidget *widget;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
  {
    /* State is defunct */
    return FALSE;
  }

  shell = GTK_MENU_SHELL (widget);
  if (shell->active_menu_item == NULL)
    return FALSE;
  
  j = g_list_index (shell->children, shell->active_menu_item);

  return (j==i);   
}

static gboolean
gail_menu_shell_remove_selection (AtkSelection   *selection,
                                  gint           i)
{
  GtkMenuShell *shell;
  GtkWidget *widget;

  if (i != 0)
    return FALSE;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
  {
    /* State is defunct */
    return FALSE;
  }

  shell = GTK_MENU_SHELL (widget);

  if (shell->active_menu_item && 
      GTK_MENU_ITEM (shell->active_menu_item)->submenu)
  {
    /*
     * Menu item contains a menu and it is the selected menu item
     * so deselect it.
     */
    gtk_menu_shell_deselect (shell);
  }
  return TRUE;
}
