/* GTK - The GIMP Toolkit
 * gtkfilechooserdialog.c: File selector dialog
 * Copyright (C) 2003, Red Hat, Inc.
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

#include "gtkfilechooserdialog.h"
#include "gtkfilechooserwidget.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooserembed.h"
#include "gtkfilesystem.h"
#include "gtktypebuiltins.h"

#include <stdarg.h>

struct _GtkFileChooserDialogPrivate
{
  GtkWidget *widget;
  
  char *file_system;

  /* for use with GtkFileChooserEmbed */
  gint default_width;
  gint default_height;
};

#define GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE(o)  (GTK_FILE_CHOOSER_DIALOG (o)->priv)

static void gtk_file_chooser_dialog_class_init (GtkFileChooserDialogClass *class);
static void gtk_file_chooser_dialog_init       (GtkFileChooserDialog      *dialog);
static void gtk_file_chooser_dialog_finalize   (GObject                   *object);

static GObject* gtk_file_chooser_dialog_constructor  (GType                  type,
						      guint                  n_construct_properties,
						      GObjectConstructParam *construct_params);
static void     gtk_file_chooser_dialog_set_property (GObject               *object,
						      guint                  prop_id,
						      const GValue          *value,
						      GParamSpec            *pspec);
static void     gtk_file_chooser_dialog_get_property (GObject               *object,
						      guint                  prop_id,
						      GValue                *value,
						      GParamSpec            *pspec);

static void     gtk_file_chooser_dialog_style_set    (GtkWidget             *widget,
						      GtkStyle              *previous_style);

static GObjectClass *parent_class;

GType
gtk_file_chooser_dialog_get_type (void)
{
  static GType file_chooser_dialog_type = 0;

  if (!file_chooser_dialog_type)
    {
      static const GTypeInfo file_chooser_dialog_info =
      {
	sizeof (GtkFileChooserDialogClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_chooser_dialog_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileChooserDialog),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_chooser_dialog_init,
      };

      static const GInterfaceInfo file_chooser_info =
      {
	(GInterfaceInitFunc) _gtk_file_chooser_delegate_iface_init, /* interface_init */
	NULL,			                                    /* interface_finalize */
	NULL			                                    /* interface_data */
      };

      file_chooser_dialog_type = g_type_register_static (GTK_TYPE_DIALOG, "GtkFileChooserDialog",
							 &file_chooser_dialog_info, 0);
      g_type_add_interface_static (file_chooser_dialog_type,
				   GTK_TYPE_FILE_CHOOSER,
				   &file_chooser_info);
    }

  return file_chooser_dialog_type;
}

static void
gtk_file_chooser_dialog_class_init (GtkFileChooserDialogClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->constructor = gtk_file_chooser_dialog_constructor;
  gobject_class->set_property = gtk_file_chooser_dialog_set_property;
  gobject_class->get_property = gtk_file_chooser_dialog_get_property;
  gobject_class->finalize = gtk_file_chooser_dialog_finalize;

  widget_class->style_set = gtk_file_chooser_dialog_style_set;

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserDialogPrivate));
}

static void
gtk_file_chooser_dialog_init (GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
								   GTK_TYPE_FILE_CHOOSER_DIALOG,
								   GtkFileChooserDialogPrivate);
  dialog->priv = priv;
  dialog->priv->default_width = -1;
  dialog->priv->default_height = -1;

  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
}

static void
gtk_file_chooser_dialog_finalize (GObject *object)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (object);

  g_free (dialog->priv->file_system);

  G_OBJECT_CLASS (parent_class)->finalize (object);  
}

/* Callback used when the user activates a file in the file chooser widget */
static void
file_chooser_widget_file_activated (GtkFileChooser       *chooser,
				    GtkFileChooserDialog *dialog)
{
  gtk_window_activate_default (GTK_WINDOW (dialog));
}

