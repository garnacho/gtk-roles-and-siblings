/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "gdkconfig.h"

#if defined (GDK_WINDOWING_X11)
#include "x11/gdkx.h"
#elif defined (GDK_WINDOWING_WIN32)
#include "win32/gdkwin32.h"
#elif defined (GDK_WINDOWING_FB)
#include "linux-fb/gdkfb.h"
#endif

#include "gdk/gdkkeysyms.h"
#include "gtkcolorsel.h"
#include "gtkhsv.h"
#include "gtkwindow.h"
#include "gtkselection.h"
#include "gtkdnd.h"
#include "gtkdrawingarea.h"
#include "gtksignal.h"
#include "gtkhbox.h"
#include "gtkhbbox.h"
#include "gtkrc.h"
#include "gtkframe.h"
#include "gtktable.h"
#include "gtklabel.h"
#include "gtkpixmap.h"
#include "gtkspinbutton.h"
#include "gtkrange.h"
#include "gtkhscale.h"
#include "gtkentry.h"
#include "gtkbutton.h"
#include "gtkhseparator.h"
#include "gtkintl.h"

/* Number of elements in the custom palatte */
#define GTK_CUSTOM_PALETTE_WIDTH 10
#define GTK_CUSTOM_PALETTE_HEIGHT 2

enum {
  COLOR_CHANGED,
  LAST_SIGNAL
};

enum {
  COLORSEL_RED = 0,
  COLORSEL_GREEN = 1,
  COLORSEL_BLUE = 2,
  COLORSEL_OPACITY = 3,
  COLORSEL_HUE,
  COLORSEL_SATURATION,
  COLORSEL_VALUE,
  COLORSEL_NUM_CHANNELS
};

typedef struct _ColorSelectionPrivate ColorSelectionPrivate;

struct _ColorSelectionPrivate
{
  guint use_opacity : 1;
  guint use_palette : 1;
  guint changing : 1;
  guint default_set : 1;
  
  /* The color dropper */
  guint moving_dropper : 1;
  
  gdouble color[COLORSEL_NUM_CHANNELS];
  gdouble old_color[COLORSEL_NUM_CHANNELS];
  
  GtkWidget *triangle_colorsel;
  GtkWidget *hue_spinbutton;
  GtkWidget *sat_spinbutton;
  GtkWidget *val_spinbutton;
  GtkWidget *red_spinbutton;
  GtkWidget *green_spinbutton;
  GtkWidget *blue_spinbutton;
  GtkWidget *opacity_slider;
  GtkWidget *opacity_label;
  GtkWidget *opacity_entry;
  GtkWidget *palette_frame;
  GtkWidget *hex_entry;
  
  /* The Palette code */
  GtkWidget *custom_palette [GTK_CUSTOM_PALETTE_WIDTH][GTK_CUSTOM_PALETTE_HEIGHT];
  GtkWidget *last_palette;
  
  /* The color_sample stuff */
  GtkWidget *sample_area;
  GtkWidget *old_sample;
  GtkWidget *cur_sample;
  GtkWidget *colorsel;
};


static void gtk_color_selection_init		(GtkColorSelection	 *colorsel);
static void gtk_color_selection_class_init	(GtkColorSelectionClass	 *klass);
static void gtk_color_selection_destroy		(GtkObject		 *object);
static void update_color			(GtkColorSelection	 *colorsel);

static gpointer parent_class = NULL;
static guint color_selection_signals[LAST_SIGNAL] = { 0 };


/* The cursor for the dropper */
#define DROPPER_WIDTH 17
#define DROPPER_HEIGHT 17
#define DROPPER_X_HOT 2
#define DROPPER_Y_HOT 16


static char dropper_bits[] = {
  0xff, 0x8f, 0x01, 0xff, 0x77, 0x01, 0xff, 0xfb, 0x00, 0xff, 0xf8, 0x00,
  0x7f, 0xff, 0x00, 0xff, 0x7e, 0x01, 0xff, 0x9d, 0x01, 0xff, 0xd8, 0x01,
  0x7f, 0xd4, 0x01, 0x3f, 0xee, 0x01, 0x1f, 0xff, 0x01, 0x8f, 0xff, 0x01,
  0xc7, 0xff, 0x01, 0xe3, 0xff, 0x01, 0xf3, 0xff, 0x01, 0xfd, 0xff, 0x01,
  0xff, 0xff, 0x01, };

static char dropper_mask[] = {
  0x00, 0x70, 0x00, 0x00, 0xf8, 0x00, 0x00, 0xfc, 0x01, 0x00, 0xff, 0x01,
  0x80, 0xff, 0x01, 0x00, 0xff, 0x00, 0x00, 0x7f, 0x00, 0x80, 0x3f, 0x00,
  0xc0, 0x3f, 0x00, 0xe0, 0x13, 0x00, 0xf0, 0x01, 0x00, 0xf8, 0x00, 0x00,
  0x7c, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x0d, 0x00, 0x00,
  0x02, 0x00, 0x00, };

static GdkCursor *picker_cursor = NULL;


/* XPM */
static char *picker[] = {
  /* columns rows colors chars-per-pixel */
  "25 25 8 1",
  "  c Gray0",
  ". c #020202",
  "X c Gray12",
  "o c Gray13",
  "O c Gray52",
  "+ c #929292",
  "@ c Gray100",
  "# c None",
  /* pixels */
  "#########################",
  "#########################",
  "#########################",
  "#########################",
  "#########################",
  "#################   #####",
  "################     ####",
  "################     +###",
  "#############        +###",
  "##############      ++###",
  "#############+@   +++####",
  "############+@@@  +######",
  "###########+@@@ + +######",
  "##########+@@@ ++#+######",
  "#########+@@@ ++#########",
  "########+@@@ ++##########",
  "#######+@@@ ++###########",
  "######+@@@ ++############",
  "######+@@ ++#############",
  "#####+@  ++##############",
  "###### +++###############",
  "#########################",
  "#########################",
  "#########################",
  "#########################"
};


/*
 *
 * The Sample Color
 *
 */
#define SAMPLE_WIDTH  64
#define SAMPLE_HEIGHT 28

static void color_sample_draw_sample (GtkColorSelection *colorsel, int which);
static void color_sample_draw_samples (GtkColorSelection *colorsel);

static void
color_sample_drag_begin (GtkWidget      *widget,
			 GdkDragContext *context,
			 gpointer        data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  GtkWidget *window;
  gdouble colors[4];
  gdouble *colsrc;
  GdkColor bg;
  gint n, i;
  
  priv = colorsel->private_data;
  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
  gtk_widget_set_usize (window, 48, 32);
  gtk_widget_realize (window);
  gtk_object_set_data_full (GTK_OBJECT (widget),
			    "gtk-color-selection-drag-window",
			    window,
			    (GtkDestroyNotify) gtk_widget_destroy);
  
  if (widget == priv->old_sample)
    colsrc = priv->old_color;
  else
    colsrc = priv->color;
  
  for (i=0, n = COLORSEL_RED; n <= COLORSEL_BLUE; n++)
    {
      colors[i++] = colsrc[n];
    }
  
  if (priv->use_opacity)
    {
      colors[i] = colsrc[COLORSEL_OPACITY];
    }
  
  bg.red = 0xffff * colors[0];
  bg.green = 0xffff * colors[1];
  bg.blue = 0xffff * colors[2];
  
  gdk_color_alloc (gtk_widget_get_colormap (window), &bg);
  gdk_window_set_background (window->window, &bg);
  
  gtk_drag_set_icon_widget (context, window, -2, -2);
}

