/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GTK Calendar Widget
 * Copyright (C) 1998 Cesar Miquel, Shawn T. Amundson and Mattias Gr�nlund
 * 
 * lib_date routines
 * Copyright (c) 1995, 1996, 1997, 1998 by Steffen Beyer
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <glib/gprintf.h>

#include "gtkcalendar.h"
#include "gtkmarshalers.h"
#include "gdk/gdkkeysyms.h"

/***************************************************************************/
/* The following date routines are taken from the lib_date package.  Keep
 * them seperate in case we want to update them if a newer lib_date comes
 * out with fixes.  */

typedef	 unsigned   int	    N_int;
typedef	 unsigned   long    N_long;
typedef	 signed	    long    Z_long;
typedef enum { false = FALSE , true = TRUE } boolean;

#define and	    &&	    /* logical (boolean) operators: lower case */
#define or	    ||

static const N_int month_length[2][13] =
{
  { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const N_int days_in_months[2][14] =
{
  { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
  { 0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

static Z_long  calc_days(N_int year, N_int mm, N_int dd);
static N_int   day_of_week(N_int year, N_int mm, N_int dd);
static Z_long  dates_difference(N_int year1, N_int mm1, N_int dd1,
				N_int year2, N_int mm2, N_int dd2);
static N_int   weeks_in_year(N_int year);

static boolean 
leap(N_int year)
{
  return((((year % 4) == 0) and ((year % 100) != 0)) or ((year % 400) == 0));
}

static N_int 
day_of_week(N_int year, N_int mm, N_int dd)
{
  Z_long  days;
  
  days = calc_days(year, mm, dd);
  if (days > 0L)
    {
      days--;
      days %= 7L;
      days++;
    }
  return( (N_int) days );
}

static N_int weeks_in_year(N_int year)
{
  return(52 + ((day_of_week(year,1,1)==4) or (day_of_week(year,12,31)==4)));
}

static boolean 
check_date(N_int year, N_int mm, N_int dd)
{
  if (year < 1) return(false);
  if ((mm < 1) or (mm > 12)) return(false);
  if ((dd < 1) or (dd > month_length[leap(year)][mm])) return(false);
  return(true);
}

static N_int 
week_number(N_int year, N_int mm, N_int dd)
{
  N_int first;
  
  first = day_of_week(year,1,1) - 1;
  return( (N_int) ( (dates_difference(year,1,1, year,mm,dd) + first) / 7L ) +
	  (first < 4) );
}

static Z_long 
year_to_days(N_int year)
{
  return( year * 365L + (year / 4) - (year / 100) + (year / 400) );
}


static Z_long 
calc_days(N_int year, N_int mm, N_int dd)
{
  boolean lp;
  
  if (year < 1) return(0L);
  if ((mm < 1) or (mm > 12)) return(0L);
  if ((dd < 1) or (dd > month_length[(lp = leap(year))][mm])) return(0L);
  return( year_to_days(--year) + days_in_months[lp][mm] + dd );
}

static boolean 
week_of_year(N_int *week, N_int *year, N_int mm, N_int dd)
{
  if (check_date(*year,mm,dd))
    {
      *week = week_number(*year,mm,dd);
      if (*week == 0) 
	*week = weeks_in_year(--(*year));
      else if (*week > weeks_in_year(*year))
	{
	  *week = 1;
	  (*year)++;
	}
      return(true);
    }
  return(false);
}

static Z_long 
dates_difference(N_int year1, N_int mm1, N_int dd1,
		 N_int year2, N_int mm2, N_int dd2)
{
  return( calc_days(year2, mm2, dd2) - calc_days(year1, mm1, dd1) );
}

/*** END OF lib_date routines ********************************************/

/* Spacing around day/week headers and main area, inside those windows */
#define CALENDAR_MARGIN		 0
/* Spacing around day/week headers and main area, outside those windows */
#define INNER_BORDER		 4
/* Separation between day headers and main area */
#define CALENDAR_YSEP		 4
/* Separation between week headers and main area */
#define CALENDAR_XSEP		 4

#define DAY_XSEP		 0 /* not really good for small calendar */
#define DAY_YSEP		 0 /* not really good for small calendar */

/* Color usage */
#define HEADER_FG_COLOR(widget)		 (& (widget)->style->fg[GTK_WIDGET_STATE (widget)])
#define HEADER_BG_COLOR(widget)		 (& (widget)->style->bg[GTK_WIDGET_STATE (widget)])
#define SELECTED_BG_COLOR(widget)	 (& (widget)->style->base[GTK_WIDGET_HAS_FOCUS (widget) ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE])
#define SELECTED_FG_COLOR(widget)	 (& (widget)->style->text[GTK_WIDGET_HAS_FOCUS (widget) ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE])
#define NORMAL_DAY_COLOR(widget)	 (& (widget)->style->fg[GTK_WIDGET_STATE (widget)])
#define PREV_MONTH_COLOR(widget)	 (& (widget)->style->mid[GTK_WIDGET_STATE (widget)])
#define NEXT_MONTH_COLOR(widget)	 (& (widget)->style->mid[GTK_WIDGET_STATE (widget)])
#define MARKED_COLOR(widget)		 (& (widget)->style->fg[GTK_WIDGET_STATE (widget)])
#define BACKGROUND_COLOR(widget)	 (& (widget)->style->base[GTK_WIDGET_STATE (widget)])
#define HIGHLIGHT_BACK_COLOR(widget)	 (& (widget)->style->mid[GTK_WIDGET_STATE (widget)])

enum {
  ARROW_YEAR_LEFT,
  ARROW_YEAR_RIGHT,
  ARROW_MONTH_LEFT,
  ARROW_MONTH_RIGHT
};

enum {
  MONTH_PREV,
  MONTH_CURRENT,
  MONTH_NEXT
};

enum {
  MONTH_CHANGED_SIGNAL,
  DAY_SELECTED_SIGNAL,
  DAY_SELECTED_DOUBLE_CLICK_SIGNAL,
  PREV_MONTH_SIGNAL,
  NEXT_MONTH_SIGNAL,
  PREV_YEAR_SIGNAL,
  NEXT_YEAR_SIGNAL,
  LAST_SIGNAL
};

static gint gtk_calendar_signals[LAST_SIGNAL] = { 0 };

static GtkWidgetClass *parent_class = NULL;

typedef struct _GtkCalendarPrivateData GtkCalendarPrivateData;
struct _GtkCalendarPrivateData
{
  GdkWindow *header_win;
  GdkWindow *day_name_win;
  GdkWindow *main_win;
  GdkWindow *week_win;
  GdkWindow *arrow_win[4];

  guint header_h;
  guint day_name_h;
  guint main_h;

  guint	     arrow_state[4];
  guint	     arrow_width;
  guint	     max_month_width;
  guint	     max_year_width;
  
  guint day_width;
  guint week_width;

  guint min_day_width;
  guint max_day_char_width;
  guint max_day_char_ascent;
  guint max_day_char_descent;
  guint max_label_char_ascent;
  guint max_label_char_descent;
  guint max_week_char_width;
  
  guint freeze_count;

  /* flags */
  guint dirty_header : 1;
  guint dirty_day_names : 1;
  guint dirty_main : 1;
  guint dirty_week : 1;
};

#define GTK_CALENDAR_PRIVATE_DATA(widget)  (((GtkCalendarPrivateData*)(GTK_CALENDAR (widget)->private_data)))

typedef void (*GtkCalendarSignalDate) (GtkObject *object, guint arg1, guint arg2, guint arg3, gpointer data);

static void gtk_calendar_class_init	(GtkCalendarClass *class);
static void gtk_calendar_init		(GtkCalendar *calendar);
static void gtk_calendar_finalize	(GObject *calendar);
static void gtk_calendar_realize	(GtkWidget *widget);
static void gtk_calendar_unrealize	(GtkWidget *widget);
static void gtk_calendar_size_request	(GtkWidget *widget,
					 GtkRequisition *requisition);
static void gtk_calendar_size_allocate	(GtkWidget *widget,
					 GtkAllocation *allocation);
static gint gtk_calendar_expose		(GtkWidget *widget,
					 GdkEventExpose *event);
static gint gtk_calendar_button_press	(GtkWidget *widget,
					 GdkEventButton *event);
static void gtk_calendar_main_button	(GtkWidget *widget,
					 GdkEventButton *event);
static gint gtk_calendar_motion_notify	(GtkWidget *widget,
					 GdkEventMotion *event);
static gint gtk_calendar_enter_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static gint gtk_calendar_leave_notify	(GtkWidget *widget,
					 GdkEventCrossing *event);
static gint gtk_calendar_key_press	(GtkWidget	   *widget,
					 GdkEventKey	   *event);
static void gtk_calendar_state_changed	(GtkWidget *widget,
					 GtkStateType previous_state);
static void gtk_calendar_style_set	(GtkWidget *widget,
					 GtkStyle  *previous_style);
static void gtk_calendar_paint_header	    (GtkWidget *widget);
static void gtk_calendar_paint_day_names    (GtkWidget *widget);
static void gtk_calendar_paint_week_numbers (GtkWidget *widget);
static void gtk_calendar_paint_main	    (GtkWidget *widget);

static void gtk_calendar_select_and_focus_day (GtkCalendar *calendar,
					       guint        day);

static void gtk_calendar_paint_arrow	(GtkWidget    *widget,
					 guint	       arrow);
static void gtk_calendar_paint_day_num	(GtkWidget    *widget,
					 gint	       day);
static void gtk_calendar_paint_day	(GtkWidget    *widget,
					 gint	       row,
					 gint	       col);
static void gtk_calendar_compute_days	(GtkCalendar  *calendar);
static gint left_x_for_column		(GtkCalendar  *calendar,
					 gint	       column);
static gint top_y_for_row		(GtkCalendar  *calendar,
					 gint	       row);

static char    *default_abbreviated_dayname[7];
static char    *default_monthname[12];

GType
gtk_calendar_get_type (void)
{
  static GType calendar_type = 0;
  
  if (!calendar_type)
    {
      static const GTypeInfo calendar_info =
      {
	sizeof (GtkCalendarClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_calendar_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkCalendar),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_calendar_init,
      };

      calendar_type = g_type_register_static (GTK_TYPE_WIDGET, "GtkCalendar",
					      &calendar_info, 0);
    }
  
  return calendar_type;
}

static void
gtk_calendar_class_init (GtkCalendarClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = (GObjectClass*)  class;
  widget_class = (GtkWidgetClass*) class;
  
  parent_class = g_type_class_peek_parent (class);
  
  gobject_class->finalize = gtk_calendar_finalize;

  widget_class->realize = gtk_calendar_realize;
  widget_class->unrealize = gtk_calendar_unrealize;
  widget_class->expose_event = gtk_calendar_expose;
  widget_class->size_request = gtk_calendar_size_request;
  widget_class->size_allocate = gtk_calendar_size_allocate;
  widget_class->button_press_event = gtk_calendar_button_press;
  widget_class->motion_notify_event = gtk_calendar_motion_notify;
  widget_class->enter_notify_event = gtk_calendar_enter_notify;
  widget_class->leave_notify_event = gtk_calendar_leave_notify;
  widget_class->key_press_event = gtk_calendar_key_press;
  widget_class->style_set = gtk_calendar_style_set;
  widget_class->state_changed = gtk_calendar_state_changed;
  
  class->month_changed = NULL;
  class->day_selected = NULL;
  class->day_selected_double_click = NULL;
  class->prev_month = NULL;
  class->next_month = NULL;
  class->prev_year = NULL;
  class->next_year = NULL;

  gtk_calendar_signals[MONTH_CHANGED_SIGNAL] =
    g_signal_new ("month_changed",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCalendarClass, month_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  gtk_calendar_signals[DAY_SELECTED_SIGNAL] =
    g_signal_new ("day_selected",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCalendarClass, day_selected),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  gtk_calendar_signals[DAY_SELECTED_DOUBLE_CLICK_SIGNAL] =
    g_signal_new ("day_selected_double_click",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCalendarClass, day_selected_double_click),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  gtk_calendar_signals[PREV_MONTH_SIGNAL] =
    g_signal_new ("prev_month",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCalendarClass, prev_month),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  gtk_calendar_signals[NEXT_MONTH_SIGNAL] =
    g_signal_new ("next_month",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCalendarClass, next_month),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  gtk_calendar_signals[PREV_YEAR_SIGNAL] =
    g_signal_new ("prev_year",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCalendarClass, prev_year),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  gtk_calendar_signals[NEXT_YEAR_SIGNAL] =
    g_signal_new ("next_year",
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCalendarClass, next_year),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gtk_calendar_init (GtkCalendar *calendar)
{
  time_t secs;
  struct tm *tm;
  gint i;
  char buffer[255];
  time_t tmp_time;
  GtkWidget *widget;
  GtkCalendarPrivateData *private_data;
  
  widget = GTK_WIDGET (calendar);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
  
  calendar->private_data = g_malloc (sizeof (GtkCalendarPrivateData));
  private_data = GTK_CALENDAR_PRIVATE_DATA (calendar);

  if (!default_abbreviated_dayname[0])
    for (i=0; i<7; i++)
      {
	tmp_time= (i+3)*86400;
	strftime ( buffer, sizeof (buffer), "%a", gmtime (&tmp_time));
	default_abbreviated_dayname[i] = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
      }
  
  if (!default_monthname[0])
    for (i=0; i<12; i++)
      {
	tmp_time=i*2764800;
	strftime ( buffer, sizeof (buffer), "%B", gmtime (&tmp_time));
	default_monthname[i] = g_locale_to_utf8 (buffer, -1, NULL, NULL, NULL);
      }
  
  /* Set defaults */
  secs = time (NULL);
  tm = localtime (&secs);
  calendar->month = tm->tm_mon;
  calendar->year  = 1900 + tm->tm_year;
  
  for (i=0;i<31;i++)
    calendar->marked_date[i] = FALSE;
  calendar->num_marked_dates = 0;
  calendar->selected_day = tm->tm_mday;
  
  calendar->display_flags = ( GTK_CALENDAR_SHOW_HEADING | 
			      GTK_CALENDAR_SHOW_DAY_NAMES );
  
  calendar->highlight_row = -1;
  calendar->highlight_col = -1;
  
  calendar->focus_row = -1;
  calendar->focus_col = -1;
  calendar->xor_gc = NULL;

  private_data->max_year_width = 0;
  private_data->max_month_width = 0;
  private_data->max_day_char_width = 0;
  private_data->max_week_char_width = 0;

  private_data->max_day_char_ascent = 0;
  private_data->max_day_char_descent = 0;
  private_data->max_label_char_ascent = 0;
  private_data->max_label_char_descent = 0;

  private_data->arrow_width = 10;

  private_data->freeze_count = 0;
  
  private_data->dirty_header = 0;
  private_data->dirty_day_names = 0;
  private_data->dirty_week = 0;
  private_data->dirty_main = 0;
}

GtkWidget*
gtk_calendar_new (void)
{
  return g_object_new (GTK_TYPE_CALENDAR, NULL);
}

/* column_from_x: returns the column 0-6 that the
 * x pixel of the xwindow is in */
static gint
column_from_x (GtkCalendar *calendar,
	       gint	    event_x)
{
  gint c, column;
  gint x_left, x_right;
  
  column = -1;
  
  for (c = 0; c < 7; c++)
    {
      x_left = left_x_for_column (calendar, c);
      x_right = x_left + GTK_CALENDAR_PRIVATE_DATA (calendar)->day_width;
      
      if (event_x >= x_left && event_x < x_right)
	{
	  column = c;
	  break;
	}
    }
  
  return column;
}

static gint
row_height (GtkCalendar *calendar)
{
  return (GTK_CALENDAR_PRIVATE_DATA (calendar)->main_h - CALENDAR_MARGIN
	  - ((calendar->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
	     ? CALENDAR_YSEP : CALENDAR_MARGIN)) / 6;
}


/* row_from_y: returns the row 0-5 that the
 * y pixel of the xwindow is in */
static gint
row_from_y (GtkCalendar *calendar,
	    gint	 event_y)
{
  gint r, row;
  gint height;
  gint y_top, y_bottom;
  
  height = row_height (calendar);
  row = -1;
  
  for (r = 0; r < 6; r++)
    {
      y_top = top_y_for_row (calendar, r);
      y_bottom = y_top + height;
      
      if (event_y >= y_top && event_y < y_bottom)
	{
	  row = r;
	  break;
	}
    }
  
  return row;
}

/* left_x_for_column: returns the x coordinate
 * for the left of the column */
static gint
left_x_for_column (GtkCalendar *calendar,
		   gint		column)
{
  gint width;
  gint x_left;
  
  if (gtk_widget_get_direction (GTK_WIDGET (calendar)) == GTK_TEXT_DIR_RTL)
    column = 6 - column;

  width = GTK_CALENDAR_PRIVATE_DATA (calendar)->day_width;
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    x_left = CALENDAR_XSEP + (width + DAY_XSEP) * column;
  else
    x_left = CALENDAR_MARGIN + (width + DAY_XSEP) * column;
  
  return x_left;
}

/* top_y_for_row: returns the y coordinate
 * for the top of the row */
static gint
top_y_for_row (GtkCalendar *calendar,
	       gint	    row)
{
  
  return (GTK_CALENDAR_PRIVATE_DATA (calendar)->main_h 
	  - (CALENDAR_MARGIN + (6 - row)
	     * row_height (calendar)));
}

/* This function should be done by the toolkit, but we don't like the
 * GTK arrows because they don't look good on this widget */
static void
draw_arrow_right (GdkWindow *window,
		  GdkGC	    *gc,
		  gint	     x,
		  gint	     y,
		  gint	     size)
{
  gint i;
  
  for (i = 0; i <= size / 2; i++)
    {
      gdk_draw_line (window, gc,
		     x + i,
		     y + i,
		     x + i,
		     y + size - i);
    }
}

/* This function should be done by the toolkit, but we don't like the
 * GTK arrows because they don't look good on this widget */
static void
draw_arrow_left (GdkWindow *window,
		 GdkGC	   *gc,
		 gint	    x,
		 gint	    y,
		 gint	    size)
{
  gint i;
  
  for (i = 0; i <= size / 2; i++)
    {
      gdk_draw_line (window, gc,
		     x + size/2 - i,
		     y + i,
		     x + size/2 - i,
		     y + size - i);
    }
}

static void
gtk_calendar_set_month_prev (GtkCalendar *calendar)
{
  gint month_len;
  
  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    return;
  
  if (calendar->month == 0)
    {
      calendar->month = 11;
      calendar->year--;
    } 
  else 
    calendar->month--;
  
  month_len = month_length[leap (calendar->year)][calendar->month + 1];
  
  gtk_calendar_freeze (calendar);
  gtk_calendar_compute_days (calendar);
  
  g_signal_emit (calendar,
		 gtk_calendar_signals[PREV_MONTH_SIGNAL],
		 0);
  g_signal_emit (calendar,
		 gtk_calendar_signals[MONTH_CHANGED_SIGNAL],
		 0);
  
  if (month_len < calendar->selected_day)
    {
      calendar->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    {
      if (calendar->selected_day < 0)
	calendar->selected_day = calendar->selected_day + 1 + month_length[leap (calendar->year)][calendar->month + 1];
      gtk_calendar_select_day (calendar, calendar->selected_day);
    }

  gtk_widget_queue_draw (GTK_WIDGET (calendar));
  gtk_calendar_thaw (calendar);
}


static void
gtk_calendar_set_month_next (GtkCalendar *calendar)
{
  gint month_len;
  
  g_return_if_fail (GTK_IS_WIDGET (calendar));
  
  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    return;
  
  
  if (calendar->month == 11)
    {
      calendar->month = 0;
      calendar->year++;
    } 
  else 
    calendar->month++;
  
  gtk_calendar_freeze (calendar);
  gtk_calendar_compute_days (calendar);
  g_signal_emit (calendar,
		 gtk_calendar_signals[NEXT_MONTH_SIGNAL],
		 0);
  g_signal_emit (calendar,
		 gtk_calendar_signals[MONTH_CHANGED_SIGNAL],
		 0);
  
  month_len = month_length[leap (calendar->year)][calendar->month + 1];
  
  if (month_len < calendar->selected_day)
    {
      calendar->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    gtk_calendar_select_day (calendar, calendar->selected_day);

  gtk_widget_queue_draw (GTK_WIDGET (calendar));
  gtk_calendar_thaw (calendar);
}

static void
gtk_calendar_set_year_prev (GtkCalendar *calendar)
{
  gint month_len;
  
  g_return_if_fail (GTK_IS_WIDGET (calendar));
  
  calendar->year--;
  gtk_calendar_freeze (calendar);
  gtk_calendar_compute_days (calendar);
  g_signal_emit (calendar,
		 gtk_calendar_signals[PREV_YEAR_SIGNAL],
		 0);
  g_signal_emit (calendar,
		 gtk_calendar_signals[MONTH_CHANGED_SIGNAL],
		 0);
  
  month_len = month_length[leap (calendar->year)][calendar->month + 1];
  
  if (month_len < calendar->selected_day)
    {
      calendar->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    gtk_calendar_select_day (calendar, calendar->selected_day);
  
  gtk_widget_queue_draw (GTK_WIDGET (calendar));
  gtk_calendar_thaw (calendar);
}

static void
gtk_calendar_set_year_next (GtkCalendar *calendar)
{
  gint month_len;
  GtkWidget *widget;
  
  g_return_if_fail (GTK_IS_WIDGET (calendar));
  
  widget = GTK_WIDGET (calendar);
  
  gtk_calendar_freeze (calendar);
  
  calendar->year++;
  gtk_calendar_compute_days (calendar);
  g_signal_emit (calendar,
		 gtk_calendar_signals[NEXT_YEAR_SIGNAL],
		 0);
  g_signal_emit (calendar,
		 gtk_calendar_signals[MONTH_CHANGED_SIGNAL],
		 0);
  
  month_len = month_length[leap (calendar->year)][calendar->month + 1];
  
  if (month_len < calendar->selected_day)
    {
      calendar->selected_day = 0;
      gtk_calendar_select_day (calendar, month_len);
    }
  else
    gtk_calendar_select_day (calendar, calendar->selected_day);
  
  gtk_widget_queue_draw (GTK_WIDGET (calendar));
  gtk_calendar_thaw (calendar);
}

static void
gtk_calendar_main_button (GtkWidget	 *widget,
			  GdkEventButton *event)
{
  GtkCalendar *calendar;
  gint x, y;
  gint row, col;
  gint day_month;
  gint day;
  
  calendar = GTK_CALENDAR (widget);
  
  x = (gint) (event->x);
  y = (gint) (event->y);
  
  row = row_from_y (calendar, y);
  col = column_from_x (calendar, x);

  /* If row or column isn't found, just return. */
  if (row == -1 || col == -1)
    return;
  
  day_month = calendar->day_month[row][col];

  if (event->type == GDK_BUTTON_PRESS)
    {
      day = calendar->day[row][col];
      
      if (day_month == MONTH_PREV)
	gtk_calendar_set_month_prev (calendar);
      else if (day_month == MONTH_NEXT)
	gtk_calendar_set_month_next (calendar);
      
      if (!GTK_WIDGET_HAS_FOCUS (widget))
	gtk_widget_grab_focus (widget);
	  
      gtk_calendar_select_and_focus_day (calendar, day);

    }
  else if (event->type == GDK_2BUTTON_PRESS)
    {
      if (day_month == MONTH_CURRENT)
	g_signal_emit (calendar,
		       gtk_calendar_signals[DAY_SELECTED_DOUBLE_CLICK_SIGNAL],
		       0);
    }
}

static void
gtk_calendar_realize_arrows (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint i;
  
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /* Arrow windows ------------------------------------- */
  if (! (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
      && (calendar->display_flags & GTK_CALENDAR_SHOW_HEADING))
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = (gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK
			       | GDK_BUTTON_PRESS_MASK	| GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.y = 3;
      attributes.width = private_data->arrow_width;
      attributes.height = private_data->header_h - 7;
      for (i = 0; i < 4; i++)
	{
	  switch (i)
	    {
	    case ARROW_MONTH_LEFT:
	      attributes.x = 3;
	      break;
	    case ARROW_MONTH_RIGHT:
	      attributes.x = (private_data->arrow_width 
			      + private_data->max_month_width);
	      break;
	    case ARROW_YEAR_LEFT:
	      attributes.x = (widget->allocation.width - 2 * widget->style->xthickness
			      - (3 + 2*private_data->arrow_width 
				 + private_data->max_year_width));
	      break;
	    case ARROW_YEAR_RIGHT:
	      attributes.x = (widget->allocation.width - 2 * widget->style->xthickness 
			      - 3 - private_data->arrow_width);
	      break;
	    }
	  private_data->arrow_win[i] = gdk_window_new (private_data->header_win,
						       &attributes, 
						       attributes_mask);
	  private_data->arrow_state[i] = GTK_STATE_NORMAL;
	  gdk_window_set_background (private_data->arrow_win[i],
				     HEADER_BG_COLOR (GTK_WIDGET (calendar)));
	  gdk_window_show (private_data->arrow_win[i]);
	  gdk_window_set_user_data (private_data->arrow_win[i], widget);
	}
    }
  else
    {
      for (i = 0; i < 4; i++)
	private_data->arrow_win[i] = NULL;
    }
}

static void
gtk_calendar_realize_header (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /* Header window ------------------------------------- */
  if (calendar->display_flags & GTK_CALENDAR_SHOW_HEADING)
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.x = widget->style->xthickness;
      attributes.y = widget->style->ythickness;
      attributes.width = widget->allocation.width - 2 * attributes.x;
      attributes.height = private_data->header_h - 2 * attributes.y;
      private_data->header_win = gdk_window_new (widget->window,
					     &attributes, attributes_mask);
      
      gdk_window_set_background (private_data->header_win,
				 HEADER_BG_COLOR (GTK_WIDGET (calendar)));
      gdk_window_show (private_data->header_win);
      gdk_window_set_user_data (private_data->header_win, widget);
      
    }
  else
    {
      private_data->header_win = NULL;
    }
  gtk_calendar_realize_arrows (widget);
}

static void
gtk_calendar_realize_day_names (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /* Day names	window --------------------------------- */
  if ( calendar->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.x = (widget->style->xthickness + INNER_BORDER);
      attributes.y = private_data->header_h + (widget->style->ythickness 
					   + INNER_BORDER);
      attributes.width = (widget->allocation.width 
			  - (widget->style->xthickness + INNER_BORDER) 
			  * 2);
      attributes.height = private_data->day_name_h;
      private_data->day_name_win = gdk_window_new (widget->window,
						   &attributes, 
						   attributes_mask);
      gdk_window_set_background (private_data->day_name_win, 
				 BACKGROUND_COLOR ( GTK_WIDGET ( calendar)));
      gdk_window_show (private_data->day_name_win);
      gdk_window_set_user_data (private_data->day_name_win, widget);
    }
  else
    {
      private_data->day_name_win = NULL;
    }
}

static void
gtk_calendar_realize_week_numbers (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /* Week number window -------------------------------- */
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    {
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      attributes.x = widget->style->xthickness + INNER_BORDER;
      attributes.y = (private_data->header_h + private_data->day_name_h 
		      + (widget->style->ythickness + INNER_BORDER));
      attributes.width = private_data->week_width;
      attributes.height = private_data->main_h;
      private_data->week_win = gdk_window_new (widget->window,
					       &attributes, attributes_mask);
      gdk_window_set_background (private_data->week_win,  
				 BACKGROUND_COLOR (GTK_WIDGET (calendar)));
      gdk_window_show (private_data->week_win);
      gdk_window_set_user_data (private_data->week_win, widget);
    } 
  else
    {
      private_data->week_win = NULL;
    }
}

static void
gtk_calendar_realize (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkGCValues values;

  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  gtk_calendar_compute_days (calendar);
  
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask =  (gtk_widget_get_events (widget) 
			    | GDK_EXPOSURE_MASK |GDK_KEY_PRESS_MASK);
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (widget->parent->window,
				   &attributes, attributes_mask);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  /* Header window ------------------------------------- */
  gtk_calendar_realize_header (widget);
  /* Day names	window --------------------------------- */
  gtk_calendar_realize_day_names (widget);
  /* Week number window -------------------------------- */
  gtk_calendar_realize_week_numbers (widget);
  /* Main Window --------------------------------------	 */
  attributes.event_mask =  (gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK
			    | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			    | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
  
  attributes.x = private_data->week_width + (widget->style->ythickness + INNER_BORDER);
  attributes.y = (private_data->header_h + private_data->day_name_h 
		  + (widget->style->ythickness + INNER_BORDER));
  attributes.width = (widget->allocation.width - attributes.x 
		      - (widget->style->xthickness + INNER_BORDER));
  attributes.height = private_data->main_h;
  private_data->main_win = gdk_window_new (widget->window,
					   &attributes, attributes_mask);
  gdk_window_set_background (private_data->main_win, 
			     BACKGROUND_COLOR ( GTK_WIDGET ( calendar)));
  gdk_window_show (private_data->main_win);
  gdk_window_set_user_data (private_data->main_win, widget);
  gdk_window_set_background (widget->window, BACKGROUND_COLOR (widget));
  gdk_window_show (widget->window);
  gdk_window_set_user_data (widget->window, widget);
  
  /* Set widgets gc */
  calendar->gc = gdk_gc_new (widget->window);

  values.foreground = widget->style->white;
  values.function = GDK_XOR;
  calendar->xor_gc = gdk_gc_new_with_values (widget->window,
					     &values,
					     GDK_GC_FOREGROUND |
					     GDK_GC_FUNCTION);
}

static void
gtk_calendar_unrealize (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint i;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (private_data->header_win)
    {
      for (i = 0; i < 4; i++)
	{
	  if (private_data->arrow_win[i])
	    {
	      gdk_window_set_user_data (private_data->arrow_win[i], NULL);
	      gdk_window_destroy (private_data->arrow_win[i]);
	      private_data->arrow_win[i] = NULL;
	    }
	}
      gdk_window_set_user_data (private_data->header_win, NULL);
      gdk_window_destroy (private_data->header_win);
      private_data->header_win = NULL;
    }
  
  if (private_data->week_win)
    {
      gdk_window_set_user_data (private_data->week_win, NULL);
      gdk_window_destroy (private_data->week_win);
      private_data->week_win = NULL;      
    }
  
  if (private_data->main_win)
    {
      gdk_window_set_user_data (private_data->main_win, NULL);
      gdk_window_destroy (private_data->main_win);
      private_data->main_win = NULL;      
    }
  if (private_data->day_name_win)
    {
      gdk_window_set_user_data (private_data->day_name_win, NULL);
      gdk_window_destroy (private_data->day_name_win);
      private_data->day_name_win = NULL;      
    }
  if (calendar->xor_gc)
    g_object_unref (calendar->xor_gc);
  if (calendar->gc)
    g_object_unref (calendar->gc);
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_calendar_size_request (GtkWidget	  *widget,
			   GtkRequisition *requisition)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  PangoLayout *layout;
  PangoRectangle logical_rect;

  gint height;
  gint i;
  gchar buffer[255];
  gint calendar_margin = CALENDAR_MARGIN;
  gint header_width, main_width;
  gint max_header_height = 0;
  gint focus_width;
  gint focus_padding;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  gtk_widget_style_get (GTK_WIDGET (widget),
			"focus-line-width", &focus_width,
			"focus-padding", &focus_padding,
			NULL);

  layout = gtk_widget_create_pango_layout (widget, NULL);
  
  /*
   * Calculate the requisition	width for the widget.
   */
  
  /* Header width */
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_HEADING)
    {
      private_data->max_month_width = 0;
      for (i = 0; i < 12; i++)
	{
	  pango_layout_set_text (layout, default_monthname[i], -1);
	  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	  private_data->max_month_width = MAX (private_data->max_month_width,
					       logical_rect.width + 8);
	  max_header_height = MAX (max_header_height, logical_rect.height); 
	}
      private_data->max_year_width = 0;
      for (i=0; i<10; i++)
	{
	  g_snprintf (buffer, sizeof (buffer), "%d%d%d%d", i,i,i,i);
	  pango_layout_set_text (layout, buffer, -1);	  
	  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	  private_data->max_year_width = MAX (private_data->max_year_width,
					      logical_rect.width + 8);
	  max_header_height = MAX (max_header_height, logical_rect.height); 
	}
    } 
  else 
    {
      private_data->max_month_width = 0;
      private_data->max_year_width = 0;
    }
  
  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    header_width = (private_data->max_month_width 
		    + private_data->max_year_width
		    + 3 * 3);
  else
    header_width = (private_data->max_month_width 
		    + private_data->max_year_width
		    + 4 * private_data->arrow_width + 3 * 3);

  /* Mainwindow labels width */
  
  private_data->max_day_char_width = 0;
  private_data->min_day_width = 0;
  private_data->max_label_char_ascent = 0;
  private_data->max_label_char_descent = 0;

  for (i = 0; i < 9; i++)
    {
      g_snprintf (buffer, sizeof (buffer), "%d%d", i, i);
      pango_layout_set_text (layout, buffer, -1);	  
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
      private_data->min_day_width = MAX (private_data->min_day_width,
					 logical_rect.width);

      private_data->max_day_char_ascent = MAX (private_data->max_label_char_ascent,
					       PANGO_ASCENT (logical_rect));
      private_data->max_day_char_descent = MAX (private_data->max_label_char_descent, 
						PANGO_DESCENT (logical_rect));
    }
  /* We add one to max_day_char_width to be able to make the marked day "bold" */
  private_data->max_day_char_width = private_data->min_day_width / 2 + 1;
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    for (i = 0; i < 7; i++)
      {
	pango_layout_set_text (layout, default_abbreviated_dayname[i], -1);
	pango_layout_line_get_pixel_extents (pango_layout_get_lines (layout)->data, NULL, &logical_rect);

	private_data->min_day_width = MAX (private_data->min_day_width, logical_rect.width);
	private_data->max_label_char_ascent = MAX (private_data->max_label_char_ascent,
						   PANGO_ASCENT (logical_rect));
	private_data->max_label_char_descent = MAX (private_data->max_label_char_descent, 
						    PANGO_DESCENT (logical_rect));
      }
  
  private_data->max_week_char_width = 0;
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    for (i = 0; i < 9; i++)
      {
	g_snprintf (buffer, sizeof (buffer), "%d%d", i, i);
	pango_layout_set_text (layout, buffer, -1);	  
	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	private_data->max_week_char_width = MAX (private_data->max_week_char_width,
						 logical_rect.width / 2);
      }
  
  main_width = (7 * (private_data->min_day_width + (focus_padding + focus_width) * 2) + (DAY_XSEP * 6) + CALENDAR_MARGIN * 2
		+ (private_data->max_week_char_width
		   ? private_data->max_week_char_width * 2 + (focus_padding + focus_width) * 2 + CALENDAR_XSEP * 2
		   : 0));
  
  
  requisition->width = MAX (header_width, main_width + INNER_BORDER * 2) + widget->style->xthickness * 2;
  
  /*
   * Calculate the requisition height for the widget.
   */
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_HEADING)
    {
      private_data->header_h = (max_header_height + CALENDAR_YSEP * 2);
    }
  else
    {
      private_data->header_h = 0;
    }
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_DAY_NAMES)
    {
      private_data->day_name_h = (private_data->max_label_char_ascent
				  + private_data->max_label_char_descent
				  + 2 * (focus_padding + focus_width) + calendar_margin);
      calendar_margin = CALENDAR_YSEP;
    } 
  else
    {
      private_data->day_name_h = 0;
    }

  private_data->main_h = (CALENDAR_MARGIN + calendar_margin
			  + 6 * (private_data->max_day_char_ascent
				 + private_data->max_day_char_descent 
				 + 2 * (focus_padding + focus_width))
			  + DAY_YSEP * 5);
  
  height = (private_data->header_h + private_data->day_name_h 
	    + private_data->main_h);
  
  requisition->height = height + (widget->style->ythickness + INNER_BORDER) * 2;

  g_object_unref (layout);
}

static void
gtk_calendar_size_allocate (GtkWidget	  *widget,
			    GtkAllocation *allocation)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint xthickness = widget->style->xthickness;
  gint ythickness = widget->style->xthickness;
  
  widget->allocation = *allocation;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    {
      private_data->day_width = (private_data->min_day_width
			     * ((allocation->width - (xthickness + INNER_BORDER) * 2
				 - (CALENDAR_MARGIN * 2) -  (DAY_XSEP * 6) - CALENDAR_XSEP * 2))
			     / (7 * private_data->min_day_width + private_data->max_week_char_width * 2));
      private_data->week_width = ((allocation->width - (xthickness + INNER_BORDER) * 2
			       - (CALENDAR_MARGIN * 2) - (DAY_XSEP * 6) - CALENDAR_XSEP * 2 )
			      - private_data->day_width * 7 + CALENDAR_MARGIN + CALENDAR_XSEP);
    } 
  else 
    {
      private_data->day_width = (allocation->width
			     - (xthickness + INNER_BORDER) * 2
			     - (CALENDAR_MARGIN * 2)
			     - (DAY_XSEP * 6))/7;
      private_data->week_width = 0;
    }
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      if (private_data->header_win)
	gdk_window_move_resize (private_data->header_win,
				xthickness, ythickness,
				allocation->width - 2 * xthickness, private_data->header_h);
      if (private_data->arrow_win[ARROW_MONTH_LEFT])
	gdk_window_move_resize (private_data->arrow_win[ARROW_MONTH_LEFT],
				3, 3,
				private_data->arrow_width,
				private_data->header_h - 7);
      if (private_data->arrow_win[ARROW_MONTH_RIGHT])
	gdk_window_move_resize (private_data->arrow_win[ARROW_MONTH_RIGHT],
				(private_data->arrow_width 
				 + private_data->max_month_width), 
				3,
				private_data->arrow_width,
				private_data->header_h - 7);
      if (private_data->arrow_win[ARROW_YEAR_LEFT])
	gdk_window_move_resize (private_data->arrow_win[ARROW_YEAR_LEFT],
				(allocation->width - 2 * xthickness
				 - (3 + 2*private_data->arrow_width 
				    + private_data->max_year_width)),
				3,
				private_data->arrow_width,
				private_data->header_h - 7);
      if (private_data->arrow_win[ARROW_YEAR_RIGHT])
	gdk_window_move_resize (private_data->arrow_win[ARROW_YEAR_RIGHT],
				(allocation->width - 2 * xthickness 
				 - 3 - private_data->arrow_width), 
				3,
				private_data->arrow_width,
				private_data->header_h - 7);
      if (private_data->day_name_win)
	gdk_window_move_resize (private_data->day_name_win,
				xthickness + INNER_BORDER,
				private_data->header_h + (widget->style->ythickness + INNER_BORDER),
				allocation->width - (xthickness + INNER_BORDER) * 2,
				private_data->day_name_h);
      if (private_data->week_win)
	gdk_window_move_resize (private_data->week_win,
				(xthickness + INNER_BORDER),
				private_data->header_h + private_data->day_name_h
				+ (widget->style->ythickness + INNER_BORDER),
				private_data->week_width,
				private_data->main_h);
      gdk_window_move_resize (private_data->main_win,
			      private_data->week_width + (xthickness + INNER_BORDER),
			      private_data->header_h + private_data->day_name_h
			      + (widget->style->ythickness + INNER_BORDER),
			      allocation->width 
			      - private_data->week_width 
			      - (xthickness + INNER_BORDER) * 2,
			      private_data->main_h);
    }
}

static gboolean
gtk_calendar_expose (GtkWidget	    *widget,
		     GdkEventExpose *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;

  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (event->window == private_data->main_win)
	gtk_calendar_paint_main (widget);
      
      if (event->window == private_data->header_win)
	gtk_calendar_paint_header (widget);
      
      if (event->window == private_data->day_name_win)
	gtk_calendar_paint_day_names (widget);
      
      if (event->window == private_data->week_win)
	gtk_calendar_paint_week_numbers (widget);
      if (event->window == widget->window)
	{
	  gtk_paint_shadow (widget->style, widget->window, GTK_WIDGET_STATE (widget),
			    GTK_SHADOW_IN, NULL, widget, "calendar",
			    0, 0, widget->allocation.width, widget->allocation.height);
	}
    }
  
  return FALSE;
}

static void
gtk_calendar_paint_header (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GdkGC *gc;
  char buffer[255];
  int x, y;
  gint header_width, cal_height;
  gint max_month_width;
  gint max_year_width;
  GtkCalendarPrivateData *private_data;
  PangoLayout *layout;
  PangoRectangle logical_rect;

  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  if (private_data->freeze_count)
    {
      private_data->dirty_header = 1;
      return;
    }
  private_data->dirty_header = 0;
  gc = calendar->gc;
  
  /* Clear window */
  gdk_window_clear (private_data->header_win);
  
  header_width = widget->allocation.width - 2 * widget->style->xthickness;
  cal_height = widget->allocation.height;
  
  max_month_width = private_data->max_month_width;
  max_year_width = private_data->max_year_width;
  
  gdk_gc_set_foreground (gc, BACKGROUND_COLOR (GTK_WIDGET (calendar)));
  gtk_paint_shadow (widget->style, private_data->header_win,
		    GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		    NULL, widget, "calendar",
		    0, 0, header_width, private_data->header_h);
  
  
  g_snprintf (buffer, sizeof (buffer), "%d", calendar->year);
  layout = gtk_widget_create_pango_layout (widget, buffer);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
  
  /* Draw title */
  y = (private_data->header_h - logical_rect.height) / 2;
  
  /* Draw year and its arrows */
  
  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    x = header_width - (3 + max_year_width
			- (max_year_width - logical_rect.width)/2);
  else
    x = header_width - (3 + private_data->arrow_width + max_year_width
			- (max_year_width - logical_rect.width)/2);
    
  
  gdk_gc_set_foreground (gc, HEADER_FG_COLOR (GTK_WIDGET (calendar)));
  gdk_draw_layout (private_data->header_win, gc, x, y, layout);
  
  /* Draw month */
  g_snprintf (buffer, sizeof (buffer), "%s", default_monthname[calendar->month]);
  pango_layout_set_text (layout, buffer, -1);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

  if (calendar->display_flags & GTK_CALENDAR_NO_MONTH_CHANGE)
    x = 3 + (max_month_width - logical_rect.width) / 2;
  else
    x = 3 + private_data->arrow_width + (max_month_width - logical_rect.width)/2;

  gdk_draw_layout (private_data->header_win, gc, x, y, layout);
  
  gdk_gc_set_foreground (gc, BACKGROUND_COLOR (GTK_WIDGET (calendar)));
  
  gtk_calendar_paint_arrow (widget, ARROW_MONTH_LEFT);
  gtk_calendar_paint_arrow (widget, ARROW_MONTH_RIGHT);
  gtk_calendar_paint_arrow (widget, ARROW_YEAR_LEFT);
  gtk_calendar_paint_arrow (widget, ARROW_YEAR_RIGHT);

  g_object_unref (layout);
}

static void
gtk_calendar_paint_day_names (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GdkGC *gc;
  char buffer[255];
  int day,i;
  int day_width, cal_width;
  gint cal_height;
  int day_wid_sep;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  GtkCalendarPrivateData *private_data;
  gint focus_padding;
  gint focus_width;
  
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  gc = calendar->gc;
  
  gtk_widget_style_get (GTK_WIDGET (widget),
			"focus-line-width", &focus_width,
			"focus-padding", &focus_padding,
			NULL);
  /*
   * Handle freeze/thaw functionality
   */
  
  if (private_data->freeze_count)
    {
      private_data->dirty_day_names = 1;
      return;
    }
  private_data->dirty_day_names = 0;
  
  /*
   * Clear the window
   */
  
  gdk_window_clear (private_data->day_name_win);
  
  day_width = private_data->day_width;
  cal_width = widget->allocation.width;
  cal_height = widget->allocation.height;
  day_wid_sep = day_width + DAY_XSEP;
  
  /*
   * Draw rectangles as inverted background for the labels.
   */
  
  gdk_gc_set_foreground (gc, SELECTED_BG_COLOR (widget));
  gdk_draw_rectangle (private_data->day_name_win, gc, TRUE,
		      CALENDAR_MARGIN, CALENDAR_MARGIN,
		      cal_width-CALENDAR_MARGIN * 2,
		      private_data->day_name_h - CALENDAR_MARGIN);
  
  if (calendar->display_flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
    gdk_draw_rectangle (private_data->day_name_win, gc, TRUE,
			CALENDAR_MARGIN,
			private_data->day_name_h - CALENDAR_YSEP,
			private_data->week_width - CALENDAR_YSEP - CALENDAR_MARGIN,
			CALENDAR_YSEP);
  
  /*
   * Write the labels
   */

  layout = gtk_widget_create_pango_layout (widget, NULL);
  
  gdk_gc_set_foreground (gc, SELECTED_FG_COLOR (widget));
  for (i = 0; i < 7; i++)
    {
      if (gtk_widget_get_direction (GTK_WIDGET (calendar)) == GTK_TEXT_DIR_RTL)
	day = 6 - i;
      else
	day = i;
      if (calendar->display_flags & GTK_CALENDAR_WEEK_START_MONDAY)
	day= (day+1)%7;
      g_snprintf (buffer, sizeof (buffer), "%s", default_abbreviated_dayname[day]);

      pango_layout_set_text (layout, buffer, -1);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      gdk_draw_layout (private_data->day_name_win, gc, 
		       (CALENDAR_MARGIN +
			+ private_data->week_width
			+ (private_data->week_width ? CALENDAR_XSEP : 0)
			+ day_wid_sep * i
			+ (day_width - logical_rect.width)/2),
		       CALENDAR_MARGIN + focus_width + focus_padding + logical_rect.y,
		       layout);
    }
  
  g_object_unref (layout);
}

static void
gtk_calendar_paint_week_numbers (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GdkGC *gc;
  gint row, week = 0, year;
  gint x_loc;
  char buffer[3];
  gint y_loc, day_height;
  GtkCalendarPrivateData *private_data;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint focus_padding;
  gint focus_width;
  
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  g_return_if_fail (widget->window != NULL);
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  gc = calendar->gc;
  
  /*
   * Handle freeze/thaw functionality
   */
  
  if (private_data->freeze_count)
    {
      private_data->dirty_week = 1;
      return;
    }
  private_data->dirty_week = 0;
  
  gtk_widget_style_get (GTK_WIDGET (widget),
			"focus-line-width", &focus_width,
			"focus-padding", &focus_padding,
			NULL);
  
  /*
   * Clear the window
   */
  
  gdk_window_clear (private_data->week_win);
  
  /*
   * Draw a rectangle as inverted background for the labels.
   */
  
  gdk_gc_set_foreground (gc, SELECTED_BG_COLOR (widget));
  if (private_data->day_name_win)
    gdk_draw_rectangle (private_data->week_win, gc, TRUE,
			CALENDAR_MARGIN,
			0,
			private_data->week_width - CALENDAR_MARGIN,
			private_data->main_h - CALENDAR_MARGIN);
  else
    gdk_draw_rectangle (private_data->week_win, gc, TRUE,
			CALENDAR_MARGIN,
			CALENDAR_MARGIN,
			private_data->week_width - CALENDAR_MARGIN,
			private_data->main_h - 2 * CALENDAR_MARGIN);
  
  /*
   * Write the labels
   */
  
  layout = gtk_widget_create_pango_layout (widget, NULL);
  
  gdk_gc_set_foreground (gc, SELECTED_FG_COLOR (widget));
  day_height = row_height (calendar);
  for (row = 0; row < 6; row++)
    {
      year = calendar->year;
      if (calendar->day[row][6] < 15 && row > 3 && calendar->month == 11)
	year++;

      g_return_if_fail (week_of_year (&week, &year,		
				      ((calendar->day[row][6] < 15 && row > 3 ? 1 : 0)
				       + calendar->month) % 12 + 1, calendar->day[row][6]));

      g_snprintf (buffer, sizeof (buffer), "%d", week);
      pango_layout_set_text (layout, buffer, -1);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      y_loc = top_y_for_row (calendar, row) + (day_height - logical_rect.height) / 2;

      x_loc = (private_data->week_width
	       - logical_rect.width
	       - CALENDAR_XSEP - focus_padding - focus_width);

      gdk_draw_layout (private_data->week_win, gc, x_loc, y_loc, layout);
    }
  
  g_object_unref (layout);
}

static void
gtk_calendar_paint_day_num (GtkWidget *widget,
			    gint       day)
{
  GtkCalendar *calendar;
  gint r, c, row, col;
  
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  
  calendar = GTK_CALENDAR (widget);
  
  row = -1;
  col = -1;
  for (r = 0; r < 6; r++)
    for (c = 0; c < 7; c++)
      if (calendar->day_month[r][c] == MONTH_CURRENT &&
	  calendar->day[r][c] == day)
	{
	  row = r;
	  col = c;
	}
  
  g_return_if_fail (row != -1);
  g_return_if_fail (col != -1);
  
  gtk_calendar_paint_day (widget, row, col);
}

static void
gtk_calendar_paint_day (GtkWidget *widget,
			gint	   row,
			gint	   col)
{
  GtkCalendar *calendar;
  GdkGC *gc;
  gchar buffer[255];
  gint day;
  gint day_height;
  gint x_left;
  gint x_loc;
  gint y_top;
  gint y_loc;
  gint day_xspace;
  gint focus_width;

  GtkCalendarPrivateData *private_data;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  g_return_if_fail (row < 6);
  g_return_if_fail (col < 7);
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  /*
   * Handle freeze/thaw functionality
   */
  
  if (private_data->freeze_count)
    {
      private_data->dirty_main = 1;
      return;
    }
  
  gtk_widget_style_get (widget, "focus-line-width", &focus_width, NULL);

  day_height = row_height (calendar);
  
  day_xspace = private_data->day_width - private_data->max_day_char_width*2;
  
  day = calendar->day[row][col];
  
  x_left = left_x_for_column (calendar, col);
  x_loc = x_left + private_data->day_width / 2 + private_data->max_day_char_width;
  
  y_top = top_y_for_row (calendar, row);
  
  gdk_window_clear_area (private_data->main_win, x_left, y_top,
			 private_data->day_width, day_height);
  
  gc = calendar->gc;
  
  if (calendar->day_month[row][col] == MONTH_PREV)
    {
      gdk_gc_set_foreground (gc, PREV_MONTH_COLOR (GTK_WIDGET (calendar)));
    } 
  else if (calendar->day_month[row][col] == MONTH_NEXT)
    {
      gdk_gc_set_foreground (gc, NEXT_MONTH_COLOR (GTK_WIDGET (calendar)));
    } 
  else 
    {
      /*
      if (calendar->highlight_row == row && calendar->highlight_col == col)
	{
	  gdk_gc_set_foreground (gc, HIGHLIGHT_BACK_COLOR (GTK_WIDGET (calendar)));
	  gdk_draw_rectangle (private_data->main_win, gc, TRUE, x_left, y_top,
			      private_data->day_width, day_height);
	}
      */
      if (calendar->selected_day == day)
	{
	  gdk_gc_set_foreground (gc, SELECTED_BG_COLOR (GTK_WIDGET (calendar)));
	  gdk_draw_rectangle (private_data->main_win, gc, TRUE, x_left, y_top,
			      private_data->day_width, day_height);
	}
      
      if (calendar->marked_date[day-1])
	gdk_gc_set_foreground (gc, MARKED_COLOR	 (GTK_WIDGET (calendar)));
      else
	gdk_gc_set_foreground (gc, NORMAL_DAY_COLOR (GTK_WIDGET (calendar)));
  
      if (calendar->selected_day == day)
	gdk_gc_set_foreground (gc, SELECTED_FG_COLOR (GTK_WIDGET (calendar)));
      else
	gdk_gc_set_foreground (gc, & (GTK_WIDGET (calendar)->style->fg[GTK_WIDGET_STATE (calendar)]));
    }
    

  g_snprintf (buffer, sizeof (buffer), "%d", day);
  layout = gtk_widget_create_pango_layout (widget, buffer);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
  
  x_loc -= logical_rect.width;

  y_loc = y_top + (day_height - logical_rect.height) / 2;
  gdk_draw_layout (private_data->main_win, gc,
		   x_loc, y_loc, layout);
  if (calendar->marked_date[day-1]
      && calendar->day_month[row][col] == MONTH_CURRENT)
    gdk_draw_layout (private_data->main_win, gc,
		     x_loc-1, y_loc, layout);

  if (GTK_WIDGET_HAS_FOCUS (calendar) 
      && calendar->focus_row == row && calendar->focus_col == col)
    {
      GtkStateType state;

      if (calendar->selected_day == day)
	state = GTK_WIDGET_HAS_FOCUS (widget) ? GTK_STATE_SELECTED : GTK_STATE_ACTIVE;
      else
	state = GTK_STATE_NORMAL;
      
      gtk_paint_focus (widget->style, 
		       private_data->main_win,
		       (calendar->selected_day == day) 
		          ? GTK_STATE_SELECTED : GTK_STATE_NORMAL, 
		       NULL, widget, "calendar-day",
		       x_left, y_top, 
		       private_data->day_width, 
		       day_height);
    }

  g_object_unref (layout);
}


static void
gtk_calendar_paint_main (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint row, col;
  
  g_return_if_fail (GTK_IS_CALENDAR (widget));
  g_return_if_fail (widget->window != NULL);
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  if (private_data->freeze_count)
    {
      private_data->dirty_main = 1;
      return;
    }
  private_data->dirty_main = 0;
  gdk_window_clear (private_data->main_win);
  
  /* gtk_calendar_compute_days (calendar); */ /* REMOVE later */
  
  for (col = 0; col < 7; col++)
    for (row = 0; row < 6; row++)
      gtk_calendar_paint_day (widget, row, col);
}

static void
gtk_calendar_compute_days (GtkCalendar *calendar)
{
  gint month;
  gint year;
  gint ndays_in_month;
  gint ndays_in_prev_month;
  gint first_day;
  gint row;
  gint col;
  gint day;
  
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  year = calendar->year;
  month = calendar->month + 1;
  
  ndays_in_month = month_length[leap (year)][month];
  
  first_day = day_of_week (year, month, 1);
  
  if (calendar->display_flags & GTK_CALENDAR_WEEK_START_MONDAY)
    first_day--;
  else
    first_day %= 7;
  
  
  /* Compute days of previous month */
  if (month > 1)
    ndays_in_prev_month = month_length[leap (year)][month-1];
  else
    ndays_in_prev_month = month_length[leap (year)][12];
  day = ndays_in_prev_month - first_day + 1;
  
  row = 0;
  if (first_day > 0)
    {
      for (col = 0; col < first_day; col++)
	{
	  calendar->day[row][col] = day;
	  calendar->day_month[row][col] = MONTH_PREV;
	  day++;
	}
    }
  
  /* Compute days of current month */
  col = first_day;
  for (day = 1; day <= ndays_in_month; day++)
    {
      calendar->day[row][col] = day;
      calendar->day_month[row][col] = MONTH_CURRENT;
      
      col++;
      if (col == 7)
	{
	  row++;
	  col = 0;
	}
    }
  
  /* Compute days of next month */
  day = 1;
  for (; row <= 5; row++)
    {
      for (; col <= 6; col++)
	{
	  calendar->day[row][col] = day;
	  calendar->day_month[row][col] = MONTH_NEXT;
	  day++;
	}
      col = 0;
    }
}

/* ----------------------------------------------------------------------
   NAME:	gtk_calendar_display_options
   DESCRIPTION:	Set display options (whether to display the
   heading and the month headings)
   
   flags is can be an XOR of:
   GTK_CALENDAR_SHOW_HEADING
   GTK_CALENDAR_SHOW_DAY_NAMES
   GTK_CALENDAR_NO_MONTH_CHANGE
   GTK_CALENDAR_SHOW_WEEK_NUMBERS
   GTK_CALENDAR_WEEK_START_MONDAY
   ---------------------------------------------------------------------- */

void
gtk_calendar_display_options (GtkCalendar	       *calendar,
			      GtkCalendarDisplayOptions flags)
{
  GtkCalendarPrivateData *private_data;
  gint resize = 0;
  GtkWidget *widget;
  gint i;
  
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  widget = GTK_WIDGET (calendar);
  private_data = GTK_CALENDAR_PRIVATE_DATA (calendar);
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_NO_MONTH_CHANGE)
	{
	  resize ++;
	  if (! (flags & GTK_CALENDAR_NO_MONTH_CHANGE)
	      && (private_data->header_win))
	    {
	      calendar->display_flags &= ~GTK_CALENDAR_NO_MONTH_CHANGE;
	      gtk_calendar_realize_arrows (widget);
	    }
	  else
	    {
	      for (i = 0; i < 4; i++)
		{
		  if (private_data->arrow_win[i])
		    {
		      gdk_window_set_user_data (private_data->arrow_win[i], 
						NULL);
		      gdk_window_destroy (private_data->arrow_win[i]);
		      private_data->arrow_win[i] = NULL;
		    }
		}
	    }
	}
      
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_SHOW_HEADING)
	{
	  resize++;
	  
	  if (flags & GTK_CALENDAR_SHOW_HEADING)
	    {
	      calendar->display_flags |= GTK_CALENDAR_SHOW_HEADING;
	      gtk_calendar_realize_header (widget);
	    }
	  else
	    {
	      for (i = 0; i < 4; i++)
		{
		  if (private_data->arrow_win[i])
		    {
		      gdk_window_set_user_data (private_data->arrow_win[i], 
						NULL);
		      gdk_window_destroy (private_data->arrow_win[i]);
		      private_data->arrow_win[i] = NULL;
		    }
		}
	      gdk_window_set_user_data (private_data->header_win, NULL);
	      gdk_window_destroy (private_data->header_win);
	      private_data->header_win = NULL;
	    }
	}
      
      
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_SHOW_DAY_NAMES)
	{
	  resize++;
	  
	  if (flags & GTK_CALENDAR_SHOW_DAY_NAMES)
	    {
	      calendar->display_flags |= GTK_CALENDAR_SHOW_DAY_NAMES;
	      gtk_calendar_realize_day_names (widget);
	    }
	  else
	    {
	      gdk_window_set_user_data (private_data->day_name_win, NULL);
	      gdk_window_destroy (private_data->day_name_win);
	      private_data->day_name_win = NULL;
	    }
	}
      
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
	{
	  resize++;
	  
	  if (flags & GTK_CALENDAR_SHOW_WEEK_NUMBERS)
	    {
	      calendar->display_flags |= GTK_CALENDAR_SHOW_WEEK_NUMBERS;
	      gtk_calendar_realize_week_numbers (widget);
	    }
	  else
	    {
	      gdk_window_set_user_data (private_data->week_win, NULL);
	      gdk_window_destroy (private_data->week_win);
	      private_data->week_win = NULL;
	    }
	}
      
      if ((flags ^ calendar->display_flags) & GTK_CALENDAR_WEEK_START_MONDAY)
	{
	  if (calendar->display_flags & GTK_CALENDAR_WEEK_START_MONDAY)
	    calendar->display_flags &= ~GTK_CALENDAR_WEEK_START_MONDAY;
	  else
	    calendar->display_flags |= GTK_CALENDAR_WEEK_START_MONDAY;
	  
	  gtk_calendar_compute_days (calendar);
	  gtk_calendar_paint_main (GTK_WIDGET (calendar));
	  if (private_data->day_name_win)
	    gtk_calendar_paint_day_names (GTK_WIDGET (calendar));
	}
      
      calendar->display_flags = flags;
      if (resize)
	gtk_widget_queue_resize (GTK_WIDGET (calendar));
      
    } 
  else
    calendar->display_flags = flags;
  
}

gboolean
gtk_calendar_select_month (GtkCalendar *calendar,
			   guint	month,
			   guint	year)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), FALSE);
  g_return_val_if_fail (month <= 11, FALSE);
  
  calendar->month = month;
  calendar->year  = year;
  
  gtk_calendar_compute_days (calendar);
  
  gtk_widget_queue_draw (GTK_WIDGET (calendar));
  g_signal_emit (calendar,
		 gtk_calendar_signals[MONTH_CHANGED_SIGNAL],
		 0);
  return TRUE;
}

