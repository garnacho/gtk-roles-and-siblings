/* GTK - The GIMP Toolkit
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

#include "gtkbutton.h"
#include "gtkdialog.h"
#include "gtkhbbox.h"
#include "gtkhseparator.h"
#include "gtkvbox.h"
#include "gtksignal.h"
#include "gdkkeysyms.h"
#include "gtkmain.h"

static void gtk_dialog_class_init (GtkDialogClass *klass);
static void gtk_dialog_init       (GtkDialog      *dialog);
static gint gtk_dialog_key_press  (GtkWidget      *widget,
                                   GdkEventKey    *key);                                   

static void gtk_dialog_add_buttons_valist (GtkDialog   *dialog,
                                           const gchar *first_button_text,
                                           va_list      args);

static gint gtk_dialog_delete_event_handler (GtkWidget   *widget,
                                             GdkEventAny *event,
                                             gpointer     user_data);


enum {
  RESPONSE,
  LAST_SIGNAL
};

static gpointer parent_class;
static guint dialog_signals[LAST_SIGNAL];

GtkType
gtk_dialog_get_type (void)
{
  static GtkType dialog_type = 0;

  if (!dialog_type)
    {
      static const GtkTypeInfo dialog_info =
      {
	"GtkDialog",
	sizeof (GtkDialog),
	sizeof (GtkDialogClass),
	(GtkClassInitFunc) gtk_dialog_class_init,
	(GtkObjectInitFunc) gtk_dialog_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      dialog_type = gtk_type_unique (GTK_TYPE_WINDOW, &dialog_info);
    }

  return dialog_type;
}

static void
gtk_dialog_class_init (GtkDialogClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = g_type_class_peek_parent (class);
  
  dialog_signals[RESPONSE] =
    gtk_signal_new ("response",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkDialogClass, response),
                    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, dialog_signals, LAST_SIGNAL);

  widget_class->key_press_event = gtk_dialog_key_press;
}

static void
gtk_dialog_init (GtkDialog *dialog)
{
  GtkWidget *separator;

  /* To avoid breaking old code that prevents destroy on delete event
   * by connecting a handler, we have to have the FIRST signal
   * connection on the dialog.
   */
  gtk_signal_connect (GTK_OBJECT (dialog),
                      "delete_event",
                      GTK_SIGNAL_FUNC (gtk_dialog_delete_event_handler),
                      NULL);
  
  dialog->vbox = gtk_vbox_new (FALSE, 0);

  gtk_container_set_border_width (GTK_CONTAINER (dialog->vbox), 2);
  
  gtk_container_add (GTK_CONTAINER (dialog), dialog->vbox);
  gtk_widget_show (dialog->vbox);

  dialog->action_area = gtk_hbutton_box_new ();

  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog->action_area),
                             GTK_BUTTONBOX_END);

  gtk_button_box_set_spacing (GTK_BUTTON_BOX (dialog->action_area), 5);
  
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 5);
  gtk_box_pack_end (GTK_BOX (dialog->vbox), dialog->action_area,
                    FALSE, TRUE, 0);
  gtk_widget_show (dialog->action_area);

  separator = gtk_hseparator_new ();
  gtk_box_pack_end (GTK_BOX (dialog->vbox), separator, FALSE, TRUE, 0);
  gtk_widget_show (separator);
}

static gint
gtk_dialog_delete_event_handler (GtkWidget   *widget,
                                 GdkEventAny *event,
                                 gpointer     user_data)
{
  /* emit response signal */
  gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_DELETE_EVENT);

  /* Do the destroy by default */
  return FALSE;
}

static gint
gtk_dialog_key_press (GtkWidget   *widget,
                      GdkEventKey *key)
{
  GdkEventAny event;

  event.type = GDK_DELETE;
  event.window = widget->window;
  event.send_event = TRUE;

  if (GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, key))
    return TRUE;

  if (key->keyval != GDK_Escape)
    return FALSE;

  /* Synthesize delete_event on key press. */
  g_object_ref (G_OBJECT (event.window));
  
  gtk_main_do_event ((GdkEvent*)&event);
  
  g_object_unref (G_OBJECT (event.window));

  return TRUE;
}

GtkWidget*
gtk_dialog_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_DIALOG));
}