static void
color_sample_drag_end (GtkWidget      *widget,
		       GdkDragContext *context,
		       gpointer        data)
{
  gtk_object_set_data (GTK_OBJECT (widget), "gtk-color-selection-drag-window", NULL);
}

static void
color_sample_drop_handle (GtkWidget        *widget,
			  GdkDragContext   *context,
			  gint              x,
			  gint              y,
			  GtkSelectionData *selection_data,
			  guint             info,
			  guint             time,
			  gpointer          data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  guint16 *vals;
  gdouble color[4];
  priv = colorsel->private_data;
  
  /* This is currently a guint16 array of the format:
   * R
   * G
   * B
   * opacity
   */
  
  if (selection_data->length < 0)
    return;
  
  if ((selection_data->format != 16) ||
      (selection_data->length != 8))
    {
      g_warning ("Received invalid color data\n");
      return;
    }
  
  vals = (guint16 *)selection_data->data;
  
  if (widget == priv->cur_sample)
    {
      color[0] = (gdouble)vals[0] / 0xffff;
      color[1] = (gdouble)vals[1] / 0xffff;
      color[2] = (gdouble)vals[2] / 0xffff;
      color[3] = (gdouble)vals[3] / 0xffff;
      
      gtk_color_selection_set_color (colorsel, color);
    }
}

static void
color_sample_drag_handle (GtkWidget        *widget,
			  GdkDragContext   *context,
			  GtkSelectionData *selection_data,
			  guint             info,
			  guint             time,
			  gpointer          data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  guint16 vals[4];
  gdouble *colsrc;
  
  priv = colorsel->private_data;
  
  if (widget == priv->old_sample)
    colsrc = priv->old_color;
  else
    colsrc = priv->color;
  
  vals[0] = colsrc[COLORSEL_RED] * 0xffff;
  vals[1] = colsrc[COLORSEL_GREEN] * 0xffff;
  vals[2] = colsrc[COLORSEL_BLUE] * 0xffff;
  vals[3] = priv->use_opacity ? colsrc[COLORSEL_OPACITY] * 0xffff : 0xffff;
  
  gtk_selection_data_set (selection_data,
			  gdk_atom_intern ("application/x-color", FALSE),
			  16, (guchar *)vals, 8);
}

/* which = 0 means draw old sample, which = 1 means draw new */
static void
color_sample_draw_sample (GtkColorSelection *colorsel, int which)
{
  GtkWidget *da;
  gint x, y, i, wid, heig, f, n, goff;
  guchar c[3 * 2], cc[3 * 4], *cp = c;
  gdouble o;
  guchar *buf;
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (colorsel != NULL);
  priv = colorsel->private_data;
  
  g_return_if_fail (priv->sample_area != NULL);
  if (!GTK_WIDGET_DRAWABLE (priv->sample_area))
    return;
  
  if (which == 0)
    {
      da = priv->old_sample;
      for (n = 0, i = COLORSEL_RED; n < 3; n++, i++)
	c[n] = (guchar) (255.0 * priv->old_color[i]);
      goff = 0;
    }
  else
    {
      da = priv->cur_sample;
      for (n = 0, i = COLORSEL_RED; n < 3; n++, i++)
	c[n] = (guchar) (255.0 * priv->color[i]);
      goff =  priv->old_sample->allocation.width % 32;
    }
  
  wid = da->allocation.width;
  heig = da->allocation.height;
  
  buf = g_new(guchar, 3 * wid * heig);
  
#if 0
  i = COLORSEL_RED;
  for (n = 0; n < 3; n++)
    {
      c[n] = (guchar) (255.0 * priv->old_color[i]);
      c[n + 3] = (guchar) (255.0 * priv->color[i++]);
    }
#endif
  
  if (priv->use_opacity)
    {
      o = (which) ? priv->color[COLORSEL_OPACITY] : priv->old_color[COLORSEL_OPACITY];
      
      for (n = 0; n < 3; n++)
	{
	  cc[n] = (guchar) ((1.0 - o) * 192 + (o * (gdouble) c[n]));
	  cc[n + 3] = (guchar) ((1.0 - o) * 128 + (o * (gdouble) c[n]));
	}
      cp = cc;
    }
  
  i = 0;
  for (y = 0; y < heig; y++)
    {
      for (x = 0; x < wid; x++)
	{
	  if (priv->use_opacity)
	    f = 3 * ((((goff + x) % 32) < 16) ^ ((y % 32) < 16));
	  else
	    f = 0;
	  
	  for (n = 0; n < 3; n++)
	    buf[i++] = cp[n + f];
	}
    }
  
  gdk_draw_rgb_image(da->window,
		     da->style->black_gc,
		     0, 0,
		     wid, heig,
		     GDK_RGB_DITHER_NORMAL,
		     buf,
		     3*wid);
  
  
  g_free (buf);
}


static void
color_sample_draw_samples (GtkColorSelection *colorsel)
{
  color_sample_draw_sample (colorsel, 0);
  color_sample_draw_sample (colorsel, 1);
}

static void
color_old_sample_expose(GtkWidget* da, GdkEventExpose* event, GtkColorSelection *colorsel)
{
  color_sample_draw_sample (colorsel, 0);
}


static void
color_cur_sample_expose(GtkWidget* da, GdkEventExpose* event, GtkColorSelection *colorsel)
{
  color_sample_draw_sample (colorsel, 1);
}

static void
color_sample_setup_dnd (GtkColorSelection *colorsel, GtkWidget *sample)
{
  static const GtkTargetEntry targets[] = {
    { "application/x-color", 0 }
  };
  ColorSelectionPrivate *priv;
  priv = colorsel->private_data;
  
  gtk_drag_source_set (sample,
		       GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
		       targets, 1,
		       GDK_ACTION_COPY | GDK_ACTION_MOVE);
  
  gtk_signal_connect (GTK_OBJECT (sample),
		      "drag_begin",
		      GTK_SIGNAL_FUNC (color_sample_drag_begin),
		      colorsel);
  if (sample == priv->cur_sample)
    {
      
      gtk_drag_dest_set (sample,
			 GTK_DEST_DEFAULT_HIGHLIGHT |
			 GTK_DEST_DEFAULT_MOTION |
			 GTK_DEST_DEFAULT_DROP,
			 targets, 1,
			 GDK_ACTION_COPY);
      
      gtk_signal_connect (GTK_OBJECT (sample),
			  "drag_end",
			  GTK_SIGNAL_FUNC (color_sample_drag_end),
			  colorsel);
    }
  
  gtk_signal_connect (GTK_OBJECT (sample),
		      "drag_data_get",
		      GTK_SIGNAL_FUNC (color_sample_drag_handle),
		      colorsel);
  gtk_signal_connect (GTK_OBJECT (sample),
		      "drag_data_received",
		      GTK_SIGNAL_FUNC (color_sample_drop_handle),
		      colorsel);
  
}


static void
color_sample_new (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  priv = colorsel->private_data;
  
  priv->sample_area = gtk_hbox_new (FALSE, 0);
  priv->old_sample = gtk_drawing_area_new ();
  priv->cur_sample = gtk_drawing_area_new ();
  
  gtk_box_pack_start (GTK_BOX (priv->sample_area), priv->old_sample,
		      TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (priv->sample_area), priv->cur_sample,
		      TRUE, TRUE, 0);
  
  gtk_signal_connect(GTK_OBJECT (priv->old_sample), "expose_event",
		     GTK_SIGNAL_FUNC (color_old_sample_expose),
		     colorsel);
  gtk_signal_connect(GTK_OBJECT (priv->cur_sample), "expose_event",
		     GTK_SIGNAL_FUNC (color_cur_sample_expose),
		     colorsel);
  
  color_sample_setup_dnd (colorsel, priv->old_sample);
  color_sample_setup_dnd (colorsel, priv->cur_sample);
  
  gtk_widget_show_all (priv->sample_area);
}