void
gtk_calendar_select_day (GtkCalendar *calendar,
			 guint	      day)
{
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  g_return_if_fail (day <= 31);
  
  /* gtk_calendar_compute_days (calendar); */
  
  /* Deselect the old day */
  if (calendar->selected_day > 0)
    {
      gint selected_day;
      
      selected_day = calendar->selected_day;
      calendar->selected_day = 0;
      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (calendar)))
	gtk_calendar_paint_day_num (GTK_WIDGET (calendar), selected_day);
    }
  
  calendar->selected_day = day;
  
  /* Select the new day */
  if (day != 0)
    {
      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (calendar)))
	gtk_calendar_paint_day_num (GTK_WIDGET (calendar), day);
    }
  
  g_signal_emit (calendar,
		 gtk_calendar_signals[DAY_SELECTED_SIGNAL],
		 0);
}

static void
gtk_calendar_select_and_focus_day (GtkCalendar *calendar,
				   guint        day)
{
  gint old_focus_row = calendar->focus_row;
  gint old_focus_col = calendar->focus_col;
  gint row;
  gint col;
  
  for (row = 0; row < 6; row ++)
    for (col = 0; col < 7; col++)
      {
	if (calendar->day_month[row][col] == MONTH_CURRENT 
	    && calendar->day[row][col] == day)
	  {
	    calendar->focus_row = row;
	    calendar->focus_col = col;
	  }
      }

  if (old_focus_row != -1 && old_focus_col != -1)
    gtk_calendar_paint_day (GTK_WIDGET (calendar), old_focus_row, old_focus_col);
  
  gtk_calendar_select_day (calendar, day);
}

