/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkmessagedialog.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkimage.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"
#include <string.h>

static void gtk_message_dialog_class_init (GtkMessageDialogClass *klass);
static void gtk_message_dialog_init       (GtkMessageDialog      *dialog);
static void gtk_message_dialog_style_set  (GtkWidget             *widget,
                                           GtkStyle              *prev_style);

static void gtk_message_dialog_set_property (GObject          *object,
					     guint             prop_id,
					     const GValue     *value,
					     GParamSpec       *pspec);
static void gtk_message_dialog_get_property (GObject          *object,
					     guint             prop_id,
					     GValue           *value,
					     GParamSpec       *pspec);
static void gtk_message_dialog_add_buttons  (GtkMessageDialog *message_dialog,
					     GtkButtonsType    buttons);


enum {
  PROP_0,
  PROP_MESSAGE_TYPE,
  PROP_BUTTONS
};

static gpointer parent_class;

GType
gtk_message_dialog_get_type (void)
{
  static GType dialog_type = 0;

  if (!dialog_type)
    {
      static const GTypeInfo dialog_info =
      {
	sizeof (GtkMessageDialogClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_message_dialog_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkMessageDialog),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_message_dialog_init,
      };

      dialog_type = g_type_register_static (GTK_TYPE_DIALOG, "GtkMessageDialog",
					    &dialog_info, 0);
    }

  return dialog_type;
}

