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

/* Cannot use TrackMouseEvent, as the stupid WM_MOUSELEAVE message
 * doesn't tell us where the mouse has gone. Thus we cannot use it to
 * generate a correct GdkNotifyType. Pity, as using TrackMouseEvent
 * otherwise would make it possible to reliably generate
 * GDK_LEAVE_NOTIFY events, which would help get rid of those pesky
 * tooltips sometimes popping up in the wrong place.
 */

/* Do use SetCapture, it works now. Thanks to jpe@archaeopteryx.com */
#define USE_SETCAPTURE 1

#include <glib/gprintf.h>

#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"
#include "gdkkeysyms.h"

#ifdef G_WITH_CYGWIN
#include <fcntl.h>
#include <errno.h>
#endif

#include <objbase.h>

#if defined (__GNUC__) && defined (HAVE_DIMM_H)
/* The w32api imm.h clashes a bit with the IE5.5 dimm.h */
# define IMEMENUITEMINFOA hidden_IMEMENUITEMINFOA
# define IMEMENUITEMINFOW hidden_IMEMENUITEMINFOW
#endif

#include <imm.h>

#if defined (__GNUC__) && defined (HAVE_DIMM_H)
# undef IMEMENUITEMINFOA
# undef IMEMENUITEMINFOW
#endif

#ifdef HAVE_DIMM_H
#include <dimm.h>
#endif

/* 
 * Private function declarations
 */

static GdkFilterReturn
		 gdk_event_apply_filters(MSG      *msg,
					 GdkEvent *event,
					 GList    *filters);
static gboolean  gdk_event_translate	(GdkDisplay *display,
					 GdkEvent *event, 
					 MSG      *msg,
					 gboolean *ret_val_flagp,
					 gint     *ret_valp,
					 gboolean  return_exposes);

static gboolean gdk_event_prepare  (GSource     *source,
				    gint        *timeout);
static gboolean gdk_event_check    (GSource     *source);
static gboolean gdk_event_dispatch (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);

/* Private variable declarations
 */

static GdkWindow *p_grab_window = NULL; /* Window that currently
					 * holds the pointer grab
					 */

static GdkWindow *k_grab_window = NULL; /* Window the holds the
					 * keyboard grab
					 */

static GList *client_filters;	/* Filters for client messages */

static gboolean p_grab_automatic;
static GdkEventMask p_grab_mask;
static gboolean p_grab_owner_events, k_grab_owner_events;
static HCURSOR p_grab_cursor;

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  NULL
};

GPollFD event_poll_fd;

static GdkWindow *current_window = NULL;
static gint current_x, current_y;
static gdouble current_x_root, current_y_root;
static UINT gdk_ping_msg;
static UINT msh_mousewheel_msg;
static gboolean ignore_wm_char = FALSE;
static gboolean is_altgr_key = FALSE;

#ifdef HAVE_DIMM_H
static IActiveIMMApp *active_imm_app = NULL;
static IActiveIMMMessagePumpOwner *active_imm_msgpump_owner = NULL;
#endif

static gboolean use_ime_composition = FALSE;

static HKL latin_locale = NULL;

gulong
_gdk_win32_get_next_tick (gulong suggested_tick)
{
  static gulong cur_tick = 0;

  if (suggested_tick == 0)
    suggested_tick = GetTickCount ();
  if (suggested_tick <= cur_tick)
    return ++cur_tick;
  else
    return cur_tick = suggested_tick;
}

static LRESULT 
real_window_procedure (HWND   hwnd,
		       UINT   message,
		       WPARAM wparam,
		       LPARAM lparam)
{
  /* any way to have more than one display on win32 ? */
  GdkDisplay *display = gdk_display_get_default ();
  GdkEvent *event;
  MSG msg;
  DWORD pos;
#ifdef HAVE_DIMM_H
  LRESULT lres;
#endif
  gint ret_val;
  gboolean ret_val_flag;

  msg.hwnd = hwnd;
  msg.message = message;
  msg.wParam = wparam;
  msg.lParam = lparam;
  msg.time = _gdk_win32_get_next_tick (0);
  pos = GetMessagePos ();
  msg.pt.x = LOWORD (pos);
  msg.pt.y = HIWORD (pos);

  event = gdk_event_new (GDK_NOTHING);
  ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;
  if (gdk_event_translate (display, event, &msg, &ret_val_flag, &ret_val, FALSE))
    {
      ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;

      _gdk_event_queue_append (display, event);

      if (event->type == GDK_BUTTON_PRESS)
	_gdk_event_button_generate (display, event);
#if 1
      /* Wake up WaitMessage */
      PostMessage (NULL, gdk_ping_msg, 0, 0);
#endif
      
      if (ret_val_flag)
	return ret_val;
      else
	return FALSE;
    }
  else
    gdk_event_free (event);

  if (ret_val_flag)
    return ret_val;
  else
    {
#ifndef HAVE_DIMM_H
      return DefWindowProc (hwnd, message, wparam, lparam);
#else
      if (active_imm_app == NULL
	  || (*active_imm_app->lpVtbl->OnDefWindowProc) (active_imm_app, hwnd, message, wparam, lparam, &lres) == S_FALSE)
	return DefWindowProc (hwnd, message, wparam, lparam);
      else
	return lres;
#endif
    }
}

LRESULT CALLBACK
_gdk_win32_window_procedure (HWND   hwnd,
                             UINT   message,
                             WPARAM wparam,
                             LPARAM lparam)
{
  LRESULT retval;

  GDK_NOTE (MISC, g_print ("_gdk_win32_window_procedure: %p %s\n",
			   hwnd, _gdk_win32_message_to_string (message)));

  retval = real_window_procedure (hwnd, message, wparam, lparam);

  GDK_NOTE (MISC, g_print ("_gdk_win32_window_procedure: %p %s return %ld\n",
			   hwnd, _gdk_win32_message_to_string (message), retval));

  return retval;
}

void 
_gdk_events_init (void)
{
  GSource *source;
#ifdef HAVE_DIMM_H
  HRESULT hres;
#endif
  int i, j, n;

  /* List of languages that use a latin keyboard. Somewhat sorted in
   * "order of least surprise", in case we have to load one of them if
   * the user only has arabic loaded, for instance.
   */
  static int latin_languages[] = {
    LANG_ENGLISH,
    LANG_SPANISH,
    LANG_PORTUGUESE,
    LANG_FRENCH,
    LANG_GERMAN,
    /* Rest in numeric order */
    LANG_CZECH,
    LANG_DANISH,
    LANG_FINNISH,
    LANG_HUNGARIAN,
    LANG_ICELANDIC,
    LANG_ITALIAN,
    LANG_DUTCH,
    LANG_NORWEGIAN,
    LANG_POLISH,
    LANG_ROMANIAN,
    LANG_SLOVAK,
    LANG_ALBANIAN,
    LANG_SWEDISH,
    LANG_TURKISH,
    LANG_INDONESIAN,
    LANG_SLOVENIAN,
    LANG_ESTONIAN,
    LANG_LATVIAN,
    LANG_LITHUANIAN,
    LANG_VIETNAMESE,
    LANG_AFRIKAANS,
    LANG_FAEROESE,
    LANG_SWAHILI
  };

  gdk_ping_msg = RegisterWindowMessage ("gdk-ping");
  GDK_NOTE (EVENTS, g_print ("gdk-ping = %#x\n", gdk_ping_msg));

  /* This is the string MSH_MOUSEWHEEL from zmouse.h,
   * http://www.microsoft.com/mouse/intellimouse/sdk/zmouse.h
   * This message is used by mouse drivers than cannot generate WM_MOUSEWHEEL
   * or on Win95.
   */
  msh_mousewheel_msg = RegisterWindowMessage ("MSWHEEL_ROLLMSG");
  GDK_NOTE (EVENTS, g_print ("MSH_MOUSEWHEEL = %#x\n", msh_mousewheel_msg));

  /* Check if we have some input locale identifier loaded that uses a
   * latin keyboard, to be able to get the virtual-key code for the
   * latin characters corresponding to ASCII control characters.
   */
  if ((n = GetKeyboardLayoutList (0, NULL)) == 0)
    WIN32_API_FAILED ("GetKeyboardLayoutList");
  else
    {
      HKL *hkl_list = g_new (HKL, n);
      if (GetKeyboardLayoutList (n, hkl_list) == 0)
	WIN32_API_FAILED ("GetKeyboardLayoutList");
      else
	{
	  for (i = 0; latin_locale == NULL && i < n; i++)
	    for (j = 0; j < G_N_ELEMENTS (latin_languages); j++)
	      if (PRIMARYLANGID (LOWORD (hkl_list[i])) == latin_languages[j])
		{
		  latin_locale = hkl_list [i];
		  break;
		}
	}
      g_free (hkl_list);
    }

  if (latin_locale == NULL)
    {
      /* Try to load a keyboard layout with latin characters then.
       */
      i = 0;
      while (latin_locale == NULL && i < G_N_ELEMENTS (latin_languages))
	{
	  char id[9];
	  g_sprintf (id, "%08x", MAKELANGID (latin_languages[i++], SUBLANG_DEFAULT));
	  latin_locale = LoadKeyboardLayout (id, KLF_NOTELLSHELL|KLF_SUBSTITUTE_OK);
	}
    }

  GDK_NOTE (EVENTS, g_print ("latin_locale = %08x\n", (guint) latin_locale));

  source = g_source_new (&event_funcs, sizeof (GSource));
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);

#ifdef G_WITH_CYGWIN
  event_poll_fd.fd = open ("/dev/windows", O_RDONLY);
  if (event_poll_fd.fd == -1)
    g_error ("can't open \"/dev/windows\": %s", g_strerror (errno));
#else
  event_poll_fd.fd = G_WIN32_MSG_HANDLE;
#endif
  event_poll_fd.events = G_IO_IN;
  
  g_source_add_poll (source, &event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

#ifdef HAVE_DIMM_H
  hres = CoCreateInstance (&CLSID_CActiveIMM,
			   NULL,
			   CLSCTX_ALL,
			   &IID_IActiveIMMApp,
			   (LPVOID *) &active_imm_app);
  
  if (hres == S_OK)
    {
      GDK_NOTE (EVENTS, g_print ("IActiveIMMApp created %p\n",
				 active_imm_app));
      (*active_imm_app->lpVtbl->Activate) (active_imm_app, TRUE);
      
      hres = (*active_imm_app->lpVtbl->QueryInterface) (active_imm_app, &IID_IActiveIMMMessagePumpOwner, &active_imm_msgpump_owner);
      GDK_NOTE (EVENTS, g_print ("IActiveIMMMessagePumpOwner created %p\n",
				 active_imm_msgpump_owner));
      (active_imm_msgpump_owner->lpVtbl->Start) (active_imm_msgpump_owner);
    }
#endif

  if (IS_WIN_NT () && (_windows_version & 0xFF) == 5)
    {
      /* On Win2k (Beta 3, at least) WM_IME_CHAR doesn't seem to work
       * correctly for non-Unicode applications. Handle
       * WM_IME_COMPOSITION with GCS_RESULTSTR instead, fetch the
       * Unicode char from the IME with ImmGetCompositionStringW().
       */
      use_ime_composition = TRUE;
    }
}

/*
 *--------------------------------------------------------------
 * gdk_events_pending
 *
 *   Returns if events are pending on the queue.
 *
 * Arguments:
 *
 * Results:
 *   Returns TRUE if events are pending
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gboolean
gdk_events_pending (void)
{
  MSG msg;
  GdkDisplay *display = gdk_display_get_default ();

  return (_gdk_event_queue_find_first (display) ||
	  PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE));
}

/*
 *--------------------------------------------------------------
 * gdk_event_get_graphics_expose
 *
 *   Waits for a GraphicsExpose or NoExpose event
 *
 * Arguments:
 *
 * Results: 
 *   For GraphicsExpose events, returns a pointer to the event
 *   converted into a GdkEvent Otherwise, returns NULL.
 *
 * Side effects:
 *
 *-------------------------------------------------------------- */

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  MSG msg;
  GdkEvent *event;

  g_return_val_if_fail (window != NULL, NULL);
  
  GDK_NOTE (EVENTS, g_print ("gdk_event_get_graphics_expose\n"));

  if (PeekMessage (&msg, GDK_WINDOW_HWND (window), WM_PAINT, WM_PAINT, PM_REMOVE))
    {
      event = gdk_event_new (GDK_NOTHING);
      
      if (gdk_event_translate (gdk_drawable_get_display (window), 
                               event, &msg, NULL, NULL, TRUE))
	{
	  GDK_NOTE (EVENTS, g_print ("gdk_event_get_graphics_expose: got it!\n"));
	  return event;
	}
      else
	gdk_event_free (event);
    }
  
  GDK_NOTE (EVENTS, g_print ("gdk_event_get_graphics_expose: nope\n"));
  return NULL;	
}