void
gtk_calendar_clear_marks (GtkCalendar *calendar)
{
  guint day;
  
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  for (day = 0; day < 31; day++)
    {
      calendar->marked_date[day] = FALSE;
    }
 
  calendar->num_marked_dates = 0;
 
  if (GTK_WIDGET_DRAWABLE (calendar))
    {
      gtk_calendar_paint_main (GTK_WIDGET (calendar));
    }
}

gboolean
gtk_calendar_mark_day (GtkCalendar *calendar,
		       guint	    day)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), FALSE);
  
  if (day >= 1 && day <= 31 && calendar->marked_date[day-1] == FALSE)
    {
      calendar->marked_date[day - 1] = TRUE;
      calendar->num_marked_dates++;
    }
  if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (calendar)))
    {
      gtk_calendar_paint_main (GTK_WIDGET (calendar));
    }
  
  return TRUE;
}

gboolean
gtk_calendar_unmark_day (GtkCalendar *calendar,
			 guint	      day)
{
  g_return_val_if_fail (GTK_IS_CALENDAR (calendar), FALSE);
  
  if (day >= 1 && day <= 31 && calendar->marked_date[day-1] == TRUE)
    {
      calendar->marked_date[day - 1] = FALSE;
      calendar->num_marked_dates--;
    }
  
  if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (calendar)))
    {
      gtk_calendar_paint_main (GTK_WIDGET (calendar));
    }
  
  return TRUE;
}

