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

#include <gdk/gdk.h>
#include <gdk/gdkinternals.h>
#include "gdkprivate-fb.h"
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <math.h>

typedef struct _GdkFBMouse GdkFBMouse;
typedef struct _GdkFBMouseDevice GdkFBMouseDevice;

struct _GdkFBMouse {
  gint fd; /* Set by open */

  /* These are written to by parse_packet */
  gdouble x, y;
  gboolean button_pressed[3];

  guchar mouse_packet[5]; /* read by parse_packet */
  gint packet_nbytes;
  
  gboolean click_grab;
  GIOChannel *io;
  gint io_tag;

  GdkFBMouseDevice *dev;
};

static GdkFBMouse *gdk_fb_mouse = NULL;

void
gdk_fb_mouse_get_info (gint *x,
		       gint *y,
		       GdkModifierType *mask)
{
  if (x)
    *x = gdk_fb_mouse->x;
  if (y)
    *y = gdk_fb_mouse->y;
  if (mask)
    *mask =
      (gdk_fb_mouse->button_pressed[0]?GDK_BUTTON1_MASK:0) |
      (gdk_fb_mouse->button_pressed[1]?GDK_BUTTON2_MASK:0) |
      (gdk_fb_mouse->button_pressed[2]?GDK_BUTTON3_MASK:0) |
      gdk_fb_keyboard_modifiers ();
}

static void
handle_mouse_movement(GdkFBMouse *mouse)
{
  GdkWindow *mousewin;
  GdkEvent *event;
  gint x, y;
  GdkWindow *win, *grabwin;
  guint state;
  GdkDrawableFBData *mousewin_private;

  if (_gdk_fb_pointer_grab_confine)
    mousewin = _gdk_fb_pointer_grab_confine;
  else
    mousewin = gdk_parent_root;

  mousewin_private = GDK_DRAWABLE_IMPL_FBDATA (mousewin);
  
  if (mouse->x < mousewin_private->llim_x)
    mouse->x = mousewin_private->llim_x;
  else if (mouse->x > mousewin_private->lim_x - 1)
    mouse->x = mousewin_private->lim_x - 1;
  
  if (mouse->y < mousewin_private->llim_y)
    mouse->y = mousewin_private->llim_y;
  else if (mouse->y > mousewin_private->lim_y - 1)
    mouse->y = mousewin_private->lim_y - 1;

  win = gdk_window_at_pointer (NULL, NULL);
  if (_gdk_fb_pointer_grab_window_events)
    grabwin = _gdk_fb_pointer_grab_window_events;
  else
    grabwin = win;
  
  gdk_fb_cursor_move (mouse->x, mouse->y, grabwin);
  
  gdk_window_get_origin (grabwin, &x, &y);
  x = mouse->x - x;
  y = mouse->y - y;

  state = (mouse->button_pressed[0]?GDK_BUTTON1_MASK:0) |
    (mouse->button_pressed[1]?GDK_BUTTON2_MASK:0) |
    (mouse->button_pressed[2]?GDK_BUTTON3_MASK:0) |
    gdk_fb_keyboard_modifiers ();

  event = gdk_event_make (grabwin, GDK_MOTION_NOTIFY, TRUE);
  if (event)
    {
      event->motion.x = x;
      event->motion.y = y;
      event->motion.state = state;
      event->motion.is_hint = FALSE;
      event->motion.device = gdk_core_pointer;
      event->motion.x_root = mouse->x;
      event->motion.y_root = mouse->y;
    }

  gdk_fb_window_send_crossing_events (win, GDK_CROSSING_NORMAL);
}

static void
send_button_event (GdkFBMouse *mouse,
		   guint button,
		   gboolean press_event)
{
  GdkEvent *event;
  gint x, y, i;
  GdkWindow *window;
  int nbuttons;

  if (_gdk_fb_pointer_grab_window_events)
    window = _gdk_fb_pointer_grab_window_events;
  else
    window = gdk_window_at_pointer(NULL, NULL);

  event = gdk_event_make (window, press_event ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE, FALSE);

  if (event)
    {
      gdk_window_get_origin (window, &x, &y);
      x = mouse->x - x;
      y = mouse->y - y;
      
      event->button.x = x;
      event->button.y = y;
      event->button.button = button;
      event->button.state = (mouse->button_pressed[0]?GDK_BUTTON1_MASK:0) |
	(mouse->button_pressed[1] ? GDK_BUTTON2_MASK : 0) |
	(mouse->button_pressed[2] ? GDK_BUTTON3_MASK : 0) |
	(1 << (button + 8)) /* badhack */ |
	gdk_fb_keyboard_modifiers ();
      event->button.device = gdk_core_pointer;
      event->button.x_root = mouse->x;
      event->button.y_root = mouse->y;
      
      gdk_event_queue_append (event);
      
      /* For double-clicks */
      if (press_event)
	gdk_event_button_generate (event);
    }

  nbuttons = 0;
  for (i=0;i<3;i++)
    if (mouse->button_pressed[i])
      nbuttons++;
  
  /* Handle implicit button grabs: */
  if (press_event && nbuttons == 1)
    {
      gdk_fb_pointer_grab (window, FALSE,
			   gdk_window_get_events (window),
			   NULL, NULL,
			   GDK_CURRENT_TIME, TRUE);
      mouse->click_grab = TRUE;
    }
  else if (!press_event && nbuttons == 0 && mouse->click_grab)
    {
      gdk_fb_pointer_ungrab (GDK_CURRENT_TIME, TRUE);
      mouse->click_grab = FALSE;
    }
}

