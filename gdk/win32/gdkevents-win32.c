/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999 Tor Lillqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "config.h"

/* Cannot use TrackMouseEvent, as the stupid WM_MOUSELEAVE message
 * doesn't tell us where the mouse has gone. Thus we cannot use it to
 * generate a correct GdkNotifyType. Pity, as using TrackMouseEvent
 * otherwise would make it possible to reliably generate
 * GDK_LEAVE_NOTIFY events, which would help get rid of those pesky
 * tooltips sometimes popping up in the wrong place.
 */
/* define USE_TRACKMOUSEEVENT */

#include <stdio.h>

#include "gdk.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"

#include "gdkkeysyms.h"

#include "gdkinputprivate.h"

#include <objbase.h>
#include <imm.h>

#ifdef HAVE_DIMM_H
#include <dimm.h>
#else
#include "surrogate-dimm.h"
#endif

#ifdef HAVE_WINTAB
#include <wintab.h>
#endif

#define PING() printf("%s: %d\n",__FILE__,__LINE__),fflush(stdout)

#define WINDOW_PRIVATE(wp) GDK_WINDOW_WIN32DATA (wp)

typedef struct _GdkIOClosure GdkIOClosure;
typedef struct _GdkEventPrivate GdkEventPrivate;

typedef enum
{
  /* Following flag is set for events on the event queue during
   * translation and cleared afterwards.
   */
  GDK_EVENT_PENDING = 1 << 0
} GdkEventFlags;

struct _GdkIOClosure
{
  GdkInputFunction function;
  GdkInputCondition condition;
  GdkDestroyNotify notify;
  gpointer data;
};

struct _GdkEventPrivate
{
  GdkEvent event;
  guint    flags;
};

/* 
 * Private function declarations
 */

static GdkFilterReturn
		 gdk_event_apply_filters(MSG      *xevent,
					 GdkEvent *event,
					 GList    *filters);
static gboolean  gdk_event_translate	(GdkEvent *event, 
					 MSG      *xevent,
					 gboolean *ret_val_flagp,
					 gint     *ret_valp);
static gboolean  gdk_event_prepare      (gpointer  source_data, 
				 	 GTimeVal *current_time,
					 gint     *timeout,
					 gpointer  user_data);
static gboolean  gdk_event_check        (gpointer  source_data,
				 	 GTimeVal *current_time,
					 gpointer  user_data);
static gboolean  gdk_event_dispatch     (gpointer  source_data,
					 GTimeVal *current_time,
					 gpointer  user_data);

static void	 gdk_synthesize_click	(GdkEvent     *event, 
					 gint	       nclicks);

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

static GList *client_filters;	            /* Filters for client messages */

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  (GDestroyNotify)g_free
};

GPollFD event_poll_fd;

static GdkWindow *curWnd = NULL;
static HWND active = NULL;
static gint curX, curY;
static gdouble curXroot, curYroot;
static UINT gdk_ping_msg;
static gboolean ignore_WM_CHAR = FALSE;
static gboolean is_AltGr_key = FALSE;

static IActiveIMMApp *paimmapp = NULL;
static IActiveIMMMessagePumpOwner *paimmmpo = NULL;

typedef BOOL (WINAPI *PFN_TrackMouseEvent) (LPTRACKMOUSEEVENT);
static PFN_TrackMouseEvent p_TrackMouseEvent = NULL;

static gboolean use_IME_COMPOSITION = FALSE;

LRESULT CALLBACK 
gdk_WindowProc (HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam)
{
  GdkEvent event;
  GdkEvent *eventp;
  MSG msg;
  DWORD pos;
  LRESULT lres;
  gint ret_val;
  gboolean ret_val_flag;

  GDK_NOTE (EVENTS, g_print ("gdk_WindowProc: %#x %#.03x\n", hWnd, message));

  msg.hwnd = hWnd;
  msg.message = message;
  msg.wParam = wParam;
  msg.lParam = lParam;
  msg.time = GetTickCount ();
  pos = GetMessagePos ();
  msg.pt.x = LOWORD (pos);
  msg.pt.y = HIWORD (pos);

  ((GdkEventPrivate *)&event)->flags |= GDK_EVENT_PENDING;
  if (gdk_event_translate (&event, &msg, &ret_val_flag, &ret_val))
    {
      ((GdkEventPrivate *)&event)->flags &= ~GDK_EVENT_PENDING;
#if 1
      if (event.any.type == GDK_CONFIGURE)
	{
	  /* Compress configure events */
	  GList *list = gdk_queued_events;

	  while (list != NULL
		 && (((GdkEvent *)list->data)->any.type != GDK_CONFIGURE
		     || ((GdkEvent *)list->data)->any.window != event.any.window))
	    list = list->next;
	  if (list != NULL)
	    {
	      GDK_NOTE (EVENTS, g_print ("... compressing an CONFIGURE event\n"));

	      *((GdkEvent *)list->data) = event;
	      gdk_drawable_unref (event.any.window);
	      /* Wake up WaitMessage */
	      PostMessage (NULL, gdk_ping_msg, 0, 0);
	      return FALSE;
	    }
	}
      else if (event.any.type == GDK_EXPOSE)
	{
	  /* Compress expose events */
	  GList *list = gdk_queued_events;

	  while (list != NULL
		 && (((GdkEvent *)list->data)->any.type != GDK_EXPOSE
		     || ((GdkEvent *)list->data)->any.window != event.any.window))
	    list = list->next;
	  if (list != NULL)
	    {
	      GdkRectangle u;

	      GDK_NOTE (EVENTS, g_print ("... compressing an EXPOSE event\n"));
	      gdk_rectangle_union (&event.expose.area,
				   &((GdkEvent *)list->data)->expose.area,
				   &u);
	      ((GdkEvent *)list->data)->expose.area = u;
	      gdk_drawable_unref (event.any.window);
#if 0
	      /* Wake up WaitMessage */
	      PostMessage (NULL, gdk_ping_msg, 0, 0);
#endif
	      return FALSE;
	    }
	}
#endif
      eventp = gdk_event_new ();
      *eventp = event;

      /* Philippe Colantoni <colanton@aris.ss.uci.edu> suggests this
       * in order to handle events while opaque resizing neatly.  I
       * don't want it as default. Set the
       * GDK_EVENT_FUNC_FROM_WINDOW_PROC env var to get this
       * behaviour.
       */
      if (gdk_event_func_from_window_proc && gdk_event_func)
	{
	  GDK_THREADS_ENTER ();
	  
	  (*gdk_event_func) (eventp, gdk_event_data);
	  gdk_event_free (eventp);
	  
	  GDK_THREADS_LEAVE ();
	}
      else
	{
	  gdk_event_queue_append (eventp);
#if 1
	  /* Wake up WaitMessage */
	  PostMessage (NULL, gdk_ping_msg, 0, 0);
#endif
	}
      
      if (ret_val_flag)
	return ret_val;
      else
	return FALSE;
    }

  if (ret_val_flag)
    return ret_val;
  else
    {
      if (paimmapp == NULL
	  || (*paimmapp->lpVtbl->OnDefWindowProc) (paimmapp, hWnd, message, wParam, lParam, &lres) == S_FALSE)
	return DefWindowProc (hWnd, message, wParam, lParam);
      else
	return lres;
    }
}

void 
gdk_events_init (void)
{
  HRESULT hres;
  HMODULE user32, imm32;
  HINSTANCE commctrl32;

  if (g_pipe_readable_msg == 0)
    g_pipe_readable_msg = RegisterWindowMessage ("g-pipe-readable");
  GDK_NOTE (EVENTS, g_print ("g-pipe-readable = %#.03x\n",
			     g_pipe_readable_msg));

  gdk_ping_msg = RegisterWindowMessage ("gdk-ping");
  GDK_NOTE (EVENTS, g_print ("gdk-ping = %#.03x\n",
			     gdk_ping_msg));

  g_source_add (GDK_PRIORITY_EVENTS, TRUE, &event_funcs, NULL, NULL, NULL);

  event_poll_fd.fd = G_WIN32_MSG_HANDLE;
  event_poll_fd.events = G_IO_IN;
  
  g_main_add_poll (&event_poll_fd, GDK_PRIORITY_EVENTS);

  hres = CoCreateInstance (&CLSID_CActiveIMM,
			   NULL,
			   CLSCTX_ALL,
			   &IID_IActiveIMMApp,
			   (LPVOID *) &paimmapp);
  
  if (hres == S_OK)
    {
      GDK_NOTE (EVENTS, g_print ("IActiveIMMApp created %#x\n",
				 paimmapp));
      (*paimmapp->lpVtbl->Activate) (paimmapp, TRUE);
      
      hres = (*paimmapp->lpVtbl->QueryInterface) (paimmapp, &IID_IActiveIMMMessagePumpOwner, &paimmmpo);
      GDK_NOTE (EVENTS, g_print ("IActiveIMMMessagePumpOwner created %#x\n",
				 paimmmpo));
      (paimmmpo->lpVtbl->Start) (paimmmpo);
    }

#ifdef USE_TRACKMOUSEEVENT
  user32 = GetModuleHandle ("user32.dll");
  if ((p_TrackMouseEvent = GetProcAddress (user32, "TrackMouseEvent")) == NULL)
    {
      if ((commctrl32 = LoadLibrary ("commctrl32.dll")) != NULL)
	p_TrackMouseEvent = (PFN_TrackMouseEvent)
	  GetProcAddress (commctrl32, "_TrackMouseEvent");
    }
  if (p_TrackMouseEvent != NULL)
    GDK_NOTE (EVENTS, g_print ("Using TrackMouseEvent to detect leave events\n"));
#endif
  if (IS_WIN_NT (windows_version) && (windows_version & 0xFF) == 5)
    {
      /* On Win2k (Beta 3, at least) WM_IME_CHAR doesn't seem to work
       * correctly for non-Unicode applications. Handle
       * WM_IME_COMPOSITION with GCS_RESULTSTR instead, fetch the
       * Unicode char from the IME with ImmGetCompositionStringW().
       */
      use_IME_COMPOSITION = TRUE;
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

  return (gdk_event_queue_find_first() || PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE));
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
  MSG xevent;
  GdkEvent *event;

  g_return_val_if_fail (window != NULL, NULL);
  
  GDK_NOTE (EVENTS, g_print ("gdk_event_get_graphics_expose\n"));

#if 0 /* ??? */
  /* Some nasty bugs here, just return NULL for now. */
  return NULL;
#else
  if (PeekMessage (&xevent, GDK_DRAWABLE_XID (window), WM_PAINT, WM_PAINT, PM_REMOVE))
    {
      event = gdk_event_new ();
      
      if (gdk_event_translate (event, &xevent, NULL, NULL))
	return event;
      else
	gdk_event_free (event);
    }
  
  return NULL;	
#endif
}