static char *
event_mask_string (GdkEventMask mask)
{
  static char bfr[500];
  char *p = bfr;

  *p = '\0';
#define BIT(x) \
  if (mask & GDK_##x##_MASK) \
    p += g_sprintf (p, "%s" #x, (p > bfr ? " " : ""))
  BIT (EXPOSURE);
  BIT (POINTER_MOTION);
  BIT (POINTER_MOTION_HINT);
  BIT (BUTTON_MOTION);
  BIT (BUTTON1_MOTION);
  BIT (BUTTON2_MOTION);
  BIT (BUTTON3_MOTION);
  BIT (BUTTON_PRESS);
  BIT (BUTTON_RELEASE);
  BIT (KEY_PRESS);
  BIT (KEY_RELEASE);
  BIT (ENTER_NOTIFY);
  BIT (LEAVE_NOTIFY);
  BIT (FOCUS_CHANGE);
  BIT (STRUCTURE);
  BIT (PROPERTY_CHANGE);
  BIT (VISIBILITY_NOTIFY);
  BIT (PROXIMITY_IN);
  BIT (PROXIMITY_OUT);
  BIT (SUBSTRUCTURE);
  BIT (SCROLL);
#undef BIT

  return bfr;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_grab
 *
 *   Grabs the pointer to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "event_mask" masks only interesting events
 *   "confine_to" limits the cursor movement to the specified window
 *   "cursor" changes the cursor for the duration of the grab
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_pointer_ungrab
 *
 *--------------------------------------------------------------
 */

GdkGrabStatus
gdk_pointer_grab (GdkWindow    *window,
		  gboolean	owner_events,
		  GdkEventMask	event_mask,
		  GdkWindow    *confine_to,
		  GdkCursor    *cursor,
		  guint32	time)
{
  HWND hwnd_confined_to;
  HCURSOR hcursor;
  GdkCursorPrivate *cursor_private;
  gint return_val = GDK_GRAB_SUCCESS;

  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);
  
  cursor_private = (GdkCursorPrivate*) cursor;
  
  if (!confine_to || GDK_WINDOW_DESTROYED (confine_to))
    hwnd_confined_to = NULL;
  else
    hwnd_confined_to = GDK_WINDOW_HWND (confine_to);
  
  if (!cursor)
    hcursor = NULL;
  else
    hcursor = cursor_private->hcursor;
#if 0
  return_val = _gdk_input_grab_pointer (window,
					owner_events,
					event_mask,
					confine_to,
					time);
#endif
  if (return_val == GDK_GRAB_SUCCESS)
    {
      if (!GDK_WINDOW_DESTROYED (window))
	{
	  GDK_NOTE (EVENTS, g_print ("gdk_pointer_grab: %p %s %p %s\n",
				     GDK_WINDOW_HWND (window),
				     (owner_events ? "TRUE" : "FALSE"),
				     hcursor,
				     event_mask_string (event_mask)));
	  p_grab_mask = event_mask;
	  p_grab_owner_events = (owner_events != 0);
	  p_grab_automatic = FALSE;
	  
#if USE_SETCAPTURE
	  SetCapture (GDK_WINDOW_HWND (window));
#endif
	  return_val = GDK_GRAB_SUCCESS;
	}
      else
	return_val = GDK_GRAB_ALREADY_GRABBED;
    }
  
  if (return_val == GDK_GRAB_SUCCESS)
    {
      p_grab_window = window;
      p_grab_cursor = hcursor;
    }
  
  return return_val;
}

void
gdk_display_pointer_ungrab (GdkDisplay *display,
                            guint32     time)
{
  GDK_NOTE (EVENTS, g_print ("gdk_pointer_ungrab\n"));
  g_return_if_fail (display == gdk_display_get_default ());

#if 0
  _gdk_input_ungrab_pointer (time);
#endif

#if USE_SETCAPTURE
  if (GetCapture () != NULL)
    ReleaseCapture ();
#endif

  p_grab_window = NULL;
}

/*
 *--------------------------------------------------------------
 * find_window_for_pointer_event
 *
 *   Find the window a pointer event (mouse up, down, move) should
 *   be reported to.  If the return value != reported_window then
 *   the ref count of reported_window will be decremented and the
 *   ref count of the return value will be incremented.
 *
 * Arguments:
 *
 *  "reported_window" is the gdk window the xevent was reported relative to
 *  "xevent" is the win32 message
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static GdkWindow* 
find_window_for_pointer_event (GdkWindow*  reported_window,
                               MSG*        msg)
{
  HWND hwnd;
  POINTS points;
  POINT pt;
  GdkWindow* other_window;

  if (p_grab_window == NULL || !p_grab_owner_events)
    return reported_window;

  points = MAKEPOINTS (msg->lParam);
  pt.x = points.x;
  pt.y = points.y;
  ClientToScreen (msg->hwnd, &pt);

  GDK_NOTE (EVENTS, g_print ("Finding window for grabbed pointer event at (%ld, %ld)\n",
                             pt.x, pt.y));

  hwnd = WindowFromPoint (pt);
  if (hwnd == NULL)
    return reported_window;
  other_window = gdk_win32_handle_table_lookup ((GdkNativeWindow) hwnd);
  if (other_window == NULL)
    return reported_window;

  GDK_NOTE (EVENTS, g_print ("Found window %p for point (%ld, %ld)\n",
			     hwnd, pt.x, pt.y));

  gdk_window_unref (reported_window);
  gdk_window_ref (other_window);

  return other_window;
}

gboolean
gdk_display_pointer_is_grabbed (GdkDisplay *display)
{
  g_return_val_if_fail (display == gdk_display_get_default (), FALSE);
  GDK_NOTE (EVENTS, g_print ("gdk_pointer_is_grabbed: %s\n",
			     p_grab_window != NULL ? "TRUE" : "FALSE"));
  return p_grab_window != NULL;
}

gboolean
gdk_pointer_grab_info_libgtk_only (GdkDisplay *display,
				   GdkWindow **grab_window,
				   gboolean   *owner_events)
{
  g_return_val_if_fail (display == gdk_display_get_default (), FALSE);

  if (p_grab_window != NULL)
    {
      if (grab_window)
        *grab_window = p_grab_window;
      if (owner_events)
        *owner_events = p_grab_owner_events;

      return TRUE;
    }
  else
    return FALSE;
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_grab
 *
 *   Grabs the keyboard to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_keyboard_ungrab
 *
 *--------------------------------------------------------------
 */

GdkGrabStatus
gdk_keyboard_grab (GdkWindow *window,
		   gboolean   owner_events,
		   guint32    time)
{
  gint return_val;
  
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  
  GDK_NOTE (EVENTS, g_print ("gdk_keyboard_grab %p\n",
			     GDK_WINDOW_HWND (window)));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      k_grab_owner_events = owner_events != 0;
      return_val = GDK_GRAB_SUCCESS;
    }
  else
    return_val = GDK_GRAB_ALREADY_GRABBED;

  if (return_val == GDK_GRAB_SUCCESS)
    k_grab_window = window;
  
  return return_val;
}

void
gdk_display_keyboard_ungrab (GdkDisplay *display,
                             guint32 time)
{
  g_return_if_fail (display == gdk_display_get_default ());

  GDK_NOTE (EVENTS, g_print ("gdk_keyboard_ungrab\n"));

  k_grab_window = NULL;
}

gboolean
gdk_keyboard_grab_info_libgtk_only (GdkDisplay *display,
				    GdkWindow **grab_window,
				    gboolean   *owner_events)
{
  g_return_val_if_fail (display == gdk_display_get_default (), FALSE);

  if (k_grab_window)
    {
      if (grab_window)
        *grab_window = k_grab_window;
      if (owner_events)
        *owner_events = k_grab_owner_events;

      return TRUE;
    }
  else
    return FALSE;
}

static GdkFilterReturn
gdk_event_apply_filters (MSG      *msg,
			 GdkEvent *event,
			 GList    *filters)
{
  GdkEventFilter *filter;
  GList *tmp_list;
  GdkFilterReturn result;
  
  tmp_list = filters;
  
  while (tmp_list)
    {
      filter = (GdkEventFilter *) tmp_list->data;
      
      result = (*filter->function) (msg, event, filter->data);
      if (result !=  GDK_FILTER_CONTINUE)
	return result;
      
      tmp_list = tmp_list->next;
    }
  
  return GDK_FILTER_CONTINUE;
}

void 
gdk_display_add_client_message_filter (GdkDisplay   *display,
				       GdkAtom       message_type,
				       GdkFilterFunc func,
				       gpointer      data)
{
  /* XXX */
  gdk_add_client_message_filter (message_type, func, data);
}

void 
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
  GdkClientFilter *filter = g_new (GdkClientFilter, 1);

  filter->type = message_type;
  filter->function = func;
  filter->data = data;
  
  client_filters = g_list_prepend (client_filters, filter);
}

static void
build_key_event_state (GdkEvent *event)
{
  if (GetKeyState (VK_SHIFT) < 0)
    event->key.state |= GDK_SHIFT_MASK;
  if (GetKeyState (VK_CAPITAL) & 0x1)
    event->key.state |= GDK_LOCK_MASK;
  if (!is_altgr_key)
    {
      if (GetKeyState (VK_CONTROL) < 0)
	event->key.state |= GDK_CONTROL_MASK;
      if (GetKeyState (VK_MENU) < 0)
	event->key.state |= GDK_MOD1_MASK;
    }
  else
    {
      event->key.state |= GDK_MOD2_MASK;
      event->key.group = 1;
    }
}

static gint
build_pointer_event_state (MSG *msg)
{
  gint state;
  
  state = 0;
  if (msg->wParam & MK_CONTROL)
    state |= GDK_CONTROL_MASK;
  if (msg->wParam & MK_LBUTTON)
    state |= GDK_BUTTON1_MASK;
  if (msg->wParam & MK_MBUTTON)
    state |= GDK_BUTTON2_MASK;
  if (msg->wParam & MK_RBUTTON)
    state |= GDK_BUTTON3_MASK;
  if (msg->wParam & MK_SHIFT)
    state |= GDK_SHIFT_MASK;
  if (GetKeyState (VK_MENU) < 0)
    state |= GDK_MOD1_MASK;
  if (GetKeyState (VK_CAPITAL) & 0x1)
    state |= GDK_LOCK_MASK;

  return state;
}

static guint
vk_from_char (guint c)
{
  HKL locale = _gdk_input_locale;

  /* For some control characters (control-C, control-J and control-M),
   * VkKeyScanEx returns special keycodes (different from the
   * corresponding uncontrolified character).  Thus, for control
   * characters, uncontrolify it first and return the virtual-key code
   * it would have on a Latin-based keyboard, otherwise stuff breaks.
   */
  if (latin_locale != NULL && c >= '\000' && c <= '\032')
    {
      c += '@';
      locale = latin_locale;
    }

  return (VkKeyScanEx (c, locale) & 0xFF);
}

static void
build_keypress_event (GdkEvent *event,
		      MSG      *msg)
{
  HIMC himc;
  gint i, bytecount, ucount;
  guchar buf[100];
  wchar_t wbuf[100];

  event->key.type = GDK_KEY_PRESS;
  event->key.time = _gdk_win32_get_next_tick (msg->time);
  event->key.state = 0;
  event->key.group = 0;		/* ??? */
  event->key.keyval = GDK_VoidSymbol;
  
  if (msg->message == WM_IME_COMPOSITION)
    {
      himc = ImmGetContext (msg->hwnd);
      bytecount = ImmGetCompositionStringW (himc, GCS_RESULTSTR,
					    wbuf, sizeof (wbuf));
      ImmReleaseContext (msg->hwnd, himc);

      ucount = bytecount / 2;
      event->key.hardware_keycode = wbuf[0]; /* ??? */
    }
  else
    {
      if (msg->message == WM_CHAR || msg->message == WM_SYSCHAR)
	{
	  bytecount = MIN ((msg->lParam & 0xFFFF), sizeof (buf));
	  for (i = 0; i < bytecount; i++)
	    buf[i] = msg->wParam;
	  event->key.hardware_keycode = vk_from_char (msg->wParam);
	  if (msg->wParam < ' ')
	    {
	      /* For ASCII control chars, the keyval should be the
	       * corresponding ASCII character.
	       */
	      event->key.keyval = msg->wParam + '@';
	      /* This is needed in case of Alt+nnn or Alt+0nnn (on the numpad)
	       * where nnn<32
	       */
	      event->key.state |= GDK_CONTROL_MASK;
	    }
	}
      else /* WM_IME_CHAR */
	{
	  event->key.hardware_keycode = 0; /* ??? */
	  if (msg->wParam & 0xFF00)
	    {
	      /* Contrary to some versions of the documentation,
	       * the lead byte is the most significant byte.
	       */
	      buf[0] = ((msg->wParam >> 8) & 0xFF);
	      buf[1] = (msg->wParam & 0xFF);
	      bytecount = 2;
	    }
	  else
	    {
	      buf[0] = (msg->wParam & 0xFF);
	      bytecount = 1;
	    }
	}

      /* Convert from the thread's current code page
       * to Unicode. (Followed by conversion to UTF-8 below.)
       */
      ucount = MultiByteToWideChar (_gdk_input_codepage,
				    0, buf, bytecount,
				    wbuf, G_N_ELEMENTS (wbuf));
    }

  build_key_event_state (event);

  /* Build UTF-8 string */
  if (ucount > 0)
    {
      if (ucount == 1 && wbuf[0] < 0200)
	{
	  event->key.string = g_malloc (2);
	  event->key.string[0] = wbuf[0];
	  event->key.string[1] = '\0';
	  event->key.length = 1;
	}
      else
	{
	  event->key.string = _gdk_ucs2_to_utf8 (wbuf, ucount);
	  event->key.length = strlen (event->key.string);
	}
      if (event->key.keyval == GDK_VoidSymbol)
	event->key.keyval = gdk_unicode_to_keyval (wbuf[0]);
    }
}

