/* HSV color selector for GTK+
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Simon Budig <Simon.Budig@unix-ag.org> (original code)
 *          Federico Mena-Quintero <federico@gimp.org> (cleanup for GTK+)
 *          Jonathan Blandford <jrb@redhat.com> (cleanup for GTK+)
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

#include <config.h>
#include <math.h>
#include <string.h>
#include "gtkalias.h"
#include "gtkhsv.h"
#include "gdk/gdkkeysyms.h"
#include "gtkbindings.h"
#include "gtkcontainer.h"
#include "gtkmarshalers.h"

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/* Default width/height */
#define DEFAULT_SIZE 100

/* Default ring width */
#define DEFAULT_RING_WIDTH 10


/* Dragging modes */
typedef enum {
  DRAG_NONE,
  DRAG_H,
  DRAG_SV
} DragMode;

/* Private part of the GtkHSV structure */
typedef struct {
  /* Color value */
  double h;
  double s;
  double v;
  
  /* Size and ring width */
  int size;
  int ring_width;
  
  /* Window for capturing events */
  GdkWindow *window;
  
  /* GC for drawing */
  GdkGC *gc;
  
  /* Dragging mode */
  DragMode mode;

  guint focus_on_ring : 1;
  
} HSVPrivate;



/* Signal IDs */

enum {
  CHANGED,
  MOVE,
  LAST_SIGNAL
};

static void     gtk_hsv_class_init     (GtkHSVClass      *class);
static void     gtk_hsv_init           (GtkHSV           *hsv);
static void     gtk_hsv_destroy        (GtkObject        *object);
static void     gtk_hsv_map            (GtkWidget        *widget);
static void     gtk_hsv_unmap          (GtkWidget        *widget);
static void     gtk_hsv_realize        (GtkWidget        *widget);
static void     gtk_hsv_unrealize      (GtkWidget        *widget);
static void     gtk_hsv_size_request   (GtkWidget        *widget,
					GtkRequisition   *requisition);
static void     gtk_hsv_size_allocate  (GtkWidget        *widget,
					GtkAllocation    *allocation);
static gint     gtk_hsv_button_press   (GtkWidget        *widget,
					GdkEventButton   *event);
static gint     gtk_hsv_button_release (GtkWidget        *widget,
					GdkEventButton   *event);
static gint     gtk_hsv_motion         (GtkWidget        *widget,
					GdkEventMotion   *event);
static gint     gtk_hsv_expose         (GtkWidget        *widget,
					GdkEventExpose   *event);
static gboolean gtk_hsv_focus          (GtkWidget        *widget,
					GtkDirectionType  direction);
static void     gtk_hsv_move           (GtkHSV           *hsv,
					GtkDirectionType  dir);

static guint hsv_signals[LAST_SIGNAL];
static GtkWidgetClass *parent_class;


/**
 * gtk_hsv_get_type:
 * @void:
 *
 * Registers the &GtkHSV class if necessary, and returns the type ID associated
 * to it.
 *
 * Return value: The type ID of the &GtkHSV class.
 **/
GType
gtk_hsv_get_type (void)
{
  static GType hsv_type = 0;
  
  if (!hsv_type) {
    static const GTypeInfo hsv_info = {
      sizeof (GtkHSVClass),
      NULL,		/* base_init */
      NULL,		/* base_finalize */
      (GClassInitFunc) gtk_hsv_class_init,
      NULL,		/* class_finalize */
      NULL,		/* class_data */
      sizeof (GtkHSV),
      0,		/* n_preallocs */
      (GInstanceInitFunc) gtk_hsv_init,
    };
    
    hsv_type = g_type_register_static (GTK_TYPE_WIDGET, "GtkHSV",
				       &hsv_info, 0);
  }
  
  return hsv_type;
}

/* Class initialization function for the HSV color selector */
static void
gtk_hsv_class_init (GtkHSVClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkHSVClass    *hsv_class;
  GtkBindingSet  *binding_set;
  
  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  hsv_class = GTK_HSV_CLASS (class);
  
  parent_class = g_type_class_peek_parent (class);
  
  object_class->destroy = gtk_hsv_destroy;
  
  widget_class->map = gtk_hsv_map;
  widget_class->unmap = gtk_hsv_unmap;                                      
  widget_class->realize = gtk_hsv_realize;
  widget_class->unrealize = gtk_hsv_unrealize;
  widget_class->size_request = gtk_hsv_size_request;
  widget_class->size_allocate = gtk_hsv_size_allocate;
  widget_class->button_press_event = gtk_hsv_button_press;
  widget_class->button_release_event = gtk_hsv_button_release;
  widget_class->motion_notify_event = gtk_hsv_motion;
  widget_class->expose_event = gtk_hsv_expose;
  widget_class->focus = gtk_hsv_focus;
  
  hsv_class->move = gtk_hsv_move;
  
  hsv_signals[CHANGED] =
    g_signal_new ("changed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkHSVClass, changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  hsv_signals[MOVE] =
    g_signal_new ("move",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkHSVClass, move),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_DIRECTION_TYPE);

  binding_set = gtk_binding_set_by_class (class);

  gtk_binding_entry_add_signal (binding_set, GDK_Up, 0,
                                "move", 1,
                                G_TYPE_ENUM, GTK_DIR_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Up, 0,
                                "move", 1,
                                G_TYPE_ENUM, GTK_DIR_UP);
  
  gtk_binding_entry_add_signal (binding_set, GDK_Down, 0,
                                "move", 1,
                                G_TYPE_ENUM, GTK_DIR_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Down, 0,
                                "move", 1,
                                G_TYPE_ENUM, GTK_DIR_DOWN);

  
  gtk_binding_entry_add_signal (binding_set, GDK_Right, 0,
                                "move", 1,
                                G_TYPE_ENUM, GTK_DIR_RIGHT);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Right, 0,
                                "move", 1,
                                G_TYPE_ENUM, GTK_DIR_RIGHT);
  
  gtk_binding_entry_add_signal (binding_set, GDK_Left, 0,
                                "move", 1,
                                G_TYPE_ENUM, GTK_DIR_LEFT);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Left, 0,
                                "move", 1,
                                G_TYPE_ENUM, GTK_DIR_LEFT);
}

