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

#if HAVE_CONFIG_H
#  include <config.h>
#  if STDC_HEADERS
#    include <string.h>
#    include <stdio.h>
#  endif
#else
#  include <stdio.h>
#endif

#include "gtkprogressbar.h"
#include "gtksignal.h"
#include "gtkintl.h"


#define MIN_HORIZONTAL_BAR_WIDTH   150
#define MIN_HORIZONTAL_BAR_HEIGHT  20
#define MIN_VERTICAL_BAR_WIDTH     22
#define MIN_VERTICAL_BAR_HEIGHT    80
#define MAX_TEXT_LENGTH            80
#define TEXT_SPACING               2

enum {
  PROP_0,

  /* Supported args */
  PROP_FRACTION,
  PROP_PULSE_STEP,
  PROP_ORIENTATION,
  PROP_TEXT,
  
  /* Deprecated args */
  PROP_ADJUSTMENT,
  PROP_BAR_STYLE,
  PROP_ACTIVITY_STEP,
  PROP_ACTIVITY_BLOCKS,
  PROP_DISCRETE_BLOCKS
};

static void gtk_progress_bar_class_init    (GtkProgressBarClass *klass);
static void gtk_progress_bar_init          (GtkProgressBar      *pbar);
static void gtk_progress_bar_set_property  (GObject             *object,
					    guint                prop_id,
					    const GValue        *value,
					    GParamSpec          *pspec);
static void gtk_progress_bar_get_property  (GObject             *object,
					    guint                prop_id,
					    GValue              *value,
					    GParamSpec          *pspec);
static void gtk_progress_bar_size_request  (GtkWidget           *widget,
					    GtkRequisition      *requisition);
static void gtk_progress_bar_real_update   (GtkProgress         *progress);
static void gtk_progress_bar_paint         (GtkProgress         *progress);
static void gtk_progress_bar_act_mode_enter (GtkProgress        *progress);


