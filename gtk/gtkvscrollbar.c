/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
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

#include <config.h>
#include "gtkvscrollbar.h"
#include "gdk/gdkkeysyms.h"
#include "gtkintl.h"

static void     gtk_vscrollbar_class_init       (GtkVScrollbarClass *klass);
static void     gtk_vscrollbar_init             (GtkVScrollbar      *vscrollbar);

GType
gtk_vscrollbar_get_type (void)
{
  static GType vscrollbar_type = 0;
  
  if (!vscrollbar_type)
    {
      static const GTypeInfo vscrollbar_info =
      {
        sizeof (GtkVScrollbarClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_vscrollbar_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkVScrollbar),
	0,		/* n_preallocs */
        (GInstanceInitFunc) gtk_vscrollbar_init,
      };
      
      vscrollbar_type =
	g_type_register_static (GTK_TYPE_SCROLLBAR, "GtkVScrollbar",
				&vscrollbar_info, 0);
    }
  
  return vscrollbar_type;
}

static void
gtk_vscrollbar_class_init (GtkVScrollbarClass *class)
{
  GTK_RANGE_CLASS (class)->stepper_detail = "vscrollbar";
}

static void
gtk_vscrollbar_init (GtkVScrollbar *vscrollbar)
{
  GtkRange *range;

  range = GTK_RANGE (vscrollbar);

  range->orientation = GTK_ORIENTATION_VERTICAL;
}

GtkWidget*
gtk_vscrollbar_new (GtkAdjustment *adjustment)
{
  GtkWidget *vscrollbar;
  
  vscrollbar = g_object_new (GTK_TYPE_VSCROLLBAR,
			     "adjustment", adjustment,
			     NULL);
  
  return vscrollbar;
}