/*
 *
 * The palette area code
 *
 */
#define CUSTOM_PALETTE_ENTRY_WIDTH   16
#define CUSTOM_PALETTE_ENTRY_HEIGHT  16

static void
palette_get_color (GtkWidget *drawing_area, gdouble *color)
{
  gdouble *color_val;
  
  g_return_if_fail (color != NULL);
  
  color_val = gtk_object_get_data (GTK_OBJECT (drawing_area), "color_val");
  if (color_val == NULL)
    {
      /* Default to white for no good reason */
      color[0] = 1.0;
      color[1] = 1.0;
      color[2] = 1.0;
      color[3] = 1.0;
      return;
    }
  
  color[0] = color_val[0];
  color[1] = color_val[1];
  color[2] = color_val[2];
  color[3] = 1.0;
}

#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)
static void
palette_paint (GtkWidget    *drawing_area,
	       GdkRectangle *area,
	       gpointer      data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data); 
  ColorSelectionPrivate *priv = colorsel->private_data; 

  if (drawing_area->window == NULL)
    return;
  
  gdk_window_clear_area (drawing_area->window,
			 area->x,
			 area->y,
			 area->width, 
			 area->height);
  
  if (priv->last_palette == drawing_area)
    {
      GdkGC *gc;
      gdouble color[4];
      
      palette_get_color (drawing_area, color);
      
      if (INTENSITY (color[0], color[1], color[2]) > 0.5)
	gc = drawing_area->style->black_gc;
      else
	gc = drawing_area->style->white_gc;

      gdk_draw_rectangle (drawing_area->window,
			  gc, FALSE, 0, 0,
			  drawing_area->allocation.width - 1,
			  drawing_area->allocation.height - 1);
    }
}

static void
palette_expose (GtkWidget      *drawing_area,
		GdkEventExpose *event,
		gpointer        data)
{
  if (drawing_area->window == NULL)
    return;
  
  palette_paint (drawing_area, &(event->area), data);
}

static void
palette_press (GtkWidget      *drawing_area,
	       GdkEventButton *event,
	       gpointer        data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data); 
  ColorSelectionPrivate *priv = colorsel->private_data; 
 
  if (priv->last_palette != NULL) 
    gtk_widget_queue_clear (priv->last_palette);
  
  priv->last_palette = drawing_area;

  if (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (drawing_area), "color_set")) != 0)
    {
      gdouble color[4];
      palette_get_color (drawing_area, color);
      gtk_color_selection_set_color (GTK_COLOR_SELECTION (data), color);
    }

  gtk_widget_queue_clear (priv->last_palette);
}

static void
palette_draw (GtkWidget    *drawing_area,
	      GdkRectangle *area,
	      gpointer      data)
{
  palette_paint (drawing_area, area, data);
}

static void
palette_unset_color (GtkWidget *drawing_area)
{
  if (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (drawing_area), "color_set")) == 0)
    return;
  
  gtk_widget_reset_rc_styles (drawing_area);
  gtk_object_set_data (GTK_OBJECT (drawing_area), "color_set", GINT_TO_POINTER (0));
}

static void
palette_drag_begin (GtkWidget      *widget,
		    GdkDragContext *context,
		    gpointer        data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  GtkWidget *window;
  gdouble colors[4];
  GdkColor bg;
  
  priv = colorsel->private_data;
  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
  gtk_widget_set_usize (window, 48, 32);
  gtk_widget_realize (window);
  gtk_object_set_data_full (GTK_OBJECT (widget),
			    "gtk-color-selection-drag-window",
			    window,
			    (GtkDestroyNotify) gtk_widget_destroy);
  
  palette_get_color (widget, colors);
  bg.red = 0xffff * colors[0];
  bg.green = 0xffff * colors[1];
  bg.blue = 0xffff * colors[2];
  
  gdk_color_alloc (gtk_widget_get_colormap (window), &bg);
  gdk_window_set_background (window->window, &bg);
  
  gtk_drag_set_icon_widget (context, window, -2, -2);
}

static void
palette_drag_handle (GtkWidget        *widget,
		     GdkDragContext   *context,
		     GtkSelectionData *selection_data,
		     guint             info,
		     guint             time,
		     gpointer          data)
{
  guint16 vals[4];
  gdouble colsrc[4];
  
  palette_get_color (widget, colsrc);
  
  vals[0] = colsrc[COLORSEL_RED] * 0xffff;
  vals[1] = colsrc[COLORSEL_GREEN] * 0xffff;
  vals[2] = colsrc[COLORSEL_BLUE] * 0xffff;
  vals[3] = 0xffff;
  
  gtk_selection_data_set (selection_data,
			  gdk_atom_intern ("application/x-color", FALSE),
			  16, (guchar *)vals, 8);
}

static void
palette_set_color (GtkWidget         *drawing_area,
		   GtkColorSelection *colorsel,
		   gdouble           *color)
{
  GtkRcStyle *rc_style;
  gdouble *new_color = g_new (double, 4);
  gdouble *old_color;
  
  rc_style = gtk_rc_style_new ();
  rc_style->bg[GTK_STATE_NORMAL].red = color[0]*65535;
  rc_style->bg[GTK_STATE_NORMAL].green = color[1]*65535;
  rc_style->bg[GTK_STATE_NORMAL].blue = color[2]*65535;
  rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_BG;
  gtk_rc_style_ref (rc_style);
  gtk_widget_modify_style (drawing_area, rc_style);
  
  if (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (drawing_area), "color_set")) == 0)
    {
      static const GtkTargetEntry targets[] = {
	{ "application/x-color", 0 }
      };
      gtk_drag_source_set (drawing_area,
			   GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			   targets, 1,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
      
      gtk_signal_connect (GTK_OBJECT (drawing_area),
			  "drag_begin",
			  GTK_SIGNAL_FUNC (palette_drag_begin),
			  colorsel);
      gtk_signal_connect (GTK_OBJECT (drawing_area),
			  "drag_data_get",
			  GTK_SIGNAL_FUNC (palette_drag_handle),
			  colorsel);
      
      gtk_object_set_data (GTK_OBJECT (drawing_area), "color_set", GINT_TO_POINTER (1));
    }
  else
    {
      old_color = (gdouble *) gtk_object_get_data (GTK_OBJECT (drawing_area), "color_val");
      if (old_color)
	{
	  g_free (old_color);
	}
    }
  new_color[0] = color[0];
  new_color[1] = color[1];
  new_color[2] = color[2];
  new_color[3] = 1.0;
  
  gtk_object_set_data (GTK_OBJECT (drawing_area), "color_val", new_color);
}

static void
palette_drag_end (GtkWidget      *widget,
		  GdkDragContext *context,
		  gpointer        data)
{
  gtk_object_set_data (GTK_OBJECT (widget), "gtk-color-selection-drag-window", NULL);
}

