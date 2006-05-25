/* gtkstatusicon.h:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __GTK_STATUS_ICON_H__
#define __GTK_STATUS_ICON_H__

#include <gtk/gtkimage.h>
#include <gtk/gtkmenu.h>

G_BEGIN_DECLS

#define GTK_TYPE_STATUS_ICON         (gtk_status_icon_get_type ())
#define GTK_STATUS_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STATUS_ICON, GtkStatusIcon))
#define GTK_STATUS_ICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_STATUS_ICON, GtkStatusIconClass))
#define GTK_IS_STATUS_ICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STATUS_ICON))
#define GTK_IS_STATUS_ICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_STATUS_ICON))
#define GTK_STATUS_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_STATUS_ICON, GtkStatusIconClass))

typedef struct _GtkStatusIcon	     GtkStatusIcon;
typedef struct _GtkStatusIconClass   GtkStatusIconClass;
typedef struct _GtkStatusIconPrivate GtkStatusIconPrivate;

struct _GtkStatusIcon
{
  GObject               parent_instance;

  GtkStatusIconPrivate *priv;
};

struct _GtkStatusIconClass
{
  GObjectClass parent_class;

  void     (* activate)     (GtkStatusIcon *status_icon);
  void     (* popup_menu)   (GtkStatusIcon *status_icon,
			     guint          button,
			     guint32        activate_time);
  gboolean (* size_changed) (GtkStatusIcon *status_icon,
			     gint           size);

  void (*__gtk_reserved1);
  void (*__gtk_reserved2);
  void (*__gtk_reserved3);
  void (*__gtk_reserved4);
  void (*__gtk_reserved5);
  void (*__gtk_reserved6);  
};

GType                 gtk_status_icon_get_type           (void) G_GNUC_CONST;

GtkStatusIcon        *gtk_status_icon_new                (void);
GtkStatusIcon        *gtk_status_icon_new_from_pixbuf    (GdkPixbuf          *pixbuf);
GtkStatusIcon        *gtk_status_icon_new_from_file      (const gchar        *filename);
GtkStatusIcon        *gtk_status_icon_new_from_stock     (const gchar        *stock_id);
GtkStatusIcon        *gtk_status_icon_new_from_icon_name (const gchar        *icon_name);

void                  gtk_status_icon_set_from_pixbuf    (GtkStatusIcon      *status_icon,
							  GdkPixbuf          *pixbuf);
void                  gtk_status_icon_set_from_file      (GtkStatusIcon      *status_icon,
							  const gchar        *filename);
void                  gtk_status_icon_set_from_stock     (GtkStatusIcon      *status_icon,
							  const gchar        *stock_id);
void                  gtk_status_icon_set_from_icon_name (GtkStatusIcon      *status_icon,
							  const gchar        *icon_name);

GtkImageType          gtk_status_icon_get_storage_type   (GtkStatusIcon      *status_icon);

GdkPixbuf            *gtk_status_icon_get_pixbuf         (GtkStatusIcon      *status_icon);
G_CONST_RETURN gchar *gtk_status_icon_get_stock          (GtkStatusIcon      *status_icon);
G_CONST_RETURN gchar *gtk_status_icon_get_icon_name      (GtkStatusIcon      *status_icon);

gint                  gtk_status_icon_get_size           (GtkStatusIcon      *status_icon);

void                  gtk_status_icon_set_tooltip        (GtkStatusIcon      *status_icon,
							  const gchar        *tooltip_text);

void                  gtk_status_icon_set_visible        (GtkStatusIcon      *status_icon,
							  gboolean            visible);
gboolean              gtk_status_icon_get_visible        (GtkStatusIcon      *status_icon);

void                  gtk_status_icon_set_blinking       (GtkStatusIcon      *status_icon,
							  gboolean            blinking);
gboolean              gtk_status_icon_get_blinking       (GtkStatusIcon      *status_icon);

gboolean              gtk_status_icon_is_embedded        (GtkStatusIcon      *status_icon);

void                  gtk_status_icon_position_menu      (GtkMenu            *menu,
							  gint               *x,
							  gint               *y,
							  gboolean           *push_in,
							  gpointer            user_data);
gboolean              gtk_status_icon_get_geometry       (GtkStatusIcon      *status_icon,
							  GdkScreen         **screen,
							  GdkRectangle       *area,
							  GtkOrientation     *orientation);

G_END_DECLS

#endif /* __GTK_STATUS_ICON_H__ */