/******************************************************
 ************ Device specific mouse code **************
 ******************************************************/

struct _GdkFBMouseDevice {
  char *name;
  gint packet_size;
  gboolean (*open)(GdkFBMouse *mouse);
  void (*close)(GdkFBMouse *mouse);
  gboolean (*parse_packet)(GdkFBMouse *mouse, gboolean *got_motion);
};

static gboolean handle_mouse_io             (GIOChannel   *gioc,
					     GIOCondition  cond,
					     gpointer      data);
static gboolean gdk_fb_mouse_ps2_open       (GdkFBMouse   *mouse);
static void     gdk_fb_mouse_ps2_close      (GdkFBMouse   *mouse);
static gboolean gdk_fb_mouse_ps2_packet     (GdkFBMouse   *mouse,
					     gboolean     *got_motion);
static gboolean gdk_fb_mouse_ms_open        (GdkFBMouse   *mouse);
static void     gdk_fb_mouse_ms_close       (GdkFBMouse   *mouse);
static gboolean gdk_fb_mouse_ms_packet      (GdkFBMouse   *mouse,
					     gboolean     *got_motion);
static gboolean gdk_fb_mouse_fidmour_open   (GdkFBMouse   *mouse);
static void     gdk_fb_mouse_fidmour_close  (GdkFBMouse   *mouse);
static gboolean gdk_fb_mouse_fidmour_packet (GdkFBMouse   *mouse,
					     gboolean     *got_motion);

static GdkFBMouseDevice mouse_devs[] =
{
  { "ps2",
    3,
    gdk_fb_mouse_ps2_open,
    gdk_fb_mouse_ps2_close,
    gdk_fb_mouse_ps2_packet
  },
  { "ms",
    3,
    gdk_fb_mouse_ms_open,
    gdk_fb_mouse_ms_close,
    gdk_fb_mouse_ms_packet
  },
  { "fidmour",
    5,
    gdk_fb_mouse_fidmour_open,
    gdk_fb_mouse_fidmour_close,
    gdk_fb_mouse_fidmour_packet
  }
};


gboolean
gdk_fb_mouse_open (void)
{
  GdkFBMouse *mouse;
  GdkFBMouseDevice *device;
  char *mouse_type;
  int i;

  mouse = g_new0 (GdkFBMouse, 1);
  mouse->fd = -1;
  mouse_type = getenv ("GDK_MOUSE_TYPE");
  if (!mouse_type)
    mouse_type = "ps2";

  for (i=0;i<G_N_ELEMENTS(mouse_devs);i++)
    {
      if (g_strcasecmp(mouse_type, mouse_devs[i].name)==0)
	break;
    }
  
  if (i == G_N_ELEMENTS(mouse_devs))
    {
      g_warning ("No mouse driver of type %s found", mouse_type);
      return FALSE;
    }

  device = &mouse_devs[i];

  mouse->dev = device;
  
  mouse->x = gdk_display->modeinfo.xres / 2;
  mouse->y = gdk_display->modeinfo.yres / 2;

  if (!device->open(mouse))
    {
      g_warning ("Mouse driver open failed");
      g_free (mouse);
      return FALSE;
    }

  mouse->io = g_io_channel_unix_new (mouse->fd);
  mouse->io_tag = g_io_add_watch (mouse->io, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, handle_mouse_io, mouse);

  gdk_fb_mouse = mouse;
  return TRUE;
}

void 
gdk_fb_mouse_close (void)
{
  g_source_remove (gdk_fb_mouse->io_tag);
  gdk_fb_mouse->dev->close(gdk_fb_mouse);
  g_io_channel_unref (gdk_fb_mouse->io);
  g_free (gdk_fb_mouse);
}

