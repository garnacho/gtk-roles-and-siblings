/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001, 2002 Anders Carlsson
 * Copyright (C) 2003, 2004 Matthias Clasen <mclasen@redhat.com>
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
 * Author: Anders Carlsson <andersca@gnome.org>
 *
 * Modified by the GTK+ Team and others 1997-2004.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>

#include <gdk/gdkkeysyms.h>

#include "gtkalias.h"

#include "gtkaboutdialog.h"
#include "gtkbutton.h"
#include "gtkbbox.h"
#include "gtkdialog.h"
#include "gtkhbox.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtknotebook.h"
#include "gtkscrolledwindow.h"
#include "gtkstock.h"
#include "gtktextview.h"
#include "gtkvbox.h"
#include "gtkviewport.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"

#include <string.h>

typedef struct _GtkAboutDialogPrivate GtkAboutDialogPrivate;
struct _GtkAboutDialogPrivate 
{
  gchar *name;
  gchar *version;
  gchar *copyright;
  gchar *comments;
  gchar *website;
  gchar *website_label;
  gchar *translator_credits;
  gchar *license;
  
  gchar **authors;
  gchar **documenters;
  gchar **artists;
  
  GtkWidget *logo_image;
  GtkWidget *name_label;
  GtkWidget *comments_label;
  GtkWidget *copyright_label;
  GtkWidget *website_button;

  GtkWidget *credits_button;
  GtkWidget *credits_dialog;
  GtkWidget *license_button;
  GtkWidget *license_dialog;
  
  GdkCursor *hand_cursor;
  GdkCursor *regular_cursor;
  gboolean hovering_over_link;
};

#define GTK_ABOUT_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_ABOUT_DIALOG, GtkAboutDialogPrivate))


enum 
{
  PROP_0,
  PROP_NAME,
  PROP_VERSION,
  PROP_COPYRIGHT,
  PROP_COMMENTS,
  PROP_WEBSITE,
  PROP_WEBSITE_LABEL,
  PROP_LICENSE,
  PROP_AUTHORS,
  PROP_DOCUMENTERS,
  PROP_TRANSLATOR_CREDITS,
  PROP_ARTISTS,
  PROP_LOGO,
  PROP_LOGO_ICON_NAME
};

static void                 gtk_about_dialog_finalize       (GObject            *object);
static void                 gtk_about_dialog_get_property   (GObject            *object,
							     guint               prop_id,
							     GValue             *value,
							     GParamSpec         *pspec);
static void                 gtk_about_dialog_set_property   (GObject            *object,
							     guint               prop_id,
							     const GValue       *value,
							     GParamSpec         *pspec);
static void                 gtk_about_dialog_style_set      (GtkWidget          *widget,
			                                     GtkStyle           *previous_style);
static void                 dialog_style_set                (GtkWidget          *widget,
		                                             GtkStyle           *previous_style,
		                                             gpointer            data);
static void                 update_name_version             (GtkAboutDialog     *about);
static GtkIconSet *         icon_set_new_from_pixbufs       (GList              *pixbufs);
static void                 activate_url                    (GtkWidget          *widget,
							     gpointer            data);
static void                 set_link_button_text            (GtkWidget          *about,
							     GtkWidget          *button,
							     gchar              *text);
static GtkWidget *          create_link_button              (GtkWidget          *about,
							     gchar              *text,
							     gchar              *url,
							     GCallback           callback,
							     gpointer            data);
static void                 follow_if_link                  (GtkAboutDialog     *about,
							     GtkTextIter        *iter);
static void                 set_cursor_if_appropriate       (GtkAboutDialog     *about,
							     GtkTextView        *text_view,
							     gint                x,
							     gint                y);
static void                 add_credits_page                (GtkAboutDialog     *about,
							     GtkWidget          *notebook,
							     gchar              *title,
							     gchar             **people);
static gboolean             credits_key_press_event         (GtkWidget          *text_view,
							     GdkEventKey        *event,
							     GtkAboutDialog     *about);
static gboolean             credits_event_after             (GtkWidget          *text_view,
							     GdkEvent           *event,
							     GtkAboutDialog     *about);
static gboolean             credits_motion_notify_event     (GtkWidget          *text_view,
							     GdkEventMotion     *event,
							     GtkAboutDialog     *about);
static gboolean             credits_visibility_notify_event (GtkWidget          *text_view,
							     GdkEventVisibility *event,
							     GtkAboutDialog     *about);
static void                 display_credits_dialog          (GtkWidget          *button,
							     gpointer            data);
static void                 display_license_dialog          (GtkWidget          *button,
							     gpointer            data);
static void                 close_cb                        (GtkAboutDialog     *about);
				 
				 				       
static GtkAboutDialogActivateLinkFunc activate_email_hook = NULL;
static gpointer activate_email_hook_data = NULL;
static GDestroyNotify activate_email_hook_destroy = NULL;

static GtkAboutDialogActivateLinkFunc activate_url_hook = NULL;
static gpointer activate_url_hook_data = NULL;
static GDestroyNotify activate_url_hook_destroy = NULL;

G_DEFINE_TYPE (GtkAboutDialog, gtk_about_dialog, GTK_TYPE_DIALOG);

