/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include <string.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xlocale.h>	/* so we get the right setlocale */
#else
#include <locale.h>
#endif

#include "gtksignal.h"
#include "gtkimmulticontext.h"
#include "gtkimmodule.h"
#include "gtkmenuitem.h"

static void     gtk_im_multicontext_class_init         (GtkIMMulticontextClass  *class);
static void     gtk_im_multicontext_init               (GtkIMMulticontext       *im_multicontext);
static void     gtk_im_multicontext_finalize           (GObject                 *object);

static void     gtk_im_multicontext_set_slave          (GtkIMMulticontext       *multicontext,
							GtkIMContext            *slave);

static void     gtk_im_multicontext_set_client_window  (GtkIMContext            *context,
							GdkWindow               *window);
static void     gtk_im_multicontext_get_preedit_string (GtkIMContext            *context,
							gchar                  **str,
							PangoAttrList          **attrs,
							gint                   *cursor_pos);
static gboolean gtk_im_multicontext_filter_keypress    (GtkIMContext            *context,
							GdkEventKey             *event);
static void     gtk_im_multicontext_focus_in           (GtkIMContext            *context);
static void     gtk_im_multicontext_focus_out          (GtkIMContext            *context);
static void     gtk_im_multicontext_reset              (GtkIMContext            *context);
static void     gtk_im_multicontext_set_cursor_location (GtkIMContext            *context,
							GdkRectangle		*area);

void            gtk_im_multicontext_preedit_start_cb   (GtkIMContext            *slave,
							GtkIMMulticontext       *multicontext);
void            gtk_im_multicontext_preedit_end_cb     (GtkIMContext            *slave,
							GtkIMMulticontext       *multicontext);
void            gtk_im_multicontext_preedit_changed_cb (GtkIMContext            *slave,
							GtkIMMulticontext       *multicontext);
void            gtk_im_multicontext_commit_cb          (GtkIMContext            *slave,
							const gchar             *str,
							GtkIMMulticontext       *multicontext);

static GtkIMContextClass *parent_class;

static const gchar *global_context_id = NULL;

GtkType
gtk_im_multicontext_get_type (void)
{
  static GtkType im_multicontext_type = 0;

  if (!im_multicontext_type)
    {
      static const GtkTypeInfo im_multicontext_info =
      {
	"GtkIMMulticontext",
	sizeof (GtkIMMulticontext),
	sizeof (GtkIMMulticontextClass),
	(GtkClassInitFunc) gtk_im_multicontext_class_init,
	(GtkObjectInitFunc) gtk_im_multicontext_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      im_multicontext_type = gtk_type_unique (GTK_TYPE_IM_CONTEXT, &im_multicontext_info);
    }

  return im_multicontext_type;
}

static void
gtk_im_multicontext_class_init (GtkIMMulticontextClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  
  parent_class = g_type_class_peek_parent (class);

  im_context_class->set_client_window = gtk_im_multicontext_set_client_window;
  im_context_class->get_preedit_string = gtk_im_multicontext_get_preedit_string;
  im_context_class->filter_keypress = gtk_im_multicontext_filter_keypress;
  im_context_class->focus_in = gtk_im_multicontext_focus_in;
  im_context_class->focus_out = gtk_im_multicontext_focus_out;
  im_context_class->reset = gtk_im_multicontext_reset;
  im_context_class->set_cursor_location = gtk_im_multicontext_set_cursor_location;

  gobject_class->finalize = gtk_im_multicontext_finalize;
}

static void
gtk_im_multicontext_init (GtkIMMulticontext *multicontext)
{
  multicontext->slave = NULL;
}

GtkIMContext *
gtk_im_multicontext_new (void)
{
  return GTK_IM_CONTEXT (gtk_type_new (GTK_TYPE_IM_MULTICONTEXT));
}

static void
gtk_im_multicontext_finalize (GObject *object)
{
  gtk_im_multicontext_set_slave (GTK_IM_MULTICONTEXT (object), NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_im_multicontext_set_slave (GtkIMMulticontext *multicontext,
			       GtkIMContext      *slave)
{
  if (multicontext->slave)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (multicontext->slave), multicontext);
      gtk_object_unref (GTK_OBJECT (multicontext->slave));
    }
  
  multicontext->slave = slave;

  if (multicontext->slave)
    {
      gtk_object_ref (GTK_OBJECT (multicontext->slave));
      gtk_object_sink (GTK_OBJECT (multicontext->slave));

      gtk_signal_connect (GTK_OBJECT (multicontext->slave), "preedit_start",
			  GTK_SIGNAL_FUNC (gtk_im_multicontext_preedit_start_cb),
			  multicontext);
      gtk_signal_connect (GTK_OBJECT (multicontext->slave), "preedit_end",
			  GTK_SIGNAL_FUNC (gtk_im_multicontext_preedit_end_cb),
			  multicontext);
      gtk_signal_connect (GTK_OBJECT (multicontext->slave), "preedit_changed",
			  GTK_SIGNAL_FUNC (gtk_im_multicontext_preedit_changed_cb),
			  multicontext);
      gtk_signal_connect (GTK_OBJECT (multicontext->slave), "commit",
			  GTK_SIGNAL_FUNC (gtk_im_multicontext_commit_cb),
			  multicontext);

      if (multicontext->client_window)
	gtk_im_context_set_client_window (slave, multicontext->client_window);
    }
}

static GtkIMContext *
gtk_im_multicontext_get_slave (GtkIMMulticontext *multicontext)
{
  if (!multicontext->slave)
    {
      if (!global_context_id)
	{
	  const char *locale;
	  
#ifdef HAVE_LC_MESSAGES
	  locale = setlocale (LC_MESSAGES, NULL);
#else
	  locale = setlocale (LC_CTYPE, NULL);
#endif
	  global_context_id = _gtk_im_module_get_default_context_id (locale);
	}
	
      gtk_im_multicontext_set_slave (multicontext, _gtk_im_module_create (global_context_id));
      multicontext->context_id = global_context_id;
    }

  return multicontext->slave;
}

static void
gtk_im_multicontext_set_client_window (GtkIMContext *context,
				       GdkWindow    *window)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  multicontext->client_window = window;
  
  if (slave)
    gtk_im_context_set_client_window (slave, window);
}