void
gtk_calendar_get_date (GtkCalendar *calendar,
		       guint	   *year,
		       guint	   *month,
		       guint	   *day)
{
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  if (year)
    *year = calendar->year;
  
  if (month)
    *month = calendar->month;
  
  if (day)
    *day = calendar->selected_day;
}

static gboolean
gtk_calendar_button_press (GtkWidget	  *widget,
			   GdkEventButton *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint x, y;
  void (* action_func) (GtkCalendar *);
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  x = (gint) (event->x);
  y = (gint) (event->y);
  
  if (event->window == private_data->main_win)
    gtk_calendar_main_button (widget, event);

  action_func = NULL;  

  if (event->window == private_data->arrow_win[ARROW_MONTH_LEFT])
    action_func = gtk_calendar_set_month_prev;
  else if (event->window == private_data->arrow_win[ARROW_MONTH_RIGHT])
    action_func = gtk_calendar_set_month_next;
  else if (event->window == private_data->arrow_win[ARROW_YEAR_LEFT])
    action_func = gtk_calendar_set_year_prev;
  else if (event->window == private_data->arrow_win[ARROW_YEAR_RIGHT])
    action_func = gtk_calendar_set_year_next;

  /* only call the action on single click, not double */
  if (action_func && event->type == GDK_BUTTON_PRESS)
    (* action_func) (calendar);
  
  return action_func != NULL;
}