static void
gtk_about_dialog_class_init (GtkAboutDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkDialogClass *dialog_class;
	
  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  dialog_class = (GtkDialogClass *)klass;
	
  object_class->set_property = gtk_about_dialog_set_property;
  object_class->get_property = gtk_about_dialog_get_property;

  object_class->finalize = gtk_about_dialog_finalize;

  widget_class->style_set = gtk_about_dialog_style_set;

  /**
   * GtkAboutDialog:name:
   *
   * The name of the program. 
   * If this is not set, it defaults to g_get_application_name().
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
							P_("Program name"),
							P_("The name of the program. If this is not set, it defaults to g_get_application_name()"),
							NULL,
							G_PARAM_READWRITE));

  /**
   * GtkAboutDialog:version:
   *
   * The version of the program. 
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_VERSION,
				   g_param_spec_string ("version",
							P_("Program version"),
							P_("The version of the program"),
							NULL,
							G_PARAM_READWRITE));

  /**
   * GtkAboutDialog:copyright:
   *
   * Copyright information for the program. 
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_COPYRIGHT,
				   g_param_spec_string ("copyright",
							P_("Copyright string"),
							P_("Copyright information for the program"),
							NULL,
							G_PARAM_READWRITE));
	

  /**
   * GtkAboutDialog:comments:
   *
   * Comments about the program. This string is displayed in a label 
   * in the main dialog, thus it should be a short explanation of
   * the main purpose of the program, not a detailed list of features.
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_COMMENTS,
				   g_param_spec_string ("comments",
							P_("Comments string"),
							P_("Comments about the program"),
							NULL,
							G_PARAM_READWRITE));

  /**
   * GtkAboutDialog:license:
   *
   * The license of the program. This string is displayed in a 
   * text view in a secondary dialog, therefore it is fine to use
   * a long multi-paragraph text. Note that the text is not wrapped
   * in the text view, thus it must contain the intended linebreaks.
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_LICENSE,
				   g_param_spec_string ("license",
							_("License"),
							_("The license of the program"),
							NULL,
							G_PARAM_READWRITE));

  /**
   * GtkAboutDialog:website:
   *
   * The URL for the link to the website of the program. 
   * This should be a string starting with "http://.
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_WEBSITE,
				   g_param_spec_string ("website",
							P_("Website URL"),
							P_("The URL for the link to the website of the program"),
							NULL,
							G_PARAM_READWRITE));

  /**
   * GtkAboutDialog:website-label:
   *
   * The label for the link to the website of the program. If this is not set, 
   * it defaults to the URL specified in the 
   * <link linkend="GtkAboutDialog--website">website</link> property.
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_WEBSITE_LABEL,
				   g_param_spec_string ("website-label",
							P_("Website label"),
							P_("The label for the link to the website of the program. If this is not set, it defaults to the URL"),
							NULL,
							G_PARAM_READWRITE));

  /**
   * GtkAboutDialog:authors:
   *
   * The authors of the program, as a %NULL-terminated array of strings.
   * Each string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_AUTHORS,
				   g_param_spec_boxed ("authors",
						       P_("Authors"),
						       P_("List of authors of the program"),
						       G_TYPE_STRV,
						       G_PARAM_READWRITE));

  /**
   * GtkAboutDialog:documenters:
   *
   * The people documenting the program, as a %NULL-terminated array of strings.
   * Each string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_DOCUMENTERS,
				   g_param_spec_boxed ("documenters",
						       P_("Documenters"),
						       P_("List of people documenting the program"),
						       G_TYPE_STRV,
						       G_PARAM_READWRITE));

  /**
   * GtkAboutDialog:artists:
   *
   * The people who contributed artwork to the program, as a %NULL-terminated array of strings.
   * Each string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_ARTISTS,
				   g_param_spec_boxed ("artists",
						       P_("Artists"),
						       P_("List of people who have contributed artwork to the program"),
						       G_TYPE_STRV,
						       G_PARAM_READWRITE));


  /**
   * GtkAboutDialog:translator-credits:
   *
   * Credits to the translators. This string should be marked as translatable.
   * The string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_TRANSLATOR_CREDITS,
				   g_param_spec_string ("translator-credits",
							P_("Translator credits"),
							P_("Credits to the translators. This string should be marked as translatable"),
							NULL,
							G_PARAM_READWRITE));
	
  /**
   * GtkAboutDialog:logo:
   *
   * A logo for the about box. If this is not set, it defaults to 
   * gtk_window_get_default_icon_list().
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_LOGO,
				   g_param_spec_object ("logo",
							P_("Logo"),
							P_("A logo for the about box. If this is not set, it defaults to gtk_window_get_default_icon_list()"),
							GDK_TYPE_PIXBUF,
							G_PARAM_READWRITE));

  /**
   * GtkAboutDialog:logo-icon-name:
   *
   * A named icon to use as the logo for the about box. This property
   * overrides the <link linkend="GtkAboutDialog--logo">logo</link> property.
   *
   * Since: 2.6
   */  
  g_object_class_install_property (object_class,
				   PROP_LOGO_ICON_NAME,
				   g_param_spec_string ("logo-icon-name",
							P_("Logo Icon Name"),
							P_("A named icon to use as the logo for the about box."),
							NULL,
							G_PARAM_READWRITE));

  /* Style properties */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("link-color",
                                                               P_("Link Color"),
                                                               P_("Color of hyperlinks"),
                                                               GDK_TYPE_COLOR,
                                                               G_PARAM_READABLE));

  g_type_class_add_private (object_class, sizeof (GtkAboutDialogPrivate));
}

static void
gtk_about_dialog_init (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  GtkWidget *vbox, *hbox, *button;

  /* Data */
  priv = GTK_ABOUT_DIALOG_GET_PRIVATE (about);
  about->private_data = priv;

  priv->name = NULL;
  priv->version = NULL;
  priv->copyright = NULL;
  priv->comments = NULL;
  priv->website = NULL;
  priv->website_label = NULL;
  priv->translator_credits = NULL;
  priv->license = NULL;
  priv->authors = NULL;
  priv->documenters = NULL;
  priv->artists = NULL;

  priv->hand_cursor = gdk_cursor_new (GDK_HAND2);
  priv->regular_cursor = gdk_cursor_new (GDK_XTERM);
  priv->hovering_over_link = FALSE;

  gtk_dialog_set_has_separator (GTK_DIALOG (about), FALSE);
  
  /* Widgets */
  gtk_widget_push_composite_child ();
  vbox = gtk_vbox_new (FALSE, 8);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about)->vbox), vbox, TRUE, TRUE, 0);

  priv->logo_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (vbox), priv->logo_image, FALSE, FALSE, 0);

  priv->name_label = gtk_label_new (NULL);
  gtk_label_set_selectable (GTK_LABEL (priv->name_label), TRUE);
  gtk_label_set_justify (GTK_LABEL (priv->name_label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (vbox), priv->name_label, FALSE, FALSE, 0);

  priv->comments_label = gtk_label_new (NULL);
  gtk_label_set_selectable (GTK_LABEL (priv->comments_label), TRUE);
  gtk_label_set_justify (GTK_LABEL (priv->comments_label), GTK_JUSTIFY_CENTER);
  gtk_label_set_line_wrap (GTK_LABEL (priv->comments_label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), priv->comments_label, FALSE, FALSE, 0);

  priv->copyright_label = gtk_label_new (NULL);
  gtk_label_set_selectable (GTK_LABEL (priv->copyright_label), TRUE);	
  gtk_label_set_justify (GTK_LABEL (priv->copyright_label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (vbox), priv->copyright_label, FALSE, FALSE, 0);

  button = create_link_button (GTK_WIDGET (about), "", "", 
			       G_CALLBACK (activate_url), about);

  hbox = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0); 
  priv->website_button = button;
  
  gtk_widget_show (vbox);
  gtk_widget_show (priv->logo_image);
  gtk_widget_show (priv->name_label);
  gtk_widget_show (hbox);

  /* Add the OK button */
  gtk_dialog_add_button (GTK_DIALOG (about), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (about), GTK_RESPONSE_CLOSE);

  /* Add the credits button */
  button = gtk_button_new_from_stock (_("C_redits"));
  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (about)->action_area), 
		    button, FALSE, TRUE, 0); 
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (GTK_DIALOG (about)->action_area), button, TRUE);
  g_signal_connect (button, "clicked", G_CALLBACK (display_credits_dialog), about);
  priv->credits_button = button;
  priv->credits_dialog = NULL;

  /* Add the license button */
  button = gtk_button_new_from_stock (_("_License"));
  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (about)->action_area), 
		    button, FALSE, TRUE, 0); 
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (GTK_DIALOG (about)->action_area), button, TRUE);
  g_signal_connect (button, "clicked", G_CALLBACK (display_license_dialog), about);
  priv->license_button = button;
  priv->license_dialog = NULL;

  gtk_window_set_resizable (GTK_WINDOW (about), FALSE);

  gtk_widget_pop_composite_child ();

  /* force defaults */
  gtk_about_dialog_set_name (about, NULL);
  gtk_about_dialog_set_logo (about, NULL);

  /* Close dialog on user response */
  g_signal_connect (about, "response", G_CALLBACK (close_cb), NULL);
}