static void
file_chooser_widget_resizable_hints_changed (GtkWidget            *widget,
					     GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv;
  gboolean resize_horizontally;
  gboolean resize_vertically;
  GdkGeometry geometry;

  priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

  _gtk_file_chooser_embed_get_resizable_hints (GTK_FILE_CHOOSER_EMBED (priv->widget),
					       &resize_horizontally,
					       &resize_vertically);


  geometry.min_width = -1;
  geometry.min_height = -1;
  geometry.max_width = (resize_horizontally?G_MAXSHORT:-1);
  geometry.max_height = (resize_vertically?G_MAXSHORT:-1);

  gtk_window_set_geometry_hints (GTK_WINDOW (dialog), NULL,
				 &geometry,
 				 GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
}

static void
file_chooser_widget_default_size_changed (GtkWidget            *widget,
					  GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv;
  gint extra_width;
  gint extra_height;
  gint width, height;
  GtkRequisition req;
  gboolean resize_horizontally;
  gboolean resize_vertically;

  priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

  /* Force a size request of everything before we start.  This will make sure
   * that widget->requisition is meaningful. */
  gtk_widget_size_request (GTK_WIDGET (dialog), &req);

  /* Determine how much space the rest of the dialog uses compared to priv->widget */
  extra_width = GTK_WIDGET (dialog)->requisition.width - priv->widget->requisition.width;
  extra_height = GTK_WIDGET (dialog)->requisition.height - priv->widget->requisition.height;

  _gtk_file_chooser_embed_get_default_size (GTK_FILE_CHOOSER_EMBED (priv->widget),
					    &width, &height);

  width = extra_width + width;
  height = extra_height + height;

  /* FIXME: We should make sure that we arent' bigger than the current screen */
  if (GTK_WIDGET_REALIZED (dialog) &&
      priv->default_width > 0 &&
      priv->default_height > 0)
    {
      gint cur_width, cur_height;
      gint dx, dy;

      gtk_window_get_size (GTK_WINDOW (dialog), &cur_width, &cur_height);

      dx = width - priv->default_width;
      dy = height - priv->default_height;
      gtk_window_resize (GTK_WINDOW (dialog),
			 cur_width + dx,
			 cur_height + dy);
    }
  else
    {
      gtk_window_resize (GTK_WINDOW (dialog), width, height);
    }

  _gtk_file_chooser_embed_get_resizable_hints (GTK_FILE_CHOOSER_EMBED (priv->widget),
					       &resize_horizontally,
					       &resize_vertically);

  /* Only store the size if we can resize in that direction. */
  if (resize_horizontally)
    priv->default_width = width;
  if (resize_vertically)
    priv->default_height = height;
}


static GObject*
gtk_file_chooser_dialog_constructor (GType                  type,
				     guint                  n_construct_properties,
				     GObjectConstructParam *construct_params)
{
  GtkFileChooserDialogPrivate *priv;
  GObject *object;

  object = parent_class->constructor (type,
				      n_construct_properties,
				      construct_params);
  priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (object);

  gtk_widget_push_composite_child ();

  if (priv->file_system)
    priv->widget = g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
				 "file-system-backend", priv->file_system,
				 NULL);
  else
    priv->widget = g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET, NULL);

  g_signal_connect (priv->widget, "file-activated",
		    G_CALLBACK (file_chooser_widget_file_activated), object);
  g_signal_connect (priv->widget, "default-size-changed",
		    G_CALLBACK (file_chooser_widget_default_size_changed), object);
  g_signal_connect (priv->widget, "resizable-hints-changed",
		    G_CALLBACK (file_chooser_widget_resizable_hints_changed), object);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox), priv->widget, TRUE, TRUE, 0);
  gtk_widget_show (priv->widget);

  _gtk_file_chooser_set_delegate (GTK_FILE_CHOOSER (object),
				  GTK_FILE_CHOOSER (priv->widget));

  gtk_widget_pop_composite_child ();

  return object;
}

static void
gtk_file_chooser_dialog_set_property (GObject         *object,
				      guint            prop_id,
				      const GValue    *value,
				      GParamSpec      *pspec)

{
  GtkFileChooserDialogPrivate *priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND:
      g_free (priv->file_system);
      priv->file_system = g_value_dup_string (value);
      break;
    default:
      g_object_set_property (G_OBJECT (priv->widget), pspec->name, value);
      break;
    }
}

static void
gtk_file_chooser_dialog_get_property (GObject         *object,
				      guint            prop_id,
				      GValue          *value,
				      GParamSpec      *pspec)
{
  GtkFileChooserDialogPrivate *priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (object);

  g_object_get_property (G_OBJECT (priv->widget), pspec->name, value);
}

