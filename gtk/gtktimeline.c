/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* gtktimeline.c
 *
 * Copyright (C) 2007 Carlos Garnacho <carlos@imendio.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtktimeline.h>
#include <gtk/gtktypebuiltins.h>
#include <gtk/gtksettings.h>
#include <math.h>

#define GTK_TIMELINE_GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_TIMELINE, GtkTimelinePriv))
#define MSECS_PER_SEC 1000
#define FRAME_INTERVAL(nframes) (MSECS_PER_SEC / nframes)
#define DEFAULT_FPS 30

typedef struct GtkTimelinePriv GtkTimelinePriv;

struct GtkTimelinePriv
{
  guint duration;
  guint fps;
  guint source_id;

  GTimer *timer;

  gdouble progress;
  gdouble last_progress;

  GdkScreen *screen;

  guint animations_enabled : 1;
  guint loop               : 1;
  guint direction          : 1;
};

enum {
  PROP_0,
  PROP_FPS,
  PROP_DURATION,
  PROP_LOOP,
  PROP_DIRECTION,
  PROP_SCREEN
};

enum {
  STARTED,
  PAUSED,
  FINISHED,
  FRAME,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };


static void  gtk_timeline_set_property  (GObject         *object,
                                         guint            prop_id,
                                         const GValue    *value,
                                         GParamSpec      *pspec);
static void  gtk_timeline_get_property  (GObject         *object,
                                         guint            prop_id,
                                         GValue          *value,
                                         GParamSpec      *pspec);
static void  gtk_timeline_finalize      (GObject *object);


G_DEFINE_TYPE (GtkTimeline, gtk_timeline, G_TYPE_OBJECT)


