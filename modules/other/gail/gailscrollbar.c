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
#include "gailscrollbar.h"

static void	    gail_scrollbar_class_init        (GailScrollbarClass *klass);

static gint         gail_scrollbar_get_index_in_parent (AtkObject *accessible);

static GailRangeClass *parent_class = NULL;

GType
gail_scrollbar_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (GailScrollbarClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) gail_scrollbar_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (GailScrollbar), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };

    type = g_type_register_static (GAIL_TYPE_RANGE,
                                   "GailScrollbar", &tinfo, 0);
  }
  return type;
}

static void	 
gail_scrollbar_class_init (GailScrollbarClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_index_in_parent = gail_scrollbar_get_index_in_parent;

  parent_class = g_type_class_peek_parent (klass);
}

AtkObject* 
gail_scrollbar_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_SCROLLBAR (widget), NULL);

  object = g_object_new (GAIL_TYPE_SCROLLBAR, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  accessible->role = ATK_ROLE_SCROLL_BAR;

  return accessible;
}

static gint
gail_scrollbar_get_index_in_parent (AtkObject *accessible)
{
  GtkWidget *widget;
  GtkScrolledWindow *scrolled_window;
  gint n_children;
  GList *children;

  widget = GTK_ACCESSIBLE (accessible)->widget;

  if (widget == NULL)
  {
    /*
     * State is defunct
     */
    return -1;
  }
  g_return_val_if_fail (GTK_IS_SCROLLBAR (widget), -1);

  if (!GTK_IS_SCROLLED_WINDOW(widget->parent))
    return ATK_OBJECT_CLASS (parent_class)->get_index_in_parent (accessible);

  scrolled_window = GTK_SCROLLED_WINDOW (widget->parent);
  children = gtk_container_get_children (GTK_CONTAINER (scrolled_window));
  n_children = g_list_length (children);
  g_list_free (children);

  if (GTK_IS_HSCROLLBAR (widget))
  {
    if (!scrolled_window->hscrollbar_visible) 
    {
      n_children = -1;
    }
  }
  else if (GTK_IS_VSCROLLBAR (widget))
  {
    if (!scrolled_window->vscrollbar_visible) 
    {
      n_children = -1;
    }
    else if (scrolled_window->hscrollbar_visible) 
    {
      n_children++;
    }
  }
  else
  {
    n_children = -1;
  }
  return n_children;
} 