/* Object initialization function for the HSV color selector */
static void
gtk_hsv_init (GtkHSV *hsv)
{
  HSVPrivate *priv;
  
  priv = g_new0 (HSVPrivate, 1);
  hsv->priv = priv;
  
  GTK_WIDGET_SET_FLAGS (hsv, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (hsv, GTK_CAN_FOCUS);
  
  priv->h = 0.0;
  priv->s = 0.0;
  priv->v = 0.0;
  
  priv->size = DEFAULT_SIZE;
  priv->ring_width = DEFAULT_RING_WIDTH;
}

/* Destroy handler for the HSV color selector */
static void
gtk_hsv_destroy (GtkObject *object)
{
  GtkHSV *hsv;
  
  g_return_if_fail (GTK_IS_HSV (object));
  
  hsv = GTK_HSV (object);

  if (hsv->priv)
    {
      g_free (hsv->priv);
      hsv->priv = NULL;
    }

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

/* Default signal handlers */

    
/* Map handler for the HSV color selector */

static void
gtk_hsv_map (GtkWidget *widget)
{
  GtkHSV *hsv;
  HSVPrivate *priv;

  hsv = GTK_HSV (widget);
  priv = hsv->priv;

  GTK_WIDGET_CLASS (parent_class)->map (widget);

  gdk_window_show (priv->window);
}

/* Unmap handler for the HSV color selector */

static void
gtk_hsv_unmap (GtkWidget *widget)
{
  GtkHSV *hsv;
  HSVPrivate *priv;

  hsv = GTK_HSV (widget);
  priv = hsv->priv;

  gdk_window_hide (priv->window);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}                                                                           
                                      
/* Realize handler for the HSV color selector */
static void
gtk_hsv_realize (GtkWidget *widget)
{
  GtkHSV *hsv;
  HSVPrivate *priv;
  GdkWindowAttr attr;
  int attr_mask;
  GdkWindow *parent_window;
  
  hsv = GTK_HSV (widget);
  priv = hsv->priv;
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  
  /* Create window */
  
  attr.window_type = GDK_WINDOW_CHILD;
  attr.x = widget->allocation.x;
  attr.y = widget->allocation.y;
  attr.width = widget->allocation.width;
  attr.height = widget->allocation.height;
  attr.wclass = GDK_INPUT_ONLY;
  attr.event_mask = gtk_widget_get_events (widget);
  attr.event_mask |= (GDK_KEY_PRESS_MASK
                      | GDK_BUTTON_PRESS_MASK
		      | GDK_BUTTON_RELEASE_MASK
		      | GDK_POINTER_MOTION_MASK
                      | GDK_ENTER_NOTIFY_MASK
                      | GDK_LEAVE_NOTIFY_MASK);
  
  attr_mask = GDK_WA_X | GDK_WA_Y;
  
  parent_window = gtk_widget_get_parent_window (widget);
  
  widget->window = parent_window;
  g_object_ref (widget->window);
  
  priv->window = gdk_window_new (parent_window, &attr, attr_mask);
  gdk_window_set_user_data (priv->window, hsv);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  /* Create GC */
  
  priv->gc = gdk_gc_new (parent_window);
}

/* Unrealize handler for the HSV color selector */
static void
gtk_hsv_unrealize (GtkWidget *widget)
{
  GtkHSV *hsv;
  HSVPrivate *priv;
  
  hsv = GTK_HSV (widget);
  priv = hsv->priv;
  
  gdk_window_set_user_data (priv->window, NULL);
  gdk_window_destroy (priv->window);
  priv->window = NULL;
  
  g_object_unref (priv->gc);
  priv->gc = NULL;
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

/* Size_request handler for the HSV color selector */
static void
gtk_hsv_size_request (GtkWidget      *widget,
		      GtkRequisition *requisition)
{
  GtkHSV *hsv = GTK_HSV (widget);
  HSVPrivate *priv = hsv->priv;
  gint focus_width;
  gint focus_pad;

  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			NULL);
  
  requisition->width = priv->size + 2 * (focus_width + focus_pad);
  requisition->height = priv->size + 2 * (focus_width + focus_pad);
}

/* Size_allocate handler for the HSV color selector */
static void
gtk_hsv_size_allocate (GtkWidget     *widget,
		       GtkAllocation *allocation)
{
  GtkHSV *hsv;
  HSVPrivate *priv;
  
  hsv = GTK_HSV (widget);
  priv = hsv->priv;
  
  widget->allocation = *allocation;
  
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (priv->window,
			    allocation->x,
			    allocation->y,
			    allocation->width,
			    allocation->height);
}


/* Utility functions */

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

/* Converts from HSV to RGB */
static void
hsv_to_rgb (gdouble *h,
	    gdouble *s,
	    gdouble *v)
{
  gdouble hue, saturation, value;
  gdouble f, p, q, t;
  
  if (*s == 0.0)
    {
      *h = *v;
      *s = *v;
      *v = *v; /* heh */
    }
  else
    {
      hue = *h * 6.0;
      saturation = *s;
      value = *v;
      
      if (hue == 6.0)
	hue = 0.0;
      
      f = hue - (int) hue;
      p = value * (1.0 - saturation);
      q = value * (1.0 - saturation * f);
      t = value * (1.0 - saturation * (1.0 - f));
      
      switch ((int) hue)
	{
	case 0:
	  *h = value;
	  *s = t;
	  *v = p;
	  break;
	  
	case 1:
	  *h = q;
	  *s = value;
	  *v = p;
	  break;
	  
	case 2:
	  *h = p;
	  *s = value;
	  *v = t;
	  break;
	  
	case 3:
	  *h = p;
	  *s = q;
	  *v = value;
	  break;
	  
	case 4:
	  *h = t;
	  *s = p;
	  *v = value;
	  break;
	  
	case 5:
	  *h = value;
	  *s = p;
	  *v = q;
	  break;
	  
	default:
	  g_assert_not_reached ();
	}
    }
}

/* Converts from RGB to HSV */
static void
rgb_to_hsv (gdouble *r,
	    gdouble *g,
	    gdouble *b)
{
  gdouble red, green, blue;
  gdouble h, s, v;
  gdouble min, max;
  gdouble delta;
  
  red = *r;
  green = *g;
  blue = *b;
  
  h = 0.0;
  
  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;
      
      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;
      
      if (red < blue)
	min = red;
      else
	min = blue;
    }
  
  v = max;
  
  if (max != 0.0)
    s = (max - min) / max;
  else
    s = 0.0;
  
  if (s == 0.0)
    h = 0.0;
  else
    {
      delta = max - min;
      
      if (red == max)
	h = (green - blue) / delta;
      else if (green == max)
	h = 2 + (blue - red) / delta;
      else if (blue == max)
	h = 4 + (red - green) / delta;
      
      h /= 6.0;
      
      if (h < 0.0)
	h += 1.0;
      else if (h > 1.0)
	h -= 1.0;
    }
  
  *r = h;
  *g = s;
  *b = v;
}

/* Computes the vertices of the saturation/value triangle */
static void
compute_triangle (GtkHSV *hsv,
		  gint   *hx,
		  gint   *hy,
		  gint   *sx,
		  gint   *sy,
		  gint   *vx,
		  gint   *vy)
{
  HSVPrivate *priv;
  gdouble center;
  gdouble inner, outer;
  gdouble angle;
  
  priv = hsv->priv;
  
  center = GTK_WIDGET (hsv)->requisition.width / 2.0;
  outer = priv->size / 2.0;
  inner = outer - priv->ring_width;
  angle = priv->h * 2.0 * G_PI;
  
  *hx = floor (center + cos (angle) * inner + 0.5);
  *hy = floor (center - sin (angle) * inner + 0.5);
  *sx = floor (center + cos (angle + 2.0 * G_PI / 3.0) * inner + 0.5);
  *sy = floor (center - sin (angle + 2.0 * G_PI / 3.0) * inner + 0.5);
  *vx = floor (center + cos (angle + 4.0 * G_PI / 3.0) * inner + 0.5);
  *vy = floor (center - sin (angle + 4.0 * G_PI / 3.0) * inner + 0.5);
}

/* Computes whether a point is inside the hue ring */
static gboolean
is_in_ring (GtkHSV *hsv,
	    gdouble x,
	    gdouble y)
{
  HSVPrivate *priv;
  gdouble dx, dy, dist;
  gdouble center, inner, outer;
  
  priv = hsv->priv;
  
  center = priv->size / 2.0;
  outer = priv->size / 2.0;
  inner = outer - priv->ring_width;
  
  dx = x - center;
  dy = center - y;
  dist = dx * dx + dy * dy;
  
  return (dist >= inner * inner && dist <= outer * outer);
}

/* Computes a saturation/value pair based on the mouse coordinates */
static void
compute_sv (GtkHSV  *hsv,
	    gdouble  x,
	    gdouble  y,
	    gdouble *s,
	    gdouble *v)
{
  HSVPrivate *priv;
  int ihx, ihy, isx, isy, ivx, ivy;
  double hx, hy, sx, sy, vx, vy;
  double center;
  
  priv = hsv->priv;
  
  compute_triangle (hsv, &ihx, &ihy, &isx, &isy, &ivx, &ivy);
  center = GTK_WIDGET (hsv)->requisition.width / 2.0;
  hx = ihx - center;
  hy = center - ihy;
  sx = isx - center;
  sy = center - isy;
  vx = ivx - center;
  vy = center - ivy;
  x -= center;
  y = center - y;
  
  if (vx * (x - sx) + vy * (y - sy) < 0.0)
    {
      *s = 1.0;
      *v = (((x - sx) * (hx - sx) + (y - sy) * (hy-sy))
	    / ((hx - sx) * (hx - sx) + (hy - sy) * (hy - sy)));
      
      if (*v < 0.0)
	*v = 0.0;
      else if (*v > 1.0)
	*v = 1.0;
    }
  else if (hx * (x - sx) + hy * (y - sy) < 0.0)
    {
      *s = 0.0;
      *v = (((x - sx) * (vx - sx) + (y - sy) * (vy - sy))
	    / ((vx - sx) * (vx - sx) + (vy - sy) * (vy - sy)));
      
      if (*v < 0.0)
	*v = 0.0;
      else if (*v > 1.0)
	*v = 1.0;
    }
  else if (sx * (x - hx) + sy * (y - hy) < 0.0)
    {
      *v = 1.0;
      *s = (((x - vx) * (hx - vx) + (y - vy) * (hy - vy)) /
	    ((hx - vx) * (hx - vx) + (hy - vy) * (hy - vy)));
      
      if (*s < 0.0)
	*s = 0.0;
      else if (*s > 1.0)
	*s = 1.0;
    }
  else
    {
      *v = (((x - sx) * (hy - vy) - (y - sy) * (hx - vx))
	    / ((vx - sx) * (hy - vy) - (vy - sy) * (hx - vx)));
      
      if (*v<= 0.0)
	{
	  *v = 0.0;
	  *s = 0.0;
	}
      else
	{
	  if (*v > 1.0)
	    *v = 1.0;

	  if (fabs (hy - vy) < fabs (hx - vx))
	    *s = (x - sx - *v * (vx - sx)) / (*v * (hx - vx));
	  else
	    *s = (y - sy - *v * (vy - sy)) / (*v * (hy - vy));
	    
	  if (*s < 0.0)
	    *s = 0.0;
	  else if (*s > 1.0)
	    *s = 1.0;
	}
    }
}

/* Computes whether a point is inside the saturation/value triangle */
static gboolean
is_in_triangle (GtkHSV *hsv,
		gdouble x,
		gdouble y)
{
  int hx, hy, sx, sy, vx, vy;
  double det, s, v;
  
  compute_triangle (hsv, &hx, &hy, &sx, &sy, &vx, &vy);
  
  det = (vx - sx) * (hy - sy) - (vy - sy) * (hx - sx);
  
  s = ((x - sx) * (hy - sy) - (y - sy) * (hx - sx)) / det;
  v = ((vx - sx) * (y - sy) - (vy - sy) * (x - sx)) / det;
  
  return (s >= 0.0 && v >= 0.0 && s + v <= 1.0);
}

/* Computes a value based on the mouse coordinates */
static double
compute_v (GtkHSV *hsv,
	   gdouble x,
	   gdouble y)
{
  HSVPrivate *priv;
  double center;
  double dx, dy;
  double angle;
  
  priv = hsv->priv;
  
  center = GTK_WIDGET (hsv)->requisition.width / 2.0;
  dx = x - center;
  dy = center - y;
  
  angle = atan2 (dy, dx);
  if (angle < 0.0)
    angle += 2.0 * G_PI;
  
  return angle / (2.0 * G_PI);
}

/* Event handlers */

static void
set_cross_grab (GtkHSV *hsv,
		guint32 time)
{
  HSVPrivate *priv;
  GdkCursor *cursor;
  
  priv = hsv->priv;
  
  cursor = gdk_cursor_new_for_display (gtk_widget_get_display (GTK_WIDGET (hsv)),
				       GDK_CROSSHAIR);
  gdk_pointer_grab (priv->window, FALSE,
		    (GDK_POINTER_MOTION_MASK
		     | GDK_POINTER_MOTION_HINT_MASK
		     | GDK_BUTTON_RELEASE_MASK),
		    NULL,
		    cursor,
		    time);
  gdk_cursor_unref (cursor);
}

/* Button_press_event handler for the HSV color selector */
static gint
gtk_hsv_button_press (GtkWidget      *widget,
		      GdkEventButton *event)
{
  GtkHSV *hsv;
  HSVPrivate *priv;
  double x, y;
  
  hsv = GTK_HSV (widget);
  priv = hsv->priv;
  
  if (priv->mode != DRAG_NONE || event->button != 1)
    return FALSE;
  
  x = event->x;
  y = event->y;
  
  if (is_in_ring (hsv, x, y))
    {
      priv->mode = DRAG_H;
      set_cross_grab (hsv, event->time);
      
      gtk_hsv_set_color (hsv,
			 compute_v (hsv, x, y),
			 priv->s,
			 priv->v);

      gtk_widget_grab_focus (widget);
      priv->focus_on_ring = TRUE;
      
      return TRUE;
    }
  
  if (is_in_triangle (hsv, x, y))
    {
      gdouble s, v;
      
      priv->mode = DRAG_SV;
      set_cross_grab (hsv, event->time);
      
      compute_sv (hsv, x, y, &s, &v);
      gtk_hsv_set_color (hsv, priv->h, s, v);

      gtk_widget_grab_focus (widget);
      priv->focus_on_ring = FALSE;
      
      return TRUE;
    }
  
  return FALSE;
}

/* Button_release_event handler for the HSV color selector */
static gint
gtk_hsv_button_release (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkHSV *hsv;
  HSVPrivate *priv;
  DragMode mode;
  gdouble x, y;
  
  hsv = GTK_HSV (widget);
  priv = hsv->priv;
  
  if (priv->mode == DRAG_NONE || event->button != 1)
    return FALSE;
  
  /* Set the drag mode to DRAG_NONE so that signal handlers for "catched"
   * can see that this is the final color state.
   */
  
  mode = priv->mode;
  priv->mode = DRAG_NONE;
  
  x = event->x;
  y = event->y;
  
  if (mode == DRAG_H)
    gtk_hsv_set_color (hsv, compute_v (hsv, x, y), priv->s, priv->v);
  else if (mode == DRAG_SV) {
    double s, v;
    
    compute_sv (hsv, x, y, &s, &v);
    gtk_hsv_set_color (hsv, priv->h, s, v);
  } else
    g_assert_not_reached ();
  
  gdk_display_pointer_ungrab (gdk_drawable_get_display (event->window),
			      event->time);
  return TRUE;
}

/* Motion_notify_event handler for the HSV color selector */
static gint
gtk_hsv_motion (GtkWidget      *widget,
		GdkEventMotion *event)
{
  GtkHSV *hsv;
  HSVPrivate *priv;
  double x, y;
  gint ix, iy;
  GdkModifierType mods;
  
  hsv = GTK_HSV (widget);
  priv = hsv->priv;
  
  if (priv->mode == DRAG_NONE)
    return FALSE;
  
  if (event->is_hint)
    {
      gdk_window_get_pointer (priv->window, &ix, &iy, &mods);
      x = ix;
      y = iy;
    }
  else
    {
      x = event->x;
      y = event->y;
    }
  
  if (priv->mode == DRAG_H)
    {
      gtk_hsv_set_color (hsv, compute_v (hsv, x, y), priv->s, priv->v);
      return TRUE;
    }
  else if (priv->mode == DRAG_SV)
    {
      double s, v;
      
      compute_sv (hsv, x, y, &s, &v);
      gtk_hsv_set_color (hsv, priv->h, s, v);
      return TRUE;
    }
  
  g_assert_not_reached ();
  return FALSE;
}


/* Redrawing */

/* Paints the hue ring */
static void
paint_ring (GtkHSV      *hsv,
	    GdkDrawable *drawable,
	    gint         x,
	    gint         y,
	    gint         width,
	    gint         height)
{
  GtkWidget *widget = GTK_WIDGET (hsv);
  HSVPrivate *priv;
  int xx, yy;
  gdouble dx, dy, dist;
  gdouble center;
  gdouble inner, outer;
  guchar *buf, *p;
  gdouble angle;
  gdouble hue;
  gdouble r, g, b;
  GdkBitmap *mask;
  GdkGC *gc;
  GdkColor color;
  gint focus_width;
  gint focus_pad;

  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			NULL);
  
  priv = hsv->priv;
  
  center = widget->requisition.width / 2.0;
  
  outer = priv->size / 2.0;
  inner = outer - priv->ring_width;
  
  /* Paint the ring */
  
  buf = g_new (guchar, width * height * 3);
  
  for (yy = 0; yy < height; yy++)
    {
      p = buf + yy * width * 3;
      
      dy = -(yy + y - center);
      
      for (xx = 0; xx < width; xx++)
	{
	  dx = xx + x - center;
	  
	  dist = dx * dx + dy * dy;
	  if (dist < ((inner-1) * (inner-1)) || dist > ((outer+1) * (outer+1)))
	    {
	      *p++ = 0;
	      *p++ = 0;
	      *p++ = 0;
	      continue;
	    }
	  
	  angle = atan2 (dy, dx);
	  if (angle < 0.0)
	    angle += 2.0 * G_PI;
	  
	  hue = angle / (2.0 * G_PI);
	  
	  r = hue;
	  g = 1.0;
	  b = 1.0;
	  hsv_to_rgb (&r, &g, &b);
	  
	  *p++ = floor (r * 255 + 0.5);
	  *p++ = floor (g * 255 + 0.5);
	  *p++ = floor (b * 255 + 0.5);
	}
    }
  
  /* Create clipping mask */
  
  mask = gdk_pixmap_new (widget->window, width, height, 1);

  gc = gdk_gc_new (mask);
  
  color.pixel = 0;
  gdk_gc_set_foreground (gc, &color);
  gdk_draw_rectangle (mask, gc, TRUE,
		      0, 0, width, height);
  
  
  color.pixel = 1;
  gdk_gc_set_foreground (gc, &color);
  gdk_draw_arc (mask, gc, TRUE,
		focus_width + focus_pad - x, 
		focus_width + focus_pad - y,
		priv->size - 1, priv->size - 1,
		0, 360 * 64);
  
  color.pixel = 0;
  gdk_gc_set_foreground (gc, &color);
  gdk_draw_arc (mask, gc, TRUE,
		focus_width + focus_pad - x + priv->ring_width - 1, 
		focus_width + focus_pad - y + priv->ring_width - 1,
		priv->size - 2 * priv->ring_width + 1, priv->size - 2 * priv->ring_width + 1,
		0, 360 * 64);
  
  g_object_unref (gc);
  
  gdk_gc_set_clip_mask (priv->gc, mask);
  gdk_gc_set_clip_origin (priv->gc, 0, 0);
  
  /* Draw ring */
  
  gdk_draw_rgb_image_dithalign (drawable, priv->gc, 0, 0, width, height,
				GDK_RGB_DITHER_MAX,
				buf,
				width * 3,
				x, y);
  
  /* Draw value marker */
  
  r = priv->h;
  g = 1.0;
  b = 1.0;
  hsv_to_rgb (&r, &g, &b);
  
  if (INTENSITY (r, g, b) > 0.5)
    {
      color.red = 0x0000;
      color.green = 0x0000;
      color.blue = 0x0000;
    }
  else
    {
      color.red = 0xffff;
      color.green = 0xffff;
      color.blue = 0xffff;
    }

  gdk_gc_set_rgb_fg_color (priv->gc, &color);
  
  gdk_draw_line (drawable, priv->gc,
		 -x + center, -y + center,
		 -x + center + cos (priv->h * 2.0 * G_PI) * center,
		 -y + center - sin (priv->h * 2.0 * G_PI) * center);
  
  gdk_gc_set_clip_mask (priv->gc, NULL);
  g_object_unref (mask);
  
  g_free (buf);
  
  /* Draw ring outline */
}

