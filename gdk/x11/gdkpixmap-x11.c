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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* Needed for SEEK_END in SunOS */
#include <unistd.h>
#include <X11/Xlib.h>

#include "gdkx.h"

#include "gdkpixmap-x11.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"

#include <gdk/gdkinternals.h>

typedef struct
{
  gchar *color_string;
  GdkColor color;
  gint transparent;
} _GdkPixmapColor;

typedef struct
{
  guint ncolors;
  GdkColormap *colormap;
  gulong pixels[1];
} _GdkPixmapInfo;

static void gdk_pixmap_impl_x11_get_size   (GdkDrawable        *drawable,
                                        gint               *width,
                                        gint               *height);

static void gdk_pixmap_impl_x11_init       (GdkPixmapImplX11      *pixmap);
static void gdk_pixmap_impl_x11_class_init (GdkPixmapImplX11Class *klass);
static void gdk_pixmap_impl_x11_finalize   (GObject            *object);

static gpointer parent_class = NULL;

static GType
gdk_pixmap_impl_x11_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkPixmapImplX11Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_pixmap_impl_x11_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkPixmapImplX11),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_pixmap_impl_x11_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_X11,
                                            "GdkPixmapImplX11",
                                            &object_info, 0);
    }
  
  return object_type;
}


GType
_gdk_pixmap_impl_get_type (void)
{
  return gdk_pixmap_impl_x11_get_type ();
}

static void
gdk_pixmap_impl_x11_init (GdkPixmapImplX11 *impl)
{
  impl->width = 1;
  impl->height = 1;
}

static void
gdk_pixmap_impl_x11_class_init (GdkPixmapImplX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_pixmap_impl_x11_finalize;

  drawable_class->get_size = gdk_pixmap_impl_x11_get_size;
}