static void
gtk_message_dialog_class_init (GtkMessageDialogClass *class)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;

  widget_class = GTK_WIDGET_CLASS (class);
  gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  
  widget_class->style_set = gtk_message_dialog_style_set;

  gobject_class->set_property = gtk_message_dialog_set_property;
  gobject_class->get_property = gtk_message_dialog_get_property;
  
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("message_border",
                                                             _("Image/label border"),
                                                             _("Width of border around the label and image in the message dialog"),
                                                             0,
                                                             G_MAXINT,
                                                             8,
                                                             G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_MESSAGE_TYPE,
                                   g_param_spec_enum ("message_type",
						      _("Message Type"),
						      _("The type of message"),
						      GTK_TYPE_MESSAGE_TYPE,
                                                      GTK_MESSAGE_INFO,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (gobject_class,
                                   PROP_BUTTONS,
                                   g_param_spec_enum ("buttons",
						      _("Message Buttons"),
						      _("The buttons shown in the message dialog"),
						      GTK_TYPE_BUTTONS_TYPE,
                                                      GTK_BUTTONS_NONE,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

}

static void
gtk_message_dialog_init (GtkMessageDialog *dialog)
{
  GtkWidget *hbox;
  
  dialog->label = gtk_label_new (NULL);
  dialog->image = gtk_image_new_from_stock (NULL, GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (dialog->image), 0.5, 0.0);
  
  gtk_label_set_line_wrap (GTK_LABEL (dialog->label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (dialog->label), TRUE);
  
  hbox = gtk_hbox_new (FALSE, 6);

  gtk_box_pack_start (GTK_BOX (hbox), dialog->image,
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), dialog->label,
                      TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      hbox,
                      FALSE, FALSE, 0);

  gtk_widget_show_all (hbox);
}

static GtkMessageType
gtk_message_dialog_get_message_type (GtkMessageDialog *dialog)
{
  const gchar* stock_id = NULL;

  g_return_val_if_fail (GTK_IS_MESSAGE_DIALOG (dialog), GTK_MESSAGE_INFO);
  g_return_val_if_fail (GTK_IS_IMAGE(dialog->image), GTK_MESSAGE_INFO);

  stock_id = GTK_IMAGE(dialog->image)->data.stock.stock_id;

  /* Look at the stock id of the image to guess the
   * GtkMessageType value that was used to choose it
   * in setup_type()
   */
  if (strcmp (stock_id, GTK_STOCK_DIALOG_INFO) == 0)
    return GTK_MESSAGE_INFO;
  else if (strcmp (stock_id, GTK_STOCK_DIALOG_QUESTION) == 0)
    return GTK_MESSAGE_QUESTION;
  else if (strcmp (stock_id, GTK_STOCK_DIALOG_WARNING) == 0)
    return GTK_MESSAGE_WARNING;
  else if (strcmp (stock_id, GTK_STOCK_DIALOG_ERROR) == 0)
    return GTK_MESSAGE_ERROR;
  else
    {
      g_assert_not_reached (); 
      return GTK_MESSAGE_INFO;
    }
}

static void
setup_type (GtkMessageDialog *dialog,
	    GtkMessageType    type)
{
  const gchar *stock_id = NULL;
  GtkStockItem item;
  
  switch (type)
    {
    case GTK_MESSAGE_INFO:
      stock_id = GTK_STOCK_DIALOG_INFO;
      break;

    case GTK_MESSAGE_QUESTION:
      stock_id = GTK_STOCK_DIALOG_QUESTION;
      break;

    case GTK_MESSAGE_WARNING:
      stock_id = GTK_STOCK_DIALOG_WARNING;
      break;
      
    case GTK_MESSAGE_ERROR:
      stock_id = GTK_STOCK_DIALOG_ERROR;
      break;
      
    default:
      g_warning ("Unknown GtkMessageType %d", type);
      break;
    }

  if (stock_id == NULL)
    stock_id = GTK_STOCK_DIALOG_INFO;

  if (gtk_stock_lookup (stock_id, &item))
    {
      gtk_image_set_from_stock (GTK_IMAGE (dialog->image), stock_id,
                                GTK_ICON_SIZE_DIALOG);
      
      gtk_window_set_title (GTK_WINDOW (dialog), item.label);
    }
  else
    g_warning ("Stock dialog ID doesn't exist?");  
}

static void 
gtk_message_dialog_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
  GtkMessageDialog *dialog;
  
  dialog = GTK_MESSAGE_DIALOG (object);
  
  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      setup_type (dialog, g_value_get_enum (value));
      break;
    case PROP_BUTTONS:
      gtk_message_dialog_add_buttons (dialog, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_message_dialog_get_property (GObject     *object,
				 guint        prop_id,
				 GValue      *value,
				 GParamSpec  *pspec)
{
  GtkMessageDialog *dialog;
  
  dialog = GTK_MESSAGE_DIALOG (object);
  
  switch (prop_id)
    {
    case PROP_MESSAGE_TYPE:
      g_value_set_enum (value, gtk_message_dialog_get_message_type (dialog));
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/**
 * gtk_message_dialog_new:
 * @parent: transient parent, or NULL for none 
 * @flags: flags
 * @type: type of message
 * @buttons: set of buttons to use
 * @message_format: printf()-style format string, or NULL
 * @Varargs: arguments for @message_format
 * 
 * Creates a new message dialog, which is a simple dialog with an icon
 * indicating the dialog type (error, warning, etc.) and some text the
 * user may want to see. When the user clicks a button a "response"
 * signal is emitted with response IDs from #GtkResponseType. See
 * #GtkDialog for more details.
 * 
 * Return value: a new #GtkMessageDialog
 **/
GtkWidget*
gtk_message_dialog_new (GtkWindow     *parent,
                        GtkDialogFlags flags,
                        GtkMessageType type,
                        GtkButtonsType buttons,
                        const gchar   *message_format,
                        ...)
{
  GtkWidget *widget;
  GtkDialog *dialog;
  gchar* msg = 0;
  va_list args;
  
  widget = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
			 "message_type", type,
			 "buttons", buttons,
			 NULL);
  dialog = GTK_DIALOG (widget);

  if (flags & GTK_DIALOG_NO_SEPARATOR)
    {
      g_warning ("The GTK_DIALOG_NO_SEPARATOR flag cannot be used for GtkMessageDialog");
      flags &= ~GTK_DIALOG_NO_SEPARATOR;
    }

  if (message_format)
    {
      va_start (args, message_format);
      msg = g_strdup_vprintf (message_format, args);
      va_end (args);
      
      
      gtk_label_set_text (GTK_LABEL (GTK_MESSAGE_DIALOG (widget)->label),
                          msg);
      
      g_free (msg);
    }

  if (parent != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (widget),
                                  GTK_WINDOW (parent));
  
  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_NO_SEPARATOR)
    gtk_dialog_set_has_separator (dialog, FALSE);

  return widget;
}

static void
gtk_message_dialog_add_buttons (GtkMessageDialog* message_dialog,
				GtkButtonsType buttons)
{
  GtkDialog* dialog = GTK_DIALOG (message_dialog);

  switch (buttons)
    {
    case GTK_BUTTONS_NONE:
      /* nothing */
      break;

    case GTK_BUTTONS_OK:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_OK,
                             GTK_RESPONSE_OK);
      break;

    case GTK_BUTTONS_CLOSE:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_CLOSE,
                             GTK_RESPONSE_CLOSE);
      break;

    case GTK_BUTTONS_CANCEL:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_CANCEL,
                             GTK_RESPONSE_CANCEL);
      break;

    case GTK_BUTTONS_YES_NO:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_NO,
                             GTK_RESPONSE_NO);
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_YES,
                             GTK_RESPONSE_YES);
      break;

    case GTK_BUTTONS_OK_CANCEL:
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_CANCEL,
                             GTK_RESPONSE_CANCEL);
      gtk_dialog_add_button (dialog,
                             GTK_STOCK_OK,
                             GTK_RESPONSE_OK);
      break;
      
    default:
      g_warning ("Unknown GtkButtonsType");
      break;
    } 

  g_object_notify (G_OBJECT (message_dialog), "buttons");
}

static void
gtk_message_dialog_style_set (GtkWidget *widget,
                              GtkStyle  *prev_style)
{
  GtkWidget *parent;
  gint border_width = 0;

  parent = GTK_WIDGET (GTK_MESSAGE_DIALOG (widget)->image->parent);

  if (parent)
    {
      gtk_widget_style_get (widget, "message_border",
                            &border_width, NULL);
      
      gtk_container_set_border_width (GTK_CONTAINER (parent),
                                      border_width);
    }

  if (GTK_WIDGET_CLASS (parent_class)->style_set)
    (GTK_WIDGET_CLASS (parent_class)->style_set) (widget, prev_style);
}
