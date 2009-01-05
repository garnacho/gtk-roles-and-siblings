/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2002,2005 Hans Breuer
 * Copyright (C) 2003 Tor Lillqvist
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

#include "config.h"
#include "gdk.h"
#include "gdkprivate-win32.h"

#define HAVE_MONITOR_INFO

#if defined(_MSC_VER) && (WINVER < 0x500) && (WINVER > 0x0400)
#include <multimon.h>
#elif defined(_MSC_VER) && (WINVER <= 0x0400)
#undef HAVE_MONITOR_INFO
#endif

void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
  g_assert (display == NULL || _gdk_display == display);
}

#ifdef HAVE_MONITOR_INFO
static BOOL CALLBACK
count_monitor (HMONITOR hmonitor,
	       HDC      hdc,
	       LPRECT   rect,
	       LPARAM   data)
{
  gint *n = (gint *) data;

  (*n)++;

  return TRUE;
}

static BOOL CALLBACK
enum_monitor (HMONITOR hmonitor,
	      HDC      hdc,
	      LPRECT   rect,
	      LPARAM   data)
{
  /* The struct MONITORINFOEX definition is for some reason different
   * in the winuser.h bundled with mingw64 from that in MSDN and the
   * official 32-bit mingw (the MONITORINFO part is in a separate "mi"
   * member). So to keep this easily compileable with either, repeat
   * the MSDN definition it here.
   */
  typedef struct tagMONITORINFOEXA2 {
    DWORD cbSize;
    RECT  rcMonitor;
    RECT  rcWork;
    DWORD dwFlags;
    CHAR szDevice[CCHDEVICENAME];
  } MONITORINFOEXA2;
  
  MONITORINFOEXA2 monitor_info;
  HDC hDC;

  gint *index = (gint *) data;
  GdkWin32Monitor *monitor;

  g_assert (*index < _gdk_num_monitors);

  monitor = _gdk_monitors + *index;

  monitor_info.cbSize = sizeof (MONITORINFOEX);
  GetMonitorInfoA (hmonitor, (MONITORINFO *) &monitor_info);

#ifndef MONITORINFOF_PRIMARY
#define MONITORINFOF_PRIMARY 1
#endif

  monitor->name = g_strdup (monitor_info.szDevice);
  hDC = CreateDCA ("DISPLAY", monitor_info.szDevice, NULL, NULL);
  monitor->width_mm = GetDeviceCaps (hDC, HORZSIZE);
  monitor->height_mm = GetDeviceCaps (hDC, VERTSIZE);
  DeleteDC (hDC);
  monitor->rect.x = monitor_info.rcMonitor.left;
  monitor->rect.y = monitor_info.rcMonitor.top;
  monitor->rect.width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
  monitor->rect.height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;

  if (monitor_info.dwFlags & MONITORINFOF_PRIMARY &&
      *index != 0)
    {
      /* Put primary monitor at index 0, just in case somebody needs
       * to know which one is the primary.
       */
      GdkWin32Monitor temp = *monitor;
      *monitor = _gdk_monitors[0];
      _gdk_monitors[0] = temp;
    }

  (*index)++;

  return TRUE;
}
#endif /* HAVE_MONITOR_INFO */

void
_gdk_monitor_init (void)
{
#ifdef HAVE_MONITOR_INFO
  gint i, index;

  _gdk_num_monitors = 0;

  EnumDisplayMonitors (NULL, NULL, count_monitor, (LPARAM) &_gdk_num_monitors);

  _gdk_monitors = g_renew (GdkWin32Monitor, _gdk_monitors, _gdk_num_monitors);

  index = 0;
  EnumDisplayMonitors (NULL, NULL, enum_monitor, (LPARAM) &index);

  _gdk_offset_x = G_MININT;
  _gdk_offset_y = G_MININT;

  /* Calculate offset */
  for (i = 0; i < _gdk_num_monitors; i++)
    {
      _gdk_offset_x = MAX (_gdk_offset_x, -_gdk_monitors[i].rect.x);
      _gdk_offset_y = MAX (_gdk_offset_y, -_gdk_monitors[i].rect.y);
    }
  GDK_NOTE (MISC, g_print ("Multi-monitor offset: (%d,%d)\n",
			   _gdk_offset_x, _gdk_offset_y));

  /* Translate monitor coords into GDK coordinate space */
  for (i = 0; i < _gdk_num_monitors; i++)
    {
      _gdk_monitors[i].rect.x += _gdk_offset_x;
      _gdk_monitors[i].rect.y += _gdk_offset_y;
      GDK_NOTE (MISC, g_print ("Monitor %d: %dx%d@%+d%+d\n",
			       i, _gdk_monitors[i].rect.width,
			       _gdk_monitors[i].rect.height,
			       _gdk_monitors[i].rect.x,
			       _gdk_monitors[i].rect.y));
    }
#else
  HDC hDC;

  _gdk_num_monitors = 1;
  _gdk_monitors = g_renew (GdkWin32Monitor, _gdk_monitors, 1);

  _gdk_monitors[0].name = g_strdup ("DISPLAY");
  hDC = GetDC (NULL);
  _gdk_monitors[0].width_mm = GetDeviceCaps (hDC, HORZSIZE);
  _gdk_monitors[0].height_mm = GetDeviceCaps (hDC, VERTSIZE);
  ReleaseDC (NULL, hDC);
  _gdk_monitors[0].rect.x = 0;
  _gdk_monitors[0].rect.y = 0;
  _gdk_monitors[0].rect.width = GetSystemMetrics (SM_CXSCREEN);
  _gdk_monitors[0].rect.height = GetSystemMetrics (SM_CYSCREEN);
  _gdk_offset_x = 0;
  _gdk_offset_y = 0;
#endif
}

GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  GDK_NOTE (MISC, g_print ("gdk_display_open: %s\n", (display_name ? display_name : "NULL")));

  if (display_name == NULL ||
      g_ascii_strcasecmp (display_name,
			  gdk_display_get_name (_gdk_display)) == 0)
    {
      if (_gdk_display != NULL)
	{
	  GDK_NOTE (MISC, g_print ("... return _gdk_display\n"));
	  return _gdk_display;
	}
    }
  else
    {
      GDK_NOTE (MISC, g_print ("... return NULL\n"));
      return NULL;
    }

  _gdk_display = g_object_new (GDK_TYPE_DISPLAY, NULL);
  _gdk_screen = g_object_new (GDK_TYPE_SCREEN, NULL);

  _gdk_monitor_init ();
  _gdk_visual_init ();
  gdk_screen_set_default_colormap (_gdk_screen,
                                   gdk_screen_get_system_colormap (_gdk_screen));
  _gdk_windowing_window_init ();
  _gdk_windowing_image_init ();
  _gdk_events_init ();
  _gdk_input_init (_gdk_display);
  _gdk_dnd_init ();

  /* Precalculate display name */
  (void) gdk_display_get_name (_gdk_display);

  g_signal_emit_by_name (gdk_display_manager_get (),
			 "display_opened", _gdk_display);

  GDK_NOTE (MISC, g_print ("... _gdk_display now set up\n"));

  return _gdk_display;
}

G_CONST_RETURN gchar *
gdk_display_get_name (GdkDisplay *display)
{
  HDESK hdesk = GetThreadDesktop (GetCurrentThreadId ());
  char dummy;
  char *desktop_name;
  HWINSTA hwinsta = GetProcessWindowStation ();
  char *window_station_name;
  DWORD n;
  DWORD session_id;
  char *display_name;
  static const char *display_name_cache = NULL;
  typedef BOOL (* PFN_ProcessIdToSessionId) (DWORD, DWORD *);
  PFN_ProcessIdToSessionId processIdToSessionId;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  if (display_name_cache != NULL)
    return display_name_cache;

  n = 0;
  GetUserObjectInformation (hdesk, UOI_NAME, &dummy, 0, &n);
  if (n == 0)
    desktop_name = "Default";
  else
    {
      n++;
      desktop_name = g_alloca (n + 1);
      memset (desktop_name, 0, n + 1);

      if (!GetUserObjectInformation (hdesk, UOI_NAME, desktop_name, n, &n))
	desktop_name = "Default";
    }

  n = 0;
  GetUserObjectInformation (hwinsta, UOI_NAME, &dummy, 0, &n);
  if (n == 0)
    window_station_name = "WinSta0";
  else
    {
      n++;
      window_station_name = g_alloca (n + 1);
      memset (window_station_name, 0, n + 1);

      if (!GetUserObjectInformation (hwinsta, UOI_NAME, window_station_name, n, &n))
	window_station_name = "WinSta0";
    }

  processIdToSessionId = (PFN_ProcessIdToSessionId) GetProcAddress (GetModuleHandle ("kernel32.dll"), "ProcessIdToSessionId");
  if (!processIdToSessionId || !processIdToSessionId (GetCurrentProcessId (), &session_id))
    session_id = 0;

  display_name = g_strdup_printf ("%ld\\%s\\%s",
				  session_id,
				  window_station_name,
				  desktop_name);

  GDK_NOTE (MISC, g_print ("gdk_display_get_name: %s\n", display_name));

  display_name_cache = display_name;

  return display_name_cache;
}

gint
gdk_display_get_n_screens (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);
  
  return 1;
}

GdkScreen *
gdk_display_get_screen (GdkDisplay *display,
			gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (screen_num == 0, NULL);

  return _gdk_screen;
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return _gdk_screen;
}

GdkWindow *
gdk_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  g_warning ("gdk_display_get_default_group not yet implemented");

  return NULL;
}

gboolean 
gdk_display_supports_selection_notification (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return FALSE;
}

gboolean 
gdk_display_request_selection_notification (GdkDisplay *display,
                                            GdkAtom     selection)

{
  return FALSE;
}

gboolean
gdk_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

void
gdk_display_store_clipboard (GdkDisplay    *display,
			     GdkWindow     *clipboard_window,
			     guint32        time_,
			     const GdkAtom *targets,
			     gint           n_targets)
{
}

gboolean 
gdk_display_supports_shapes (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

gboolean 
gdk_display_supports_input_shapes (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  /* Not yet implemented. See comment in
   * gdk_window_input_shape_combine_mask().
   */

  return FALSE;
}

gboolean
gdk_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}
