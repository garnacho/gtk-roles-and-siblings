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
#ifndef __GTK_HPANED_H__
#define __GTK_HPANED_H__


#include <gdk/gdk.h>
#include <gtk/gtkpaned.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_HPANED(obj)          GTK_CHECK_CAST (obj, gtk_hpaned_get_type (), GtkHPaned)
#define GTK_HPANED_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_hpaned_get_type (), GtkHPanedClass)
#define GTK_IS_HPANED(obj)       GTK_CHECK_TYPE (obj, gtk_hpaned_get_type ())


typedef struct _GtkHPaned       GtkHPaned;
typedef struct _GtkHPanedClass  GtkHPanedClass;

struct _GtkHPaned
{
  GtkPaned paned;
};

struct _GtkHPanedClass
{
  GtkPanedClass parent_class;
};


guint      gtk_hpaned_get_type (void);
GtkWidget* gtk_hpaned_new      (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_HPANED_H__ */