static void
gtk_about_dialog_finalize (GObject *object)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;

  g_free (priv->name);
  g_free (priv->version);
  g_free (priv->copyright);
  g_free (priv->comments);
  g_free (priv->license);
  g_free (priv->website);
  g_free (priv->website_label);
  g_free (priv->translator_credits);

  g_strfreev (priv->authors);
  g_strfreev (priv->documenters);
  g_strfreev (priv->artists);

  gdk_cursor_unref (priv->hand_cursor);
  gdk_cursor_unref (priv->regular_cursor);

  G_OBJECT_CLASS (gtk_about_dialog_parent_class)->finalize (object);
}

static void
gtk_about_dialog_set_property (GObject      *object, 
			       guint         prop_id, 
			       const GValue *value, 
			       GParamSpec   *pspec)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);

  switch (prop_id) 
    {
    case PROP_NAME:
      gtk_about_dialog_set_name (about, g_value_get_string (value));
      break;
    case PROP_VERSION:
      gtk_about_dialog_set_version (about, g_value_get_string (value));
      break;
    case PROP_COMMENTS:
      gtk_about_dialog_set_comments (about, g_value_get_string (value));
      break;
    case PROP_WEBSITE:
      gtk_about_dialog_set_website (about, g_value_get_string (value));
      break;
    case PROP_WEBSITE_LABEL:
      gtk_about_dialog_set_website_label (about, g_value_get_string (value));
      break;
    case PROP_LICENSE:
      gtk_about_dialog_set_license (about, g_value_get_string (value));
      break;
    case PROP_COPYRIGHT:
      gtk_about_dialog_set_copyright (about, g_value_get_string (value));
      break;
    case PROP_LOGO:
      gtk_about_dialog_set_logo (about, g_value_get_object (value));
      break; 
    case PROP_AUTHORS:
      gtk_about_dialog_set_authors (about, (const gchar**)g_value_get_boxed (value));
      break;
    case PROP_DOCUMENTERS:
      gtk_about_dialog_set_documenters (about, (const gchar**)g_value_get_boxed (value));
      break;	
    case PROP_ARTISTS:
      gtk_about_dialog_set_artists (about, (const gchar**)g_value_get_boxed (value));
      break;	
    case PROP_TRANSLATOR_CREDITS:
      gtk_about_dialog_set_translator_credits (about, g_value_get_string (value));
      break;
    case PROP_LOGO_ICON_NAME:
      gtk_about_dialog_set_logo_icon_name (about, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_about_dialog_get_property (GObject    *object, 
			       guint       prop_id, 
			       GValue     *value, 
			       GParamSpec *pspec)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;
	
  switch (prop_id) 
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_VERSION:
      g_value_set_string (value, priv->version);
      break;
    case PROP_COPYRIGHT:
      g_value_set_string (value, priv->copyright);
      break;
    case PROP_COMMENTS:
      g_value_set_string (value, priv->comments);
      break;
    case PROP_WEBSITE:
      g_value_set_string (value, priv->website);
      break;
    case PROP_WEBSITE_LABEL:
      g_value_set_string (value, priv->website_label);
      break;
    case PROP_LICENSE:
      g_value_set_string (value, priv->license);
      break;
    case PROP_TRANSLATOR_CREDITS:
      g_value_set_string (value, priv->translator_credits);
      break;
    case PROP_AUTHORS:
      g_value_set_boxed (value, priv->authors);
      break;
    case PROP_DOCUMENTERS:
      g_value_set_boxed (value, priv->documenters);
      break;
    case PROP_ARTISTS:
      g_value_set_boxed (value, priv->artists);
      break;
    case PROP_LOGO:
      if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_PIXBUF)
	g_value_set_object (value, gtk_image_get_pixbuf (GTK_IMAGE (priv->logo_image)));
      else
	g_value_set_object (value, NULL);
      break;
    case PROP_LOGO_ICON_NAME:
      if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_ICON_NAME)
	{
	  const gchar *icon_name;

	  gtk_image_get_icon_name (GTK_IMAGE (priv->logo_image), &icon_name, NULL);
	  g_value_set_string (value, icon_name);
	}
      else
	g_value_set_string (value, NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
dialog_style_set (GtkWidget *widget,
		  GtkStyle *previous_style,
		  gpointer data)
{
  GtkDialog *dialog;

  dialog = GTK_DIALOG (widget);

  /* Override the style properties with HIG-compliant spacings.  Ugh.
   * http://developer.gnome.org/projects/gup/hig/1.0/layout.html#layout-dialogs
   * http://developer.gnome.org/projects/gup/hig/1.0/windows.html#alert-spacing
   */

  gtk_container_set_border_width (GTK_CONTAINER (dialog->vbox), 12);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 12);

  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 0);
  gtk_box_set_spacing (GTK_BOX (dialog->action_area), 6);
}

static void
gtk_about_dialog_style_set (GtkWidget *widget,
			    GtkStyle  *previous_style)
{
  if (GTK_WIDGET_CLASS (gtk_about_dialog_parent_class)->style_set)
    GTK_WIDGET_CLASS (gtk_about_dialog_parent_class)->style_set (widget, previous_style);

  dialog_style_set (widget, previous_style, NULL);
}

/**
 * gtk_about_dialog_get_name:
 * @about: a #GtkAboutDialog
 * 
 * Returns the program name displayed in the about dialog.
 * 
 * Return value: The program name. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_about_dialog_get_name (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->name;
}

static void
update_name_version (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  gchar *title_string, *name_string;

  priv = (GtkAboutDialogPrivate *)about->private_data;

  title_string = g_strdup_printf (_("About %s"), priv->name);
  gtk_window_set_title (GTK_WINDOW (about), title_string);
  g_free (title_string);

  if (priv->version != NULL) 
    name_string = g_markup_printf_escaped ("<span size=\"xx-large\" weight=\"bold\">%s %s</span>", 
					     priv->name, priv->version);
  else
    name_string = g_markup_printf_escaped ("<span size=\"xx-large\" weight=\"bold\">%s</span>", 
					   priv->name);

  gtk_label_set_markup (GTK_LABEL (priv->name_label), name_string);

  g_free (name_string);
}

/**
 * gtk_about_dialog_set_name:
 * @about: a #GtkAboutDialog
 * @name: the program name
 *
 * Sets the name to display in the about dialog. 
 * If this is not set, it defaults to g_get_application_name().
 * 
 * Since: 2.6
 **/
