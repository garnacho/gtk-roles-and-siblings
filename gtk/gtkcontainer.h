/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_CONTAINER_H__
#define __GTK_CONTAINER_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_CONTAINER(obj)          GTK_CHECK_CAST (obj, gtk_container_get_type (), GtkContainer)
#define GTK_CONTAINER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_container_get_type, GtkContainerClass)
#define GTK_IS_CONTAINER(obj)       GTK_CHECK_TYPE (obj, gtk_container_get_type ())

#define GTK_TYPE_CONTAINER          (gtk_container_get_type ())

typedef struct _GtkContainer       GtkContainer;
typedef struct _GtkContainerClass  GtkContainerClass;

struct _GtkContainer
{
  GtkWidget widget;

  GtkWidget *focus_child;

  gint16 border_width;
  guint auto_resize : 1;
  guint need_resize : 1;
  guint block_resize : 1;


  /* The list of children that requested a resize
   */
  GSList *resize_widgets;
};

struct _GtkContainerClass
{
  GtkWidgetClass parent_class;

  void (* add)         (GtkContainer     *container,
			GtkWidget        *widget);
  void (* remove)      (GtkContainer     *container,
			GtkWidget        *widget);
  gint (* need_resize) (GtkContainer     *container);
  void (* foreach)     (GtkContainer     *container,
			GtkCallback       callback,
			gpointer          callbabck_data);
  gint (* focus)       (GtkContainer     *container,
			GtkDirectionType  direction);
};



guint  gtk_container_get_type            (void);
void   gtk_container_border_width        (GtkContainer     *container,
					  gint              border_width);
void   gtk_container_add                 (GtkContainer     *container,
					  GtkWidget        *widget);
void   gtk_container_remove              (GtkContainer     *container,
					  GtkWidget        *widget);
void   gtk_container_disable_resize      (GtkContainer     *container);
void   gtk_container_enable_resize       (GtkContainer     *container);
void   gtk_container_block_resize        (GtkContainer     *container);
void   gtk_container_unblock_resize      (GtkContainer     *container);
gint   gtk_container_need_resize         (GtkContainer     *container);
void   gtk_container_foreach             (GtkContainer     *container,
					  GtkCallback       callback,
					  gpointer          callback_data);
void   gtk_container_foreach_interp      (GtkContainer     *container,
					  GtkCallbackMarshal marshal,
					  gpointer          callback_data,
					  GtkDestroyNotify  notify);
void   gtk_container_foreach_full        (GtkContainer     *container,
					  GtkCallback       callback,
					  GtkCallbackMarshal marshal,
					  gpointer          callback_data,
					  GtkDestroyNotify  notify);
gint   gtk_container_focus               (GtkContainer     *container,
					  GtkDirectionType  direction);
GList* gtk_container_children            (GtkContainer     *container);

void   gtk_container_register_toplevel   (GtkContainer     *container);
void   gtk_container_unregister_toplevel (GtkContainer     *container);
     

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_CONTAINER_H__ */
