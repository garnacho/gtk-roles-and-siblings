/* gtkliststore.h
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

#ifndef __GTK_LIST_STORE_H__
#define __GTK_LIST_STORE_H__

#include <gtk/gtktreemodel.h>
#include <gtk/gtktreesortable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_LIST_STORE	       (gtk_list_store_get_type ())
#define GTK_LIST_STORE(obj)	       (GTK_CHECK_CAST ((obj), GTK_TYPE_LIST_STORE, GtkListStore))
#define GTK_LIST_STORE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_LIST_STORE, GtkListStoreClass))
#define GTK_IS_LIST_STORE(obj)	       (GTK_CHECK_TYPE ((obj), GTK_TYPE_LIST_STORE))
#define GTK_IS_LIST_STORE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_LIST_STORE))
#define GTK_LIST_STORE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_LIST_STORE, GtkListStoreClass))

typedef struct _GtkListStore       GtkListStore;
typedef struct _GtkListStoreClass  GtkListStoreClass;

struct _GtkListStore
{
  GObject parent;

  /*< private >*/
  gint stamp;
  gpointer root;
  gpointer tail;
  GList *sort_list;
  gint n_columns;
  gint sort_column_id;
  GtkSortType order;
  GType *column_headers;
  gint length;
  GtkTreeIterCompareFunc default_sort_func;
  gpointer default_sort_data;
  GtkDestroyNotify default_sort_destroy;
  guint columns_dirty : 1;
};

struct _GtkListStoreClass
{
  GObjectClass parent_class;
};


GtkType       gtk_list_store_get_type         (void);
GtkListStore *gtk_list_store_new              (gint          n_columns,
					       ...);
GtkListStore *gtk_list_store_newv             (gint          n_columns,
					       GType        *types);
void          gtk_list_store_set_column_types (GtkListStore *list_store,
					       gint          n_columns,
					       GType        *types);
void          gtk_list_store_set_value        (GtkListStore *list_store,
					       GtkTreeIter  *iter,
					       gint          column,
					       GValue       *value);
void          gtk_list_store_set              (GtkListStore *list_store,
					       GtkTreeIter  *iter,
					       ...);
void          gtk_list_store_set_valist       (GtkListStore *list_store,
					       GtkTreeIter  *iter,
					       va_list       var_args);
void          gtk_list_store_remove           (GtkListStore *list_store,
					       GtkTreeIter  *iter);
void          gtk_list_store_insert           (GtkListStore *list_store,
					       GtkTreeIter  *iter,
					       gint          position);
void          gtk_list_store_insert_before    (GtkListStore *list_store,
					       GtkTreeIter  *iter,
					       GtkTreeIter  *sibling);
void          gtk_list_store_insert_after     (GtkListStore *list_store,
					       GtkTreeIter  *iter,
					       GtkTreeIter  *sibling);
void          gtk_list_store_prepend          (GtkListStore *list_store,
					       GtkTreeIter  *iter);
void          gtk_list_store_append           (GtkListStore *list_store,
					       GtkTreeIter  *iter);
void          gtk_list_store_clear            (GtkListStore *list_store);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_LIST_STORE_H__ */
