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

#include "gdk.h"
#include "gdkinternals.h"

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

/* Private variable declarations
 */

static guint32 button_click_time[2] = { 0, 0}; /* The last 2 button click times. Used
						* to determine if the latest button click
						* is part of a double or triple click.
						*/
static GdkWindow *button_window[2] = { NULL, NULL}; /* The last 2 windows to receive button presses.
						     *	Also used to determine if the latest button
						     *	click is part of a double or triple click.
					     */
static guint button_number[2] = { -1, -1 }; /* The last 2 buttons to be pressed.
					     */
GdkEventFunc   _gdk_event_func = NULL;    /* Callback for events */
gpointer       _gdk_event_data = NULL;
GDestroyNotify _gdk_event_notify = NULL;

static guint double_click_time = 250;
#define TRIPLE_CLICK_TIME      (2*double_click_time)
#define DOUBLE_CLICK_DIST      5
#define TRIPLE_CLICK_DIST      5

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

/*************************************************************
 * _gdk_event_queue_find_first:
 *     Find the first event on the queue that is not still
 *     being filled in.
 *   arguments:
 *     
 *   results:
 *     Pointer to the list node for that event, or NULL
 *************************************************************/

GList*
_gdk_event_queue_find_first (void)
{
  GList *tmp_list = _gdk_queued_events;

  while (tmp_list)
    {
      GdkEventPrivate *event = tmp_list->data;
      if (!(event->flags & GDK_EVENT_PENDING))
	return tmp_list;

      tmp_list = g_list_next (tmp_list);
    }

  return NULL;
}

/*************************************************************
 * _gdk_event_queue_remove_link:
 *     Remove a specified list node from the event queue.
 *   arguments:
 *     node: Node to remove.
 *   results:
 *************************************************************/

void
_gdk_event_queue_remove_link (GList *node)
{
  if (node->prev)
    node->prev->next = node->next;
  else
    _gdk_queued_events = node->next;
  
  if (node->next)
    node->next->prev = node->prev;
  else
    _gdk_queued_tail = node->prev;
}

/*************************************************************
 * _gdk_event_queue_append:
 *     Append an event onto the tail of the event queue.
 *   arguments:
 *     event: Event to append.
 *   results:
 *************************************************************/

void
_gdk_event_queue_append (GdkEvent *event)
{
  _gdk_queued_tail = g_list_append (_gdk_queued_tail, event);
  
  if (!_gdk_queued_events)
    _gdk_queued_events = _gdk_queued_tail;
  else
    _gdk_queued_tail = _gdk_queued_tail->next;
}

/*************************************************************
 * gdk_event_handler_set:
 *     
 *   arguments:
 *     func: Callback function to be called for each event.
 *     data: Data supplied to the function
 *     notify: function called when function is no longer needed
 * 
 *   results:
 *************************************************************/

void 
gdk_event_handler_set (GdkEventFunc   func,
		       gpointer       data,
		       GDestroyNotify notify)
{
  if (_gdk_event_notify)
    (*_gdk_event_notify) (_gdk_event_data);

  _gdk_event_func = func;
  _gdk_event_data = data;
  _gdk_event_notify = notify;
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
 *   If an event is waiting that we care about, returns 
 *   a pointer to that event, to be freed with gdk_event_free.
 *   Otherwise, returns NULL.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

GdkEvent*
gdk_event_get (void)
{
  _gdk_events_queue ();

  return _gdk_event_unqueue ();
}

/*
 *--------------------------------------------------------------
 * gdk_event_peek
 *
 *   Gets the next event.
 *
 * Arguments:
 *
 * Results:
 *   If an event is waiting that we care about, returns 
 *   a copy of that event, but does not remove it from
 *   the queue. The pointer is to be freed with gdk_event_free.
 *   Otherwise, returns NULL.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

GdkEvent*
gdk_event_peek (void)
{
  GList *tmp_list;

  tmp_list = _gdk_event_queue_find_first ();
  
  if (tmp_list)
    return gdk_event_copy (tmp_list->data);
  else
    return NULL;
}

void
gdk_event_put (GdkEvent *event)
{
  GdkEvent *new_event;
  
  g_return_if_fail (event != NULL);
  
  new_event = gdk_event_copy (event);

  _gdk_event_queue_append (new_event);
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

static GMemChunk *event_chunk = NULL;

GdkEvent*
_gdk_event_new (void)
{
  GdkEventPrivate *new_event;
  
  if (event_chunk == NULL)
    event_chunk = g_mem_chunk_new ("events",
				   sizeof (GdkEventPrivate),
				   4096,
				   G_ALLOC_AND_FREE);
  
  new_event = g_chunk_new (GdkEventPrivate, event_chunk);
  new_event->flags = 0;
  
  return (GdkEvent*) new_event;
}

GdkEvent*
gdk_event_copy (GdkEvent *event)
{
  GdkEvent *new_event;
  
  g_return_val_if_fail (event != NULL, NULL);
  
  new_event = _gdk_event_new ();
  
  *new_event = *event;
  if (new_event->any.window)
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
      
    case GDK_EXPOSE:
      if (event->expose.region)
	new_event->expose.region = gdk_region_copy (event->expose.region);
      break;
      
    case GDK_SETTING:
      new_event->setting.name = g_strdup (new_event->setting.name);
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
  g_return_if_fail (event != NULL);

  g_assert (event_chunk != NULL); /* paranoid */
  
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

    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      if (event->button.axes)
	g_free (event->button.axes);
      break;
      
    case GDK_EXPOSE:
      if (event->expose.region)
	gdk_region_destroy (event->expose.region);
      break;
      
    case GDK_MOTION_NOTIFY:
      if (event->motion.axes)
	g_free (event->motion.axes);
      break;
      
    case GDK_SETTING:
      g_free (event->setting.name);
      break;
      
    default:
      break;
    }
  
  g_mem_chunk_free (event_chunk, event);
}

