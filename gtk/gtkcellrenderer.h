/* gtkcellrenderer.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#ifndef __GTK_CELL_RENDERER_H__
#define __GTK_CELL_RENDERER_H__

#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkcelleditable.h>

G_BEGIN_DECLS

typedef enum
{
  GTK_CELL_RENDERER_SELECTED    = 1 << 0,
  GTK_CELL_RENDERER_PRELIT      = 1 << 1,
  GTK_CELL_RENDERER_INSENSITIVE = 1 << 2,
  /* this flag means the cell is in the sort column/row */
  GTK_CELL_RENDERER_SORTED      = 1 << 3
} GtkCellRendererState;

typedef enum
{
  GTK_CELL_RENDERER_MODE_INERT,
  GTK_CELL_RENDERER_MODE_ACTIVATABLE,
  GTK_CELL_RENDERER_MODE_EDITABLE
} GtkCellRendererMode;

#define GTK_TYPE_CELL_RENDERER		  (gtk_cell_renderer_get_type ())
#define GTK_CELL_RENDERER(obj)		  (GTK_CHECK_CAST ((obj), GTK_TYPE_CELL_RENDERER, GtkCellRenderer))
#define GTK_CELL_RENDERER_CLASS(klass)	  (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_RENDERER, GtkCellRendererClass))
#define GTK_IS_CELL_RENDERER(obj)	  (GTK_CHECK_TYPE ((obj), GTK_TYPE_CELL_RENDERER))
#define GTK_IS_CELL_RENDERER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_RENDERER))
#define GTK_CELL_RENDERER_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_CELL_RENDERER, GtkCellRendererClass))

typedef struct _GtkCellRenderer GtkCellRenderer;
typedef struct _GtkCellRendererClass GtkCellRendererClass;

struct _GtkCellRenderer
{
  GtkObject parent;

  gfloat xalign;
  gfloat yalign;

  gint width;
  gint height;

  guint16 xpad;
  guint16 ypad;

  guint mode : 2;
  guint visible : 1;
  guint is_expander : 1;
  guint is_expanded : 1;
};

struct _GtkCellRendererClass
{
  GtkObjectClass parent_class;

  /* vtable - not signals */
  void             (* get_size)      (GtkCellRenderer      *cell,
				      GtkWidget            *widget,
				      GdkRectangle         *cell_area,
				      gint                 *x_offset,
				      gint                 *y_offset,
				      gint                 *width,
				      gint                 *height);
  void             (* render)        (GtkCellRenderer      *cell,
				      GdkWindow            *window,
				      GtkWidget            *widget,
				      GdkRectangle         *background_area,
				      GdkRectangle         *cell_area,
				      GdkRectangle         *expose_area,
				      GtkCellRendererState  flags);
  gboolean         (* activate)      (GtkCellRenderer      *cell,
				      GdkEvent             *event,
				      GtkWidget            *widget,
				      const gchar          *path,
				      GdkRectangle         *background_area,
				      GdkRectangle         *cell_area,
				      GtkCellRendererState  flags);
  GtkCellEditable *(* start_editing) (GtkCellRenderer      *cell,
				      GdkEvent             *event,
				      GtkWidget            *widget,
				      const gchar          *path,
				      GdkRectangle         *background_area,
				      GdkRectangle         *cell_area,
				      GtkCellRendererState  flags);
};

GType            gtk_cell_renderer_get_type       (void) G_GNUC_CONST;

void             gtk_cell_renderer_get_size       (GtkCellRenderer      *cell,
						   GtkWidget            *widget,
						   GdkRectangle         *cell_area,
						   gint                 *x_offset,
						   gint                 *y_offset,
						   gint                 *width,
						   gint                 *height);
void             gtk_cell_renderer_render         (GtkCellRenderer      *cell,
						   GdkWindow            *window,
						   GtkWidget            *widget,
						   GdkRectangle         *background_area,
						   GdkRectangle         *cell_area,
						   GdkRectangle         *expose_area,
						   GtkCellRendererState  flags);
gboolean         gtk_cell_renderer_activate       (GtkCellRenderer      *cell,
						   GdkEvent             *event,
						   GtkWidget            *widget,
						   const gchar          *path,
						   GdkRectangle         *background_area,
						   GdkRectangle         *cell_area,
						   GtkCellRendererState  flags);
GtkCellEditable *gtk_cell_renderer_start_editing  (GtkCellRenderer      *cell,
						   GdkEvent             *event,
						   GtkWidget            *widget,
						   const gchar          *path,
						   GdkRectangle         *background_area,
						   GdkRectangle         *cell_area,
						   GtkCellRendererState  flags);
void             gtk_cell_renderer_set_fixed_size (GtkCellRenderer      *cell,
						   gint                  width,
						   gint                  height);
void             gtk_cell_renderer_get_fixed_size (GtkCellRenderer      *cell,
						   gint                 *width,
						   gint                 *height);


 
G_END_DECLS

#endif /* __GTK_CELL_RENDERER_H__ */
