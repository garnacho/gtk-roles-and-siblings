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
#ifndef __GTK_LABEL_H__
#define __GTK_LABEL_H__


#include <gdk/gdk.h>
#include <gtk/gtkmisc.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_LABEL(obj)          GTK_CHECK_CAST (obj, gtk_label_get_type (), GtkLabel)
#define GTK_LABEL_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_label_get_type (), GtkLabelClass)
#define GTK_IS_LABEL(obj)       GTK_CHECK_TYPE (obj, gtk_label_get_type ())


typedef struct _GtkLabel       GtkLabel;
typedef struct _GtkLabelClass  GtkLabelClass;

struct _GtkLabel
{
  GtkMisc misc;

  char *label;
  GSList *row;
  guint jtype : 2;
  gint  needs_clear : 1;
};

struct _GtkLabelClass
{
  GtkMiscClass parent_class;
};


guint      gtk_label_get_type    (void);
GtkWidget* gtk_label_new         (const char        *str);
void       gtk_label_set         (GtkLabel          *label,
                                  const char        *str);
void       gtk_label_set_justify (GtkLabel          *label,
                                  GtkJustification   jtype);
void       gtk_label_get         (GtkLabel          *label,
                                  char             **str);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_LABEL_H__ */