static GtkWidget*
gtk_dialog_new_empty (const gchar     *title,
                      GtkWindow       *parent,
                      GtkDialogFlags   flags)
{
  GtkDialog *dialog;

  dialog = GTK_DIALOG (g_object_new (GTK_TYPE_DIALOG, NULL));

  if (title)
    gtk_window_set_title (GTK_WINDOW (dialog), title);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  return GTK_WIDGET (dialog);
}

/**
 * gtk_dialog_new_with_buttons:
 * @title: Title of the dialog, or NULL
 * @parent: Transient parent of the dialog, or NULL
 * @flags: from #GtkDialogFlags
 * @first_button_text: stock ID or text to go in first button, or NULL
 * @Varargs: response ID for first button, then additional buttons, ending with NULL
 * 
 * Creates a new #GtkDialog with title @title (or NULL for the default
 * title; see gtk_window_set_title()) and transient parent @parent (or
 * NULL for none; see gtk_window_set_transient_for()). The @flags
 * argument can be used to make the dialog modal (GTK_DIALOG_MODAL)
 * and/or to have it destroyed along with its transient parent
 * (GTK_DIALOG_DESTROY_WITH_PARENT). After @flags, button
 * text/response ID pairs should be listed, with a NULL pointer ending
 * the list. Button text can be either a stock ID such as
 * GTK_STOCK_BUTTON_OK, or some arbitrary text.  A response ID can be
 * any positive number, or one of the values in the #GtkResponseType
 * enumeration. If the user clicks one of these dialog buttons,
 * #GtkDialog will emit the "response" signal with the corresponding
 * response ID. If a #GtkDialog receives the "delete_event" signal, it
 * will emit "response" with a response ID of GTK_RESPONSE_NONE.
 * However, destroying a dialog does not emit the "response" signal;
 * so be careful relying on "response" when using
 * the GTK_DIALOG_DESTROY_WITH_PARENT flag. Buttons are from left to right,
 * so the first button in the list will be the leftmost button in the dialog.
 *
 * Here's a simple example:
 * <programlisting>
 *  GtkWidget *dialog = gtk_dialog_new_with_buttons ("My dialog",
 *                                                   main_app_window,
 *                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
 *                                                   GTK_STOCK_BUTTON_OK,
 *                                                   GTK_RESPONSE_ACCEPT,
 *                                                   GTK_STOCK_BUTTON_CANCEL,
 *                                                   GTK_RESPONSE_REJECT,
 *                                                   NULL);
 * </programlisting>
 * 
 * Return value: a new #GtkDialog
 **/
GtkWidget*
gtk_dialog_new_with_buttons (const gchar    *title,
                             GtkWindow      *parent,
                             GtkDialogFlags  flags,
                             const gchar    *first_button_text,
                             ...)
{
  GtkDialog *dialog;
  va_list args;
  
  dialog = GTK_DIALOG (gtk_dialog_new_empty (title, parent, flags));

  va_start (args, first_button_text);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text,
                                 args);
  
  va_end (args);

  return GTK_WIDGET (dialog);
}

typedef struct _ResponseData ResponseData;

struct _ResponseData
{
  gint response_id;
};

static ResponseData*
get_response_data (GtkWidget *widget)
{
  ResponseData *ad = gtk_object_get_data (GTK_OBJECT (widget),
                                          "gtk-dialog-response-data");

  if (ad == NULL)
    {
      ad = g_new (ResponseData, 1);
      
      gtk_object_set_data_full (GTK_OBJECT (widget),
                                "gtk-dialog-response-data",
                                ad,
                                g_free);
    }

  return ad;
}

static void
action_widget_activated (GtkWidget *widget, GtkDialog *dialog)
{
  ResponseData *ad;
  gint response_id;
  
  g_return_if_fail (GTK_IS_DIALOG (dialog));

  response_id = GTK_RESPONSE_NONE;
  
  ad = get_response_data (widget);

  g_assert (ad != NULL);
  
  response_id = ad->response_id;

  gtk_dialog_response (dialog, response_id);
}
/**
 * gtk_dialog_add_action_widget:
 * @dialog: a #GtkDialog
 * @child: an activatable widget
 * @response_id: response ID for @child
 * 
 * Adds an activatable widget to the action area of a #GtkDialog,
 * connecting a signal handler that will emit the "response" signal on
 * the dialog when the widget is activated.  The widget is appended to
 * the end of the dialog's action area.  If you want to add a
 * non-activatable widget, simply pack it into the
 * <literal>action_area</literal> field of the #GtkDialog struct.
 * 
 **/
