/*
 * gdkscreen.h
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

#ifndef __GDK_SCREEN_H__
#define __GDK_SCREEN_H__

#include "gdk/gdktypes.h"
#include "gdk/gdkdisplay.h"

G_BEGIN_DECLS

typedef struct _GdkScreenClass GdkScreenClass;

#define GDK_TYPE_SCREEN            (gdk_screen_get_type ())
#define GDK_SCREEN(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SCREEN, GdkScreen))
#define GDK_SCREEN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SCREEN, GdkScreenClass))
#define GDK_IS_SCREEN(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SCREEN))
#define GDK_IS_SCREEN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SCREEN))
#define GDK_SCREEN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SCREEN, GdkScreenClass))

struct _GdkScreen
{
  GObject parent_instance;
};

struct _GdkScreenClass
{
  GObjectClass parent_class;
  
  GdkDisplay *  (*get_display)           (GdkScreen    *screen);
  gint          (*get_width)             (GdkScreen    *screen);
  gint          (*get_height)            (GdkScreen    *screen);
  gint          (*get_width_mm)          (GdkScreen    *screen);
  gint          (*get_height_mm)         (GdkScreen    *screen);
  gint          (*get_root_depth)        (GdkScreen    *screen);
  gint          (*get_screen_num)        (GdkScreen    *screen);
  GdkWindow *   (*get_root_window)       (GdkScreen    *screen);
  GdkColormap * (*get_default_colormap)  (GdkScreen    *screen);
  void          (*set_default_colormap)  (GdkScreen    *screen,
					  GdkColormap  *colormap);
  GdkWindow *   (*get_window_at_pointer) (GdkScreen    *screen,
					  gint         *win_x,
					  gint         *win_y);
  gint          (*get_n_monitors)        (GdkScreen    *screen);
  void          (*get_monitor_geometry)  (GdkScreen    *screen,
					  gint          monitor_num,
					  GdkRectangle *dest);
};

GType        gdk_screen_get_type              (void);
GdkColormap *gdk_screen_get_default_colormap  (GdkScreen   *screen);
void         gdk_screen_set_default_colormap  (GdkScreen   *screen,
					       GdkColormap *colormap);
GdkColormap* gdk_screen_get_system_colormap   (GdkScreen   *screen);
GdkVisual*   gdk_screen_get_system_visual     (GdkScreen   *screen);
GdkColormap *gdk_screen_get_rgb_colormap      (GdkScreen   *screen);
GdkVisual *  gdk_screen_get_rgb_visual        (GdkScreen   *screen);

GdkWindow *  gdk_screen_get_root_window       (GdkScreen   *screen);
GdkDisplay * gdk_screen_get_display           (GdkScreen   *screen);
gint         gdk_screen_get_number            (GdkScreen   *screen);
GdkWindow *  gdk_screen_get_window_at_pointer (GdkScreen   *screen,
					       gint        *win_x,
					       gint        *win_y);
gint         gdk_screen_get_width             (GdkScreen   *screen);
gint         gdk_screen_get_height            (GdkScreen   *screen);
gint         gdk_screen_get_width_mm          (GdkScreen   *screen);
gint         gdk_screen_get_height_mm         (GdkScreen   *screen);
void	     gdk_screen_close		      (GdkScreen   *screen);

GList *      gdk_screen_list_visuals          (GdkScreen   *screen);
GList *      gdk_screen_get_toplevel_windows  (GdkScreen   *screen);


gint          gdk_screen_get_n_monitors        (GdkScreen *screen);
void          gdk_screen_get_monitor_geometry  (GdkScreen *screen,
						gint       monitor_num,
						GdkRectangle *dest);
gint          gdk_screen_get_monitor_at_point  (GdkScreen *screen,
						gint       x,
						gint       y);
gint          gdk_screen_get_monitor_at_window (GdkScreen *screen,
						GdkWindow *window);

void          gdk_screen_broadcast_client_message  (GdkScreen       *screen,
						    GdkEvent        *event);

GdkScreen *gdk_get_default_screen (void);

gboolean   gdk_screen_get_setting (GdkScreen   *screen,
				   const gchar *name,
				   GValue      *value);

G_END_DECLS

#endif				/* __GDK_SCREEN_H__ */