/* Converts an HSV triplet to an integer RGB triplet */
static void
get_color (gdouble h,
	   gdouble s,
	   gdouble v,
	   gint   *r,
	   gint   *g,
	   gint   *b)
{
  hsv_to_rgb (&h, &s, &v);
  
  *r = floor (h * 255 + 0.5);
  *g = floor (s * 255 + 0.5);
  *b = floor (v * 255 + 0.5);
}

#define SWAP(a, b, t) ((t) = (a), (a) = (b), (b) = (t))

#define LERP(a, b, v1, v2, i) (((v2) - (v1) != 0)					\
			       ? ((a) + ((b) - (a)) * ((i) - (v1)) / ((v2) - (v1)))	\
			       : (a))

/* Paints the HSV triangle */
static void
paint_triangle (GtkHSV      *hsv,
		GdkDrawable *drawable,
		gint         x,
		gint         y,
		gint         width,
		gint         height)
{
  GtkWidget *widget = GTK_WIDGET (hsv);
  HSVPrivate *priv;
  gint hx, hy, sx, sy, vx, vy; /* HSV vertices */
  gint x1, y1, r1, g1, b1; /* First vertex in scanline order */
  gint x2, y2, r2, g2, b2; /* Second vertex */
  gint x3, y3, r3, g3, b3; /* Third vertex */
  gint t;
  guchar *buf, *p;
  gint xl, xr, rl, rr, gl, gr, bl, br; /* Scanline data */
  gint xx, yy;
  GdkBitmap *mask;
  GdkGC *gc;
  GdkColor color;
  GdkPoint points[3];
  gdouble r, g, b;
  gchar *detail;
  
  priv = hsv->priv;
  
  /* Compute triangle's vertices */
  
  compute_triangle (hsv, &hx, &hy, &sx, &sy, &vx, &vy);
  
  x1 = hx;
  y1 = hy;
  get_color (priv->h, 1.0, 1.0, &r1, &g1, &b1);
  
  x2 = sx;
  y2 = sy;
  get_color (priv->h, 1.0, 0.0, &r2, &g2, &b2);
  
  x3 = vx;
  y3 = vy;
  get_color (priv->h, 0.0, 1.0, &r3, &g3, &b3);
  
  if (y2 > y3)
    {
      SWAP (x2, x3, t);
      SWAP (y2, y3, t);
      SWAP (r2, r3, t);
      SWAP (g2, g3, t);
      SWAP (b2, b3, t);
    }
  
  if (y1 > y3)
    {
      SWAP (x1, x3, t);
      SWAP (y1, y3, t);
      SWAP (r1, r3, t);
      SWAP (g1, g3, t);
      SWAP (b1, b3, t);
    }
  
  if (y1 > y2)
    {
      SWAP (x1, x2, t);
      SWAP (y1, y2, t);
      SWAP (r1, r2, t);
      SWAP (g1, g2, t);
      SWAP (b1, b2, t);
    }
  
  /* Shade the triangle */
  
  buf = g_new (guchar, width * height * 3);
  
  for (yy = 0; yy < height; yy++)
    {
      p = buf + yy * width * 3;
      
      if (yy + y < y1 || yy + y > y3)
	for (xx = 0; xx < width; xx++)
	  {
	    *p++ = 0;
	    *p++ = 0;
	    *p++ = 0;
	  }
      else {
	if (yy + y < y2)
	  {
	    xl = LERP (x1, x2, y1, y2, yy + y);
	    
	    rl = LERP (r1, r2, y1, y2, yy + y);
	    gl = LERP (g1, g2, y1, y2, yy + y);
	    bl = LERP (b1, b2, y1, y2, yy + y);
	  }
	else
	  {
	    xl = LERP (x2, x3, y2, y3, yy + y);
	    
	    rl = LERP (r2, r3, y2, y3, yy + y);
	    gl = LERP (g2, g3, y2, y3, yy + y);
	    bl = LERP (b2, b3, y2, y3, yy + y);
	  }
	
	xr = LERP (x1, x3, y1, y3, yy + y);
	
	rr = LERP (r1, r3, y1, y3, yy + y);
	gr = LERP (g1, g3, y1, y3, yy + y);
	br = LERP (b1, b3, y1, y3, yy + y);
	
	if (xl > xr)
	  {
	    SWAP (xl, xr, t);
	    SWAP (rl, rr, t);
	    SWAP (gl, gr, t);
	    SWAP (bl, br, t);
	  }
	
	for (xx = 0; xx < width; xx++)
	  {
	    if (xx + x < xl || xx + x > xr)
	      {
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
	      }
	    else
	      {
		*p++ = LERP (rl, rr, xl, xr, xx + x);
		*p++ = LERP (gl, gr, xl, xr, xx + x);
		*p++ = LERP (bl, br, xl, xr, xx + x);
	      }
	  }
      }
    }
  
  /* Create clipping mask */
  
  mask = gdk_pixmap_new (widget->window, width, height, 1);

  gc = gdk_gc_new (mask);
  
  color.pixel = 0;
  gdk_gc_set_foreground (gc, &color);
  gdk_draw_rectangle (mask, gc, TRUE,
		      0, 0, width, height);
  
  color.pixel = 1;
  gdk_gc_set_foreground (gc, &color);
  
  points[0].x = x1 - x;
  points[0].y = y1 - y;
  points[1].x = x2 - x;
  points[1].y = y2 - y;
  points[2].x = x3 - x;
  points[2].y = y3 - y;
  gdk_draw_polygon (mask, gc, TRUE, points, 3);
  
  g_object_unref (gc);
  
  gdk_gc_set_clip_mask (priv->gc, mask);
  gdk_gc_set_clip_origin (priv->gc, 0, 0);
  
  /* Draw triangle */
  
  gdk_draw_rgb_image_dithalign (drawable, priv->gc, 0, 0, width, height,
				GDK_RGB_DITHER_MAX,
				buf,
				width * 3,
				x, y);
  
  gdk_gc_set_clip_mask (priv->gc, NULL);
  g_object_unref (mask);
  
  g_free (buf);
  
  /* Draw value marker */
  
  xx = floor (sx + (vx - sx) * priv->v + (hx - vx) * priv->s * priv->v + 0.5) - x;
  yy = floor (sy + (vy - sy) * priv->v + (hy - vy) * priv->s * priv->v + 0.5) - y;
  
  r = priv->h;
  g = priv->s;
  b = priv->v;
  hsv_to_rgb (&r, &g, &b);

  if (INTENSITY (r, g, b) > 0.5)
    {
      detail = "colorwheel_light";
      color.red = 0x0000;
      color.green = 0x0000;
      color.blue = 0x0000;
    }
  else
    {
      detail = "colorwheel_dark";
      color.red = 0xffff;
      color.green = 0xffff;
      color.blue = 0xffff;
    }

  gdk_gc_set_rgb_fg_color (priv->gc, &color);

#define OUTER_RADIUS 4
#define INNER_RADIUS 3 
#define FOCUS_RADIUS 6
  
  gdk_draw_arc (drawable, priv->gc, FALSE,
		xx - OUTER_RADIUS, yy - OUTER_RADIUS,
		OUTER_RADIUS * 2, OUTER_RADIUS * 2,
		0, 360 * 64);
  gdk_draw_arc (drawable, priv->gc, FALSE,
		xx - INNER_RADIUS, yy - INNER_RADIUS,
		INNER_RADIUS * 2, INNER_RADIUS * 2,
		0, 360 * 64);

  /* Draw focus outline */

  if (GTK_WIDGET_HAS_FOCUS (hsv) &&
      !priv->focus_on_ring)
    {
      gint focus_width;
      gint focus_pad;

      gtk_widget_style_get (widget,
			    "focus-line-width", &focus_width,
			    "focus-padding", &focus_pad,
			    NULL);
  
      gtk_paint_focus (widget->style, drawable,
		       GTK_WIDGET_STATE (widget),
		       NULL, widget, detail,
		       xx - FOCUS_RADIUS - focus_width - focus_pad, 
		       yy - FOCUS_RADIUS - focus_width - focus_pad, 
		       2 * (FOCUS_RADIUS + focus_width + focus_pad), 
		       2 * (FOCUS_RADIUS + focus_width + focus_pad));
    }
  
}