static void
palette_drop_handle (GtkWidget        *widget,
		     GdkDragContext   *context,
		     gint              x,
		     gint              y,
		     GtkSelectionData *selection_data,
		     guint             info,
		     guint             time,
		     gpointer          data)
{
  guint16 *vals;
  gdouble color[4];
  
  if (selection_data->length < 0)
    return;
  
  if ((selection_data->format != 16) ||
      (selection_data->length != 8))
    {
      g_warning ("Received invalid color data\n");
      return;
    }
  
  vals = (guint16 *)selection_data->data;
  
  color[0] = (gdouble)vals[0] / 0xffff;
  color[1] = (gdouble)vals[1] / 0xffff;
  color[2] = (gdouble)vals[2] / 0xffff;
  color[3] = (gdouble)vals[3] / 0xffff;
  palette_set_color (widget, GTK_COLOR_SELECTION (data), color);
  gtk_color_selection_set_color (GTK_COLOR_SELECTION (data), color);
}

static gint
palette_activate (GtkWidget   *widget,
		  GdkEventKey *event,
		  gpointer     data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  
  if ((event->keyval == ' ') || (event->keyval == GDK_Return))
    {
      priv = colorsel->private_data;
      palette_set_color (widget, GTK_COLOR_SELECTION (data), priv->color);
    }
  
  return TRUE;
}

static GtkWidget*
palette_new (GtkColorSelection *colorsel)
{
  GtkWidget *retval;
  
  static const GtkTargetEntry targets[] = {
    { "application/x-color", 0 }
  };
  
  retval = gtk_drawing_area_new ();
  gtk_object_set_data (GTK_OBJECT (retval), "color_set", GINT_TO_POINTER (0)); 
  gtk_widget_set_events (retval, GDK_BUTTON_PRESS_MASK | GDK_EXPOSURE_MASK);
  
  gtk_signal_connect (GTK_OBJECT (retval), "draw", palette_draw, colorsel);
  gtk_signal_connect (GTK_OBJECT (retval), "expose_event", palette_expose, colorsel);
  gtk_signal_connect (GTK_OBJECT (retval), "button_press_event", palette_press, colorsel);
  
  gtk_drag_dest_set (retval,
		     GTK_DEST_DEFAULT_HIGHLIGHT |
		     GTK_DEST_DEFAULT_MOTION |
		     GTK_DEST_DEFAULT_DROP,
		     targets, 1,
		     GDK_ACTION_COPY);
  
  gtk_signal_connect (GTK_OBJECT (retval), "drag_end", palette_drag_end, NULL);
  gtk_signal_connect (GTK_OBJECT (retval), "drag_data_received", palette_drop_handle, colorsel);
  gtk_signal_connect (GTK_OBJECT (retval), "key_press_event", GTK_SIGNAL_FUNC (palette_activate), colorsel);
  
  return retval;
}


/*
 *
 * The actual GtkColorSelection widget
 *
 */

static void
initialize_cursor (void)
{
  GdkColor fg, bg;
  
  GdkPixmap *pixmap =
    gdk_bitmap_create_from_data (NULL,
				 dropper_bits,
				 DROPPER_WIDTH, DROPPER_HEIGHT);
  GdkPixmap *mask =
    gdk_bitmap_create_from_data (NULL,
				 dropper_mask,
				 DROPPER_WIDTH, DROPPER_HEIGHT);
  
  gdk_color_white (gdk_colormap_get_system (), &bg);
  gdk_color_black (gdk_colormap_get_system (), &fg);
  
  picker_cursor = gdk_cursor_new_from_pixmap (pixmap, mask, &fg, &bg, DROPPER_X_HOT ,DROPPER_Y_HOT);
  
  gdk_pixmap_unref (pixmap);
  gdk_pixmap_unref (mask);
  
}

static void
grab_color_at_mouse (GtkWidget *button,
		     gint       x_root,
		     gint       y_root,
		     gpointer   data)
{
  GdkImage *image;
  guint32 pixel;
  GdkVisual *visual;
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  GdkColormap *colormap = gdk_colormap_get_system ();
#if defined (GDK_WINDOWING_X11)
  XColor xcolor;
#endif
  
  priv = colorsel->private_data;
  
  image = gdk_image_get (GDK_ROOT_PARENT (), x_root, y_root, 1, 1);
  pixel = gdk_image_get_pixel (image, 0, 0);
  visual = gdk_colormap_get_visual (colormap);
  
  switch (visual->type) {
  case GDK_VISUAL_DIRECT_COLOR:
  case GDK_VISUAL_TRUE_COLOR:
    priv->color[COLORSEL_RED] = (double)((pixel & visual->red_mask)>>visual->red_shift)/((1<<visual->red_prec) - 1);
    priv->color[COLORSEL_GREEN] = (double)((pixel & visual->green_mask)>>visual->green_shift)/((1<<visual->green_prec) - 1);
    priv->color[COLORSEL_BLUE] = (double)((pixel & visual->blue_mask)>>visual->blue_shift)/((1<<visual->blue_prec) - 1);
    break;
  case GDK_VISUAL_STATIC_GRAY:
  case GDK_VISUAL_GRAYSCALE:
    priv->color[COLORSEL_RED] = (double)pixel/((1<<visual->depth) - 1);
    priv->color[COLORSEL_GREEN] = (double)pixel/((1<<visual->depth) - 1);
    priv->color[COLORSEL_BLUE] = (double)pixel/((1<<visual->depth) - 1);
    break;
#if defined (GDK_WINDOWING_X11)
  case GDK_VISUAL_STATIC_COLOR:
    xcolor.pixel = pixel;
    XQueryColor (GDK_DISPLAY (), GDK_COLORMAP_XCOLORMAP (colormap), &xcolor);
    priv->color[COLORSEL_RED] = xcolor.red/65535.0;
    priv->color[COLORSEL_GREEN] = xcolor.green/65535.0;
    priv->color[COLORSEL_BLUE] = xcolor.blue/65535.0;
    break;
#endif
  case GDK_VISUAL_PSEUDO_COLOR:
    priv->color[COLORSEL_RED] = colormap->colors[pixel].red/(double)0xffffff;
    priv->color[COLORSEL_GREEN] = colormap->colors[pixel].green/(double)0xffffff;
    priv->color[COLORSEL_BLUE] = colormap->colors[pixel].blue/(double)0xffffff;
    break;
  default:
    g_assert_not_reached ();
    break;
  }
  
  gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
		  priv->color[COLORSEL_GREEN],
		  priv->color[COLORSEL_BLUE],
		  &priv->color[COLORSEL_HUE],
		  &priv->color[COLORSEL_SATURATION],
		  &priv->color[COLORSEL_VALUE]);
  update_color (colorsel);
}

static void
mouse_motion (GtkWidget      *button,
	      GdkEventMotion *event,
	      gpointer        data)
{
  grab_color_at_mouse (button, event->x_root, event->y_root, data); 
}

static void
mouse_release (GtkWidget      *button,
	       GdkEventButton *event,
	       gpointer        data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  priv = colorsel->private_data;
  
  gtk_signal_disconnect_by_func (GTK_OBJECT (button), mouse_motion, data);
  gtk_signal_disconnect_by_func (GTK_OBJECT (button), mouse_release, data);
  
  grab_color_at_mouse (button, event->x_root, event->y_root, data);
  gdk_pointer_ungrab (0);
}

/* Helper Functions */
static void
mouse_press (GtkWidget      *button,
	     GdkEventButton *event,
	     gpointer        data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  priv = colorsel->private_data;
  
  gtk_signal_connect (GTK_OBJECT (button), "motion_notify_event", mouse_motion, data);
  gtk_signal_connect (GTK_OBJECT (button), "button_release_event", mouse_release, data);
  gtk_signal_disconnect_by_func (GTK_OBJECT (button), mouse_press, data); 
}

