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
#ifndef __GTK_COLOR_SELECTION_H__
#define __GTK_COLOR_SELECTION_H__

#include <gtk/gtkdialog.h>
#include <gtk/gtkvbox.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_COLOR_SELECTION			(gtk_color_selection_get_type ())
#define GTK_COLOR_SELECTION(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_COLOR_SELECTION, GtkColorSelection))
#define GTK_COLOR_SELECTION_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_COLOR_SELECTION, GtkColorSelectionClass))
#define GTK_IS_COLOR_SELECTION(obj)			(GTK_CHECK_TYPE ((obj), GTK_TYPE_COLOR_SELECTION))
#define GTK_IS_COLOR_SELECTION_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), GTK_TYPE_COLOR_SELECTION))
#define GTK_COLOR_SELECTION_GET_CLASS(obj)              (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_COLOR_SELECTION, GtkColorSelectionClass))


typedef struct _GtkColorSelection       GtkColorSelection;
typedef struct _GtkColorSelectionClass  GtkColorSelectionClass;


typedef void (* GtkColorSelectionChangePaletteFunc) (const GdkColor    *colors,
                                                     gint               n_colors);

struct _GtkColorSelection
{
  GtkVBox parent_instance;

  /* < private_data > */
  gpointer private_data;
};

struct _GtkColorSelectionClass
{
  GtkVBoxClass parent_class;

  void (*color_changed)	(GtkColorSelection *color_selection);
};


/* ColorSelection */

GtkType    gtk_color_selection_get_type                (void) G_GNUC_CONST;
GtkWidget *gtk_color_selection_new                     (void);
gboolean   gtk_color_selection_get_has_opacity_control (GtkColorSelection *colorsel);
void       gtk_color_selection_set_has_opacity_control (GtkColorSelection *colorsel,
							gboolean           has_opacity);
gboolean   gtk_color_selection_get_has_palette         (GtkColorSelection *colorsel);
void       gtk_color_selection_set_has_palette         (GtkColorSelection *colorsel,
							gboolean           has_palette);


void     gtk_color_selection_set_current_color   (GtkColorSelection *colorsel,
						  GdkColor          *color);
void     gtk_color_selection_set_current_alpha   (GtkColorSelection *colorsel,
						  guint16            alpha);
void     gtk_color_selection_get_current_color   (GtkColorSelection *colorsel,
						  GdkColor          *color);
guint16  gtk_color_selection_get_current_alpha   (GtkColorSelection *colorsel);
void     gtk_color_selection_set_previous_color  (GtkColorSelection *colorsel,
						  GdkColor          *color);
void     gtk_color_selection_set_previous_alpha  (GtkColorSelection *colorsel,
						  guint16            alpha);
void     gtk_color_selection_get_previous_color  (GtkColorSelection *colorsel,
						  GdkColor          *color);
guint16  gtk_color_selection_get_previous_alpha  (GtkColorSelection *colorsel);

gboolean gtk_color_selection_is_adjusting        (GtkColorSelection *colorsel);

gboolean gtk_color_selection_palette_from_string (const gchar       *str,
                                                  GdkColor         **colors,
                                                  gint              *n_colors);
gchar*   gtk_color_selection_palette_to_string   (const GdkColor    *colors,
                                                  gint               n_colors);

GtkColorSelectionChangePaletteFunc gtk_color_selection_set_change_palette_hook (GtkColorSelectionChangePaletteFunc func);

#ifndef GTK_DISABLE_DEPRECATED
/* Deprecated calls: */
void gtk_color_selection_set_color         (GtkColorSelection *colorsel,
					    gdouble           *color);
void gtk_color_selection_get_color         (GtkColorSelection *colorsel,
					    gdouble           *color);
void gtk_color_selection_set_update_policy (GtkColorSelection *colorsel,
					    GtkUpdateType      policy);
#endif /* GTK_DISABLE_DEPRECATED */

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_COLOR_SELECTION_H__ */