GtkType
gtk_progress_bar_get_type (void)
{
  static GtkType progress_bar_type = 0;

  if (!progress_bar_type)
    {
      static const GtkTypeInfo progress_bar_info =
      {
	"GtkProgressBar",
	sizeof (GtkProgressBar),
	sizeof (GtkProgressBarClass),
	(GtkClassInitFunc) gtk_progress_bar_class_init,
	(GtkObjectInitFunc) gtk_progress_bar_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

      progress_bar_type = gtk_type_unique (GTK_TYPE_PROGRESS, &progress_bar_info);
    }

  return progress_bar_type;
}

static void
gtk_progress_bar_class_init (GtkProgressBarClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkProgressClass *progress_class;
  
  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass *) class;
  progress_class = (GtkProgressClass *) class;

  gobject_class->set_property = gtk_progress_bar_set_property;
  gobject_class->get_property = gtk_progress_bar_get_property;
  
  widget_class->size_request = gtk_progress_bar_size_request;

  progress_class->paint = gtk_progress_bar_paint;
  progress_class->update = gtk_progress_bar_real_update;
  progress_class->act_mode_enter = gtk_progress_bar_act_mode_enter;

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
                                                        _("Adjustment"),
                                                        _("The GtkAdjustment connected to the progress bar (Deprecated)"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
						      _("Orientation"),
						      _("Orientation and growth direction of the progress bar"),
						      GTK_TYPE_PROGRESS_BAR_ORIENTATION,
						      GTK_PROGRESS_LEFT_TO_RIGHT,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_BAR_STYLE,
                                   g_param_spec_enum ("bar_style",
						      _("Bar style"),
						      _("Specifies the visual style of the bar in percentage mode (Deprecated)"),
						      GTK_TYPE_PROGRESS_BAR_STYLE,
						      GTK_PROGRESS_CONTINUOUS,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVITY_STEP,
                                   g_param_spec_uint ("activity_step",
						      _("Activity Step"),
						      _("The increment used for each iteration in activity mode (Deprecated)"),
						      -G_MAXUINT,
						      G_MAXUINT,
						      3,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVITY_BLOCKS,
                                   g_param_spec_uint ("activity_blocks",
						      _("Activity Blocks"),
						      _("The number of blocks which can fit in the progress bar area in activity mode (Deprecated)"),
						      2,
						      G_MAXUINT,
						      5,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DISCRETE_BLOCKS,
                                   g_param_spec_uint ("discrete_blocks",
						      _("Discrete Blocks"),
						      _("The number of discrete blocks in a progress bar (when shown in the discrete style)"),
						      2,
						      G_MAXUINT,
						      10,
						      G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
				   PROP_FRACTION,
				   g_param_spec_double ("fraction",
							_("Fraction"),
							_("The fraction of total work that has been completed"),
							0.0,
							1.0,
							0.0,
							G_PARAM_READWRITE));  
  
  g_object_class_install_property (gobject_class,
				   PROP_PULSE_STEP,
				   g_param_spec_double ("pulse_step",
							_("Pulse Step"),
							_("The fraction of total progress to move the bouncing block when pulsed"),
							0.0,
							1.0,
							0.1,
							G_PARAM_READWRITE));  
  
  g_object_class_install_property (gobject_class,
				   PROP_TEXT,
				   g_param_spec_string ("text",
							_("Text"),
							_("Text to be displayed in the progress bar"),
							"%P %%",
							G_PARAM_READWRITE));

}

static void
gtk_progress_bar_init (GtkProgressBar *pbar)
{
  pbar->bar_style = GTK_PROGRESS_CONTINUOUS;
  pbar->blocks = 10;
  pbar->in_block = -1;
  pbar->orientation = GTK_PROGRESS_LEFT_TO_RIGHT;
  pbar->pulse_fraction = 0.1;
  pbar->activity_pos = 0;
  pbar->activity_dir = 1;
  pbar->activity_step = 3;
  pbar->activity_blocks = 5;
}

static void
gtk_progress_bar_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  GtkProgressBar *pbar;

  pbar = GTK_PROGRESS_BAR (object);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      gtk_progress_set_adjustment (GTK_PROGRESS (pbar),
				   GTK_ADJUSTMENT (g_value_get_object (value)));
      break;
    case PROP_ORIENTATION:
      gtk_progress_bar_set_orientation (pbar, g_value_get_enum (value));
      break;
    case PROP_BAR_STYLE:
      gtk_progress_bar_set_bar_style (pbar, g_value_get_enum (value));
      break;
    case PROP_ACTIVITY_STEP:
      gtk_progress_bar_set_activity_step (pbar, g_value_get_uint (value));
      break;
    case PROP_ACTIVITY_BLOCKS:
      gtk_progress_bar_set_activity_blocks (pbar, g_value_get_uint (value));
      break;
    case PROP_DISCRETE_BLOCKS:
      gtk_progress_bar_set_discrete_blocks (pbar, g_value_get_uint (value));
      break;
    case PROP_FRACTION:
      gtk_progress_bar_set_fraction (pbar, g_value_get_double (value));
      break;
    case PROP_PULSE_STEP:
      gtk_progress_bar_set_pulse_step (pbar, g_value_get_double (value));
      break;
    case PROP_TEXT:
      gtk_progress_bar_set_text (pbar, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_progress_bar_get_property (GObject      *object,
			       guint         prop_id,
			       GValue       *value,
			       GParamSpec   *pspec)
{
  GtkProgressBar *pbar;

  pbar = GTK_PROGRESS_BAR (object);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, GTK_PROGRESS (pbar)->adjustment);
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, pbar->orientation);
      break;
    case PROP_BAR_STYLE:
      g_value_set_enum (value, pbar->bar_style);
      break;
    case PROP_ACTIVITY_STEP:
      g_value_set_uint (value, pbar->activity_step);
      break;
    case PROP_ACTIVITY_BLOCKS:
      g_value_set_uint (value, pbar->activity_blocks);
      break;
    case PROP_DISCRETE_BLOCKS:
      g_value_set_uint (value, pbar->blocks);
      break;
    case PROP_FRACTION:
      g_value_set_double (value, gtk_progress_get_current_percentage (GTK_PROGRESS (pbar)));
      break;
    case PROP_PULSE_STEP:
      g_value_set_double (value, pbar->pulse_fraction);
      break;
    case PROP_TEXT:
      g_value_set_string (value, gtk_progress_bar_get_text (pbar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkWidget*
gtk_progress_bar_new (void)
{
  GtkWidget *pbar;

  pbar = gtk_widget_new (GTK_TYPE_PROGRESS_BAR, NULL);

  return pbar;
}

GtkWidget*
gtk_progress_bar_new_with_adjustment (GtkAdjustment *adjustment)
{
  GtkWidget *pbar;

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), NULL);

  pbar = gtk_widget_new (GTK_TYPE_PROGRESS_BAR,
			 "adjustment", adjustment,
			 NULL);

  return pbar;
}

static void
gtk_progress_bar_real_update (GtkProgress *progress)
{
  GtkProgressBar *pbar;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_PROGRESS (progress));

  pbar = GTK_PROGRESS_BAR (progress);
  widget = GTK_WIDGET (progress);
 
  if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS ||
      GTK_PROGRESS (pbar)->activity_mode)
    {
      if (GTK_PROGRESS (pbar)->activity_mode)
	{
	  guint size;
          
	  /* advance the block */

	  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
	      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
	    {
              /* Update our activity step. */
              
              pbar->activity_step = widget->allocation.width * pbar->pulse_fraction;
              
	      size = MAX (2, widget->allocation.width / pbar->activity_blocks);

	      if (pbar->activity_dir == 0)
		{
		  pbar->activity_pos += pbar->activity_step;
		  if (pbar->activity_pos + size >=
		      widget->allocation.width -
		      widget->style->xthickness)
		    {
		      pbar->activity_pos = widget->allocation.width -
			widget->style->xthickness - size;
		      pbar->activity_dir = 1;
		    }
		}
	      else
		{
		  pbar->activity_pos -= pbar->activity_step;
		  if (pbar->activity_pos <= widget->style->xthickness)
		    {
		      pbar->activity_pos = widget->style->xthickness;
		      pbar->activity_dir = 0;
		    }
		}
	    }
	  else
	    {
              /* Update our activity step. */
              
              pbar->activity_step = widget->allocation.height * pbar->pulse_fraction;
              
	      size = MAX (2, widget->allocation.height / pbar->activity_blocks);

	      if (pbar->activity_dir == 0)
		{
		  pbar->activity_pos += pbar->activity_step;
		  if (pbar->activity_pos + size >=
		      widget->allocation.height -
		      widget->style->ythickness)
		    {
		      pbar->activity_pos = widget->allocation.height -
			widget->style->ythickness - size;
		      pbar->activity_dir = 1;
		    }
		}
	      else
		{
		  pbar->activity_pos -= pbar->activity_step;
		  if (pbar->activity_pos <= widget->style->ythickness)
		    {
		      pbar->activity_pos = widget->style->ythickness;
		      pbar->activity_dir = 0;
		    }
		}
	    }
	}
      gtk_progress_bar_paint (progress);
      gtk_widget_queue_draw (GTK_WIDGET (progress));
    }
  else
    {
      gint in_block;
      
      in_block = -1 + (gint)(gtk_progress_get_current_percentage (progress) *
			     (gdouble)pbar->blocks);
      
      if (pbar->in_block != in_block)
	{
	  pbar->in_block = in_block;
	  gtk_progress_bar_paint (progress);
	  gtk_widget_queue_draw (GTK_WIDGET (progress));
	}
    }
}

static void
gtk_progress_bar_size_request (GtkWidget      *widget,
			       GtkRequisition *requisition)
{
  GtkProgress *progress;
  GtkProgressBar *pbar;
  gchar *buf;
  PangoRectangle logical_rect;
  PangoLayout *layout;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (widget));
  g_return_if_fail (requisition != NULL);

  progress = GTK_PROGRESS (widget);
  pbar = GTK_PROGRESS_BAR (widget);

  if (progress->show_text && pbar->bar_style != GTK_PROGRESS_DISCRETE)
    {
      if (!progress->adjustment)
	gtk_progress_set_adjustment (progress, NULL);

      buf = gtk_progress_get_text_from_value (progress, progress->adjustment->upper);

      layout = gtk_widget_create_pango_layout (widget, buf);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	  
      g_object_unref (G_OBJECT (layout));
      g_free (buf);
    }
  
  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
    {
      if (progress->show_text && pbar->bar_style != GTK_PROGRESS_DISCRETE)
	{
	  requisition->width = MAX (MIN_HORIZONTAL_BAR_WIDTH,
				    2 * widget->style->xthickness + 3 +
				    logical_rect.width + 2 * TEXT_SPACING);

	  requisition->height = MAX (MIN_HORIZONTAL_BAR_HEIGHT,
				     2 * widget->style->ythickness + 3 +
				     logical_rect.height + 2 * TEXT_SPACING);
	}
      else
	{
	  requisition->width = MIN_HORIZONTAL_BAR_WIDTH;
	  requisition->height = MIN_HORIZONTAL_BAR_HEIGHT;
	}
    }
  else
    {
      if (progress->show_text && pbar->bar_style != GTK_PROGRESS_DISCRETE)
	{	  
	  requisition->width = MAX (MIN_VERTICAL_BAR_WIDTH,
				    2 * widget->style->xthickness + 3 +
				    logical_rect.width + 2 * TEXT_SPACING);

	  requisition->height = MAX (MIN_VERTICAL_BAR_HEIGHT,
				     2 * widget->style->ythickness + 3 +
				     logical_rect.height + 2 * TEXT_SPACING);
	}
      else
	{
	  requisition->width = MIN_VERTICAL_BAR_WIDTH;
	  requisition->height = MIN_VERTICAL_BAR_HEIGHT;
	}
    }
}

static void
gtk_progress_bar_act_mode_enter (GtkProgress *progress)
{
  GtkProgressBar *pbar;
  GtkWidget *widget;
  gint size;

  pbar = GTK_PROGRESS_BAR (progress);
  widget = GTK_WIDGET (progress);

  /* calculate start pos */

  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
    {
      size = MAX (2, widget->allocation.width / pbar->activity_blocks);

      if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT)
	{
	  pbar->activity_pos = widget->style->xthickness;
	  pbar->activity_dir = 0;
	}
      else
	{
	  pbar->activity_pos = widget->allocation.width - 
	    widget->style->xthickness - (widget->allocation.height - 
		widget->style->ythickness * 2);
	  pbar->activity_dir = 1;
	}
    }
  else
    {
      size = MAX (2, widget->allocation.height / pbar->activity_blocks);

      if (pbar->orientation == GTK_PROGRESS_TOP_TO_BOTTOM)
	{
	  pbar->activity_pos = widget->style->ythickness;
	  pbar->activity_dir = 0;
	}
      else
	{
	  pbar->activity_pos = widget->allocation.height -
	    widget->style->ythickness - (widget->allocation.width - 
		widget->style->xthickness * 2);
	  pbar->activity_dir = 1;
	}
    }
}

