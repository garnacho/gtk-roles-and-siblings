/* GTK - The GIMP Toolkit
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdio.h>
#include "gtk/gtk.h"

/* Backing pixmap for drawing area */

static GdkPixmap *pixmap = NULL;

/* Information about cursor */

static gint cursor_proximity = TRUE;
static gdouble cursor_x;
static gdouble cursor_y;

/* Unique ID of current device */
static GdkDevice *current_device;

/* Erase the old cursor, and/or draw a new one, if necessary */
static void
update_cursor (GtkWidget *widget,  gdouble x, gdouble y)
{
  static gint cursor_present = 0;
  gint state = !current_device->has_cursor && cursor_proximity;

  if (pixmap != NULL)
    {
      if (cursor_present && (cursor_present != state ||
			     x != cursor_x || y != cursor_y))
	{
	  gdk_draw_pixmap(widget->window,
			  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			  pixmap,
			  cursor_x - 5, cursor_y - 5,
			  cursor_x - 5, cursor_y - 5,
			  10, 10);
	}

      cursor_present = state;
      cursor_x = x;
      cursor_y = y;

      if (cursor_present)
	{
	  gdk_draw_rectangle (widget->window,
			      widget->style->black_gc,
			      TRUE,
			      cursor_x - 5, cursor_y -5,
			      10, 10);
	}
    }
}

/* Create a new backing pixmap of the appropriate size */
static gint
configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
  if (pixmap)
    gdk_pixmap_unref (pixmap);
  pixmap = gdk_pixmap_new(widget->window,
			  widget->allocation.width,
			  widget->allocation.height,
			  -1);
  gdk_draw_rectangle (pixmap,
		      widget->style->white_gc,
		      TRUE,
		      0, 0,
		      widget->allocation.width,
		      widget->allocation.height);

  return TRUE;
}

/* Refill the screen from the backing pixmap */
static gint
expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  gdk_draw_pixmap(widget->window,
		  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		  pixmap,
		  event->area.x, event->area.y,
		  event->area.x, event->area.y,
		  event->area.width, event->area.height);

  return FALSE;
}

/* Draw a rectangle on the screen, size depending on pressure,
   and color on the type of device */
static void
draw_brush (GtkWidget *widget, GdkInputSource source,
	    gdouble x, gdouble y, gdouble pressure)
{
  GdkGC *gc;
  GdkRectangle update_rect;

  switch (source)
    {
    case GDK_SOURCE_MOUSE:
      gc = widget->style->dark_gc[GTK_WIDGET_STATE (widget)];
      break;
    case GDK_SOURCE_PEN:
      gc = widget->style->black_gc;
      break;
    case GDK_SOURCE_ERASER:
      gc = widget->style->white_gc;
      break;
    default:
      gc = widget->style->light_gc[GTK_WIDGET_STATE (widget)];
    }

  update_rect.x = x - 10 * pressure;
  update_rect.y = y - 10 * pressure;
  update_rect.width = 20 * pressure;
  update_rect.height = 20 * pressure;
  gdk_draw_rectangle (pixmap, gc, TRUE,
		      update_rect.x, update_rect.y,
		      update_rect.width, update_rect.height);
  gtk_widget_draw (widget, &update_rect);
}

static guint32 motion_time;

static void
print_axes (GdkDevice *device, gdouble *axes)
{
  int i;
  
  if (axes)
    {
      g_print ("%s ", device->name);
      
      for (i=0; i<device->num_axes; i++)
	g_print ("%g ", axes[i]);

      g_print ("\n");
    }
}

static gint
button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  current_device = event->device;
  cursor_proximity = TRUE;

  if (event->button == 1 && pixmap != NULL)
    {
      gdouble pressure = 0.5;

      print_axes (event->device, event->axes);
      gdk_event_get_axis ((GdkEvent *)event, GDK_AXIS_PRESSURE, &pressure);
      draw_brush (widget, event->device->source, event->x, event->y, pressure);
      
      motion_time = event->time;
    }

  update_cursor (widget, event->x, event->y);

  return TRUE;
}

static gint
key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  if ((event->keyval >= 0x20) && (event->keyval <= 0xFF))
    printf("I got a %c\n", event->keyval);
  else
    printf("I got some other key\n");

  return TRUE;
}