static gboolean
handle_mouse_io (GIOChannel *gioc,
		 GIOCondition cond,
		 gpointer data)
{
  GdkFBMouse *mouse = (GdkFBMouse *)data;
  GdkFBMouseDevice *dev = mouse->dev;
  gboolean got_motion;
  gint n;

  got_motion = FALSE;
  
  while (1)
    {
      n = read (mouse->fd, mouse->mouse_packet + mouse->packet_nbytes, dev->packet_size - mouse->packet_nbytes);
      if (n<=0) /* error or nothing to read */
	break;
  
      mouse->packet_nbytes += n;
      
      if (mouse->packet_nbytes == dev->packet_size)
	{
	  if (dev->parse_packet (mouse, &got_motion))
	    mouse->packet_nbytes = 0;
	}
    }
  
  if (got_motion)
    handle_mouse_movement (mouse);
  
  return TRUE;
}

static gint
gdk_fb_mouse_dev_open (char *devname, gint mode)
{
  gint fd;
  
  /* Use nonblocking mode to open, to not hang on device */
  fd = open (devname, mode | O_NONBLOCK);
  return fd;
}

static gboolean
gdk_fb_mouse_ps2_open (GdkFBMouse *mouse)
{
  gint fd;
  guchar buf[7];
  int i = 0;

  fd = gdk_fb_mouse_dev_open ("/dev/psaux", O_RDWR);
  if (fd < 0)
    return FALSE;
  
  /* From xf86_Mouse.c */
  buf[i++] = 230; /* 1:1 scaling */
  buf[i++] = 244; /* enable mouse */
  buf[i++] = 243; /* Sample rate */
  buf[i++] = 200;
  buf[i++] = 232; /* device resolution */
  buf[i++] = 1;
  
  write (fd, buf, i);
  
  usleep (10000); /* sleep 10 ms, then read whatever junk we can get from the mouse, in a vain attempt
		     to get synchronized with the event stream */
  
  while ((i = read (fd, buf, sizeof(buf))) > 0)
    g_print ("Got %d bytes of junk from psaux\n", i);
  
  mouse->fd = fd;
  return TRUE;
}

static void
gdk_fb_mouse_ps2_close (GdkFBMouse *mouse)
{
  close (mouse->fd);
}

static gboolean
gdk_fb_mouse_ps2_packet (GdkFBMouse *mouse, gboolean *got_motion)
{
  int dx=0, dy=0;
  gboolean new_button1, new_button2, new_button3;
  guchar *buf;

  buf = mouse->mouse_packet;
      
  new_button1 = (buf[0] & 1) && 1;
  new_button3 = (buf[0] & 2) && 1;
  new_button2 = (buf[0] & 4) && 1;
  

  if (*got_motion &&
      (new_button1 != mouse->button_pressed[0] ||
       new_button2 != mouse->button_pressed[1] ||
       new_button3 != mouse->button_pressed[2]))
    {
      /* If a mouse button state changes we need to get correct ordering with enter/leave events,
	 so push those out via handle_mouse_input */
      *got_motion = FALSE;
      handle_mouse_movement (mouse);
    }

  if (new_button1 != mouse->button_pressed[0])
    {
      mouse->button_pressed[0] = new_button1; 
      send_button_event (mouse, 1, new_button1);
    }
  
  if (new_button2 != mouse->button_pressed[1])
    {
      mouse->button_pressed[1] = new_button2;
      send_button_event (mouse, 2, new_button2);
    }
  
  if (new_button3 != mouse->button_pressed[2])
    {
      mouse->button_pressed[2] = new_button3; 
      send_button_event (mouse, 3, new_button3);
    }
      
  if (buf[1] != 0)
    dx = ((buf[0] & 0x10) ? ((gint)buf[1])-256 : buf[1]);
  else
    dx = 0;
  if (buf[2] != 0)
    dy = -((buf[0] & 0x20) ? ((gint)buf[2])-256 : buf[2]);
  else
    dy = 0;
  
  mouse->x += dx;
  mouse->y += dy;
  
  if (dx || dy)
    *got_motion = TRUE;

  return TRUE;
}


static gboolean
gdk_fb_mouse_ms_open (GdkFBMouse   *mouse)
{
  gint fd;
  gint i;
  guchar buf[7];
  struct termios tty;

  fd = gdk_fb_mouse_dev_open ("/dev/ttyS0", O_RDWR);
  if (fd < 0)
    return FALSE;
  
  while ((i = read (fd, buf, sizeof(buf))) > 0)
    g_print ("Got %d bytes of junk from psaux\n", i);

  tcgetattr (fd, &tty);
  tty.c_iflag = IGNBRK | IGNPAR;
  tty.c_cflag = CREAD|CLOCAL|HUPCL|CS7|B1200;
  tty.c_oflag = 0;
  tty.c_lflag = 0;
  tty.c_line = 0;
  tty.c_cc[VTIME] = 0;
  tty.c_cc[VMIN] = 1;
  tcsetattr (fd, TCSAFLUSH, &tty);
  
  write (fd, "*n", 2);

  mouse->fd = fd;
  return TRUE;
}