void
gtk_about_dialog_set_name (GtkAboutDialog *about, 
			   const gchar    *name)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));
	
  priv = (GtkAboutDialogPrivate *)about->private_data;
  tmp = priv->name;
  priv->name = g_strdup (name ? name : g_get_application_name ());
  g_free (tmp);

  update_name_version (about);

  g_object_notify (G_OBJECT (about), "name");
}

/**
 * gtk_about_dialog_get_version:
 * @about: a #GtkAboutDialog
 * 
 * Returns the version string.
 * 
 * Return value: The version string. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_about_dialog_get_version (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->version;
}

/**
 * gtk_about_dialog_set_version:
 * @about: a #GtkAboutDialog
 * @version: the version string 
 *
 * Sets the version string to display in the about dialog.
 * 
 * Since: 2.6
 **/
void
gtk_about_dialog_set_version (GtkAboutDialog *about, 
			      const gchar    *version)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;
  
  tmp = priv->version;
  priv->version = version ? g_strdup (version) : NULL;
  g_free (tmp);

  update_name_version (about);

  g_object_notify (G_OBJECT (about), "version");
}

/**
 * gtk_about_dialog_get_copyright:
 * @about: a #GtkAboutDialog
 * 
 * Returns the copyright string.
 * 
 * Return value: The copyright string. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_about_dialog_get_copyright (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->copyright;
}

/**
 * gtk_about_dialog_set_copyright:
 * @about: a #GtkAboutDialog
 * @copyright: the copyright string
 * 
 * Sets the copyright string to display in the about dialog.
 * This should be a short string of one or two lines. 
 *
 * Since: 2.6
 **/
void
gtk_about_dialog_set_copyright (GtkAboutDialog *about, 
				const gchar    *copyright)
{
  GtkAboutDialogPrivate *priv;
  gchar *copyright_string, *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;
	
  tmp = priv->copyright;
  priv->copyright = copyright ? g_strdup (copyright) : NULL;
  g_free (tmp);
  
  if (priv->copyright != NULL) 
    {
      copyright_string = g_markup_printf_escaped ("<span size=\"small\">%s</span>", 
						  priv->copyright);
      gtk_label_set_markup (GTK_LABEL (priv->copyright_label), copyright_string);
      g_free (copyright_string);
  
      gtk_widget_show (priv->copyright_label);
    }
  else 
    gtk_widget_hide (priv->copyright_label);
  
  g_object_notify (G_OBJECT (about), "copyright");
}

/**
 * gtk_about_dialog_get_comments:
 * @about: a #GtkAboutDialog
 * 
 * Returns the comments string.
 * 
 * Return value: The comments. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_about_dialog_get_comments (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->comments;
}

/**
 * gtk_about_dialog_set_comments:
 * @about: a #GtkAboutDialog
 * @comments: a comments string
 * 
 * Sets the comments string to display in the about 
 * dialog. This should be a short string of one or
 * two lines.
 *
 * Since: 2.6
 **/
void
gtk_about_dialog_set_comments (GtkAboutDialog *about, 
			       const gchar    *comments)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;
  
  tmp = priv->comments;
  if (comments) 
    {
      priv->comments = g_strdup (comments);
      gtk_label_set_text (GTK_LABEL (priv->comments_label), priv->comments);
      gtk_widget_show (priv->comments_label);
    }
  else
    {
      priv->comments = NULL;
      gtk_widget_hide (priv->comments_label);
    }
  g_free (tmp);

  g_object_notify (G_OBJECT (about), "comments");
}

/**
 * gtk_about_dialog_get_license:
 * @about: a #GtkAboutDialog
 * 
 * Returns the license information.
 * 
 * Return value: The license information. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_about_dialog_get_license (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->license;
}

/**
 * gtk_about_dialog_set_license:
 * @about: a #GtkAboutDialog
 * @license: the license information or %NULL
 *
 * Sets the license information to be displayed in the secondary
 * license dialog. If @license is %NULL, the license button is
 * hidden.
 * 
 * Since: 2.6
 **/
void
gtk_about_dialog_set_license (GtkAboutDialog *about, 
			      const gchar    *license)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->license;
  if (license) 
    {
      priv->license = g_strdup (license);
      gtk_widget_show (priv->license_button);
    }
  else
    {
      priv->license = NULL;
      gtk_widget_hide (priv->license_button);
    }
  g_free (tmp);

  g_object_notify (G_OBJECT (about), "license");
}

/**
 * gtk_about_dialog_get_website:
 * @about: a #GtkAboutDialog
 * 
 * Returns the website URL.
 * 
 * Return value: The website URL. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_about_dialog_get_website (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->website;
}

/**
 * gtk_about_dialog_set_website:
 * @about: a #GtkAboutDialog
 * @website: a URL string starting with "http://"
 * 
 * Sets the URL to use for the website link.
 *
 * Since: 2.6
 **/
void
gtk_about_dialog_set_website (GtkAboutDialog *about, 
			      const gchar    *website)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;
  
  tmp = priv->website;
  if (website != NULL)
    {
      priv->website = g_strdup (website);
      if (activate_url_hook != NULL)
	{
	  g_object_set_data_full (G_OBJECT (priv->website_button), 
				  "url", g_strdup (website), g_free);  
	  if (priv->website_label == NULL) 
	    gtk_about_dialog_set_website_label (about, website);
	}
      else 
	{
	  GtkWidget *hbox = priv->website_button->parent;
	  gtk_widget_destroy (priv->website_button);
	  priv->website_button = gtk_label_new (website);
	  gtk_label_set_selectable (GTK_LABEL (priv->website_button), TRUE);
	  gtk_container_add (GTK_CONTAINER (hbox), priv->website_button);
	  gtk_widget_show (priv->website_button);
	}
    }
  else 
    {
      priv->website = NULL;
      g_object_set_data (G_OBJECT (priv->website_button), "url", NULL);
      gtk_widget_hide (priv->website_button);
    }
  g_free (tmp);

  g_object_notify (G_OBJECT (about), "website");
}

/**
 * gtk_about_dialog_get_website_label:
 * @about: a #GtkAboutDialog
 * 
 * Returns the label used for the website link. 
 * 
 * Return value: The label used for the website link. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_about_dialog_get_website_label (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->website_label;
}

/**
 * gtk_about_dialog_set_website_label:
 * @about: a #GtkAboutDialog
 * @website_label: the label used for the website link
 * 
 * Sets the label to be used for the website link.
 * It defaults to the website URL.
 *
 * Since: 2.6
 **/