static void
gtk_progress_bar_paint (GtkProgress *progress)
{
  GtkProgressBar *pbar;
  GtkWidget *widget;
  gint amount;
  gint block_delta = 0;
  gint space = 0;
  gint i;
  gint x;
  gint y;
  gdouble percentage;
  gint size;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (progress));

  pbar = GTK_PROGRESS_BAR (progress);
  widget = GTK_WIDGET (progress);

  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
    space = widget->allocation.width -
      2 * widget->style->xthickness;
  else
    space = widget->allocation.height -
      2 * widget->style->ythickness;

  percentage = gtk_progress_get_current_percentage (progress);

  if (progress->offscreen_pixmap)
    {
      gtk_paint_box (widget->style,
		     progress->offscreen_pixmap,
		     GTK_STATE_NORMAL, GTK_SHADOW_IN, 
		     NULL, widget, "trough",
		     0, 0,
		     widget->allocation.width,
		     widget->allocation.height);
      
      if (progress->activity_mode)
	{
	  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
	      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
	    {
	      size = MAX (2, widget->allocation.width / pbar->activity_blocks);
	      
	      gtk_paint_box (widget->style,
			     progress->offscreen_pixmap,
			     GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
			     NULL, widget, "bar",
			     pbar->activity_pos,
			     widget->style->ythickness,
			     size,
			     widget->allocation.height - widget->style->ythickness * 2);
	      return;
	    }
	  else
	    {
	      size = MAX (2, widget->allocation.height / pbar->activity_blocks);
	      
	      gtk_paint_box (widget->style,
			     progress->offscreen_pixmap,
			     GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
			     NULL, widget, "bar",
			     widget->style->xthickness,
			     pbar->activity_pos,
			     widget->allocation.width - widget->style->xthickness * 2,
			     size);
	      return;
	    }
	}
      
      amount = percentage * space;
      
      if (amount > 0)
	{
	  switch (pbar->orientation)
	    {
	      
	    case GTK_PROGRESS_LEFT_TO_RIGHT:
	      
	      if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS)
		{
		  gtk_paint_box (widget->style,
				 progress->offscreen_pixmap,
				 GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				 NULL, widget, "bar",
				 widget->style->xthickness,
				 widget->style->ythickness,
				 amount,
				 widget->allocation.height - widget->style->ythickness * 2);
		}
	      else
		{
		  x = widget->style->xthickness;
		  
		  for (i = 0; i <= pbar->in_block; i++)
		    {
		      block_delta = (((i + 1) * space) / pbar->blocks)
			- ((i * space) / pbar->blocks);
		      
		      gtk_paint_box (widget->style,
				     progress->offscreen_pixmap,
				     GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				     NULL, widget, "bar",
				     x,
				     widget->style->ythickness,
				     block_delta,
				     widget->allocation.height - widget->style->ythickness * 2);
		      
		      x +=  block_delta;
		    }
		}
	      break;
	      
	    case GTK_PROGRESS_RIGHT_TO_LEFT:
	      
	      if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS)
		{
		  gtk_paint_box (widget->style,
				 progress->offscreen_pixmap,
				 GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				 NULL, widget, "bar",
				 widget->allocation.width - 
				 widget->style->xthickness - amount,
				 widget->style->ythickness,
				 amount,
				 widget->allocation.height -
				 widget->style->ythickness * 2);
		}
	      else
		{
		  x = widget->allocation.width - 
		    widget->style->xthickness;
		  
		  for (i = 0; i <= pbar->in_block; i++)
		    {
		      block_delta = (((i + 1) * space) / pbar->blocks) -
			((i * space) / pbar->blocks);
		      
		      x -=  block_delta;
		      
		      gtk_paint_box (widget->style,
				     progress->offscreen_pixmap,
				     GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				     NULL, widget, "bar",
				     x,
				     widget->style->ythickness,
				     block_delta,
				     widget->allocation.height -
				     widget->style->ythickness * 2);
		    }
		}
	      break;

	    case GTK_PROGRESS_BOTTOM_TO_TOP:

	      if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS)
		{
		  gtk_paint_box (widget->style,
				 progress->offscreen_pixmap,
				 GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				 NULL, widget, "bar",
				 widget->style->xthickness,
				 widget->allocation.height - 
				 widget->style->ythickness - amount,
				 widget->allocation.width -
				 widget->style->xthickness * 2,
				 amount);
		}
	      else
		{
		  y = widget->allocation.height - 
		    widget->style->ythickness;
		  
		  for (i = 0; i <= pbar->in_block; i++)
		    {
		      block_delta = (((i + 1) * space) / pbar->blocks) -
			((i * space) / pbar->blocks);
		      
		      y -= block_delta;
		      
		      gtk_paint_box (widget->style,
				     progress->offscreen_pixmap,
				     GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				     NULL, widget, "bar",
				     widget->style->xthickness,
				     y,
				     widget->allocation.width - 
				     widget->style->xthickness * 2,
				     block_delta);
		    }
		}
	      break;
	      
	    case GTK_PROGRESS_TOP_TO_BOTTOM:
	      
	      if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS)
		{
		  gtk_paint_box (widget->style,
				 progress->offscreen_pixmap,
				 GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				 NULL, widget, "bar",
				 widget->style->xthickness,
				 widget->style->ythickness,
				 widget->allocation.width -
				 widget->style->xthickness * 2,
				 amount);
		}
	      else
		{
		  y = widget->style->ythickness;
		  
		  for (i = 0; i <= pbar->in_block; i++)
		    {
		      
		      block_delta = (((i + 1) * space) / pbar->blocks)
			- ((i * space) / pbar->blocks);
		      
		      gtk_paint_box (widget->style,
				     progress->offscreen_pixmap,
				     GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				     NULL, widget, "bar",
				     widget->style->xthickness,
				     y,
				     widget->allocation.width -
				     widget->style->xthickness * 2,
				     block_delta);
		      
		      y += block_delta;
		    }
		}
	      break;
	      
	    default:
	      break;
	    }
	}
      
      if (progress->show_text && pbar->bar_style != GTK_PROGRESS_DISCRETE)
	{
	  gint x;
	  gint y;
	  gchar *buf;
	  GdkRectangle rect;
	  PangoLayout *layout;
	  PangoRectangle logical_rect;

	  buf = gtk_progress_get_current_text (progress);

	  layout = gtk_widget_create_pango_layout (widget, buf);
	  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	  
	  x = widget->style->xthickness + 1 + 
	    (widget->allocation.width - 2 * widget->style->xthickness -
	     3 - logical_rect.width)
	    * progress->x_align; 

	  y = widget->style->ythickness + 1 +
	    (widget->allocation.height - 2 * widget->style->ythickness -
	     3 - logical_rect.height)
	    * progress->y_align;

	  rect.x = widget->style->xthickness + 1;
	  rect.y = widget->style->ythickness + 1;
	  rect.width = widget->allocation.width -
	    2 * widget->style->xthickness - 3;
	  rect.height = widget->allocation.height -
	    2 * widget->style->ythickness - 3;
      
          gtk_paint_layout (widget->style,
                            progress->offscreen_pixmap,
                            GTK_WIDGET_STATE (widget),
			    FALSE,
                            &rect,
                            widget,
                            "progressbar",
                            x, y,
                            layout);

          g_object_unref (G_OBJECT (layout));
	  g_free (buf);
 	}
    }
}