static gboolean
gtk_calendar_motion_notify (GtkWidget	   *widget,
			    GdkEventMotion *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint event_x, event_y;
  gint row, col;
  gint old_row, old_col;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  event_x = (gint) (event->x);
  event_y = (gint) (event->y);
  
  if (event->window == private_data->main_win)
    {
      
      row = row_from_y (calendar, event_y);
      col = column_from_x (calendar, event_x);
      
      if (row != calendar->highlight_row || calendar->highlight_col != col)
	{
	  old_row = calendar->highlight_row;
	  old_col = calendar->highlight_col;
	  if (old_row > -1 && old_col > -1)
	    {
	      calendar->highlight_row = -1;
	      calendar->highlight_col = -1;
	      gtk_calendar_paint_day (widget, old_row, old_col);
	    }
	  
	  calendar->highlight_row = row;
	  calendar->highlight_col = col;
	  
	  if (row > -1 && col > -1)
	    gtk_calendar_paint_day (widget, row, col);
	}
    }
  return TRUE;
}

static gboolean
gtk_calendar_enter_notify (GtkWidget	    *widget,
			   GdkEventCrossing *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (event->window == private_data->arrow_win[ARROW_MONTH_LEFT])
    {
      private_data->arrow_state[ARROW_MONTH_LEFT] = GTK_STATE_PRELIGHT;
      gtk_calendar_paint_arrow (widget, ARROW_MONTH_LEFT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_MONTH_RIGHT])
    {
      private_data->arrow_state[ARROW_MONTH_RIGHT] = GTK_STATE_PRELIGHT;
      gtk_calendar_paint_arrow (widget, ARROW_MONTH_RIGHT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_LEFT])
    {
      private_data->arrow_state[ARROW_YEAR_LEFT] = GTK_STATE_PRELIGHT;
      gtk_calendar_paint_arrow (widget, ARROW_YEAR_LEFT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_RIGHT])
    {
      private_data->arrow_state[ARROW_YEAR_RIGHT] = GTK_STATE_PRELIGHT;
      gtk_calendar_paint_arrow (widget, ARROW_YEAR_RIGHT);
    }
  
  return TRUE;
}