static void
gtk_timeline_class_init (GtkTimelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gtk_timeline_set_property;
  object_class->get_property = gtk_timeline_get_property;
  object_class->finalize = gtk_timeline_finalize;

  g_object_class_install_property (object_class,
				   PROP_FPS,
				   g_param_spec_uint ("fps",
						      "FPS",
						      "Frames per second for the timeline",
						      1, G_MAXUINT,
						      DEFAULT_FPS,
						      G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_DURATION,
				   g_param_spec_uint ("duration",
						      "Animation Duration",
						      "Animation Duration",
						      0, G_MAXUINT,
						      0,
						      G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_LOOP,
				   g_param_spec_boolean ("loop",
							 "Loop",
							 "Whether the timeline loops or not",
							 FALSE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_DIRECTION,
				   g_param_spec_enum ("direction",
						      "Direction",
						      "Whether the timeline moves forward or backward in time",
						      GTK_TYPE_TIMELINE_DIRECTION,
						      GTK_TIMELINE_DIRECTION_FORWARD,
						      G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_SCREEN,
				   g_param_spec_object ("screen",
							"Screen",
							"Screen to get the settings from",
							GDK_TYPE_SCREEN,
							G_PARAM_READWRITE));

  signals[STARTED] =
    g_signal_new ("started",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTimelineClass, started),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  signals[PAUSED] =
    g_signal_new ("paused",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTimelineClass, paused),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  signals[FINISHED] =
    g_signal_new ("finished",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTimelineClass, finished),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  signals[FRAME] =
    g_signal_new ("frame",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkTimelineClass, frame),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__DOUBLE,
		  G_TYPE_NONE, 1,
		  G_TYPE_DOUBLE);

  g_type_class_add_private (klass, sizeof (GtkTimelinePriv));
}

static void
gtk_timeline_init (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  priv->fps = DEFAULT_FPS;
  priv->duration = 0.0;
  priv->direction = GTK_TIMELINE_DIRECTION_FORWARD;
  priv->screen = gdk_screen_get_default ();

  priv->last_progress = 0;
}

static void
gtk_timeline_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkTimeline *timeline;
  GtkTimelinePriv *priv;

  timeline = GTK_TIMELINE (object);
  priv = GTK_TIMELINE_GET_PRIV (timeline);

  switch (prop_id)
    {
    case PROP_FPS:
      gtk_timeline_set_fps (timeline, g_value_get_uint (value));
      break;
    case PROP_DURATION:
      gtk_timeline_set_duration (timeline, g_value_get_uint (value));
      break;
    case PROP_LOOP:
      gtk_timeline_set_loop (timeline, g_value_get_boolean (value));
      break;
    case PROP_DIRECTION:
      gtk_timeline_set_direction (timeline, g_value_get_enum (value));
      break;
    case PROP_SCREEN:
      gtk_timeline_set_screen (timeline,
                               GDK_SCREEN (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_timeline_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkTimeline *timeline;
  GtkTimelinePriv *priv;

  timeline = GTK_TIMELINE (object);
  priv = GTK_TIMELINE_GET_PRIV (timeline);

  switch (prop_id)
    {
    case PROP_FPS:
      g_value_set_uint (value, priv->fps);
      break;
    case PROP_DURATION:
      g_value_set_uint (value, priv->duration);
      break;
    case PROP_LOOP:
      g_value_set_boolean (value, priv->loop);
      break;
    case PROP_DIRECTION:
      g_value_set_enum (value, priv->direction);
      break;
    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_timeline_finalize (GObject *object)
{
  GtkTimelinePriv *priv;

  priv = GTK_TIMELINE_GET_PRIV (object);

  if (priv->source_id)
    {
      g_source_remove (priv->source_id);
      priv->source_id = 0;
    }

  if (priv->timer)
    g_timer_destroy (priv->timer);

  G_OBJECT_CLASS (gtk_timeline_parent_class)->finalize (object);
}

static gboolean
gtk_timeline_run_frame (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;
  gdouble delta_progress, progress;
  guint elapsed_time;

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  elapsed_time = (guint) (g_timer_elapsed (priv->timer, NULL) * 1000);
  g_timer_start (priv->timer);

  if (priv->animations_enabled)
    {
      delta_progress = (gdouble) elapsed_time / priv->duration;
      progress = priv->last_progress;

      if (priv->direction == GTK_TIMELINE_DIRECTION_BACKWARD)
	progress -= delta_progress;
      else
	progress += delta_progress;

      priv->last_progress = progress;

      progress = CLAMP (progress, 0., 1.);
    }
  else
    progress = (priv->direction == GTK_TIMELINE_DIRECTION_FORWARD) ? 1.0 : 0.0;

  priv->progress = progress;
  g_signal_emit (timeline, signals [FRAME], 0, progress);

  if ((priv->direction == GTK_TIMELINE_DIRECTION_FORWARD && progress == 1.0) ||
      (priv->direction == GTK_TIMELINE_DIRECTION_BACKWARD && progress == 0.0))
    {
      if (!priv->loop)
	{
	  if (priv->source_id)
	    {
	      g_source_remove (priv->source_id);
	      priv->source_id = 0;
	    }
          g_timer_stop (priv->timer);
	  g_signal_emit (timeline, signals [FINISHED], 0);
	  return FALSE;
	}
      else
	gtk_timeline_rewind (timeline);
    }

  return TRUE;
}

/**
 * gtk_timeline_new:
 * @duration: duration in milliseconds for the timeline
 *
 * Creates a new #GtkTimeline with the specified number of frames.
 *
 * Return Value: the newly created #GtkTimeline
 **/
GtkTimeline *
gtk_timeline_new (guint duration)
{
  return g_object_new (GTK_TYPE_TIMELINE,
		       "duration", duration,
		       NULL);
}

GtkTimeline *
gtk_timeline_new_for_screen (guint      duration,
                             GdkScreen *screen)
{
  return g_object_new (GTK_TYPE_TIMELINE,
		       "duration", duration,
		       "screen", screen,
		       NULL);
}

/**
 * gtk_timeline_start:
 * @timeline: A #GtkTimeline
 *
 * Runs the timeline from the current frame.
 **/
void
gtk_timeline_start (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;
  GtkSettings *settings;
  gboolean enable_animations = FALSE;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  if (!priv->source_id)
    {
      if (priv->timer)
        g_timer_continue (priv->timer);
      else
        priv->timer = g_timer_new ();

      /* sanity check */
      g_assert (priv->fps > 0);

      if (priv->screen)
        {
          settings = gtk_settings_get_for_screen (priv->screen);
          g_object_get (settings, "gtk-enable-animations", &enable_animations, NULL);
        }

      priv->animations_enabled = (enable_animations == TRUE);

      g_signal_emit (timeline, signals [STARTED], 0);

      if (enable_animations)
        priv->source_id = gdk_threads_add_timeout (FRAME_INTERVAL (priv->fps),
                                                   (GSourceFunc) gtk_timeline_run_frame,
                                                   timeline);
      else
        priv->source_id = gdk_threads_add_idle ((GSourceFunc) gtk_timeline_run_frame,
                                                timeline);
    }
}

/**
 * gtk_timeline_pause:
 * @timeline: A #GtkTimeline
 *
 * Pauses the timeline.
 **/
void
gtk_timeline_pause (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  if (priv->source_id)
    {
      g_timer_stop (priv->timer);
      g_source_remove (priv->source_id);
      priv->source_id = 0;
      g_signal_emit (timeline, signals [PAUSED], 0);
    }
}

/**
 * gtk_timeline_rewind:
 * @timeline: A #GtkTimeline
 *
 * Rewinds the timeline.
 **/
void
gtk_timeline_rewind (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  if (gtk_timeline_get_direction(timeline) != GTK_TIMELINE_DIRECTION_FORWARD)
    priv->progress = priv->last_progress = 1.;
  else
    priv->progress = priv->last_progress = 0.;

  /* reset timer */
  if (priv->timer)
    {
      g_timer_start (priv->timer);

      if (!priv->source_id)
        g_timer_stop (priv->timer);
    }
}

/**
 * gtk_timeline_is_running:
 * @timeline: A #GtkTimeline
 *
 * Returns whether the timeline is running or not.
 *
 * Return Value: %TRUE if the timeline is running
 **/
gboolean
gtk_timeline_is_running (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), FALSE);

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  return (priv->source_id != 0);
}

/**
 * gtk_timeline_get_fps:
 * @timeline: A #GtkTimeline
 *
 * Returns the number of frames per second.
 *
 * Return Value: frames per second
 **/
guint
gtk_timeline_get_fps (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), 1);

  priv = GTK_TIMELINE_GET_PRIV (timeline);
  return priv->fps;
}

/**
 * gtk_timeline_set_fps:
 * @timeline: A #GtkTimeline
 * @fps: frames per second
 *
 * Sets the number of frames per second that
 * the timeline will play.
 **/
void
gtk_timeline_set_fps (GtkTimeline *timeline,
                      guint        fps)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));
  g_return_if_fail (fps > 0);

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  priv->fps = fps;

  if (gtk_timeline_is_running (timeline))
    {
      g_source_remove (priv->source_id);
      priv->source_id = gdk_threads_add_timeout (FRAME_INTERVAL (priv->fps),
						 (GSourceFunc) gtk_timeline_run_frame,
						 timeline);
    }

  g_object_notify (G_OBJECT (timeline), "fps");
}