/*******************************************************************/

/**
 * gtk_progress_bar_set_fraction:
 * @pbar: a #GtkProgressBar
 * @fraction: fraction of the task that's been completed
 * 
 * Causes the progress bar to "fill in" the given fraction
 * of the bar. The fraction should be between 0.0 and 1.0,
 * inclusive.
 * 
 **/
void
gtk_progress_bar_set_fraction (GtkProgressBar *pbar,
                               gdouble         fraction)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  /* If we know the percentage, we don't want activity mode. */
  gtk_progress_set_activity_mode (GTK_PROGRESS (pbar), FALSE);
  
  /* We use the deprecated GtkProgress interface internally.
   * Once everything's been deprecated for a good long time,
   * we can clean up all this code.
   */
  gtk_progress_set_percentage (GTK_PROGRESS (pbar), fraction);

  g_object_notify (G_OBJECT (pbar), "fraction");
}

/**
 * gtk_progress_bar_pulse:
 * @pbar: a #GtkProgressBar
 * 
 * Indicates that some progress is made, but you don't know how much.
 * Causes the progress bar to enter "activity mode," where a block
 * bounces back and forth. Each call to gtk_progress_bar_pulse()
 * causes the block to move by a little bit (the amount of movement
 * per pulse is determined by gtk_progress_bar_set_pulse_step()).
 **/