static char *
event_mask_string (GdkEventMask mask)
{
  static char bfr[500];
  char *p = bfr;

  *p = '\0';
#define BIT(x) \
  if (mask & GDK_##x##_MASK) \
    p += sprintf (p, "%s" #x, (p > bfr ? " " : ""))
  BIT(EXPOSURE);
  BIT(POINTER_MOTION);
  BIT(POINTER_MOTION_HINT);
  BIT(BUTTON_MOTION);
  BIT(BUTTON1_MOTION);
  BIT(BUTTON2_MOTION);
  BIT(BUTTON3_MOTION);
  BIT(BUTTON_PRESS);
  BIT(BUTTON_RELEASE);
  BIT(KEY_PRESS);
  BIT(KEY_RELEASE);
  BIT(ENTER_NOTIFY);
  BIT(LEAVE_NOTIFY);
  BIT(FOCUS_CHANGE);
  BIT(STRUCTURE);
  BIT(PROPERTY_CHANGE);
  BIT(VISIBILITY_NOTIFY);
  BIT(PROXIMITY_IN);
  BIT(PROXIMITY_OUT);
  BIT(SUBSTRUCTURE);
  BIT(SCROLL);
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

gint
gdk_pointer_grab (GdkWindow *	  window,
		  gboolean	  owner_events,
		  GdkEventMask	  event_mask,
		  GdkWindow *	  confine_to,
		  GdkCursor *	  cursor,
		  guint32	  time)
{
  HWND xwindow;
  HWND xconfine_to;
  HCURSOR xcursor;
  GdkCursorPrivate *cursor_private;
  gint return_val;

  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);
  
  cursor_private = (GdkCursorPrivate*) cursor;
  
  xwindow = GDK_DRAWABLE_XID (window);
  
  if (!confine_to || GDK_DRAWABLE_DESTROYED (confine_to))
    xconfine_to = NULL;
  else
    xconfine_to = GDK_DRAWABLE_XID (confine_to);
  
  if (!cursor)
    xcursor = NULL;
  else
    xcursor = cursor_private->xcursor;
  
  if (gdk_input_vtable.grab_pointer)
    return_val = gdk_input_vtable.grab_pointer (window,
						owner_events,
						event_mask,
						confine_to,
						time);
  else
    return_val = Success;
  
  if (return_val == Success)
    {
      if (!GDK_DRAWABLE_DESTROYED (window))
      {
	GDK_NOTE (EVENTS, g_print ("gdk_pointer_grab: %#x %s %#x %s\n",
				   xwindow,
				   (owner_events ? "TRUE" : "FALSE"),
				   xcursor,
				   event_mask_string (event_mask)));
	p_grab_mask = event_mask;
	p_grab_owner_events = (owner_events != 0);
	p_grab_automatic = FALSE;

#if 0 /* Menus don't work if we use mouse capture. Pity, because many other
       * things work better with mouse capture.
       */
	SetCapture (xwindow);
#endif
	return_val = GrabSuccess;
      }
      else
	return_val = AlreadyGrabbed;
    }
  
  if (return_val == GrabSuccess)
    {
      p_grab_window = window;
      p_grab_cursor = xcursor;
    }
  
  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_ungrab
 *
 *   Releases any pointer grab
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_pointer_ungrab (guint32 time)
{
  if (gdk_input_vtable.ungrab_pointer)
    gdk_input_vtable.ungrab_pointer (time);
#if 0
  if (GetCapture () != NULL)
    ReleaseCapture ();
#endif
  GDK_NOTE (EVENTS, g_print ("gdk_pointer_ungrab\n"));

  p_grab_window = NULL;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_is_grabbed
 *
 *   Tell wether there is an active x pointer grab in effect
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gboolean
gdk_pointer_is_grabbed (void)
{
  return p_grab_window != NULL;
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

gint
gdk_keyboard_grab (GdkWindow *	   window,
		   gboolean	   owner_events,
		   guint32	   time)
{
  gint return_val;
  
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  
  GDK_NOTE (EVENTS, g_print ("gdk_keyboard_grab %#x\n",
			     GDK_DRAWABLE_XID (window)));

  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      k_grab_owner_events = owner_events != 0;
      return_val = GrabSuccess;
    }
  else
    return_val = AlreadyGrabbed;

  if (return_val == GrabSuccess)
    k_grab_window = window;
  
  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_ungrab
 *
 *   Releases any keyboard grab
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_keyboard_ungrab (guint32 time)
{
  GDK_NOTE (EVENTS, g_print ("gdk_keyboard_ungrab\n"));

  k_grab_window = NULL;
}

static void
gdk_io_destroy (gpointer data)
{
  GdkIOClosure *closure = data;

  if (closure->notify)
    closure->notify (closure->data);

  g_free (closure);
}

static gboolean  
gdk_io_invoke (GIOChannel   *source,
	       GIOCondition  condition,
	       gpointer      data)
{
  GdkIOClosure *closure = data;
  GdkInputCondition gdk_cond = 0;

  if (condition & (G_IO_IN | G_IO_PRI))
    gdk_cond |= GDK_INPUT_READ;
  if (condition & G_IO_OUT)
    gdk_cond |= GDK_INPUT_WRITE;
  if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
    gdk_cond |= GDK_INPUT_EXCEPTION;

  if (closure->condition & gdk_cond)
    closure->function (closure->data, g_io_channel_unix_get_fd (source), gdk_cond);

  return TRUE;
}

static GdkFilterReturn
gdk_event_apply_filters (MSG      *xevent,
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
      
      result = (*filter->function) (xevent, event, filter->data);
      if (result !=  GDK_FILTER_CONTINUE)
	return result;
      
      tmp_list = tmp_list->next;
    }
  
  return GDK_FILTER_CONTINUE;
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

/* Thanks to Markus G. Kuhn <mkuhn@acm.org> for the ksysym<->Unicode
 * mapping functions, from the xterm sources.
 */

static void
build_key_event_state (GdkEvent *event)
{
  event->key.state = 0;
  if (GetKeyState (VK_SHIFT) < 0)
    event->key.state |= GDK_SHIFT_MASK;
  if (GetKeyState (VK_CAPITAL) & 0x1)
    event->key.state |= GDK_LOCK_MASK;
  if (!is_AltGr_key)
    {
      if (GetKeyState (VK_CONTROL) < 0)
	{
	  event->key.state |= GDK_CONTROL_MASK;
	  if (event->key.keyval < ' ')
	    event->key.keyval += '@';
	}
      else if (event->key.keyval < ' ')
	{
	  event->key.state |= GDK_CONTROL_MASK;
	  event->key.keyval += '@';
	}
      if (GetKeyState (VK_MENU) < 0)
	event->key.state |= GDK_MOD1_MASK;
    }
}

static gint
build_pointer_event_state (MSG *xevent)
{
  gint state;
  
  state = 0;
  if (xevent->wParam & MK_CONTROL)
    state |= GDK_CONTROL_MASK;
  if (xevent->wParam & MK_LBUTTON)
    state |= GDK_BUTTON1_MASK;
  if (xevent->wParam & MK_MBUTTON)
    state |= GDK_BUTTON2_MASK;
  if (xevent->wParam & MK_RBUTTON)
    state |= GDK_BUTTON3_MASK;
  if (xevent->wParam & MK_SHIFT)
    state |= GDK_SHIFT_MASK;
  if (GetKeyState (VK_MENU) < 0)
    state |= GDK_MOD1_MASK;
  if (GetKeyState (VK_CAPITAL) & 0x1)
    state |= GDK_LOCK_MASK;

  return state;
}


static void
build_keypress_event (GdkWindowWin32Data *windata,
		      GdkEvent           *event,
		      MSG                *xevent)
{
  HIMC hIMC;
  gint i, bytesleft, bytecount, ucount, ucleft, len;
  guchar buf[100], *bp;
  wchar_t wbuf[100], *wcp;

  event->key.type = GDK_KEY_PRESS;
  event->key.time = xevent->time;
  
  if (xevent->message == WM_IME_COMPOSITION)
    {
      hIMC = ImmGetContext (xevent->hwnd);

      bytecount = ImmGetCompositionStringW (hIMC, GCS_RESULTSTR,
					    wbuf, sizeof (wbuf));
      ucount = bytecount / 2;
    }
  else
    {
      if (xevent->message == WM_CHAR)
	{
	  bytecount = MIN ((xevent->lParam & 0xFFFF), sizeof (buf));
	  for (i = 0; i < bytecount; i++)
	    buf[i] = xevent->wParam;
	}
      else /* WM_IME_CHAR */
	{
	  event->key.keyval = GDK_VoidSymbol;
	  if (xevent->wParam & 0xFF00)
	    {
	      /* Contrary to some versions of the documentation,
	       * the lead byte is the most significant byte.
	       */
	      buf[0] = ((xevent->wParam >> 8) & 0xFF);
	      buf[1] = (xevent->wParam & 0xFF);
	      bytecount = 2;
	    }
	  else
	    {
	      buf[0] = (xevent->wParam & 0xFF);
	      bytecount = 1;
	    }
	}

      /* Convert from the window's current code page
       * to Unicode. Then convert to UTF-8.
       * We don't handle the surrogate stuff. Should we?
       */
      ucount = MultiByteToWideChar (windata->charset_info.ciACP,
				    0, buf, bytecount,
				    wbuf, sizeof (wbuf) / sizeof (wbuf[0]));
      
    }
  if (ucount == 0)
    event->key.keyval = GDK_VoidSymbol;
  else if (xevent->message == WM_CHAR)
    if (xevent->wParam < ' ')
      event->key.keyval = xevent->wParam + '@';
    else
      event->key.keyval = gdk_unicode_to_keyval (wbuf[0]);
  else
    event->key.keyval = GDK_VoidSymbol;

  build_key_event_state (event);

  ucleft = ucount;
  len = 0;
  wcp = wbuf;
  while (ucleft-- > 0)
    {
      wchar_t c = *wcp++;

      if (c < 0x80)
	len += 1;
      else if (c < 0x800)
	len += 2;
      else
	len += 3;
    }

  event->key.string = g_malloc (len + 1);
  event->key.length = len;
  
  ucleft = ucount;
  wcp = wbuf;
  bp = event->key.string;
  while (ucleft-- > 0)
    {
      int first;
      int i;
      wchar_t c = *wcp++;

      if (c < 0x80)
	{
	  first = 0;
	  len = 1;
	}
      else if (c < 0x800)
	{
	  first = 0xc0;
	  len = 2;
	}
      else
	{
	  first = 0xe0;
	  len = 3;
	}

#if 1      
      /* Woo-hoo! */
      switch (len)
	{
	case 3: bp[2] = (c & 0x3f) | 0x80; c >>= 6; /* Fall through */
	case 2: bp[1] = (c & 0x3f) | 0x80; c >>= 6; /* Fall through */
	case 1: bp[0] = c | first;
	}
#else
      for (i = len - 1; i > 0; --i)
	{
	  bp[i] = (c & 0x3f) | 0x80;
	  c >>= 6;
	}
      bp[0] = c | first;
#endif

      bp += len;
    }
  *bp = 0;
}

static void
build_keyrelease_event (GdkWindowWin32Data *windata,
			GdkEvent           *event,
			MSG                *xevent)
{
  guchar buf;
  wchar_t wbuf;

  event->key.type = GDK_KEY_RELEASE;
  event->key.time = xevent->time;

  if (xevent->message == WM_CHAR)
    if (xevent->wParam < ' ')
      event->key.keyval = xevent->wParam + '@';
    else
      {
	buf = xevent->wParam;
	MultiByteToWideChar (windata->charset_info.ciACP,
			     0, &buf, 1, &wbuf, 1);

	event->key.keyval = gdk_unicode_to_keyval (wbuf);
      }
  else
    event->key.keyval = GDK_VoidSymbol;
  build_key_event_state (event);
  event->key.string = NULL;
  event->key.length = 0;
}

static void
print_event_state (gint state)
{
  if (state & GDK_SHIFT_MASK)
    g_print ("SHIFT ");
  if (state & GDK_LOCK_MASK)
    g_print ("LOCK ");
  if (state & GDK_CONTROL_MASK)
    g_print ("CONTROL ");
  if (state & GDK_MOD1_MASK)
    g_print ("MOD1 ");
  if (state & GDK_BUTTON1_MASK)
    g_print ("BUTTON1 ");
  if (state & GDK_BUTTON2_MASK)
    g_print ("BUTTON2 ");
  if (state & GDK_BUTTON3_MASK)
    g_print ("BUTTON3 ");
}

static void
print_event (GdkEvent *event)
{
  gchar *escaped, *kvname;

  switch (event->any.type)
    {
    case GDK_NOTHING: g_print ("GDK_NOTHING "); break;
    case GDK_DELETE: g_print ("GDK_DELETE "); break;
    case GDK_DESTROY: g_print ("GDK_DESTROY "); break;
    case GDK_EXPOSE: g_print ("GDK_EXPOSE "); break;
    case GDK_MOTION_NOTIFY: g_print ("GDK_MOTION_NOTIFY "); break;
    case GDK_BUTTON_PRESS: g_print ("GDK_BUTTON_PRESS "); break;
    case GDK_2BUTTON_PRESS: g_print ("GDK_2BUTTON_PRESS "); break;
    case GDK_3BUTTON_PRESS: g_print ("GDK_3BUTTON_PRESS "); break;
    case GDK_BUTTON_RELEASE: g_print ("GDK_BUTTON_RELEASE "); break;
    case GDK_KEY_PRESS: g_print ("GDK_KEY_PRESS "); break;
    case GDK_KEY_RELEASE: g_print ("GDK_KEY_RELEASE "); break;
    case GDK_ENTER_NOTIFY: g_print ("GDK_ENTER_NOTIFY "); break;
    case GDK_LEAVE_NOTIFY: g_print ("GDK_LEAVE_NOTIFY "); break;
    case GDK_FOCUS_CHANGE: g_print ("GDK_FOCUS_CHANGE "); break;
    case GDK_CONFIGURE: g_print ("GDK_CONFIGURE "); break;
    case GDK_MAP: g_print ("GDK_MAP "); break;
    case GDK_UNMAP: g_print ("GDK_UNMAP "); break;
    case GDK_PROPERTY_NOTIFY: g_print ("GDK_PROPERTY_NOTIFY "); break;
    case GDK_SELECTION_CLEAR: g_print ("GDK_SELECTION_CLEAR "); break;
    case GDK_SELECTION_REQUEST: g_print ("GDK_SELECTION_REQUEST "); break;
    case GDK_SELECTION_NOTIFY: g_print ("GDK_SELECTION_NOTIFY "); break;
    case GDK_PROXIMITY_IN: g_print ("GDK_PROXIMITY_IN "); break;
    case GDK_PROXIMITY_OUT: g_print ("GDK_PROXIMITY_OUT "); break;
    case GDK_DRAG_ENTER: g_print ("GDK_DRAG_ENTER "); break;
    case GDK_DRAG_LEAVE: g_print ("GDK_DRAG_LEAVE "); break;
    case GDK_DRAG_MOTION: g_print ("GDK_DRAG_MOTION "); break;
    case GDK_DRAG_STATUS: g_print ("GDK_DRAG_STATUS "); break;
    case GDK_DROP_START: g_print ("GDK_DROP_START "); break;
    case GDK_DROP_FINISHED: g_print ("GDK_DROP_FINISHED "); break;
    case GDK_CLIENT_EVENT: g_print ("GDK_CLIENT_EVENT "); break;
    case GDK_VISIBILITY_NOTIFY: g_print ("GDK_VISIBILITY_NOTIFY "); break;
    case GDK_NO_EXPOSE: g_print ("GDK_NO_EXPOSE "); break;
    case GDK_SCROLL: g_print ("GDK_SCROLL "); break;
    }
  g_print ("%#x ", GDK_DRAWABLE_XID (event->any.window));

  switch (event->any.type)
    {
    case GDK_EXPOSE:
      g_print ("%dx%d@+%d+%d %d",
	       event->expose.area.width,
	       event->expose.area.height,
	       event->expose.area.x,
	       event->expose.area.y,
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
      g_print ("%s %d:\"%s\" ",
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
    case GDK_SCROLL:
      g_print ("%s ",
	       (event->scroll.direction == GDK_SCROLL_UP ? "UP" :
		(event->scroll.direction == GDK_SCROLL_DOWN ? "DOWN" :
		 (event->scroll.direction == GDK_SCROLL_LEFT ? "LEFT" :
		  (event->scroll.direction == GDK_SCROLL_RIGHT ? "RIGHT" :
		   "???")))));
      print_event_state (event->scroll.state);
      break;
    }  
  g_print ("\n");
}

static void
synthesize_crossing_events (GdkWindow *window,
			    MSG       *xevent)
{
  GdkEvent *event;
  
  /* If we are not using TrackMouseEvent, generate a leave notify
   * event if necessary
   */
  if (p_TrackMouseEvent == NULL
      && curWnd
      && (GDK_WINDOW_WIN32DATA (curWnd)->event_mask & GDK_LEAVE_NOTIFY_MASK))
    {
      GDK_NOTE (EVENTS, g_print ("synthesizing LEAVE_NOTIFY event\n"));

      event = gdk_event_new ();
      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = curWnd;
      gdk_drawable_ref (event->crossing.window);
      event->crossing.subwindow = NULL;
      event->crossing.time = xevent->time;
      event->crossing.x = curX;
      event->crossing.y = curY;
      event->crossing.x_root = curXroot;
      event->crossing.y_root = curYroot;
      event->crossing.mode = GDK_CROSSING_NORMAL;
      if (IsChild (GDK_DRAWABLE_XID (curWnd), GDK_DRAWABLE_XID (window)))
	event->crossing.detail = GDK_NOTIFY_INFERIOR;
      else if (IsChild (GDK_DRAWABLE_XID (window), GDK_DRAWABLE_XID (curWnd)))
	event->crossing.detail = GDK_NOTIFY_ANCESTOR;
      else
	event->crossing.detail = GDK_NOTIFY_NONLINEAR;

      event->crossing.focus = TRUE; /* ??? */
      event->crossing.state = 0; /* ??? */

      gdk_event_queue_append (event);
      GDK_NOTE (EVENTS, print_event (event));
    }

  if (GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_ENTER_NOTIFY_MASK)
    {
      GDK_NOTE (EVENTS, g_print ("synthesizing ENTER_NOTIFY event\n"));
      
      event = gdk_event_new ();
      event->crossing.type = GDK_ENTER_NOTIFY;
      event->crossing.window = window;
      gdk_drawable_ref (event->crossing.window);
      event->crossing.subwindow = NULL;
      event->crossing.time = xevent->time;
      event->crossing.x = LOWORD (xevent->lParam);
      event->crossing.y = HIWORD (xevent->lParam);
      event->crossing.x_root = (gfloat) xevent->pt.x;
      event->crossing.y_root = (gfloat) xevent->pt.y;
      event->crossing.mode = GDK_CROSSING_NORMAL;
      if (curWnd
	  && IsChild (GDK_DRAWABLE_XID (curWnd), GDK_DRAWABLE_XID (window)))
	event->crossing.detail = GDK_NOTIFY_ANCESTOR;
      else if (curWnd
	       && IsChild (GDK_DRAWABLE_XID (window), GDK_DRAWABLE_XID (curWnd)))
	event->crossing.detail = GDK_NOTIFY_INFERIOR;
      else
	event->crossing.detail = GDK_NOTIFY_NONLINEAR;
      
      event->crossing.focus = TRUE; /* ??? */
      event->crossing.state = 0; /* ??? */
      
      gdk_event_queue_append (event);

      GDK_NOTE (EVENTS, print_event (event));

      if (((GdkWindowPrivate *) window)->extension_events != 0
	  && gdk_input_vtable.enter_event)
	gdk_input_vtable.enter_event (&event->crossing, window);

    }
  
  if (curWnd)
    gdk_drawable_unref (curWnd);
  curWnd = window;
  gdk_drawable_ref (curWnd);
#ifdef USE_TRACKMOUSEEVENT
  if (p_TrackMouseEvent != NULL)
    {
      TRACKMOUSEEVENT tme;

      tme.cbSize = sizeof (TRACKMOUSEEVENT);
      tme.dwFlags = TME_LEAVE;
      tme.hwndTrack = GDK_DRAWABLE_XID (curWnd);
      tme.dwHoverTime = HOVER_DEFAULT;
      
      (*p_TrackMouseEvent) (&tme);
    }
#endif
}

static void
translate_mouse_coords (GdkWindow *window1,
			GdkWindow *window2,
			MSG       *xevent)
{
  POINT pt;

  pt.x = LOWORD (xevent->lParam);
  pt.y = HIWORD (xevent->lParam);
  ClientToScreen (GDK_DRAWABLE_XID (window1), &pt);
  ScreenToClient (GDK_DRAWABLE_XID (window2), &pt);
  xevent->lParam = MAKELPARAM (pt.x, pt.y);
  GDK_NOTE (EVENTS, g_print ("...new coords are (%d,%d)\n", pt.x, pt.y));
}

static gboolean
propagate (GdkWindow  **window,
	   MSG         *xevent,
	   GdkWindow   *grab_window,
	   gboolean     grab_owner_events,
	   gint	        grab_mask,
	   gboolean   (*doesnt_want_it) (gint mask,
					 MSG *xevent))
{
  if (grab_window != NULL && !grab_owner_events)
    {
      /* Event source is grabbed with owner_events FALSE */
      GDK_NOTE (EVENTS, g_print ("...grabbed, owner_events FALSE, "));
      if ((*doesnt_want_it) (grab_mask, xevent))
	{
	  GDK_NOTE (EVENTS, g_print ("...grabber doesn't want it\n"));
	  return FALSE;
	}
      else
	{
	  GDK_NOTE (EVENTS, g_print ("...sending to grabber %#x\n",
				     GDK_DRAWABLE_XID (grab_window)));
	  gdk_drawable_unref (*window);
	  *window = grab_window;
	  gdk_drawable_ref (*window);
	  return TRUE;
	}
    }
  while (TRUE)
    {
     if ((*doesnt_want_it) (GDK_WINDOW_WIN32DATA (*window)->event_mask, xevent))
	{
	  /* Owner doesn't want it, propagate to parent. */
	  if (((GdkWindowPrivate *) *window)->parent == gdk_parent_root)
	    {
	      /* No parent; check if grabbed */
	      if (grab_window != NULL)
		{
		  /* Event source is grabbed with owner_events TRUE */
		  GDK_NOTE (EVENTS, g_print ("...undelivered, but grabbed\n"));
		  if ((*doesnt_want_it) (grab_mask, xevent))
		    {
		      /* Grabber doesn't want it either */
		      GDK_NOTE (EVENTS, g_print ("...grabber doesn't want it\n"));
		      return FALSE;
		    }
		  else
		    {
		      /* Grabbed! */
		      GDK_NOTE (EVENTS, g_print ("...sending to grabber %#x\n",
						 GDK_DRAWABLE_XID (grab_window)));
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
	  else
	    {
	      gdk_drawable_unref (*window);
	      *window = ((GdkWindowPrivate *) *window)->parent;
	      gdk_drawable_ref (*window);
	      GDK_NOTE (EVENTS, g_print ("...propagating to %#x\n",
					 GDK_DRAWABLE_XID (*window)));
	      /* The only branch where we actually continue the loop */
	    }
	}
      else
	return TRUE;
    }
}

static gboolean
doesnt_want_key (gint mask,
		 MSG *xevent)
{
  return (((xevent->message == WM_KEYUP
	    || xevent->message == WM_SYSKEYUP)
	   && !(mask & GDK_KEY_RELEASE_MASK))
	  ||
	  ((xevent->message == WM_KEYDOWN
	    || xevent->message == WM_SYSKEYDOWN)
	   && !(mask & GDK_KEY_PRESS_MASK)));
}

static gboolean
doesnt_want_char (gint mask,
		  MSG *xevent)
{
  return !(mask & (GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK));
}

static gboolean
doesnt_want_button_press (gint mask,
			  MSG *xevent)
{
  return !(mask & GDK_BUTTON_PRESS_MASK);
}

static gboolean
doesnt_want_button_release (gint mask,
			    MSG *xevent)
{
  return !(mask & GDK_BUTTON_RELEASE_MASK);
}

static gboolean
doesnt_want_button_motion (gint mask,
			   MSG *xevent)
{
  return !((mask & GDK_POINTER_MOTION_MASK)
	   || ((xevent->wParam & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON))
	       && (mask & GDK_BUTTON_MOTION_MASK))
	   || ((xevent->wParam & MK_LBUTTON)
	       && (mask & GDK_BUTTON1_MOTION_MASK))
	   || ((xevent->wParam & MK_MBUTTON)
	       && (mask & GDK_BUTTON2_MOTION_MASK))
	   || ((xevent->wParam & MK_RBUTTON)
	       && (mask & GDK_BUTTON3_MOTION_MASK)));
}

static gboolean
doesnt_want_scroll (gint mask,
		    MSG *xevent)
{
#if 0
  return !(mask & GDK_SCROLL_MASK);
#else
  return !(mask & GDK_BUTTON_PRESS_MASK);
#endif
}

static gboolean
gdk_event_translate (GdkEvent *event,
		     MSG      *xevent,
		     gboolean *ret_val_flagp,
		     gint     *ret_valp)
{
  DWORD pidActWin;
  DWORD pidThis;
  DWORD dwStyle;
  PAINTSTRUCT paintstruct;
  HDC hdc;
  HDC bgdc;
  HGDIOBJ oldbitmap;
  HBRUSH hbr;
  COLORREF bg;
  RECT rect;
  POINT pt;
  MINMAXINFO *lpmmi;
  HWND hwnd;
  GdkWindow *window, *orig_window, *newwindow;
  GdkColormapPrivateWin32 *colormap_private;
  GdkEventMask mask;
  GdkPixmap *pixmap;
  GdkDrawablePrivate *pixmap_private;
  int button;
  int i, j, n, k;
  gchar buf[256];
  gchar *msgname;
  gboolean return_val;
  gboolean flag;
  
  return_val = FALSE;
  
  if (ret_val_flagp)
    *ret_val_flagp = FALSE;

  window = gdk_window_lookup (xevent->hwnd);
  orig_window = window;
  
  event->any.window = window;
  event->any.send_event = FALSE;

  if (window != NULL)
    gdk_drawable_ref (window);
  else
    {
      /* Handle WM_QUIT here ? */
      if (xevent->message == WM_QUIT)
	{
	  GDK_NOTE (EVENTS, g_print ("WM_QUIT: %d\n", xevent->wParam));
	  exit (xevent->wParam);
	}
      else if (xevent->message == WM_MOVE
	       || xevent->message == WM_SIZE)
	{
	  /* It's quite normal to get these messages before we have
	   * had time to register the window in our lookup table, or
	   * when the window is being destroyed and we already have
	   * removed it. Repost the same message to our queue so that
	   * we will get it later when we are prepared.
	   */
	  GDK_NOTE(MISC, g_print("gdk_event_translate: %#x %s posted.\n",
				 xevent->hwnd, 
				 xevent->message == WM_MOVE ?
				 "WM_MOVE" : "WM_SIZE"));
	
	  PostMessage (xevent->hwnd, xevent->message,
		       xevent->wParam, xevent->lParam);
	}
      return FALSE;
    }
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      /* Check for filters for this window */
      GdkFilterReturn result;

      result = gdk_event_apply_filters
	(xevent, event, ((GdkWindowPrivate *) window)->filters);
      
      if (result != GDK_FILTER_CONTINUE)
	{
	  return_val =  (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	  goto done;
	}
    }

  if (xevent->message == gdk_selection_notify_msg)
    {
      GDK_NOTE (EVENTS, g_print ("gdk_selection_notify_msg: %#x\n",
				 xevent->hwnd));

      event->selection.type = GDK_SELECTION_NOTIFY;
      event->selection.window = window;
      event->selection.selection = xevent->wParam;
      event->selection.target = xevent->lParam;
      event->selection.property = gdk_selection_property;
      event->selection.time = xevent->time;

      return_val = !GDK_DRAWABLE_DESTROYED (window);

      /* Will pass through switch below without match */
    }
  else if (xevent->message == gdk_selection_request_msg)
    {
      GDK_NOTE (EVENTS, g_print ("gdk_selection_request_msg: %#x\n",
				 xevent->hwnd));

      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = gdk_clipboard_atom;
      event->selection.target = GDK_TARGET_STRING;
      event->selection.property = gdk_selection_property;
      event->selection.requestor = (guint32) xevent->hwnd;
      event->selection.time = xevent->time;

      return_val = !GDK_DRAWABLE_DESTROYED (window);

      /* Again, will pass through switch below without match */
    }
  else if (xevent->message == gdk_selection_clear_msg)
    {
      GDK_NOTE (EVENTS, g_print ("gdk_selection_clear_msg: %#x\n",
				 xevent->hwnd));

      event->selection.type = GDK_SELECTION_CLEAR;
      event->selection.window = window;
      event->selection.selection = xevent->wParam;
      event->selection.time = xevent->time;

      return_val = !GDK_DRAWABLE_DESTROYED (window);

      /* Once again, we will pass through switch below without match */
    }
  else
    {
      GList *tmp_list;
      GdkFilterReturn result = GDK_FILTER_CONTINUE;

      tmp_list = client_filters;
      while (tmp_list)
	{
	  GdkClientFilter *filter = tmp_list->data;
	  if (filter->type == xevent->message)
	    {
	      GDK_NOTE (EVENTS, g_print ("client filter matched\n"));
	      event->any.window = window;
	      result = (*filter->function) (xevent, event, filter->data);
	      switch (result)
		{
		case GDK_FILTER_REMOVE:
		  return_val = FALSE;
		  break;

		case GDK_FILTER_TRANSLATE:
		  return_val = TRUE;
		  break;

		case GDK_FILTER_CONTINUE:
		  return_val = TRUE;
		  event->client.type = GDK_CLIENT_EVENT;
		  event->client.window = window;
		  event->client.message_type = xevent->message;
		  event->client.data_format = 0;
		  event->client.data.l[0] = xevent->wParam;
		  event->client.data.l[1] = xevent->lParam;
		  break;
		}
	      goto done;
	    }
	  tmp_list = tmp_list->next;
	}
    }

  switch (xevent->message)
    {
    case WM_INPUTLANGCHANGE:
      GDK_NOTE (EVENTS,
		g_print ("WM_INPUTLANGCHANGE: %#x charset %d locale %x\n",
			 xevent->hwnd, xevent->wParam, xevent->lParam));
      GDK_WINDOW_WIN32DATA (window)->input_locale = (HKL) xevent->lParam;
      TranslateCharsetInfo ((DWORD FAR *) xevent->wParam,
			    &GDK_WINDOW_WIN32DATA (window)->charset_info,
			    TCI_SRCCHARSET);
      break;

    case WM_SYSKEYUP:
    case WM_SYSKEYDOWN:
      GDK_NOTE (EVENTS,
		g_print ("WM_SYSKEY%s: %#x  key: %s  %#x %#.08x\n",
			 (xevent->message == WM_SYSKEYUP ? "UP" : "DOWN"),
			 xevent->hwnd,
			 (GetKeyNameText (xevent->lParam, buf,
					  sizeof (buf)) > 0 ?
			  buf : ""),
			 xevent->wParam,
			 xevent->lParam));

      /* Let the system handle Alt-Tab and Alt-Enter */
      if (xevent->wParam == VK_TAB
	  || xevent->wParam == VK_RETURN
	  || xevent->wParam == VK_F4)
	break;
      /* If posted without us having keyboard focus, ignore */
      if (!(xevent->lParam & 0x20000000))
	break;
#if 0
      /* don't generate events for just the Alt key */
      if (xevent->wParam == VK_MENU)
	break;
#endif
      /* Jump to code in common with WM_KEYUP and WM_KEYDOWN */
      goto keyup_or_down;

    case WM_KEYUP:
    case WM_KEYDOWN:
      GDK_NOTE (EVENTS, 
		g_print ("WM_KEY%s: %#x  key: %s  %#x %#.08x\n",
			 (xevent->message == WM_KEYUP ? "UP" : "DOWN"),
			 xevent->hwnd,
			 (GetKeyNameText (xevent->lParam, buf,
					  sizeof (buf)) > 0 ?
			  buf : ""),
			 xevent->wParam,
			 xevent->lParam));

      ignore_WM_CHAR = TRUE;

    keyup_or_down:
      if (!propagate (&window, xevent,
		      k_grab_window, k_grab_owner_events, GDK_ALL_EVENTS_MASK,
		      doesnt_want_key))
	  break;
      event->key.window = window;
      switch (xevent->wParam)
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
	  event->key.keyval = (GetKeyState(VK_SHIFT) < 0 ? 
	    GDK_ISO_Left_Tab : GDK_Tab);
	  break;
	case VK_CLEAR:
	  event->key.keyval = GDK_Clear; break;
	case VK_RETURN:
	  event->key.keyval = GDK_Return; break;
	case VK_SHIFT:
	  /* Don't let Shift auto-repeat */
	  if (xevent->message == WM_KEYDOWN
	      && (xevent->lParam & 0x40000000))
	    ignore_WM_CHAR = FALSE;
	  else
	    event->key.keyval = GDK_Shift_L;
	  break;
	case VK_CONTROL:
	  /* And not Control either */
	  if (xevent->message == WM_KEYDOWN
	      && (xevent->lParam & 0x40000000))
	    ignore_WM_CHAR = FALSE;
	  else if (xevent->lParam & 0x01000000)
	    event->key.keyval = GDK_Control_R;
	  else
	    event->key.keyval = GDK_Control_L;
	  break;
	case VK_MENU:
	  /* And not Alt */
	  if (xevent->message == WM_KEYDOWN
	      && (xevent->lParam & 0x40000000))
	    ignore_WM_CHAR = FALSE;
	  else if (xevent->lParam & 0x01000000)
	    {
	      /* AltGr key comes in as Control+Right Alt */
	      if (GetKeyState (VK_CONTROL) < 0)
		{
		  ignore_WM_CHAR = FALSE;
		  is_AltGr_key = TRUE;
		}
	      event->key.keyval = GDK_Alt_R;
	    }
	  else
	    event->key.keyval = GDK_Alt_L;
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
	  ignore_WM_CHAR = FALSE;
	  break;
	case VK_MULTIPLY:
	  event->key.keyval = GDK_KP_Multiply; break;
	case VK_ADD:
	  /* Pass it on as an ASCII plus in WM_CHAR. */
	  ignore_WM_CHAR = FALSE;
	  break;
	case VK_SEPARATOR:
	  event->key.keyval = GDK_KP_Separator; break;
	case VK_SUBTRACT:
	  /* Pass it on as an ASCII minus in WM_CHAR. */
	  ignore_WM_CHAR = FALSE;
	  break;
	case VK_DECIMAL:
	  /* The keypad decimal key should also be passed on as the decimal
	   * sign ('.' or ',' depending on the Windows locale settings,
	   * apparently). So wait for the WM_CHAR here, also.
	   */
	  ignore_WM_CHAR = FALSE;
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
	  if (!is_AltGr_key && (GetKeyState (VK_CONTROL) < 0
				|| GetKeyState (VK_MENU) < 0))
	    /* Control- or Alt-digits won't come in as a WM_CHAR,
	     * but beware of AltGr-digits, which are used for instance
	     * on Finnish keyboards.
	     */
	    event->key.keyval = GDK_0 + (xevent->wParam - '0');
	  else
	    ignore_WM_CHAR = FALSE;
	  break;
	case VK_OEM_PLUS:	/* On my Win98, the '+' key comes in
				 * as VK_OEM_PLUS
				 */
	  if (!is_AltGr_key && (GetKeyState (VK_CONTROL) < 0
				|| GetKeyState (VK_MENU) < 0))
	    /* Control- or Alt-plus won't come in as WM_CHAR,
	     * but beware of AltGr-plus which is backslash on
	     * Finnish keyboards
	     */
	    event->key.keyval = '+';
	  else
	    ignore_WM_CHAR = FALSE;
	  break;
	default:
	  if (xevent->message == WM_SYSKEYDOWN || xevent->message == WM_SYSKEYUP)
	    event->key.keyval = xevent->wParam;
	  else
	    ignore_WM_CHAR = FALSE;
	  break;
	}

      if (!ignore_WM_CHAR)
	break;

      is_AltGr_key = FALSE;
      event->key.type = ((xevent->message == WM_KEYDOWN
			  || xevent->message == WM_SYSKEYDOWN) ?
			 GDK_KEY_PRESS : GDK_KEY_RELEASE);
      event->key.time = xevent->time;
      event->key.state = 0;
      if (GetKeyState (VK_SHIFT) < 0)
	event->key.state |= GDK_SHIFT_MASK;
      if (GetKeyState (VK_CAPITAL) & 0x1)
	event->key.state |= GDK_LOCK_MASK;
      if (GetKeyState (VK_CONTROL) < 0)
	event->key.state |= GDK_CONTROL_MASK;
      if (xevent->wParam != VK_MENU && GetKeyState (VK_MENU) < 0)
	event->key.state |= GDK_MOD1_MASK;
      event->key.string = NULL;
      event->key.length = 0;
      return_val = !GDK_DRAWABLE_DESTROYED (window);
      break;

    case WM_IME_COMPOSITION:
      if (!use_IME_COMPOSITION)
	break;
      GDK_NOTE (EVENTS, g_print ("WM_IME_COMPOSITION: %#x  %#x\n",
				 xevent->hwnd, xevent->lParam));
      if (xevent->lParam & GCS_RESULTSTR)
	goto wm_char;
      break;

    case WM_IME_CHAR:
      GDK_NOTE (EVENTS,
		g_print ("WM_IME_CHAR: %#x  bytes: %#.04x\n",
			 xevent->hwnd, xevent->wParam));
      goto wm_char;
      
    case WM_CHAR:
      GDK_NOTE (EVENTS, 
		g_print ("WM_CHAR: %#x  char: %#x %#.08x  %s\n",
			 xevent->hwnd, xevent->wParam, xevent->lParam,
			 (ignore_WM_CHAR ? "ignored" : "")));

      if (ignore_WM_CHAR)
	{
	  ignore_WM_CHAR = FALSE;
	  break;
	}

    wm_char:
      if (!propagate (&window, xevent,
		      k_grab_window, k_grab_owner_events, GDK_ALL_EVENTS_MASK,
		      doesnt_want_char))
	  break;
      event->key.window = window;
      return_val = !GDK_DRAWABLE_DESTROYED (window);
      if (return_val && (event->key.window == k_grab_window
			 || (GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_KEY_RELEASE_MASK)))
	{
	  if (window == k_grab_window
	      || (GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_KEY_PRESS_MASK))
	    {
	      /* Append a GDK_KEY_PRESS event to the pushback list
	       * (from which it will be fetched before the release
	       * event).
	       */
	      GdkEvent *event2 = gdk_event_new ();
	      build_keypress_event (GDK_WINDOW_WIN32DATA (window), event2, xevent);
	      event2->key.window = window;
	      gdk_drawable_ref (window);
	      gdk_event_queue_append (event2);
	      GDK_NOTE (EVENTS, print_event (event2));
	    }
	  /* Return the key release event.  */
	  build_keyrelease_event (GDK_WINDOW_WIN32DATA (window), event, xevent);
	}
      else if (return_val
	       && (GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_KEY_PRESS_MASK))
	{
	  /* Return just the key press event. */
	  build_keypress_event (GDK_WINDOW_WIN32DATA (window), event, xevent);
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
		g_print ("WM_%cBUTTONDOWN: %#x  (%d,%d)\n",
			 " LMR"[button],
			 xevent->hwnd,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

      if (((GdkWindowPrivate *) window)->extension_events != 0
	  && gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      if (window != curWnd)
	synthesize_crossing_events (window, xevent);

      event->button.type = GDK_BUTTON_PRESS;
      if (!propagate (&window, xevent,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_button_press))
	  break;
      event->button.window = window;
      /* Emulate X11's automatic active grab */
      if (!p_grab_window)
	{
	  /* No explicit active grab, let's start one automatically */
	  gint owner_events =
	    GDK_WINDOW_WIN32DATA (window)->event_mask
	    & (GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK);
	  
	  GDK_NOTE (EVENTS, g_print ("...automatic grab started\n"));
	  gdk_pointer_grab (window,
			    owner_events,
			    GDK_WINDOW_WIN32DATA (window)->event_mask,
			    NULL, NULL, 0);
	  p_grab_automatic = TRUE;
	}

      event->button.time = xevent->time;
      if (window != orig_window)
	translate_mouse_coords (orig_window, window, xevent);
      event->button.x = curX = (gint16) LOWORD (xevent->lParam);
      event->button.y = curY = (gint16) HIWORD (xevent->lParam);
      event->button.x_root = xevent->pt.x;
      event->button.y_root = xevent->pt.y;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = build_pointer_event_state (xevent);
      event->button.button = button;
      event->button.source = GDK_SOURCE_MOUSE;
      event->button.deviceid = GDK_CORE_POINTER;

      gdk_event_button_generate (event);
      
      return_val = !GDK_DRAWABLE_DESTROYED (window);
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
		g_print ("WM_%cBUTTONUP: %#x  (%d,%d)\n",
			 " LMR"[button],
			 xevent->hwnd,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

      if (((GdkWindowPrivate *) window)->extension_events != 0
	  && gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      if (window != curWnd)
	synthesize_crossing_events (window, xevent);

      event->button.type = GDK_BUTTON_RELEASE;
      if (!propagate (&window, xevent,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_button_release))
	  goto maybe_ungrab;
      event->button.window = window;
      event->button.time = xevent->time;
      if (window != orig_window)
	translate_mouse_coords (orig_window, window, xevent);
      event->button.x = (gint16) LOWORD (xevent->lParam);
      event->button.y = (gint16) HIWORD (xevent->lParam);
      event->button.x_root = xevent->pt.x;
      event->button.y_root = xevent->pt.y;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = build_pointer_event_state (xevent);
      event->button.button = button;
      event->button.source = GDK_SOURCE_MOUSE;
      event->button.deviceid = GDK_CORE_POINTER;

      return_val = !GDK_DRAWABLE_DESTROYED (window);

    maybe_ungrab:
      if (p_grab_window != NULL
	  && p_grab_automatic
	  && (event->button.state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) == 0)
	gdk_pointer_ungrab (0);
      break;

    case WM_MOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print ("WM_MOUSEMOVE: %#x  %#x (%d,%d)\n",
			 xevent->hwnd, xevent->wParam,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

      /* If we haven't moved, don't create any event.
       * Windows sends WM_MOUSEMOVE messages after button presses
       * even if the mouse doesn't move. This disturbs gtk.
       */
      if (window == curWnd
	  && LOWORD (xevent->lParam) == curX
	  && HIWORD (xevent->lParam) == curY)
	break;

      /* HB: only process mouse move messages if we own the active window. */
      GetWindowThreadProcessId(GetActiveWindow(), &pidActWin);
      GetWindowThreadProcessId(xevent->hwnd, &pidThis);
      if (pidActWin != pidThis)
	break;

      if (window != curWnd)
	synthesize_crossing_events (window, xevent);

      if (((GdkWindowPrivate *) window)->extension_events != 0
	  && gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      event->motion.type = GDK_MOTION_NOTIFY;
      if (!propagate (&window, xevent,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_button_motion))
	  break;
      event->motion.window = window;
      event->motion.time = xevent->time;
      if (window != orig_window)
	translate_mouse_coords (orig_window, window, xevent);
      event->motion.x = curX = (gint16) LOWORD (xevent->lParam);
      event->motion.y = curY = (gint16) HIWORD (xevent->lParam);
      event->motion.x_root = xevent->pt.x;
      event->motion.y_root = xevent->pt.y;
      curXroot = event->motion.x_root;
      curYroot = event->motion.y_root;
      event->motion.pressure = 0.5;
      event->motion.xtilt = 0;
      event->motion.ytilt = 0;
      event->motion.state = build_pointer_event_state (xevent);
      event->motion.is_hint = FALSE;
      event->motion.source = GDK_SOURCE_MOUSE;
      event->motion.deviceid = GDK_CORE_POINTER;

      return_val = !GDK_DRAWABLE_DESTROYED (window);
      break;

    case WM_NCMOUSEMOVE:
      GDK_NOTE (EVENTS,
		g_print ("WM_NCMOUSEMOVE: %#x  x,y: %d %d\n",
			 xevent->hwnd,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));
      if (p_TrackMouseEvent == NULL
	  && curWnd != NULL
	  && (GDK_WINDOW_WIN32DATA (curWnd)->event_mask & GDK_LEAVE_NOTIFY_MASK))
	{
	  GDK_NOTE (EVENTS, g_print ("...synthesizing LEAVE_NOTIFY event\n"));

	  event->crossing.type = GDK_LEAVE_NOTIFY;
	  event->crossing.window = curWnd;
	  event->crossing.subwindow = NULL;
	  event->crossing.time = xevent->time;
	  event->crossing.x = curX;
	  event->crossing.y = curY;
	  event->crossing.x_root = curXroot;
	  event->crossing.y_root = curYroot;
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;

	  event->crossing.focus = TRUE; /* ??? */
	  event->crossing.state = 0; /* ??? */
	  return_val = TRUE;
	}

      if (curWnd)
	{
	  gdk_drawable_unref (curWnd);
	  curWnd = NULL;
	}

      break;

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x20a
#endif

    case WM_MOUSEWHEEL:
      GDK_NOTE (EVENTS, g_print ("WM_MOUSEWHEEL: %#x\n", xevent->hwnd));

      if (((GdkWindowPrivate *) window)->extension_events != 0
	  && gdk_input_ignore_core)
	{
	  GDK_NOTE (EVENTS, g_print ("...ignored\n"));
	  break;
	}

      event->scroll.type = GDK_SCROLL;

      /* WM_MOUSEWHEEL seems to be delivered to top-level windows
       * only, for some reason. Work around that. Also, the position
       * is in screen coordinates, not client coordinates as with the
       * button messages. I love the consistency of Windows.
       */
      pt.x = LOWORD (xevent->lParam);
      pt.y = HIWORD (xevent->lParam);
      if ((hwnd = WindowFromPoint (pt)) == NULL)
	break;
      xevent->hwnd = hwnd;
      if ((newwindow = gdk_window_lookup (xevent->hwnd)) == NULL)
	break;
      if (newwindow != window)
	{
	  gdk_drawable_unref (window);
	  window = newwindow;
	  gdk_drawable_ref (window);
	}
      ScreenToClient (xevent->hwnd, &pt);
      if (!propagate (&window, xevent,
		      p_grab_window, p_grab_owner_events, p_grab_mask,
		      doesnt_want_scroll))
	break;
      event->button.window = window;
      event->scroll.direction = (((short) HIWORD (xevent->wParam)) > 0) ?
	GDK_SCROLL_UP : GDK_SCROLL_DOWN;
      event->scroll.window = window;
      event->scroll.time = xevent->time;
      event->scroll.x = (gint16) pt.x;
      event->scroll.y = (gint16) pt.y;
      event->scroll.x_root = (gint16) LOWORD (xevent->lParam);
      event->scroll.y_root = (gint16) LOWORD (xevent->lParam);
      event->scroll.pressure = 0.5;
      event->scroll.xtilt = 0;
      event->scroll.ytilt = 0;
      event->scroll.state = build_pointer_event_state (xevent);
      event->scroll.source = GDK_SOURCE_MOUSE;
      event->scroll.deviceid = GDK_CORE_POINTER;
      return_val = !GDK_DRAWABLE_DESTROYED (window);
      
      break;

#ifdef USE_TRACKMOUSEEVENT
    case WM_MOUSELEAVE:
      GDK_NOTE (EVENTS, g_print ("WM_MOUSELEAVE: %#x\n", xevent->hwnd));

      if (!(GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_LEAVE_NOTIFY_MASK))
	break;

      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = window;
      event->crossing.subwindow = NULL;
      event->crossing.time = xevent->time;
      event->crossing.x = curX;
      event->crossing.y = curY;
      event->crossing.x_root = curXroot;
      event->crossing.y_root = curYroot;
      event->crossing.mode = GDK_CROSSING_NORMAL;
      if (curWnd
	  && IsChild (GDK_DRAWABLE_XID (curWnd), GDK_DRAWABLE_XID (window)))
	event->crossing.detail = GDK_NOTIFY_INFERIOR;
      else if (curWnd
	       && IsChild (GDK_DRAWABLE_XID (window), GDK_DRAWABLE_XID (curWnd)))
	event->crossing.detail = GDK_NOTIFY_ANCESTOR;
      else
	event->crossing.detail = GDK_NOTIFY_NONLINEAR;

      event->crossing.focus = TRUE; /* ??? */
      event->crossing.state = 0; /* ??? */

      if (curWnd)
	{
	  gdk_drawable_unref (curWnd);
	  curWnd = NULL;
	}

      return_val = !GDK_DRAWABLE_DESTROYED (window);
      break;
#endif
	
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
      GDK_NOTE (EVENTS, g_print ("WM_%sFOCUS: %#x\n",
				 (xevent->message == WM_SETFOCUS ?
				  "SET" : "KILL"),
				 xevent->hwnd));
      
      if (!(GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_FOCUS_CHANGE_MASK))
	break;

      event->focus_change.type = GDK_FOCUS_CHANGE;
      event->focus_change.window = window;
      event->focus_change.in = (xevent->message == WM_SETFOCUS);
      return_val = !GDK_DRAWABLE_DESTROYED (window);
      break;

    case WM_ERASEBKGND:
      GDK_NOTE (EVENTS, g_print ("WM_ERASEBKGND: %#x  dc %#x\n",
				 xevent->hwnd, xevent->wParam));
      
      if (GDK_DRAWABLE_DESTROYED (window))
	break;

      colormap_private = (GdkColormapPrivateWin32 *) ((GdkWindowPrivate *) window)->drawable.colormap;
      hdc = (HDC) xevent->wParam;
      if (colormap_private
	  && colormap_private->xcolormap->rc_palette)
	{
	  int k;

	  if (SelectPalette (hdc,  colormap_private->xcolormap->palette,
			     FALSE) == NULL)
	    WIN32_GDI_FAILED ("SelectPalette");
	  if ((k = RealizePalette (hdc)) == GDI_ERROR)
	    WIN32_GDI_FAILED ("RealizePalette");
#if 0
	  g_print ("WM_ERASEBKGND: selected %#x, realized %d colors\n",
		   colormap_private->xcolormap->palette, k);
#endif
	}
      *ret_val_flagp = TRUE;
      *ret_valp = 1;

      if (GDK_WINDOW_WIN32DATA (window)->bg_type == GDK_WIN32_BG_TRANSPARENT)
	break;

      if (GDK_WINDOW_WIN32DATA (window)->bg_type == GDK_WIN32_BG_PARENT_RELATIVE)
	{
	  /* If this window should have the same background as the
	   * parent, fetch the parent. (And if the same goes for
	   * the parent, fetch the grandparent, etc.)
	   */
	  while (window
		 && GDK_WINDOW_WIN32DATA (window)->bg_type == GDK_WIN32_BG_PARENT_RELATIVE)
	    {
	      gdk_drawable_unref (window);
	      window = ((GdkWindowPrivate *) window)->parent;
	      gdk_drawable_ref (window);
	    }
	}

      if (GDK_WINDOW_WIN32DATA (window)->bg_type == GDK_WIN32_BG_PIXEL)
	{
	  bg = gdk_colormap_color (colormap_private,
				   GDK_WINDOW_WIN32DATA (window)->bg_pixel);

	  GetClipBox (hdc, &rect);
	  GDK_NOTE (EVENTS,
		    g_print ("...%dx%d@+%d+%d BG_PIXEL %.06x\n",
			     rect.right - rect.left,
			     rect.bottom - rect.top,
			     rect.left, rect.top,
			     bg));
	  hbr = CreateSolidBrush (bg);
#if 0
	  g_print ("...CreateSolidBrush (%.08x) = %.08x\n", bg, hbr);
#endif
	  if (!FillRect (hdc, &rect, hbr))
	    WIN32_GDI_FAILED ("FillRect");
	  DeleteObject (hbr);
	}
      else if (GDK_WINDOW_WIN32DATA (window)->bg_type == GDK_WIN32_BG_PIXMAP)
	{
	  pixmap = GDK_WINDOW_WIN32DATA (window)->bg_pixmap;
	  pixmap_private = (GdkDrawablePrivate*) pixmap;
	  GetClipBox (hdc, &rect);

	  if (pixmap_private->width <= 8
	      && pixmap_private->height <= 8)
	    {
	      GDK_NOTE (EVENTS, g_print ("...small pixmap, using brush\n"));
	      hbr = CreatePatternBrush (GDK_DRAWABLE_XID (pixmap));
	      if (!FillRect (hdc, &rect, hbr))
		WIN32_GDI_FAILED ("FillRect");
	      DeleteObject (hbr);
	    }
	  else
	    {
	      GDK_NOTE (EVENTS,
			g_print ("...blitting pixmap %#x (%dx%d) "
				 "all over the place,\n"
				 "...clip box = %dx%d@+%d+%d\n",
				 GDK_DRAWABLE_XID (pixmap),
				 pixmap_private->width, pixmap_private->height,
				 rect.right - rect.left, rect.bottom - rect.top,
				 rect.left, rect.top));

	      if (!(bgdc = CreateCompatibleDC (hdc)))
		{
		  WIN32_GDI_FAILED ("CreateCompatibleDC");
		  break;
		}
	      if (!(oldbitmap = SelectObject (bgdc, GDK_DRAWABLE_XID (pixmap))))
		{
		  WIN32_GDI_FAILED ("SelectObject");
		  DeleteDC (bgdc);
		  break;
		}
	      i = 0;
	      while (i < rect.right)
		{
		  j = 0;
		  while (j < rect.bottom)
		    {
		      if (i + pixmap_private->width >= rect.left
			  && j + pixmap_private->height >= rect.top)
			{
			  if (!BitBlt (hdc, i, j,
				       pixmap_private->width, pixmap_private->height,
				       bgdc, 0, 0, SRCCOPY))
			    {
			      WIN32_GDI_FAILED ("BitBlt");
			      goto loopexit;
			    }
			}
		      j += pixmap_private->height;
		    }
		  i += pixmap_private->width;
		}
	    loopexit:
	      SelectObject (bgdc, oldbitmap);
	      DeleteDC (bgdc);
	    }
	}
      else
	{
	  GDK_NOTE (EVENTS, g_print ("...BLACK_BRUSH (?)\n"));
	  hbr = GetStockObject (BLACK_BRUSH);
	  GetClipBox (hdc, &rect);
	  if (!FillRect (hdc, &rect, hbr))
	    WIN32_GDI_FAILED ("FillRect");
	}
      break;

    case WM_PAINT:
      if (!GetUpdateRect(xevent->hwnd, NULL, FALSE))
        {
          GDK_NOTE (EVENTS, g_print ("WM_PAINT: %#x no update rect\n",
				     xevent->hwnd));
          break;
        }

      hdc = BeginPaint (xevent->hwnd, &paintstruct);

      GDK_NOTE (EVENTS,
		g_print ("WM_PAINT: %#x  %dx%d@+%d+%d %s dc %#x\n",
			 xevent->hwnd,
			 paintstruct.rcPaint.right - paintstruct.rcPaint.left,
			 paintstruct.rcPaint.bottom - paintstruct.rcPaint.top,
			 paintstruct.rcPaint.left, paintstruct.rcPaint.top,
			 (paintstruct.fErase ? "erase" : ""),
			 hdc));

      EndPaint (xevent->hwnd, &paintstruct);

      if (!(GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_EXPOSURE_MASK))
	break;

      if ((paintstruct.rcPaint.right == paintstruct.rcPaint.left)
          || (paintstruct.rcPaint.bottom == paintstruct.rcPaint.top))
        break;

      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = paintstruct.rcPaint.left;
      event->expose.area.y = paintstruct.rcPaint.top;
      event->expose.area.width = paintstruct.rcPaint.right - paintstruct.rcPaint.left;
      event->expose.area.height = paintstruct.rcPaint.bottom - paintstruct.rcPaint.top;
      event->expose.count = 0;

      return_val = !GDK_DRAWABLE_DESTROYED (window);
      if (return_val)
	{
	  GList *list = gdk_queued_events;
	  while (list != NULL )
	    {
	      if ((((GdkEvent *)list->data)->any.type == GDK_EXPOSE) &&
		  (((GdkEvent *)list->data)->any.window == window) &&
		  !(((GdkEventPrivate *)list->data)->flags & GDK_EVENT_PENDING))
		((GdkEvent *)list->data)->expose.count++;
	      
	      list = list->next;
	    }
	}
      break;

    case WM_SETCURSOR:
      GDK_NOTE (EVENTS, g_print ("WM_SETCURSOR: %#x %#x %#x\n",
				 xevent->hwnd,
				 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

      if (LOWORD (xevent->lParam) != HTCLIENT)
	break;
      if (p_grab_window != NULL && p_grab_cursor != NULL)
	{
	  GDK_NOTE (EVENTS, g_print ("...SetCursor(%#x)\n", p_grab_cursor));
	  SetCursor (p_grab_cursor);
	}
      else if (!GDK_DRAWABLE_DESTROYED (window)
	       && GDK_WINDOW_WIN32DATA (window)->xcursor)
	{
	  GDK_NOTE (EVENTS, g_print ("...SetCursor(%#x)\n",
				     GDK_WINDOW_WIN32DATA (window)->xcursor));
	  SetCursor (GDK_WINDOW_WIN32DATA (window)->xcursor);
	}

      if (window != curWnd)
	synthesize_crossing_events (window, xevent);

      *ret_val_flagp = TRUE;
      *ret_valp = FALSE;
      break;

    case WM_SHOWWINDOW:
      GDK_NOTE (EVENTS, g_print ("WM_SHOWWINDOW: %#x  %d\n",
				 xevent->hwnd,
				 xevent->wParam));

      if (!(GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_STRUCTURE_MASK))
	break;

      event->any.type = (xevent->wParam ? GDK_MAP : GDK_UNMAP);
      event->any.window = window;

      if (event->any.type == GDK_UNMAP
	  && p_grab_window == window)
	gdk_pointer_ungrab (xevent->time);

      if (event->any.type == GDK_UNMAP
	  && k_grab_window == window)
	gdk_keyboard_ungrab (xevent->time);

      return_val = !GDK_DRAWABLE_DESTROYED (window);
      break;

    case WM_SIZE:
      GDK_NOTE (EVENTS,
		g_print ("WM_SIZE: %#x  %s %dx%d\n",
			 xevent->hwnd,
			 (xevent->wParam == SIZE_MAXHIDE ? "MAXHIDE" :
			  (xevent->wParam == SIZE_MAXIMIZED ? "MAXIMIZED" :
			   (xevent->wParam == SIZE_MAXSHOW ? "MAXSHOW" :
			    (xevent->wParam == SIZE_MINIMIZED ? "MINIMIZED" :
			     (xevent->wParam == SIZE_RESTORED ? "RESTORED" : "?"))))),
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

      if (!(GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_STRUCTURE_MASK))
	break;

      if (xevent->wParam == SIZE_MINIMIZED)
	{
	  event->any.type = GDK_UNMAP;
	  event->any.window = window;

	  if (p_grab_window == window)
	    gdk_pointer_ungrab (xevent->time);

	  if (k_grab_window == window)
	    gdk_keyboard_ungrab (xevent->time);

	  return_val = !GDK_DRAWABLE_DESTROYED (window);
	}
      else if ((xevent->wParam == SIZE_RESTORED
		|| xevent->wParam == SIZE_MAXIMIZED)
#if 1
	       && GDK_DRAWABLE_TYPE (window) != GDK_WINDOW_CHILD
#endif
								 )
	{
	  if (LOWORD (xevent->lParam) == 0)
	    break;

	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  pt.x = 0;
	  pt.y = 0;
	  ClientToScreen (xevent->hwnd, &pt);
	  event->configure.x = pt.x;
	  event->configure.y = pt.y;
	  event->configure.width = LOWORD (xevent->lParam);
	  event->configure.height = HIWORD (xevent->lParam);
	  ((GdkWindowPrivate *) window)->x = event->configure.x;
	  ((GdkWindowPrivate *) window)->y = event->configure.y;
	  ((GdkWindowPrivate *) window)->drawable.width = event->configure.width;
	  ((GdkWindowPrivate *) window)->drawable.height = event->configure.height;
	  if (((GdkWindowPrivate *) window)->resize_count > 1)
	    ((GdkWindowPrivate *) window)->resize_count -= 1;
	  
	  return_val = !GDK_DRAWABLE_DESTROYED (window);
	  if (return_val
	      && ((GdkWindowPrivate *) window)->extension_events != 0
	      && gdk_input_vtable.configure_event)
	    gdk_input_vtable.configure_event (&event->configure, window);
	}
      break;

    case WM_GETMINMAXINFO:
      GDK_NOTE (EVENTS, g_print ("WM_GETMINMAXINFO: %#x\n", xevent->hwnd));

      lpmmi = (MINMAXINFO*) xevent->lParam;
      if (GDK_WINDOW_WIN32DATA (window)->hint_flags & GDK_HINT_MIN_SIZE)
	{
	  lpmmi->ptMinTrackSize.x = GDK_WINDOW_WIN32DATA (window)->hint_min_width;
	  lpmmi->ptMinTrackSize.y = GDK_WINDOW_WIN32DATA (window)->hint_min_height;
	}
      if (GDK_WINDOW_WIN32DATA (window)->hint_flags & GDK_HINT_MAX_SIZE)
	{
	  lpmmi->ptMaxTrackSize.x = GDK_WINDOW_WIN32DATA (window)->hint_max_width;
	  lpmmi->ptMaxTrackSize.y = GDK_WINDOW_WIN32DATA (window)->hint_max_height;
	    
	  lpmmi->ptMaxSize.x = GDK_WINDOW_WIN32DATA (window)->hint_max_width;
	  lpmmi->ptMaxSize.y = GDK_WINDOW_WIN32DATA (window)->hint_max_height;
	}
      break;

    case WM_MOVE:
      GDK_NOTE (EVENTS, g_print ("WM_MOVE: %#x  (%d,%d)\n",
				 xevent->hwnd,
				 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));

      if (!(GDK_WINDOW_WIN32DATA (window)->event_mask & GDK_STRUCTURE_MASK))
	break;

      if (GDK_DRAWABLE_TYPE (window) != GDK_WINDOW_CHILD
	  && !IsIconic(xevent->hwnd)
          && IsWindowVisible(xevent->hwnd))
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.x = LOWORD (xevent->lParam);
	  event->configure.y = HIWORD (xevent->lParam);
	  GetClientRect (xevent->hwnd, &rect);
	  event->configure.width = rect.right;
	  event->configure.height = rect.bottom;
	  ((GdkWindowPrivate *) window)->x = event->configure.x;
	  ((GdkWindowPrivate *) window)->y = event->configure.y;
	  ((GdkWindowPrivate *) window)->drawable.width = event->configure.width;
	  ((GdkWindowPrivate *) window)->drawable.height = event->configure.height;
	  
	  return_val = !GDK_DRAWABLE_DESTROYED (window);
	}
      break;

    case WM_CLOSE:
      GDK_NOTE (EVENTS, g_print ("WM_CLOSE: %#x\n", xevent->hwnd));

      event->any.type = GDK_DELETE;
      event->any.window = window;
      
      return_val = !GDK_DRAWABLE_DESTROYED (window);
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
	g_print ("WM_%s: %#x %#x (%s)\n",
		 (xevent->message == WM_RENDERFORMAT ? "RENDERFORMAT" :
		  "RENDERALLFORMATS"),
		 xevent->hwnd,
		 xevent->wParam,
		 (xevent->wParam == CF_TEXT ? "CF_TEXT" :
		  (xevent->wParam == CF_DIB ? "CF_DIB" :
		   (xevent->wParam == CF_UNICODETEXT ? "CF_UNICODETEXT" :
		    (GetClipboardFormatName (xevent->wParam, buf, sizeof (buf)), buf)))));

#if 0
      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = gdk_clipboard_atom;
      if (xevent->wParam == CF_TEXT)
	event->selection.target = GDK_TARGET_STRING;
      else
	{
	  GetClipboardFormatName (xevent->wParam, buf, sizeof (buf));
	  event->selection.target = gdk_atom_intern (buf, FALSE);
	}
      event->selection.property = gdk_selection_property;
      event->selection.requestor = (guint32) xevent->hwnd;
      event->selection.time = xevent->time;
      return_val = !GDK_DRAWABLE_DESTROYED (window);
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
      GDK_NOTE (EVENTS, g_print ("WM_DESTROY: %#x\n", xevent->hwnd));

      event->any.type = GDK_DESTROY;
      event->any.window = window;
      if (window != NULL && window == curWnd)
	{
	  gdk_drawable_unref (curWnd);
	  curWnd = NULL;
	}

      if (p_grab_window == window)
	gdk_pointer_ungrab (xevent->time);

      if (k_grab_window == window)
	gdk_keyboard_ungrab (xevent->time);

      return_val = window != NULL && !GDK_DRAWABLE_DESTROYED (window);

      if (window != NULL)
	gdk_window_destroy_notify (window);

      break;

#ifdef HAVE_WINTAB
      /* Handle WINTAB events here, as we know that gdkinput.c will
       * use the fixed WT_DEFBASE as lcMsgBase, and we thus can use the
       * constants as case labels.
       */
    case WT_PACKET:
      GDK_NOTE (EVENTS, g_print ("WT_PACKET: %#x %d %#x\n",
				 xevent->hwnd,
				 xevent->wParam, xevent->lParam));
      goto wintab;
      
    case WT_CSRCHANGE:
      GDK_NOTE (EVENTS, g_print ("WT_CSRCHANGE: %#x %d %#x\n",
				 xevent->hwnd,
				 xevent->wParam, xevent->lParam));
      goto wintab;
      
    case WT_PROXIMITY:
      GDK_NOTE (EVENTS,
		g_print ("WT_PROXIMITY: %#x %#x %d %d\n",
			 xevent->hwnd, xevent->wParam,
			 LOWORD (xevent->lParam), HIWORD (xevent->lParam)));
      /* Fall through */
    wintab:
      event->any.window = window;
      return_val = gdk_input_vtable.other_event(event, xevent);
      break;
#endif

    default:
      GDK_NOTE (EVENTS, g_print ("%s: %#x %#x %#x\n",
				 gdk_win32_message_name (xevent->message),
				 xevent->hwnd,
				 xevent->wParam, xevent->lParam));
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
gdk_events_queue (void)
{
  GList *node;
  GdkEvent *event;
  MSG msg;
  LRESULT lres;

  while (!gdk_event_queue_find_first ()
	 && PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
      GDK_NOTE (EVENTS, g_print ("PeekMessage: %#x %#x\n",
				 msg.hwnd, msg.message));

      if (paimmmpo == NULL
	  || (paimmmpo->lpVtbl->OnTranslateMessage) (paimmmpo, &msg) != S_OK)
	TranslateMessage (&msg);

      if (msg.message == g_pipe_readable_msg)
	{
	  GDK_NOTE (EVENTS, g_print ("g_pipe_readable_msg: %d %d\n",
				     msg.wParam, msg.lParam));

	  g_io_channel_win32_pipe_readable (msg.wParam, msg.lParam);

	  continue;
	}
      
      DispatchMessage (&msg);
    }
}

static gboolean  
gdk_event_prepare (gpointer  source_data, 
		   GTimeVal *current_time,
		   gint     *timeout,
		   gpointer  user_data)
{
  MSG msg;
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  *timeout = -1;

  retval = (gdk_event_queue_find_first () != NULL)
	      || PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_check (gpointer  source_data,
		 GTimeVal *current_time,
		 gpointer  user_data)
{
  MSG msg;
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  if (event_poll_fd.revents & G_IO_IN)
    retval = (gdk_event_queue_find_first () != NULL)
	      || PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE);
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_dispatch (gpointer  source_data,
		    GTimeVal *current_time,
		    gpointer  user_data)
{
  GdkEvent *event;
 
  GDK_THREADS_ENTER ();

  gdk_events_queue();
  event = gdk_event_unqueue();

  if (event)
    {
      if (gdk_event_func)
	(*gdk_event_func) (event, gdk_event_data);
      
      gdk_event_free (event);
    }
  
  GDK_THREADS_LEAVE ();

  return TRUE;
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message (GdkEvent *event, guint32 xid)
{
  /* XXX */
  return FALSE;
}

void
gdk_event_send_clientmessage_toall (GdkEvent *event)
{
  /* XXX */
}

void
gdk_flush (void)
{
  GdiFlush ();
}

#ifdef G_ENABLE_DEBUG

gchar *
gdk_win32_message_name (UINT msg)
{
  static gchar bfr[100];

  switch (msg)
    {
#define CASE(x) case x: return #x
      CASE (WM_NULL);
      CASE (WM_CREATE);
      CASE (WM_DESTROY);
      CASE (WM_MOVE);
      CASE (WM_SIZE);
      CASE (WM_ACTIVATE);
      CASE (WM_SETFOCUS);
      CASE (WM_KILLFOCUS);
      CASE (WM_ENABLE);
      CASE (WM_SETREDRAW);
      CASE (WM_SETTEXT);
      CASE (WM_GETTEXT);
      CASE (WM_GETTEXTLENGTH);
      CASE (WM_PAINT);
      CASE (WM_CLOSE);
      CASE (WM_QUERYENDSESSION);
      CASE (WM_QUERYOPEN);
      CASE (WM_ENDSESSION);
      CASE (WM_QUIT);
      CASE (WM_ERASEBKGND);
      CASE (WM_SYSCOLORCHANGE);
      CASE (WM_SHOWWINDOW);
      CASE (WM_WININICHANGE);
      CASE (WM_DEVMODECHANGE);
      CASE (WM_ACTIVATEAPP);
      CASE (WM_FONTCHANGE);
      CASE (WM_TIMECHANGE);
      CASE (WM_CANCELMODE);
      CASE (WM_SETCURSOR);
      CASE (WM_MOUSEACTIVATE);
      CASE (WM_CHILDACTIVATE);
      CASE (WM_QUEUESYNC);
      CASE (WM_GETMINMAXINFO);
      CASE (WM_PAINTICON);
      CASE (WM_ICONERASEBKGND);
      CASE (WM_NEXTDLGCTL);
      CASE (WM_SPOOLERSTATUS);
      CASE (WM_DRAWITEM);
      CASE (WM_MEASUREITEM);
      CASE (WM_DELETEITEM);
      CASE (WM_VKEYTOITEM);
      CASE (WM_CHARTOITEM);
      CASE (WM_SETFONT);
      CASE (WM_GETFONT);
      CASE (WM_SETHOTKEY);
      CASE (WM_GETHOTKEY);
      CASE (WM_QUERYDRAGICON);
      CASE (WM_COMPAREITEM);
      CASE (WM_GETOBJECT);
      CASE (WM_COMPACTING);
      CASE (WM_WINDOWPOSCHANGING);
      CASE (WM_WINDOWPOSCHANGED);
      CASE (WM_POWER);
      CASE (WM_COPYDATA);
      CASE (WM_CANCELJOURNAL);
      CASE (WM_NOTIFY);
      CASE (WM_INPUTLANGCHANGEREQUEST);
      CASE (WM_INPUTLANGCHANGE);
      CASE (WM_TCARD);
      CASE (WM_HELP);
      CASE (WM_USERCHANGED);
      CASE (WM_NOTIFYFORMAT);
      CASE (WM_CONTEXTMENU);
      CASE (WM_STYLECHANGING);
      CASE (WM_STYLECHANGED);
      CASE (WM_DISPLAYCHANGE);
      CASE (WM_GETICON);
      CASE (WM_SETICON);
      CASE (WM_NCCREATE);
      CASE (WM_NCDESTROY);
      CASE (WM_NCCALCSIZE);
      CASE (WM_NCHITTEST);
      CASE (WM_NCPAINT);
      CASE (WM_NCACTIVATE);
      CASE (WM_GETDLGCODE);
      CASE (WM_SYNCPAINT);
      CASE (WM_NCMOUSEMOVE);
      CASE (WM_NCLBUTTONDOWN);
      CASE (WM_NCLBUTTONUP);
      CASE (WM_NCLBUTTONDBLCLK);
      CASE (WM_NCRBUTTONDOWN);
      CASE (WM_NCRBUTTONUP);
      CASE (WM_NCRBUTTONDBLCLK);
      CASE (WM_NCMBUTTONDOWN);
      CASE (WM_NCMBUTTONUP);
      CASE (WM_NCMBUTTONDBLCLK);
      CASE (WM_NCXBUTTONDOWN);
      CASE (WM_NCXBUTTONUP);
      CASE (WM_NCXBUTTONDBLCLK);
      CASE (WM_KEYDOWN);
      CASE (WM_KEYUP);
      CASE (WM_CHAR);
      CASE (WM_DEADCHAR);
      CASE (WM_SYSKEYDOWN);
      CASE (WM_SYSKEYUP);
      CASE (WM_SYSCHAR);
      CASE (WM_SYSDEADCHAR);
      CASE (WM_KEYLAST);
      CASE (WM_IME_STARTCOMPOSITION);
      CASE (WM_IME_ENDCOMPOSITION);
      CASE (WM_IME_COMPOSITION);
      CASE (WM_INITDIALOG);
      CASE (WM_COMMAND);
      CASE (WM_SYSCOMMAND);
      CASE (WM_TIMER);
      CASE (WM_HSCROLL);
      CASE (WM_VSCROLL);
      CASE (WM_INITMENU);
      CASE (WM_INITMENUPOPUP);
      CASE (WM_MENUSELECT);
      CASE (WM_MENUCHAR);
      CASE (WM_ENTERIDLE);
      CASE (WM_MENURBUTTONUP);
      CASE (WM_MENUDRAG);
      CASE (WM_MENUGETOBJECT);
      CASE (WM_UNINITMENUPOPUP);
      CASE (WM_MENUCOMMAND);
      CASE (WM_CHANGEUISTATE);
      CASE (WM_UPDATEUISTATE);
      CASE (WM_QUERYUISTATE);
      CASE (WM_CTLCOLORMSGBOX);
      CASE (WM_CTLCOLOREDIT);
      CASE (WM_CTLCOLORLISTBOX);
      CASE (WM_CTLCOLORBTN);
      CASE (WM_CTLCOLORDLG);
      CASE (WM_CTLCOLORSCROLLBAR);
      CASE (WM_CTLCOLORSTATIC);
      CASE (WM_MOUSEMOVE);
      CASE (WM_LBUTTONDOWN);
      CASE (WM_LBUTTONUP);
      CASE (WM_LBUTTONDBLCLK);
      CASE (WM_RBUTTONDOWN);
      CASE (WM_RBUTTONUP);
      CASE (WM_RBUTTONDBLCLK);
      CASE (WM_MBUTTONDOWN);
      CASE (WM_MBUTTONUP);
      CASE (WM_MBUTTONDBLCLK);
      CASE (WM_MOUSEWHEEL);
      CASE (WM_XBUTTONDOWN);
      CASE (WM_XBUTTONUP);
      CASE (WM_XBUTTONDBLCLK);
      CASE (WM_PARENTNOTIFY);
      CASE (WM_ENTERMENULOOP);
      CASE (WM_EXITMENULOOP);
      CASE (WM_NEXTMENU);
      CASE (WM_SIZING);
      CASE (WM_CAPTURECHANGED);
      CASE (WM_MOVING);
      CASE (WM_POWERBROADCAST);
      CASE (WM_DEVICECHANGE);
      CASE (WM_MDICREATE);
      CASE (WM_MDIDESTROY);
      CASE (WM_MDIACTIVATE);
      CASE (WM_MDIRESTORE);
      CASE (WM_MDINEXT);
      CASE (WM_MDIMAXIMIZE);
      CASE (WM_MDITILE);
      CASE (WM_MDICASCADE);
      CASE (WM_MDIICONARRANGE);
      CASE (WM_MDIGETACTIVE);
      CASE (WM_MDISETMENU);
      CASE (WM_ENTERSIZEMOVE);
      CASE (WM_EXITSIZEMOVE);
      CASE (WM_DROPFILES);
      CASE (WM_MDIREFRESHMENU);
      CASE (WM_IME_SETCONTEXT);
      CASE (WM_IME_NOTIFY);
      CASE (WM_IME_CONTROL);
      CASE (WM_IME_COMPOSITIONFULL);
      CASE (WM_IME_SELECT);
      CASE (WM_IME_CHAR);
      CASE (WM_IME_REQUEST);
      CASE (WM_IME_KEYDOWN);
      CASE (WM_IME_KEYUP);
      CASE (WM_MOUSEHOVER);
      CASE (WM_MOUSELEAVE);
      CASE (WM_NCMOUSEHOVER);
      CASE (WM_NCMOUSELEAVE);
      CASE (WM_CUT);
      CASE (WM_COPY);
      CASE (WM_PASTE);
      CASE (WM_CLEAR);
      CASE (WM_UNDO);
      CASE (WM_RENDERFORMAT);
      CASE (WM_RENDERALLFORMATS);
      CASE (WM_DESTROYCLIPBOARD);
      CASE (WM_DRAWCLIPBOARD);
      CASE (WM_PAINTCLIPBOARD);
      CASE (WM_VSCROLLCLIPBOARD);
      CASE (WM_SIZECLIPBOARD);
      CASE (WM_ASKCBFORMATNAME);
      CASE (WM_CHANGECBCHAIN);
      CASE (WM_HSCROLLCLIPBOARD);
      CASE (WM_QUERYNEWPALETTE);
      CASE (WM_PALETTEISCHANGING);
      CASE (WM_PALETTECHANGED);
      CASE (WM_HOTKEY);
      CASE (WM_PRINT);
      CASE (WM_PRINTCLIENT);
      CASE (WM_APPCOMMAND);
      CASE (WM_HANDHELDFIRST);
      CASE (WM_HANDHELDLAST);
      CASE (WM_AFXFIRST);
      CASE (WM_AFXLAST);
      CASE (WM_PENWINFIRST);
      CASE (WM_PENWINLAST);
      CASE (WM_APP);
#undef CASE
    default:
      if (msg >= WM_HANDHELDFIRST && msg <= WM_HANDHELDLAST)
	sprintf (bfr, "WM_HANDHELDFIRST+%d", msg - WM_HANDHELDFIRST);
      else if (msg >= WM_AFXFIRST && msg <= WM_AFXLAST)
	sprintf (bfr, "WM_AFXFIRST+%d", msg - WM_AFXFIRST);
      else if (msg >= WM_PENWINFIRST && msg <= WM_PENWINLAST)
	sprintf (bfr, "WM_PENWINFIRST+%d", msg - WM_PENWINFIRST);
      else if (msg >= WM_USER && msg <= 0x7FFF)
	sprintf (bfr, "WM_USER+%d", msg - WM_USER);
      else if (msg >= 0xC000 && msg <= 0xFFFF)
	sprintf (bfr, "reg-%#x", msg);
      else
	sprintf (bfr, "unk-%#x", msg);
      return bfr;
    }
  g_assert_not_reached ();
}
      
#endif /* G_ENABLE_DEBUG */
