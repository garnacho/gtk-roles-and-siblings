/*
 * gdkdisplay.h
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

#ifndef __GDK_DISPLAY_H__
#define __GDK_DISPLAY_H__

#include <gdk/gdktypes.h>
#include <gdk/gdkevents.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GdkDisplayClass GdkDisplayClass;

#define GDK_TYPE_DISPLAY              (gdk_display_get_type ())
#define GDK_DISPLAY_OBJECT(object)    (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY, GdkDisplay))
#define GDK_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY, GdkDisplayClass))
#define GDK_IS_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY))
#define GDK_IS_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY))
#define GDK_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY, GdkDisplayClass))


struct _GdkDisplay
{
  GObject parent_instance;

  /*< private >*/
  GList *queued_events;
  GList *queued_tail;

  /* Information for determining if the latest button click
   * is part of a double-click or triple-click
   */
  guint32 button_click_time[2];	/* The last 2 button click times. */
  GdkWindow *button_window[2];  /* The last 2 windows to receive button presses. */
  guint button_number[2];       /* The last 2 buttons to be pressed. */

  guint double_click_time;	/* Maximum time between clicks in msecs */
  GdkDevice *core_pointer;	/* Core pointer device */

  guint closed : 1;		/* Whether this display has been closed */
};

struct _GdkDisplayClass
{
  GObjectClass parent_class;
  
  G_CONST_RETURN gchar *     (*get_display_name)   (GdkDisplay *display);
  gint			     (*get_n_screens)      (GdkDisplay *display);
  GdkScreen *		     (*get_screen)         (GdkDisplay *display,
						    gint        screen_num);
  GdkScreen *		     (*get_default_screen) (GdkDisplay *display);
};

GType       gdk_display_get_type (void);
GdkDisplay *gdk_display_open                (const gchar *display_name);

G_CONST_RETURN gchar * gdk_display_get_name (GdkDisplay *display);

gint        gdk_display_get_n_screens      (GdkDisplay  *display);
GdkScreen * gdk_display_get_screen         (GdkDisplay  *display,
					    gint         screen_num);
GdkScreen * gdk_display_get_default_screen (GdkDisplay  *display);
void        gdk_display_pointer_ungrab     (GdkDisplay  *display,
					    guint32      time);
void        gdk_display_keyboard_ungrab    (GdkDisplay  *display,
					    guint32      time);
gboolean    gdk_display_pointer_is_grabbed (GdkDisplay  *display);
void        gdk_display_beep               (GdkDisplay  *display);
void        gdk_display_sync               (GdkDisplay  *display);
void	    gdk_display_close		   (GdkDisplay  *display);

GList *     gdk_display_list_devices       (GdkDisplay  *display);

GdkEvent* gdk_display_get_event  (GdkDisplay *display);
GdkEvent* gdk_display_peek_event (GdkDisplay *display);
void      gdk_display_put_event  (GdkDisplay *display,
				  GdkEvent   *event);

void gdk_display_add_client_message_filter (GdkDisplay   *display,
					    GdkAtom       message_type,
					    GdkFilterFunc func,
					    gpointer      data);

void gdk_display_set_double_click_time (GdkDisplay  *display,
					guint        msec);
void gdk_display_set_sm_client_id      (GdkDisplay  *display,
					const gchar *sm_client_id);

void        gdk_set_default_display (GdkDisplay *display);
GdkDisplay *gdk_display_get_default (void);

GdkDevice  *gdk_display_get_core_pointer (GdkDisplay *display);

G_END_DECLS

#endif				/* __GDK_DISPLAY_H__ */
