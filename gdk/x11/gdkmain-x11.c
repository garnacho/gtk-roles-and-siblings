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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"

/* If you don't want to use gdk's signal handlers define this */
/* #define I_NEED_TO_ACTUALLY_DEBUG_MY_PROGRAMS 1 */

#include <X11/Xlocale.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H_ */

#define XLIB_ILLEGAL_ACCESS
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/cursorfont.h>
#include "gdk.h"
#include "gdkprivate.h"
#include "gdkinput.h"
#include "gdki18n.h"
#include "gdkx.h"
#include "gdkkeysyms.h"

#ifndef X_GETTIMEOFDAY
#define X_GETTIMEOFDAY(tv)  gettimeofday (tv, NULL)
#endif /* X_GETTIMEOFDAY */


#define DOUBLE_CLICK_TIME      250
#define TRIPLE_CLICK_TIME      500
#define DOUBLE_CLICK_DIST      5
#define TRIPLE_CLICK_DIST      5


#ifndef NO_FD_SET
#  define SELECT_MASK fd_set
#else
#  ifndef _AIX
typedef long fd_mask;
#  endif
#  if defined(_IBMR2)
#    define SELECT_MASK void
#  else
#    define SELECT_MASK int
#  endif
#endif


typedef struct _GdkInput      GdkInput;
typedef struct _GdkPredicate  GdkPredicate;

struct _GdkInput
{
  gint tag;
  gint source;
  GdkInputCondition condition;
  GdkInputFunction function;
  gpointer data;
  GdkDestroyNotify destroy;
};

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

/* 
 * Private function declarations
 */

static GdkEvent *gdk_event_new		(void);
static gint	 gdk_event_wait		(void);
static gint	 gdk_event_apply_filters (XEvent *xevent,
					  GdkEvent *event,
					  GList *filters);
static gint	 gdk_event_translate	(GdkEvent     *event, 
					 XEvent	      *xevent);
#if 0
static Bool	 gdk_event_get_type	(Display      *display, 
					 XEvent	      *xevent, 
					 XPointer      arg);
#endif
static void	 gdk_synthesize_click	(GdkEvent     *event, 
					 gint	       nclicks);

#ifndef HAVE_XCONVERTCASE
static void	 gdkx_XConvertCase	(KeySym	       symbol,
					 KeySym	      *lower,
					 KeySym	      *upper);
#define XConvertCase gdkx_XConvertCase
#endif

/* 
 * old junk from offix, we might use it though so leave it 
 */
Window	     gdk_get_client_window   (Display	  *dpy, 
				      Window	   win);
#ifdef WE_HAVE_MOTIF_DROPS_DONE
static GdkWindow *  gdk_drop_get_real_window	 (GdkWindow   *w, 
						  guint16     *x,
						  guint16     *y);
#endif
static void	    gdk_exit_func		 (void);
static int	    gdk_x_error			 (Display     *display, 
						  XErrorEvent *error);
static int	    gdk_x_io_error		 (Display     *display);
static RETSIGTYPE   gdk_signal			 (int	       signum);

GdkFilterReturn gdk_wm_protocols_filter (GdkXEvent *xev,
					 GdkEvent  *event,
					 gpointer   data);

/* Private variable declarations
 */
static int gdk_initialized = 0;			    /* 1 if the library is initialized,
						     * 0 otherwise.
						     */
static int connection_number = 0;		    /* The file descriptor number of our
						     *	connection to the X server. This
						     *	is used so that we may determine
						     *	when events are pending by using
						     *	the "select" system call.
						     */


static struct timeval start;			    /* The time at which the library was
						     *	last initialized.
						     */
static struct timeval timer;			    /* Timeout interval to use in the call
						     *	to "select". This is used in
						     *	conjunction with "timerp" to create
						     *	a maximum time to wait for an event
						     *	to arrive.
						     */
static struct timeval *timerp;			    /* The actual timer passed to "select"
						     *	This may be NULL, in which case
						     *	"select" will block until an event
						     *	arrives.
						     */
static guint32 timer_val;			    /* The timeout length as specified by
						     *	the user in milliseconds.
						     */
static GList *inputs;				    /* A list of the input file descriptors
						     *	that we care about. Each list node
						     *	contains a GdkInput struct that describes
						     *	when we are interested in the specified
						     *	file descriptor. That is, when it is
						     *	available for read, write or has an
						     *	exception pending.
						     */
static guint32 button_click_time[2];		    /* The last 2 button click times. Used
						     *	to determine if the latest button click
						     *	is part of a double or triple click.
						     */
static GdkWindow *button_window[2];		    /* The last 2 windows to receive button presses.
						     *	Also used to determine if the latest button
						     *	click is part of a double or triple click.
						     */
static guint button_number[2];			    /* The last 2 buttons to be pressed.
						     */
static GdkWindowPrivate *xgrab_window = NULL;	    /* Window that currently holds the
						     *	x pointer grab
						     */

static GList *client_filters;	                    /* Filters for client messages */

static GList *putback_events = NULL;

static gulong base_id;
static gint autorepeat;

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  {"events",	    GDK_DEBUG_EVENTS},
  {"misc",	    GDK_DEBUG_MISC},
  {"dnd",	    GDK_DEBUG_DND},
  {"color-context", GDK_DEBUG_COLOR_CONTEXT},
  {"xim",	    GDK_DEBUG_XIM}
};

static const int gdk_ndebug_keys = sizeof(gdk_debug_keys)/sizeof(GDebugKey);

#endif /* G_ENABLE_DEBUG */

/*
 *--------------------------------------------------------------
 * gdk_init
 *
 *   Initialize the library for use.
 *
 * Arguments:
 *   "argc" is the number of arguments.
 *   "argv" is an array of strings.
 *
 * Results:
 *   "argc" and "argv" are modified to reflect any arguments
 *   which were not handled. (Such arguments should either
 *   be handled by the application or dismissed).
 *
 * Side effects:
 *   The library is initialized.
 *
 *--------------------------------------------------------------
 */