void
gtk_about_dialog_set_website_label (GtkAboutDialog *about, 
				    const gchar    *website_label)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->website_label;
  if (activate_url_hook != NULL)
    {
      if (website_label != NULL) 
	{
	  priv->website_label = g_strdup (website_label);
	  set_link_button_text (GTK_WIDGET (about),
				priv->website_button, 
				priv->website_label);
	  gtk_widget_show (priv->website_button);
	}
      else 
	{
	  priv->website_label = NULL;
	  gtk_widget_hide (priv->website_button);
	}
    }
  g_free (tmp);

  g_object_notify (G_OBJECT (about), "website_label");
}

/**
 * gtk_about_dialog_get_authors:
 * @about: a #GtkAboutDialog
 * 
 * Returns the string which are displayed in the authors tab
 * of the secondary credits dialog.
 * 
 * Return value: A %NULL-terminated string array containing
 *  the authors. The array is owned by the about dialog 
 *  and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar * G_CONST_RETURN *
gtk_about_dialog_get_authors (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return (const gchar * const *) priv->authors;
}

/**
 * gtk_about_dialog_set_authors:
 * @about: a #GtkAboutDialog
 * @authors: a %NULL-terminated array of strings 
 *
 * Sets the strings which are displayed in the authors tab
 * of the secondary credits dialog. 
 * 
 * Since: 2.6
 **/
void
gtk_about_dialog_set_authors (GtkAboutDialog  *about, 
			      const gchar    **authors)
{
  GtkAboutDialogPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->authors;
  priv->authors = g_strdupv ((gchar **)authors);
  g_strfreev (tmp);

  if (priv->authors != NULL)
    gtk_widget_show (priv->credits_button);

  g_object_notify (G_OBJECT (about), "authors");
}

/**
 * gtk_about_dialog_get_documenters:
 * @about: a #GtkAboutDialog
 * 
 * Returns the string which are displayed in the documenters 
 * tab of the secondary credits dialog.
 * 
 * Return value: A %NULL-terminated string array containing
 *  the documenters. The array is owned by the about dialog 
 *  and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar * G_CONST_RETURN *
gtk_about_dialog_get_documenters (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return (const gchar * const *)priv->documenters;
}

/**
 * gtk_about_dialog_set_documenters:
 * @about: a #GtkAboutDialog
 * @documenters: a %NULL-terminated array of strings 
 *
 * Sets the strings which are displayed in the documenters tab
 * of the secondary credits dialog. 
 * 
 * Since: 2.6
 **/
void
gtk_about_dialog_set_documenters (GtkAboutDialog *about, 
				  const gchar   **documenters)
{
  GtkAboutDialogPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;
  
  tmp = priv->documenters;
  priv->documenters = g_strdupv ((gchar **)documenters);
  g_strfreev (tmp);

  if (priv->documenters != NULL)
    gtk_widget_show (priv->credits_button);

  g_object_notify (G_OBJECT (about), "documenters");
}

/**
 * gtk_about_dialog_get_artists:
 * @about: a #GtkAboutDialog
 * 
 * Returns the string which are displayed in the artists tab
 * of the secondary credits dialog.
 * 
 * Return value: A %NULL-terminated string array containing
 *  the artists. The array is owned by the about dialog 
 *  and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar * G_CONST_RETURN *
gtk_about_dialog_get_artists (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return (const gchar * const *)priv->artists;
}

/**
 * gtk_about_dialog_set_artists:
 * @about: a #GtkAboutDialog
 * @artists: a %NULL-terminated array of strings 
 *
 * Sets the strings which are displayed in the artists tab
 * of the secondary credits dialog. 
 * 
 * Since: 2.6
 **/
void
gtk_about_dialog_set_artists (GtkAboutDialog *about, 
			      const gchar   **artists)
{
  GtkAboutDialogPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;
  
  tmp = priv->artists;
  priv->artists = g_strdupv ((gchar **)artists);
  g_strfreev (tmp);

  if (priv->artists != NULL)
    gtk_widget_show (priv->credits_button);

  g_object_notify (G_OBJECT (about), "artists");
}

/**
 * gtk_about_dialog_get_translator_credits:
 * @about: a #GtkAboutDialog
 * 
 * Returns the translator credits string which is displayed
 * in the translators tab of the secondary credits dialog.
 * 
 * Return value: The translator credits string. The string is
 *   owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_about_dialog_get_translator_credits (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->translator_credits;
}

/**
 * gtk_about_dialog_set_translator_credits:
 * @about: a #GtkAboutDialog
 * @translator_credits: the translator credits
 * 
 * Sets the translator credits string which is displayed in
 * the translators tab of the secondary credits dialog.
 * 
 * The intended use for this string is to display the translator
 * of the language which is currently used in the user interface.
 * Using gettext(), a simple way to achieve that is to mark the
 * string for translation:
 * <informalexample><programlisting>
 *  gtk_about_dialog_set_translator_credits (about, _("translator-credits"));
 * </programlisting></informalexample>
 * It is a good idea to use the customary msgid "translator-credits" for this
 * purpose, since translators will already know the purpose of that msgid, and
 * since #GtkAboutDialog will detect if "translator-credits" is untranslated
 * and hide the tab.
 *
 * Since: 2.6
 **/
void
gtk_about_dialog_set_translator_credits (GtkAboutDialog *about, 
					 const gchar    *translator_credits)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->translator_credits;
  priv->translator_credits = g_strdup (translator_credits);
  g_free (tmp);

  if (priv->translator_credits != NULL)
    gtk_widget_show (priv->credits_button);

  g_object_notify (G_OBJECT (about), "translator-credits");
}

/**
 * gtk_about_dialog_get_logo:
 * @about: a #GtkAboutDialog
 * 
 * Returns the pixbuf displayed as logo in the about dialog.
 * 
 * Return value: the pixbuf displayed as logo. The pixbuf is
 *   owned by the about dialog. If you want to keep a reference
 *   to it, you have to call g_object_ref() on it.
 *
 * Since: 2.6
 **/
GdkPixbuf *
gtk_about_dialog_get_logo (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_PIXBUF)
    return gtk_image_get_pixbuf (GTK_IMAGE (priv->logo_image));
  else
    return NULL;
}

static GtkIconSet *
icon_set_new_from_pixbufs (GList *pixbufs)
{
  GtkIconSet *icon_set = gtk_icon_set_new ();
  
  for (; pixbufs; pixbufs = pixbufs->next)
    {
      GdkPixbuf *pixbuf = GDK_PIXBUF (pixbufs->data);

      GtkIconSource *icon_source = gtk_icon_source_new ();
      gtk_icon_source_set_pixbuf (icon_source, pixbuf);
      gtk_icon_set_add_source (icon_set, icon_source);
      gtk_icon_source_free (icon_source);
    }
  
  return icon_set;
}

