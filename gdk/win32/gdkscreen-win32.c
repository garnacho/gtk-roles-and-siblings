/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2002 Hans Breuer
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

#include "gdk.h"
#include "gdkprivate-win32.h"

static GdkColormap *default_colormap = NULL;

GdkDisplay *
gdk_screen_get_display (GdkScreen *screen)
{
  return _gdk_display;
}

GdkWindow *
gdk_screen_get_root_window (GdkScreen *screen)
{
  return _gdk_parent_root;
}

GdkColormap *
gdk_screen_get_default_colormap (GdkScreen *screen)
{
  return default_colormap;
}

void
gdk_screen_set_default_colormap (GdkScreen   *screen,
				 GdkColormap *colormap)
{
  GdkColormap *old_colormap;
  
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  old_colormap = default_colormap;

  default_colormap = g_object_ref (colormap);
  
  if (old_colormap)
    g_object_unref (old_colormap);
}

gint
gdk_screen_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 1);

  return 1;
}

void
gdk_screen_get_monitor_geometry (GdkScreen    *screen, 
				 gint          num_monitor,
				 GdkRectangle *dest)
{
  dest->x = 0;
  dest->y = 0;
  dest->width = gdk_screen_width ();
  dest->height = gdk_screen_height ();
}

gint
gdk_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);  
  
  return 0;
}

gchar * 
_gdk_windowing_substitute_screen_number (const gchar *display_name,
					 int          screen_number)
{
  return g_strdup (display_name);
}

gchar *
gdk_screen_make_display_name (GdkScreen *screen)
{
  return g_strdup ("");
}
