/* gtktoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@codefactory.se>
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

#ifndef __GTK_TOOL_ITEM_H__
#define __GTK_TOOL_ITEM_H__

#include <gtk/gtkbin.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkmenuitem.h>

#define GTK_TYPE_TOOL_ITEM            (gtk_tool_item_get_type ())
#define GTK_TOOL_ITEM(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_TOOL_ITEM, GtkToolItem))
#define GTK_TOOL_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TOOL_ITEM, GtkToolItemClass))
#define GTK_IS_TOOL_ITEM(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_TOOL_ITEM))
#define GTK_IS_TOOL_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((o), GTK_TYPE_TOOL_ITEM))
#define GTK_TOOL_ITEM_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS((o), GTK_TYPE_TOOL_ITEM, GtkToolItemClass))
#define GTK_TOOL_ITEM_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_TOOL_ITEM, GtkToolItemPrivate))

typedef struct _GtkToolItem        GtkToolItem;
typedef struct _GtkToolItemClass   GtkToolItemClass;
typedef struct _GtkToolItemPrivate GtkToolItemPrivate;

struct _GtkToolItem
{
  GtkBin parent;

  GtkToolItemPrivate *priv;
};

struct _GtkToolItemClass
{
  GtkBinClass parent_class;

  /* signals */
  gboolean   (* create_menu_proxy)    (GtkToolItem *tool_item);
  void       (* toolbar_reconfigured) (GtkToolItem *tool_item);
  gboolean   (* set_tooltip)	      (GtkToolItem *tool_item,
				       GtkTooltips *tooltips,
				       const gchar *tip_text,
				       const gchar *tip_private);
};

GType        gtk_tool_item_get_type (void);
GtkToolItem *gtk_tool_item_new      (void);

void            gtk_tool_item_toolbar_reconfigured     (GtkToolItem *tool_item);

void            gtk_tool_item_set_homogeneous          (GtkToolItem *tool_item,
							gboolean     homogeneous);
gboolean	gtk_tool_item_get_homogeneous          (GtkToolItem *tool_item);

void            gtk_tool_item_set_expand               (GtkToolItem *tool_item,
							gboolean     expand);
gboolean        gtk_tool_item_get_expand               (GtkToolItem *tool_item);

void            gtk_tool_item_set_pack_end             (GtkToolItem *tool_item,
							gboolean     pack_end);
gboolean        gtk_tool_item_get_pack_end             (GtkToolItem *tool_item);

void            gtk_tool_item_set_tooltip              (GtkToolItem *tool_item,
							GtkTooltips *tooltips,
							const gchar *tip_text,
							const gchar *tip_private);

void            gtk_tool_item_set_use_drag_window      (GtkToolItem *toolitem,
							gboolean     use_drag_window);
gboolean        gtk_tool_item_get_use_drag_window      (GtkToolItem *toolitem);

void            gtk_tool_item_set_visible_horizontal   (GtkToolItem *toolitem,
							gboolean     visible_horizontal);
gboolean        gtk_tool_item_get_visible_horizontal   (GtkToolItem *toolitem);

void            gtk_tool_item_set_visible_vertical     (GtkToolItem *toolitem,
							gboolean     visible_horizontal);
gboolean        gtk_tool_item_get_visible_vertical     (GtkToolItem *toolitem);

GtkIconSize     gtk_tool_item_get_icon_size            (GtkToolItem *tool_item);
GtkOrientation  gtk_tool_item_get_orientation          (GtkToolItem *tool_item);
GtkToolbarStyle gtk_tool_item_get_toolbar_style        (GtkToolItem *tool_item);
GtkReliefStyle  gtk_tool_item_get_relief_style         (GtkToolItem *tool_item);

GtkWidget *     gtk_tool_item_retrieve_proxy_menu_item (GtkToolItem *tool_item);
GtkWidget *     gtk_tool_item_get_proxy_menu_item      (GtkToolItem *tool_item,
							const gchar *menu_item_id);
void            gtk_tool_item_set_proxy_menu_item      (GtkToolItem *tool_item,
							const gchar *menu_item_id,
							GtkWidget   *menu_item);

/* internal function */
GdkWindow *_gtk_tool_item_get_drag_window (GtkToolItem *tool_item);


#endif /* __GTK_TOOL_ITEM_H__ */