/**
 * gtk_about_dialog_set_logo:
 * @about: a #GtkAboutDialog
 * @logo: a #GdkPixbuf, or %NULL
 * 
 * Sets the pixbuf to be displayed as logo in 
 * the about dialog. If it is %NULL, the default
 * window icon set with gtk_window_set_default_icon ()
 * will be used.
 *
 * Since: 2.6
 **/
void
gtk_about_dialog_set_logo (GtkAboutDialog *about,
			   GdkPixbuf      *logo)
{
  GtkAboutDialogPrivate *priv;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  g_object_freeze_notify (G_OBJECT (about));

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_ICON_NAME)
    g_object_notify (G_OBJECT (about), "logo_icon_name");

  if (logo != NULL) 
    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->logo_image), logo);
  else 
    {
      GList *pixbufs = gtk_window_get_default_icon_list ();

      if (pixbufs != NULL)
	{
	  GtkIconSet *icon_set = icon_set_new_from_pixbufs (pixbufs); 
	  
	  gtk_image_set_from_icon_set (GTK_IMAGE (priv->logo_image),
				       icon_set, GTK_ICON_SIZE_DIALOG);
	  
	  gtk_icon_set_unref (icon_set);
	  g_list_free (pixbufs);
	}
    }

  g_object_notify (G_OBJECT (about), "logo");

  g_object_thaw_notify (G_OBJECT (about));
}

/**
 * gtk_about_dialog_get_logo_icon_name:
 * @about: a #GtkAboutDialog
 * 
 * Returns the icon name displayed as logo in the about dialog.
 * 
 * Return value: the icon name displayed as logo. The string is
 *   owned by the dialog. If you want to keep a reference
 *   to it, you have to call g_strdup() on it.
 *
 * Since: 2.6
 **/
G_CONST_RETURN gchar *
gtk_about_dialog_get_logo_icon_name (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  const gchar *icon_name = NULL;
  
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_ICON_NAME)
    gtk_image_get_icon_name (GTK_IMAGE (priv->logo_image), &icon_name, NULL);

  return icon_name;
}

/**
 * gtk_about_dialog_set_logo_icon_name:
 * @about: a #GtkAboutDialog
 * @icon_name: an icon name, or %NULL
 * 
 * Sets the pixbuf to be displayed as logo in 
 * the about dialog. If it is %NULL, the default
 * window icon set with gtk_window_set_default_icon()
 * will be used.
 *
 * Since: 2.6
 **/
void
gtk_about_dialog_set_logo_icon_name (GtkAboutDialog *about,
				     const gchar    *icon_name)
{
  GtkAboutDialogPrivate *priv;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  g_object_freeze_notify (G_OBJECT (about));

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_PIXBUF)
    g_object_notify (G_OBJECT (about), "logo");

  gtk_image_set_from_icon_name (GTK_IMAGE (priv->logo_image), icon_name,
				GTK_ICON_SIZE_DIALOG);
  g_object_notify (G_OBJECT (about), "logo_icon_name");

  g_object_thaw_notify (G_OBJECT (about));
}

static void
activate_url (GtkWidget *widget, 
	      gpointer   data)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (data);
  gchar *url = g_object_get_data (G_OBJECT (widget), "url");
  
  if (activate_url_hook != NULL)
    (* activate_url_hook) (about, url, activate_url_hook_data);
}

static void
set_link_button_text (GtkWidget *about,
		      GtkWidget *button, 
		      gchar     *text)
{
  GtkWidget *label;
  gchar *link;
  GdkColor *style_link_color;
  GdkColor link_color = { 0, 0, 0, 0xffff };

  gtk_widget_ensure_style (about);
  gtk_widget_style_get (about, "link_color", &style_link_color, NULL);
  if (style_link_color)
    {
      link_color = *style_link_color;
      gdk_color_free (style_link_color);
    }

  link = g_markup_printf_escaped ("<span foreground=\"#%04x%04x%04x\" underline=\"single\">%s</span>", 
				  link_color.red, link_color.green, link_color.blue, text);

  label = gtk_bin_get_child (GTK_BIN (button));  
  gtk_label_set_markup (GTK_LABEL (label), link);
  g_free (link);
}

static gboolean
link_button_enter (GtkWidget        *widget,
		   GdkEventCrossing *event,
		   GtkAboutDialog   *about)
{
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;
  gdk_window_set_cursor (widget->window, priv->hand_cursor);

  return FALSE;
}

static gboolean
link_button_leave (GtkWidget        *widget,
		   GdkEventCrossing *event,
		   GtkAboutDialog   *about)
{
  gdk_window_set_cursor (widget->window, NULL);

  return FALSE;
}

static GtkWidget *
create_link_button (GtkWidget *about,
		    gchar     *text,
		    gchar     *url, 
		    GCallback  callback, 
		    gpointer   data)
{
  GtkWidget *button;

  button = gtk_button_new_with_label ("");
  GTK_WIDGET_UNSET_FLAGS (button, GTK_RECEIVES_DEFAULT);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  g_object_set_data_full (G_OBJECT (button), "url", g_strdup (url), g_free);
  set_link_button_text (about, button, text);
  
  g_signal_connect (button, "clicked", callback, data);
  g_signal_connect (button, "enter_notify_event",
		    G_CALLBACK (link_button_enter), data);
  g_signal_connect (button, "leave_notify_event",
		    G_CALLBACK (link_button_leave), data);

  return button;
}

static void
follow_if_link (GtkAboutDialog *about,
		GtkTextIter    *iter)
{
  GSList *tags = NULL, *tagp = NULL;

  tags = gtk_text_iter_get_tags (iter);
  for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;
      gchar *email = g_object_get_data (G_OBJECT (tag), "email");
      gchar *url = g_object_get_data (G_OBJECT (tag), "url");

      if (email != NULL && activate_email_hook != NULL)
        {
	  (* activate_email_hook) (about, email, activate_email_hook_data);
	  break;
        }

      if (url != NULL && activate_url_hook != NULL)
        {
	  (* activate_url_hook) (about, url, activate_url_hook_data);
	  break;
        }
    }

  if (tags) 
    g_slist_free (tags);
}

static gboolean
credits_key_press_event (GtkWidget      *text_view,
			 GdkEventKey    *event,
			 GtkAboutDialog *about)
{
  GtkTextIter iter;
  GtkTextBuffer *buffer;

  switch (event->keyval)
    {
      case GDK_Return: 
      case GDK_KP_Enter:
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
        gtk_text_buffer_get_iter_at_mark (buffer, &iter, 
                                          gtk_text_buffer_get_insert (buffer));
        follow_if_link (about, &iter);
        break;

      default:
        break;
    }

  return FALSE;
}