/* when the button is clicked */
static void
get_screen_color (GtkWidget *button)
{
  GtkColorSelection *colorsel = gtk_object_get_data (GTK_OBJECT (button), "COLORSEL");
  ColorSelectionPrivate *priv = colorsel->private_data; 
  
  if (picker_cursor == NULL)
    {
      initialize_cursor ();
    }
  
  /* Why do we do this? Because the "clicked" signal will be emitted after the "button_released"
     signal. We don't want to do this stuff again, though, or else it will get trapped here. So, 
     priv->moving_dropper is initialized to FALSE at the initialization of the colorselector, 
     it is initialized to true when we start waiting for the user to click the the dropper on a 
     color, and whenver it is true when this function starts to execute, we set it to false. */
  if (priv->moving_dropper == FALSE)
    {
      priv->moving_dropper = TRUE;
      gtk_signal_connect (GTK_OBJECT (button), "button_press_event", mouse_press, colorsel); 
      
      gdk_pointer_grab (button->window,
			FALSE,
			GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK,
			NULL,
			picker_cursor,
			0);
    }
  else
    {
      priv->moving_dropper = FALSE; 
    }
}

void
hex_changed (GtkWidget *hex_entry,
	     gpointer   data)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  GdkColor color;
  gchar *text;
  
  colorsel = GTK_COLOR_SELECTION (data);
  priv = colorsel->private_data;
  
  if (priv->changing)
    return;
  
  text = gtk_editable_get_chars (GTK_EDITABLE (priv->hex_entry), 0, -1);
  if (gdk_color_parse (text, &color))
    {
      priv->color[COLORSEL_RED] = CLAMP (color.red/65280.0, 0.0, 1.0);
      priv->color[COLORSEL_GREEN] = CLAMP (color.green/65280.0, 0.0, 1.0);
      priv->color[COLORSEL_BLUE] = CLAMP (color.blue/65280.0, 0.0, 1.0);
      gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
		      priv->color[COLORSEL_GREEN],
		      priv->color[COLORSEL_BLUE],
		      &priv->color[COLORSEL_HUE],
		      &priv->color[COLORSEL_SATURATION],
		      &priv->color[COLORSEL_VALUE]);
      update_color (colorsel);
    }
  g_free (text);
}

void
hsv_changed (GtkWidget *hsv,
	     gpointer   data)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  
  colorsel = GTK_COLOR_SELECTION (data);
  priv = colorsel->private_data;
  
  if (priv->changing)
    return;
  
  gtk_hsv_get_color (GTK_HSV (hsv),
		     &priv->color[COLORSEL_HUE],
		     &priv->color[COLORSEL_SATURATION],
		     &priv->color[COLORSEL_VALUE]);
  gtk_hsv_to_rgb (priv->color[COLORSEL_HUE],
		  priv->color[COLORSEL_SATURATION],
		  priv->color[COLORSEL_VALUE],
		  &priv->color[COLORSEL_RED],
		  &priv->color[COLORSEL_GREEN],
		  &priv->color[COLORSEL_BLUE]);
  update_color (colorsel);
}

void
adjustment_changed (GtkAdjustment *adjustment,
		    gpointer       data)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  
  colorsel = GTK_COLOR_SELECTION (gtk_object_get_data (GTK_OBJECT (adjustment), "COLORSEL"));
  priv = colorsel->private_data;
  
  if (priv->changing)
    return;
  
  switch (GPOINTER_TO_INT (data))
    {
    case COLORSEL_SATURATION:
    case COLORSEL_VALUE:
      priv->color[GPOINTER_TO_INT (data)] = adjustment->value / 255;
      gtk_hsv_to_rgb (priv->color[COLORSEL_HUE],
		      priv->color[COLORSEL_SATURATION],
		      priv->color[COLORSEL_VALUE],
		      &priv->color[COLORSEL_RED],
		      &priv->color[COLORSEL_GREEN],
		      &priv->color[COLORSEL_BLUE]);
      break;
    case COLORSEL_HUE:
      priv->color[GPOINTER_TO_INT (data)] = adjustment->value / 360;
      gtk_hsv_to_rgb (priv->color[COLORSEL_HUE],
		      priv->color[COLORSEL_SATURATION],
		      priv->color[COLORSEL_VALUE],
		      &priv->color[COLORSEL_RED],
		      &priv->color[COLORSEL_GREEN],
		      &priv->color[COLORSEL_BLUE]);
      break;
    case COLORSEL_RED:
    case COLORSEL_GREEN:
    case COLORSEL_BLUE:
      priv->color[GPOINTER_TO_INT (data)] = adjustment->value / 255;
      
      gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
		      priv->color[COLORSEL_GREEN],
		      priv->color[COLORSEL_BLUE],
		      &priv->color[COLORSEL_HUE],
		      &priv->color[COLORSEL_SATURATION],
		      &priv->color[COLORSEL_VALUE]);
      break;
    default:
      priv->color[GPOINTER_TO_INT (data)] = adjustment->value / 255;
      break;
    }
  update_color (colorsel);
}

void 
opacity_entry_changed (GtkWidget *opacity_entry,
		       gpointer   data)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  GtkAdjustment *adj;
  gchar *text;
  
  colorsel = GTK_COLOR_SELECTION (data);
  priv = colorsel->private_data;
  
  if (priv->changing)
    return;
  
  text = gtk_editable_get_chars (GTK_EDITABLE (priv->opacity_entry), 0, -1);
  adj = gtk_range_get_adjustment (GTK_RANGE (priv->opacity_slider));
  gtk_adjustment_set_value (adj, g_strtod (text, NULL)); 
  
  update_color (colorsel);
  
  g_free (text);
}

static void
widget_focus_in (GtkWidget     *drawing_area,
		 GdkEventFocus *event,
		 gpointer       data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data);
  ColorSelectionPrivate *priv = colorsel->private_data;
  
  /* This signal is connected to by all of the widgets except the "Set Color" button
   * This will let you add a color to the currently selected palette
   */
  
  priv->last_palette = NULL;
}


static void
make_label_spinbutton (GtkColorSelection *colorsel,
		       GtkWidget        **spinbutton,
		       gchar             *text,
		       GtkWidget         *table,
		       gint               i,
		       gint               j,
		       gint               channel_type)
{
  GtkWidget *label;
  GtkAdjustment *adjust;
  
  if (channel_type == COLORSEL_HUE)
    {
      adjust = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 360.0, 1.0, 1.0, 1.0));
    }
  else
    {
      adjust = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 255.0, 1.0, 1.0, 1.0));
    }
  gtk_object_set_data (GTK_OBJECT (adjust), "COLORSEL", colorsel);
  *spinbutton = gtk_spin_button_new (adjust, 10.0, 0);
  gtk_signal_connect (GTK_OBJECT (*spinbutton), "focus_in_event", widget_focus_in, colorsel);
  gtk_signal_connect (GTK_OBJECT (adjust), "value_changed", adjustment_changed, GINT_TO_POINTER (channel_type));
  label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE (table), label, i, i+1, j, j+1);
  gtk_table_attach_defaults (GTK_TABLE (table), *spinbutton, i+1, i+2, j, j+1);
  
}

static void
make_palette_frame (GtkColorSelection *colorsel,
		    GtkWidget         *table,
		    gint               i,
		    gint               j)
{
  GtkWidget *frame;
  ColorSelectionPrivate *priv;
  
  priv = colorsel->private_data;
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  priv->custom_palette[i][j] = palette_new (colorsel);
  gtk_widget_set_usize (priv->custom_palette[i][j], CUSTOM_PALETTE_ENTRY_WIDTH, CUSTOM_PALETTE_ENTRY_HEIGHT);
  gtk_container_add (GTK_CONTAINER (frame), priv->custom_palette[i][j]);
  gtk_table_attach_defaults (GTK_TABLE (table), frame, i, i+1, j, j+1);
}

