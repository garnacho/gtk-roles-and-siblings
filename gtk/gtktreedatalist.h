/* gtktreedatalist.h
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


#ifndef __GTK_TREE_DATA_LIST_H__
#define __GTK_TREE_DATA_LIST_H__

#include <glib.h>
#include <glib-object.h>
#include "gtktreesortable.h"

typedef struct _GtkTreeDataList GtkTreeDataList;
struct _GtkTreeDataList
{
  GtkTreeDataList *next;

  union {
    gint	   v_int;
    gint8          v_char;
    guint8         v_uchar;
    guint	   v_uint;
    gfloat	   v_float;
    gdouble        v_double;
    gpointer	   v_pointer;
  } data;
};

typedef struct _GtkTreeDataSortHeader
{
  gint sort_column_id;
  GtkTreeIterCompareFunc func;
  gpointer data;
  GtkDestroyNotify destroy;
} GtkTreeDataSortHeader;

GtkTreeDataList *_gtk_tree_data_list_alloc          (void);
void             _gtk_tree_data_list_free           (GtkTreeDataList *list,
						     GType           *column_headers);
gboolean         _gtk_tree_data_list_check_type     (GType            type);
void             _gtk_tree_data_list_node_to_value  (GtkTreeDataList *list,
						     GType            type,
						     GValue          *value);
void             _gtk_tree_data_list_value_to_node  (GtkTreeDataList *list,
						     GValue          *value);

GtkTreeDataList *_gtk_tree_data_list_node_copy      (GtkTreeDataList *list,
                                                     GType            type);

/* Header code */
GList *                _gtk_tree_data_list_header_new  (gint   n_columns,
							GType *types);
void                   _gtk_tree_data_list_header_free (GList *header_list);
GtkTreeDataSortHeader *_gtk_tree_data_list_get_header  (GList *header_list,
							gint   sort_column_id);


#endif /* __GTK_TREE_DATA_LIST_H__ */
