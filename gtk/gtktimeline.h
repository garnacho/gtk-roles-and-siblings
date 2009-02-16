/* aftimeline.h
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

#ifndef __GTK_TIMELINE_H__
#define __GTK_TIMELINE_H__

#include <glib-object.h>
#include <gtk/gtkenums.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GTK_TYPE_TIMELINE                 (gtk_timeline_get_type ())
#define GTK_TIMELINE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TIMELINE, GtkTimeline))
#define GTK_TIMELINE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TIMELINE, GtkTimelineClass))
#define GTK_IS_TIMELINE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TIMELINE))
#define GTK_IS_TIMELINE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TIMELINE))
#define GTK_TIMELINE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TIMELINE, GtkTimelineClass))

typedef struct GtkTimeline      GtkTimeline;
typedef struct GtkTimelineClass GtkTimelineClass;

struct GtkTimeline
{
  GObject parent_instance;
};

struct GtkTimelineClass
{
  GObjectClass parent_class;

  void (* started)           (GtkTimeline *timeline);
  void (* finished)          (GtkTimeline *timeline);
  void (* paused)            (GtkTimeline *timeline);

  void (* frame)             (GtkTimeline *timeline,
			      gdouble     progress);

  void (* __gtk_reserved1) (void);
  void (* __gtk_reserved2) (void);
  void (* __gtk_reserved3) (void);
  void (* __gtk_reserved4) (void);
};


GType                 gtk_timeline_get_type            (void) G_GNUC_CONST;

GtkTimeline           *gtk_timeline_new                (guint                     duration);
GtkTimeline           *gtk_timeline_new_for_screen     (guint                     duration,
                                                        GdkScreen                *screen);

void                  gtk_timeline_start               (GtkTimeline              *timeline);
void                  gtk_timeline_pause               (GtkTimeline              *timeline);
void                  gtk_timeline_rewind              (GtkTimeline              *timeline);

gboolean              gtk_timeline_is_running          (GtkTimeline              *timeline);

guint                 gtk_timeline_get_fps             (GtkTimeline              *timeline);
void                  gtk_timeline_set_fps             (GtkTimeline              *timeline,
                                                        guint                     fps);

gboolean              gtk_timeline_get_loop            (GtkTimeline              *timeline);
void                  gtk_timeline_set_loop            (GtkTimeline              *timeline,
                                                        gboolean                  loop);

guint                 gtk_timeline_get_duration        (GtkTimeline              *timeline);
void                  gtk_timeline_set_duration        (GtkTimeline              *timeline,
                                                        guint                     duration);

GdkScreen            *gtk_timeline_get_screen          (GtkTimeline              *timeline);
void                  gtk_timeline_set_screen          (GtkTimeline              *timeline,
                                                        GdkScreen                *screen);

GtkTimelineDirection  gtk_timeline_get_direction       (GtkTimeline              *timeline);
void                  gtk_timeline_set_direction       (GtkTimeline              *timeline,
                                                        GtkTimelineDirection      direction);

gdouble               gtk_timeline_get_progress        (GtkTimeline              *timeline);

gdouble               gtk_timeline_calculate_progress  (gdouble                   linear_progress,
                                                        GtkTimelineProgressType   progress_type);

G_END_DECLS

#endif /* __GTK_TIMELINE_H__ */