static void
gtk_im_multicontext_get_preedit_string (GtkIMContext   *context,
					gchar         **str,
					PangoAttrList **attrs,
					gint           *cursor_pos)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    gtk_im_context_get_preedit_string (slave, str, attrs, cursor_pos);
  else
    {
      if (str)
	*str = g_strdup ("");
      if (attrs)
	*attrs = pango_attr_list_new ();
    }
}

static gboolean
gtk_im_multicontext_filter_keypress (GtkIMContext *context,
				     GdkEventKey  *event)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    return gtk_im_context_filter_keypress (slave, event);
  else
    return FALSE;
}

static void
gtk_im_multicontext_focus_in (GtkIMContext   *context)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave;

  /* If the global context type is different from the context we were
   * using before, get rid of the old slave and create a new one
   * for the new global context type.
   */
  if (!multicontext->context_id ||
      strcmp (global_context_id, multicontext->context_id) != 0)
    gtk_im_multicontext_set_slave (multicontext, NULL);

  slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    gtk_im_context_focus_in (slave);
}

static void
gtk_im_multicontext_focus_out (GtkIMContext   *context)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    gtk_im_context_focus_out (slave);
}

static void
gtk_im_multicontext_reset (GtkIMContext   *context)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    gtk_im_context_reset (slave);
}

static void
gtk_im_multicontext_set_cursor_location (GtkIMContext   *context,
					 GdkRectangle   *area)
{
  GtkIMMulticontext *multicontext = GTK_IM_MULTICONTEXT (context);
  GtkIMContext *slave = gtk_im_multicontext_get_slave (multicontext);

  if (slave)
    gtk_im_context_set_cursor_location (slave, area);
}

void
gtk_im_multicontext_preedit_start_cb   (GtkIMContext      *slave,
					GtkIMMulticontext *multicontext)
{
  gtk_signal_emit_by_name (GTK_OBJECT (multicontext), "preedit_start");
}

void
gtk_im_multicontext_preedit_end_cb (GtkIMContext      *slave,
				    GtkIMMulticontext *multicontext)
{
  gtk_signal_emit_by_name (GTK_OBJECT (multicontext), "preedit_end");
}

void
gtk_im_multicontext_preedit_changed_cb (GtkIMContext      *slave,
					GtkIMMulticontext *multicontext)
{
  gtk_signal_emit_by_name (GTK_OBJECT (multicontext), "preedit_changed");
}

void
gtk_im_multicontext_commit_cb (GtkIMContext      *slave,
			       const gchar       *str,
			       GtkIMMulticontext *multicontext)
{
  gtk_signal_emit_by_name (GTK_OBJECT (multicontext), "commit", str);;
}

static void
activate_cb (GtkWidget         *menuitem,
	     GtkIMMulticontext *context)
{
  const gchar *id = gtk_object_get_data (GTK_OBJECT (menuitem), "gtk-context-id");

  gtk_im_context_reset (GTK_IM_CONTEXT (context));

  global_context_id = id;
  gtk_im_multicontext_set_slave (context, NULL);
}

/**
 * gtk_im_multicontext_append_menuitems:
 * @context: a #GtkIMMultiContext
 * @menushell: a #GtkMenuShell
 * 
 * Add menuitems for various available input methods to a menu;
 * the menuitems, when selected, will switch the input method
 * for the context and the global default input method.
 **/
void
gtk_im_multicontext_append_menuitems (GtkIMMulticontext *context,
				      GtkMenuShell      *menushell)
{
  const GtkIMContextInfo **contexts;
  gint n_contexts, i;
  
  _gtk_im_module_list (&contexts, &n_contexts);

  for (i=0; i < n_contexts; i++)
    {
      GtkWidget *menuitem;

      menuitem = gtk_menu_item_new_with_label (contexts[i]->context_name);

      gtk_object_set_data (GTK_OBJECT (menuitem), "gtk-context-id",
			   (char *)contexts[i]->context_id);
      gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			  activate_cb, context);

      gtk_widget_show (menuitem);
      gtk_menu_shell_append (menushell, menuitem);
    }
}