/* Set the palette entry [x][y] to be the currently selected one. */
static void 
set_selected_palette (GtkColorSelection *colorsel, int x, int y)
{
  ColorSelectionPrivate *priv = colorsel->private_data; 

  if (priv->last_palette != NULL) 
    gtk_widget_queue_clear (priv->last_palette);

  priv->last_palette = priv->custom_palette[x][y]; 

  gtk_widget_queue_clear (priv->last_palette);
}

static void
update_color (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv = colorsel->private_data;
  gchar entryval[12];
  gchar opacity_text[32];
  gchar *ptr;
  
  priv->changing = TRUE;
  color_sample_draw_samples (colorsel);
  
  gtk_hsv_set_color (GTK_HSV (priv->triangle_colorsel),
		     priv->color[COLORSEL_HUE],
		     priv->color[COLORSEL_SATURATION],
		     priv->color[COLORSEL_VALUE]);
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->hue_spinbutton)),
			    priv->color[COLORSEL_HUE] * 360);
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->sat_spinbutton)),
			    priv->color[COLORSEL_SATURATION] * 255);
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->val_spinbutton)),
			    priv->color[COLORSEL_VALUE] * 255);
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->red_spinbutton)),
			    priv->color[COLORSEL_RED] * 255);
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->green_spinbutton)),
			    priv->color[COLORSEL_GREEN] * 255);
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->blue_spinbutton)),
			    priv->color[COLORSEL_BLUE] * 255);
  gtk_adjustment_set_value (gtk_range_get_adjustment
			    (GTK_RANGE (priv->opacity_slider)),
			    priv->color[COLORSEL_OPACITY] * 255);
  
  g_snprintf (opacity_text, 32, "%.0f", priv->color[COLORSEL_OPACITY] * 255);
  gtk_entry_set_text (GTK_ENTRY (priv->opacity_entry), opacity_text);
  
  g_snprintf (entryval, 11, "#%2X%2X%2X",
	      (guint) (255 * priv->color[COLORSEL_RED]),
	      (guint) (255 * priv->color[COLORSEL_GREEN]),
	      (guint) (255 * priv->color[COLORSEL_BLUE]));
  
  for (ptr = entryval; *ptr; ptr++)
    if (*ptr == ' ')
      *ptr = '0';
  gtk_entry_set_text (GTK_ENTRY (priv->hex_entry), entryval);
  priv->changing = FALSE;
}

static void
add_button_pressed (GtkWidget         *button,
		    GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  gint i, j;
  
  priv = colorsel->private_data;
  
  for (j = 0; j < GTK_CUSTOM_PALETTE_HEIGHT; j++)
    {
      for (i = 0; i < GTK_CUSTOM_PALETTE_WIDTH; i++)
	{
	  /* Either last_palette is set and we're on it, or it's an empty palette */
	  if ((priv->last_palette && priv->last_palette == priv->custom_palette[i][j]) ||
	      ((priv->last_palette == NULL) &&
	       (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (priv->custom_palette[i][j]),
						      "color_set")) == 0)))
	    {
	      palette_set_color (priv->custom_palette[i][j], colorsel, priv->color);

	      /* forward the selection */
	      if ((i == GTK_CUSTOM_PALETTE_WIDTH - 1) && (j == GTK_CUSTOM_PALETTE_HEIGHT - 1))
 		set_selected_palette (colorsel, 0, 0);
	      else if (i == GTK_CUSTOM_PALETTE_WIDTH - 1)
 		set_selected_palette (colorsel, 0, j + 1);
	      else
 		set_selected_palette (colorsel, i + 1, j);

	      return;
	    }
	}
    }

  /* the palette is totally full.  Add to the first one totally arbitrarily */
  palette_set_color (priv->custom_palette[0][0], colorsel, priv->color);

  /* forward the selection */
  set_selected_palette (colorsel, 1, 0);
}

GtkType
gtk_color_selection_get_type (void)
{
  static GtkType color_selection_type = 0;
  
  if (!color_selection_type)
    {
      static const GtkTypeInfo color_selection_info =
      {
        "GtkColorSelection",
        sizeof (GtkColorSelection),
        sizeof (GtkColorSelectionClass),
        (GtkClassInitFunc) gtk_color_selection_class_init,
        (GtkObjectInitFunc) gtk_color_selection_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      color_selection_type = gtk_type_unique (GTK_TYPE_VBOX, &color_selection_info);
    }
  
  return color_selection_type;
}

static void
gtk_color_selection_class_init (GtkColorSelectionClass *klass)
{
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass*) klass;
  
  parent_class = gtk_type_class (GTK_TYPE_VBOX);
  
  color_selection_signals[COLOR_CHANGED] =
    gtk_signal_new ("color_changed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkColorSelectionClass, color_changed),
                    gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);
  
  
  gtk_object_class_add_signals (object_class, color_selection_signals, LAST_SIGNAL);
  
  object_class->destroy = gtk_color_selection_destroy;
}

