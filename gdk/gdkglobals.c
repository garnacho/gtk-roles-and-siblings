/* GDK - The GIMP Drawing Kit
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include "gdktypes.h"
#include "gdkprivate.h"
#include "config.h"

guint             gdk_debug_flags = 0;
gint              gdk_use_xshm = TRUE;
gchar            *gdk_display_name = NULL;
Display          *gdk_display = NULL;
gint              gdk_screen;
Window            gdk_root_window;
Window            gdk_leader_window;
GdkWindowPrivate  gdk_root_parent = { { NULL, }, NULL, };
Atom              gdk_wm_delete_window;
Atom              gdk_wm_take_focus;
Atom              gdk_wm_protocols;
Atom              gdk_wm_window_protocols[2];
Atom              gdk_selection_property;
GdkDndCursorInfo  gdk_dnd_cursorinfo = {None, None, NULL, NULL,
					{0,0}, {0,0}, NULL};
GdkDndGlobals     gdk_dnd = {None,None,None,
			     None,None,None,
			     None,
			     &gdk_dnd_cursorinfo,
			     NULL,
			     0,
			     FALSE, FALSE, FALSE,
			     None,
			     {0,0},
			     {0,0}, {0,0},
			     {0,0,0,0}, NULL, None, 0};
gchar            *gdk_progclass = NULL;
gint              gdk_error_code = 0;
gint              gdk_error_warnings = TRUE;
gint              gdk_null_window_warnings = TRUE;
GList            *gdk_default_filters = NULL;

gboolean      gdk_xim_using;  	        /* using XIM Protocol if TRUE */
#ifdef USE_XIM
GdkICPrivate *gdk_xim_ic;		/* currently using IC */
#endif /* USE_XIM */
GdkWindow    *gdk_xim_window;		/* currently using Widow */

GdkWindowPrivate *gdk_xgrab_window = NULL;  /* Window that currently holds the
					     *	x pointer grab
					     */

GMutex *gdk_threads_mutex = NULL;          /* Global GDK lock */

#ifdef USE_XIM
GdkICPrivate *gdk_xim_ic;		/* currently using IC */
GdkWindow *gdk_xim_window;	        /* currently using Window */
#endif