void
gdk_init (int	 *argc,
	  char ***argv)
{
  XKeyboardState keyboard_state;
  gint synchronize;
  gint i, j, k;
  XClassHint *class_hint;
  gchar **argv_orig = NULL;
  gint argc_orig = 0;
  
  if (gdk_initialized)
    return;
  
  if (argc && argv)
    {
      argc_orig = *argc;
      
      argv_orig = g_malloc ((argc_orig + 1) * sizeof (char*));
      for (i = 0; i < argc_orig; i++)
	argv_orig[i] = g_strdup ((*argv)[i]);
      argv_orig[argc_orig] = NULL;
    }
  
  X_GETTIMEOFDAY (&start);
  
#ifndef I_NEED_TO_ACTUALLY_DEBUG_MY_PROGRAMS
  signal (SIGHUP, gdk_signal);
  signal (SIGINT, gdk_signal);
  signal (SIGQUIT, gdk_signal);
  signal (SIGBUS, gdk_signal);
  signal (SIGSEGV, gdk_signal);
  signal (SIGPIPE, gdk_signal);
  signal (SIGTERM, gdk_signal);
#endif
  
  gdk_display_name = NULL;
  
  XSetErrorHandler (gdk_x_error);
  XSetIOErrorHandler (gdk_x_io_error);
  
  synchronize = FALSE;
  
#ifdef G_ENABLE_DEBUG
  {
    gchar *debug_string = getenv("GDK_DEBUG");
    if (debug_string != NULL)
      gdk_debug_flags = g_parse_debug_string (debug_string,
					      gdk_debug_keys,
					      gdk_ndebug_keys);
  }
#endif	/* G_ENABLE_DEBUG */
  
  if (argc && argv)
    {
      if (*argc > 0)
	{
	  gchar *d;
	  
	  d = strrchr((*argv)[0],'/');
	  if (d != NULL)
	    g_set_prgname (d + 1);
	  else
	    g_set_prgname ((*argv)[0]);
	}
      
      for (i = 1; i < *argc;)
	{
#ifdef G_ENABLE_DEBUG	  
	  if ((strcmp ("--gdk-debug", (*argv)[i]) == 0) ||
	      (strncmp ("--gdk-debug=", (*argv)[i], 12) == 0))
	    {
	      gchar *equal_pos = strchr ((*argv)[i], '=');
	      
	      if (equal_pos != NULL)
		{
		  gdk_debug_flags |= g_parse_debug_string (equal_pos+1,
							   gdk_debug_keys,
							   gdk_ndebug_keys);
		}
	      else if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  gdk_debug_flags |= g_parse_debug_string ((*argv)[i+1],
							   gdk_debug_keys,
							   gdk_ndebug_keys);
		  (*argv)[i] = NULL;
		  i += 1;
		}
	      (*argv)[i] = NULL;
	    }
	  else if ((strcmp ("--gdk-no-debug", (*argv)[i]) == 0) ||
		   (strncmp ("--gdk-no-debug=", (*argv)[i], 15) == 0))
	    {
	      gchar *equal_pos = strchr ((*argv)[i], '=');
	      
	      if (equal_pos != NULL)
		{
		  gdk_debug_flags &= ~g_parse_debug_string (equal_pos+1,
							    gdk_debug_keys,
							    gdk_ndebug_keys);
		}
	      else if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  gdk_debug_flags &= ~g_parse_debug_string ((*argv)[i+1],
							    gdk_debug_keys,
							    gdk_ndebug_keys);
		  (*argv)[i] = NULL;
		  i += 1;
		}
	      (*argv)[i] = NULL;
	    }
	  else 
#endif /* G_ENABLE_DEBUG */
	    if (strcmp ("--display", (*argv)[i]) == 0)
	      {
		(*argv)[i] = NULL;
		
		if ((i + 1) < *argc && (*argv)[i + 1])
		  {
		    gdk_display_name = g_strdup ((*argv)[i + 1]);
		    (*argv)[i + 1] = NULL;
		    i += 1;
		  }
	      }
	    else if (strcmp ("--sync", (*argv)[i]) == 0)
	      {
		(*argv)[i] = NULL;
		synchronize = TRUE;
	      }
	    else if (strcmp ("--no-xshm", (*argv)[i]) == 0)
	      {
		(*argv)[i] = NULL;
		gdk_use_xshm = FALSE;
	      }
	    else if (strcmp ("--name", (*argv)[i]) == 0)
	      {
		if ((i + 1) < *argc && (*argv)[i + 1])
		  {
		    (*argv)[i++] = NULL;
		    g_set_prgname ((*argv)[i]);
		    (*argv)[i] = NULL;
		  }
	      }
	    else if (strcmp ("--class", (*argv)[i]) == 0)
	      {
		if ((i + 1) < *argc && (*argv)[i + 1])
		  {
		    (*argv)[i++] = NULL;
		    gdk_progclass = (*argv)[i];
		    (*argv)[i] = NULL;
		  }
	      }
#ifdef XINPUT_GXI
	    else if (strcmp ("--gxid_host", (*argv)[i]) == 0)
	      {
		if ((i + 1) < *argc && (*argv)[i + 1])
		  {
		    (*argv)[i++] = NULL;
		    gdk_input_gxid_host = ((*argv)[i]);
		    (*argv)[i] = NULL;
		  }
	      }
	    else if (strcmp ("--gxid_port", (*argv)[i]) == 0)
	      {
		if ((i + 1) < *argc && (*argv)[i + 1])
		  {
		    (*argv)[i++] = NULL;
		    gdk_input_gxid_port = atoi ((*argv)[i]);
		    (*argv)[i] = NULL;
		  }
	      }
#endif
#ifdef USE_XIM
	    else if (strcmp ("--xim-preedit", (*argv)[i]) == 0)
	      {
		if ((i + 1) < *argc && (*argv)[i + 1])
		  {
		    (*argv)[i++] = NULL;
		    if (strcmp ("none", (*argv)[i]) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_NONE);
		    else if (strcmp ("nothing", (*argv)[i]) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_NOTHING);
		    else if (strcmp ("area", (*argv)[i]) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_AREA);
		    else if (strcmp ("position", (*argv)[i]) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_POSITION);
		    else if (strcmp ("callbacks", (*argv)[i]) == 0)
		      gdk_im_set_best_style (GDK_IM_PREEDIT_CALLBACKS);
		  }
	      }
	    else if (strcmp ("--xim-status", (*argv)[i]) == 0)
	      {
		if ((i + 1) < *argc && (*argv)[i + 1])
		  {
		    (*argv)[i++] = NULL;
		    if (strcmp ("none", (*argv)[i]) == 0)
		      gdk_im_set_best_style (GDK_IM_STATUS_NONE);
		    else if (strcmp ("nothing", (*argv)[i]) == 0)
		      gdk_im_set_best_style (GDK_IM_STATUS_NOTHING);
		    else if (strcmp ("area", (*argv)[i]) == 0)
		      gdk_im_set_best_style (GDK_IM_STATUS_AREA);
		    else if (strcmp ("callbacks", (*argv)[i]) == 0)
		      gdk_im_set_best_style (GDK_IM_STATUS_CALLBACKS);
		  }
	      }
#endif
	  
	  i += 1;
	}
      
      for (i = 1; i < *argc; i++)
	{
	  for (k = i; k < *argc; k++)
	    if ((*argv)[k] != NULL)
	      break;
	  
	  if (k > i)
	    {
	      k -= i;
	      for (j = i + k; j < *argc; j++)
		(*argv)[j-k] = (*argv)[j];
	      *argc -= k;
	    }
	}
    }
  else
    {
      g_set_prgname ("<unknown>");
    }
  
  GDK_NOTE (MISC, g_message ("progname: \"%s\"", g_get_prgname ()));
  
  gdk_display = XOpenDisplay (gdk_display_name);
  if (!gdk_display)
    {
      g_warning ("cannot open display: %s", XDisplayName (gdk_display_name));
      exit(1);
    }
  
  /* This is really crappy. We have to look into the display structure
   *  to find the base resource id. This is only needed for recording
   *  and playback of events.
   */
  /* base_id = RESOURCE_BASE; */
  base_id = 0;
  GDK_NOTE (EVENTS, g_message ("base id: %lu", base_id));
  
  connection_number = ConnectionNumber (gdk_display);
  GDK_NOTE (MISC,
	    g_message ("connection number: %d", connection_number));
  
  if (synchronize)
    XSynchronize (gdk_display, True);
  
  gdk_screen = DefaultScreen (gdk_display);
  gdk_root_window = RootWindow (gdk_display, gdk_screen);
  
  gdk_leader_window = XCreateSimpleWindow(gdk_display, gdk_root_window,
					  10, 10, 10, 10, 0, 0 , 0);
  class_hint = XAllocClassHint();
  class_hint->res_name = g_get_prgname ();
  if (gdk_progclass == NULL)
    {
      gdk_progclass = g_strdup (g_get_prgname ());
      gdk_progclass[0] = toupper (gdk_progclass[0]);
    }
  class_hint->res_class = gdk_progclass;
  XSetClassHint(gdk_display, gdk_leader_window, class_hint);
  XSetCommand(gdk_display, gdk_leader_window, argv_orig, argc_orig);
  XFree (class_hint);
  
  for (i = 0; i < argc_orig; i++)
    g_free(argv_orig[i]);
  g_free(argv_orig);
  
  gdk_wm_delete_window = XInternAtom (gdk_display, "WM_DELETE_WINDOW", True);
  gdk_wm_take_focus = XInternAtom (gdk_display, "WM_TAKE_FOCUS", True);
  gdk_wm_protocols = XInternAtom (gdk_display, "WM_PROTOCOLS", True);
  gdk_wm_window_protocols[0] = gdk_wm_delete_window;
  gdk_wm_window_protocols[1] = gdk_wm_take_focus;
  gdk_selection_property = XInternAtom (gdk_display, "GDK_SELECTION", False);
  
  XGetKeyboardControl (gdk_display, &keyboard_state);
  autorepeat = keyboard_state.global_auto_repeat;
  
  timer.tv_sec = 0;
  timer.tv_usec = 0;
  timerp = NULL;
  
  button_click_time[0] = 0;
  button_click_time[1] = 0;
  button_window[0] = NULL;
  button_window[1] = NULL;
  button_number[0] = -1;
  button_number[1] = -1;
  
  g_atexit (gdk_exit_func);
  
  gdk_visual_init ();
  gdk_window_init ();
  gdk_image_init ();
  gdk_input_init ();
  gdk_dnd_init ();

  gdk_add_client_message_filter (gdk_wm_protocols, 
				 gdk_wm_protocols_filter, NULL);
  
#ifdef USE_XIM
  gdk_im_open ();
#endif
  
  gdk_initialized = 1;
}

/*
 *--------------------------------------------------------------
 * gdk_exit
 *
 *   Restores the library to an un-itialized state and exits
 *   the program using the "exit" system call.
 *
 * Arguments:
 *   "errorcode" is the error value to pass to "exit".
 *
 * Results:
 *   Allocated structures are freed and the program exits
 *   cleanly.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_exit (int errorcode)
{
  /* de-initialisation is done by the gdk_exit_funct(),
     no need to do this here (Alex J.) */
  exit (errorcode);
}

/*
 *--------------------------------------------------------------
 * gdk_set_locale
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gchar*
gdk_set_locale (void)
{
  if (!setlocale (LC_ALL,""))
    g_message ("locale not supported by C library");
  
  if (!XSupportsLocale ())
    {
      g_message ("locale not supported by Xlib, locale set to C");
      setlocale (LC_ALL, "C");
    }
  
  if (!XSetLocaleModifiers (""))
    {
      g_message ("can not set locale modifiers");
    }
  
  return setlocale (LC_ALL,NULL);
}

/*
 *--------------------------------------------------------------
 * gdk_events_pending
 *
 *   Returns the number of events pending on the queue.
 *   These events have already been read from the server
 *   connection.
 *
 * Arguments:
 *
 * Results:
 *   Returns the number of events on XLib's event queue.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_events_pending (void)
{
  gint result;
  GList *tmp_list;
  
  result = XPending (gdk_display);
  
  tmp_list = putback_events;
  while (tmp_list)
    {
      result++;
      tmp_list = tmp_list->next;
    }
  
  return result;
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

static Bool
graphics_expose_predicate  (Display  *display,
			    XEvent   *xevent,
			    XPointer  arg)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)arg;
  
  g_return_val_if_fail (private != NULL, False);
  
  if ((xevent->xany.window == private->xwindow) &&
      ((xevent->xany.type == GraphicsExpose) ||
       (xevent->xany.type == NoExpose)))
    return True;
  else
    return False;
}

GdkEvent *
gdk_event_get_graphics_expose (GdkWindow *window)
{
  XEvent xevent;
  GdkEvent *event;
  
  g_return_val_if_fail (window != NULL, NULL);
  
  XIfEvent (gdk_display, &xevent, graphics_expose_predicate, (XPointer)window);
  
  if (xevent.xany.type == GraphicsExpose)
    {
      event = gdk_event_new ();
      
      if (gdk_event_translate (event, &xevent))
	return event;
      else
	gdk_event_free (event);
    }
  
  return NULL;	
}

/************************
 * Exposure compression *
 ************************/

/*
 * The following implements simple exposure compression. It is
 * modelled after the way Xt does exposure compression - in
 * particular compress_expose = XtExposeCompressMultiple.
 * It compress consecutive sequences of exposure events,
 * but not sequences that cross other events. (This is because
 * if it crosses a ConfigureNotify, we could screw up and
 * mistakenly compress the exposures generated for the new
 * size - could we just check for ConfigureNotify?)
 *
 * Xt compresses to a region / bounding rectangle, we compress
 * to two rectangles, and try find the two rectangles of minimal
 * area for this - this is supposed to handle the typical
 * L-shaped regions generated by OpaqueMove.
 */

/* Given three rectangles, find the two rectangles that cover
 * them with the smallest area.
 */
static void
gdk_add_rect_to_rects (GdkRectangle *rect1,
		       GdkRectangle *rect2, 
		       GdkRectangle *new_rect)
{
  GdkRectangle t1, t2, t3;
  gint size1, size2, size3;

  gdk_rectangle_union (rect1, rect2, &t1);
  gdk_rectangle_union (rect1, new_rect, &t2);
  gdk_rectangle_union (rect2, new_rect, &t3);

  size1 = t1.width * t1.height + new_rect->width * new_rect->height;
  size2 = t2.width * t2.height + rect2->width * rect2->height;
  size3 = t1.width * t1.height + rect1->width * rect1->height;

  if (size1 < size2)
    {
      if (size1 < size3)
	{
	  *rect1 = t1;
	  *rect2 = *new_rect;
	}
      else
	*rect2 = t3;
    }
  else
    {
      if (size2 < size3)
	*rect1 = t2;
      else
	*rect2 = t3;
    }
}

typedef struct _GdkExposeInfo GdkExposeInfo;

struct _GdkExposeInfo {
  Window window;
  gboolean seen_nonmatching;
};

Bool
expose_predicate (Display *display, XEvent *xevent, XPointer arg)
{
  GdkExposeInfo *info = (GdkExposeInfo *)arg;

  if (xevent->xany.type != Expose)
    {
      info->seen_nonmatching = TRUE;
    }

  if (info->seen_nonmatching || (xevent->xany.window != info->window))
    return FALSE;
  else
    return TRUE;
}