/* Paints the contents of the HSV color selector */
static void
paint (GtkHSV      *hsv,
       GdkDrawable *drawable,
       gint         x,
       gint         y,
       gint         width,
       gint         height)
{
  paint_ring (hsv, drawable, x, y, width, height);
  paint_triangle (hsv, drawable, x, y, width, height);
}

/* Expose_event handler for the HSV color selector */
static gint
gtk_hsv_expose (GtkWidget      *widget,
		GdkEventExpose *event)
{
  GtkHSV *hsv;
  HSVPrivate *priv;
  GdkRectangle rect, dest;
  GdkPixmap *pixmap;
  
  hsv = GTK_HSV (widget);
  priv = hsv->priv;
  
  if (!(GTK_WIDGET_DRAWABLE (widget) && event->window == widget->window))
    return FALSE;
  
  rect.x = widget->allocation.x;
  rect.y = widget->allocation.y;
  rect.width = widget->allocation.width;
  rect.height = widget->allocation.height;
  
  if (!gdk_rectangle_intersect (&event->area, &rect, &dest))
    return FALSE;
  
  pixmap = gdk_pixmap_new (widget->window, dest.width, dest.height,
			   gtk_widget_get_visual (widget)->depth);
  
  rect = dest;
  rect.x = 0;
  rect.y = 0;
  
  gdk_draw_rectangle (pixmap,
		      widget->style->bg_gc[GTK_WIDGET_STATE (widget)],
		      TRUE,
		      0, 0, dest.width, dest.height);
  paint (hsv, pixmap,
	 dest.x - widget->allocation.x, dest.y - widget->allocation.y,
	 dest.width, dest.height);
  
  gdk_draw_drawable (widget->window,
		     priv->gc,
		     pixmap,
		     0, 0,
		     dest.x,
		     dest.y,
		     event->area.width, event->area.height);
  
  if (GTK_WIDGET_HAS_FOCUS (hsv) && priv->focus_on_ring)
    gtk_paint_focus (widget->style, widget->window,
		     GTK_WIDGET_STATE (widget),
		     &event->area, widget, NULL,
		     widget->allocation.x,
		     widget->allocation.y, 
		     widget->allocation.width, 
		     widget->allocation.height);

  g_object_unref (pixmap);
  
  return FALSE;
}

