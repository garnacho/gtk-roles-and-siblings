/* GTK - The GIMP Toolkit
 * gdkasync.c: Utility functions using the Xlib asynchronous interfaces
 * Copyright (C) 2003, Red Hat, Inc.
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
/* Portions of code in this file are based on code from Xlib
 */
/*
Copyright 1986, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
#include <X11/Xlibint.h>
#include "gdkasync.h"
#include "gdkx.h"

typedef struct _ChildInfoChildState ChildInfoChildState;
typedef struct _ChildInfoState ChildInfoState;
typedef struct _SendEventState SendEventState;
typedef struct _SetInputFocusState SetInputFocusState;

typedef enum {
  CHILD_INFO_GET_PROPERTY,
  CHILD_INFO_GET_WA,
  CHILD_INFO_GET_GEOMETRY
} ChildInfoReq;

struct _ChildInfoChildState
{
  gulong seq[3];
};

struct _ChildInfoState
{
  gboolean get_wm_state;
  Window *children;
  guint nchildren;
  GdkChildInfoX11 *child_info;
  ChildInfoChildState *child_states;

  guint current_child;
  guint n_children_found;
  gint current_request;
  gboolean have_error;
  gboolean child_has_error;
};

struct _SendEventState
{
  Display *dpy;
  Window window;
  _XAsyncHandler async;
  gulong send_event_req;
  gulong get_input_focus_req;
  gboolean have_error;
  GdkSendXEventCallback callback;
  gpointer data;
};

struct _SetInputFocusState
{
  Display *dpy;
  _XAsyncHandler async;
  gulong set_input_focus_req;
  gulong get_input_focus_req;
};

static Bool
send_event_handler (Display *dpy,
		    xReply  *rep,
		    char    *buf,
		    int      len,
		    XPointer data)
{
  SendEventState *state = (SendEventState *)data;  

  if (dpy->last_request_read == state->send_event_req)
    {
      if (rep->generic.type == X_Error &&
	  rep->error.errorCode == BadWindow)
	{
	  state->have_error = TRUE;
	  return True;
	}
    }
  else if (dpy->last_request_read == state->get_input_focus_req)
    {
      xGetInputFocusReply replbuf;
      xGetInputFocusReply *repl;
      
      if (rep->generic.type != X_Error)
	{
	  /* Actually does nothing, since there are no additional bytes
	   * to read, but maintain good form.
	   */
	  repl = (xGetInputFocusReply *)
	    _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			    (sizeof(xGetInputFocusReply) - sizeof(xReply)) >> 2,
			    True);
	}

      if (state->callback)
	state->callback (state->window, !state->have_error, state->data);

      DeqAsyncHandler(state->dpy, &state->async);

      return (rep->generic.type != X_Error);
    }

  return False;
}

void
_gdk_x11_send_xevent_async (GdkDisplay           *display, 
			    Window                window, 
			    gboolean              propagate,
			    glong                 event_mask,
			    XEvent               *event_send,
			    GdkSendXEventCallback callback,
			    gpointer              data)
{
  Display *dpy;
  SendEventState *state;
  Status status;
  
  dpy = GDK_DISPLAY_XDISPLAY (display);

  state = g_new (SendEventState, 1);

  state->dpy = dpy;
  state->window = window;
  state->callback = callback;
  state->data = data;
  state->have_error = FALSE;
  
  LockDisplay(dpy);

  state->async.next = dpy->async_handlers;
  state->async.handler = send_event_handler;
  state->async.data = (XPointer) state;
  dpy->async_handlers = &state->async;

  {
    register xSendEventReq *req;
    xEvent ev;
    register Status (**fp)();
    
    /* call through display to find proper conversion routine */
    
    fp = &dpy->wire_vec[event_send->type & 0177];
    if (*fp == NULL) *fp = _XEventToWire;
    status = (**fp)(dpy, event_send, &ev);
    
    if (!status)
      {
	g_warning ("Error converting event to wire");
	DeqAsyncHandler(dpy, &state->async);
	UnlockDisplay(dpy);
	SyncHandle();
	g_free (state);

	return;
      }
      
    GetReq(SendEvent, req);
    req->destination = window;
    req->propagate = propagate;
    req->eventMask = event_mask;
    /* gross, matches Xproto.h */
#ifdef WORD64			
    memcpy ((char *) req->eventdata, (char *) &ev, SIZEOF(xEvent));
#else    
    memcpy ((char *) &req->event, (char *) &ev, SIZEOF(xEvent));
#endif
    
    state->send_event_req = dpy->request;
  }

  /*
   * XSync (dpy, 0)
   */
  {
    xReq *req;
    
    GetEmptyReq(GetInputFocus, req);
    state->get_input_focus_req = dpy->request;
  }
  
  UnlockDisplay(dpy);
  SyncHandle();
}

