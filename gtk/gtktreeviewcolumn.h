/* gtktreeviewcolumn.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_TREE_VIEW_COLUMN_H__
#define __GTK_TREE_VIEW_COLUMN_H__

#include <gtk/gtkobject.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtktreemodel.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_COLUMN		     (gtk_tree_view_column_get_type ())
#define GTK_TREE_VIEW_COLUMN(obj)	     (GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_COLUMN, GtkTreeViewColumn))
#define GTK_TREE_VIEW_COLUMN_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_COLUMN, GtkTreeViewColumnClass))
#define GTK_IS_TREE_VIEW_COLUMN(obj)	     (GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_COLUMN))
#define GTK_IS_TREE_VIEW_COLUMN_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_COLUMN))

typedef enum
{
  GTK_TREE_VIEW_COLUMN_RESIZEABLE,
  GTK_TREE_VIEW_COLUMN_AUTOSIZE,
  GTK_TREE_VIEW_COLUMN_FIXED
} GtkTreeViewColumnType;

typedef struct _GtkTreeViewColumn      GtkTreeViewColumn;
typedef struct _GtkTreeViewColumnClass GtkTreeViewColumnClass;

typedef gboolean (* GtkTreeViewColumnFunc) (GtkTreeViewColumn *tree_column,
					    GtkTreeModel      *tree_model,
					    GtkTreeNode        tree_node,
					    gpointer           data);

struct _GtkTreeViewColumn
{
  GtkObject parent;

  GtkWidget *tree_view;
  GtkWidget *button;
  GdkWindow *window;
  GtkJustification justification;

  gint id;
  gint size;
  gint min_width;
  gint max_width;

  GtkTreeViewColumnFunc *func;
  gpointer func_data;
  gchar *title;
  GtkCellRenderer *cell;
  GSList *attributes;
  GtkTreeViewColumnType column_type;
  guint visible       : 1;
  guint button_active : 1;
  guint dirty         : 1;
};

struct _GtkTreeViewColumnClass
{
  GtkObjectClass parent_class;

  void (*clicked) (GtkTreeViewColumn *tree_column);
};


GtkType          gtk_tree_view_column_get_type            (void);
GtkObject       *gtk_tree_view_column_new                 (void);
GtkObject       *gtk_tree_view_column_new_with_attributes (gchar                 *title,
							   GtkCellRenderer       *cell,
							   ...);
void             gtk_tree_view_column_set_cell_renderer   (GtkTreeViewColumn     *tree_column,
							   GtkCellRenderer       *cell);
void             gtk_tree_view_column_add_attribute       (GtkTreeViewColumn     *tree_column,
							   gchar                 *attribute,
							   gint                   column);
void             gtk_tree_view_column_set_attributes      (GtkTreeViewColumn     *tree_column,
							   ...);
void             gtk_tree_view_column_set_cell_data       (GtkTreeViewColumn     *tree_column,
							   GtkTreeModel          *tree_model,
							   GtkTreeNode            tree_node);
void             gtk_tree_view_column_set_visible         (GtkTreeViewColumn     *tree_column,
							   gboolean               visible);
gboolean         gtk_tree_view_column_get_visible         (GtkTreeViewColumn     *tree_column);
void             gtk_tree_view_column_set_col_type        (GtkTreeViewColumn     *tree_column,
							   GtkTreeViewColumnType  type);
gint             gtk_tree_view_column_get_col_type        (GtkTreeViewColumn     *tree_column);
gint             gtk_tree_view_column_get_preferred_size  (GtkTreeViewColumn     *tree_column);
gint             gtk_tree_view_column_get_size            (GtkTreeViewColumn     *tree_column);
void             gtk_tree_view_column_set_size            (GtkTreeViewColumn     *tree_column,
							   gint                   width);
void             gtk_tree_view_column_set_min_width       (GtkTreeViewColumn     *tree_column,
							   gint                   min_width);
gint             gtk_tree_view_column_get_min_width       (GtkTreeViewColumn     *tree_column);
void             gtk_tree_view_column_set_max_width       (GtkTreeViewColumn     *tree_column,
							   gint                   max_width);
gint             gtk_tree_view_column_get_max_width       (GtkTreeViewColumn     *tree_column);


/* Options for manipulating the column headers
 */
void             gtk_tree_view_column_set_title           (GtkTreeViewColumn     *tree_column,
							   gchar                 *title);
gchar           *gtk_tree_view_column_get_title           (GtkTreeViewColumn     *tree_column);
void             gtk_tree_view_column_set_header_active   (GtkTreeViewColumn     *tree_column,
							   gboolean               active);
void             gtk_tree_view_column_set_widget          (GtkTreeViewColumn     *tree_column,
							   GtkWidget             *widget);
GtkWidget       *gtk_tree_view_column_get_widget          (GtkTreeViewColumn     *tree_column);
void             gtk_tree_view_column_set_justification   (GtkTreeViewColumn     *tree_column,
							   GtkJustification       justification);
GtkJustification gtk_tree_view_column_get_justification   (GtkTreeViewColumn     *tree_column);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_TREE_VIEW_COLUMN_H__ */