void
gtk_dialog_add_action_widget  (GtkDialog *dialog,
                               GtkWidget *child,
                               gint       response_id)
{
  ResponseData *ad;
  
  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (GTK_IS_WIDGET (child));

  ad = get_response_data (child);

  ad->response_id = response_id;

  if (GTK_WIDGET_GET_CLASS (child)->activate_signal != 0)
    {
      const gchar* name =
        gtk_signal_name (GTK_WIDGET_GET_CLASS (child)->activate_signal);

      gtk_signal_connect_while_alive (GTK_OBJECT (child),
                                      name,
                                      GTK_SIGNAL_FUNC (action_widget_activated),
                                      dialog,
                                      GTK_OBJECT (dialog));
    }
  else
    g_warning ("Only 'activatable' widgets can be packed into the action area of a GtkDialog");

  gtk_box_pack_end (GTK_BOX (dialog->action_area),
                    child,
                    FALSE, TRUE, 5);  
}

/**
 * gtk_dialog_add_button:
 * @dialog: a #GtkDialog
 * @button_text: text of button, or stock ID
 * @response_id: response ID for the button
 * 
 * Adds a button with the given text (or a stock button, if @button_text is a
 * stock ID) and sets things up so that clicking the button will emit the
 * "response" signal with the given @response_id. The button is appended to the
 * end of the dialog's action area. The button widget is returned, but usually
 * you don't need it.
 *
 * Return value: the button widget that was added
 **/
GtkWidget*
gtk_dialog_add_button (GtkDialog   *dialog,
                       const gchar *button_text,
                       gint         response_id)
{
  GtkWidget *button;
  
  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (button_text != NULL);

  button = gtk_button_new_stock (button_text,
                                 gtk_window_get_default_accel_group (GTK_WINDOW (dialog)));

  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  
  gtk_widget_show (button);
  
  gtk_dialog_add_action_widget (dialog,
                                button,
                                response_id);

  return button;
}

static void
gtk_dialog_add_buttons_valist(GtkDialog      *dialog,
                              const gchar    *first_button_text,
                              va_list         args)
{
  const gchar* text;
  gint response_id;

  if (first_button_text == NULL)
    return;
  
  text = first_button_text;
  response_id = va_arg (args, gint);

  while (text != NULL)
    {
      gtk_dialog_add_button (dialog, text, response_id);

      text = va_arg (args, gchar*);
      if (text == NULL)
        break;
      response_id = va_arg (args, int);
    }
}

/**
 * gtk_dialog_add_buttons:
 * @dialog: a #GtkDialog
 * @first_button_text: button text or stock ID
 * @Varargs: response ID for first button, then more text-response_id pairs
 * 
 * Adds more buttons, same as calling gtk_dialog_add_button()
 * repeatedly.  The variable argument list should be NULL-terminated
 * as with gtk_dialog_new_with_buttons(). Each button must have both
 * text and response ID.
 * 
 **/
void
gtk_dialog_add_buttons (GtkDialog   *dialog,
                        const gchar *first_button_text,
                        ...)
{
  
  va_list args;

  va_start (args, first_button_text);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text,
                                 args);
  
  va_end (args);
}

/**
 * gtk_dialog_response:
 * @dialog: a #GtkDialog
 * @response_id: response ID 
 * 
 * Emits the "response" signal with the given response ID. Used to
 * indicate that the user has responded to the dialog in some way;
 * typically either you or gtk_dialog_run() will be monitoring the
 * "response" signal and take appropriate action.
 **/
void
gtk_dialog_response (GtkDialog *dialog,
                     gint       response_id)
{
  g_return_if_fail (dialog != NULL);
  g_return_if_fail (GTK_IS_DIALOG (dialog));

  gtk_signal_emit (GTK_OBJECT (dialog),
                   dialog_signals[RESPONSE],
                   response_id);
}

typedef struct
{
  GtkDialog *dialog;
  gint response_id;
  GMainLoop *loop;
} RunInfo;

static void
shutdown_loop (RunInfo *ri)
{
  if (ri->loop != NULL)
    {
      g_main_quit (ri->loop);
      g_main_destroy (ri->loop);
      ri->loop = NULL;
    }
}