void
gdk_compress_exposures (XEvent *xevent, GdkWindow *window)
{
  gint nrects = 1;
  gint count = 0;
  GdkRectangle rect1;
  GdkRectangle rect2;
  GdkRectangle tmp_rect;
  XEvent tmp_event;
  GdkFilterReturn result;
  GdkExposeInfo info;
  GdkEvent event;

  info.window = xevent->xany.window;
  info.seen_nonmatching = FALSE;
  
  rect1.x = xevent->xexpose.x;
  rect1.y = xevent->xexpose.y;
  rect1.width = xevent->xexpose.width;
  rect1.height = xevent->xexpose.height;

  while (1)
    {
      if (count == 0)
	{
	  if (!XCheckIfEvent (gdk_display, 
			      &tmp_event, 
			      expose_predicate, 
			      (XPointer)&info))
	    break;
	}
      else
	XIfEvent (gdk_display, 
		  &tmp_event, 
		  expose_predicate, 
		  (XPointer)&info);
      
      /* We apply filters here, and if it was filtered, completely
       * ignore the return
       */
      result = gdk_event_apply_filters (xevent, &event,
					window ? 
					  ((GdkWindowPrivate *)window)->filters
					  : gdk_default_filters);
      
      if (result != GDK_FILTER_CONTINUE)
	{
	  if (result == GDK_FILTER_TRANSLATE)
	    gdk_event_put (&event);
	  continue;
	}

      if (nrects == 1)
	{
	  rect2.x = tmp_event.xexpose.x;
	  rect2.y = tmp_event.xexpose.y;
	  rect2.width = tmp_event.xexpose.width;
	  rect2.height = tmp_event.xexpose.height;

	  nrects++;
	}
      else
	{
	  tmp_rect.x = tmp_event.xexpose.x;
	  tmp_rect.y = tmp_event.xexpose.y;
	  tmp_rect.width = tmp_event.xexpose.width;
	  tmp_rect.height = tmp_event.xexpose.height;

	  gdk_add_rect_to_rects (&rect1, &rect2, &tmp_rect);
	}

      count = tmp_event.xexpose.count;
    }

  if (nrects == 2)
    {
      gdk_rectangle_union (&rect1, &rect2, &tmp_rect);

      if ((tmp_rect.width * tmp_rect.height) <
	  2 * (rect1.height * rect1.width +
	       rect2.height * rect2.width))
	{
	  rect1 = tmp_rect;
	  nrects = 1;
	}
    }

  if (nrects == 2)
    {
      event.expose.type = GDK_EXPOSE;
      event.expose.window = window;
      event.expose.area.x = rect2.x;
      event.expose.area.y = rect2.y;
      event.expose.area.width = rect2.width;
      event.expose.area.height = rect2.height;
      event.expose.count = 0;

      gdk_event_put (&event);
    }

  xevent->xexpose.count = nrects - 1;
  xevent->xexpose.x = rect1.x;
  xevent->xexpose.y = rect1.y;
  xevent->xexpose.width = rect1.width;
  xevent->xexpose.height = rect1.height;
}

/*
 *--------------------------------------------------------------
 * gdk_event_get
 *
 *   Gets the next event.
 *
 * Arguments:
 *
 * Results:
 *   If an event was received that we care about, returns 
 *   a pointer to that event, to be freed with gdk_event_free.
 *   Otherwise, returns NULL. This function will also return
 *   before an event is received if the timeout interval
 *   runs out.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

GdkEvent *
gdk_event_get (void)
{
  GdkEvent *event;
  GList *temp_list;
  XEvent xevent;
  
#if 0
  if (pred)
    {
      temp_list = putback_events;
      while (temp_list)
	{
	  temp_event = temp_list->data;
	  
	  if ((* pred) (temp_event, data))
	    {
	      if (event)
		*event = *temp_event;
	      putback_events = g_list_remove_link (putback_events, temp_list);
	      g_list_free (temp_list);
	      return TRUE;
	    }
	  
	  temp_list = temp_list->next;
	}
      
      event_pred.func = pred;
      event_pred.data = data;
      
      if (XCheckIfEvent (gdk_display, &xevent, gdk_event_get_type, (XPointer) & event_pred))
	if (event)
	  return gdk_event_translate (event, &xevent);
    }
  else
#endif
    if (putback_events)
      {
	event = putback_events->data;
	
	temp_list = putback_events;
	putback_events = g_list_remove_link (putback_events, temp_list);
	g_list_free_1 (temp_list);
	
	return event;
      }
  
  /* Wait for an event to occur or the timeout to elapse.
   * If an event occurs "gdk_event_wait" will return TRUE.
   *  If the timeout elapses "gdk_event_wait" will return
   *  FALSE.
   */
  if (gdk_event_wait ())
    {
      /* If we get here we can rest assurred that an event
       *  has occurred. Read it.
       */
#ifdef USE_XIM
      Window w = None;

      XNextEvent (gdk_display, &xevent);
      if (gdk_xim_window)
	switch (xevent.type)
	  {
	  case KeyPress:
	  case KeyRelease:
	  case ButtonPress:
	  case ButtonRelease:
	    w = GDK_WINDOW_XWINDOW (gdk_xim_window);
	    break;
	  }

      if (XFilterEvent (&xevent, w))
	return NULL;
#else
      XNextEvent (gdk_display, &xevent);
#endif
      
      event = gdk_event_new ();
      
      event->any.type = GDK_NOTHING;
      event->any.window = NULL;
      event->any.send_event = FALSE;
      event->any.send_event = xevent.xany.send_event;
      
      if (gdk_event_translate (event, &xevent))
	return event;
      else
	gdk_event_free (event);
    }
  
  return NULL;
}

void
gdk_event_put (GdkEvent *event)
{
  GdkEvent *new_event;
  
  g_return_if_fail (event != NULL);
  
  new_event = gdk_event_copy (event);
  
  putback_events = g_list_prepend (putback_events, new_event);
}

/*
 *--------------------------------------------------------------
 * gdk_event_copy
 *
 *   Copy a event structure into new storage.
 *
 * Arguments:
 *   "event" is the event struct to copy.
 *
 * Results:
 *   A new event structure.  Free it with gdk_event_free.
 *
 * Side effects:
 *   The reference count of the window in the event is increased.
 *
 *--------------------------------------------------------------
 */

static GMemChunk *event_chunk;

static GdkEvent*
gdk_event_new (void)
{
  GdkEvent *new_event;
  
  if (event_chunk == NULL)
    event_chunk = g_mem_chunk_new ("events",
				   sizeof (GdkEvent),
				   4096,
				   G_ALLOC_AND_FREE);
  
  new_event = g_chunk_new (GdkEvent, event_chunk);
  
  return new_event;
}

GdkEvent*
gdk_event_copy (GdkEvent *event)
{
  GdkEvent *new_event;
  
  g_return_val_if_fail (event != NULL, NULL);
  
  new_event = gdk_event_new ();
  
  *new_event = *event;
  gdk_window_ref (new_event->any.window);
  
  switch (event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      new_event->key.string = g_strdup (event->key.string);
      break;
      
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      if (event->crossing.subwindow != NULL)
	gdk_window_ref (event->crossing.subwindow);
      break;
      
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      gdk_drag_context_ref (event->dnd.context);
      break;

      
    default:
      break;
    }
  
  return new_event;
}

/*
 *--------------------------------------------------------------
 * gdk_event_free
 *
 *   Free a event structure obtained from gdk_event_copy.  Do not use
 *   with other event structures.
 *
 * Arguments:
 *   "event" is the event struct to free.
 *
 * Results:
 *
 * Side effects:
 *   The reference count of the window in the event is decreased and
 *   might be freed, too.
 *
 *-------------------------------------------------------------- */

void
gdk_event_free (GdkEvent *event)
{
  g_assert (event_chunk != NULL);
  g_return_if_fail (event != NULL);
  
  if (event->any.window)
    gdk_window_unref (event->any.window);
  
  switch (event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      g_free (event->key.string);
      break;
      
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      if (event->crossing.subwindow != NULL)
	gdk_window_unref (event->crossing.subwindow);
      break;
      
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      gdk_drag_context_unref (event->dnd.context);
      break;

      
    default:
      break;
    }
  
  g_mem_chunk_free (event_chunk, event);
}

/*
 *--------------------------------------------------------------
 * gdk_set_show_events
 *
 *   Turns on/off the showing of events.
 *
 * Arguments:
 *   "show_events" is a boolean describing whether or
 *   not to show the events gdk receives.
 *
 * Results:
 *
 * Side effects:
 *   When "show_events" is TRUE, calls to "gdk_event_get"
 *   will output debugging informatin regarding the event
 *   received to stdout.
 *
 *--------------------------------------------------------------
 */

/*
 *--------------------------------------------------------------
 * gdk_event_get_time:
 *    Get the timestamp from an event.
 *   arguments:
 *     event:
 *   results:
 *    The event's time stamp, if it has one, otherwise
 *    GDK_CURRENT_TIME.
 *--------------------------------------------------------------
 */

guint32
gdk_event_get_time (GdkEvent *event)
{
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
	return event->motion.time;
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
	return event->button.time;
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
	return event->key.time;
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
	return event->crossing.time;
      case GDK_PROPERTY_NOTIFY:
	return event->property.time;
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
	return event->selection.time;
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
	return event->proximity.time;
      case GDK_DRAG_ENTER:
      case GDK_DRAG_LEAVE:
      case GDK_DRAG_MOTION:
      case GDK_DRAG_STATUS:
      case GDK_DROP_START:
      case GDK_DROP_FINISHED:
	return event->dnd.time;
      default:			/* use current time */
	break;
      }
  
  return GDK_CURRENT_TIME;
}

void
gdk_set_show_events (int show_events)
{
  if (show_events)
    gdk_debug_flags |= GDK_DEBUG_EVENTS;
  else
    gdk_debug_flags &= ~GDK_DEBUG_EVENTS;
}

void
gdk_set_use_xshm (gint use_xshm)
{
  gdk_use_xshm = use_xshm;
}

gint
gdk_get_show_events (void)
{
  return gdk_debug_flags & GDK_DEBUG_EVENTS;
}

gint
gdk_get_use_xshm (void)
{
  return gdk_use_xshm;
}

