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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <io.h>

#include "gdk.h"
#include "gdkkeysyms.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"

#include <objbase.h>

static gboolean gdk_synchronize = FALSE;

GdkArgDesc _gdk_windowing_args[] = {
  { "sync",          GDK_ARG_BOOL, &gdk_synchronize, (GdkArgFunc) NULL},
  { "no-wintab",     GDK_ARG_BOOL, &gdk_input_ignore_wintab,
						     (GdkArgFunc) NULL},
  { "ignore-wintab", GDK_ARG_BOOL, &gdk_input_ignore_wintab,
						     (GdkArgFunc) NULL},
  { "event-func-from-window-proc",
		     GDK_ARG_BOOL, &gdk_event_func_from_window_proc,
						     (GdkArgFunc) NULL},
  { "max-colors",    GDK_ARG_INT,  &gdk_max_colors,  (GdkArgFunc) NULL},
  { NULL }
};

int __stdcall
DllMain(HINSTANCE hinstDLL,
	DWORD     dwReason,
	LPVOID    reserved)
{
  gdk_dll_hinstance = hinstDLL;

  return TRUE;
}

void
_gdk_windowing_init (gint    *argc,
                     gchar ***argv)
{
  gchar buf[10];

#ifdef HAVE_WINTAB
  if (getenv ("GDK_IGNORE_WINTAB") != NULL)
    gdk_input_ignore_wintab = TRUE;
#endif
  if (getenv ("GDK_EVENT_FUNC_FROM_WINDOW_PROC") != NULL)
    gdk_event_func_from_window_proc = TRUE;

  if (gdk_synchronize)
    GdiSetBatchLimit (1);

  gdk_app_hmodule = GetModuleHandle (NULL);
  gdk_display_hdc = CreateDC ("DISPLAY", NULL, NULL, NULL);
  gdk_root_window = GetDesktopWindow ();
  windows_version = GetVersion ();

  _gdk_input_locale = GetKeyboardLayout (0);
  GetLocaleInfo (MAKELCID (LOWORD (_gdk_input_locale), SORT_DEFAULT),
		 LOCALE_IDEFAULTANSICODEPAGE,
		 buf, sizeof (buf));
  _gdk_input_codepage = atoi (buf);
  GDK_NOTE (MISC, g_print ("input_locale: %p, codepage:%d\n",
			   _gdk_input_locale, _gdk_input_codepage));

  CoInitialize (NULL);

  cf_rtf = RegisterClipboardFormat ("Rich Text Format");
  cf_utf8_string = RegisterClipboardFormat ("UTF8_STRING");

  utf8_string = gdk_atom_intern ("UTF8_STRING", FALSE);
  compound_text = gdk_atom_intern ("COMPOUND_TEXT", FALSE);
  text_uri_list = gdk_atom_intern ("text/uri-list", FALSE);

  local_dnd = gdk_atom_intern ("LocalDndSelection", FALSE);
  gdk_win32_dropfiles = gdk_atom_intern ("DROPFILES_DND", FALSE);
  gdk_ole2_dnd = gdk_atom_intern ("OLE2_DND", FALSE);

  _gdk_selection_property = gdk_atom_intern ("GDK_SELECTION", FALSE);

  _gdk_win32_selection_init ();
}

void
gdk_win32_api_failed (const gchar *where,
		      gint         line,
		      const gchar *api)
{
  gchar *msg = g_win32_error_message (GetLastError ());
  g_warning ("%s:%d: %s failed: %s", where, line, api, msg);
  g_free (msg);
}

void
gdk_other_api_failed (const gchar *where,
		      gint         line,
		      const gchar *api)
{
  g_warning ("%s:%d: %s failed", where, line, api);
}

void
gdk_win32_gdi_failed (const gchar *where,
		      gint         line,
		      const gchar *api)
{
  /* On Win9x GDI calls are implemented in 16-bit code and thus
   * don't set the 32-bit error code, sigh.
   */
  if (IS_WIN_NT ())
    gdk_win32_api_failed (where, line, api);
  else
    gdk_other_api_failed (where, line, api);
}

void
gdk_set_use_xshm (gboolean use_xshm)
{
  /* Always on */
}

gboolean
gdk_get_use_xshm (void)
{
  return TRUE;
}

gint
gdk_screen_get_width (GdkScreen *screen)
{
  return GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (_gdk_parent_root)->impl)->width;
}

gint
gdk_screen_get_height (GdkScreen *screen)
{
  return GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (_gdk_parent_root)->impl)->height;
}
gint
gdk_screen_get_width_mm (GdkScreen *screen)
{
  return (double) GetDeviceCaps (gdk_display_hdc, HORZRES) / GetDeviceCaps (gdk_display_hdc, LOGPIXELSX) * 25.4;
}

gint
gdk_screen_get_height_mm (GdkScreen *screen)
{
  return (double) GetDeviceCaps (gdk_display_hdc, VERTRES) / GetDeviceCaps (gdk_display_hdc, LOGPIXELSY) * 25.4;
}

void
_gdk_windowing_display_set_sm_client_id (GdkDisplay  *display,
					 const gchar *sm_client_id)
{
  g_warning("gdk_set_sm_client_id %s", sm_client_id ? sm_client_id : "NULL");
}

void
gdk_display_beep (GdkDisplay *display)
{
  g_return_if_fail (display == gdk_display_get_default());
  Beep(1000, 50);
}

void
_gdk_windowing_exit (void)
{
  _gdk_win32_dnd_exit ();
  CoUninitialize ();
  DeleteDC (gdk_display_hdc);
  gdk_display_hdc = NULL;
}

void
gdk_error_trap_push (void)
{
}

gint
gdk_error_trap_pop (void)
{
  return 0;
}

void
gdk_notify_startup_complete (void)
{
}
