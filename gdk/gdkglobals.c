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

#include <stdio.h>

#include "gdktypes.h"
#include "gdkprivate.h"
#include "config.h"

guint             _gdk_debug_flags = 0;
GdkWindow        *_gdk_parent_root = NULL;
gint              _gdk_error_code = 0;
gint              _gdk_error_warnings = TRUE;
GList            *_gdk_default_filters = NULL;
GList            *_gdk_queued_events = NULL;
GList            *_gdk_queued_tail = NULL;

GDKVAR GMutex     *gdk_threads_mutex = NULL;          /* Global GDK lock */

