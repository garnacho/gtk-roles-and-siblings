/* GdkPixbuf library - Io handling.  This is an internal header for 
 * GdkPixbuf. You should never use it unless you are doing development for 
 * GdkPixbuf itself.
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
 *          Michael Fulbright <drmike@redhat.com>
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

#ifndef GDK_PIXBUF_IO_H
#define GDK_PIXBUF_IO_H

#include "gdk-pixbuf/gdk-pixbuf.h"
#include <gmodule.h>
#include <stdio.h>

G_BEGIN_DECLS

#ifdef GDK_PIXBUF_ENABLE_BACKEND



typedef void (* GdkPixbufModuleSizeFunc)           (gint *width, 
						    gint *height, 
						    gpointer user_data);

typedef void (* GdkPixbufModulePreparedFunc) (GdkPixbuf          *pixbuf,
					      GdkPixbufAnimation *anim,
					      gpointer            user_data);
typedef void (* GdkPixbufModuleUpdatedFunc)  (GdkPixbuf *pixbuf,
					      int        x,
					      int        y,
					      int        width,
					      int        height,
					      gpointer   user_data);

typedef struct _GdkPixbufModulePattern GdkPixbufModulePattern;
struct _GdkPixbufModulePattern {
	unsigned char *prefix;
	unsigned char *mask;
	int relevance;
};

typedef struct _GdkPixbufModule GdkPixbufModule;
struct _GdkPixbufModule {
	char *module_name;
	char *module_path;
	GModule *module;
	GdkPixbufFormat *info;
	
        GdkPixbuf *(* load) (FILE    *f,
                             GError **error);
        GdkPixbuf *(* load_xpm_data) (const char **data);

        /* Incremental loading */

        gpointer (* begin_load)     (GdkPixbufModuleSizeFunc size_func,
                                     GdkPixbufModulePreparedFunc prepare_func,
                                     GdkPixbufModuleUpdatedFunc update_func,
                                     gpointer user_data,
                                     GError **error);
        gboolean (* stop_load)      (gpointer context,
                                     GError **error);
        gboolean (* load_increment) (gpointer      context,
                                     const guchar *buf,
                                     guint         size,
                                     GError      **error);

	/* Animation loading */
	GdkPixbufAnimation *(* load_animation) (FILE    *f,
                                                GError **error);

        gboolean (* save) (FILE      *f,
                           GdkPixbuf *pixbuf,
                           gchar    **param_keys,
                           gchar    **param_values,
                           GError   **error);
  
  /*< private >*/
	void (*_reserved1) (void); 
	void (*_reserved2) (void); 
	void (*_reserved3) (void); 
	void (*_reserved4) (void); 
	void (*_reserved5) (void); 
	void (*_reserved6) (void); 

};

typedef void (* GdkPixbufModuleFillVtableFunc) (GdkPixbufModule *module);
typedef void (* GdkPixbufModuleFillInfoFunc) (GdkPixbufFormat *info);

/*  key/value pairs that can be attached by the pixbuf loader  */

gboolean gdk_pixbuf_set_option  (GdkPixbuf   *pixbuf,
                                 const gchar *key,
                                 const gchar *value);

typedef enum /*< skip >*/
{
  GDK_PIXBUF_FORMAT_WRITABLE = 1 << 0
} GdkPixbufFormatFlags;

struct _GdkPixbufFormat {
  gchar *name;
  GdkPixbufModulePattern *signature;
  gchar *domain;
  gchar *description;
  gchar **mime_types;
  gchar **extensions;
  guint32 flags;
};


#endif /* GDK_PIXBUF_ENABLE_BACKEND */

G_END_DECLS

#endif /* GDK_PIXBUF_IO_H */
