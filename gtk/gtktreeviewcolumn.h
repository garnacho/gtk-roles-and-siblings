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
#include <gtk/gtktreesortable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_TREE_VIEW_COLUMN	     (gtk_tree_view_column_get_type ())
#define GTK_TREE_VIEW_COLUMN(obj)	     (GTK_CHECK_CAST ((obj), GTK_TYPE_TREE_VIEW_COLUMN, GtkTreeViewColumn))
#define GTK_TREE_VIEW_COLUMN_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TREE_VIEW_COLUMN, GtkTreeViewColumnClass))
#define GTK_IS_TREE_VIEW_COLUMN(obj)	     (GTK_CHECK_TYPE ((obj), GTK_TYPE_TREE_VIEW_COLUMN))
#define GTK_IS_TREE_VIEW_COLUMN_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TREE_VIEW_COLUMN))
#define GTK_TREE_VIEW_COLUMN_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TREE_VIEW_COLUMN, GtkTreeViewColumnClass))

typedef enum
{
  GTK_TREE_VIEW_COLUMN_RESIZEABLE,
  GTK_TREE_VIEW_COLUMN_AUTOSIZE,
  GTK_TREE_VIEW_COLUMN_FIXED
} GtkTreeViewColumnSizing;

typedef struct _GtkTreeViewColumn      GtkTreeViewColumn;
typedef struct _GtkTreeViewColumnClass GtkTreeViewColumnClass;

typedef void (* GtkTreeCellDataFunc) (GtkTreeViewColumn *tree_column,
				      GtkCellRenderer   *cell,
				      GtkTreeModel      *tree_model,
				      GtkTreeIter       *iter,
				      gpointer           data);

  
struct _GtkTreeViewColumn
{
  GtkObject parent;

  GtkWidget *tree_view;
  GtkWidget *button;
  GtkWidget *child;  
  GtkWidget *arrow;
  GtkWidget *alignment;
  GdkWindow *window;
  gfloat xalign;
  guint property_changed_signal;

  gint width;
  gint requested_width;
  gint min_width;
  gint max_width;
  gint displayed_width;

  /* dragging columns */
  gint drag_x;
  gint drag_y;

  GtkTreeCellDataFunc func;
  gpointer func_data;
  GtkDestroyNotify destroy;
  gchar *title;
  GtkCellRenderer *cell;
  GSList *attributes;
  GtkTreeViewColumnSizing column_type;

  /* Sorting */
  guint sort_clicked_signal;
  guint sort_column_changed_signal;
  gint sort_column_id;
  GtkTreeSortOrder sort_order;

  guint visible             : 1;
  guint clickable           : 1;
  guint dirty               : 1;
  guint show_sort_indicator : 1;
  guint maybe_reordered     : 1;
  guint reorderable         : 1;
};

struct _GtkTreeViewColumnClass
{
  GtkObjectClass parent_class;

  void (*clicked) (GtkTreeViewColumn *tree_column);
};

GtkType                 gtk_tree_view_column_get_type            (void);
GtkTreeViewColumn    *  gtk_tree_view_column_new                 (void);
GtkTreeViewColumn    *  gtk_tree_view_column_new_with_attributes (const gchar             *title,
								  GtkCellRenderer         *cell,
								  ...);
void                    gtk_tree_view_column_set_cell_renderer   (GtkTreeViewColumn       *tree_column,
								  GtkCellRenderer         *cell);