/**
 * gdk_event_get_time:
 * @event: a #GdkEvent
 * 
 * Returns the time stamp from @event, if there is one; otherwise
 * returns #GDK_CURRENT_TIME. If @event is %NULL, returns #GDK_CURRENT_TIME.
 * 
 * Return value: time stamp field from @event
 **/
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
      case GDK_SCROLL:
        return event->scroll.time;
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
      case GDK_CLIENT_EVENT:
      case GDK_VISIBILITY_NOTIFY:
      case GDK_NO_EXPOSE:
      case GDK_CONFIGURE:
      case GDK_FOCUS_CHANGE:
      case GDK_NOTHING:
      case GDK_DELETE:
      case GDK_DESTROY:
      case GDK_EXPOSE:
      case GDK_MAP:
      case GDK_UNMAP:
      case GDK_WINDOW_STATE:
      case GDK_SETTING:
        /* return current time */
        break;
      }
  
  return GDK_CURRENT_TIME;
}

/**
 * gdk_event_get_state:
 * @event: a #GdkEvent or NULL
 * @state: return location for state
 * 
 * If the event contains a "state" field, puts that field in @state. Otherwise
 * stores an empty state (0). Returns %TRUE if there was a state field
 * in the event. @event may be %NULL, in which case it's treated
 * as if the event had no state field.
 * 
 * Return value: %TRUE if there was a state field in the event 
 **/
gboolean
gdk_event_get_state (GdkEvent        *event,
                     GdkModifierType *state)
{
  g_return_val_if_fail (state != NULL, FALSE);
  
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
	*state = event->motion.state;
        return TRUE;
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
        *state =  event->button.state;
        return TRUE;
      case GDK_SCROLL:
	*state =  event->scroll.state;
        return TRUE;
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
	*state =  event->key.state;
        return TRUE;
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
	*state =  event->crossing.state;
        return TRUE;
      case GDK_PROPERTY_NOTIFY:
	*state =  event->property.state;
        return TRUE;
      case GDK_VISIBILITY_NOTIFY:
      case GDK_CLIENT_EVENT:
      case GDK_NO_EXPOSE:
      case GDK_CONFIGURE:
      case GDK_FOCUS_CHANGE:
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
      case GDK_DRAG_ENTER:
      case GDK_DRAG_LEAVE:
      case GDK_DRAG_MOTION:
      case GDK_DRAG_STATUS:
      case GDK_DROP_START:
      case GDK_DROP_FINISHED:
      case GDK_NOTHING:
      case GDK_DELETE:
      case GDK_DESTROY:
      case GDK_EXPOSE:
      case GDK_MAP:
      case GDK_UNMAP:
      case GDK_WINDOW_STATE:
      case GDK_SETTING:
        /* no state field */
        break;
      }

  *state = 0;
  return FALSE;
}

/**
 * gdk_event_get_coords:
 * @event: a #GdkEvent
 * @x_win: location to put event window x coordinate
 * @y_win: location to put event window y coordinate
 * 
 * Extract the event window relative x/y coordinates from an event.
 * 
 * Return value: %TRUE if the event delivered event window coordinates
 **/