#if 0
static void
set_default_size (GtkFileChooserDialog *dialog)
{
  GtkWidget *widget;
  GtkWindow *window;
  int default_width, default_height;
  int width, height;
  int font_size;
  GdkScreen *screen;
  int monitor_num;
  GtkRequisition req;
  GdkRectangle monitor;

  widget = GTK_WIDGET (dialog);
  window = GTK_WINDOW (dialog);

  /* Size based on characters */

  font_size = pango_font_description_get_size (widget->style->font_desc);
  font_size = PANGO_PIXELS (font_size);

  width = font_size * NUM_CHARS;
  height = font_size * NUM_LINES;

  /* Use at least the requisition size... */

  gtk_widget_size_request (widget, &req);
  width = MAX (width, req.width);
  height = MAX (height, req.height);

  /* ... but no larger than the monitor */

  screen = gtk_widget_get_screen (widget);
  monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);

  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  width = MIN (width, monitor.width * 3 / 4);
  height = MIN (height, monitor.height * 3 / 4);

  /* Set size */

  gtk_window_get_default_size (window, &default_width, &default_height);

  gtk_window_set_default_size (window,
			       (default_width == -1) ? width : default_width,
			       (default_height == -1) ? height : default_height);
}
#endif

static void
gtk_file_chooser_dialog_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style)
{
  GtkDialog *dialog;

  if (GTK_WIDGET_CLASS (parent_class)->style_set)
    GTK_WIDGET_CLASS (parent_class)->style_set (widget, previous_style);

  dialog = GTK_DIALOG (widget);

  /* Override the style properties with HIG-compliant spacings.  Ugh.
   * http://developer.gnome.org/projects/gup/hig/1.0/layout.html#layout-dialogs
   * http://developer.gnome.org/projects/gup/hig/1.0/windows.html#alert-spacing
   */

  gtk_container_set_border_width (GTK_CONTAINER (dialog->vbox), 12);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 24);

  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 0);
  gtk_box_set_spacing (GTK_BOX (dialog->action_area), 6);
}

static GtkWidget *
gtk_file_chooser_dialog_new_valist (const gchar          *title,
				    GtkWindow            *parent,
				    GtkFileChooserAction  action,
				    const gchar          *backend,
				    const gchar          *first_button_text,
				    va_list               varargs)
{
  GtkWidget *result;
  const char *button_text = first_button_text;
  gint response_id;

  result = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			 "title", title,
			 "action", action,
			 "file-system-backend", backend,
			 NULL);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (result), parent);

  while (button_text)
    {
      response_id = va_arg (varargs, gint);
      gtk_dialog_add_button (GTK_DIALOG (result), button_text, response_id);
      button_text = va_arg (varargs, const gchar *);
    }

  return result;
}

/**
 * gtk_file_chooser_dialog_new:
 * @title: Title of the dialog, or %NULL
 * @parent: Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog
 * @first_button_text: stock ID or text to go in the first button, or %NULL
 * @Varargs: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #GtkFileChooserDialog.  This function is analogous to
 * gtk_dialog_new_with_buttons().
 *
 * Return value: a new #GtkFileChooserDialog
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_dialog_new (const gchar         *title,
			     GtkWindow           *parent,
			     GtkFileChooserAction action,
			     const gchar         *first_button_text,
			     ...)
{
  GtkWidget *result;
  va_list varargs;
  
  va_start (varargs, first_button_text);
  result = gtk_file_chooser_dialog_new_valist (title, parent, action,
					       NULL, first_button_text,
					       varargs);
  va_end (varargs);

  return result;
}

/**
 * gtk_file_chooser_dialog_new_with_backend:
 * @title: Title of the dialog, or %NULL
 * @parent: Transient parent of the dialog, or %NULL
 * @backend: The name of the specific filesystem backend to use.
 * @action: Open or save mode for the dialog
 * @first_button_text: stock ID or text to go in the first button, or %NULL
 * @Varargs: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #GtkFileChooserDialog with a specified backend. This is
 * especially useful if you use gtk_file_chooser_set_local_only() to allow
 * non-local files and you use a more expressive vfs, such as gnome-vfs,
 * to load files.
 *
 * Return value: a new #GtkFileChooserDialog
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_dialog_new_with_backend (const gchar          *title,
					  GtkWindow            *parent,
					  GtkFileChooserAction  action,
					  const gchar          *backend,
					  const gchar          *first_button_text,
					  ...)
{
  GtkWidget *result;
  va_list varargs;
  
  va_start (varargs, first_button_text);
  result = gtk_file_chooser_dialog_new_valist (title, parent, action,
					       backend, first_button_text,
					       varargs);
  va_end (varargs);

  return result;
}