/*
 *--------------------------------------------------------------
 * gdk_time_get
 *
 *   Get the number of milliseconds since the library was
 *   initialized.
 *
 * Arguments:
 *
 * Results:
 *   The time since the library was initialized is returned.
 *   This time value is accurate to milliseconds even though
 *   a more accurate time down to the microsecond could be
 *   returned.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

guint32
gdk_time_get (void)
{
  struct timeval end;
  struct timeval elapsed;
  guint32 milliseconds;
  
  X_GETTIMEOFDAY (&end);
  
  if (start.tv_usec > end.tv_usec)
    {
      end.tv_usec += 1000000;
      end.tv_sec--;
    }
  elapsed.tv_sec = end.tv_sec - start.tv_sec;
  elapsed.tv_usec = end.tv_usec - start.tv_usec;
  
  milliseconds = (elapsed.tv_sec * 1000) + (elapsed.tv_usec / 1000);
  
  return milliseconds;
}

/*
 *--------------------------------------------------------------
 * gdk_timer_get
 *
 *   Returns the current timer.
 *
 * Arguments:
 *
 * Results:
 *   Returns the current timer interval. This interval is
 *   in units of milliseconds.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

guint32
gdk_timer_get (void)
{
  return timer_val;
}

/*
 *--------------------------------------------------------------
 * gdk_timer_set
 *
 *   Sets the timer interval.
 *
 * Arguments:
 *   "milliseconds" is the new value for the timer.
 *
 * Results:
 *
 * Side effects:
 *   Calls to "gdk_event_get" will last for a maximum
 *   of time of "milliseconds". However, a value of 0
 *   milliseconds will cause "gdk_event_get" to block
 *   indefinately until an event is received.
 *
 *--------------------------------------------------------------
 */

void
gdk_timer_set (guint32 milliseconds)
{
  timer_val = milliseconds;
  timer.tv_sec = milliseconds / 1000;
  timer.tv_usec = (milliseconds % 1000) * 1000;
  
}

void
gdk_timer_enable (void)
{
  timerp = &timer;
}

void
gdk_timer_disable (void)
{
  timerp = NULL;
}

gint
gdk_input_add_full (gint	      source,
		    GdkInputCondition condition,
		    GdkInputFunction  function,
		    gpointer	      data,
		    GdkDestroyNotify  destroy)
{
  static gint next_tag = 1;
  GList *list;
  GdkInput *input;
  gint tag;
  
  tag = 0;
  list = inputs;
  
  while (list)
    {
      input = list->data;
      list = list->next;
      
      if ((input->source == source) && (input->condition == condition))
	{
	  if (input->destroy)
	    (input->destroy) (input->data);
	  input->function = function;
	  input->data = data;
	  input->destroy = destroy;
	  tag = input->tag;
	}
    }
  
  if (!tag)
    {
      input = g_new (GdkInput, 1);
      input->tag = next_tag++;
      input->source = source;
      input->condition = condition;
      input->function = function;
      input->data = data;
      input->destroy = destroy;
      tag = input->tag;
      
      inputs = g_list_prepend (inputs, input);
    }
  
  return tag;
}

gint
gdk_input_add (gint		 source,
	       GdkInputCondition condition,
	       GdkInputFunction	 function,
	       gpointer		 data)
{
  return gdk_input_add_interp (source, condition, function, data, NULL);
}

void
gdk_input_remove (gint tag)
{
  GList *list;
  GdkInput *input;
  
  list = inputs;
  while (list)
    {
      input = list->data;
      
      if (input->tag == tag)
	{
	  if (input->destroy)
	    (input->destroy) (input->data);
	  
	  input->tag = 0;	      /* do not free it here */
	  input->condition = 0;	      /* it's done in gdk_event_wait */
	  
	  break;
	}
      
      list = list->next;
    }
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
		  gint		  owner_events,
		  GdkEventMask	  event_mask,
		  GdkWindow *	  confine_to,
		  GdkCursor *	  cursor,
		  guint32	  time)
{
  /*  From gdkwindow.c	*/
  extern const int nevent_masks;
  extern const int event_mask_table[];
  
  gint return_val;
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *confine_to_private;
  GdkCursorPrivate *cursor_private;
  guint xevent_mask;
  Window xwindow;
  Window xconfine_to;
  Cursor xcursor;
  int i;
  
  g_return_val_if_fail (window != NULL, 0);
  
  window_private = (GdkWindowPrivate*) window;
  confine_to_private = (GdkWindowPrivate*) confine_to;
  cursor_private = (GdkCursorPrivate*) cursor;
  
  xwindow = window_private->xwindow;
  
  if (!confine_to || confine_to_private->destroyed)
    xconfine_to = None;
  else
    xconfine_to = confine_to_private->xwindow;
  
  if (!cursor)
    xcursor = None;
  else
    xcursor = cursor_private->xcursor;
  
  
  xevent_mask = 0;
  for (i = 0; i < nevent_masks; i++)
    {
      if (event_mask & (1 << (i + 1)))
	xevent_mask |= event_mask_table[i];
    }
  
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
      if (!window_private->destroyed)
	return_val = XGrabPointer (window_private->xdisplay,
				   xwindow,
				   owner_events,
				   xevent_mask,
				   GrabModeAsync, GrabModeAsync,
				   xconfine_to,
				   xcursor,
				   time);
      else
	return_val = AlreadyGrabbed;
    }
  
  if (return_val == GrabSuccess)
    xgrab_window = window_private;
  
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
  
  XUngrabPointer (gdk_display, time);
  xgrab_window = NULL;
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

gint
gdk_pointer_is_grabbed (void)
{
  return xgrab_window != NULL;
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
		   gint		   owner_events,
		   guint32	   time)
{
  GdkWindowPrivate *window_private;
  Window xwindow;
  
  g_return_val_if_fail (window != NULL, 0);
  
  window_private = (GdkWindowPrivate*) window;
  xwindow = window_private->xwindow;
  
  if (!window_private->destroyed)
    return XGrabKeyboard (window_private->xdisplay,
			  xwindow,
			  owner_events,
			  GrabModeAsync, GrabModeAsync,
			  time);
  else
    return AlreadyGrabbed;
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
  XUngrabKeyboard (gdk_display, time);
}

/*
 *--------------------------------------------------------------
 * gdk_screen_width
 *
 *   Return the width of the screen.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_width (void)
{
  gint return_val;
  
  return_val = DisplayWidth (gdk_display, gdk_screen);
  
  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_height (void)
{
  gint return_val;
  
  return_val = DisplayHeight (gdk_display, gdk_screen);
  
  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_width_mm
 *
 *   Return the width of the screen in millimeters.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_width_mm (void)
{
  gint return_val;
  
  return_val = DisplayWidthMM (gdk_display, gdk_screen);
  
  return return_val;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen in millimeters.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_height_mm (void)
{
  gint return_val;
  
  return_val = DisplayHeightMM (gdk_display, gdk_screen);
  
  return return_val;
}

void
gdk_key_repeat_disable (void)
{
  XAutoRepeatOff (gdk_display);
}

void
gdk_key_repeat_restore (void)
{
  if (autorepeat)
    XAutoRepeatOn (gdk_display);
  else
    XAutoRepeatOff (gdk_display);
}


/*
 *--------------------------------------------------------------
 * gdk_flush
 *
 *   Flushes the Xlib output buffer and then waits
 *   until all requests have been received and processed
 *   by the X server. The only real use for this function
 *   is in dealing with XShm.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void gdk_flush (void)
{
  XSync (gdk_display, False);
}


void
gdk_beep (void)
{
  XBell(gdk_display, 100);
}


/*
 *--------------------------------------------------------------
 * gdk_event_wait
 *
 *   Waits until an event occurs or the timer runs out.
 *
 * Arguments:
 *
 * Results:
 *   Returns TRUE if an event is ready to be read and FALSE
 *   if the timer ran out.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static gint
gdk_event_wait (void)
{
  GList *list;
  GList *temp_list;
  GdkInput *input;
  GdkInputCondition condition;
  SELECT_MASK readfds;
  SELECT_MASK writefds;
  SELECT_MASK exceptfds;
  int max_input;
  int nfd;
  
  /* If there are no events pending we will wait for an event.
   * The time we wait is dependant on the "timer". If no timer
   *  has been specified then we'll block until an event arrives.
   *  If a timer has been specified we'll block until an event
   *  arrives or the timer expires. (This is all done using the
   *  "select" system call).
   */
  
  if (XPending (gdk_display) == 0)
    {
      FD_ZERO (&readfds);
      FD_ZERO (&writefds);
      FD_ZERO (&exceptfds);
      
      FD_SET (connection_number, &readfds);
      max_input = connection_number;
      
      list = inputs;
      while (list)
	{
	  input = list->data;
	  
	  if (input->tag) 
	    {
	      if (input->condition & GDK_INPUT_READ)
		FD_SET (input->source, &readfds);
	      if (input->condition & GDK_INPUT_WRITE)
		FD_SET (input->source, &writefds);
	      if (input->condition & GDK_INPUT_EXCEPTION)
		FD_SET (input->source, &exceptfds);
	      
	      max_input = MAX (max_input, input->source);
	      list = list->next;
	    }
	  else	  /* free removed inputs */
	    {
	      temp_list = list;
	      
	      if (list->next)
		list->next->prev = list->prev;
	      if (list->prev)
		list->prev->next = list->next;
	      if (inputs == list)
		inputs = list->next;
	      
	      list = list->next;
	      
	      temp_list->next = NULL;
	      temp_list->prev = NULL;
	      
	      g_free (temp_list->data);
	      g_list_free (temp_list);
	    }
	}
      
#ifdef USE_PTHREADS
      if (gdk_using_threads)
	{
	  gdk_select_waiting = TRUE;
	  
	  FD_SET (gdk_threads_pipe[0], &readfds);
	  max_input = MAX (max_input, gdk_threads_pipe[0]);
	  gdk_threads_leave ();
	}
#endif
      
      nfd = select (max_input+1, &readfds, &writefds, &exceptfds, timerp);
      
#ifdef USE_PTHREADS
      if (gdk_using_threads)
	{
	  gchar c;
	  gdk_threads_enter ();
	  gdk_select_waiting = FALSE;
	  
	  if (FD_ISSET (gdk_threads_pipe[0], &readfds))
	    read (gdk_threads_pipe[0], &c, 1);
	}
#endif
      
      timerp = NULL;
      timer_val = 0;
      
      if (nfd > 0)
	{
	  if (FD_ISSET (connection_number, &readfds))
	    {
	      if (XPending (gdk_display) == 0)
		{
		  if (nfd == 1)
		    {
		      XNoOp (gdk_display);
		      XFlush (gdk_display);
		    }
		  return FALSE;
		}
	      else
		return TRUE;
	    }
	  
	  list = inputs;
	  while (list)
	    {
	      input = list->data;
	      list = list->next;
	      
	      condition = 0;
	      if (FD_ISSET (input->source, &readfds))
		condition |= GDK_INPUT_READ;
	      if (FD_ISSET (input->source, &writefds))
		condition |= GDK_INPUT_WRITE;
	      if (FD_ISSET (input->source, &exceptfds))
		condition |= GDK_INPUT_EXCEPTION;
	      
	      if (condition && input->function)
		(* input->function) (input->data, input->source, condition);
	    }
	}
    }
  else
    return TRUE;
  
  return FALSE;
}