static void
gdk_pixmap_impl_x11_finalize (GObject *object)
{
  GdkPixmapImplX11 *impl = GDK_PIXMAP_IMPL_X11 (object);
  GdkPixmap *wrapper = GDK_PIXMAP (GDK_DRAWABLE_IMPL_X11 (impl)->wrapper);
  GdkDisplay *display = GDK_PIXMAP_DISPLAY (wrapper);

  if (!display->closed)
    {
#ifdef HAVE_XFT  
      {
	GdkDrawableImplX11 *draw_impl = GDK_DRAWABLE_IMPL_X11 (impl);
	
#ifdef HAVE_XFT2
	if (draw_impl->xft_draw)
	  XftDrawDestroy (draw_impl->xft_draw);
#else /* !HAVE_XFT2 */
	if (draw_impl->picture)
	  XRenderFreePicture (GDK_DISPLAY_XDISPLAY (display), draw_impl->picture);
#endif /* HAVE_XFT2 */
      }
#endif /* HAVE_XFT */

      if (!impl->is_foreign)
	XFreePixmap (GDK_DISPLAY_XDISPLAY (display), GDK_PIXMAP_XID (wrapper));
    }
      
  _gdk_xid_table_remove (display, GDK_PIXMAP_XID (wrapper));
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_pixmap_impl_x11_get_size   (GdkDrawable *drawable,
                                gint        *width,
                                gint        *height)
{
  if (width)
    *width = GDK_PIXMAP_IMPL_X11 (drawable)->width;
  if (height)
    *height = GDK_PIXMAP_IMPL_X11 (drawable)->height;
}

GdkPixmap*
gdk_pixmap_new (GdkWindow *window,
		gint       width,
		gint       height,
		gint       depth)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;
  GdkColormap *cmap;
  gint window_depth;
  
  g_return_val_if_fail (window == NULL || GDK_IS_DRAWABLE (window), NULL);
  g_return_val_if_fail ((window != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  
  if (!window)
    {
      GDK_NOTE (MULTIHEAD, g_message ("need to specify the screen parent window "
				      "for gdk_pixmap_new() to be multihead safe"));
      window = gdk_screen_get_root_window (gdk_screen_get_default ());
    }

  if (GDK_IS_WINDOW (window) && GDK_WINDOW_DESTROYED (window))
    return NULL;

  window_depth = gdk_drawable_get_depth (GDK_DRAWABLE (window));
  if (depth == -1)
    depth = window_depth;

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  draw_impl->screen = GDK_WINDOW_SCREEN (window);
  draw_impl->xid = XCreatePixmap (GDK_PIXMAP_XDISPLAY (pixmap),
                                  GDK_WINDOW_XID (window),
                                  width, height, depth);
  
  pix_impl->is_foreign = FALSE;
  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  if (depth == window_depth)
    {
      cmap = gdk_drawable_get_colormap (window);
      if (cmap)
        gdk_drawable_set_colormap (pixmap, cmap);
    }
  
  _gdk_xid_table_insert (GDK_WINDOW_DISPLAY (window), 
			 &GDK_PIXMAP_XID (pixmap), pixmap);
  return pixmap;
}

GdkPixmap *
gdk_bitmap_create_from_data (GdkWindow   *window,
			     const gchar *data,
			     gint         width,
			     gint         height)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;
  
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  g_return_val_if_fail (window == NULL || GDK_IS_DRAWABLE (window), NULL);

  if (!window) 
    {
      GDK_NOTE (MULTIHEAD, g_message ("need to specify the screen parent window "
				     "for gdk_bitmap_create_from_data() to be multihead safe"));
      window = gdk_screen_get_root_window (gdk_screen_get_default ());
    }
  
  if (GDK_IS_WINDOW (window) && GDK_WINDOW_DESTROYED (window))
    return NULL;

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);

  pix_impl->is_foreign = FALSE;
  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = 1;

  draw_impl->screen = GDK_WINDOW_SCREEN (window);
  draw_impl->xid = XCreateBitmapFromData (GDK_WINDOW_XDISPLAY (window),
                                          GDK_WINDOW_XID (window),
                                          (char *)data, width, height);

  _gdk_xid_table_insert (GDK_WINDOW_DISPLAY (window), 
			 &GDK_PIXMAP_XID (pixmap), pixmap);
  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_data (GdkWindow   *window,
			     const gchar *data,
			     gint         width,
			     gint         height,
			     gint         depth,
			     GdkColor    *fg,
			     GdkColor    *bg)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;

  g_return_val_if_fail (window == NULL || GDK_IS_DRAWABLE (window), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);
  g_return_val_if_fail ((window != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (!window)
    {
      GDK_NOTE (MULTIHEAD, g_message ("need to specify the screen parent window"
				      "for gdk_pixmap_create_from_data() to be multihead safe"));
      window = gdk_screen_get_root_window (gdk_screen_get_default ());
    }

  if (GDK_IS_WINDOW (window) && GDK_WINDOW_DESTROYED (window))
    return NULL;

  if (depth == -1)
    depth = gdk_drawable_get_visual (window)->depth;

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  pix_impl->is_foreign = FALSE;
  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  draw_impl->screen = GDK_DRAWABLE_SCREEN (window);
  draw_impl->xid = XCreatePixmapFromBitmapData (GDK_WINDOW_XDISPLAY (window),
                                                GDK_WINDOW_XID (window),
                                                (char *)data, width, height,
                                                fg->pixel, bg->pixel, depth);

  _gdk_xid_table_insert (GDK_WINDOW_DISPLAY (window),
			 &GDK_PIXMAP_XID (pixmap), pixmap);
  return pixmap;
}

/**
 * gdk_pixmap_foreign_new_for_display:
 * @display: The #GdkDisplay where @anid is located.
 * @anid: a native pixmap handle.
 * 
 * Wraps a native window in a #GdkPixmap.
 * This may fail if the pixmap has been destroyed.
 *
 * For example in the X backend, a native pixmap handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the newly-created #GdkPixmap wrapper for the 
 *    native pixmap or %NULL if the pixmap has been destroyed.
 **/
GdkPixmap *
gdk_pixmap_foreign_new_for_display (GdkDisplay      *display,
				    GdkNativeWindow  anid)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;
  Pixmap xpixmap;
  Window root_return;
  int x_ret, y_ret;
  unsigned int w_ret, h_ret, bw_ret, depth_ret;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  /* check to make sure we were passed something at
   * least a little sane */
  g_return_val_if_fail ((anid != 0), NULL);
  
  /* set the pixmap to the passed in value */
  xpixmap = anid;

  /* get information about the Pixmap to fill in the structure for
     the gdk window */
  if (!XGetGeometry (GDK_DISPLAY_XDISPLAY (display),
		     xpixmap, &root_return,
		     &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
    return NULL;

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);

  draw_impl->screen =  _gdk_x11_display_screen_for_xrootwin (display, root_return);
  draw_impl->xid = xpixmap;

  pix_impl->is_foreign = TRUE;
  pix_impl->width = w_ret;
  pix_impl->height = h_ret;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth_ret;
  
  _gdk_xid_table_insert (display, &GDK_PIXMAP_XID (pixmap), pixmap);

  return pixmap;
}

/**
 * gdk_pixmap_foreign_new:
 * @anid: a native pixmap handle.
 * 
 * Wraps a native window for the default display in a #GdkPixmap.
 * This may fail if the pixmap has been destroyed.
 *
 * For example in the X backend, a native pixmap handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the newly-created #GdkPixmap wrapper for the 
 *    native pixmap or %NULL if the pixmap has been destroyed.
 **/
GdkPixmap*
gdk_pixmap_foreign_new (GdkNativeWindow anid)
{
   return gdk_pixmap_foreign_new_for_display (gdk_display_get_default (), anid);
}

/**
 * gdk_pixmap_lookup:
 * @anid: a native pixmap handle.
 * 
 * Looks up the #GdkPixmap that wraps the given native pixmap handle.
 *
 * For example in the X backend, a native pixmap handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the #GdkWindow wrapper for the native window,
 *    or %NULL if there is none.
 **/
GdkPixmap*
gdk_pixmap_lookup (GdkNativeWindow anid)
{
  return (GdkPixmap*) gdk_xid_table_lookup_for_display (gdk_display_get_default (), anid);
}

/**
 * gdk_pixmap_lookup_for_display:
 * @display : the #GdkDisplay associated with @anid
 * @anid: a native pixmap handle.
 * 
 * Looks up the #GdkPixmap that wraps the given native pixmap handle.
 *
 * For example in the X backend, a native pixmap handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the #GdkWindow wrapper for the native window,
 *    or %NULL if there is none.
 **/
GdkPixmap*
gdk_pixmap_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return (GdkPixmap*) gdk_xid_table_lookup_for_display (display, anid);
}