static gboolean
gtk_hsv_focus (GtkWidget       *widget,
               GtkDirectionType dir)
{
  GtkHSV *hsv;
  HSVPrivate *priv;

  hsv = GTK_HSV (widget);
  priv = hsv->priv;

  if (!GTK_WIDGET_HAS_FOCUS (hsv))
    {
      if (dir == GTK_DIR_TAB_BACKWARD)
        priv->focus_on_ring = FALSE;
      else
        priv->focus_on_ring = TRUE;

      gtk_widget_grab_focus (GTK_WIDGET (hsv));
      return TRUE;
    }
  
  switch (dir)
    {
    case GTK_DIR_UP:
      if (priv->focus_on_ring)
        return FALSE;
      else
        priv->focus_on_ring = TRUE;
      break;

    case GTK_DIR_DOWN:
      if (priv->focus_on_ring)
        priv->focus_on_ring = FALSE;
      else
        return FALSE;
      break;

    case GTK_DIR_LEFT:
    case GTK_DIR_TAB_BACKWARD:
      if (priv->focus_on_ring)
        return FALSE;
      else
        priv->focus_on_ring = TRUE;
      break;

    case GTK_DIR_RIGHT:
    case GTK_DIR_TAB_FORWARD:
      if (priv->focus_on_ring)
        priv->focus_on_ring = FALSE;
      else
        return FALSE;
      break;
    }

  gtk_widget_queue_draw (GTK_WIDGET (hsv));

  return TRUE;
}

