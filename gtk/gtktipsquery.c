/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkQueryTips: Query onscreen widgets for their tooltips
 * Copyright (C) 1998 Tim Janik
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include	"gtktipsquery.h"
#include	"gtksignal.h"
#include	"gtktooltips.h"
#include	"gtkmain.h"



/* --- arguments --- */
enum {
  ARG_0,
  ARG_EMIT_ALWAYS,
  ARG_CALLER,
  ARG_LABEL_INACTIVE,
  ARG_LABEL_NO_TIP
};


/* --- signals --- */
enum
{
  SIGNAL_START_QUERY,
  SIGNAL_STOP_QUERY,
  SIGNAL_WIDGET_ENTERED,
  SIGNAL_WIDGET_SELECTED,
  SIGNAL_LAST
};
typedef	void	(*SignalWidgetEntered)		(GtkObject	*object,
						 GtkWidget	*widget,
						 const gchar	*tip_text,
						 const gchar	*tip_private,
						 gpointer	 func_data);
typedef	gint	(*SignalWidgetSelected)		(GtkObject	*object,
						 GtkWidget	*widget,
						 const gchar	*tip_text,
						 const gchar	*tip_private,
						 GdkEventButton *event,
						 gpointer	 func_data);


/* --- prototypes --- */
static void	gtk_tips_query_class_init	(GtkTipsQueryClass	*class);
static void	gtk_tips_query_init		(GtkTipsQuery		*tips_query);
static void	gtk_tips_query_destroy		(GtkObject		*object);
static gint	gtk_tips_query_event		(GtkWidget		*widget,
						 GdkEvent		*event);
static void	gtk_tips_query_set_arg		(GtkTipsQuery		*tips_query,
						 GtkArg			*arg,
						 guint			 arg_id);
static void	gtk_tips_query_get_arg		(GtkTipsQuery		*tips_query,
						 GtkArg			*arg,
						 guint			arg_id);
static void	gtk_tips_query_real_start_query	(GtkTipsQuery		*tips_query);
static void	gtk_tips_query_real_stop_query	(GtkTipsQuery		*tips_query);
static void	gtk_tips_query_widget_entered	(GtkTipsQuery		*tips_query,
						 GtkWidget		*widget,
						 const gchar		*tip_text,
						 const gchar		*tip_private);


/* --- variables --- */
static GtkLabelClass	*parent_class = NULL;
static guint		 tips_query_signals[SIGNAL_LAST] = { 0 };


/* --- functions --- */
guint
gtk_tips_query_get_type (void)
{
  static guint tips_query_type = 0;

  if (!tips_query_type)
    {
      GtkTypeInfo tips_query_info =
      {
	"GtkTipsQuery",
	sizeof (GtkTipsQuery),
	sizeof (GtkTipsQueryClass),
	(GtkClassInitFunc) gtk_tips_query_class_init,
	(GtkObjectInitFunc) gtk_tips_query_init,
	(GtkArgSetFunc) gtk_tips_query_set_arg,
	(GtkArgGetFunc) gtk_tips_query_get_arg,
      };

      tips_query_type = gtk_type_unique (gtk_label_get_type (), &tips_query_info);
    }

  return tips_query_type;
}

static void
gtk_tips_query_marshal_widget_entered (GtkObject      *object,
				       GtkSignalFunc  func,
				       gpointer	      func_data,
				       GtkArg	      *args)
{
  SignalWidgetEntered sfunc = (SignalWidgetEntered) func;

  (* sfunc) (object,
	     (GtkWidget*) GTK_VALUE_OBJECT (args[0]),
	     GTK_VALUE_STRING (args[1]),
	     GTK_VALUE_STRING (args[2]),
	     func_data);
}

static void
gtk_tips_query_marshal_widget_selected (GtkObject      *object,
					GtkSignalFunc  func,
					gpointer       func_data,
					GtkArg	       *args)
{
  gint *return_val;

  SignalWidgetSelected sfunc = (SignalWidgetSelected) func;
  return_val = GTK_RETLOC_BOOL (args[4]);
  
  *return_val = (* sfunc) (object,
			   (GtkWidget*) GTK_VALUE_OBJECT (args[0]),
			   GTK_VALUE_STRING (args[1]),
			   GTK_VALUE_STRING (args[2]),
			   GTK_VALUE_BOXED (args[3]),
			   func_data);
}

