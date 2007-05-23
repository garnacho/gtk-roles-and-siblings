/* gtktooltip.h
 *
 * Copyright (C) 2006-2007 Imendio AB
 * Contact: Kristian Rietveld <kris@imendio.com>
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

#ifndef __GTK_TOOLTIP_H__
#define __GTK_TOOLTIP_H__

#include "gtkwidget.h"
#include "gtkwindow.h"

G_BEGIN_DECLS

#define GTK_TYPE_TOOLTIP                 (gtk_tooltip_get_type ())

GType gtk_tooltip_get_type (void);

void gtk_tooltip_set_markup            (GtkTooltip  *tooltip,
			                const gchar *markup);
void gtk_tooltip_set_icon              (GtkTooltip  *tooltip,
				        GdkPixbuf   *pixbuf);
void gtk_tooltip_set_icon_from_stock   (GtkTooltip  *tooltip,
				        const gchar *stock_id,
				        GtkIconSize  size);
void gtk_tooltip_set_custom	       (GtkTooltip  *tooltip,
				        GtkWidget   *custom_widget);

void gtk_tooltip_trigger_tooltip_query (GdkDisplay  *display);


void _gtk_tooltip_focus_in             (GtkWidget   *widget);
void _gtk_tooltip_focus_out            (GtkWidget   *widget);
void _gtk_tooltip_toggle_keyboard_mode (GtkWidget   *widget);
void _gtk_tooltip_handle_event         (GdkEvent    *event);
void _gtk_tooltip_hide                 (GtkWidget   *widget);

G_END_DECLS

#endif /* __GTK_TOOLTIP_H__ */
