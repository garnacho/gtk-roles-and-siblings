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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <X11/Xlib.h>
#include "gdktypes.h"
#include "gdkprivate.h"

gint              gdk_debug_level = 0;
gint              gdk_show_events = FALSE;
gint              gdk_use_xshm = TRUE;
gchar            *gdk_display_name = NULL;
Display          *gdk_display = NULL;
gint              gdk_screen;
Window            gdk_root_window;
Window            gdk_leader_window;
GdkWindowPrivate  gdk_root_parent;
Atom              gdk_wm_delete_window;
Atom              gdk_wm_take_focus;
Atom              gdk_wm_protocols;
Atom              gdk_wm_window_protocols[2];
Atom              gdk_selection_property;
GdkDndGlobals     gdk_dnd = {None,None,None,
			     None,None,None,
			     None,
			     None,None,
			     NULL,
			     0, 0,
			     {0,0}};
gchar            *gdk_progname = NULL;
gchar            *gdk_progclass = NULL;
gint              gdk_error_code;
gint              gdk_error_warnings = TRUE;