static gint
motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
  GdkTimeCoord **events;
  int n_events;
  int i;

  current_device = event->device;
  cursor_proximity = TRUE;

  if (event->state & GDK_BUTTON1_MASK && pixmap != NULL)
    {
      if (gdk_device_get_history (event->device, event->window, 
				  motion_time, event->time,
				  &events, &n_events))
	{
	  for (i=0; i<n_events; i++)
	    {
	      double x = 0, y = 0, pressure = 0.5;

	      gdk_device_get_axis (event->device, events[i]->axes, GDK_AXIS_X, &x);
	      gdk_device_get_axis (event->device, events[i]->axes, GDK_AXIS_Y, &y);
	      gdk_device_get_axis (event->device, events[i]->axes, GDK_AXIS_PRESSURE, &pressure);
	      draw_brush (widget,  event->device->source, x, y, pressure);

	      print_axes (event->device, events[i]->axes);
	    }
	  gdk_device_free_history (events, n_events);
	}
      else
	{
	  double pressure = 0.5;

	  gdk_event_get_axis ((GdkEvent *)event, GDK_AXIS_PRESSURE, &pressure);

	  draw_brush (widget,  event->device->source, event->x, event->y, pressure);
	}
      motion_time = event->time;
    }

  if (event->is_hint)
    gdk_device_get_state (event->device, event->window, NULL, NULL);

  print_axes (event->device, event->axes);
  update_cursor (widget, event->x, event->y);

  return TRUE;
}

/* We track the next two events to know when we need to draw a
   cursor */

static gint
proximity_out_event (GtkWidget *widget, GdkEventProximity *event)
{
  cursor_proximity = FALSE;
  update_cursor (widget, cursor_x, cursor_y);
  return TRUE;
}

static gint
leave_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
  cursor_proximity = FALSE;
  update_cursor (widget, cursor_x, cursor_y);
  return TRUE;
}

void
input_dialog_destroy (GtkWidget *w, gpointer data)
{
  *((GtkWidget **)data) = NULL;
}

void
create_input_dialog (void)
{
  static GtkWidget *inputd = NULL;

  if (!inputd)
    {
      inputd = gtk_input_dialog_new();

      gtk_signal_connect (GTK_OBJECT(inputd), "destroy",
			  (GtkSignalFunc)input_dialog_destroy, &inputd);
      gtk_signal_connect_object (GTK_OBJECT(GTK_INPUT_DIALOG(inputd)->close_button),
			  "clicked",
			  (GtkSignalFunc)gtk_widget_hide,
			  GTK_OBJECT(inputd));
      gtk_widget_hide (GTK_INPUT_DIALOG(inputd)->save_button);

      gtk_widget_show (inputd);
    }
  else
    {
      if (!GTK_WIDGET_MAPPED(inputd))
	gtk_widget_show(inputd);
      else
	gdk_window_raise(inputd->window);
    }
}

void
quit (void)
{
  gtk_exit (0);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *drawing_area;
  GtkWidget *vbox;

  GtkWidget *button;

  gtk_init (&argc, &argv);

  current_device = gdk_device_get_core_pointer ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (window, "Test Input");

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (quit), NULL);

  /* Create the drawing area */

  drawing_area = gtk_drawing_area_new ();
  gtk_drawing_area_size (GTK_DRAWING_AREA (drawing_area), 200, 200);
  gtk_box_pack_start (GTK_BOX (vbox), drawing_area, TRUE, TRUE, 0);

  gtk_widget_show (drawing_area);

  /* Signals used to handle backing pixmap */

  gtk_signal_connect (GTK_OBJECT (drawing_area), "expose_event",
		      (GtkSignalFunc) expose_event, NULL);
  gtk_signal_connect (GTK_OBJECT(drawing_area),"configure_event",
		      (GtkSignalFunc) configure_event, NULL);

  /* Event signals */

  gtk_signal_connect (GTK_OBJECT (drawing_area), "motion_notify_event",
		      (GtkSignalFunc) motion_notify_event, NULL);
  gtk_signal_connect (GTK_OBJECT (drawing_area), "button_press_event",
		      (GtkSignalFunc) button_press_event, NULL);
  gtk_signal_connect (GTK_OBJECT (drawing_area), "key_press_event",
		      (GtkSignalFunc) key_press_event, NULL);

  gtk_signal_connect (GTK_OBJECT (drawing_area), "leave_notify_event",
		      (GtkSignalFunc) leave_notify_event, NULL);
  gtk_signal_connect (GTK_OBJECT (drawing_area), "proximity_out_event",
		      (GtkSignalFunc) proximity_out_event, NULL);

  gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK
			 | GDK_LEAVE_NOTIFY_MASK
			 | GDK_BUTTON_PRESS_MASK
			 | GDK_KEY_PRESS_MASK
			 | GDK_POINTER_MOTION_MASK
			 | GDK_POINTER_MOTION_HINT_MASK
			 | GDK_PROXIMITY_OUT_MASK);

  /* The following call enables tracking and processing of extension
     events for the drawing area */
  gtk_widget_set_extension_events (drawing_area, GDK_EXTENSION_EVENTS_ALL);

  GTK_WIDGET_SET_FLAGS (drawing_area, GTK_CAN_FOCUS);
  gtk_widget_grab_focus (drawing_area);

  /* .. And create some buttons */
  button = gtk_button_new_with_label ("Input Dialog");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (create_input_dialog), NULL);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Quit");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (window));
  gtk_widget_show (button);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