static gint
gdk_event_apply_filters (XEvent *xevent,
			 GdkEvent *event,
			 GList *filters)
{
  GdkEventFilter *filter;
  GList *tmp_list;
  GdkFilterReturn result;
  
  tmp_list = filters;
  
  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      
      result = (*filter->function)(xevent, event, filter->data);
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

static gint
gdk_event_translate (GdkEvent *event,
		     XEvent   *xevent)
{
  
  GdkWindow *window;
  GdkWindowPrivate *window_private;
  static XComposeStatus compose;
  KeySym keysym;
  int charcount;
#ifdef USE_XIM
  static gchar* buf = NULL;
  static gint buf_len= 0;
#else
  char buf[16];
#endif
  gint return_val;
  
  return_val = FALSE;
  
  /* Find the GdkWindow that this event occurred in.
   * 
   * We handle events with window=None
   *  specially - they are generated by XFree86's XInput under
   *  some circumstances.
   */
  
  if ((xevent->xany.window == None) &&
      gdk_input_vtable.window_none_event)
    {
      return_val = gdk_input_vtable.window_none_event (event,xevent);
      
      if (return_val >= 0)	/* was handled */
	return return_val;
      else
	return_val = FALSE;
    }
  
  window = gdk_window_lookup (xevent->xany.window);
  window_private = (GdkWindowPrivate *) window;
  
  if (window != NULL)
    gdk_window_ref (window);
  
  event->any.window = window;
  event->any.send_event = xevent->xany.send_event;
  
  if (window_private && window_private->destroyed)
    {
      if (xevent->type != DestroyNotify)
	return FALSE;
    }
  else
    {
      /* Check for filters for this window */
  
      GdkFilterReturn result;

#ifdef USE_XIM
      if (window == NULL && 
	  xevent->type == KeyPress &&
	  gdk_xim_window && 
	  !((GdkWindowPrivate *) gdk_xim_window)->destroyed)
	{
	  /*
	   * If user presses a key in Preedit or Status window, keypress event
	   * is sometimes sent to these windows. These windows are not managed
	   * by GDK, so we redirect KeyPress event to gdk_xim_window.
	   *
	   * If someone want to use the window whitch is not managed by GDK
	   * and want to get KeyPress event, he/she must register the filter
	   * function to gdk_default_filters to intercept the event.
	   */
	  
	  window = gdk_xim_window;
	  window_private = (GdkWindowPrivate *) window;
	  gdk_window_ref (window);
	  event->any.window = window;
	  
	  GDK_NOTE (XIM,
		    g_message ("KeyPress event is redirected to gdk_xim_window: %#lx",
			       xevent->xany.window));
	}
#endif /* USE_XIM */
      
      result = gdk_event_apply_filters (xevent, event,
					window_private
					?window_private->filters
					:gdk_default_filters);
      
      if (result != GDK_FILTER_CONTINUE)
	return (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
    }

  if (window == NULL)
    g_message ("Got event for unknown window: %#lx\n", xevent->xany.window);

  /* We do a "manual" conversion of the XEvent to a
   *  GdkEvent. The structures are mostly the same so
   *  the conversion is fairly straightforward. We also
   *  optionally print debugging info regarding events
   *  received.
   */

  return_val = TRUE;

  switch (xevent->type)
    {
    case KeyPress:
      /* Lookup the string corresponding to the given keysym.
       */
#ifdef USE_XIM
      if (buf_len == 0) 
	{
	  buf_len = 128;
	  buf = g_new (gchar, buf_len);
	}
      keysym = GDK_VoidSymbol;
      
      if (gdk_xim_ic && gdk_xim_ic->xic)
	{
	  Status status;
	  
	  /* Clear keyval. Depending on status, may not be set */
	  charcount = XmbLookupString(gdk_xim_ic->xic,
				      &xevent->xkey, buf, buf_len-1,
				      &keysym, &status);
	  if (status == XBufferOverflow)
	    {			  /* retry */
	      /* alloc adequate size of buffer */
	      GDK_NOTE (XIM,
			g_message("XIM: overflow (required %i)", charcount));
	      
	      while (buf_len <= charcount)
		buf_len *= 2;
	      buf = (gchar *) g_realloc (buf, buf_len);
	      
	      charcount = XmbLookupString (gdk_xim_ic->xic,
					   &xevent->xkey, buf, buf_len-1,
					   &keysym, &status);
	    }
	  if (status == XLookupNone)
	    {
	      return_val = FALSE;
	      break;
	    }
	}
      else
	charcount = XLookupString (&xevent->xkey, buf, buf_len,
				   &keysym, &compose);
#else
      charcount = XLookupString (&xevent->xkey, buf, 16,
				 &keysym, &compose);
#endif
      event->key.keyval = keysym;
      
      if (charcount > 0 && buf[charcount-1] == '\0')
	charcount --;
      else
	buf[charcount] = '\0';
      
      /* Print debugging info.
       */
#ifdef G_ENABLE_DEBUG
      if (gdk_debug_flags & GDK_DEBUG_EVENTS)
	{
	  g_message ("key press:\twindow: %ld  key: %12s  %d",
		     xevent->xkey.window - base_id,
		     event->key.keyval ? XKeysymToString (event->key.keyval) : "(none)",
		     event->key.keyval);
	  if (charcount > 0)
	    g_message ("\t\tlength: %4d string: \"%s\"",
		       charcount, buf);
	}
#endif /* G_ENABLE_DEBUG */
      
      event->key.type = GDK_KEY_PRESS;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = (GdkModifierType) xevent->xkey.state;
      event->key.string = g_strdup (buf);
      event->key.length = charcount;
      
      break;
      
    case KeyRelease:
      /* Lookup the string corresponding to the given keysym.
       */
#ifdef USE_XIM
      if (buf_len == 0) 
	{
	  buf_len = 128;
	  buf = g_new (gchar, buf_len);
	}
#endif
      keysym = GDK_VoidSymbol;
      charcount = XLookupString (&xevent->xkey, buf, 16,
				 &keysym, &compose);
      event->key.keyval = keysym;      
      
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
		g_message ("key release:\t\twindow: %ld	 key: %12s  %d",
			   xevent->xkey.window - base_id,
			   XKeysymToString (event->key.keyval),
			   event->key.keyval));
      
      event->key.type = GDK_KEY_RELEASE;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = (GdkModifierType) xevent->xkey.state;
      event->key.length = 0;
      event->key.string = NULL;
      
      break;
      
    case ButtonPress:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
		g_message ("button press:\t\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window - base_id,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	{
	  return_val = FALSE;
	  break;
	}
      
      event->button.type = GDK_BUTTON_PRESS;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x;
      event->button.y = xevent->xbutton.y;
      event->button.x_root = (gfloat)xevent->xbutton.x_root;
      event->button.y_root = (gfloat)xevent->xbutton.y_root;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = (GdkModifierType) xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      event->button.source = GDK_SOURCE_MOUSE;
      event->button.deviceid = GDK_CORE_POINTER;
      
      if ((event->button.time < (button_click_time[1] + TRIPLE_CLICK_TIME)) &&
	  (event->button.window == button_window[1]) &&
	  (event->button.button == button_number[1]))
	{
	  gdk_synthesize_click (event, 3);
	  
	  button_click_time[1] = 0;
	  button_click_time[0] = 0;
	  button_window[1] = NULL;
	  button_window[0] = 0;
	  button_number[1] = -1;
	  button_number[0] = -1;
	}
      else if ((event->button.time < (button_click_time[0] + DOUBLE_CLICK_TIME)) &&
	       (event->button.window == button_window[0]) &&
	       (event->button.button == button_number[0]))
	{
	  gdk_synthesize_click (event, 2);
	  
	  button_click_time[1] = button_click_time[0];
	  button_click_time[0] = event->button.time;
	  button_window[1] = button_window[0];
	  button_window[0] = event->button.window;
	  button_number[1] = button_number[0];
	  button_number[0] = event->button.button;
	}
      else
	{
	  button_click_time[1] = 0;
	  button_click_time[0] = event->button.time;
	  button_window[1] = NULL;
	  button_window[0] = event->button.window;
	  button_number[1] = -1;
	  button_number[0] = event->button.button;
	}

      break;
      
    case ButtonRelease:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
		g_message ("button release:\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window - base_id,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	{
	  return_val = FALSE;
	  break;
	}
      
      event->button.type = GDK_BUTTON_RELEASE;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x;
      event->button.y = xevent->xbutton.y;
      event->button.x_root = (gfloat)xevent->xbutton.x_root;
      event->button.y_root = (gfloat)xevent->xbutton.y_root;
      event->button.pressure = 0.5;
      event->button.xtilt = 0;
      event->button.ytilt = 0;
      event->button.state = (GdkModifierType) xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      event->button.source = GDK_SOURCE_MOUSE;
      event->button.deviceid = GDK_CORE_POINTER;
      
      break;
      
    case MotionNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s", 
			   xevent->xmotion.window - base_id,
			   xevent->xmotion.x, xevent->xmotion.y,
			   (xevent->xmotion.is_hint) ? "true" : "false"));
      
      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	{
	  return_val = FALSE;
	  break;
	}
      
      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = xevent->xmotion.x;
      event->motion.y = xevent->xmotion.y;
      event->motion.x_root = (gfloat)xevent->xmotion.x_root;
      event->motion.y_root = (gfloat)xevent->xmotion.y_root;
      event->motion.pressure = 0.5;
      event->motion.xtilt = 0;
      event->motion.ytilt = 0;
      event->motion.state = (GdkModifierType) xevent->xmotion.state;
      event->motion.is_hint = xevent->xmotion.is_hint;
      event->motion.source = GDK_SOURCE_MOUSE;
      event->motion.deviceid = GDK_CORE_POINTER;
      
      break;
      
    case EnterNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld",
			   xevent->xcrossing.window - base_id,
			   xevent->xcrossing.detail,
			   xevent->xcrossing.subwindow - base_id));
      
      /* Tell XInput stuff about it if appropriate */
      if (window_private &&
	  !window_private->destroyed &&
	  (window_private->extension_events != 0) &&
	  gdk_input_vtable.enter_event)
	gdk_input_vtable.enter_event (&xevent->xcrossing, window);
      
      event->crossing.type = GDK_ENTER_NOTIFY;
      event->crossing.window = window;
      
      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
	event->crossing.subwindow = gdk_window_lookup (xevent->xcrossing.subwindow);
      else
	event->crossing.subwindow = NULL;
      
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x;
      event->crossing.y = xevent->xcrossing.y;
      event->crossing.x_root = xevent->xcrossing.x_root;
      event->crossing.y_root = xevent->xcrossing.y_root;
      
      /* Translate the crossing mode into Gdk terms.
       */
      switch (xevent->xcrossing.mode)
	{
	case NotifyNormal:
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  break;
	case NotifyGrab:
	  event->crossing.mode = GDK_CROSSING_GRAB;
	  break;
	case NotifyUngrab:
	  event->crossing.mode = GDK_CROSSING_UNGRAB;
	  break;
	};
      
      /* Translate the crossing detail into Gdk terms.
       */
      switch (xevent->xcrossing.detail)
	{
	case NotifyInferior:
	  event->crossing.detail = GDK_NOTIFY_INFERIOR;
	  break;
	case NotifyAncestor:
	  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	  break;
	case NotifyVirtual:
	  event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	  break;
	case NotifyNonlinear:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
	  break;
	case NotifyNonlinearVirtual:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  break;
	default:
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  break;
	}
      
      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;
  
      break;
      
    case LeaveNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS, 
		g_message ("leave notify:\t\twindow: %ld  detail: %d subwin: %ld",
			   xevent->xcrossing.window - base_id,
			   xevent->xcrossing.detail, xevent->xcrossing.subwindow - base_id));
      
      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = window;
      
      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
	event->crossing.subwindow = gdk_window_lookup (xevent->xcrossing.subwindow);
      else
	event->crossing.subwindow = NULL;
      
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x;
      event->crossing.y = xevent->xcrossing.y;
      event->crossing.x_root = xevent->xcrossing.x_root;
      event->crossing.y_root = xevent->xcrossing.y_root;
      
      /* Translate the crossing mode into Gdk terms.
       */
      switch (xevent->xcrossing.mode)
	{
	case NotifyNormal:
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  break;
	case NotifyGrab:
	  event->crossing.mode = GDK_CROSSING_GRAB;
	  break;
	case NotifyUngrab:
	  event->crossing.mode = GDK_CROSSING_UNGRAB;
	  break;
	};
      
      /* Translate the crossing detail into Gdk terms.
       */
      switch (xevent->xcrossing.detail)
	{
	case NotifyInferior:
	  event->crossing.detail = GDK_NOTIFY_INFERIOR;
	  break;
	case NotifyAncestor:
	  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	  break;
	case NotifyVirtual:
	  event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	  break;
	case NotifyNonlinear:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
	  break;
	case NotifyNonlinearVirtual:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  break;
	default:
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  break;
	}
      
      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;
      
      break;
      
    case FocusIn:
    case FocusOut:
      /* We only care about focus events that indicate that _this_
       * window (not a ancestor or child) got or lost the focus
       */
      switch (xevent->xfocus.detail)
	{
	case NotifyAncestor:
	case NotifyInferior:
	case NotifyNonlinear:
	  /* Print debugging info.
	   */
	  GDK_NOTE (EVENTS,
		    g_message ("focus %s:\t\twindow: %ld",
			       (xevent->xany.type == FocusIn) ? "in" : "out",
			       xevent->xfocus.window - base_id));
	  
	  /* gdk_keyboard_grab() causes following events. These events confuse
	   * the XIM focus, so ignore them.
	   */
	  if (xevent->xfocus.mode == NotifyGrab ||
	      xevent->xfocus.mode == NotifyUngrab)
	    break;

	  event->focus_change.type = GDK_FOCUS_CHANGE;
	  event->focus_change.window = window;
	  event->focus_change.in = (xevent->xany.type == FocusIn);

	  break;
	default:
	  return_val = FALSE;
	}
      break;
      
    case KeymapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("keymap notify"));

      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case Expose:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("expose:\t\twindow: %ld  %d	x,y: %d %d  w,h: %d %d",
			   xevent->xexpose.window - base_id, xevent->xexpose.count,
			   xevent->xexpose.x, xevent->xexpose.y,
			   xevent->xexpose.width, xevent->xexpose.height));
      gdk_compress_exposures (xevent, window);
      
      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = xevent->xexpose.x;
      event->expose.area.y = xevent->xexpose.y;
      event->expose.area.width = xevent->xexpose.width;
      event->expose.area.height = xevent->xexpose.height;
      event->expose.count = xevent->xexpose.count;
      
      break;
      
    case GraphicsExpose:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("graphics expose:\tdrawable: %ld",
			   xevent->xgraphicsexpose.drawable - base_id));
      
      event->expose.type = GDK_EXPOSE;
      event->expose.window = window;
      event->expose.area.x = xevent->xgraphicsexpose.x;
      event->expose.area.y = xevent->xgraphicsexpose.y;
      event->expose.area.width = xevent->xgraphicsexpose.width;
      event->expose.area.height = xevent->xgraphicsexpose.height;
      event->expose.count = xevent->xexpose.count;
      
      break;
      
    case NoExpose:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("no expose:\t\tdrawable: %ld",
			   xevent->xnoexpose.drawable - base_id));
      
      event->no_expose.type = GDK_NO_EXPOSE;
      event->no_expose.window = window;
      
      break;
      
    case VisibilityNotify:
      /* Print debugging info.
       */