void
gtk_progress_bar_pulse (GtkProgressBar *pbar)
{  
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  /* If we don't know the percentage, we must want activity mode. */
  gtk_progress_set_activity_mode (GTK_PROGRESS (pbar), TRUE);

  /* Sigh. */
  gtk_progress_bar_real_update (GTK_PROGRESS (pbar));
}

/**
 * gtk_progress_bar_set_text:
 * @pbar: a #GtkProgressBar
 * @text: a UTF-8 string
 * 
 * Causes the given @text to appear superimposed on the progress bar.
 **/
void
gtk_progress_bar_set_text (GtkProgressBar *pbar,
                           const gchar    *text)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  
  if (text && *text)
    {
      gtk_progress_set_show_text (GTK_PROGRESS (pbar), TRUE);
      gtk_progress_set_format_string (GTK_PROGRESS (pbar), text);
    }
  else
    {
      gtk_progress_set_show_text (GTK_PROGRESS (pbar), FALSE);
      gtk_progress_set_format_string (GTK_PROGRESS (pbar), "");
    }

  /* We don't support formats in this interface */
  GTK_PROGRESS (pbar)->use_text_format = FALSE;
  
  g_object_notify (G_OBJECT (pbar), "text");
}

/**
 * gtk_progress_bar_set_pulse_step:
 * @pbar: a #GtkProgressBar
 * @fraction: fraction between 0.0 and 1.0
 * 
 * Sets the fraction of total progress bar length to move the
 * bouncing block for each call to gtk_progress_bar_pulse().
 **/