/**
 * gtk_timeline_get_loop:
 * @timeline: A #GtkTimeline
 *
 * Returns whether the timeline loops to the
 * beginning when it has reached the end.
 *
 * Return Value: %TRUE if the timeline loops
 **/
gboolean
gtk_timeline_get_loop (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), FALSE);

  priv = GTK_TIMELINE_GET_PRIV (timeline);
  return priv->loop;
}

/**
 * gtk_timeline_set_loop:
 * @timeline: A #GtkTimeline
 * @loop: %TRUE to make the timeline loop
 *
 * Sets whether the timeline loops to the beginning
 * when it has reached the end.
 **/
void
gtk_timeline_set_loop (GtkTimeline *timeline,
                       gboolean     loop)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  if (loop != priv->loop)
    {
      priv->loop = loop;
      g_object_notify (G_OBJECT (timeline), "loop");
    }
}

void
gtk_timeline_set_duration (GtkTimeline *timeline,
                           guint        duration)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  if (duration != priv->duration)
    {
      priv->duration = duration;
      g_object_notify (G_OBJECT (timeline), "duration");
    }
}

guint
gtk_timeline_get_duration (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), 0);

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  return priv->duration;
}

/**
 * gtk_timeline_set_direction:
 * @timeline: A #GtkTimeline
 * @direction: direction
 *
 * Sets the direction of the timeline.
 **/
void
gtk_timeline_set_direction (GtkTimeline          *timeline,
                            GtkTimelineDirection  direction)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  if (direction != priv->direction)
    {
      priv->direction = direction;
      g_object_notify (G_OBJECT (timeline), "direction");
    }
}

/**
 * gtk_timeline_get_direction:
 * @timeline: A #GtkTimeline
 *
 * Returns the direction of the timeline.
 *
 * Return Value: direction
 **/
GtkTimelineDirection
gtk_timeline_get_direction (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), GTK_TIMELINE_DIRECTION_FORWARD);

  priv = GTK_TIMELINE_GET_PRIV (timeline);
  return priv->direction;
}

void
gtk_timeline_set_screen (GtkTimeline *timeline,
                         GdkScreen   *screen)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  priv = GTK_TIMELINE_GET_PRIV (timeline);

  if (priv->screen)
    g_object_unref (priv->screen);

  priv->screen = g_object_ref (screen);

  g_object_notify (G_OBJECT (timeline), "screen");
}

GdkScreen *
gtk_timeline_get_screen (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), NULL);

  priv = GTK_TIMELINE_GET_PRIV (timeline);
  return priv->screen;
}

gdouble
gtk_timeline_get_progress (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), 0.);

  priv = GTK_TIMELINE_GET_PRIV (timeline);
  return priv->progress;
}

gdouble
gtk_timeline_calculate_progress (gdouble                 linear_progress,
                                 GtkTimelineProgressType progress_type)
{
  gdouble progress;

  progress = linear_progress;

  switch (progress_type)
    {
    case GTK_TIMELINE_PROGRESS_LINEAR:
      break;
    case GTK_TIMELINE_PROGRESS_SINUSOIDAL:
      progress = sinf ((progress * G_PI) / 2);
      break;
    case GTK_TIMELINE_PROGRESS_EXPONENTIAL:
      progress *= progress;
      break;
    case GTK_TIMELINE_PROGRESS_EASE_IN_EASE_OUT:
      {
        progress *= 2;

        if (progress < 1)
          progress = pow (progress, 3) / 2;
        else
          progress = (pow (progress - 2, 3) + 2) / 2;
      }
    }

  return progress;
}