/**
 * gtk_hsv_new:
 * @void:
 *
 * Creates a new HSV color selector.
 *
 * Return value: A newly-created HSV color selector.
 **/
GtkWidget*
gtk_hsv_new (void)
{
  return g_object_new (GTK_TYPE_HSV, NULL);
}

/**
 * gtk_hsv_set_color:
 * @hsv: An HSV color selector.
 * @h: Hue.
 * @s: Saturation.
 * @v: Value.
 *
 * Sets the current color in an HSV color selector.  Color component values must
 * be in the [0.0, 1.0] range.
 **/
void
gtk_hsv_set_color (GtkHSV *hsv,
		   gdouble h,
		   gdouble s,
		   gdouble v)
{
  HSVPrivate *priv;
  
  g_return_if_fail (hsv != NULL);
  g_return_if_fail (GTK_IS_HSV (hsv));
  g_return_if_fail (h >= 0.0 && h <= 1.0);
  g_return_if_fail (s >= 0.0 && s <= 1.0);
  g_return_if_fail (v >= 0.0 && v <= 1.0);
  
  priv = hsv->priv;
  
  priv->h = h;
  priv->s = s;
  priv->v = v;
  
  g_signal_emit (hsv, hsv_signals[CHANGED], 0);
  
  gtk_widget_queue_draw (GTK_WIDGET (hsv));
}

