/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * GtkLayout: Widget for scrolling of arbitrary-sized areas.
 *
 * Copyright Owen Taylor, 1998
 */

#ifndef __GTK_LAYOUT_H
#define __GTK_LAYOUT_H

#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkadjustment.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_LAYOUT(obj)          GTK_CHECK_CAST (obj, gtk_layout_get_type (), GtkLayout)
#define GTK_LAYOUT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_layout_get_type (), GtkLayoutClass)
#define GTK_IS_LAYOUT(obj)       GTK_CHECK_TYPE (obj, gtk_layout_get_type ())

typedef struct _GtkLayout        GtkLayout;
typedef struct _GtkLayoutClass   GtkLayoutClass;
typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutChild {
  GtkWidget *widget;
  GdkWindow *window;	/* For NO_WINDOW widgets */
  gint x;
  gint y;
};

struct _GtkLayout {
  GtkContainer container;

  GList *children;

  guint width;
  guint height;

  guint xoffset;
  guint yoffset;

  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  
  GdkWindow *bin_window;

  GdkVisibilityState visibility;
  gulong configure_serial;
  gint scroll_x;
  gint scroll_y;

  guint frozen : 1;
};

struct _GtkLayoutClass {
  GtkContainerClass parent_class;

  void  (*scroll_adjustments)   (GtkLayout	*text,
				 GtkAdjustment  *hadjustment,
				 GtkAdjustment  *vadjustment);
};

GtkWidget*     gtk_layout_new             (GtkAdjustment *hadjustment,
				           GtkAdjustment *vadjustment);

guint          gtk_layout_get_type        (void);
void           gtk_layout_put             (GtkLayout     *layout, 
		                           GtkWidget     *widget, 
		                           gint           x, 
		                           gint           y);
  
void           gtk_layout_move            (GtkLayout     *layout, 
		                           GtkWidget     *widget, 
		                           gint           x, 
		                           gint           y);
  
void           gtk_layout_set_size        (GtkLayout     *layout, 
			                   guint          width,
			                   guint          height);

/* These disable and enable moving and repainting the scrolling window of the GtkLayout,
 * respectively.  If you want to update the layout's offsets but do not want it to
 * repaint itself, you should use these functions.
 */
void           gtk_layout_freeze          (GtkLayout     *layout);
void           gtk_layout_thaw            (GtkLayout     *layout);

GtkAdjustment* gtk_layout_get_hadjustment (GtkLayout     *layout);
GtkAdjustment* gtk_layout_get_vadjustment (GtkLayout     *layout);
void           gtk_layout_set_hadjustment (GtkLayout     *layout,
					   GtkAdjustment *adjustment);
void           gtk_layout_set_vadjustment (GtkLayout     *layout,
					   GtkAdjustment *adjustment);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_LAYOUT_H */