GtkCellRenderer      *  gtk_tree_view_column_get_cell_renderer   (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_add_attribute       (GtkTreeViewColumn       *tree_column,
								  const gchar             *attribute,
								  gint                     column);
void                    gtk_tree_view_column_set_attributes      (GtkTreeViewColumn       *tree_column,
								  ...);
void                    gtk_tree_view_column_set_cell_data_func  (GtkTreeViewColumn       *tree_column,
								  GtkTreeCellDataFunc      func,
								  gpointer                 func_data,
								  GtkDestroyNotify         destroy);
void                    gtk_tree_view_column_clear_attributes    (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_set_visible         (GtkTreeViewColumn       *tree_column,
								  gboolean                 visible);
gboolean                gtk_tree_view_column_get_visible         (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_set_sizing          (GtkTreeViewColumn       *tree_column,
								  GtkTreeViewColumnSizing  type);
GtkTreeViewColumnSizing gtk_tree_view_column_get_sizing          (GtkTreeViewColumn       *tree_column);
gint                    gtk_tree_view_column_get_width           (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_set_width           (GtkTreeViewColumn       *tree_column,
								  gint                     size);
void                    gtk_tree_view_column_set_min_width       (GtkTreeViewColumn       *tree_column,
								  gint                     min_width);
gint                    gtk_tree_view_column_get_min_width       (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_set_max_width       (GtkTreeViewColumn       *tree_column,
								  gint                     max_width);
gint                    gtk_tree_view_column_get_max_width       (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_clicked             (GtkTreeViewColumn       *tree_column);



/* Options for manipulating the column headers
 */
void                    gtk_tree_view_column_set_title           (GtkTreeViewColumn       *tree_column,
								  const gchar             *title);
G_CONST_RETURN gchar *  gtk_tree_view_column_get_title           (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_set_clickable       (GtkTreeViewColumn       *tree_column,
								  gboolean                 clickable);
gboolean                gtk_tree_view_column_get_clickable       (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_set_widget          (GtkTreeViewColumn       *tree_column,
								  GtkWidget               *widget);
GtkWidget            *  gtk_tree_view_column_get_widget          (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_set_alignment       (GtkTreeViewColumn       *tree_column,
								  gfloat                   xalign);
gfloat                  gtk_tree_view_column_get_alignment       (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_set_reorderable     (GtkTreeViewColumn       *tree_column,
								  gboolean                 reorderable);
gboolean                gtk_tree_view_column_get_reorderable     (GtkTreeViewColumn       *tree_column);



/* You probably only want to use gtk_tree_view_column_set_sort_column_id.  The
 * other sorting functions exist primarily to let others do their own custom sorting.
 */
void                    gtk_tree_view_column_set_sort_column_id  (GtkTreeViewColumn       *tree_column,
								  gint                     sort_column_id);
void                    gtk_tree_view_column_set_sort_indicator  (GtkTreeViewColumn       *tree_column,
								  gboolean                 setting);
gboolean                gtk_tree_view_column_get_sort_indicator  (GtkTreeViewColumn       *tree_column);
void                    gtk_tree_view_column_set_sort_order      (GtkTreeViewColumn       *tree_column,
								  GtkTreeSortOrder         order);
GtkTreeSortOrder        gtk_tree_view_column_get_sort_order      (GtkTreeViewColumn       *tree_column);




/* These functions are meant primarily for interaction between the GtkTreeView and the column.
 */
void                    gtk_tree_view_column_cell_set_cell_data  (GtkTreeViewColumn       *tree_column,
								  GtkTreeModel            *tree_model,
								  GtkTreeIter             *iter);
void                    gtk_tree_view_column_cell_get_size       (GtkTreeViewColumn       *tree_column,
								  GdkRectangle            *cell_area,
								  gint                    *x_offset,
								  gint                    *y_offset,
								  gint                    *width,
								  gint                    *height);
void                    gtk_tree_view_column_cell_render         (GtkTreeViewColumn       *tree_column,
								  GdkWindow               *window,
								  GdkRectangle            *background_area,
								  GdkRectangle            *cell_area,
								  GdkRectangle            *expose_area,
								  guint                    flags);
gboolean                gtk_tree_view_column_cell_event          (GtkTreeViewColumn       *tree_column,
								  GdkEvent                *event,
								  gchar                   *path_string,
								  GdkRectangle            *background_area,
								  GdkRectangle            *cell_area,
								  guint                    flags);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_TREE_VIEW_COLUMN_H__ */