/**
 * gtk_hsv_get_color:
 * @hsv: An HSV color selector.
 * @h: Return value for the hue.
 * @s: Return value for the saturation.
 * @v: Return value for the value.
 *
 * Queries the current color in an HSV color selector.  Returned values will be
 * in the [0.0, 1.0] range.
 **/
void
gtk_hsv_get_color (GtkHSV *hsv, double *h, double *s, double *v)
{
  HSVPrivate *priv;
  
  g_return_if_fail (GTK_IS_HSV (hsv));
  
  priv = hsv->priv;
  
  if (h)
    *h = priv->h;
  
  if (s)
    *s = priv->s;
  
  if (v)
    *v = priv->v;
}

/**
 * gtk_hsv_set_metrics:
 * @hsv: An HSV color selector.
 * @size: Diameter for the hue ring.
 * @ring_width: Width of the hue ring.
 *
 * Sets the size and ring width of an HSV color selector.
 **/
void
gtk_hsv_set_metrics (GtkHSV *hsv,
		     gint    size,
		     gint    ring_width)
{
  HSVPrivate *priv;
  int same_size;
  
  g_return_if_fail (GTK_IS_HSV (hsv));
  g_return_if_fail (size > 0);
  g_return_if_fail (ring_width > 0);
  g_return_if_fail (2 * ring_width + 1 <= size);
  
  priv = hsv->priv;
  
  same_size = (priv->size == size);
  
  priv->size = size;
  priv->ring_width = ring_width;
  
  if (same_size)
    gtk_widget_queue_draw (GTK_WIDGET (hsv));
  else
    gtk_widget_queue_resize (GTK_WIDGET (hsv));
}