static void
build_keyrelease_event (GdkEvent *event,
			MSG      *msg)
{
  guchar buf;
  wchar_t wbuf;

  event->key.type = GDK_KEY_RELEASE;
  event->key.time = _gdk_win32_get_next_tick (msg->time);
  event->key.state = 0;
  event->key.group = 0;		/* ??? */

  if (msg->message == WM_CHAR || msg->message == WM_SYSCHAR)
    {
      event->key.hardware_keycode = vk_from_char (msg->wParam);
      if (msg->wParam < ' ')
	{
	  event->key.keyval = msg->wParam + '@';
	  event->key.state |= GDK_CONTROL_MASK;
	}
      else
	{
	  buf = msg->wParam;
	  MultiByteToWideChar (_gdk_input_codepage,
			       0, &buf, 1, &wbuf, 1);
	  
	  event->key.keyval = gdk_unicode_to_keyval (wbuf);
	}
    }
  else
    {
      event->key.keyval = GDK_VoidSymbol;
      event->key.hardware_keycode = 0; /* ??? */
    }
  build_key_event_state (event);
  event->key.string = NULL;
  event->key.length = 0;
}

#ifdef G_ENABLE_DEBUG

static void
print_event_state (gint state)
{
#define CASE(bit) if (state & GDK_ ## bit ## _MASK) g_print (#bit " ");
  CASE (SHIFT);
  CASE (LOCK);
  CASE (CONTROL);
  CASE (MOD1);
  CASE (MOD2);
  CASE (MOD3);
  CASE (MOD4);
  CASE (MOD5);
  CASE (BUTTON1);
  CASE (BUTTON2);
  CASE (BUTTON3);
  CASE (BUTTON4);
  CASE (BUTTON5);
#undef CASE
}

static void
print_event (GdkEvent *event)
{
  gchar *escaped, *kvname;

  switch (event->any.type)
    {
#define CASE(x) case x: g_print ( #x " "); break;
    CASE (GDK_NOTHING);
    CASE (GDK_DELETE);
    CASE (GDK_DESTROY);
    CASE (GDK_EXPOSE);
    CASE (GDK_MOTION_NOTIFY);
    CASE (GDK_BUTTON_PRESS);
    CASE (GDK_2BUTTON_PRESS);
    CASE (GDK_3BUTTON_PRESS);
    CASE (GDK_BUTTON_RELEASE);
    CASE (GDK_KEY_PRESS);
    CASE (GDK_KEY_RELEASE);
    CASE (GDK_ENTER_NOTIFY);
    CASE (GDK_LEAVE_NOTIFY);
    CASE (GDK_FOCUS_CHANGE);
    CASE (GDK_CONFIGURE);
    CASE (GDK_MAP);
    CASE (GDK_UNMAP);
    CASE (GDK_PROPERTY_NOTIFY);
    CASE (GDK_SELECTION_CLEAR);
    CASE (GDK_SELECTION_REQUEST);
    CASE (GDK_SELECTION_NOTIFY);
    CASE (GDK_PROXIMITY_IN);
    CASE (GDK_PROXIMITY_OUT);
    CASE (GDK_DRAG_ENTER);
    CASE (GDK_DRAG_LEAVE);
    CASE (GDK_DRAG_MOTION);
    CASE (GDK_DRAG_STATUS);
    CASE (GDK_DROP_START);
    CASE (GDK_DROP_FINISHED);
    CASE (GDK_CLIENT_EVENT);
    CASE (GDK_VISIBILITY_NOTIFY);
    CASE (GDK_NO_EXPOSE);
    CASE (GDK_SCROLL);
    CASE (GDK_WINDOW_STATE);
    CASE (GDK_SETTING);
#undef CASE
    }
  g_print ("%p ", GDK_WINDOW_HWND (event->any.window));

  switch (event->any.type)
    {
    case GDK_EXPOSE:
      g_print ("%s %d",
	       _gdk_win32_gdkrectangle_to_string (&event->expose.area),
	       event->expose.count);
      break;
    case GDK_MOTION_NOTIFY:
      g_print ("(%.4g,%.4g) %s",
	       event->motion.x, event->motion.y,
	       event->motion.is_hint ? "HINT " : "");
      print_event_state (event->motion.state);
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      g_print ("%d (%.4g,%.4g) ",
	       event->button.button,
	       event->button.x, event->button.y);
      print_event_state (event->button.state);
      break;
    case GDK_KEY_PRESS: 
    case GDK_KEY_RELEASE:
      if (event->key.length == 0)
	escaped = g_strdup ("");
      else
	escaped = g_strescape (event->key.string, NULL);
      kvname = gdk_keyval_name (event->key.keyval);
      g_print ("%#.02x %d %s %d:\"%s\" ",
	       event->key.hardware_keycode, event->key.group,
	       (kvname ? kvname : "??"),
	       event->key.length,
	       escaped);
      g_free (escaped);
      print_event_state (event->key.state);
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      g_print ("%s ",
	       (event->crossing.detail == GDK_NOTIFY_INFERIOR ? "INFERIOR" :
		(event->crossing.detail == GDK_NOTIFY_ANCESTOR ? "ANCESTOR" :
		 (event->crossing.detail == GDK_NOTIFY_NONLINEAR ? "NONLINEAR" :
		  "???"))));
      break;
    case GDK_CONFIGURE:
      g_print ("x:%d y:%d w:%d h:%d",
	       event->configure.x, event->configure.y,
	       event->configure.width, event->configure.height);
      break;
    case GDK_SCROLL:
      g_print ("%s ",
	       (event->scroll.direction == GDK_SCROLL_UP ? "UP" :
		(event->scroll.direction == GDK_SCROLL_DOWN ? "DOWN" :
		 (event->scroll.direction == GDK_SCROLL_LEFT ? "LEFT" :
		  (event->scroll.direction == GDK_SCROLL_RIGHT ? "RIGHT" :
		   "???")))));
      print_event_state (event->scroll.state);
      break;
    case GDK_WINDOW_STATE:
      g_print ("%s: %s",
	       _gdk_win32_window_state_to_string (event->window_state.changed_mask),
	       _gdk_win32_window_state_to_string (event->window_state.new_window_state));
    default:
      /* Nothing */
      break;
    }  
  g_print ("\n");
}

#endif

static gboolean
gdk_window_is_child (GdkWindow *parent,
		     GdkWindow *window)
{
  if (parent == NULL || window == NULL)
    return FALSE;

  return (gdk_window_get_parent (window) == parent ||
	  gdk_window_is_child (parent, gdk_window_get_parent (window)));
}

static void
synthesize_enter_or_leave_event (GdkWindow    *window,
				 MSG          *msg,
				 GdkEventType  type,
				 GdkNotifyType detail,
				 gint          x,
				 gint          y)
{
  GdkEvent *event;
  
  event = gdk_event_new (type);
  event->crossing.window = window;
  event->crossing.send_event = FALSE;
  gdk_window_ref (event->crossing.window);
  event->crossing.subwindow = NULL;
  event->crossing.time = _gdk_win32_get_next_tick (msg->time);
  event->crossing.x = x;
  event->crossing.y = y;
  event->crossing.x_root = msg->pt.x;
  event->crossing.y_root = msg->pt.y;
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = detail;
  event->crossing.focus = TRUE; /* ??? */
  event->crossing.state = 0; /* ??? */
  
  _gdk_event_queue_append (gdk_drawable_get_display (window), event);
  
  if (type == GDK_ENTER_NOTIFY
      && GDK_WINDOW_OBJECT (window)->extension_events != 0)
    _gdk_input_enter_event (&event->crossing, window);

  GDK_NOTE (EVENTS, print_event (event));
}

static void
synthesize_leave_event (GdkWindow    *window,
			MSG          *msg,
			GdkNotifyType detail)
{
  POINT pt;

  if (!(GDK_WINDOW_OBJECT (window)->event_mask & GDK_LEAVE_NOTIFY_MASK))
    return;

  /* Leave events are at (current_x,current_y) in current_window */

  if (current_window != window)
    {
      pt.x = current_x;
      pt.y = current_y;
      ClientToScreen (GDK_WINDOW_HWND (current_window), &pt);
      ScreenToClient (GDK_WINDOW_HWND (window), &pt);
      synthesize_enter_or_leave_event (window, msg, GDK_LEAVE_NOTIFY, detail, pt.x, pt.y);
    }
  else
    synthesize_enter_or_leave_event (window, msg, GDK_LEAVE_NOTIFY, detail, current_x, current_y);

}
  
static void
synthesize_enter_event (GdkWindow    *window,
			MSG          *msg,
			GdkNotifyType detail)
{
  POINT pt;

  if (!(GDK_WINDOW_OBJECT (window)->event_mask & GDK_ENTER_NOTIFY_MASK))
    return;

  /* Enter events are at LOWORD (msg->lParam), HIWORD
   * (msg->lParam) in msg->hwnd */

  pt.x = LOWORD (msg->lParam);
  pt.y = HIWORD (msg->lParam);
  if (msg->hwnd != GDK_WINDOW_HWND (window))
    {
      ClientToScreen (msg->hwnd, &pt);
      ScreenToClient (GDK_WINDOW_HWND (window), &pt);
    }
  synthesize_enter_or_leave_event (window, msg, GDK_ENTER_NOTIFY, detail, pt.x, pt.y);
}
  
static void
synthesize_enter_events (GdkWindow    *from,
			 GdkWindow    *to,
			 MSG          *msg,
			 GdkNotifyType detail)
{
  GdkWindow *prev = gdk_window_get_parent (to);
  
  if (prev != from)
    synthesize_enter_events (from, prev, msg, detail);
  synthesize_enter_event (to, msg, detail);
}
			 
static void
synthesize_leave_events (GdkWindow    *from,
			 GdkWindow    *to,
			 MSG          *msg,
			 GdkNotifyType detail)
{
  GdkWindow *next = gdk_window_get_parent (from);
  
  synthesize_leave_event (from, msg, detail);
  if (next != to)
    synthesize_leave_events (next, to, msg, detail);
}
			 
