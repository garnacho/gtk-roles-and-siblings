/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_PRIVATE_H__
#define __GDK_PRIVATE_H__

#include <gdk/gdktypes.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkfont.h>
#include <gdk/gdkgc.h>
#include <gdk/gdkimage.h>
#include <gdk/gdkregion.h>
#include <gdk/gdkvisual.h>
#include <gdk/gdkwindow.h>

G_BEGIN_DECLS

#define GDK_PARENT_RELATIVE_BG ((GdkPixmap *)1L)
#define GDK_NO_BG ((GdkPixmap *)2L)

#define GDK_WINDOW_TYPE(d) (((GdkWindowObject*)(GDK_WINDOW (d)))->window_type)
#define GDK_WINDOW_DESTROYED(d) (((GdkWindowObject*)(GDK_WINDOW (d)))->destroyed)

typedef struct _GdkColorInfo           GdkColorInfo;
typedef struct _GdkEventFilter	       GdkEventFilter;
typedef struct _GdkClientFilter	       GdkClientFilter;

typedef enum {
  GDK_COLOR_WRITEABLE = 1 << 0
} GdkColorInfoFlags;

struct _GdkColorInfo
{
  GdkColorInfoFlags flags;
  guint ref_count;
};

struct _GdkEventFilter {
  GdkFilterFunc function;
  gpointer data;
};

struct _GdkClientFilter {
  GdkAtom       type;
  GdkFilterFunc function;
  gpointer      data;
};

void gdk_window_destroy_notify	     (GdkWindow *window);

GDKVAR GdkWindow  	*gdk_parent_root;
GDKVAR gint		 gdk_error_code;
GDKVAR gint		 gdk_error_warnings;

#ifndef GDK_DISABLE_DEPRECATED

typedef struct _GdkFontPrivate	       GdkFontPrivate;

struct _GdkFontPrivate
{
  GdkFont font;
  guint ref_count;
};

#endif /* GDK_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __GDK_PRIVATE_H__ */