/**
 * gtk_hsv_get_metrics:
 * @hsv: An HSV color selector.
 * @size: Return value for the diameter of the hue ring.
 * @ring_width: Return value for the width of the hue ring.
 *
 * Queries the size and ring width of an HSV color selector.
 **/
void
gtk_hsv_get_metrics (GtkHSV *hsv,
		     gint   *size,
		     gint   *ring_width)
{
  HSVPrivate *priv;
  
  g_return_if_fail (GTK_IS_HSV (hsv));
  
  priv = hsv->priv;
  
  if (size)
    *size = priv->size;
  
  if (ring_width)
    *ring_width = priv->ring_width;
}

/**
 * gtk_hsv_is_adjusting:
 * @hsv:
 *
 * An HSV color selector can be said to be adjusting if multiple rapid changes
 * are being made to its value, for example, when the user is adjusting the
 * value with the mouse.  This function queries whether the HSV color selector
 * is being adjusted or not.
 *
 * Return value: TRUE if clients can ignore changes to the color value, since
 * they may be transitory, or FALSE if they should consider the color value
 * status to be final.
 **/
gboolean
gtk_hsv_is_adjusting (GtkHSV *hsv)
{
  HSVPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_HSV (hsv), FALSE);
  
  priv = hsv->priv;

  return priv->mode != DRAG_NONE;
}

/**
 * gtk_hsv_to_rgb:
 * @h: Hue.
 * @s: Saturation.
 * @v: Value.
 * @r: Return value for the red component.
 * @g: Return value for the green component.
 * @b: Return value for the blue component.
 * 
 * Converts a color from HSV space to RGB.  Input values must be in the
 * [0.0, 1.0] range; output values will be in the same range.
 **/
void
gtk_hsv_to_rgb (gdouble  h,
		gdouble  s,
		gdouble  v,
		gdouble *r,
		gdouble *g,
		gdouble *b)
{
  g_return_if_fail (h >= 0.0 && h <= 1.0);
  g_return_if_fail (s >= 0.0 && s <= 1.0);
  g_return_if_fail (v >= 0.0 && v <= 1.0);
  
  hsv_to_rgb (&h, &s, &v);
  
  if (r)
    *r = h;
  
  if (g)
    *g = s;
  
  if (b)
    *b = v;
}

/**
 * gtk_hsv_to_rgb:
 * @r: Red.
 * @g: Green.
 * @b: Blue.
 * @h: Return value for the hue component.
 * @s: Return value for the saturation component.
 * @v: Return value for the value component.
 * 
 * Converts a color from RGB space to HSV.  Input values must be in the
 * [0.0, 1.0] range; output values will be in the same range.
 **/
void
gtk_rgb_to_hsv (gdouble  r,
		gdouble  g,
		gdouble  b,
		gdouble *h,
		gdouble *s,
		gdouble *v)
{
  g_return_if_fail (r >= 0.0 && r <= 1.0);
  g_return_if_fail (g >= 0.0 && g <= 1.0);
  g_return_if_fail (b >= 0.0 && b <= 1.0);
  
  rgb_to_hsv (&r, &g, &b);
  
  if (h)
    *h = r;
  
  if (s)
    *s = g;
  
  if (v)
    *v = b;
}

static void
gtk_hsv_move (GtkHSV          *hsv,
              GtkDirectionType dir)
{
  HSVPrivate *priv;
  gdouble hue, sat, val;
  gint hx, hy, sx, sy, vx, vy; /* HSV vertices */
  gint x, y; /* position in triangle */
  
  priv = hsv->priv;

  hue = priv->h;
  sat = priv->s;
  val = priv->v;

  compute_triangle (hsv, &hx, &hy, &sx, &sy, &vx, &vy);

  x = floor (sx + (vx - sx) * priv->v + (hx - vx) * priv->s * priv->v + 0.5);
  y = floor (sy + (vy - sy) * priv->v + (hy - vy) * priv->s * priv->v + 0.5);

#define HUE_DELTA 0.002
  switch (dir)
    {
    case GTK_DIR_UP:
      if (priv->focus_on_ring)
        hue += HUE_DELTA;
      else
        {
          y -= 1;
          compute_sv (hsv, x, y, &sat, &val);
        }
      break;

    case GTK_DIR_DOWN:
      if (priv->focus_on_ring)
        hue -= HUE_DELTA;
      else
        {
          y += 1;
          compute_sv (hsv, x, y, &sat, &val);
        }
      break;

    case GTK_DIR_LEFT:
      if (priv->focus_on_ring)
        hue += HUE_DELTA;
      else
        {
          x -= 1;
          compute_sv (hsv, x, y, &sat, &val);
        }
      break;

    case GTK_DIR_RIGHT:
      if (priv->focus_on_ring)
        hue -= HUE_DELTA
          ;
      else
        {
          x += 1;
          compute_sv (hsv, x, y, &sat, &val);
        }
      break;

    default:
      /* we don't care about the tab directions */
      break;
    }

  /* Wrap */
  if (hue < 0.0)
    hue = 1.0;
  else if (hue > 1.0)
    hue = 0.0;
  
  gtk_hsv_set_color (hsv, hue, sat, val);
}