#ifdef G_ENABLE_DEBUG
      if (gdk_debug_flags & GDK_DEBUG_EVENTS)
	switch (xevent->xvisibility.state)
	  {
	  case VisibilityFullyObscured:
	    g_message ("visibility notify:\twindow: %ld	 none",
		       xevent->xvisibility.window - base_id);
	    break;
	  case VisibilityPartiallyObscured:
	    g_message ("visibility notify:\twindow: %ld	 partial",
		       xevent->xvisibility.window - base_id);
	    break;
	  case VisibilityUnobscured:
	    g_message ("visibility notify:\twindow: %ld	 full",
		       xevent->xvisibility.window - base_id);
	    break;
	  }
#endif /* G_ENABLE_DEBUG */
      
      event->visibility.type = GDK_VISIBILITY_NOTIFY;
      event->visibility.window = window;
      
      switch (xevent->xvisibility.state)
	{
	case VisibilityFullyObscured:
	  event->visibility.state = GDK_VISIBILITY_FULLY_OBSCURED;
	  break;
	  
	case VisibilityPartiallyObscured:
	  event->visibility.state = GDK_VISIBILITY_PARTIAL;
	  break;
	  
	case VisibilityUnobscured:
	  event->visibility.state = GDK_VISIBILITY_UNOBSCURED;
	  break;
	}
      
      break;
      
    case CreateNotify:
      /* Not currently handled */
      break;
      
    case DestroyNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("destroy notify:\twindow: %ld",
			   xevent->xdestroywindow.window - base_id));
      
      event->any.type = GDK_DESTROY;
      event->any.window = window;
      
      return_val = window_private && !window_private->destroyed;
      
      if(window && window_private->xwindow != GDK_ROOT_WINDOW())
	gdk_window_destroy_notify (window);
      break;
      
    case UnmapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("unmap notify:\t\twindow: %ld",
			   xevent->xmap.window - base_id));
      
      event->any.type = GDK_UNMAP;
      event->any.window = window;
      
      if (xgrab_window == window_private)
	xgrab_window = NULL;
      
      break;
      
    case MapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("map notify:\t\twindow: %ld",
			   xevent->xmap.window - base_id));
      
      event->any.type = GDK_MAP;
      event->any.window = window;
      
      break;
      
    case ReparentNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("reparent notify:\twindow: %ld",
			   xevent->xreparent.window - base_id));

      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case ConfigureNotify:
      /* Print debugging info.
       */
      while ((XPending (gdk_display) > 0) &&
	     XCheckTypedWindowEvent(gdk_display, xevent->xany.window,
				    ConfigureNotify, xevent))
	{
	  GdkFilterReturn result;
	  
	  GDK_NOTE (EVENTS, 
		    g_message ("configure notify discarded:\twindow: %ld",
			       xevent->xconfigure.window - base_id));
	  
	  result = gdk_event_apply_filters (xevent, event,
					    window_private
					    ?window_private->filters
					    :gdk_default_filters);
	  
	  /* If the result is GDK_FILTER_REMOVE, there will be
	   * trouble, but anybody who filtering the Configure events
	   * better know what they are doing
	   */
	  if (result != GDK_FILTER_CONTINUE)
	    {
	      return (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	    }
	  
	  /*XSync (gdk_display, 0);*/
	}
      
      
      GDK_NOTE (EVENTS,
		g_message ("configure notify:\twindow: %ld  x,y: %d %d	w,h: %d %d  b-w: %d  above: %ld	 ovr: %d",
			   xevent->xconfigure.window - base_id,
			   xevent->xconfigure.x,
			   xevent->xconfigure.y,
			   xevent->xconfigure.width,
			   xevent->xconfigure.height,
			   xevent->xconfigure.border_width,
			   xevent->xconfigure.above - base_id,
			   xevent->xconfigure.override_redirect));
      
      if (!window_private->destroyed &&
	  (window_private->extension_events != 0) &&
	  gdk_input_vtable.configure_event)
	gdk_input_vtable.configure_event (&xevent->xconfigure, window);

      if (window_private->window_type == GDK_WINDOW_CHILD)
	return_val = FALSE;
      else
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.width = xevent->xconfigure.width;
	  event->configure.height = xevent->xconfigure.height;
	  
	  if (!xevent->xconfigure.x &&
	      !xevent->xconfigure.y &&
	      !window_private->destroyed)
	    {
	      gint tx = 0;
	      gint ty = 0;
	      Window child_window = 0;
	      
	      if (!XTranslateCoordinates (window_private->xdisplay,
					  window_private->xwindow,
					  gdk_root_window,
					  0, 0,
					  &tx, &ty,
					  &child_window))
		g_warning ("GdkWindow %ld doesn't share root windows display?",
			   window_private->xwindow - base_id);
	      event->configure.x = tx;
	      event->configure.y = ty;
	    }
	  else
	    {
	      event->configure.x = xevent->xconfigure.x;
	      event->configure.y = xevent->xconfigure.y;
	    }
	  window_private->x = event->configure.x;
	  window_private->y = event->configure.y;
	  window_private->width = xevent->xconfigure.width;
	  window_private->height = xevent->xconfigure.height;
	  if (window_private->resize_count > 1)
	    window_private->resize_count -= 1;
	}
      break;
      
    case PropertyNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("property notify:\twindow: %ld",
			   xevent->xproperty.window - base_id));
      
      event->property.type = GDK_PROPERTY_NOTIFY;
      event->property.window = window;
      event->property.atom = xevent->xproperty.atom;
      event->property.time = xevent->xproperty.time;
      event->property.state = xevent->xproperty.state;
      
      break;
      
    case SelectionClear:
      GDK_NOTE (EVENTS,
		g_message ("selection clear:\twindow: %ld",
			   xevent->xproperty.window - base_id));
      
      event->selection.type = GDK_SELECTION_CLEAR;
      event->selection.window = window;
      event->selection.selection = xevent->xselectionclear.selection;
      event->selection.time = xevent->xselectionclear.time;
      
      break;
      
    case SelectionRequest:
      GDK_NOTE (EVENTS,
		g_message ("selection request:\twindow: %ld",
			   xevent->xproperty.window - base_id));
      
      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = xevent->xselectionrequest.selection;
      event->selection.target = xevent->xselectionrequest.target;
      event->selection.property = xevent->xselectionrequest.property;
      event->selection.requestor = xevent->xselectionrequest.requestor;
      event->selection.time = xevent->xselectionrequest.time;
      
      break;
      
    case SelectionNotify:
      GDK_NOTE (EVENTS,
		g_message ("selection notify:\twindow: %ld",
			   xevent->xproperty.window - base_id));
      
      
      event->selection.type = GDK_SELECTION_NOTIFY;
      event->selection.window = window;
      event->selection.selection = xevent->xselection.selection;
      event->selection.target = xevent->xselection.target;
      event->selection.property = xevent->xselection.property;
      event->selection.time = xevent->xselection.time;
      
      break;
      
    case ColormapNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("colormap notify:\twindow: %ld",
			   xevent->xcolormap.window - base_id));
      
      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case ClientMessage:
      {
	GList *tmp_list;
	GdkFilterReturn result = GDK_FILTER_CONTINUE;

	/* Print debugging info.
	 */
	GDK_NOTE (EVENTS,
		  g_message ("client message:\twindow: %ld",
			     xevent->xclient.window - base_id));
	
	tmp_list = client_filters;
	while (tmp_list)
	  {
	    GdkClientFilter *filter = tmp_list->data;
	    if (filter->type == xevent->xclient.message_type)
	      {
		result = (*filter->function) (xevent, event, filter->data);
		break;
	      }
	    
	    tmp_list = tmp_list->next;
	  }

	switch (result)
	  {
	  case GDK_FILTER_REMOVE:
	    return_val = FALSE;
	    break;
	  case GDK_FILTER_TRANSLATE:
	    return_val = TRUE;
	    break;
	  case GDK_FILTER_CONTINUE:
	    /* Send unknown ClientMessage's on to Gtk for it to use */
	    event->client.type = GDK_CLIENT_EVENT;
	    event->client.window = window;
	    event->client.message_type = xevent->xclient.message_type;
	    event->client.data_format = xevent->xclient.format;
	    memcpy(&event->client.data, &xevent->xclient.data,
		   sizeof(event->client.data));
	  }
      }
      
      break;
      
    case MappingNotify:
      /* Print debugging info.
       */
      GDK_NOTE (EVENTS,
		g_message ("mapping notify"));
      
      /* Let XLib know that there is a new keyboard mapping.
       */
      XRefreshKeyboardMapping (&xevent->xmapping);
      return_val = FALSE;
      break;
      
    default:
      /* something else - (e.g., a Xinput event) */
      
      if (window_private &&
	  !window_private->destroyed &&
	  (window_private->extension_events != 0) &&
	  gdk_input_vtable.other_event)
	return_val = gdk_input_vtable.other_event(event, xevent, window);
      else
	return_val = FALSE;
      
      break;
    }
  
  if (return_val)
    {
      if (event->any.window)
	gdk_window_ref (event->any.window);
      if (((event->any.type == GDK_ENTER_NOTIFY) ||
	   (event->any.type == GDK_LEAVE_NOTIFY)) &&
	  (event->crossing.subwindow != NULL))
	gdk_window_ref (event->crossing.subwindow);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
      event->any.type = GDK_NOTHING;
    }
  
  if (window)
    gdk_window_unref (window);
  
  return return_val;
}