void
gtk_progress_bar_set_pulse_step   (GtkProgressBar *pbar,
                                   gdouble         fraction)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  
  pbar->pulse_fraction = fraction;

  g_object_notify (G_OBJECT (pbar), "pulse_step");
}

void
gtk_progress_bar_update (GtkProgressBar *pbar,
			 gdouble         percentage)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  /* Use of gtk_progress_bar_update() is deprecated ! 
   * Use gtk_progress_bar_set_percentage ()
   */   

  gtk_progress_set_percentage (GTK_PROGRESS (pbar), percentage);
}

/**
 * gtk_progress_bar_set_orientation:
 * @pbar: a #GtkProgressBar
 * @orientation: orientation of the progress bar
 * 
 * Causes the progress bar to switch to a different orientation
 * (left-to-right, right-to-left, top-to-bottom, or bottom-to-top). 
 **/
void
gtk_progress_bar_set_orientation (GtkProgressBar           *pbar,
				  GtkProgressBarOrientation orientation)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->orientation != orientation)
    {
      pbar->orientation = orientation;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (pbar)))
	gtk_widget_queue_resize (GTK_WIDGET (pbar));

      g_object_notify (G_OBJECT (pbar), "orientation");
    }
}

/**
 * gtk_progress_bar_get_text:
 * @pbar: a #GtkProgressBar
 * 
 * Retrieves the text displayed superimposed on the progress bar,
 * if any, otherwise %NULL. The return value is a reference
 * to the text, not a copy of it, so will become invalid
 * if you change the text in the progress bar.
 * 
 * Return value: text, or %NULL; this string is owned by the widget
 * and should not be modified or freed.
 **/