static void
gdk_fb_mouse_ms_close (GdkFBMouse   *mouse)
{
  close (mouse->fd);
}

static gboolean
gdk_fb_mouse_ms_packet (GdkFBMouse   *mouse,
			gboolean     *got_motion)
{
  int dx=0, dy=0;
  gboolean new_button1, new_button2, new_button3;
  guchar *buf;

  buf = mouse->mouse_packet;

  new_button1 = (buf[0] & 0x20) && 1;
  new_button2 = (buf[1] & 0x10) && 1;
  new_button3 = 0;

  if (*got_motion &&
      (new_button1 != mouse->button_pressed[0] ||
       new_button2 != mouse->button_pressed[1] ||
       new_button3 != mouse->button_pressed[2]))
    {
      /* If a mouse button state changes we need to get correct ordering with enter/leave events,
	 so push those out via handle_mouse_input */
      *got_motion = FALSE;
      handle_mouse_movement (mouse);
    }

  if (new_button1 != mouse->button_pressed[0])
    {
      mouse->button_pressed[0] = new_button1; 
      send_button_event (mouse, 1, new_button1);
    }
  
  if (new_button2 != mouse->button_pressed[1])
    {
      mouse->button_pressed[1] = new_button2;
      send_button_event (mouse, 2, new_button2);
    }
  
  if (new_button3 != mouse->button_pressed[2])
    {
      mouse->button_pressed[2] = new_button3; 
      send_button_event (mouse, 3, new_button3);
    }

  dx = (signed char)(((buf[0] & 0x03) << 6) | (buf[1] & 0x3F));
  dy = (signed char)(((buf[0] & 0x0C) << 4) | (buf[2] & 0x3F));
  
  mouse->x += dx;
  mouse->y += dy;
  
  if (dx || dy)
    *got_motion = TRUE;

  return TRUE;
}

static gboolean
gdk_fb_mouse_fidmour_open (GdkFBMouse   *mouse)
{
  gint fd;

  fd = gdk_fb_mouse_dev_open ("/dev/fidmour", O_RDONLY);
  if (fd < 0)
    return FALSE;
  
  mouse->fd = fd;
  return TRUE;
}

static void
gdk_fb_mouse_fidmour_close (GdkFBMouse   *mouse)
{
  close (mouse->fd);
}

static gboolean
gdk_fb_mouse_fidmour_packet (GdkFBMouse   *mouse,
			     gboolean     *got_motion)
{
  int n;
  gboolean btn_down = 0;
  gdouble x = 0.0, y = 0.0;

  n = 0;
  if (!(mouse->mouse_packet[0] & 0x80))
    {
      int i;
      /* We haven't received any of the packet yet but there is no header at the beginning */
      for (i = 1; i < mouse->packet_nbytes; i++)
	{
	  if (mouse->mouse_packet[i] & 0x80)
	    {
	      n = i;
	      break;
	    }
	}
    }
  else if (mouse->packet_nbytes > 1 &&
	   ((mouse->mouse_packet[0] & 0x90) == 0x90))
    {
      /* eat the 0x90 and following byte, no clue what it's for */
      n = 2;
    }
  else
    {
      switch (mouse->mouse_packet[0] & 0xF)
	{
	case 2:
	  btn_down = 0;
	  break;
	case 1:
	case 0:
	  btn_down = 1;
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}
      
      x = mouse->mouse_packet[1] + (mouse->mouse_packet[2] << 7);
      if (x > 8192)
	x -= 16384;
      y = mouse->mouse_packet[3] + (mouse->mouse_packet[4] << 7);
      if (y > 8192)
	y -= 16384;
      /* Now map touchscreen coords to screen coords */
      x *= ((double)gdk_display->modeinfo.xres)/4096.0;
      y *= ((double)gdk_display->modeinfo.yres)/4096.0;
    }
  
  if (n)
    {
      memmove (mouse->mouse_packet, mouse->mouse_packet+n, mouse->packet_nbytes-n);
      mouse->packet_nbytes -= n;
      return FALSE;
    }

  if (btn_down != mouse->button_pressed[0])
    {
      if (*got_motion)
	{
	  /* If a mouse button state changes we need to get correct
	     ordering with enter/leave events, so push those out
	     via handle_mouse_input */
	  *got_motion = FALSE;
	  handle_mouse_movement (mouse);
	}
      
      mouse->button_pressed[0] = btn_down;
      send_button_event (mouse, 1, btn_down);
    }
  
  if (fabs(x - mouse->x) >= 1.0 || fabs(x - mouse->y) >= 1.0)
    {
      *got_motion = TRUE;
      mouse->x = x;
      mouse->y = y;
    }
  
  return TRUE;
}
