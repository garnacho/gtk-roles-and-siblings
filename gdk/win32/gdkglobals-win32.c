/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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

#include <config.h>
#include "gdktypes.h"
#include "gdkprivate-win32.h"

GdkDisplay	 *_gdk_display = NULL;
GdkScreen	 *_gdk_screen = NULL;
GdkWindow	 *_gdk_parent_root = NULL;

gint		  _gdk_num_monitors;
GdkRectangle     *_gdk_monitors;

gint		 _gdk_offset_x, _gdk_offset_y;

HWND              _gdk_root_window = NULL;
HDC		  _gdk_display_hdc;
HINSTANCE	  _gdk_dll_hinstance;
HINSTANCE	  _gdk_app_hmodule;

HKL		  _gdk_input_locale;
gboolean	  _gdk_input_locale_is_ime;
UINT		  _gdk_input_codepage;

WORD  		  _cf_rtf;
WORD		  _cf_utf8_string;

GdkAtom           _utf8_string;
GdkAtom		  _text_uri_list;
GdkAtom		  _targets;

GdkAtom		  _local_dnd;
GdkAtom		  _gdk_win32_dropfiles;
GdkAtom		  _gdk_ole2_dnd;

GdkAtom           _gdk_selection_property;

GdkAtom	          _wm_transient_for;

DWORD		  _windows_version;

gint		  _gdk_input_ignore_wintab = TRUE;
gint		  _gdk_max_colors = 0;