/* widget functions */
static void
gtk_color_selection_init (GtkColorSelection *colorsel)
{
  GtkWidget *top_hbox;
  GtkWidget *top_right_vbox;
  GtkWidget *table, *label, *hbox, *frame, *vbox;
  GtkAdjustment *adjust;
  GdkPixmap *dropper_pixmap;
  GtkWidget *dropper_image;
  GtkWidget *button;
  GdkBitmap *mask = NULL;
  gint i, j;
  ColorSelectionPrivate *priv;
  
  priv = colorsel->private_data = g_new0 (ColorSelectionPrivate, 1);
  priv->changing = FALSE;
  priv->default_set = FALSE;
  priv->last_palette = NULL;
  priv->moving_dropper = FALSE;
  
  gtk_box_set_spacing (GTK_BOX (colorsel), 4);
  top_hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (colorsel), top_hbox, FALSE, FALSE, 0);
  
  vbox = gtk_vbox_new (FALSE, 4);
  priv->triangle_colorsel = gtk_hsv_new ();
  gtk_signal_connect (GTK_OBJECT (priv->triangle_colorsel), "changed", hsv_changed, colorsel);
  gtk_hsv_set_metrics (GTK_HSV (priv->triangle_colorsel), 174, 15);
  gtk_box_pack_start (GTK_BOX (top_hbox), vbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), priv->triangle_colorsel, FALSE, FALSE, 0);
  
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  
  frame = gtk_frame_new (NULL);
  gtk_widget_set_usize (frame, -1, 30);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  color_sample_new (colorsel);
  gtk_container_add (GTK_CONTAINER (frame), priv->sample_area);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  
  button = gtk_button_new ();
  gtk_signal_connect (GTK_OBJECT (button), "focus_in_event", widget_focus_in, colorsel);
  gtk_widget_set_events (button, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
  gtk_object_set_data (GTK_OBJECT (button), "COLORSEL", colorsel); 
  gtk_signal_connect (GTK_OBJECT (button), "clicked", get_screen_color, NULL);
  dropper_pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, gtk_widget_get_colormap (button), &mask, NULL, picker);
  dropper_image = gtk_pixmap_new (dropper_pixmap, mask);
  gtk_container_add (GTK_CONTAINER (button), dropper_image);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  
  top_right_vbox = gtk_vbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (top_hbox), top_right_vbox, FALSE, FALSE, 0);
  table = gtk_table_new (8, 6, FALSE);
  gtk_box_pack_start (GTK_BOX (top_right_vbox), table, FALSE, FALSE, 0);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  
  make_label_spinbutton (colorsel, &priv->hue_spinbutton, _("Hue:"), table, 0, 0, COLORSEL_HUE);
  make_label_spinbutton (colorsel, &priv->sat_spinbutton, _("Saturation:"), table, 0, 1, COLORSEL_SATURATION);
  make_label_spinbutton (colorsel, &priv->val_spinbutton, _("Value:"), table, 0, 2, COLORSEL_VALUE);
  make_label_spinbutton (colorsel, &priv->red_spinbutton, _("Red:"), table, 6, 0, COLORSEL_RED);
  make_label_spinbutton (colorsel, &priv->green_spinbutton, _("Green:"), table, 6, 1, COLORSEL_GREEN);
  make_label_spinbutton (colorsel, &priv->blue_spinbutton, _("Blue:"), table, 6, 2, COLORSEL_BLUE);
  gtk_table_attach_defaults (GTK_TABLE (table), gtk_hseparator_new (), 0, 8, 3, 4); 
  
  priv->opacity_label = gtk_label_new (_("Opacity:")); 
  gtk_misc_set_alignment (GTK_MISC (priv->opacity_label), 1.0, 0.5); 
  gtk_table_attach_defaults (GTK_TABLE (table), priv->opacity_label, 0, 1, 4, 5); 
  adjust = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 255.0, 1.0, 1.0, 0.0)); 
  gtk_object_set_data (GTK_OBJECT (adjust), "COLORSEL", colorsel); 
  priv->opacity_slider = gtk_hscale_new (adjust); 
  gtk_scale_set_draw_value (GTK_SCALE (priv->opacity_slider), FALSE);
  gtk_signal_connect (GTK_OBJECT(adjust), "value_changed", adjustment_changed, GINT_TO_POINTER (COLORSEL_OPACITY));
  gtk_table_attach_defaults (GTK_TABLE (table), priv->opacity_slider, 1, 7, 4, 5); 
  priv->opacity_entry = gtk_entry_new (); 
  gtk_widget_set_usize (priv->opacity_entry, 40, 0); 
  gtk_signal_connect (GTK_OBJECT (priv->opacity_entry), "focus_in_event", widget_focus_in, colorsel);
  gtk_signal_connect (GTK_OBJECT (priv->opacity_entry), "activate", opacity_entry_changed, colorsel);
  gtk_table_attach_defaults (GTK_TABLE (table), priv->opacity_entry, 7, 8, 4, 5);
  
  label = gtk_label_new (_("Hex Value:"));
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 5, 6);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  priv->hex_entry = gtk_entry_new ();
  gtk_signal_connect (GTK_OBJECT (priv->hex_entry), "activate", hex_changed, colorsel);
  gtk_widget_set_usize (priv->hex_entry, 75, -1);  
  gtk_table_set_col_spacing (GTK_TABLE (table), 3, 15);
  gtk_table_attach_defaults (GTK_TABLE (table), priv->hex_entry, 1, 5, 5, 6);
  
  /* Set up the palette */
  table = gtk_table_new (GTK_CUSTOM_PALETTE_HEIGHT, GTK_CUSTOM_PALETTE_WIDTH, TRUE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 1);
  gtk_table_set_col_spacings (GTK_TABLE (table), 1);
  for (i = 0; i < GTK_CUSTOM_PALETTE_WIDTH; i++)
    {
      for (j = 0; j < GTK_CUSTOM_PALETTE_HEIGHT; j++)
	{
	  make_palette_frame (colorsel, table, i, j);
	}
    }
  set_selected_palette (colorsel, 0, 0);
  priv->palette_frame = gtk_frame_new (_("Custom Palette"));
  gtk_box_pack_end (GTK_BOX (top_right_vbox), priv->palette_frame, FALSE, FALSE, 0);
  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (priv->palette_frame), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  button = gtk_button_new_with_label (_("Set Color"));
  gtk_signal_connect (GTK_OBJECT (button), "clicked", add_button_pressed, colorsel);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
  
  gtk_widget_show_all (top_hbox);
  
  if (priv->use_opacity == FALSE)
    {
      gtk_widget_hide (priv->opacity_label);
      gtk_widget_hide (priv->opacity_slider);
      gtk_widget_hide (priv->opacity_entry);
    }
  
  if (priv->use_palette == FALSE)
    {
      gtk_widget_hide (priv->palette_frame);
    }
}

static void
gtk_color_selection_destroy (GtkObject *object)
{
  GtkColorSelection *cselection = GTK_COLOR_SELECTION (object);
  
  if (cselection->private_data)
    {
      g_free (cselection->private_data);
      cselection->private_data = NULL;
    }
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}



/**
 * gtk_color_selection_new:
 * 
 * Creates a new GtkColorSelection.
 * 
 * Return value: The new GtkColorSelection.
 **/
GtkWidget *
gtk_color_selection_new (void)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  gdouble color[4];
  color[0] = 1.0;
  color[1] = 1.0;
  color[2] = 1.0;
  color[3] = 1.0;
  
  colorsel = gtk_type_new (GTK_TYPE_COLOR_SELECTION);
  priv = colorsel->private_data;
  gtk_color_selection_set_color (colorsel, color);
  gtk_color_selection_set_use_opacity (colorsel, FALSE);
  
  /* We want to make sure that default_set is FALSE */
  /* This way the user can still set it */
  priv->default_set = FALSE;
  
  return GTK_WIDGET (colorsel);
}


void
gtk_color_selection_set_update_policy (GtkColorSelection *colorsel,
				       GtkUpdateType      policy)
{
  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  /* */
  g_warning (G_STRLOC ": This function is deprecated.");
}

/**
 * gtk_color_selection_get_use_opacity:
 * @colorsel: A GtkColorSelection.
 * 
 * Determines whether the colorsel can use opacity.
 * 
 * Return value: TRUE if the @colorsel uses opacity.  FALSE if it does't.
 **/
gboolean
gtk_color_selection_get_use_opacity (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  g_return_val_if_fail (colorsel != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);
  
  priv = colorsel->private_data;
  
  return priv->use_opacity;
}

/**
 * gtk_color_selection_set_use_opacity:
 * @colorsel: A GtkColorSelection.
 * @use_opacity: TRUE if @colorsel can set the opacity, FALSE otherwise.
 *
 * Sets the @colorsel to use or not use opacity.
 * 
 **/
void
gtk_color_selection_set_use_opacity (GtkColorSelection *colorsel,
				     gboolean           use_opacity)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  use_opacity = use_opacity != FALSE;
  
  if (priv->use_opacity != use_opacity)
    {
      priv->use_opacity = use_opacity;
      if (use_opacity)
	{
	  gtk_widget_show (priv->opacity_slider);
	  gtk_widget_show (priv->opacity_label);
	  gtk_widget_show (priv->opacity_entry);
	}
      else
	{
	  gtk_widget_hide (priv->opacity_slider);
	  gtk_widget_hide (priv->opacity_label);
	  gtk_widget_hide (priv->opacity_entry);
	}
      color_sample_draw_samples (colorsel);
    }
}

/**
 * gtk_color_selection_get_use_palette:
 * @colorsel: A GtkColorSelection.
 * 
 * Determines whether the palette is used.
 * 
 * Return value: TRUE if the palette is used.  FALSE if it isn't.
 **/