static gboolean
gtk_calendar_leave_notify (GtkWidget	    *widget,
			   GdkEventCrossing *event)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint row;
  gint col;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);
  
  if (event->window == private_data->main_win)
    {
      row = calendar->highlight_row;
      col = calendar->highlight_col;
      calendar->highlight_row = -1;
      calendar->highlight_col = -1;
      if (row > -1 && col > -1)
	gtk_calendar_paint_day (widget, row, col);
    }
  
  if (event->window == private_data->arrow_win[ARROW_MONTH_LEFT])
    {
      private_data->arrow_state[ARROW_MONTH_LEFT] = GTK_STATE_NORMAL;
      gtk_calendar_paint_arrow (widget, ARROW_MONTH_LEFT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_MONTH_RIGHT])
    {
      private_data->arrow_state[ARROW_MONTH_RIGHT] = GTK_STATE_NORMAL;
      gtk_calendar_paint_arrow (widget, ARROW_MONTH_RIGHT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_LEFT])
    {
      private_data->arrow_state[ARROW_YEAR_LEFT] = GTK_STATE_NORMAL;
      gtk_calendar_paint_arrow (widget, ARROW_YEAR_LEFT);
    }
  
  if (event->window == private_data->arrow_win[ARROW_YEAR_RIGHT])
    {
      private_data->arrow_state[ARROW_YEAR_RIGHT] = GTK_STATE_NORMAL;
      gtk_calendar_paint_arrow (widget, ARROW_YEAR_RIGHT);
    }
  
  return TRUE;
}