gboolean
gdk_event_get_coords (GdkEvent *event,
		      gdouble  *x_win,
		      gdouble  *y_win)
{
  gdouble x = 0, y = 0;
  gboolean fetched = TRUE;
  
  g_return_val_if_fail (event != NULL, FALSE);

  switch (event->type)
    {
    case GDK_CONFIGURE:
      x = event->configure.x;
      y = event->configure.y;
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      x = event->crossing.x;
      y = event->crossing.y;
      break;
    case GDK_SCROLL:
      x = event->scroll.x;
      y = event->scroll.y;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      x = event->button.x;
      y = event->button.y;
      break;
    case GDK_MOTION_NOTIFY:
      x = event->motion.x;
      y = event->motion.y;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (x_win)
    *x_win = x;
  if (y_win)
    *y_win = y;

  return fetched;
}

/**
 * gdk_event_get_root_coords:
 * @event: a #GdkEvent
 * @x_root: location to put root window x coordinate
 * @y_root: location to put root window y coordinate
 * 
 * Extract the root window relative x/y coordinates from an event.
 * 
 * Return value: %TRUE if the event delivered root window coordinates
 **/
gboolean
gdk_event_get_root_coords (GdkEvent *event,
			   gdouble  *x_root,
			   gdouble  *y_root)
{
  gdouble x = 0, y = 0;
  gboolean fetched = TRUE;
  
  g_return_val_if_fail (event != NULL, FALSE);

  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
      x = event->motion.x_root;
      y = event->motion.y_root;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      x = event->button.x_root;
      y = event->button.y_root;
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      x = event->crossing.x_root;
      y = event->crossing.y_root;
      break;
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      x = event->dnd.x_root;
      y = event->dnd.y_root;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (x_root)
    *x_root = x;
  if (y_root)
    *y_root = y;

  return fetched;
}

/**
 * gdk_event_get_axis:
 * @event: a #GdkEvent
 * @axis_use: the axis use to look for
 * @value: location to store the value found
 * 
 * Extract the axis value for a particular axis use from
 * an event structure.
 * 
 * Return value: %TRUE if the specified axis was found, otherwise %FALSE
 **/
gboolean
gdk_event_get_axis (GdkEvent   *event,
		    GdkAxisUse  axis_use,
		    gdouble    *value)
{
  gdouble *axes;
  GdkDevice *device;
  
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (axis_use == GDK_AXIS_X || axis_use == GDK_AXIS_Y)
    {
      gdouble x, y;
      
      switch (event->type)
	{
	case GDK_MOTION_NOTIFY:
	  x = event->motion.x;
	  y = event->motion.y;
	  break;
	case GDK_SCROLL:
	  x = event->scroll.x;
	  y = event->scroll.y;
	  break;
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	  x = event->button.x;
	  y = event->button.y;
	  break;
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
	  x = event->crossing.x;
	  y = event->crossing.y;
	  break;
	  
	default:
	  return FALSE;
	}

      if (axis_use == GDK_AXIS_X && value)
	*value = x;
      if (axis_use == GDK_AXIS_Y && value)
	*value = y;

      return TRUE;
    }
  else if (event->type == GDK_BUTTON_PRESS ||
	   event->type == GDK_BUTTON_RELEASE)
    {
      device = event->button.device;
      axes = event->button.axes;
    }
  else if (event->type == GDK_MOTION_NOTIFY)
    {
      device = event->motion.device;
      axes = event->motion.axes;
    }
  else
    return FALSE;

  return gdk_device_get_axis (device, axes, axis_use, value);
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

void
gdk_set_show_events (gboolean show_events)
{
  if (show_events)
    _gdk_debug_flags |= GDK_DEBUG_EVENTS;
  else
    _gdk_debug_flags &= ~GDK_DEBUG_EVENTS;
}

gboolean
gdk_get_show_events (void)
{
  return (_gdk_debug_flags & GDK_DEBUG_EVENTS) != 0;
}

static void
gdk_io_destroy (gpointer data)
{
  GdkIOClosure *closure = data;

  if (closure->notify)
    closure->notify (closure->data);

  g_free (closure);
}

/* What do we do with G_IO_NVAL?
 */
#define READ_CONDITION (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define WRITE_CONDITION (G_IO_OUT | G_IO_ERR)
#define EXCEPTION_CONDITION (G_IO_PRI)

static gboolean  
gdk_io_invoke (GIOChannel   *source,
	       GIOCondition  condition,
	       gpointer      data)
{
  GdkIOClosure *closure = data;
  GdkInputCondition gdk_cond = 0;

  if (condition & READ_CONDITION)
    gdk_cond |= GDK_INPUT_READ;
  if (condition & WRITE_CONDITION)
    gdk_cond |= GDK_INPUT_WRITE;
  if (condition & EXCEPTION_CONDITION)
    gdk_cond |= GDK_INPUT_EXCEPTION;

  if (closure->condition & gdk_cond)
    closure->function (closure->data, g_io_channel_unix_get_fd (source), gdk_cond);

  return TRUE;
}

gint
gdk_input_add_full (gint	      source,
		    GdkInputCondition condition,
		    GdkInputFunction  function,
		    gpointer	      data,
		    GdkDestroyNotify  destroy)
{
  guint result;
  GdkIOClosure *closure = g_new (GdkIOClosure, 1);
  GIOChannel *channel;
  GIOCondition cond = 0;

  closure->function = function;
  closure->condition = condition;
  closure->notify = destroy;
  closure->data = data;

  if (condition & GDK_INPUT_READ)
    cond |= READ_CONDITION;
  if (condition & GDK_INPUT_WRITE)
    cond |= WRITE_CONDITION;
  if (condition & GDK_INPUT_EXCEPTION)
    cond |= EXCEPTION_CONDITION;

  channel = g_io_channel_unix_new (source);
  result = g_io_add_watch_full (channel, G_PRIORITY_DEFAULT, cond, 
				gdk_io_invoke,
				closure, gdk_io_destroy);
  g_io_channel_unref (channel);

  return result;
}

gint
gdk_input_add (gint		 source,
	       GdkInputCondition condition,
	       GdkInputFunction	 function,
	       gpointer		 data)
{
  return gdk_input_add_full (source, condition, function, data, NULL);
}

void
gdk_input_remove (gint tag)
{
  g_source_remove (tag);
}

GdkEvent*
_gdk_event_unqueue (void)
{
  GdkEvent *event = NULL;
  GList *tmp_list;

  tmp_list = _gdk_event_queue_find_first ();

  if (tmp_list)
    {
      event = tmp_list->data;
      _gdk_event_queue_remove_link (tmp_list);
      g_list_free_1 (tmp_list);
    }

  return event;
}

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

void
_gdk_event_button_generate (GdkEvent *event)
{
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
  else if ((event->button.time < (button_click_time[0] + double_click_time)) &&
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
}


void
gdk_synthesize_window_state (GdkWindow     *window,
                             GdkWindowState unset_flags,
                             GdkWindowState set_flags)
{
  GdkEvent temp_event;
  GdkWindowState old;
  
  g_return_if_fail (window != NULL);
  
  temp_event.window_state.window = window;
  temp_event.window_state.type = GDK_WINDOW_STATE;
  temp_event.window_state.send_event = FALSE;
  
  old = ((GdkWindowObject*) temp_event.window_state.window)->state;
  
  temp_event.window_state.changed_mask = (unset_flags | set_flags) ^ old;
  temp_event.window_state.new_window_state = old;
  temp_event.window_state.new_window_state |= set_flags;
  temp_event.window_state.new_window_state &= ~unset_flags;

  if (temp_event.window_state.new_window_state == old)
    return; /* No actual work to do, nothing changed. */

  /* Actually update the field in GdkWindow, this is sort of an odd
   * place to do it, but seems like the safest since it ensures we expose no
   * inconsistent state to the user.
   */
  
  ((GdkWindowObject*) window)->state = temp_event.window_state.new_window_state;

  /* We only really send the event to toplevels, since
   * all the window states don't apply to non-toplevels.
   * Non-toplevels do use the GDK_WINDOW_STATE_WITHDRAWN flag
   * internally so we needed to update window->state.
   */
  switch (((GdkWindowObject*) window)->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP: /* ? */
      gdk_event_put (&temp_event);
      break;
      
    case GDK_WINDOW_FOREIGN:
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_CHILD:
      break;
    }
}

/**
 * gdk_set_double_click_time:
 * @msec: double click time in milliseconds (thousandths of a second)
 *
 * Sets the double click time (two clicks within this time interval
 * count as a double click and result in a #GDK_2BUTTON_PRESS event).
 * Applications should NOT set this, it is a global user-configured setting.
 *
 **/
void
gdk_set_double_click_time (guint msec)
{
  double_click_time = msec;
}

GType
gdk_event_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("GdkEvent",
					     (GBoxedCopyFunc)gdk_event_copy,
					     (GBoxedFreeFunc)gdk_event_free);
  return our_type;
}

GdkDevice *
gdk_device_get_core_pointer (void)
{
  return _gdk_core_pointer;
}
