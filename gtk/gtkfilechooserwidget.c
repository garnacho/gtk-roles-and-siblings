/* GTK - The GIMP Toolkit
 * gtkfilechooserwidget.c: Embeddable file selector widget
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

#include "gtkfilechooserwidget.h"
#include "gtkfilechooserdefault.h"
#include "gtkfilechooserutils.h"
#ifdef G_OS_UNIX
#include "gtkfilesystemunix.h"
#endif
#include "gtktypebuiltins.h"

struct _GtkFileChooserWidgetPrivate
{
  GtkWidget *impl;

  GtkFileSystem *file_system;
};

#define GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE(o)  (GTK_FILE_CHOOSER_WIDGET (o)->priv)

static void gtk_file_chooser_widget_class_init   (GtkFileChooserWidgetClass *class);
static void gtk_file_chooser_widget_init         (GtkFileChooserWidget      *chooser_widget);

static GObject* gtk_file_chooser_widget_constructor  (GType                  type,
						      guint                  n_construct_properties,
						      GObjectConstructParam *construct_params);
static void     gtk_file_chooser_widget_set_property (GObject               *object,
						      guint                  prop_id,
						      const GValue          *value,
						      GParamSpec            *pspec);
static void     gtk_file_chooser_widget_get_property (GObject               *object,
						      guint                  prop_id,
						      GValue                *value,
						      GParamSpec            *pspec);

static GObjectClass *parent_class;

GType
gtk_file_chooser_widget_get_type (void)
{
  static GType file_chooser_widget_type = 0;

  if (!file_chooser_widget_type)
    {
      static const GTypeInfo file_chooser_widget_info =
      {
	sizeof (GtkFileChooserWidgetClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_file_chooser_widget_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkFileChooserWidget),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_file_chooser_widget_init,
      };
      
      static const GInterfaceInfo file_chooser_info =
      {
	(GInterfaceInitFunc) _gtk_file_chooser_delegate_iface_init, /* interface_init */
	NULL,			                                    /* interface_finalize */
	NULL			                                    /* interface_data */
      };

      file_chooser_widget_type = g_type_register_static (GTK_TYPE_VBOX, "GtkFileChooserWidget",
							 &file_chooser_widget_info, 0);
      g_type_add_interface_static (file_chooser_widget_type,
				   GTK_TYPE_FILE_CHOOSER,
				   &file_chooser_info);
    }

  return file_chooser_widget_type;
}

static void
gtk_file_chooser_widget_class_init (GtkFileChooserWidgetClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->constructor = gtk_file_chooser_widget_constructor;
  gobject_class->set_property = gtk_file_chooser_widget_set_property;
  gobject_class->get_property = gtk_file_chooser_widget_get_property;

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserWidgetPrivate));
}

static void
gtk_file_chooser_widget_init (GtkFileChooserWidget *chooser_widget)
{
  GtkFileChooserWidgetPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (chooser_widget,
								   GTK_TYPE_FILE_CHOOSER_WIDGET,
								   GtkFileChooserWidgetPrivate);
  chooser_widget->priv = priv;
}

static GObject*
gtk_file_chooser_widget_constructor (GType                  type,
				     guint                  n_construct_properties,
				     GObjectConstructParam *construct_params)
{
  GtkFileChooserWidgetPrivate *priv;
  GObject *object;
  gchar *current_folder;
  gchar *current_folder_uri;
  
  object = parent_class->constructor (type,
				      n_construct_properties,
				      construct_params);
  priv = GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE (object);

  gtk_widget_push_composite_child ();

  if (!priv->file_system)
    priv->file_system = gtk_file_system_unix_new ();
      
  priv->impl = _gtk_file_chooser_default_new (priv->file_system);
  gtk_box_pack_start (GTK_BOX (object), priv->impl, TRUE, TRUE, 0);
  gtk_widget_show (priv->impl);

  current_folder = g_get_current_dir ();
  current_folder_uri = g_filename_to_uri (current_folder, NULL, NULL);
  if (current_folder_uri)
    {
      gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (priv->impl), current_folder_uri);
      g_free (current_folder_uri);
    }
  g_free (current_folder);
  
  _gtk_file_chooser_set_delegate (GTK_FILE_CHOOSER (object),
				  GTK_FILE_CHOOSER (priv->impl));
  
  gtk_widget_pop_composite_child ();

  return object;
}

static void
gtk_file_chooser_widget_set_property (GObject         *object,
				      guint            prop_id,
				      const GValue    *value,
				      GParamSpec      *pspec)
{
  GtkFileChooserWidgetPrivate *priv = GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM:
      {
	GtkFileSystem *file_system = g_value_get_object (value);
	if (priv->file_system != file_system)
	  {
	    if (priv->file_system)
	      g_object_unref (priv->file_system);
	    priv->file_system = file_system;
	    if (priv->file_system)
	      g_object_ref (priv->file_system);
	  }
      }
      break;
    default:
      g_object_set_property (G_OBJECT (priv->impl), pspec->name, value);
      break;
    }
}

static void
gtk_file_chooser_widget_get_property (GObject         *object,
				      guint            prop_id,
				      GValue          *value,
				      GParamSpec      *pspec)
{
  GtkFileChooserWidgetPrivate *priv = GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE (object);
  
  g_object_get_property (G_OBJECT (priv->impl), pspec->name, value);
}

/**
 * gtk_file_chooser_widget_new:
 * @action: Open or save mode for the widget
 * 
 * Creates a new #GtkFileChooserWidget.  This is a file chooser widget that can
 * be embedded in custom windows, and it is the same widget that is used by
 * #GtkFileChooserDialog.
 * 
 * Return value: a new #GtkFileChooserWidget
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_widget_new (GtkFileChooserAction action)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
		       "action", action,
		       NULL);
}