static void
gtk_calendar_paint_arrow (GtkWidget *widget,
			  guint	     arrow)
{
  GtkCalendarPrivateData *private_data;
  GdkWindow *window;
  GdkGC *gc;
  GtkCalendar *calendar;
  gint state;
  gint width, height;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  if (private_data->freeze_count)
    {
      private_data->dirty_header = 1;
      return;
    }
  window = private_data->arrow_win[arrow];
  if (window)
    {
      state = private_data->arrow_state[arrow];
      gc = calendar->gc;
      
      gdk_window_clear (window);
      gdk_window_set_background (window, &(widget)->style->bg[state]);
      gdk_drawable_get_size (window, &width, &height);
      gdk_window_clear_area (window,
			     0,0,
			     width,height);
      
      gdk_gc_set_foreground (gc, & (widget)->style->fg[state]);
      
      if (arrow == ARROW_MONTH_LEFT || arrow == ARROW_YEAR_LEFT)
	draw_arrow_left (window, gc, width/2 - 3, height/2 - 4, 8);
      else 
	draw_arrow_right (window, gc, width/2 - 2, height/2 - 4, 8);
      return;
    }
}

void
gtk_calendar_freeze (GtkCalendar *calendar)
{
  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  GTK_CALENDAR_PRIVATE_DATA (calendar)->freeze_count++;
}