GdkFilterReturn
gdk_wm_protocols_filter (GdkXEvent *xev,
		     GdkEvent  *event,
		     gpointer data)
{
  XEvent *xevent = (XEvent *)xev;

  if ((Atom) xevent->xclient.data.l[0] == gdk_wm_delete_window)
    {
  /* The delete window request specifies a window
   *  to delete. We don't actually destroy the
   *  window because "it is only a request". (The
   *  window might contain vital data that the
   *  program does not want destroyed). Instead
   *  the event is passed along to the program,
   *  which should then destroy the window.
   */
      GDK_NOTE (EVENTS,
		g_message ("delete window:\t\twindow: %ld",
			   xevent->xclient.window - base_id));
      
      event->any.type = GDK_DELETE;

      return GDK_FILTER_TRANSLATE;
    }
  else if ((Atom) xevent->xclient.data.l[0] == gdk_wm_take_focus)
    {
    }

  return GDK_FILTER_REMOVE;
}

#if 0
static Bool
gdk_event_get_type (Display  *display,
		    XEvent   *xevent,
		    XPointer  arg)
{
  GdkEvent event;
  GdkPredicate *pred;
  
  if (gdk_event_translate (&event, xevent))
    {
      pred = (GdkPredicate*) arg;
      return (* pred->func) (&event, pred->data);
    }
  
  return FALSE;
}
#endif

static void
gdk_synthesize_click (GdkEvent *event,
		      gint	nclicks)
{
  GdkEvent temp_event;
  
  g_return_if_fail (event != NULL);
  
  temp_event = *event;
  temp_event.type = (nclicks == 2) ? GDK_2BUTTON_PRESS : GDK_3BUTTON_PRESS;
  
  gdk_event_put (&temp_event);
}

/*
 *--------------------------------------------------------------
 * gdk_exit_func
 *
 *   This is the "atexit" function that makes sure the
 *   library gets a chance to cleanup.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *   The library is un-initialized and the program exits.
 *
 *--------------------------------------------------------------
 */

static void
gdk_exit_func (void)
{
  static gboolean in_gdk_exit_func = FALSE;
  
  /* This is to avoid an infinite loop if a program segfaults in
     an atexit() handler (and yes, it does happen, especially if a program
     has trounced over memory too badly for even g_message to work) */
  if (in_gdk_exit_func == TRUE)
    return;
  in_gdk_exit_func = TRUE;
  
  if (gdk_initialized)
    {
#ifdef USE_XIM
      /* cleanup IC */
      gdk_ic_cleanup ();
      /* close IM */
      gdk_im_close ();
#endif
      gdk_image_exit ();
      gdk_input_exit ();
      gdk_key_repeat_restore ();
      
      XCloseDisplay (gdk_display);
      gdk_initialized = 0;
    }
}

/*
 *--------------------------------------------------------------
 * gdk_x_error
 *
 *   The X error handling routine.
 *
 * Arguments:
 *   "display" is the X display the error orignated from.
 *   "error" is the XErrorEvent that we are handling.
 *
 * Results:
 *   Either we were expecting some sort of error to occur,
 *   in which case we set the "gdk_error_code" flag, or this
 *   error was unexpected, in which case we will print an
 *   error message and exit. (Since trying to continue will
 *   most likely simply lead to more errors).
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
gdk_x_error (Display	 *display,
	     XErrorEvent *error)
{
  char buf[64];
  
  if (gdk_error_warnings)
    {
      XGetErrorText (display, error->error_code, buf, 63);
      g_error ("%s\n  serial %ld error_code %d request_code %d minor_code %d\n", 
	       buf, 
	       error->serial, 
	       error->error_code, 
	       error->request_code,
	       error->minor_code);
    }
  
  gdk_error_code = -1;
  return 0;
}

/*
 *--------------------------------------------------------------
 * gdk_x_io_error
 *
 *   The X I/O error handling routine.
 *
 * Arguments:
 *   "display" is the X display the error orignated from.
 *
 * Results:
 *   An X I/O error basically means we lost our connection
 *   to the X server. There is not much we can do to
 *   continue, so simply print an error message and exit.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static int
gdk_x_io_error (Display *display)
{
  g_error ("an x io error occurred");
  return 0;
}

/*
 *--------------------------------------------------------------
 * gdk_signal
 *
 *   The signal handler.
 *
 * Arguments:
 *   "sig_num" is the number of the signal we received.
 *
 * Results:
 *   The signals we catch are all fatal. So we simply build
 *   up a nice little error message and print it and exit.
 *   If in the process of doing so another signal is received
 *   we notice that we are already exiting and simply kill
 *   our process.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

static RETSIGTYPE
gdk_signal (int sig_num)
{
  static int caught_fatal_sig = 0;
  char *sig;
  
  if (caught_fatal_sig)
    kill (getpid (), sig_num);
  caught_fatal_sig = 1;
  
  switch (sig_num)
    {
    case SIGHUP:
      sig = "sighup";
      break;
    case SIGINT:
      sig = "sigint";
      break;
    case SIGQUIT:
      sig = "sigquit";
      break;
    case SIGBUS:
      sig = "sigbus";
      break;
    case SIGSEGV:
      sig = "sigsegv";
      break;
    case SIGPIPE:
      sig = "sigpipe";
      break;
    case SIGTERM:
      sig = "sigterm";
      break;
    default:
      sig = "unknown signal";
      break;
    }
  
  g_message ("\n** ERROR **: %s caught", sig);
#ifdef G_ENABLE_DEBUG
  abort ();
#else /* !G_ENABLE_DEBUG */
  gdk_exit (1);