static gboolean
credits_event_after (GtkWidget      *text_view,
		     GdkEvent       *event,
		     GtkAboutDialog *about)
{
  GtkTextIter start, end, iter;
  GtkTextBuffer *buffer;
  GdkEventButton *button_event;
  gint x, y;

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  button_event = (GdkEventButton *)event;

  if (button_event->button != 1)
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  /* we shouldn't follow a link if the user has selected something */
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
    return FALSE;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
                                         GTK_TEXT_WINDOW_WIDGET,
                                         button_event->x, button_event->y, &x, &y);

  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, x, y);

  follow_if_link (about, &iter);

  return FALSE;
}

static void
set_cursor_if_appropriate (GtkAboutDialog *about,
			   GtkTextView    *text_view,
			   gint            x,
			   gint            y)
{
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;
  GSList *tags = NULL, *tagp = NULL;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  gboolean hovering_over_link = FALSE;

  buffer = gtk_text_view_get_buffer (text_view);

  gtk_text_view_get_iter_at_location (text_view, &iter, x, y);
  
  tags = gtk_text_iter_get_tags (&iter);
  for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;
      gchar *email = g_object_get_data (G_OBJECT (tag), "email");
      gchar *url = g_object_get_data (G_OBJECT (tag), "url");

      if (email != NULL || url != NULL) 
        {
          hovering_over_link = TRUE;
          break;
        }
    }

  if (hovering_over_link != priv->hovering_over_link)
    {
      priv->hovering_over_link = hovering_over_link;

      if (hovering_over_link)
        gdk_window_set_cursor (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT), priv->hand_cursor);
      else
        gdk_window_set_cursor (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT), priv->regular_cursor);
    }

  if (tags) 
    g_slist_free (tags);
}

static gboolean
credits_motion_notify_event (GtkWidget *text_view,
			     GdkEventMotion *event,
			     GtkAboutDialog *about)
{
  gint x, y;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
                                         GTK_TEXT_WINDOW_WIDGET,
                                         event->x, event->y, &x, &y);

  set_cursor_if_appropriate (about, GTK_TEXT_VIEW (text_view), x, y);

  gdk_window_get_pointer (text_view->window, NULL, NULL, NULL);

  return FALSE;
}


static gboolean
credits_visibility_notify_event (GtkWidget          *text_view,
				 GdkEventVisibility *event,
				 GtkAboutDialog     *about)
{
  gint wx, wy, bx, by;

  gdk_window_get_pointer (text_view->window, &wx, &wy, NULL);

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view), 
                                         GTK_TEXT_WINDOW_WIDGET,
                                         wx, wy, &bx, &by);

  set_cursor_if_appropriate (about, GTK_TEXT_VIEW (text_view), bx, by);

  return FALSE;
}

static void
text_view_style_set (GtkWidget *widget, GtkStyle *prev_style, GtkWidget *text_view)
{
  gtk_widget_modify_base (text_view, GTK_STATE_NORMAL,
			  &widget->style->bg[GTK_STATE_NORMAL]);
}

static void
add_credits_page (GtkAboutDialog *about, 
		  GtkWidget      *notebook,
		  gchar          *title,
		  gchar         **people)
{
  gchar **p;
  gchar *q0, *q1, *q2, *r1, *r2;
  GtkWidget *sw, *view;
  GtkTextBuffer *buffer;
  gboolean linkify_email, linkify_urls;
  GdkColor *style_link_color;
  GdkColor link_color = { 0, 0, 0, 0xffff };

  linkify_email = (activate_email_hook != NULL);
  linkify_urls = (activate_url_hook != NULL);

  gtk_widget_ensure_style (GTK_WIDGET (about));
  gtk_widget_style_get (GTK_WIDGET (about), "link_color", &style_link_color, NULL);
  if (style_link_color)
    {
      link_color = *style_link_color;
      gdk_color_free (style_link_color);
    }

  view = gtk_text_view_new ();
  g_signal_connect_object (about, "style_set",
			   G_CALLBACK (text_view_style_set), view);
  
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);

  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 8);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 8);

  g_signal_connect (view, "key-press-event",
                    G_CALLBACK (credits_key_press_event), about);
  g_signal_connect (view, "event-after",
                    G_CALLBACK (credits_event_after), about);
  g_signal_connect (view, "motion-notify-event", 
                    G_CALLBACK (credits_motion_notify_event), about);
  g_signal_connect (view, "visibility-notify-event", 
                    G_CALLBACK (credits_visibility_notify_event), about);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
  				       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (sw), view);
  
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), 
			    sw, gtk_label_new (title));

  if (people == NULL) 
    {
      gtk_widget_hide (view);
      return;
    }
  else 
    gtk_widget_show (view);
  
  for (p = people; *p; p++) 
    {
      q0  = *p;
      while (*q0) 
	{
	  q1 = linkify_email ? strchr (q0, '<') : NULL;
	  q2 = q1 ? strchr (q1, '>') : NULL;
	  r1 = linkify_urls ? strstr (q0, "http://") : NULL;
          if (r1)
            {
              r2 = strpbrk (r1, " \n\t");
	      if (!r2)
		r2 = strchr (r1, '\0');
	    }
          else  
            r2 = NULL;

	  if (r1 && r2 && (!q1 || !q2 || (r1 < q1))) 
	    {
	      q1 = r1;
	      q2 = r2;
	    }

	  if (q1 && q2) 
	    {
	      GtkTextIter end;
	      gchar *link;
	      gchar *link_type;
	      GtkTextTag *tag;
	      
	      gtk_text_buffer_insert_at_cursor (buffer, q0, q1 - q0);
	      gtk_text_buffer_get_end_iter (buffer, &end);

	      q0 = q2;

	      if (*q1 == '<') 
		{
		  q1++;
		  q0++;
		  link_type = "email";
		}
	      else 
		link_type = "url";
	      
	      link = g_strndup (q1, q2 - q1);
	      tag = gtk_text_buffer_create_tag (buffer, NULL, 
						"foreground_gdk", &link_color, 
						"underline", PANGO_UNDERLINE_SINGLE, 
						NULL);
	      g_object_set_data_full (G_OBJECT (tag), link_type, g_strdup (link), g_free);
	      gtk_text_buffer_insert_with_tags (buffer, &end, link, -1, tag, NULL);

	      g_free (link);
	    }
	  else
	    {
	      gtk_text_buffer_insert_at_cursor (buffer, q0, -1);
	      break;
	    }
	}
      
      if (p[1])
	gtk_text_buffer_insert_at_cursor (buffer, "\n", 1);
    }
}