void
gtk_calendar_thaw (GtkCalendar *calendar)
{
  GtkCalendarPrivateData *private_data;

  g_return_if_fail (GTK_IS_CALENDAR (calendar));
  
  private_data = GTK_CALENDAR_PRIVATE_DATA (calendar);
  
  if (private_data->freeze_count)
    if (!(--private_data->freeze_count))
      {
	if (private_data->dirty_header)
	  if (GTK_WIDGET_DRAWABLE (calendar))
	    gtk_calendar_paint_header (GTK_WIDGET (calendar));
	
	if (private_data->dirty_day_names)
	  if (GTK_WIDGET_DRAWABLE (calendar))
	    gtk_calendar_paint_day_names (GTK_WIDGET (calendar));
	
	if (private_data->dirty_week)
	  if (GTK_WIDGET_DRAWABLE (calendar))
	    gtk_calendar_paint_week_numbers (GTK_WIDGET (calendar));
	
	if (private_data->dirty_main)
	  if (GTK_WIDGET_DRAWABLE (calendar))
	    gtk_calendar_paint_main (GTK_WIDGET (calendar));
      }
}

static void
gtk_calendar_set_background (GtkWidget *widget)
{
  GtkCalendar *calendar;
  GtkCalendarPrivateData *private_data;
  gint i;
  
  calendar = GTK_CALENDAR (widget);
  private_data = GTK_CALENDAR_PRIVATE_DATA (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      for (i = 0; i < 4; i++)
	{
	  if (private_data->arrow_win[i])
	    gdk_window_set_background (private_data->arrow_win[i], 
				       HEADER_BG_COLOR (widget));
	}
      if (private_data->header_win)
	gdk_window_set_background (private_data->header_win, 
				   HEADER_BG_COLOR (widget));
      if (private_data->day_name_win)
	gdk_window_set_background (private_data->day_name_win, 
				   BACKGROUND_COLOR (widget));
      if (private_data->week_win)
	gdk_window_set_background (private_data->week_win,
				   BACKGROUND_COLOR (widget));
      if (private_data->main_win)
	gdk_window_set_background (private_data->main_win,
				   BACKGROUND_COLOR (widget));
      if (widget->window)
	gdk_window_set_background (widget->window,
				   BACKGROUND_COLOR (widget)); 
    }
}

static void
gtk_calendar_style_set (GtkWidget *widget,
			GtkStyle  *previous_style)
{
  if (previous_style && GTK_WIDGET_REALIZED (widget))
    gtk_calendar_set_background(widget);
}

static void
gtk_calendar_state_changed (GtkWidget	   *widget,
			    GtkStateType    previous_state)
{
  gtk_calendar_set_background (widget);
}

static void
gtk_calendar_finalize (GObject *object)
{
  GtkCalendarPrivateData *private_data;
  private_data = GTK_CALENDAR_PRIVATE_DATA (object);
  
  g_free (private_data);
  
  if (G_OBJECT_CLASS (parent_class)->finalize)
    (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static gboolean
gtk_calendar_key_press (GtkWidget   *widget,
			GdkEventKey *event)
{
  GtkCalendar *calendar;
  gint return_val;
  gint old_focus_row;
  gint old_focus_col;
  gint row, col, day;
  
  calendar = GTK_CALENDAR (widget);
  return_val = FALSE;
  
  old_focus_row = calendar->focus_row;
  old_focus_col = calendar->focus_col;

  switch (event->keyval)
    {
    case GDK_KP_Left:
    case GDK_Left:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_calendar_set_month_prev (calendar);
	}
      else
	{
	  if (calendar->focus_col > 0)
	    {
	      calendar->focus_col--;
	    }
	  else if (calendar->focus_row > 0)
	    {
	      calendar->focus_col = 6;
	      calendar->focus_row--;
	    }
	  gtk_calendar_paint_day (widget, old_focus_row, old_focus_col);
	  gtk_calendar_paint_day (widget, calendar->focus_row,
				  calendar->focus_col);
	}
      break;
    case GDK_KP_Right:
    case GDK_Right:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	{
	gtk_calendar_set_month_next (calendar);
	}
      else
	{
	  if (calendar->focus_col < 6)
	    {
	      calendar->focus_col++;
	    }
	  else if (calendar->focus_row < 5)
	    {
	      calendar->focus_col = 0;
	      calendar->focus_row++;
	    }
	  gtk_calendar_paint_day (widget, old_focus_row, old_focus_col);
	  gtk_calendar_paint_day (widget, calendar->focus_row,
				  calendar->focus_col);
	}
      break;
    case GDK_KP_Up:
    case GDK_Up:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_calendar_set_year_prev (calendar);
	}
      else
	{
	  if (calendar->focus_row > 0)
	    {
	      calendar->focus_row--;
	    }
	  gtk_calendar_paint_day (widget, old_focus_row, old_focus_col);
	  gtk_calendar_paint_day (widget, calendar->focus_row,
				  calendar->focus_col);
	}
      break;
    case GDK_KP_Down:
    case GDK_Down:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_calendar_set_year_next (calendar);
	}
      else
	{
	  if (calendar->focus_row < 5)
	    {
	      calendar->focus_row++;
	    }
	  gtk_calendar_paint_day (widget, old_focus_row, old_focus_col);
	  gtk_calendar_paint_day (widget, calendar->focus_row,
				  calendar->focus_col);
	}
      break;
    case GDK_KP_Space:
    case GDK_space:
      row = calendar->focus_row;
      col = calendar->focus_col;
      day = calendar->day[row][col];
      
      if (row > -1 && col > -1)
	{
	  return_val = TRUE;
	  gtk_calendar_freeze (calendar);	  

	  if (calendar->day_month[row][col] == MONTH_PREV)
	    {
	      gtk_calendar_set_month_prev (calendar);
	    }
	  else if (calendar->day_month[row][col] == MONTH_NEXT)
	    {
	      gtk_calendar_set_month_next (calendar);
	    }

	  gtk_calendar_select_and_focus_day (calendar, day);
	  
	  gtk_calendar_thaw (calendar);	  
	}
    }	
  
  return return_val;
}