static void
synthesize_crossing_events (GdkWindow *window,
			    MSG       *msg)
{
  GdkWindow *intermediate, *tem, *common_ancestor;

  if (gdk_window_is_child (current_window, window))
    {
      /* Pointer has moved to an inferior window. */
      synthesize_leave_event (current_window, msg, GDK_NOTIFY_INFERIOR);
      
      /* If there are intermediate windows, generate ENTER_NOTIFY
       * events for them
       */
      intermediate = gdk_window_get_parent (window);
      if (intermediate != current_window)
	{
	  synthesize_enter_events (current_window, intermediate, msg, GDK_NOTIFY_VIRTUAL);
	}
      
      synthesize_enter_event (window, msg, GDK_NOTIFY_ANCESTOR);
    }
  else if (gdk_window_is_child (window, current_window))
    {
      /* Pointer has moved to an ancestor window. */
      synthesize_leave_event (current_window, msg, GDK_NOTIFY_ANCESTOR);
      
      /* If there are intermediate windows, generate LEAVE_NOTIFY
       * events for them
       */
      intermediate = gdk_window_get_parent (current_window);
      if (intermediate != window)
	{
	  synthesize_leave_events (intermediate, window, msg, GDK_NOTIFY_VIRTUAL);
	}
    }
  else if (current_window)
    {
      /* Find least common ancestor of current_window and window */
      tem = current_window;
      do {
	common_ancestor = gdk_window_get_parent (tem);
	tem = common_ancestor;
      } while (common_ancestor &&
	       !gdk_window_is_child (common_ancestor, window));
      if (common_ancestor)
	{
	  synthesize_leave_event (current_window, msg, GDK_NOTIFY_NONLINEAR);
	  intermediate = gdk_window_get_parent (current_window);
	  if (intermediate != common_ancestor)
	    {
	      synthesize_leave_events (intermediate, common_ancestor,
				       msg, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  intermediate = gdk_window_get_parent (window);
	  if (intermediate != common_ancestor)
	    {
	      synthesize_enter_events (common_ancestor, intermediate,
				       msg, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  synthesize_enter_event (window, msg, GDK_NOTIFY_NONLINEAR);
	}
    }
  else
    {
      /* Dunno where we are coming from */
      synthesize_enter_event (window, msg, GDK_NOTIFY_UNKNOWN);
    }

  if (current_window)
    gdk_window_unref (current_window);
  current_window = window;
  if (current_window)
    gdk_window_ref (current_window);
}

#if 0

static GList *
get_descendants (GdkWindow *window)
{
  GList *list = gdk_window_get_children (window);
  GList *head = list;
  GList *tmp = NULL;

  while (list)
    {
      tmp = g_list_concat (tmp, get_descendants ((GdkWindow *) list->data));
      list = list->next;
    }

  return g_list_concat (tmp, head);
}

#endif

static void
synthesize_expose_events (GdkWindow *window)
{
  RECT r;
  HDC hdc;
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  GList *list = gdk_window_get_children (window);
  GList *head = list;
  GdkEvent *event;
  int k;
  
  while (list)
    {
      synthesize_expose_events ((GdkWindow *) list->data);
      list = list->next;
    }

  g_list_free (head);

  if (((GdkWindowObject *) window)->input_only)
    ;
  else if (!(hdc = GetDC (impl->handle)))
    WIN32_GDI_FAILED ("GetDC");
  else
    {
      if ((k = GetClipBox (hdc, &r)) == ERROR)
	WIN32_GDI_FAILED ("GetClipBox");
      else if (k != NULLREGION)
	{
	  event = gdk_event_new (GDK_EXPOSE);
	  event->expose.window = window;
	  gdk_window_ref (window);
	  event->expose.area.x = r.left;
	  event->expose.area.y = r.top;
	  event->expose.area.width = r.right - r.left;
	  event->expose.area.height = r.bottom - r.top;
	  event->expose.region = gdk_region_rectangle (&(event->expose.area));
	  event->expose.count = 0;
  
	  _gdk_event_queue_append (gdk_drawable_get_display (window), event);
  
	  GDK_NOTE (EVENTS_OR_COLORMAP, print_event (event));
	}
      if (!ReleaseDC (impl->handle, hdc))
	WIN32_GDI_FAILED ("ReleaseDC");
    }
}

static void
update_colors (GdkWindow *window,
	       gboolean   top)
{
  HDC hdc;
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  GList *list = gdk_window_get_children (window);
  GList *head = list;

  GDK_NOTE (COLORMAP, (top ? g_print ("update_colors:") : (void) 0));

  while (list)
    {
      update_colors ((GdkWindow *) list->data, FALSE);
      list = list->next;
    }
  g_list_free (head);

  if (((GdkWindowObject *) window)->input_only ||
      impl->colormap == NULL)
    return;

  if (!(hdc = GetDC (impl->handle)))
    WIN32_GDI_FAILED ("GetDC");
  else
    {
      GdkColormapPrivateWin32 *cmapp = GDK_WIN32_COLORMAP_DATA (impl->colormap);
      HPALETTE holdpal;
      gint k;
      
      if ((holdpal = SelectPalette (hdc, cmapp->hpal, TRUE)) == NULL)
	WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (hdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else
	{
	  GDK_NOTE (COLORMAP,
		    (k > 0 ?
		     g_print (" %p pal=%p: realized %d colors\n"
			      "update_colors:",
			      impl->handle, cmapp->hpal, k) :
		     (void) 0,
		     g_print (" %p", impl->handle)));
	  if (!UpdateColors (hdc))
	    WIN32_GDI_FAILED ("UpdateColors");
	  SelectPalette (hdc, holdpal, TRUE);
	  RealizePalette (hdc);
	}
      if (!ReleaseDC (impl->handle, hdc))
	WIN32_GDI_FAILED ("ReleaseDC");
    }
  GDK_NOTE (COLORMAP, (top ? g_print ("\n") : (void) 0));
}

static void
translate_mouse_coords (GdkWindow *window1,
			GdkWindow *window2,
			MSG       *msg)
{
  POINT pt;

  pt.x = LOWORD (msg->lParam);
  pt.y = HIWORD (msg->lParam);
  ClientToScreen (GDK_WINDOW_HWND (window1), &pt);
  ScreenToClient (GDK_WINDOW_HWND (window2), &pt);
  msg->lParam = MAKELPARAM (pt.x, pt.y);
  GDK_NOTE (EVENTS, g_print ("...new coords are (%ld,%ld)\n", pt.x, pt.y));
}

static gboolean
propagate (GdkWindow  **window,
	   MSG         *msg,
	   GdkWindow   *grab_window,
	   gboolean     grab_owner_events,
	   gint	        grab_mask,
	   gboolean   (*doesnt_want_it) (gint mask,
					 MSG *msg))
{
  gboolean in_propagation = FALSE;

  if (grab_window != NULL && !grab_owner_events)
    {
      /* Event source is grabbed with owner_events FALSE */
      GDK_NOTE (EVENTS, g_print ("...grabbed, owner_events FALSE, "));
      if ((*doesnt_want_it) (grab_mask, msg))
	{
	  GDK_NOTE (EVENTS, g_print ("...grabber doesn't want it\n"));
	  return FALSE;
	}
      else
	{
	  GDK_NOTE (EVENTS, g_print ("...sending to grabber %p\n",
				     GDK_WINDOW_HWND (grab_window)));
	  gdk_drawable_unref (*window);
	  *window = grab_window;
	  gdk_drawable_ref (*window);
	  return TRUE;
	}
    }
  while (TRUE)
    {
     if ((*doesnt_want_it) (GDK_WINDOW_OBJECT (*window)->event_mask, msg))
	{
	  /* Owner doesn't want it, propagate to parent. */
	  if (GDK_WINDOW (GDK_WINDOW_OBJECT (*window)->parent) == _gdk_parent_root)
	    {
	      /* No parent; check if grabbed */
	      if (grab_window != NULL)
		{
		  /* Event source is grabbed with owner_events TRUE */
		  GDK_NOTE (EVENTS, g_print ("...undelivered, but grabbed\n"));
		  if ((*doesnt_want_it) (grab_mask, msg))
		    {
		      /* Grabber doesn't want it either */
		      GDK_NOTE (EVENTS, g_print ("...grabber doesn't want it\n"));
		      return FALSE;
		    }
		  else
		    {
		      /* Grabbed! */
		      GDK_NOTE (EVENTS,
				g_print ("...sending to grabber %p\n",
					 GDK_WINDOW_HWND (grab_window)));
		      gdk_drawable_unref (*window);
		      *window = grab_window;
		      gdk_drawable_ref (*window);
		      return TRUE;
		    }
		}
	      else
		{
		  GDK_NOTE (EVENTS, g_print ("...undelivered\n"));
		  return FALSE;
		}
	    }
	  else if (GDK_WINDOW_OBJECT (*window)->parent == NULL)
	    {
	      GDK_NOTE (EVENTS, g_print ("...parent NULL (?), undelivered\n"));
	      return FALSE;
	    }
	  else
	    {
	      gdk_drawable_unref (*window);
	      *window = GDK_WINDOW (GDK_WINDOW_OBJECT (*window)->parent);
	      gdk_drawable_ref (*window);
	      GDK_NOTE (EVENTS, g_print ("%s %p",
					 (in_propagation ? "," : " ...propagating to"),
					 GDK_WINDOW_HWND (*window)));
	      /* The only branch where we actually continue the loop */
	      in_propagation = TRUE;
	    }
	}
      else
	return TRUE;
    }
}

static gboolean
doesnt_want_key (gint mask,
		 MSG *msg)
{
  return (((msg->message == WM_KEYUP || msg->message == WM_SYSKEYUP)
	   && !(mask & GDK_KEY_RELEASE_MASK))
	  ||
	  ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN)
	   && !(mask & GDK_KEY_PRESS_MASK)));
}

static gboolean
doesnt_want_char (gint mask,
		  MSG *msg)
{
  return !(mask & (GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK));
}

static gboolean
doesnt_want_button_press (gint mask,
			  MSG *msg)
{
  return !(mask & GDK_BUTTON_PRESS_MASK);
}

static gboolean
doesnt_want_button_release (gint mask,
			    MSG *msg)
{
  return !(mask & GDK_BUTTON_RELEASE_MASK);
}

static gboolean
doesnt_want_button_motion (gint mask,
			   MSG *msg)
{
  return !((mask & GDK_POINTER_MOTION_MASK)
	   || ((msg->wParam & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON))
	       && (mask & GDK_BUTTON_MOTION_MASK))
	   || ((msg->wParam & MK_LBUTTON)
	       && (mask & GDK_BUTTON1_MOTION_MASK))
	   || ((msg->wParam & MK_MBUTTON)
	       && (mask & GDK_BUTTON2_MOTION_MASK))
	   || ((msg->wParam & MK_RBUTTON)
	       && (mask & GDK_BUTTON3_MOTION_MASK)));
}

static gboolean
doesnt_want_scroll (gint mask,
		    MSG *msg)
{
#if 0
  return !(mask & GDK_SCROLL_MASK);
#else
  return !(mask & GDK_BUTTON_PRESS_MASK);
#endif
}

static char *
decode_key_lparam (LPARAM lParam)
{
  static char buf[100];
  char *p = buf;

  if (HIWORD (lParam) & KF_UP)
    p += g_sprintf (p, "KF_UP ");
  if (HIWORD (lParam) & KF_REPEAT)
    p += g_sprintf (p, "KF_REPEAT ");
  if (HIWORD (lParam) & KF_ALTDOWN)
    p += g_sprintf (p, "KF_ALTDOWN ");
  if (HIWORD (lParam) & KF_EXTENDED)
    p += g_sprintf (p, "KF_EXTENDED ");
  p += g_sprintf (p, "sc:%d rep:%d", LOBYTE (HIWORD (lParam)), LOWORD (lParam));

  return buf;
}

static void
erase_background (GdkWindow *window,
		  HDC        hdc)
{
  HDC bgdc = NULL;
  HBRUSH hbr = NULL;
  HPALETTE holdpal = NULL;
  RECT rect;
  COLORREF bg;
  GdkColormap *colormap;
  GdkColormapPrivateWin32 *colormap_private;
  int x, y;
  int x_offset, y_offset;
  
  if (((GdkWindowObject *) window)->input_only ||
      ((GdkWindowObject *) window)->bg_pixmap == GDK_NO_BG ||
      GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->position_info.no_bg)
    {
      GDK_NOTE (EVENTS, g_print (((GdkWindowObject *) window)->input_only ? "...input_only\n" :
				 ((GdkWindowObject *) window)->bg_pixmap == GDK_NO_BG ? "GDK_NO_BG" :
				 GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->position_info.no_bg ? "no_bg\n" : "???\n"));
      return;
    }

  colormap = gdk_drawable_get_colormap (window);

  if (colormap &&
      (colormap->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
       colormap->visual->type == GDK_VISUAL_STATIC_COLOR))
    {
      int k;
	  
      colormap_private = GDK_WIN32_COLORMAP_DATA (colormap);

      if (!(holdpal = SelectPalette (hdc,  colormap_private->hpal, FALSE)))
        WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (hdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else if (k > 0)
	GDK_NOTE (COLORMAP, g_print ("erase_background: realized %p: %d colors\n",
				     colormap_private->hpal, k));
    }
  
  x_offset = y_offset = 0;
  while (window && ((GdkWindowObject *) window)->bg_pixmap == GDK_PARENT_RELATIVE_BG)
    {
      /* If this window should have the same background as the parent,
       * fetch the parent. (And if the same goes for the parent, fetch
       * the grandparent, etc.)
       */
      x_offset += ((GdkWindowObject *) window)->x;
      y_offset += ((GdkWindowObject *) window)->y;
      window = GDK_WINDOW (((GdkWindowObject *) window)->parent);
    }
  
  if (GDK_WINDOW_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->position_info.no_bg)
    {
      GDK_NOTE (EVENTS, g_print ("no_bg on ancestor (?)\n"));
      /* Improves scolling effect, e.g. main buttons of testgtk */
      return;
    }

  GetClipBox (hdc, &rect);

  GDK_NOTE (EVENTS, (hbr = GetStockObject (BLACK_BRUSH),
		     FillRect (hdc, &rect, hbr),
		     GdiFlush (),
		     Sleep (200)));

  if (((GdkWindowObject *) window)->bg_pixmap == NULL)
    {
      bg = _gdk_win32_colormap_color (GDK_DRAWABLE_IMPL_WIN32 (((GdkWindowObject *) window)->impl)->colormap,
				      ((GdkWindowObject *) window)->bg_color.pixel);
      
      GDK_NOTE (EVENTS, g_print ("...%s bg %06lx\n",
				 _gdk_win32_rect_to_string (&rect),
				 (gulong) bg));
      if (!(hbr = CreateSolidBrush (bg)))
	WIN32_GDI_FAILED ("CreateSolidBrush");
      else if (!FillRect (hdc, &rect, hbr))
	WIN32_GDI_FAILED ("FillRect");
      if (hbr != NULL)
	DeleteObject (hbr);
    }
  else if (((GdkWindowObject *) window)->bg_pixmap != GDK_NO_BG)
    {
      GdkPixmap *pixmap = ((GdkWindowObject *) window)->bg_pixmap;
      GdkPixmapImplWin32 *pixmap_impl = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
      
      if (x_offset == 0 && y_offset == 0 &&
	  pixmap_impl->width <= 8 && pixmap_impl->height <= 8)
	{
	  GDK_NOTE (EVENTS, g_print ("...small pixmap, using brush\n"));
	  if (!(hbr = CreatePatternBrush (GDK_PIXMAP_HBITMAP (pixmap))))
	    WIN32_GDI_FAILED ("CreatePatternBrush");
	  else if (!FillRect (hdc, &rect, hbr))
	    WIN32_GDI_FAILED ("FillRect");
	  if (hbr != NULL)
	    DeleteObject (hbr);
	}
      else
	{
	  HGDIOBJ oldbitmap;

	  GDK_NOTE (EVENTS,
		    g_print ("...blitting pixmap %p (%dx%d) "
			     " clip box = %s\n",
			     GDK_PIXMAP_HBITMAP (pixmap),
			     pixmap_impl->width, pixmap_impl->height,
			     _gdk_win32_rect_to_string (&rect)));
	  
	  if (!(bgdc = CreateCompatibleDC (hdc)))
	    {
	      WIN32_GDI_FAILED ("CreateCompatibleDC");
	      return;
	    }
	  if (!(oldbitmap = SelectObject (bgdc, GDK_PIXMAP_HBITMAP (pixmap))))
	    {
	      WIN32_GDI_FAILED ("SelectObject");
	      DeleteDC (bgdc);
	      return;
	    }
	  x = -x_offset;
	  while (x < rect.right)
	    {
	      if (x + pixmap_impl->width >= rect.left)
		{
		  y = -y_offset;
		  while (y < rect.bottom)
		    {
		      if (y + pixmap_impl->height >= rect.top)
			{
			  if (!BitBlt (hdc, x, y,
				       pixmap_impl->width, pixmap_impl->height,
				       bgdc, 0, 0, SRCCOPY))
			    {
			      WIN32_GDI_FAILED ("BitBlt");
			      SelectObject (bgdc, oldbitmap);
			      DeleteDC (bgdc);
			      return;
			    }
			}
		      y += pixmap_impl->height;
		    }
		}
	      x += pixmap_impl->width;
	    }
	  SelectObject (bgdc, oldbitmap);
	  DeleteDC (bgdc);
	}
    }
}

static GdkRegion *
_gdk_win32_hrgn_to_region (HRGN hrgn)
{
  RGNDATA *rgndata;
  RECT *rects;
  GdkRegion *result;
  gint nbytes;
  gint i;

  if ((nbytes = GetRegionData (hrgn, 0, NULL)) == 0)
    {
      WIN32_GDI_FAILED ("GetRegionData");
      return NULL;
    }

  rgndata = (RGNDATA *) g_malloc (nbytes);

  if (GetRegionData (hrgn, nbytes, rgndata) == 0)
    {
      WIN32_GDI_FAILED ("GetRegionData");
      g_free (rgndata);
      return NULL;
    }

  result = gdk_region_new ();
  rects = (RECT *) rgndata->Buffer;
  for (i = 0; i < rgndata->rdh.nCount; i++)
    {
      GdkRectangle r;

      r.x = rects[i].left;
      r.y = rects[i].top;
      r.width = rects[i].right - r.x;
      r.height = rects[i].bottom - r.y;

      gdk_region_union_with_rect (result, &r);
    }

  g_free (rgndata);

  return result;
}

static gboolean
gdk_event_translate (GdkDisplay *display,
		     GdkEvent *event,
		     MSG      *msg,
		     gboolean *ret_val_flagp,
		     gint     *ret_valp,
		     gboolean  return_exposes)
{
  DWORD pidActWin;
  DWORD pidThis;
  PAINTSTRUCT paintstruct;
  HDC hdc;
  RECT rect;
  POINT pt;
  MINMAXINFO *mmi;
  HWND hwnd;
  HCURSOR hcursor;
  HRGN hrgn;
  CHARSETINFO charset_info;

  /* Invariant:
   * private == GDK_WINDOW_OBJECT (window)
   */
  GdkWindow *window;
  GdkWindowObject *private;

#define ASSIGN_WINDOW(rhs)						   \
  (window = rhs,							   \
   private = (GdkWindowObject *) window)

  GdkWindowImplWin32 *impl;

  GdkWindow *orig_window, *new_window;
  gint xoffset, yoffset;

  static gint update_colors_counter = 0;
  gint button;

  gchar buf[256];
  gboolean return_val = FALSE;
  
  if (ret_val_flagp)
    *ret_val_flagp = FALSE;

  ASSIGN_WINDOW (gdk_win32_handle_table_lookup ((GdkNativeWindow) msg->hwnd));
  orig_window = window;
  
  event->any.window = window;

  /* InSendMessage() does not really mean the same as X11's send_event flag,
   * but it is close enough, says jpe@archaeopteryx.com.
   */
  event->any.send_event = InSendMessage (); 

  if (window == NULL)
    {
      /* Handle WM_QUIT here ? */
      if (msg->message == WM_QUIT)
	{
	  GDK_NOTE (EVENTS, g_print ("WM_QUIT: %d\n", msg->wParam));
	  exit (msg->wParam);
	}
      else if (msg->message == WM_MOVE
	       || msg->message == WM_SIZE)
	{
	  /* It's quite normal to get these messages before we have
	   * had time to register the window in our lookup table, or
	   * when the window is being destroyed and we already have
	   * removed it. Repost the same message to our queue so that
	   * we will get it later when we are prepared.
	   */
	  GDK_NOTE (MISC, g_print ("gdk_event_translate: %p %s posted.\n",
				   msg->hwnd, 
				   msg->message == WM_MOVE ?
				   "WM_MOVE" : "WM_SIZE"));
	
	  PostMessage (msg->hwnd, msg->message,
		       msg->wParam, msg->lParam);
	}
#ifndef WITHOUT_WM_CREATE
      else if (WM_CREATE == msg->message)
	{
	  window = (UNALIGNED GdkWindow*) (((LPCREATESTRUCT) msg->lParam)->lpCreateParams);
	  GDK_WINDOW_HWND (window) = msg->hwnd;
	  GDK_NOTE (EVENTS, g_print ("gdk_event_translate: created %p\n",
				     msg->hwnd));
# if 0
	  /* This should handle allmost all the other window==NULL cases.
	   * This code is executed while gdk_window_new is in it's 
	   * CreateWindowEx call.
	   * Don't insert xid there a second time, if it's done here. 
	   */
	  gdk_drawable_ref (window);
	  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);
# endif
	}
      else
      {
        GDK_NOTE (EVENTS, g_print ("gdk_event_translate: %s for %p (NULL)\n",
                                   _gdk_win32_message_to_string (msg->message),
				   msg->hwnd));
      }
#endif
      return FALSE;
    }
  
  gdk_drawable_ref (window);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      /* Check for filters for this window */
      GdkFilterReturn result;

      result = gdk_event_apply_filters
	(msg, event, GDK_WINDOW_OBJECT (window)->filters);
      
      if (result != GDK_FILTER_CONTINUE)
	{
	  return_val =  (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	  if (ret_val_flagp)
	    *ret_val_flagp = TRUE;
	  if (ret_valp)
	    *ret_valp = return_val;
	  goto done;
	}
    }

  if (msg->message == msh_mousewheel_msg)
    {
      GDK_NOTE (EVENTS, g_print ("MSH_MOUSEWHEEL: %p %d\n",
				 msg->hwnd, msg->wParam));
      
      event->scroll.type = GDK_SCROLL;

      /* MSG_MOUSEWHEEL is delivered to the foreground window.  Work
       * around that. Also, the position is in screen coordinates, not
       * client coordinates as with the button messages.
       */
      pt.x = LOWORD (msg->lParam);
      pt.y = HIWORD (msg->lParam);
      if ((hwnd = WindowFromPoint (pt)) == NULL)
	goto done;

      msg->hwnd = hwnd;
      if ((new_window = gdk_win32_handle_table_lookup ((GdkNativeWindow) msg->hwnd)) == NULL)
	goto done;

      if (new_window != window)
	{
	  gdk_drawable_unref (window);
	  ASSIGN_WINDOW (new_window);
	  gdk_drawable_ref (window);
	}

      if (GDK_WINDOW_OBJECT (window)->extension_events != 0
	  && _gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  goto done;
	}

      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_scroll))
	goto done;

      ASSIGN_WINDOW (window);

      ScreenToClient (msg->hwnd, &pt);
      event->button.window = window;
      event->scroll.direction = ((int) msg->wParam > 0) ?
	GDK_SCROLL_UP : GDK_SCROLL_DOWN;
      event->scroll.window = window;
      event->scroll.time = _gdk_win32_get_next_tick (msg->time);
      _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
      event->scroll.x = (gint16) pt.x + xoffset;
      event->scroll.y = (gint16) pt.y + yoffset;
      event->scroll.x_root = (gint16) LOWORD (msg->lParam);
      event->scroll.y_root = (gint16) HIWORD (msg->lParam);
      event->scroll.state = 0;	/* No state information with MSH_MOUSEWHEEL */
      event->scroll.device = display->core_pointer;
      return_val = !GDK_WINDOW_DESTROYED (window);

      goto done;
    }
  else
    {
      GList *tmp_list;
      GdkFilterReturn result = GDK_FILTER_CONTINUE;

      tmp_list = client_filters;
      while (tmp_list)
	{
	  GdkClientFilter *filter = tmp_list->data;
	  /* FIXME: under win32 messages are not really atoms
	   * as the following cast suggest, but the appears to be right
	   * Haven't found a use case though ...
	   */
	  if (filter->type == GDK_POINTER_TO_ATOM (msg->message))
	    {
	      GDK_NOTE (EVENTS, g_print ("client filter matched\n"));
	      event->any.window = window;
	      result = (*filter->function) (msg, event, filter->data);
	      switch (result)
		{
		case GDK_FILTER_REMOVE:
		  *ret_val_flagp = TRUE;
		  *ret_valp = 0;
		  return_val = FALSE;
		  break;

		case GDK_FILTER_TRANSLATE:
		  return_val = TRUE;
		  break;

		case GDK_FILTER_CONTINUE:
		  *ret_val_flagp = TRUE;
		  *ret_valp = 1;
		  return_val = TRUE;
		  event->client.type = GDK_CLIENT_EVENT;
		  event->client.window = window;
		  /* FIXME: check if the cast is correct, see above */
		  event->client.message_type = GDK_POINTER_TO_ATOM (msg->message);
		  event->client.data_format = 0;
		  event->client.data.l[0] = msg->wParam;
		  event->client.data.l[1] = msg->lParam;
		  break;
		}
	      goto done;
	    }
	  tmp_list = tmp_list->next;
	}
    }

  switch (msg->message)
    {
    case WM_INPUTLANGCHANGE:
      _gdk_input_locale = (HKL) msg->lParam;
      TranslateCharsetInfo ((DWORD FAR *) msg->wParam,
			    &charset_info,
			    TCI_SRCCHARSET);
      _gdk_input_codepage = charset_info.ciACP;
      _gdk_keymap_serial++;
      GDK_NOTE (EVENTS,
		g_print ("WM_INPUTLANGCHANGE: %p  charset %lu locale %lx cp%d\n",
			 msg->hwnd, (gulong) msg->wParam, msg->lParam,
			 _gdk_input_codepage));
      break;

    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
      GDK_NOTE (EVENTS,
		g_print ("WM_SYSKEY%s: %p  %s ch:%.02x %s\n",
			 (msg->message == WM_SYSKEYUP ? "UP" : "DOWN"),
			 msg->hwnd,
			 (GetKeyNameText (msg->lParam, buf,
					  sizeof (buf)) > 0 ?
			  buf : ""),
			 msg->wParam,
			 decode_key_lparam (msg->lParam)));

      /* If posted without us having keyboard focus, ignore */
      if (msg->wParam != VK_F10 && !(HIWORD (msg->lParam) & KF_ALTDOWN))
	break;

      /* Let the system handle Alt-Tab, Alt-Enter and Alt-F4 */
      if (msg->wParam == VK_TAB
	  || msg->wParam == VK_RETURN
	  || msg->wParam == VK_F4)
	break;

      /* Let the system handle Alt-Space, and ignore the WM_SYSCHAR too */
      if (msg->wParam == VK_SPACE)
	{
	  ignore_wm_char = TRUE;
	  break;
	}

      /* Jump to code in common with WM_KEYUP and WM_KEYDOWN */
      goto keyup_or_down;

    case WM_KEYUP:
    case WM_KEYDOWN:
      GDK_NOTE (EVENTS, 
		g_print ("WM_KEY%s: %p  %s ch:%.02x %s\n",
			 (msg->message == WM_KEYUP ? "UP" : "DOWN"),
			 msg->hwnd,
			 (GetKeyNameText (msg->lParam, buf,
					  sizeof (buf)) > 0 ?
			  buf : ""),
			 msg->wParam,
			 decode_key_lparam (msg->lParam)));

    keyup_or_down:

      ignore_wm_char = TRUE;

      event->key.window = window;

      switch (msg->wParam)
	{
	case VK_LBUTTON:
	  event->key.keyval = GDK_Pointer_Button1; break;
	case VK_RBUTTON:
	  event->key.keyval = GDK_Pointer_Button3; break;
	case VK_MBUTTON:
	  event->key.keyval = GDK_Pointer_Button2; break;
	case VK_CANCEL:
	  event->key.keyval = GDK_Cancel; break;
	case VK_BACK:
	  event->key.keyval = GDK_BackSpace; break;
	case VK_TAB:
	  event->key.keyval = (GetKeyState (VK_SHIFT) < 0 ? 
	    GDK_ISO_Left_Tab : GDK_Tab);
	  break;
	case VK_CLEAR:
	  event->key.keyval = GDK_Clear; break;
	case VK_RETURN:
	  event->key.keyval = GDK_Return; break;
	case VK_SHIFT:
	  /* Don't let Shift auto-repeat */
	  if (msg->message == WM_KEYDOWN
	      && (HIWORD (msg->lParam) & KF_REPEAT))
	    ignore_wm_char = FALSE;
	  else
	    event->key.keyval = GDK_Shift_L;
	  break;
	case VK_CONTROL:
	  /* And not Control either */
	  if (msg->message == WM_KEYDOWN
	      && (HIWORD (msg->lParam) & KF_REPEAT))
	    ignore_wm_char = FALSE;
	  else if (HIWORD (msg->lParam) & KF_EXTENDED)
	    event->key.keyval = GDK_Control_R;
	  else
	    event->key.keyval = GDK_Control_L;
	  break;
	case VK_MENU:
	  /* And not Alt */
	  if (msg->message == WM_KEYDOWN
	      && (HIWORD (msg->lParam) & KF_REPEAT))
	    ignore_wm_char = FALSE;
	  else if (HIWORD (msg->lParam) & KF_EXTENDED)
	    {
	      /* AltGr key comes in as Control+Right Alt */
	      if (GetKeyState (VK_CONTROL) < 0)
		{
		  ignore_wm_char = FALSE;
		  is_altgr_key = TRUE;
		}
	      event->key.keyval = GDK_Alt_R;
	    }
	  else
	    {
	      event->key.keyval = GDK_Alt_L;
	      /* This needed in case she types Alt+nnn (on the numpad) */
	      ignore_wm_char = FALSE;
	    }
	  break;
	case VK_PAUSE:
	  event->key.keyval = GDK_Pause; break;
	case VK_CAPITAL:
	  event->key.keyval = GDK_Caps_Lock; break;
	case VK_ESCAPE:
	  event->key.keyval = GDK_Escape; break;
	case VK_PRIOR:
	  event->key.keyval = GDK_Prior; break;
	case VK_NEXT:
	  event->key.keyval = GDK_Next; break;
	case VK_END:
	  event->key.keyval = GDK_End; break;
	case VK_HOME:
	  event->key.keyval = GDK_Home; break;
	case VK_LEFT:
	  event->key.keyval = GDK_Left; break;
	case VK_UP:
	  event->key.keyval = GDK_Up; break;
	case VK_RIGHT:
	  event->key.keyval = GDK_Right; break;
	case VK_DOWN:
	  event->key.keyval = GDK_Down; break;
	case VK_SELECT:
	  event->key.keyval = GDK_Select; break;
	case VK_PRINT:
	  event->key.keyval = GDK_Print; break;
	case VK_EXECUTE:
	  event->key.keyval = GDK_Execute; break;
	case VK_INSERT:
	  event->key.keyval = GDK_Insert; break;
	case VK_DELETE:
	  event->key.keyval = GDK_Delete; break;
	case VK_HELP:
	  event->key.keyval = GDK_Help; break;
	case VK_NUMPAD0:
	case VK_NUMPAD1:
	case VK_NUMPAD2:
	case VK_NUMPAD3:
	case VK_NUMPAD4:
	case VK_NUMPAD5:
	case VK_NUMPAD6:
	case VK_NUMPAD7:
	case VK_NUMPAD8:
	case VK_NUMPAD9:
	  /* Apparently applications work better if we just pass numpad digits
	   * on as real digits? So wait for the WM_CHAR instead.
	   */
	  ignore_wm_char = FALSE;
	  break;
	case VK_MULTIPLY:
	  event->key.keyval = GDK_KP_Multiply; break;
	case VK_ADD:
	  /* Pass it on as an ASCII plus in WM_CHAR. */
	  ignore_wm_char = FALSE;
	  break;
	case VK_SEPARATOR:
	  event->key.keyval = GDK_KP_Separator; break;
	case VK_SUBTRACT:
	  /* Pass it on as an ASCII minus in WM_CHAR. */
	  ignore_wm_char = FALSE;
	  break;
	case VK_DECIMAL:
	  /* The keypad decimal key should also be passed on as the decimal
	   * sign ('.' or ',' depending on the Windows locale settings,
	   * apparently). So wait for the WM_CHAR here, also.
	   */
	  ignore_wm_char = FALSE;
	  break;
	case VK_DIVIDE:
	  event->key.keyval = GDK_KP_Divide; break;
	case VK_F1:
	  event->key.keyval = GDK_F1; break;
	case VK_F2:
	  event->key.keyval = GDK_F2; break;
	case VK_F3:
	  event->key.keyval = GDK_F3; break;
	case VK_F4:
	  event->key.keyval = GDK_F4; break;
	case VK_F5:
	  event->key.keyval = GDK_F5; break;
	case VK_F6:
	  event->key.keyval = GDK_F6; break;
	case VK_F7:
	  event->key.keyval = GDK_F7; break;
	case VK_F8:
	  event->key.keyval = GDK_F8; break;
	case VK_F9:
	  event->key.keyval = GDK_F9; break;
	case VK_F10:
	  event->key.keyval = GDK_F10; break;
	case VK_F11:
	  event->key.keyval = GDK_F11; break;
	case VK_F12:
	  event->key.keyval = GDK_F12; break;
	case VK_F13:
	  event->key.keyval = GDK_F13; break;
	case VK_F14:
	  event->key.keyval = GDK_F14; break;
	case VK_F15:
	  event->key.keyval = GDK_F15; break;
	case VK_F16:
	  event->key.keyval = GDK_F16; break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  if (!is_altgr_key && (GetKeyState (VK_CONTROL) < 0
				|| GetKeyState (VK_MENU) < 0))
	    /* Control- or Alt-digits won't come in as a WM_CHAR,
	     * but beware of AltGr-digits, which are used for instance
	     * on Finnish keyboards.
	     */
	    event->key.keyval = GDK_0 + (msg->wParam - '0');
	  else
	    ignore_wm_char = FALSE;
	  break;
	case VK_OEM_PLUS:	/* On my Win98, the '+' key comes in
				 * as VK_OEM_PLUS, etc.
				 */
	case VK_OEM_COMMA:
	case VK_OEM_MINUS:
	case VK_OEM_PERIOD:
	case VK_OEM_2:
	case VK_OEM_4:
	case VK_OEM_5:
	case VK_OEM_6:
	  if (!is_altgr_key && (GetKeyState (VK_CONTROL) < 0
				|| GetKeyState (VK_MENU) < 0))
	    /* Control- or Alt-plus won't come in as WM_CHAR,
	     * but beware of AltGr-plus which is backslash on
	     * Finnish keyboards
	     */
	    /* All these VK_OEM keycodes happen to be the corresponding ASCII
	     * char + 0x90
	     */
	    event->key.keyval = msg->wParam - 0x90;
	  else
	    ignore_wm_char = FALSE;
	  break;
	case VK_OEM_1:
	  if (!is_altgr_key && (GetKeyState (VK_CONTROL) < 0
				|| GetKeyState (VK_MENU) < 0))
	    /* ;: on US keyboard */
	    event->key.keyval = ';';
	  else
	    ignore_wm_char = FALSE;
	  break;
	case VK_OEM_3:
	  if (!is_altgr_key && (GetKeyState (VK_CONTROL) < 0
				|| GetKeyState (VK_MENU) < 0))
	    /* `~ on US keyboard */
	    event->key.keyval = '`';
	  else
	    ignore_wm_char = FALSE;
	  break;
	case VK_OEM_7:
	  if (!is_altgr_key && (GetKeyState (VK_CONTROL) < 0
				|| GetKeyState (VK_MENU) < 0))
	    /* '" on US keyboard */
	    event->key.keyval = '\'';
	  else
	    ignore_wm_char = FALSE;
	  break;
	default:
	  if (msg->message == WM_SYSKEYDOWN || msg->message == WM_SYSKEYUP)
	    event->key.keyval = msg->wParam;
	  else
	    ignore_wm_char = FALSE;
	  break;
	}

      if (!ignore_wm_char)
	break;

      if (!propagate (&window, msg,
		      k_grab_window, k_grab_owner_events, GDK_ALL_EVENTS_MASK,
		      doesnt_want_key))
	break;
      ASSIGN_WINDOW (window);

      is_altgr_key = FALSE;
      event->key.type = ((msg->message == WM_KEYDOWN
			  || msg->message == WM_SYSKEYDOWN) ?
			 GDK_KEY_PRESS : GDK_KEY_RELEASE);
      event->key.time = _gdk_win32_get_next_tick (msg->time);
      event->key.state = 0;
      if (GetKeyState (VK_SHIFT) < 0)
	event->key.state |= GDK_SHIFT_MASK;
      if (GetKeyState (VK_CAPITAL) & 0x1)
	event->key.state |= GDK_LOCK_MASK;
      if (GetKeyState (VK_CONTROL) < 0)
	event->key.state |= GDK_CONTROL_MASK;
      if (msg->wParam != VK_MENU && GetKeyState (VK_MENU) < 0)
	event->key.state |= GDK_MOD1_MASK;
      event->key.hardware_keycode = msg->wParam;
      event->key.group = 0;
      event->key.string = NULL;
      event->key.length = 0;
      return_val = !GDK_WINDOW_DESTROYED (window);
      break;

    case WM_IME_COMPOSITION:
      if (!use_ime_composition)
	break;

      GDK_NOTE (EVENTS, g_print ("WM_IME_COMPOSITION: %p  %#lx\n",
				 msg->hwnd, msg->lParam));
      if (msg->lParam & GCS_RESULTSTR)
	goto wm_char;
      break;

    case WM_IME_CHAR:
      GDK_NOTE (EVENTS,
		g_print ("WM_IME_CHAR: %p  bytes: %#.04x\n",
			 msg->hwnd, msg->wParam));
      goto wm_char;
      
    case WM_CHAR:
    case WM_SYSCHAR:
      GDK_NOTE (EVENTS, 
		g_print ("WM_%sCHAR: %p  %#x %s %s\n",
			 (msg->message == WM_CHAR ? "" : "SYS"),
			 msg->hwnd, msg->wParam,
			 decode_key_lparam (msg->lParam),
			 (ignore_wm_char ? "ignored" : "")));

      if (ignore_wm_char)
	{
	  ignore_wm_char = FALSE;
	  break;
	}

    wm_char:
      if (!propagate (&window, msg,
		      k_grab_window, k_grab_owner_events, GDK_ALL_EVENTS_MASK,
		      doesnt_want_char))
	  break;
      ASSIGN_WINDOW (window);

      event->key.window = window;
      return_val = !GDK_WINDOW_DESTROYED (window);

      if (return_val && (event->key.window == k_grab_window
			 || (private->event_mask & GDK_KEY_RELEASE_MASK)))
	{
	  if (window == k_grab_window
	      || (private->event_mask & GDK_KEY_PRESS_MASK))
	    {
	      /* Append a GDK_KEY_PRESS event to the pushback list
	       * (from which it will be fetched before the release
	       * event).
	       */
	      GdkEvent *event2 = gdk_event_new (GDK_KEY_PRESS);
	      build_keypress_event (event2, msg);
	      event2->key.window = window;
	      gdk_drawable_ref (window);
	      _gdk_event_queue_append (display, event2);
	      GDK_NOTE (EVENTS, print_event (event2));
	    }
	  /* Return the key release event.  */
	  build_keyrelease_event (event, msg);
	}
      else if (return_val && (private->event_mask & GDK_KEY_PRESS_MASK))
	{
	  /* Return just the key press event. */
	  build_keypress_event (event, msg);
	}
      else
	return_val = FALSE;

#if 0 /* Don't reset is_AltGr_key here. Othewise we can't type several
       * AltGr-accessed chars while keeping the AltGr pressed down
       * all the time.
       */
      is_AltGr_key = FALSE;
#endif
      break;

    case WM_LBUTTONDOWN:
      button = 1;
      goto buttondown0;
    case WM_MBUTTONDOWN:
      button = 2;
      goto buttondown0;
    case WM_RBUTTONDOWN:
      button = 3;

    buttondown0:
      GDK_NOTE (EVENTS, 
		g_print ("WM_%cBUTTONDOWN: %p  (%d,%d)\n",
			 " LMR"[button],
			 msg->hwnd,
			 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      if (GDK_WINDOW_OBJECT (window)->extension_events != 0
	  && _gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      ASSIGN_WINDOW (find_window_for_pointer_event (window, msg));

      if (window != current_window)
	synthesize_crossing_events (window, msg);

      event->button.type = GDK_BUTTON_PRESS;
      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_button_press))
	  break;
      ASSIGN_WINDOW (window);

      event->button.window = window;

      /* Emulate X11's automatic active grab */
      if (!p_grab_window)
	{
	  /* No explicit active grab, let's start one automatically */
	  gint owner_events = (private->event_mask & (GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK));
	  
	  GDK_NOTE (EVENTS, g_print ("...automatic grab started\n"));
	  gdk_pointer_grab (window,
			    owner_events,
			    private->event_mask,
			    NULL, NULL, 0);
	  p_grab_automatic = TRUE;
	}

      event->button.time = _gdk_win32_get_next_tick (msg->time);
      if (window != orig_window)
	translate_mouse_coords (orig_window, window, msg);
      event->button.x = current_x = (gint16) LOWORD (msg->lParam);
      event->button.y = current_y = (gint16) HIWORD (msg->lParam);
      _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
      event->button.x += xoffset;  /* XXX translate current_x, y too? */
      event->button.y += yoffset;
      event->button.x_root = current_x_root = msg->pt.x;
      event->button.y_root = current_y_root = msg->pt.y;
      event->button.axes = NULL;
      event->button.state = build_pointer_event_state (msg);
      event->button.button = button;
      event->button.device = display->core_pointer;

      return_val = !GDK_WINDOW_DESTROYED (window);
      break;

    case WM_LBUTTONUP:
      button = 1;
      goto buttonup0;
    case WM_MBUTTONUP:
      button = 2;
      goto buttonup0;
    case WM_RBUTTONUP:
      button = 3;

    buttonup0:
      GDK_NOTE (EVENTS, 
		g_print ("WM_%cBUTTONUP: %p  (%d,%d)\n",
			 " LMR"[button],
			 msg->hwnd,
			 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      ASSIGN_WINDOW (find_window_for_pointer_event (window, msg));

      if (GDK_WINDOW_OBJECT (window)->extension_events != 0
	  && _gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      if (window != current_window)
	synthesize_crossing_events (window, msg);

      event->button.type = GDK_BUTTON_RELEASE;
      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_button_release))
	{
	}
      else
	{
	  ASSIGN_WINDOW (window);

	  event->button.window = window;
	  event->button.time = _gdk_win32_get_next_tick (msg->time);
	  if (window != orig_window)
	    translate_mouse_coords (orig_window, window, msg);
	  _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
	  event->button.x = (gint16) LOWORD (msg->lParam) + xoffset;
	  event->button.y = (gint16) HIWORD (msg->lParam) + yoffset;
	  event->button.x_root = current_x_root = msg->pt.x;
	  event->button.y_root = current_y_root = msg->pt.y;
	  event->button.axes = NULL;
	  event->button.state = build_pointer_event_state (msg);
	  event->button.button = button;
	  event->button.device = display->core_pointer;
	  
	  return_val = !GDK_WINDOW_DESTROYED (window);
	}

      if (p_grab_window != NULL
	  && p_grab_automatic
	  && (msg->wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) == 0)
	gdk_pointer_ungrab (0);
      break;

    case WM_MOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print ("WM_MOUSEMOVE: %p  %#x (%d,%d)\n",
			 msg->hwnd, msg->wParam,
			 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      ASSIGN_WINDOW (find_window_for_pointer_event (window, msg));

      /* If we haven't moved, don't create any event.
       * Windows sends WM_MOUSEMOVE messages after button presses
       * even if the mouse doesn't move. This disturbs gtk.
       */
      if (window == current_window
	  && LOWORD (msg->lParam) == current_x
	  && HIWORD (msg->lParam) == current_y)
	break;

      /* HB: only process mouse move messages if we own the active window. */
      GetWindowThreadProcessId (GetActiveWindow (), &pidActWin);
      GetWindowThreadProcessId (msg->hwnd, &pidThis);
      if (pidActWin != pidThis)
	break;

      if (window != current_window)
	synthesize_crossing_events (window, msg);

      if (GDK_WINDOW_OBJECT (window)->extension_events != 0
	  && _gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      event->motion.type = GDK_MOTION_NOTIFY;
      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_button_motion))
	  break;
      ASSIGN_WINDOW (window);

      event->motion.window = window;
      event->motion.time = _gdk_win32_get_next_tick (msg->time);
      if (window != orig_window)
	translate_mouse_coords (orig_window, window, msg);
      event->motion.x = current_x = (gint16) LOWORD (msg->lParam);
      event->motion.y = current_y = (gint16) HIWORD (msg->lParam);
      _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
      event->motion.x += xoffset;
      event->motion.y += yoffset;
      event->motion.x_root = current_x_root = msg->pt.x;
      event->motion.y_root = current_y_root = msg->pt.y;
      event->motion.axes = NULL;
      event->motion.state = build_pointer_event_state (msg);
      event->motion.is_hint = FALSE;
      event->motion.device = display->core_pointer;

      return_val = !GDK_WINDOW_DESTROYED (window);
      break;

    case WM_NCMOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print ("WM_NCMOUSEMOVE: %p  x,y: %d %d\n",
			 msg->hwnd,
			 LOWORD (msg->lParam), HIWORD (msg->lParam)));
      if (current_window != NULL
	  && (GDK_WINDOW_OBJECT (current_window)->event_mask & GDK_LEAVE_NOTIFY_MASK))
	{
	  GDK_NOTE (EVENTS, g_print ("...synthesizing LEAVE_NOTIFY event\n"));

	  event->crossing.type = GDK_LEAVE_NOTIFY;
	  event->crossing.window = current_window;
	  event->crossing.subwindow = NULL;
	  event->crossing.time = _gdk_win32_get_next_tick (msg->time);
	  _gdk_windowing_window_get_offsets (current_window, &xoffset, &yoffset);
	  event->crossing.x = current_x + xoffset;
	  event->crossing.y = current_y + yoffset;
	  event->crossing.x_root = current_x_root;
	  event->crossing.y_root = current_y_root;
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;

	  event->crossing.focus = TRUE; /* ??? */
	  event->crossing.state = 0; /* ??? */
	  return_val = TRUE;
	}

      if (current_window)
	{
	  gdk_drawable_unref (current_window);
	  current_window = NULL;
	}

      break;

    case WM_MOUSEWHEEL:
      GDK_NOTE (EVENTS, g_print ("WM_MOUSEWHEEL: %p %d\n",
				 msg->hwnd, HIWORD (msg->wParam)));

      event->scroll.type = GDK_SCROLL;

      /* WM_MOUSEWHEEL is delivered to the focus window Work around
       * that. Also, the position is in screen coordinates, not client
       * coordinates as with the button messages. I love the
       * consistency of Windows.
       */
      pt.x = LOWORD (msg->lParam);
      pt.y = HIWORD (msg->lParam);
      if ((hwnd = WindowFromPoint (pt)) == NULL)
	break;

      msg->hwnd = hwnd;
      if ((new_window = gdk_win32_handle_table_lookup ((GdkNativeWindow) msg->hwnd)) == NULL)
	break;

      if (new_window != window)
	{
	  gdk_drawable_unref (window);
	  ASSIGN_WINDOW (new_window);
	  gdk_drawable_ref (window);
	}

      if (GDK_WINDOW_OBJECT (window)->extension_events != 0
	  && _gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      if (!propagate (&window, msg,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_scroll))
	break;

      ASSIGN_WINDOW (window);

      ScreenToClient (msg->hwnd, &pt);
      event->button.window = window;
      event->scroll.direction = (((short) HIWORD (msg->wParam)) > 0) ?
	GDK_SCROLL_UP : GDK_SCROLL_DOWN;
      event->scroll.window = window;
      event->scroll.time = _gdk_win32_get_next_tick (msg->time);
      _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
      event->scroll.x = (gint16) pt.x + xoffset;
      event->scroll.y = (gint16) pt.y + yoffset;
      event->scroll.x_root = (gint16) LOWORD (msg->lParam);
      event->scroll.y_root = (gint16) HIWORD (msg->lParam);
      event->scroll.state = build_pointer_event_state (msg);
      event->scroll.device = display->core_pointer;
      return_val = !GDK_WINDOW_DESTROYED (window);
      
      break;

    case WM_QUERYNEWPALETTE:
      GDK_NOTE (EVENTS_OR_COLORMAP, g_print ("WM_QUERYNEWPALETTE: %p\n",
					     msg->hwnd));
      if (gdk_visual_get_system ()->type == GDK_VISUAL_PSEUDO_COLOR)
	{
	  synthesize_expose_events (window);
	  update_colors_counter = 0;
	}
      *ret_val_flagp = TRUE;
      *ret_valp = FALSE;
      break;

    case WM_PALETTECHANGED:
      GDK_NOTE (EVENTS_OR_COLORMAP, g_print ("WM_PALETTECHANGED: %p %p\n",
					     msg->hwnd, (HWND) msg->wParam));
      *ret_val_flagp = TRUE;
      *ret_valp = FALSE;

      if (gdk_visual_get_system ()->type != GDK_VISUAL_PSEUDO_COLOR)
	break;

      if (msg->hwnd == (HWND) msg->wParam)
	break;

      if (++update_colors_counter == 5)
	{
	  synthesize_expose_events (window);
	  update_colors_counter = 0;
	  break;
	}
      
      update_colors (window, TRUE);
      break;

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
      GDK_NOTE (EVENTS, g_print ("WM_%sFOCUS: %p\n",
				 (msg->message == WM_SETFOCUS ?
				  "SET" : "KILL"),
				 msg->hwnd));
      
      if (!(private->event_mask & GDK_FOCUS_CHANGE_MASK))
	break;

      event->focus_change.type = GDK_FOCUS_CHANGE;
      event->focus_change.window = window;
      event->focus_change.in = (msg->message == WM_SETFOCUS);
      return_val = !GDK_WINDOW_DESTROYED (window);
      break;

    case WM_ERASEBKGND:
      GDK_NOTE (EVENTS, g_print ("WM_ERASEBKGND: %p  dc %p\n",
				 msg->hwnd, (HANDLE) msg->wParam));
      
      if (GDK_WINDOW_DESTROYED (window))
	break;

      erase_background (window, (HDC) msg->wParam);
      *ret_val_flagp = TRUE; /* always claim as handled */
      *ret_valp = 1;
      break;

    case WM_PAINT:
      hrgn = CreateRectRgn (0, 0, 0, 0);
      if (GetUpdateRgn (msg->hwnd, hrgn, FALSE) == ERROR)
	{
	  WIN32_GDI_FAILED ("GetUpdateRgn");
	  break;
	}

      hdc = BeginPaint (msg->hwnd, &paintstruct);

      GDK_NOTE (EVENTS, g_print ("WM_PAINT: %p  %s %s dc %p%s\n",
				 msg->hwnd,
				 _gdk_win32_rect_to_string (&paintstruct.rcPaint),
				 (paintstruct.fErase ? "erase" : ""),
				 hdc,
				 (return_exposes ? " return_exposes" : "")));

      EndPaint (msg->hwnd, &paintstruct);

      /* HB: don't generate GDK_EXPOSE events for InputOnly
       * windows -> backing store now works!
       */
      if (GDK_WINDOW_OBJECT (window)->input_only)
	{
	  DeleteObject (hrgn);
	  break;
	}

      if (!(private->event_mask & GDK_EXPOSURE_MASK))
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  DeleteObject (hrgn);
	  break;
	}

#if 0 /* we need to process exposes even with GDK_NO_BG
       * Otherwise The GIMP canvas update is broken ....
       */
      if (GDK_WINDOW_OBJECT (window)->bg_pixmap == GDK_NO_BG)
	break;
#endif

      if ((paintstruct.rcPaint.right == paintstruct.rcPaint.left)
          || (paintstruct.rcPaint.bottom == paintstruct.rcPaint.top))
	{
	  GDK_NOTE (EVENTS, g_print ("...empty paintstruct, ignored\n"));
	  DeleteObject (hrgn);
	  break;
	}

      if (return_exposes)
        {
          event->expose.type = GDK_EXPOSE;
          event->expose.window = window;
          event->expose.area.x = paintstruct.rcPaint.left;
          event->expose.area.y = paintstruct.rcPaint.top;
          event->expose.area.width = paintstruct.rcPaint.right - paintstruct.rcPaint.left;
          event->expose.area.height = paintstruct.rcPaint.bottom - paintstruct.rcPaint.top;
          event->expose.region = _gdk_win32_hrgn_to_region (hrgn);
          event->expose.count = 0;

          return_val = !GDK_WINDOW_DESTROYED (window);
          if (return_val)
            {
              GList *list = display->queued_events;
              while (list != NULL )
                {
		  GdkEventPrivate *event = list->data;
                  if (event->event.any.type == GDK_EXPOSE &&
		      event->event.any.window == window &&
		      !(event->flags & GDK_EVENT_PENDING))
		    event->event.expose.count++;

		  list = list->next;
                }
            }
        }
      else
        {
          GdkRegion *update_region = _gdk_win32_hrgn_to_region (hrgn);

	  _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
	  gdk_region_offset (update_region, xoffset, yoffset);

	  _gdk_window_process_expose (window, update_region);
	  gdk_region_destroy (update_region);
	  return_val = FALSE;
        }
      DeleteObject (hrgn);
      break;

    case WM_GETICON:
      GDK_NOTE (EVENTS, g_print ("WM_GETICON: %p %s\n",
				 msg->hwnd, 
				 (ICON_BIG == msg->wParam ? "big" : "small")));
      break;
 
    case WM_SETCURSOR:
      GDK_NOTE (EVENTS, g_print ("WM_SETCURSOR: %p %#x %#x\n",
				 msg->hwnd,
				 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      if (LOWORD (msg->lParam) != HTCLIENT)
	break;

      if (p_grab_window != NULL && p_grab_cursor != NULL)
	hcursor = p_grab_cursor;
      else if (!GDK_WINDOW_DESTROYED (window))
	hcursor = GDK_WINDOW_IMPL_WIN32 (private->impl)->hcursor;
      else
	hcursor = NULL;

      if (hcursor != NULL)
	{
	  GDK_NOTE (EVENTS, g_print ("...SetCursor(%p)\n", hcursor));
	  SetCursor (hcursor);
	  *ret_val_flagp = TRUE;
	  *ret_valp = TRUE;
	}
      break;

    case WM_SHOWWINDOW:
      GDK_NOTE (EVENTS, g_print ("WM_SHOWWINDOW: %p  %d\n",
				 msg->hwnd, msg->wParam));

      if (!(private->event_mask & GDK_STRUCTURE_MASK))
	break;

      if (msg->lParam == SW_OTHERUNZOOM ||
	  msg->lParam == SW_OTHERZOOM)
	break;

      event->any.type = (msg->wParam ? GDK_MAP : GDK_UNMAP);
      event->any.window = window;

      if (event->any.type == GDK_UNMAP
	  && p_grab_window == window)
	gdk_pointer_ungrab (msg->time);

      if (event->any.type == GDK_UNMAP
	  && k_grab_window == window)
	gdk_keyboard_ungrab (msg->time);

      return_val = !GDK_WINDOW_DESTROYED (window);
      break;

    case WM_SIZE:
      GDK_NOTE (EVENTS,
		g_print ("WM_SIZE: %p  %s %dx%d\n",
			 msg->hwnd,
			 (msg->wParam == SIZE_MAXHIDE ? "MAXHIDE" :
			  (msg->wParam == SIZE_MAXIMIZED ? "MAXIMIZED" :
			   (msg->wParam == SIZE_MAXSHOW ? "MAXSHOW" :
			    (msg->wParam == SIZE_MINIMIZED ? "MINIMIZED" :
			     (msg->wParam == SIZE_RESTORED ? "RESTORED" : "?"))))),
			 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      if (msg->wParam == SIZE_MINIMIZED)
	{
	  /* Don't generate any GDK event. This is *not* an UNMAP. */

	  if (p_grab_window == window)
	    gdk_pointer_ungrab (msg->time);

	  if (k_grab_window == window)
	    gdk_keyboard_ungrab (msg->time);

	  gdk_synthesize_window_state (window,
				       GDK_WINDOW_STATE_WITHDRAWN,
				       GDK_WINDOW_STATE_ICONIFIED);
	}
      else if ((msg->wParam == SIZE_RESTORED
		|| msg->wParam == SIZE_MAXIMIZED)
#if 1
	       && GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD
#endif
								 )
	{
	  if (!(private->event_mask & GDK_STRUCTURE_MASK))
	    break;

	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  pt.x = 0;
	  pt.y = 0;
	  ClientToScreen (msg->hwnd, &pt);
	  event->configure.x = pt.x;
	  event->configure.y = pt.y;
	  event->configure.width = LOWORD (msg->lParam);
	  event->configure.height = HIWORD (msg->lParam);
	  private->x = event->configure.x;
	  private->y = event->configure.y;
	  GDK_WINDOW_IMPL_WIN32 (private->impl)->width = event->configure.width;
	  GDK_WINDOW_IMPL_WIN32 (private->impl)->height = event->configure.height;

	  if (msg->wParam == SIZE_RESTORED)
	    gdk_synthesize_window_state (window,
					 GDK_WINDOW_STATE_ICONIFIED |
					 GDK_WINDOW_STATE_MAXIMIZED |
					 GDK_WINDOW_STATE_WITHDRAWN,
					 0);
	  else if (msg->wParam == SIZE_MAXIMIZED)
	    gdk_synthesize_window_state (window,
					 GDK_WINDOW_STATE_ICONIFIED |
					 GDK_WINDOW_STATE_WITHDRAWN,
					 GDK_WINDOW_STATE_MAXIMIZED);

	  if (private->resize_count > 1)
	    private->resize_count -= 1;
	  
	  return_val = !GDK_WINDOW_DESTROYED (window);
	  if (return_val && private->extension_events != 0)
	    _gdk_input_configure_event (&event->configure, window);
	}
      break;
#if 0
    case WM_SIZING :
      {
        LPRECT lpr = (LPRECT) msg->lParam;
        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof (NONCLIENTMETRICS);

        SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);

        g_print ("WM_SIZING borderWidth %d captionHeight %d\n",
                 ncm.iBorderWidth, ncm.iCaptionHeight);
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;

	  event->configure.x = lpr->left + ncm.iBorderWidth;
	  event->configure.y = lpr->top + ncm.iCaptionHeight;
	  event->configure.width = lpr->right - lpr->left - 2 * ncm.iBorderWidth;
	  event->configure.height = lpr->bottom - lpr->top - ncm.iCaptionHeight;
	  private->x = event->configure.x;
	  private->y = event->configure.y;
	  GDK_WINDOW_IMPL_WIN32 (private->impl)->width = event->configure.width;
	  GDK_WINDOW_IMPL_WIN32 (private->impl)->height = event->configure.height;

	  if (private->resize_count > 1)
	    private->resize_count -= 1;

	  return_val = !GDK_WINDOW_DESTROYED (window);
	  if (return_val && private->extension_events != 0)
	    _gdk_input_configure_event (&event->configure, window);
      }
      break;
#endif
    case WM_GETMINMAXINFO:
      GDK_NOTE (EVENTS, g_print ("WM_GETMINMAXINFO: %p\n", msg->hwnd));

      impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
      mmi = (MINMAXINFO*) msg->lParam;
      if (impl->hint_flags & GDK_HINT_MIN_SIZE)
	{
	  mmi->ptMinTrackSize.x = impl->hint_min_width;
	  mmi->ptMinTrackSize.y = impl->hint_min_height;
	}
      if (impl->hint_flags & GDK_HINT_MAX_SIZE)
	{
	  mmi->ptMaxTrackSize.x = impl->hint_max_width;
	  mmi->ptMaxTrackSize.y = impl->hint_max_height;

	  /* kind of WM functionality, limit maximized size to screen */
	  mmi->ptMaxPosition.x = 0;
	  mmi->ptMaxPosition.y = 0;	    
	  mmi->ptMaxSize.x = MIN (impl->hint_max_width, gdk_screen_width ());
	  mmi->ptMaxSize.y = MIN (impl->hint_max_height, gdk_screen_height ());
	}
      else if (impl->hint_flags & GDK_HINT_MIN_SIZE)
	{
	  /* need to initialize */
	  mmi->ptMaxSize.x = gdk_screen_width ();
	  mmi->ptMaxSize.y = gdk_screen_height ();
	}
      /* lovely API inconsistence: return FALSE when handled */
      if (ret_val_flagp)
	*ret_val_flagp = !(impl->hint_flags & (GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE));
      break;

    case WM_MOVE:
      GDK_NOTE (EVENTS, g_print ("WM_MOVE: %p  (%d,%d)\n",
				 msg->hwnd,
				 LOWORD (msg->lParam), HIWORD (msg->lParam)));

      if (!(private->event_mask & GDK_STRUCTURE_MASK))
	break;

      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD
	  && !IsIconic (msg->hwnd)
          && IsWindowVisible (msg->hwnd))
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.x = LOWORD (msg->lParam);
	  event->configure.y = HIWORD (msg->lParam);
	  GetClientRect (msg->hwnd, &rect);
	  event->configure.width = rect.right;
	  event->configure.height = rect.bottom;
	  private->x = event->configure.x;
	  private->y = event->configure.y;
	  GDK_WINDOW_IMPL_WIN32 (private->impl)->width = event->configure.width;
	  GDK_WINDOW_IMPL_WIN32 (private->impl)->height = event->configure.height;
	  
	  return_val = !GDK_WINDOW_DESTROYED (window);
	}
      break;

#if 0 /* Not quite right, otherwise it may be faster/better than
       * WM_(MOVE|SIZE) remove decoration (frame) sizes ?
       */
    case WM_WINDOWPOSCHANGED :

      if (!(private->event_mask & GDK_STRUCTURE_MASK))
	break;

      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD
	  && !IsIconic (msg->hwnd)
          && IsWindowVisible (msg->hwnd))
	{
	  LPWINDOWPOS lpwp = (LPWINDOWPOS) (msg->lParam);

	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.x = lpwp->x;
	  event->configure.y = lpwp->y;
	  event->configure.width = lpwp->cx;
	  event->configure.height = lpwp->cy;
	  private->x = event->configure.x;
	  private->y = event->configure.y;
	  GDK_WINDOW_IMPL_WIN32 (private->impl)->width = event->configure.width;
	  GDK_WINDOW_IMPL_WIN32 (private->impl)->height = event->configure.height;
	  
	  return_val = !GDK_WINDOW_DESTROYED (window);

          GDK_NOTE (EVENTS, g_print ("WM_WINDOWPOSCHANGED: %p  %ldx%ld@+%ld+%ld\n",
				     msg->hwnd,
				     lpwp->cx, lpwp->cy, lpwp->x, lpwp->y));

	  if (ret_val_flagp)
	    *ret_val_flagp = TRUE;
	  if (ret_valp)
	    *ret_valp = 0;
	}
      break;
#endif
    case WM_CLOSE:
      GDK_NOTE (EVENTS, g_print ("WM_CLOSE: %p\n", msg->hwnd));

      event->any.type = GDK_DELETE;
      event->any.window = window;
      
      return_val = !GDK_WINDOW_DESTROYED (window);
      break;

#if 0
    /* No, don't use delayed rendering after all. It works only if the
     * delayed SetClipboardData is called from the WindowProc, it
     * seems. (The #else part below is test code for that. It succeeds
     * in setting the clipboard data. But if I call SetClipboardData
     * in gdk_property_change (as a consequence of the
     * GDK_SELECTION_REQUEST event), it fails.  I deduce that this is
     * because delayed rendering requires that SetClipboardData is
     * called in the window procedure.)
     */
    case WM_RENDERFORMAT:
    case WM_RENDERALLFORMATS:
      flag = FALSE;
      GDK_NOTE (EVENTS, flag = TRUE);
      if (flag)
	g_print ("WM_%s: %p %#x (%s)\n",
		 (msg->message == WM_RENDERFORMAT ? "RENDERFORMAT" :
		  "RENDERALLFORMATS"),
		 msg->hwnd,
		 msg->wParam,
		 (msg->wParam == CF_TEXT ? "CF_TEXT" :
		  (msg->wParam == CF_DIB ? "CF_DIB" :
		   (msg->wParam == CF_UNICODETEXT ? "CF_UNICODETEXT" :
		    (GetClipboardFormatName (msg->wParam, buf, sizeof (buf)), buf)))));

#if 0
      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = gdk_clipboard_atom;
      if (msg->wParam == CF_TEXT)
	event->selection.target = GDK_TARGET_STRING;
      else
	{
	  GetClipboardFormatName (msg->wParam, buf, sizeof (buf));
	  event->selection.target = gdk_atom_intern (buf, FALSE);
	}
      event->selection.property = _gdk_selection_property;
      event->selection.requestor = (guint32) msg->hwnd;
      event->selection.time = _gdk_win32_get_next_tick (msg->time);
      return_val = !GDK_WINDOW_DESTROYED (window);
#else
      /* Test code, to see if SetClipboardData works when called from
       * the window procedure.
       */
      {
	HGLOBAL hdata = GlobalAlloc (GMEM_MOVEABLE|GMEM_DDESHARE, 10);
	char *ptr = GlobalLock (hdata);
	strcpy (ptr, "Huhhaa");
	GlobalUnlock (hdata);
	if (!SetClipboardData (CF_TEXT, hdata))
	  WIN32_API_FAILED ("SetClipboardData");
      }
      *ret_valp = 0;
      *ret_val_flagp = TRUE;
      return_val = FALSE;
#endif
      break;
#endif /* No delayed rendering */

    case WM_DESTROY:
      GDK_NOTE (EVENTS, g_print ("WM_DESTROY: %p\n", msg->hwnd));

      event->any.type = GDK_DESTROY;
      event->any.window = window;
      if (window != NULL && window == current_window)
	{
	  gdk_drawable_unref (current_window);
	  current_window = NULL;
	}

      if (p_grab_window == window)
	gdk_pointer_ungrab (msg->time);

      if (k_grab_window == window)
	gdk_keyboard_ungrab (msg->time);

      return_val = window != NULL && !GDK_WINDOW_DESTROYED (window);

      if ((window != NULL) && (_gdk_root_window != msg->hwnd))
	gdk_window_destroy_notify (window);

      break;

#ifdef HAVE_WINTAB
      /* Handle WINTAB events here, as we know that gdkinput.c will
       * use the fixed WT_DEFBASE as lcMsgBase, and we thus can use the
       * constants as case labels.
       */
    case WT_PACKET:
      GDK_NOTE (EVENTS, g_print ("WT_PACKET: %p %d %#lx\n",
				 msg->hwnd, msg->wParam, msg->lParam));
      goto wintab;
      
    case WT_CSRCHANGE:
      GDK_NOTE (EVENTS, g_print ("WT_CSRCHANGE: %p %d %#lx\n",
				 msg->hwnd, msg->wParam, msg->lParam));
      goto wintab;
      
    case WT_PROXIMITY:
      GDK_NOTE (EVENTS, g_print ("WT_PROXIMITY: %p %#x %d %d\n",
				 msg->hwnd, msg->wParam,
				 LOWORD (msg->lParam),
				 HIWORD (msg->lParam)));
      /* Fall through */
    wintab:
      event->any.window = window;
      return_val = _gdk_input_other_event (event, msg, window);
      break;
#endif

    default:
      GDK_NOTE (EVENTS, g_print ("%s: %p %#x %#lx\n",
				 _gdk_win32_message_to_string (msg->message),
				 msg->hwnd, msg->wParam, msg->lParam));
    }

done:

  if (return_val)
    {
      if (event->any.window)
	gdk_drawable_ref (event->any.window);
      if (((event->any.type == GDK_ENTER_NOTIFY) ||
	   (event->any.type == GDK_LEAVE_NOTIFY)) &&
	  (event->crossing.subwindow != NULL))
	gdk_drawable_ref (event->crossing.subwindow);

      GDK_NOTE (EVENTS, print_event (event));
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
      event->any.type = GDK_NOTHING;
    }

  if (window)
    gdk_drawable_unref (window);
  
  return return_val;
}

void
_gdk_events_queue (GdkDisplay *display)
{
  MSG msg;
  GdkEvent *event;
  GList *node;

  while (!_gdk_event_queue_find_first (display)
	 && PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
#ifndef HAVE_DIMM_H
      TranslateMessage (&msg);
#else
      if (active_imm_msgpump_owner == NULL
	  || (active_imm_msgpump_owner->lpVtbl->OnTranslateMessage) (active_imm_msgpump_owner, &msg) != S_OK)
	TranslateMessage (&msg);
#endif

      DispatchMessage (&msg);
    }
}

static gboolean
gdk_event_prepare (GSource *source,
		   gint    *timeout)
{
  MSG msg;
  gboolean retval;
  GdkDisplay *display = gdk_display_get_default ();
  
  GDK_THREADS_ENTER ();

  *timeout = -1;

  retval = (_gdk_event_queue_find_first (display) != NULL)
	    || PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
gdk_event_check (GSource *source)
{
  MSG msg;
  gboolean retval;
  GdkDisplay *display = gdk_display_get_default ();
  
  GDK_THREADS_ENTER ();

  if (event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (display) != NULL)
	      || PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE);
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean
gdk_event_dispatch (GSource     *source,
		    GSourceFunc  callback,
		    gpointer     user_data)
{
  GdkEvent *event;
  GdkDisplay *display = gdk_display_get_default ();
 
  GDK_THREADS_ENTER ();

  _gdk_events_queue (display);
  event = _gdk_event_unqueue (display);

  if (event)
    {
      if (_gdk_event_func)
	(*_gdk_event_func) (event, _gdk_event_data);
      
      gdk_event_free (event);
    }
  
  GDK_THREADS_LEAVE ();

  return TRUE;
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message_for_display (GdkDisplay     *display,
                                           GdkEvent       *event, 
                                           GdkNativeWindow winid)
{
  /* XXX */
  return FALSE;
}

void
gdk_screen_broadcast_client_message (GdkScreen *screen, 
				     GdkEvent  *event)
{
  /* XXX */
}

void
gdk_flush (void)
{
#if 0
  MSG msg;

  /* Process all messages currently available */
  while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
    }
#endif

  GdiFlush ();
}

void
gdk_display_sync (GdkDisplay * display)
{
  MSG msg;

  g_return_if_fail (display == gdk_display_get_default ());

  /* Process all messages currently available */
  while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
    }
}