static void
run_unmap_handler (GtkDialog *dialog, gpointer data)
{
  RunInfo *ri = data;

  shutdown_loop (ri);
}

static void
run_response_handler (GtkDialog *dialog,
                      gint response_id,
                      gpointer data)
{
  RunInfo *ri;

  ri = data;

  ri->response_id = response_id;

  shutdown_loop (ri);
}

static gint
run_delete_handler (GtkDialog *dialog,
                    GdkEventAny *event,
                    gpointer data)
{
  RunInfo *ri = data;
    
  shutdown_loop (ri);

  /* emit response signal */
  gtk_dialog_response (dialog, GTK_RESPONSE_NONE);
  
  return TRUE; /* Do not destroy */
}

/**
 * gtk_dialog_run:
 * @dialog: a #GtkDialog
 * 
 * Blocks in a recursive main loop until the @dialog either emits the
 * response signal, or is destroyed. If the dialog is destroyed,
 * gtk_dialog_run() returns GTK_RESPONSE_NONE. Otherwise, it returns
 * the response ID from the "response" signal emission. Before
 * entering the recursive main loop, gtk_dialog_run() calls
 * gtk_widget_show() on the dialog for you. Note that you still
 * need to show any children of the dialog yourself.
 *
 * During gtk_dialog_run(), the default behavior of "delete_event" is
 * disabled; if the dialog receives "delete_event", it will not be
 * destroyed as windows usually are, and gtk_dialog_run() will return
 * GTK_RESPONSE_DELETE_EVENT. Also, during gtk_dialog_run() the dialog will be
 * modal. You can force gtk_dialog_run() to return at any time by
 * calling gtk_dialog_response() to emit the "response"
 * signal. Destroying the dialog during gtk_dialog_run() is a very bad
 * idea, because your post-run code won't know whether the dialog was
 * destroyed or not.
 *
 * After gtk_dialog_run() returns, you are responsible for hiding or
 * destroying the dialog if you wish to do so.
 *
 * Typical usage of this function might be:
 * <programlisting>
 *   gint result = gtk_dialog_run (GTK_DIALOG (dialog));
 *   switch (result)
 *     {
 *       case GTK_RESPONSE_ACCEPT:
 *          do_application_specific_something ();
 *          break;
 *       default:
 *          do_nothing_since_dialog_was_cancelled ();
 *          break;
 *     }
 *   gtk_widget_destroy (dialog);
 * </programlisting>
 * 
 * Return value: response ID
 **/
gint
gtk_dialog_run (GtkDialog *dialog)
{
  RunInfo ri = { NULL, GTK_RESPONSE_NONE, NULL };
  gboolean was_modal;
  guint response_handler;
  guint destroy_handler;
  guint delete_handler;
  
  g_return_val_if_fail (GTK_IS_DIALOG (dialog), -1);

  gtk_object_ref (GTK_OBJECT (dialog));

  if (!GTK_WIDGET_VISIBLE (dialog))
    gtk_widget_show (GTK_WIDGET (dialog));
  
  was_modal = GTK_WINDOW (dialog)->modal;
  if (!was_modal)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  response_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "response",
                        GTK_SIGNAL_FUNC (run_response_handler),
                        &ri);
  
  destroy_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "unmap",
                        GTK_SIGNAL_FUNC (run_unmap_handler),
                        &ri);
  
  delete_handler =
    gtk_signal_connect (GTK_OBJECT (dialog),
                        "delete_event",
                        GTK_SIGNAL_FUNC (run_delete_handler),
                        &ri);
  
  ri.loop = g_main_new (FALSE);

  g_main_run (ri.loop);
  
  g_assert (ri.loop == NULL);
  
  if (!GTK_OBJECT_DESTROYED (dialog))
    {
      if (!was_modal)
        gtk_window_set_modal (GTK_WINDOW(dialog), FALSE);
      
      gtk_signal_disconnect (GTK_OBJECT (dialog), destroy_handler);
      gtk_signal_disconnect (GTK_OBJECT (dialog), response_handler);
      gtk_signal_disconnect (GTK_OBJECT (dialog), delete_handler);
    }

  gtk_object_unref (GTK_OBJECT (dialog));

  return ri.response_id;
}