static void
display_credits_dialog (GtkWidget *button, 
			gpointer   data)
{
  GtkAboutDialog *about = (GtkAboutDialog *)data;
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;
  GtkWidget *dialog, *notebook;

  if (priv->credits_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->credits_dialog));
      return;
    }
	
  dialog = gtk_dialog_new_with_buttons (_("Credits"),
					GTK_WINDOW (about),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					NULL);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  
  priv->credits_dialog = dialog;
  gtk_window_set_default_size (GTK_WINDOW (dialog), 360, 260);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

  gtk_window_set_modal (GTK_WINDOW (dialog), 
			gtk_window_get_modal (GTK_WINDOW (about)));

  g_signal_connect (dialog, "response",
		    G_CALLBACK (gtk_widget_destroy), dialog);
  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (gtk_widget_destroyed),
		    &(priv->credits_dialog));
  g_signal_connect (dialog, "style_set",
		    G_CALLBACK (dialog_style_set), NULL);

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), notebook, TRUE, TRUE, 0);

  if (priv->authors != NULL) 
    add_credits_page (about, notebook, _("Written by"), priv->authors);
  
  if (priv->documenters != NULL)
    add_credits_page (about, notebook, _("Documented by"), priv->documenters);
    
  /* Don't show an untranslated gettext msgid */
  if (priv->translator_credits != NULL &&
      strcmp (priv->translator_credits, "translator_credits") &&
      strcmp (priv->translator_credits, "translator-credits")) 
    {
      gchar *translators[2];
      
      translators[0] = priv->translator_credits;
      translators[1] = NULL;

      add_credits_page (about, notebook, _("Translated by"), translators);
    }

  if (priv->artists != NULL) 
    add_credits_page (about, notebook, _("Artwork by"), priv->artists);
  
  gtk_widget_show_all (dialog);
}

static void
set_policy (GtkWidget *sw)
{
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);  
}

static void
display_license_dialog (GtkWidget *button, 
			gpointer   data)
{
  GtkAboutDialog *about = (GtkAboutDialog *)data;
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;
  GtkWidget *dialog, *view, *sw;

  if (priv->license_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->license_dialog));
      return;
    }
	
  dialog = gtk_dialog_new_with_buttons (_("License"),
					GTK_WINDOW (about),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					NULL);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  priv->license_dialog = dialog;
  gtk_window_set_default_size (GTK_WINDOW (dialog), 420, 320);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

  gtk_window_set_modal (GTK_WINDOW (dialog), 
			gtk_window_get_modal (GTK_WINDOW (about)));

  g_signal_connect (dialog, "response",
		    G_CALLBACK (gtk_widget_destroy), dialog);
  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (gtk_widget_destroyed),
		    &(priv->license_dialog));
  g_signal_connect (dialog, "style_set",
		    G_CALLBACK (dialog_style_set), NULL);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
				       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);
  g_signal_connect (sw, "map", G_CALLBACK (set_policy), NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), sw, TRUE, TRUE, 0);

  view = gtk_text_view_new ();
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), 
			    priv->license, -1);

  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);

  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 8);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 8);

  gtk_container_add (GTK_CONTAINER (sw), view);

  gtk_widget_show_all (dialog);
}

static void 
close_cb (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;

  if (priv->license_dialog != NULL)
    {
      gtk_widget_destroy (priv->license_dialog);
      priv->license_dialog = NULL;
    }

  if (priv->credits_dialog != NULL)
    {
      gtk_widget_destroy (priv->credits_dialog);
      priv->credits_dialog = NULL;
    }

  gtk_widget_hide (GTK_WIDGET (about));
  
}

/**
 * gtk_about_dialog_new:
 *
 * Creates a new #GtkAboutDialog.
 *
 * Returns: a newly created #GtkAboutDialog
 *
 * Since: 2.6
 */
GtkWidget *
gtk_about_dialog_new (void)
{
  GtkAboutDialog *dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG, NULL);

  return GTK_WIDGET (dialog);
}

/**
 * gtk_about_dialog_set_email_hook:
 * @func: a function to call when an email link is activated.
 * @data: data to pass to @func
 * @destroy: #GDestroyNotify for @data
 * 
 * Installs a global function to be called whenever the user activates an
 * email link in an about dialog. 
 * 
 * Return value: the previous email hook.
 *
 * Since: 2.6
 */
GtkAboutDialogActivateLinkFunc      
gtk_about_dialog_set_email_hook (GtkAboutDialogActivateLinkFunc func, 
				 gpointer                       data, 
				 GDestroyNotify                 destroy)
{
  GtkAboutDialogActivateLinkFunc old;

  if (activate_email_hook_destroy != NULL)
    (* activate_email_hook_destroy) (activate_email_hook_data);

  old = activate_email_hook;

  activate_email_hook = func;
  activate_email_hook_data = data;
  activate_email_hook_destroy = destroy;

  return old;
}

/**
 * gtk_about_dialog_set_url_hook:
 * @func: a function to call when a URL link is activated.
 * @data: data to pass to @func
 * @destroy: #GDestroyNotify for @data
 * 
 * Installs a global function to be called whenever the user activates a
 * URL link in an about dialog.
 * 
 * Return value: the previous URL hook.
 *
 * Since: 2.6
 */
GtkAboutDialogActivateLinkFunc      
gtk_about_dialog_set_url_hook (GtkAboutDialogActivateLinkFunc func, 
			       gpointer                       data, 
			       GDestroyNotify                 destroy)
{
  GtkAboutDialogActivateLinkFunc old;

  if (activate_url_hook_destroy != NULL)
    (* activate_url_hook_destroy) (activate_url_hook_data);

  old = activate_url_hook;

  activate_url_hook = func;
  activate_url_hook_data = data;
  activate_url_hook_destroy = destroy;

  return old;
}

/**
 * gtk_show_about_dialog:
 * @parent: transient parent, or %NULL for none
 * @first_property_name: the name of the first property 
 * @Varargs: value of first property, followed by more properties, %NULL-terminated
 *
 * This is a convenience function for showing an application's about box.
 * The constructed dialog is associated with the parent window and 
 * reused for future invocations of this function.
 *
 * Since: 2.6
 */
void
gtk_show_about_dialog (GtkWindow   *parent,
		       const gchar *first_property_name,
		       ...)
{
  static GtkWidget *global_about_dialog = NULL;
  GtkWidget *dialog = NULL;
  va_list var_args;

  if (parent)
    dialog = g_object_get_data (G_OBJECT (parent), "gtk-about-dialog");
  else 
    dialog = global_about_dialog;

  if (!dialog) 
    {
      dialog = gtk_about_dialog_new ();

      g_object_ref (dialog);
      gtk_object_sink (GTK_OBJECT (dialog));

      g_signal_connect (dialog, "delete_event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      va_start (var_args, first_property_name);
      g_object_set_valist (G_OBJECT (dialog), first_property_name, var_args);
      va_end (var_args);

      if (parent) 
	{
	  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	  g_object_set_data_full (G_OBJECT (parent), "gtk-about-dialog", 
				  dialog, g_object_unref);
	}
      else 
	global_about_dialog = dialog;
      
    }
  
  gtk_window_present (GTK_WINDOW (dialog));
}
