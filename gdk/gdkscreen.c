/*
 * gdkscreen.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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

#include "gdk.h"		/* For gdk_rectangle_intersect() */
#include "gdkcolor.h"
#include "gdkinternals.h"
#include "gdkwindow.h"
#include "gdkscreen.h"

GType
gdk_screen_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (GdkScreenClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  NULL,                 /* class_init */
	  NULL,			/* class_finalize */
	  NULL,			/* class_data */
	  sizeof (GdkScreen),
	  0,			/* n_preallocs */
	  (GInstanceInitFunc) NULL,
	};
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
					    "GdkScreen", &object_info, 0);
    }

  return object_type;
}


/**
 * gdk_screen_close:
 * @screen: a #GdkScreen
 *
 * Closes the @screen connection and cleanup its resources.
 * Note that this function is called automatically by gdk_display_close().
 **/
void 
gdk_screen_close (GdkScreen *screen)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));
  
  g_object_run_dispose (G_OBJECT (screen));
}

/**
 * gdk_screen_get_monitor_at_point:
 * @screen : a #GdkScreen.
 * @x : the x coordinate in the virtual screen.
 * @y : the y coordinate in the virtual screen.
 *
 * Returns the monitor number in which the point (@x,@y) is located.
 *
 * Returns: the monitor number in which the point (@x,@y) belong, or
 *   -1 if the point is not in any monitor.
 **/
gint 
gdk_screen_get_monitor_at_point (GdkScreen *screen,
				 gint       x,
				 gint       y)
{
  gint num_monitors, i;
  
  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);

  num_monitors = gdk_screen_get_n_monitors (screen);
  
  for (i=0;i<num_monitors;i++)
    {
      GdkRectangle monitor;
      
      gdk_screen_get_monitor_geometry (screen, i, &monitor);

      if (x >= monitor.x &&
          x < monitor.x + monitor.width &&
          y >= monitor.y &&
          y < (monitor.y + monitor.height))
        return i;
    }

  return -1;
}

/**
 * gdk_screen_get_monitor_at_window:
 * @screen: a #GdkScreen.
 * @window: a #GdkWindow
 * @returns: the monitor number in which most of @window is located.
 *
 * Returns the number of the monitor in which the largest area of the bounding rectangle
 * of @window resides. 
 **/
gint 
gdk_screen_get_monitor_at_window (GdkScreen      *screen,
				  GdkWindow	 *window)
{
  gint num_monitors, i, sum = 0, screen_num = 0;
  GdkRectangle win_rect;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);
  
  gdk_window_get_geometry (window, &win_rect.x, &win_rect.y, &win_rect.width,
			   &win_rect.height, NULL);
  gdk_window_get_origin (window, &win_rect.x, &win_rect.y);
  num_monitors = gdk_screen_get_n_monitors (screen);
  
  for (i=0;i<num_monitors;i++)
    {
      GdkRectangle tmp_monitor, intersect;
      
      gdk_screen_get_monitor_geometry (screen, i, &tmp_monitor);
      gdk_rectangle_intersect (&win_rect, &tmp_monitor, &intersect);
      
      if (intersect.width * intersect.height > sum)
	{ 
	  sum = intersect.width * intersect.height;
	  screen_num = i;
	}
    }
  return screen_num;
}

/**
 * gdk_screen_width:
 * 
 * Returns the width of the default screen in pixels.
 * 
 * Return value:  the width of the default screen in pixels.
 **/
gint
gdk_screen_width (void)
{
  return gdk_screen_get_width (gdk_get_default_screen());
}

/**
 * gdk_screen_height:
 * 
 * Returns the height of the default screen in pixels.
 * 
 * Return value: the height of the default screen in pixels.
 **/
gint
gdk_screen_height (void)
{
  return gdk_screen_get_height (gdk_get_default_screen());
}

/**
 * gdk_screen_width_mm:
 * 
 * Returns the width of the default screen in millimeters.
 * Note that on many X servers this value will not be correct.
 * 
 * Return value: the width of the default screen in millimeters,
 * though it is not always correct.
 **/
gint
gdk_screen_width_mm (void)
{
  return gdk_screen_get_width_mm (gdk_get_default_screen());
}

/**
 * gdk_screen_height_mm:
 * 
 * Returns the height of the default screen in millimeters.
 * Note that on many X servers this value will not be correct.
 * 
 * Return value: the height of the default screen in millimeters,
 * though it is not always correct.
 **/
gint
gdk_screen_height_mm (void)
{
  return gdk_screen_get_height_mm (gdk_get_default_screen ());
}

/**
 * gdk_screen_get_window_at_pointer:
 * @screen: a #GdkScreen
 * @win_x: return location for origin of the window under the pointer
 * @win_y: return location for origin of the window under the pointer
 * 
 * Obtains the window underneath the mouse pointer, returning the location
 * of that window in @win_x, @win_y for @screen. Returns %NULL if the window 
 * under the mouse pointer is not known to GDK (for example, belongs to
 * another application).
 * 
 * Returns: the window under the mouse pointer, or %NULL
 **/
GdkWindow *
gdk_screen_get_window_at_pointer (GdkScreen *screen,
				  gint      *win_x,
				  gint      *win_y)
{
  return _gdk_current_pointer_hooks->window_at_pointer (screen, win_x, win_y);
}