static Bool
set_input_focus_handler (Display *dpy,
			 xReply  *rep,
			 char    *buf,
			 int      len,
			 XPointer data)
{
  SetInputFocusState *state = (SetInputFocusState *)data;  

  if (dpy->last_request_read == state->set_input_focus_req)
    {
      if (rep->generic.type == X_Error &&
	  rep->error.errorCode == BadMatch)
	{
	  /* Consume BadMatch errors, since we have no control
	   * over them.
	   */
	  return True;
	}
    }
  
  if (dpy->last_request_read == state->get_input_focus_req)
    {
      xGetInputFocusReply replbuf;
      xGetInputFocusReply *repl;
      
      if (rep->generic.type != X_Error)
	{
	  /* Actually does nothing, since there are no additional bytes
	   * to read, but maintain good form.
	   */
	  repl = (xGetInputFocusReply *)
	    _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			    (sizeof(xGetInputFocusReply) - sizeof(xReply)) >> 2,
			    True);
	}

      DeqAsyncHandler(state->dpy, &state->async);

      g_free (state);
      
      return (rep->generic.type != X_Error);
    }

  return False;
}

void
_gdk_x11_set_input_focus_safe (GdkDisplay             *display,
			       Window                  window,
			       int                     revert_to,
			       Time                    time)
{
  Display *dpy;
  SetInputFocusState *state;
  
  dpy = GDK_DISPLAY_XDISPLAY (display);

  state = g_new (SetInputFocusState, 1);

  state->dpy = dpy;
  
  LockDisplay(dpy);

  state->async.next = dpy->async_handlers;
  state->async.handler = set_input_focus_handler;
  state->async.data = (XPointer) state;
  dpy->async_handlers = &state->async;

  {
    xSetInputFocusReq *req;
    
    GetReq(SetInputFocus, req);
    req->focus = window;
    req->revertTo = revert_to;
    req->time = time;
    state->set_input_focus_req = dpy->request;
  }

  /*
   * XSync (dpy, 0)
   */
  {
    xReq *req;
    
    GetEmptyReq(GetInputFocus, req);
    state->get_input_focus_req = dpy->request;
  }
  
  UnlockDisplay(dpy);
  SyncHandle();
}

static void
handle_get_wa_reply (Display                   *dpy,
		     ChildInfoState            *state,
		     xGetWindowAttributesReply *repl)
{
  GdkChildInfoX11 *child = &state->child_info[state->n_children_found];
  child->is_mapped = repl->mapState != IsUnmapped;
  child->window_class = repl->class;
}

static void
handle_get_geometry_reply (Display           *dpy,
			   ChildInfoState    *state,
			   xGetGeometryReply *repl)
{
  GdkChildInfoX11 *child = &state->child_info[state->n_children_found];
  
  child->x = cvtINT16toInt (repl->x);
  child->y = cvtINT16toInt (repl->y);
  child->width = repl->width;
  child->height = repl->height;
}

static void
handle_get_property_reply (Display           *dpy,
			   ChildInfoState    *state,
			   xGetPropertyReply *repl)
{
  GdkChildInfoX11 *child = &state->child_info[state->n_children_found];
  child->has_wm_state = repl->propertyType != None;

  /* Since we called GetProperty with longLength of 0, we don't
   * have to worry about consuming the property data that would
   * normally follow after the reply
   */
}

static void
next_child (ChildInfoState *state)
{
  if (state->current_request == CHILD_INFO_GET_GEOMETRY)
    {
      if (!state->have_error && !state->child_has_error)
	{
	  state->child_info[state->n_children_found].window = state->children[state->current_child];
	  state->n_children_found++;
	}
      state->current_child++;
      if (state->get_wm_state)
	state->current_request = CHILD_INFO_GET_PROPERTY;
      else
	state->current_request = CHILD_INFO_GET_WA;
      state->child_has_error = FALSE;
      state->have_error = FALSE;
    }
  else
    state->current_request++;
}