static void
gtk_tips_query_class_init (GtkTipsQueryClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (gtk_label_get_type ());

  gtk_object_add_arg_type ("GtkTipsQuery::emit_always", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_EMIT_ALWAYS);
  gtk_object_add_arg_type ("GtkTipsQuery::caller", GTK_TYPE_WIDGET, GTK_ARG_READWRITE, ARG_CALLER);
  gtk_object_add_arg_type ("GtkTipsQuery::label_inactive", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_LABEL_INACTIVE);
  gtk_object_add_arg_type ("GtkTipsQuery::label_no_tip", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_LABEL_NO_TIP);

  tips_query_signals[SIGNAL_START_QUERY] =
    gtk_signal_new ("start_query",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkTipsQueryClass, start_query),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  tips_query_signals[SIGNAL_STOP_QUERY] =
    gtk_signal_new ("stop_query",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkTipsQueryClass, stop_query),
		    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);
  tips_query_signals[SIGNAL_WIDGET_ENTERED] =
    gtk_signal_new ("widget_entered",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkTipsQueryClass, widget_entered),
		    gtk_tips_query_marshal_widget_entered,
		    GTK_TYPE_NONE, 3,
		    GTK_TYPE_WIDGET,
		    GTK_TYPE_STRING,
		    GTK_TYPE_STRING);
  tips_query_signals[SIGNAL_WIDGET_SELECTED] =
    gtk_signal_new ("widget_selected",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkTipsQueryClass, widget_selected),
		    gtk_tips_query_marshal_widget_selected,
		    GTK_TYPE_INT, 4,
		    GTK_TYPE_WIDGET,
		    GTK_TYPE_STRING,
		    GTK_TYPE_STRING,
		    GTK_TYPE_BOXED);
  gtk_object_class_add_signals (object_class, tips_query_signals, SIGNAL_LAST);

  object_class->destroy = gtk_tips_query_destroy;
  widget_class->event = gtk_tips_query_event;

  class->start_query = gtk_tips_query_real_start_query;
  class->stop_query = gtk_tips_query_real_stop_query;
  class->widget_entered = gtk_tips_query_widget_entered;
  class->widget_selected = NULL;
}

static void
gtk_tips_query_init (GtkTipsQuery *tips_query)
{
  tips_query->emit_always = FALSE;
  tips_query->in_query = FALSE;
  tips_query->label_inactive = g_strdup ("");
  tips_query->label_no_tip = g_strdup ("--- No Tip ---");
  tips_query->caller = NULL;
  tips_query->last_crossed = NULL;
  tips_query->query_cursor = NULL;

  gtk_label_set (GTK_LABEL (tips_query), tips_query->label_inactive);
}