G_CONST_RETURN gchar*
gtk_progress_bar_get_text (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), NULL);

  if (GTK_PROGRESS (pbar)->use_text_format)
    return NULL;
  else
    return GTK_PROGRESS (pbar)->format;
}

/**
 * gtk_progress_bar_get_fraction:
 * @pbar: a #GtkProgressBar
 * 
 * Returns the current fraction of the task that's been completed.
 * 
 * Return value: a fraction from 0.0 to 1.0
 **/
gdouble
gtk_progress_bar_get_fraction (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return gtk_progress_get_current_percentage (GTK_PROGRESS (pbar));
}

/**
 * gtk_progress_bar_get_pulse_step:
 * @pbar: a #GtkProgressBar
 * 
 * Retrieves the pulse step set with gtk_progress_bar_set_pulse_step()
 * 
 * Return value: a fraction from 0.0 to 1.0
 **/
gdouble
gtk_progress_bar_get_pulse_step (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->pulse_fraction;
}

/**
 * gtk_progress_bar_get_orientation:
 * @pbar: a #GtkProgressBar
 * 
 * Retrieves the current progress bar orientation.
 * 
 * Return value: orientation of the progress bar
 **/
GtkProgressBarOrientation
gtk_progress_bar_get_orientation (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->orientation;
}

void
gtk_progress_bar_set_bar_style (GtkProgressBar     *pbar,
				GtkProgressBarStyle bar_style)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->bar_style != bar_style)
    {
      pbar->bar_style = bar_style;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (pbar)))
	gtk_widget_queue_resize (GTK_WIDGET (pbar));

      g_object_notify (G_OBJECT (pbar), "bar_style");
    }
}

void
gtk_progress_bar_set_discrete_blocks (GtkProgressBar *pbar,
				      guint           blocks)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (blocks > 1);

  if (pbar->blocks != blocks)
    {
      pbar->blocks = blocks;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (pbar)))
	gtk_widget_queue_resize (GTK_WIDGET (pbar));

      g_object_notify (G_OBJECT (pbar), "discrete_blocks");
    }
}

void
gtk_progress_bar_set_activity_step (GtkProgressBar *pbar,
                                    guint           step)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->activity_step != step)
    {
      pbar->activity_step = step;
      g_object_notify (G_OBJECT (pbar), "activity_step");
    }
}

void
gtk_progress_bar_set_activity_blocks (GtkProgressBar *pbar,
				      guint           blocks)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (blocks > 1);

  if (pbar->activity_blocks != blocks)
    {
      pbar->activity_blocks = blocks;
      g_object_notify (G_OBJECT (pbar), "activity_blocks");
    }
}