static Bool
get_child_info_handler (Display *dpy,
			xReply  *rep,
			char    *buf,
			int      len,
			XPointer data)
{
  Bool result = True;
  
  ChildInfoState *state = (ChildInfoState *)data;

  if (dpy->last_request_read != state->child_states[state->current_child].seq[state->current_request])
    return False;
  
  if (rep->generic.type == X_Error)
    {
      state->child_has_error = TRUE;
      if (rep->error.errorCode != BadDrawable ||
	  rep->error.errorCode != BadWindow)
	{
	  state->have_error = TRUE;
	  result = False;
	}
    }
  else
    {
      switch (state->current_request)
	{
	case CHILD_INFO_GET_PROPERTY:
	  {
	    xGetPropertyReply replbuf;
	    xGetPropertyReply *repl;
	    
	    repl = (xGetPropertyReply *)
	      _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			      (sizeof(xGetPropertyReply) - sizeof(xReply)) >> 2,
			      True);
	    
	    handle_get_property_reply (dpy, state, repl);
	  }
	  break;
	case CHILD_INFO_GET_WA:
	  {
	    xGetWindowAttributesReply replbuf;
	    xGetWindowAttributesReply *repl;
	    
	    repl = (xGetWindowAttributesReply *)
	      _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			      (sizeof(xGetWindowAttributesReply) - sizeof(xReply)) >> 2,
			      True);
	    
	    handle_get_wa_reply (dpy, state, repl);
	  }
	  break;
	case CHILD_INFO_GET_GEOMETRY:
	  {
	    xGetGeometryReply replbuf;
	    xGetGeometryReply *repl;
	    
	    repl = (xGetGeometryReply *)
	      _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			      (sizeof(xGetGeometryReply) - sizeof(xReply)) >> 2,
			      True);
	    
	    handle_get_geometry_reply (dpy, state, repl);
	  }
	  break;
	}
    }

  next_child (state);

  return result;
}

gboolean
_gdk_x11_get_window_child_info (GdkDisplay       *display,
				Window            window,
				gboolean          get_wm_state,
				GdkChildInfoX11 **children,
				guint            *nchildren)
{
  Display *dpy;
  _XAsyncHandler async;
  ChildInfoState state;
  Window root, parent;
  Atom wm_state_atom;
  Bool result;
  guint i;

  *children = NULL;
  *nchildren = 0;
  
  dpy = GDK_DISPLAY_XDISPLAY (display);
  wm_state_atom = gdk_x11_get_xatom_by_name_for_display (display, "WM_STATE");

  gdk_error_trap_push ();
  result = XQueryTree (dpy, window, &root, &parent,
		       &state.children, &state.nchildren);
  gdk_error_trap_pop ();
  if (!result)
    return FALSE;

  state.get_wm_state = get_wm_state;
  state.child_info = g_new (GdkChildInfoX11, state.nchildren);
  state.child_states = g_new (ChildInfoChildState, state.nchildren);
  state.current_child = 0;
  state.n_children_found = 0;
  if (get_wm_state)
    state.current_request = CHILD_INFO_GET_PROPERTY;
  else
    state.current_request = CHILD_INFO_GET_WA;
  state.have_error = FALSE;
  state.child_has_error = FALSE;

  LockDisplay(dpy);

  async.next = dpy->async_handlers;
  async.handler = get_child_info_handler;
  async.data = (XPointer) &state;
  dpy->async_handlers = &async;
  
  for (i = 0; i < state.nchildren; i++)
    {
      xResourceReq *resource_req;
      xGetPropertyReq *prop_req;
      Window window = state.children[i];
      
      if (get_wm_state)
	{
	  GetReq (GetProperty, prop_req);
	  prop_req->window = window;
	  prop_req->property = wm_state_atom;
	  prop_req->type = AnyPropertyType;
	  prop_req->delete = False;
	  prop_req->longOffset = 0;
	  prop_req->longLength = 0;

	  state.child_states[i].seq[CHILD_INFO_GET_PROPERTY] = dpy->request;
	}
      
      GetResReq(GetWindowAttributes, window, resource_req);
      state.child_states[i].seq[CHILD_INFO_GET_WA] = dpy->request;
      
      GetResReq(GetGeometry, window, resource_req);
      state.child_states[i].seq[CHILD_INFO_GET_GEOMETRY] = dpy->request;
    }

  if (i != 0)
    {
      /* Wait for the last reply
       */
      xGetGeometryReply rep;

      /* On error, our async handler will get called
       */
      if (_XReply (dpy, (xReply *)&rep, 0, xTrue))
	handle_get_geometry_reply (dpy, &state, &rep);

      next_child (&state);
    }

  if (!state.have_error)
    {
      *children = state.child_info;
      *nchildren = state.n_children_found;
    }
  else
    {
      g_free (state.child_info);
    }

  XFree (state.children);
  g_free (state.child_states);
  
  DeqAsyncHandler(dpy, &async);
  UnlockDisplay(dpy);
  SyncHandle();

  return !state.have_error;
}