static void
gtk_tips_query_set_arg (GtkTipsQuery           *tips_query,
			GtkArg                 *arg,
			guint                   arg_id)
{
  switch (arg_id)
    {
    case ARG_EMIT_ALWAYS:
      tips_query->emit_always = (GTK_VALUE_BOOL (*arg) != FALSE);
      break;
    case ARG_CALLER:
      gtk_tips_query_set_caller (tips_query, GTK_WIDGET (GTK_VALUE_OBJECT (*arg)));
      break;
    case ARG_LABEL_INACTIVE:
      gtk_tips_query_set_labels (tips_query, GTK_VALUE_STRING (*arg), tips_query->label_no_tip);
      break;
    case ARG_LABEL_NO_TIP:
      gtk_tips_query_set_labels (tips_query, tips_query->label_inactive, GTK_VALUE_STRING (*arg));
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_tips_query_get_arg (GtkTipsQuery           *tips_query,
			GtkArg                 *arg,
			guint                  arg_id)
{
  switch (arg_id)
    {
    case ARG_EMIT_ALWAYS:
      GTK_VALUE_BOOL (*arg) = tips_query->emit_always;
      break;
    case ARG_CALLER:
      GTK_VALUE_OBJECT (*arg) = (GtkObject*) tips_query->caller;
      break;
    case ARG_LABEL_INACTIVE:
      GTK_VALUE_STRING (*arg) = g_strdup (tips_query->label_inactive);
      break;
    case ARG_LABEL_NO_TIP:
      GTK_VALUE_STRING (*arg) = g_strdup (tips_query->label_no_tip);
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_tips_query_destroy (GtkObject	*object)
{
  GtkTipsQuery *tips_query;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_TIPS_QUERY (object));

  tips_query = GTK_TIPS_QUERY (object);

  if (tips_query->in_query)
    gtk_tips_query_stop_query (tips_query);

  gtk_tips_query_set_caller (tips_query, NULL);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

GtkWidget*
gtk_tips_query_new (void)
{
  GtkTipsQuery *tips_query;

  tips_query = gtk_type_new (gtk_tips_query_get_type ());

  return GTK_WIDGET (tips_query);
}

void
gtk_tips_query_set_labels (GtkTipsQuery   *tips_query,
			   const gchar	  *label_inactive,
			   const gchar	  *label_no_tip)
{
  gchar *old;

  g_return_if_fail (tips_query != NULL);
  g_return_if_fail (GTK_IS_TIPS_QUERY (tips_query));
  g_return_if_fail (label_inactive != NULL);
  g_return_if_fail (label_no_tip != NULL);

  old = tips_query->label_inactive;
  tips_query->label_inactive = g_strdup (label_inactive);
  g_free (old);
  old = tips_query->label_no_tip;
  tips_query->label_no_tip = g_strdup (label_no_tip);
  g_free (old);
}

void
gtk_tips_query_set_caller (GtkTipsQuery   *tips_query,
			   GtkWidget	   *caller)
{
  g_return_if_fail (tips_query != NULL);
  g_return_if_fail (GTK_IS_TIPS_QUERY (tips_query));
  g_return_if_fail (tips_query->in_query == FALSE);
  if (caller)
    g_return_if_fail (GTK_IS_WIDGET (caller));

  if (caller)
    gtk_widget_ref (caller);

  if (tips_query->caller)
    gtk_widget_unref (tips_query->caller);

  tips_query->caller = caller;
}

void
gtk_tips_query_start_query (GtkTipsQuery *tips_query)
{
  g_return_if_fail (tips_query != NULL);
  g_return_if_fail (GTK_IS_TIPS_QUERY (tips_query));
  g_return_if_fail (tips_query->in_query == FALSE);
  g_return_if_fail (GTK_WIDGET_REALIZED (tips_query));

  tips_query->in_query = TRUE;
  gtk_signal_emit (GTK_OBJECT (tips_query), tips_query_signals[SIGNAL_START_QUERY]);
}

void
gtk_tips_query_stop_query (GtkTipsQuery *tips_query)
{
  g_return_if_fail (tips_query != NULL);
  g_return_if_fail (GTK_IS_TIPS_QUERY (tips_query));
  g_return_if_fail (tips_query->in_query == TRUE);

  gtk_signal_emit (GTK_OBJECT (tips_query), tips_query_signals[SIGNAL_STOP_QUERY]);
  tips_query->in_query = FALSE;
}

static void
gtk_tips_query_real_start_query (GtkTipsQuery *tips_query)
{
  gint failure;
  
  g_return_if_fail (tips_query != NULL);
  g_return_if_fail (GTK_IS_TIPS_QUERY (tips_query));
  
  tips_query->query_cursor = gdk_cursor_new (GDK_QUESTION_ARROW);
  failure = gdk_pointer_grab (GTK_WIDGET (tips_query)->window,
			      TRUE,
			      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			      GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK,
			      NULL,
			      tips_query->query_cursor,
			      GDK_CURRENT_TIME);
  if (failure)
    {
      gdk_cursor_destroy (tips_query->query_cursor);
      tips_query->query_cursor = NULL;
    }
  gtk_grab_add (GTK_WIDGET (tips_query));
}

static void
gtk_tips_query_real_stop_query (GtkTipsQuery *tips_query)
{
  g_return_if_fail (tips_query != NULL);
  g_return_if_fail (GTK_IS_TIPS_QUERY (tips_query));
  
  gtk_grab_remove (GTK_WIDGET (tips_query));
  if (tips_query->query_cursor)
    {
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
      gdk_cursor_destroy (tips_query->query_cursor);
      tips_query->query_cursor = NULL;
    }
  if (tips_query->last_crossed)
    {
      gtk_widget_unref (tips_query->last_crossed);
      tips_query->last_crossed = NULL;
    }
  
  gtk_label_set (GTK_LABEL (tips_query), tips_query->label_inactive);
}

static void
gtk_tips_query_widget_entered (GtkTipsQuery   *tips_query,
			       GtkWidget      *widget,
			       const gchar    *tip_text,
			       const gchar    *tip_private)
{
  g_return_if_fail (tips_query != NULL);
  g_return_if_fail (GTK_IS_TIPS_QUERY (tips_query));

  if (!tip_text)
    tip_text = tips_query->label_no_tip;

  if (!g_str_equal (GTK_LABEL (tips_query)->label, (gchar*) tip_text))
    gtk_label_set (GTK_LABEL (tips_query), tip_text);
}

static void
gtk_tips_query_emit_widget_entered (GtkTipsQuery *tips_query,
				    GtkWidget	 *widget)
{
  GtkTooltipsData *tdata;

  if (widget == (GtkWidget*) tips_query)
    widget = NULL;

  if (widget)
    tdata = gtk_tooltips_data_get (widget);
  else
    tdata = NULL;

  if (!widget && tips_query->last_crossed)
    {
      gtk_signal_emit (GTK_OBJECT (tips_query),
		       tips_query_signals[SIGNAL_WIDGET_ENTERED],
		       NULL,
		       NULL,
		       NULL);
      gtk_widget_unref (tips_query->last_crossed);
      tips_query->last_crossed = NULL;
    }
  else if (widget && widget != tips_query->last_crossed)
    {
      gtk_widget_ref (widget);
      if (tdata || tips_query->emit_always)
	  gtk_signal_emit (GTK_OBJECT (tips_query),
			   tips_query_signals[SIGNAL_WIDGET_ENTERED],
			   widget,
			   tdata ? tdata->tip_text : NULL,
			   tdata ? tdata->tip_private : NULL);
      if (tips_query->last_crossed)
	gtk_widget_unref (tips_query->last_crossed);
      tips_query->last_crossed = widget;
    }
}

static gint
gtk_tips_query_event (GtkWidget	       *widget,
		      GdkEvent	       *event)
{
  GtkTipsQuery *tips_query;
  GtkWidget *event_widget;
  gboolean event_handled;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TIPS_QUERY (widget), FALSE);

  tips_query = GTK_TIPS_QUERY (widget);
  if (!tips_query->in_query)
    {
      if (GTK_WIDGET_CLASS (parent_class)->event)
	return GTK_WIDGET_CLASS (parent_class)->event (widget, event);
      else
	return FALSE;
    }

  event_widget = gtk_get_event_widget (event);

  event_handled = FALSE;
  switch (event->type)
    {
      GdkWindow *pointer_window;
      
    case  GDK_LEAVE_NOTIFY:
      if (event_widget)
	pointer_window = gdk_window_get_pointer (event_widget->window, NULL, NULL, NULL);
      else
	pointer_window = NULL;
      event_widget = NULL;
      if (pointer_window)
	gdk_window_get_user_data (pointer_window, (gpointer*) &event_widget);
      gtk_tips_query_emit_widget_entered (tips_query, event_widget);
      event_handled = TRUE;
      break;

    case  GDK_ENTER_NOTIFY:
      gtk_tips_query_emit_widget_entered (tips_query, event_widget);
      event_handled = TRUE;
      break;

    case  GDK_BUTTON_PRESS:
    case  GDK_BUTTON_RELEASE:
      if (event_widget)
	{
	  if (event_widget == (GtkWidget*) tips_query ||
	      event_widget == tips_query->caller)
	    gtk_tips_query_stop_query (tips_query);
	  else
	    {
	      gint stop;
	      GtkTooltipsData *tdata;
	      
	      stop = TRUE;
	      tdata = gtk_tooltips_data_get (event_widget);
	      if (tdata || tips_query->emit_always)
		gtk_signal_emit (GTK_OBJECT (tips_query),
				 tips_query_signals[SIGNAL_WIDGET_SELECTED],
				 event_widget,
				 tdata ? tdata->tip_text : NULL,
				 tdata ? tdata->tip_private : NULL,
				 event,
				 &stop);
	      
	      if (stop)
		gtk_tips_query_stop_query (tips_query);
	    }
	}
      event_handled = TRUE;
      break;

    default:
      break;
    }

  return event_handled;
}
