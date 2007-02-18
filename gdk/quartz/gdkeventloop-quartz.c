#include <config.h>

#include <glib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "gdkprivate-quartz.h"

static GPollFD event_poll_fd;
static NSEvent *current_event;

static GPollFunc old_poll_func;

static pthread_t select_thread = 0;
static int wakeup_pipe[2];
static pthread_mutex_t pollfd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready_cond = PTHREAD_COND_INITIALIZER;
static GPollFD *pollfds;
static GPollFD *pipe_pollfd;
static guint n_pollfds;
static CFRunLoopSourceRef select_main_thread_source;
static CFRunLoopRef main_thread_run_loop;

static gboolean
gdk_event_prepare (GSource *source,
		   gint    *timeout)
{
  NSEvent *event;
  gboolean retval;
  
  GDK_QUARTZ_ALLOC_POOL;

  *timeout = -1;

  event = [NSApp nextEventMatchingMask: NSAnyEventMask
	                     untilDate: [NSDate distantPast]
	                        inMode: NSDefaultRunLoopMode
	                       dequeue: NO];

  retval = (_gdk_event_queue_find_first (_gdk_display) != NULL ||
	    event != NULL);

  GDK_QUARTZ_RELEASE_POOL;

  return retval;
}

static gboolean
gdk_event_check (GSource *source)
{
  if (_gdk_event_queue_find_first (_gdk_display) != NULL ||
      current_event)
    return TRUE;

  /* FIXME: We should maybe try to fetch an event again here */

  return FALSE;
}

static gboolean
gdk_event_dispatch (GSource     *source,
		    GSourceFunc  callback,
		    gpointer     user_data)
{
  GdkEvent *event;

  GDK_QUARTZ_ALLOC_POOL;

  _gdk_events_queue (_gdk_display);

  event = _gdk_event_unqueue (_gdk_display);

  if (event)
    {
      if (_gdk_event_func)
	(*_gdk_event_func) (event, _gdk_event_data);

      gdk_event_free (event);
    }

  GDK_QUARTZ_RELEASE_POOL;

  return TRUE;
}

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  NULL
};

static void 
got_fd_activity (void *info)
{
  NSEvent *event;

  /* Post a message so we'll break out of the message loop */
  event = [NSEvent otherEventWithType: NSApplicationDefined
	                     location: NSZeroPoint
	                modifierFlags: 0
	                    timestamp: 0
	                 windowNumber: 0
	                      context: nil
	                      subtype: 0
	                        data1: 0 
	                        data2: 0];

  [NSApp postEvent:event atStart:YES];
}

static void *
select_thread_func (void *arg)
{
  int n_active_fds;

  while (1)
    {
      pthread_mutex_lock (&pollfd_mutex);
      pthread_cond_wait (&ready_cond, &pollfd_mutex);

      n_active_fds = old_poll_func (pollfds, n_pollfds, -1);
      if (pipe_pollfd->revents)
	{
	  char c;
	  int n;

	  n = read (pipe_pollfd->fd, &c, 1);

	  g_assert (n == 1);
	  g_assert (c == 'A');

	  n_active_fds --;
	}
      pthread_mutex_unlock (&pollfd_mutex);

      if (n_active_fds)
	{
	  /* We have active fds, signal the main thread */
	  CFRunLoopSourceSignal (select_main_thread_source);
	  if (CFRunLoopIsWaiting (main_thread_run_loop))
	    CFRunLoopWakeUp (main_thread_run_loop);
	}
    }
}

static gint
poll_func (GPollFD *ufds, guint nfds, gint timeout_)
{
  NSEvent *event;
  NSDate *limit_date;
  int n_active = 0;
  int i;

  GDK_QUARTZ_ALLOC_POOL;

  if (nfds > 1)
    {
      if (!select_thread) {
        /* Create source used for signalling the main thread */
        main_thread_run_loop = CFRunLoopGetCurrent ();
        CFRunLoopSourceContext source_context = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, got_fd_activity };
        select_main_thread_source = CFRunLoopSourceCreate (NULL, 0, &source_context);
        CFRunLoopAddSource (main_thread_run_loop, select_main_thread_source, kCFRunLoopDefaultMode);

        pipe (wakeup_pipe);
        pthread_create (&select_thread, NULL, select_thread_func, NULL);
      }

      pthread_mutex_lock (&pollfd_mutex);
      n_pollfds = nfds;
      g_free (pollfds);
      pollfds = g_memdup (ufds, sizeof (GPollFD) * nfds);

      /* We cheat and use the fake fd for our pipe */
      for (i = 0; i < nfds; i++)
        {
          if (pollfds[i].fd == -1)
            {
              pipe_pollfd = &pollfds[i];
              pollfds[i].fd = wakeup_pipe[0];
              pollfds[i].events = G_IO_IN;
            }
        }

      pthread_mutex_unlock (&pollfd_mutex);

      /* Start our thread */
      pthread_cond_signal (&ready_cond);
    }

  if (timeout_ == -1)
    limit_date = [NSDate distantFuture];
  else if (timeout_ == 0)
    limit_date = [NSDate distantPast];
  else
    limit_date = [NSDate dateWithTimeIntervalSinceNow:timeout_/1000.0];

  event = [NSApp nextEventMatchingMask: NSAnyEventMask
	                     untilDate: limit_date
	                        inMode: NSDefaultRunLoopMode
                               dequeue: YES];
  
  if (event)
    {
      if ([event type] == NSApplicationDefined)
        {
          pthread_mutex_lock (&pollfd_mutex);

          for (i = 0; i < n_pollfds; i++)
            {
              if (ufds[i].fd == -1)
                continue;

              g_assert (ufds[i].fd == pollfds[i].fd);
              g_assert (ufds[i].events == pollfds[i].events);

              if (pollfds[i].revents)
                {
                  ufds[i].revents = pollfds[i].revents;
                  n_active ++;
                }
            }

          pthread_mutex_unlock (&pollfd_mutex);

          event = [NSApp nextEventMatchingMask: NSAnyEventMask
            untilDate: [NSDate distantPast]
            inMode: NSDefaultRunLoopMode
            dequeue: YES];

        }
    }

  /* There were no active fds, break out of the other thread's poll() */
  if (n_active == 0 && wakeup_pipe[1])
    {
      char c = 'A';

      write (wakeup_pipe[1], &c, 1);
    }

  if (event) 
    {
      ufds[0].revents = G_IO_IN;

      /* FIXME: We can't assert here, but we might need to have a
       * queue for events instead.
       */
      /*g_assert (current_event == NULL);*/

      current_event = [event retain];

      n_active ++;
    }

  GDK_QUARTZ_RELEASE_POOL;

  return n_active;
}

void
_gdk_quartz_event_loop_init (void)
{
  GSource *source;

  event_poll_fd.events = G_IO_IN;
  event_poll_fd.fd = -1;

  source = g_source_new (&event_funcs, sizeof (GSource));
  g_source_add_poll (source, &event_poll_fd);
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  old_poll_func = g_main_context_get_poll_func (NULL);
  g_main_context_set_poll_func (NULL, poll_func);  
 
}

NSEvent *
_gdk_quartz_event_loop_get_current (void)
{
  return current_event;
}

void
_gdk_quartz_event_loop_release_current (void)
{
  [current_event release];
  current_event = NULL;
}