gboolean
gtk_color_selection_get_use_palette (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);
  
  priv = colorsel->private_data;
  
  return priv->use_palette;
}

/**
 * gtk_color_selection_set_use_palette:
 * @colorsel: A GtkColorSelection.
 * @use_palette: TRUE if palette is to be visible, FALSE otherwise.
 *
 * Shows and hides the palette based upon the value of @use_palette.
 * 
 **/
void
gtk_color_selection_set_use_palette (GtkColorSelection *colorsel,
				     gboolean           use_palette)
{
  ColorSelectionPrivate *priv;
  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  use_palette = use_palette != FALSE;
  
  if (priv->use_palette != use_palette)
    {
      priv->use_palette = use_palette;
      if (use_palette)
	gtk_widget_show (priv->palette_frame);
      else
	gtk_widget_hide (priv->palette_frame);
    }
}

/**
 * gtk_color_selection_set_color:
 * @colorsel: A GtkColorSelection.
 * @color: A color to set the current color with.
 *
 * Sets the current color to be @color.  The first time this is called, it will
 * also set the original color to be @color too.
 * 
 **/
void
gtk_color_selection_set_color (GtkColorSelection    *colorsel,
			       gdouble              *color)
{
  ColorSelectionPrivate *priv;
  gint i;
  
  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->color[COLORSEL_RED] = color[0];
  priv->color[COLORSEL_GREEN] = color[1];
  priv->color[COLORSEL_BLUE] = color[2];
  priv->color[COLORSEL_OPACITY] = color[3];
  gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
		  priv->color[COLORSEL_GREEN],
		  priv->color[COLORSEL_BLUE],
		  &priv->color[COLORSEL_HUE],
		  &priv->color[COLORSEL_SATURATION],
		  &priv->color[COLORSEL_VALUE]);
  if (priv->default_set == FALSE)
    {
      for (i = 0; i < COLORSEL_NUM_CHANNELS; i++)
	priv->old_color[i] = priv->color[i];
    }
  update_color (colorsel);
  priv->default_set = TRUE;
}

/**
 * gtk_color_selection_get_color:
 * @colorsel: A GtkColorSelection.
 * @color: A color to fill in with the current color.
 *
 * Sets @color to be the current color in the GtkColorSelection widget.
 * 
 **/
void
gtk_color_selection_get_color (GtkColorSelection *colorsel,
			       gdouble           *color)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  color[0] = priv->color[COLORSEL_RED];
  color[1] = priv->color[COLORSEL_GREEN];
  color[2] = priv->color[COLORSEL_BLUE];
  color[3] = priv->use_opacity ? priv->color[COLORSEL_OPACITY] : 1.0;
}

/**
 * gtk_color_selection_get_old_color:
 * @colorsel: A GtkColorSelection.
 * @color: A color to set the original color with.
 *
 * Sets the 'original' color to be @color.  This function should be called with
 * some hesitations, as it might seem confusing to have that color change.
 * Calling gtk_color_selection_set_color will also set this color the first
 * time it is called.
 * 
 **/
void
gtk_color_selection_set_old_color (GtkColorSelection *colorsel,
				   gdouble          *color)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->old_color[COLORSEL_RED] = color[0];
  priv->old_color[COLORSEL_GREEN] = color[1];
  priv->old_color[COLORSEL_BLUE] = color[2];
  priv->old_color[COLORSEL_OPACITY] = color[3];
  gtk_rgb_to_hsv (priv->old_color[COLORSEL_RED],
		  priv->old_color[COLORSEL_GREEN],
		  priv->old_color[COLORSEL_BLUE],
		  &priv->old_color[COLORSEL_HUE],
		  &priv->old_color[COLORSEL_SATURATION],
		  &priv->old_color[COLORSEL_VALUE]);
  color_sample_draw_samples (colorsel);
  priv->default_set = TRUE;
}

/**
 * gtk_color_selection_get_old_color:
 * @colorsel: A GtkColorSelection.
 * @color: A color to fill in with the original color value.
 *
 * Fills @color in with the original color value.
 * 
 **/
void
gtk_color_selection_get_old_color (GtkColorSelection *colorsel,
				   gdouble           *color)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  color[0] = priv->old_color[COLORSEL_RED];
  color[1] = priv->old_color[COLORSEL_GREEN];
  color[2] = priv->old_color[COLORSEL_BLUE];
  color[3] = priv->use_opacity ? priv->old_color[COLORSEL_OPACITY] : 1.0;
}

/**
 * gtk_color_selection_set_palette_color:
 * @colorsel: A GtkColorSelection.
 * @x: The x coordinate of the palette.
 * @y: The y coordinate of the palette.
 * @color: A color to set the palette with.
 *
 * Set the palette located at (@x, @y) to have @color set as its color.
 * 
 **/
void
gtk_color_selection_set_palette_color (GtkColorSelection   *colorsel,
				       gint                 x,
				       gint                 y,
				       gdouble             *color)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (x >= 0 && y >= 0 && x < GTK_CUSTOM_PALETTE_WIDTH && y < GTK_CUSTOM_PALETTE_HEIGHT);
  
  priv = colorsel->private_data;
  palette_set_color (priv->custom_palette[x][y], colorsel, color);
}

/**
 * gtk_color_selection_get_palette_color:
 * @colorsel: A GtkColorSelection.
 * @x: The x coordinate of the palette.
 * @y: The y coordinate of the palette.
 * @color: A color to fill in with the color value.
 * 
 * Set @color to have the color found in the palette located at (@x, @y).  If
 * the palette is unset, it will leave the color unset.
 * 
 * Return value: TRUE if the palette located at (@x, @y) has a color set.  FALSE
 * if it doesn't.
 **/
gboolean
gtk_color_selection_get_palette_color (GtkColorSelection   *colorsel,
				       gint                 x,
				       gint                 y,
				       gdouble             *color)
{
  ColorSelectionPrivate *priv;
  
  g_return_val_if_fail (colorsel != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);
  g_return_val_if_fail (x >= 0 && y >= 0 && x < GTK_CUSTOM_PALETTE_WIDTH && y < GTK_CUSTOM_PALETTE_HEIGHT, FALSE);
  
  priv = colorsel->private_data;
  
  if (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (priv->custom_palette[x][y]), "color_set")) == 0)
    return FALSE;
  
  palette_get_color (priv->custom_palette[x][y], color);
  return TRUE;
}

/**
 * gtk_color_selection_unset_palette_color:
 * @colorsel: A GtkColorSelection.
 * @x: The x coordinate of the palette.
 * @y: The y coordinate of the palette.
 *
 * Change the palette located at (@x, @y) to have no color set.
 * 
 **/
void
gtk_color_selection_unset_palette_color (GtkColorSelection   *colorsel,
					 gint                 x,
					 gint                 y)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (colorsel != NULL);
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (x >= 0 && y >= 0 && x < GTK_CUSTOM_PALETTE_WIDTH && y < GTK_CUSTOM_PALETTE_HEIGHT);
  
  priv = colorsel->private_data;
  palette_unset_color (priv->custom_palette[x][y]);
}

/**
 * gtk_color_selection_is_adjusting:
 * @colorsel: A GtkColorSelection.
 *
 * Gets the current state of the @colorsel.
 *
 * Return value: TRUE if the user is currently dragging a color around, and FALSE
 * if the selection has stopped.
 **/
gboolean
gtk_color_selection_is_adjusting (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  g_return_val_if_fail (colorsel != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);
  
  priv = colorsel->private_data;
  
  return (gtk_hsv_is_adjusting (GTK_HSV (priv->triangle_colorsel)));
}