#endif /* !G_ENABLE_DEBUG */
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message (GdkEvent *event, guint32 xid)
{
  XEvent sev;
  
  g_return_val_if_fail(event != NULL, FALSE);
  
  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = gdk_display;
  sev.xclient.format = event->client.data_format;
  sev.xclient.window = xid;
  memcpy(&sev.xclient.data, &event->client.data, sizeof(sev.xclient.data));
  sev.xclient.message_type = event->client.message_type;
  
  return gdk_send_xevent (xid, False, NoEventMask, &sev);
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message_to_all_recurse (XEvent  *xev, 
					      guint32  xid,
					      guint    level)
{
  static GdkAtom wm_state_atom = GDK_NONE;

  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;
  
  Window *ret_children, ret_root, ret_parent;
  unsigned int ret_nchildren;
  int i;
  
  gboolean send = FALSE;
  gboolean found = FALSE;

  if (!wm_state_atom)
    wm_state_atom = gdk_atom_intern ("WM_STATE", FALSE);

  gdk_error_code = 0;
  XGetWindowProperty (gdk_display, xid, wm_state_atom, 0, 0, False, AnyPropertyType,
		      &type, &format, &nitems, &after, &data);

  if (gdk_error_code)
    {
      gdk_error_code = 0;
      return FALSE;
    }

  if (type)
    {
      send = TRUE;
      XFree (data);
    }
  else
    {
      /* OK, we're all set, now let's find some windows to send this to */
      if (XQueryTree(gdk_display, xid, &ret_root, &ret_parent,
		     &ret_children, &ret_nchildren) != True)
	return FALSE;
      
      if (gdk_error_code)
	return FALSE;

      for(i = 0; i < ret_nchildren; i++)
	if (gdk_event_send_client_message_to_all_recurse(xev, ret_children[i], level + 1))
	  found = TRUE;

      XFree(ret_children);
    }

  if (send || (!found && (level == 1)))
    {
      xev->xclient.window = xid;
      gdk_send_xevent (xid, False, NoEventMask, xev);
    }

  return (send || found);
}

void
gdk_event_send_clientmessage_toall (GdkEvent *event)
{
  XEvent sev;
  gint old_warnings = gdk_error_warnings;

  g_return_if_fail(event != NULL);
  
  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = gdk_display;
  sev.xclient.format = event->client.data_format;
  memcpy(&sev.xclient.data, &event->client.data, sizeof(sev.xclient.data));
  sev.xclient.message_type = event->client.message_type;

  gdk_event_send_client_message_to_all_recurse(&sev, gdk_root_window, 0);

  gdk_error_warnings = old_warnings;
}

gchar *
gdk_get_display(void)
{
  return (gchar *)XDisplayName (gdk_display_name);
}

gint 
gdk_send_xevent (Window window, gboolean propagate, glong event_mask,
		 XEvent *event_send)
{
  Status result;
  gint old_warnings = gdk_error_warnings;
  
  gdk_error_code = 0;
  
  gdk_error_warnings = 0;
  result = XSendEvent (gdk_display, window, propagate, event_mask, event_send);
  XSync (gdk_display, False);
  gdk_error_warnings = old_warnings;
  
  return result && (gdk_error_code != -1);
}

#ifndef HAVE_XCONVERTCASE
/* compatibility function from X11R6.3, since XConvertCase is not
 * supplied by X11R5.
 */
static void
gdkx_XConvertCase (KeySym symbol,
		   KeySym *lower,
		   KeySym *upper)
{
  register KeySym sym = symbol;
  
  g_return_if_fail (lower != NULL);
  g_return_if_fail (upper != NULL);
  
  *lower = sym;
  *upper = sym;
  
  switch (sym >> 8)
    {
#if	defined (GDK_A) && defined (GDK_Ooblique)
    case 0: /* Latin 1 */
      if ((sym >= GDK_A) && (sym <= GDK_Z))
	*lower += (GDK_a - GDK_A);
      else if ((sym >= GDK_a) && (sym <= GDK_z))
	*upper -= (GDK_a - GDK_A);
      else if ((sym >= GDK_Agrave) && (sym <= GDK_Odiaeresis))
	*lower += (GDK_agrave - GDK_Agrave);
      else if ((sym >= GDK_agrave) && (sym <= GDK_odiaeresis))
	*upper -= (GDK_agrave - GDK_Agrave);
      else if ((sym >= GDK_Ooblique) && (sym <= GDK_Thorn))
	*lower += (GDK_oslash - GDK_Ooblique);
      else if ((sym >= GDK_oslash) && (sym <= GDK_thorn))
	*upper -= (GDK_oslash - GDK_Ooblique);
      break;
#endif	/* LATIN1 */
      
#if	defined (GDK_Aogonek) && defined (GDK_tcedilla)
    case 1: /* Latin 2 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym == GDK_Aogonek)
	*lower = GDK_aogonek;
      else if (sym >= GDK_Lstroke && sym <= GDK_Sacute)
	*lower += (GDK_lstroke - GDK_Lstroke);
      else if (sym >= GDK_Scaron && sym <= GDK_Zacute)
	*lower += (GDK_scaron - GDK_Scaron);
      else if (sym >= GDK_Zcaron && sym <= GDK_Zabovedot)
	*lower += (GDK_zcaron - GDK_Zcaron);
      else if (sym == GDK_aogonek)
	*upper = GDK_Aogonek;
      else if (sym >= GDK_lstroke && sym <= GDK_sacute)
	*upper -= (GDK_lstroke - GDK_Lstroke);
      else if (sym >= GDK_scaron && sym <= GDK_zacute)
	*upper -= (GDK_scaron - GDK_Scaron);
      else if (sym >= GDK_zcaron && sym <= GDK_zabovedot)
	*upper -= (GDK_zcaron - GDK_Zcaron);
      else if (sym >= GDK_Racute && sym <= GDK_Tcedilla)
	*lower += (GDK_racute - GDK_Racute);
      else if (sym >= GDK_racute && sym <= GDK_tcedilla)
	*upper -= (GDK_racute - GDK_Racute);
      break;
#endif	/* LATIN2 */
      
#if	defined (GDK_Hstroke) && defined (GDK_Cabovedot)
    case 2: /* Latin 3 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym >= GDK_Hstroke && sym <= GDK_Hcircumflex)
	*lower += (GDK_hstroke - GDK_Hstroke);
      else if (sym >= GDK_Gbreve && sym <= GDK_Jcircumflex)
	*lower += (GDK_gbreve - GDK_Gbreve);
      else if (sym >= GDK_hstroke && sym <= GDK_hcircumflex)
	*upper -= (GDK_hstroke - GDK_Hstroke);
      else if (sym >= GDK_gbreve && sym <= GDK_jcircumflex)
	*upper -= (GDK_gbreve - GDK_Gbreve);
      else if (sym >= GDK_Cabovedot && sym <= GDK_Scircumflex)
	*lower += (GDK_cabovedot - GDK_Cabovedot);
      else if (sym >= GDK_cabovedot && sym <= GDK_scircumflex)
	*upper -= (GDK_cabovedot - GDK_Cabovedot);
      break;
#endif	/* LATIN3 */
      
#if	defined (GDK_Rcedilla) && defined (GDK_Amacron)
    case 3: /* Latin 4 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym >= GDK_Rcedilla && sym <= GDK_Tslash)
	*lower += (GDK_rcedilla - GDK_Rcedilla);
      else if (sym >= GDK_rcedilla && sym <= GDK_tslash)
	*upper -= (GDK_rcedilla - GDK_Rcedilla);
      else if (sym == GDK_ENG)
	*lower = GDK_eng;
      else if (sym == GDK_eng)
	*upper = GDK_ENG;
      else if (sym >= GDK_Amacron && sym <= GDK_Umacron)
	*lower += (GDK_amacron - GDK_Amacron);
      else if (sym >= GDK_amacron && sym <= GDK_umacron)
	*upper -= (GDK_amacron - GDK_Amacron);
      break;
#endif	/* LATIN4 */
      
#if	defined (GDK_Serbian_DJE) && defined (GDK_Cyrillic_yu)
    case 6: /* Cyrillic */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym >= GDK_Serbian_DJE && sym <= GDK_Serbian_DZE)
	*lower -= (GDK_Serbian_DJE - GDK_Serbian_dje);
      else if (sym >= GDK_Serbian_dje && sym <= GDK_Serbian_dze)
	*upper += (GDK_Serbian_DJE - GDK_Serbian_dje);
      else if (sym >= GDK_Cyrillic_YU && sym <= GDK_Cyrillic_HARDSIGN)
	*lower -= (GDK_Cyrillic_YU - GDK_Cyrillic_yu);
      else if (sym >= GDK_Cyrillic_yu && sym <= GDK_Cyrillic_hardsign)
	*upper += (GDK_Cyrillic_YU - GDK_Cyrillic_yu);
      break;
#endif	/* CYRILLIC */
      
#if	defined (GDK_Greek_ALPHAaccent) && defined (GDK_Greek_finalsmallsigma)
    case 7: /* Greek */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (sym >= GDK_Greek_ALPHAaccent && sym <= GDK_Greek_OMEGAaccent)
	*lower += (GDK_Greek_alphaaccent - GDK_Greek_ALPHAaccent);
      else if (sym >= GDK_Greek_alphaaccent && sym <= GDK_Greek_omegaaccent &&
	       sym != GDK_Greek_iotaaccentdieresis &&
	       sym != GDK_Greek_upsilonaccentdieresis)
	*upper -= (GDK_Greek_alphaaccent - GDK_Greek_ALPHAaccent);
      else if (sym >= GDK_Greek_ALPHA && sym <= GDK_Greek_OMEGA)
	*lower += (GDK_Greek_alpha - GDK_Greek_ALPHA);
      else if (sym >= GDK_Greek_alpha && sym <= GDK_Greek_omega &&
	       sym != GDK_Greek_finalsmallsigma)
	*upper -= (GDK_Greek_alpha - GDK_Greek_ALPHA);
      break;
#endif	/* GREEK */
    }
}
#endif

gchar*
gdk_keyval_name (guint	      keyval)
{
  return XKeysymToString (keyval);
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  g_return_val_if_fail (keyval_name != NULL, 0);
  
  return XStringToKeysym (keyval_name);
}

guint
gdk_keyval_to_upper (guint	  keyval)
{
  if (keyval)
    {
      KeySym lower_val = 0;
      KeySym upper_val = 0;
      
      XConvertCase (keyval, &lower_val, &upper_val);
      return upper_val;
    }
  return 0;
}

guint
gdk_keyval_to_lower (guint	  keyval)
{
  if (keyval)
    {
      KeySym lower_val = 0;
      KeySym upper_val = 0;
      
      XConvertCase (keyval, &lower_val, &upper_val);
      return lower_val;
    }
  return 0;
}

gboolean
gdk_keyval_is_upper (guint	  keyval)
{
  if (keyval)
    {
      KeySym lower_val = 0;
      KeySym upper_val = 0;
      
      XConvertCase (keyval, &lower_val, &upper_val);
      return upper_val == keyval;
    }
  return TRUE;
}

gboolean
gdk_keyval_is_lower (guint	  keyval)
{
  if (keyval)
    {
      KeySym lower_val = 0;
      KeySym upper_val = 0;
      
      XConvertCase (keyval, &lower_val, &upper_val);
      return lower_val == keyval;
    }
  return TRUE;
}
