/* GdkPixbuf library - Main header file
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
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

#ifndef GDK_PIXBUF_LOADER_H
#define GDK_PIXBUF_LOADER_H

#include <unistd.h>
#include <gtk/gtkobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#endif



#define GDK_TYPE_PIXBUF_LOADER		   (gdk_pixbuf_loader_get_type ())
#define GDK_PIXBUF_LOADER(obj)		   (GTK_CHECK_CAST ((obj), GDK_TYPE_PIXBUF_LOADER, GdkPixbufLoader))
#define GDK_PIXBUF_LOADER_CLASS(klass)	   (GTK_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF_LOADER, GdkPixbufLoaderClass))
#define GDK_IS_PIXBUF_LOADER(obj)	   (GTK_CHECK_TYPE ((obj), GDK_TYPE_PIXBUF_LOADER))
#define GDK_IS_PIXBUF_LOADER_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF_LOADER))


typedef struct _GdkPixbufLoader GdkPixbufLoader;
struct _GdkPixbufLoader
{
	GtkObject object;

	/* < Private > */
	gpointer private;
};

typedef struct _GdkPixbufLoaderClass GdkPixbufLoaderClass;
struct _GdkPixbufLoaderClass {
	GtkObjectClass parent_class;

	void (* area_prepared)   (GdkPixbufLoader *loader);

	void (* area_updated)    (GdkPixbufLoader *loader,
                                  guint x, guint y, guint width, guint height);

	void (* frame_done)      (GdkPixbufLoader *loader, GdkPixbufFrame *frame);

	void (* animation_done)  (GdkPixbufLoader *loader);

	void (* closed)          (GdkPixbufLoader *loader);
};



GtkType             gdk_pixbuf_loader_get_type      (void);
GdkPixbufLoader    *gdk_pixbuf_loader_new           (void);
gboolean            gdk_pixbuf_loader_write         (GdkPixbufLoader *loader,
						     const guchar    *buf,
						     size_t           count);
GdkPixbuf          *gdk_pixbuf_loader_get_pixbuf    (GdkPixbufLoader *loader);
GdkPixbufAnimation *gdk_pixbuf_loader_get_animation (GdkPixbufLoader *loader);
void                gdk_pixbuf_loader_close         (GdkPixbufLoader *loader);



#ifdef __cplusplus
}
#endif

#endif
