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

#include "locale.h"
#include <string.h>

#include "gtk/gtkintl.h"
#include "gtk/gtklabel.h"
#include "gtk/gtksignal.h"
#include "gtk/gtkwindow.h"
#include "gtkimcontextxim.h"

typedef struct _StatusWindow StatusWindow;

struct _GtkXIMInfo
{
  GdkScreen *screen;
  XIM im;
  char *locale;
  XIMStyle preedit_style_setting;
  XIMStyle status_style_setting;
  XIMStyle style;
  GtkSettings *settings;
  gulong status_set;
  gulong preedit_set;
  XIMStyles *xim_styles;
  GSList *ics;
};

/* A context status window; these are kept in the status_windows list. */
struct _StatusWindow
{
  GtkWidget *window;
  
  /* Toplevel window to which the status window corresponds */
  GtkWidget *toplevel;
  
  /* Signal connection ids; we connect to the toplevel */
  gulong destroy_handler_id;
  gulong configure_handler_id;
};

static void     gtk_im_context_xim_class_init         (GtkIMContextXIMClass  *class);
static void     gtk_im_context_xim_init               (GtkIMContextXIM       *im_context_xim);
static void     gtk_im_context_xim_finalize           (GObject               *obj);
static void     gtk_im_context_xim_set_client_window  (GtkIMContext          *context,
						       GdkWindow             *client_window);
static gboolean gtk_im_context_xim_filter_keypress    (GtkIMContext          *context,
						       GdkEventKey           *key);
static void     gtk_im_context_xim_reset              (GtkIMContext          *context);
static void     gtk_im_context_xim_focus_in           (GtkIMContext          *context);
static void     gtk_im_context_xim_focus_out          (GtkIMContext          *context);
static void     gtk_im_context_xim_set_cursor_location (GtkIMContext          *context,
						       GdkRectangle		*area);
static void     gtk_im_context_xim_set_use_preedit    (GtkIMContext          *context,
						       gboolean		      use_preedit);
static void     gtk_im_context_xim_get_preedit_string (GtkIMContext          *context,
						       gchar                **str,
						       PangoAttrList        **attrs,
						       gint                  *cursor_pos);

static void reinitialize_ic      (GtkIMContextXIM *context_xim,
				  gboolean         send_signal);
static void set_ic_client_window (GtkIMContextXIM *context_xim,
				  GdkWindow       *client_window,
				  gboolean         send_signal);

static void setup_styles (GtkXIMInfo *info);

static void status_window_show     (GtkIMContextXIM *context_xim);
static void status_window_hide     (GtkIMContextXIM *context_xim);
static void status_window_set_text (GtkIMContextXIM *context_xim,
				    const gchar     *text);

static XIC       gtk_im_context_xim_get_ic            (GtkIMContextXIM *context_xim);
static GObjectClass *parent_class;

GType gtk_type_im_context_xim = 0;

GSList *open_ims = NULL;

/* List of status windows for different toplevels */
static GSList *status_windows = NULL;

void
gtk_im_context_xim_register_type (GTypeModule *type_module)
{
  static const GTypeInfo im_context_xim_info =
  {
    sizeof (GtkIMContextXIMClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gtk_im_context_xim_class_init,
    NULL,           /* class_finalize */    
    NULL,           /* class_data */
    sizeof (GtkIMContextXIM),
    0,
    (GInstanceInitFunc) gtk_im_context_xim_init,
  };

  gtk_type_im_context_xim = 
    g_type_module_register_type (type_module,
				 GTK_TYPE_IM_CONTEXT,
				 "GtkIMContextXIM",
				 &im_context_xim_info, 0);
}

#define PREEDIT_MASK (XIMPreeditCallbacks | XIMPreeditPosition | \
		      XIMPreeditArea | XIMPreeditNothing | XIMPreeditNone)
#define STATUS_MASK (XIMStatusCallbacks | XIMStatusArea | \
		      XIMStatusNothing | XIMStatusNone)
#define ALLOWED_MASK (XIMPreeditCallbacks | XIMPreeditNothing | XIMPreeditNone | \
		      XIMStatusCallbacks | XIMStatusNothing | XIMStatusNone)

static XIMStyle 
choose_better_style (XIMStyle style1, XIMStyle style2) 
{
  XIMStyle s1, s2, u; 
  
  if (style1 == 0) return style2;
  if (style2 == 0) return style1;
  if ((style1 & (PREEDIT_MASK | STATUS_MASK))
    	== (style2 & (PREEDIT_MASK | STATUS_MASK)))
    return style1;

  s1 = style1 & PREEDIT_MASK;
  s2 = style2 & PREEDIT_MASK;
  u = s1 | s2;
  if (s1 != s2) {
    if (u & XIMPreeditCallbacks)
      return (s1 == XIMPreeditCallbacks) ? style1 : style2;
    else if (u & XIMPreeditPosition)
      return (s1 == XIMPreeditPosition) ? style1 :style2;
    else if (u & XIMPreeditArea)
      return (s1 == XIMPreeditArea) ? style1 : style2;
    else if (u & XIMPreeditNothing)
      return (s1 == XIMPreeditNothing) ? style1 : style2;
  } else {
    s1 = style1 & STATUS_MASK;
    s2 = style2 & STATUS_MASK;
    u = s1 | s2;
    if (u & XIMStatusCallbacks)
      return (s1 == XIMStatusCallbacks) ? style1 : style2;
    else if (u & XIMStatusArea)
      return (s1 == XIMStatusArea) ? style1 : style2;
    else if (u & XIMStatusNothing)
      return (s1 == XIMStatusNothing) ? style1 : style2;
    else if (u & XIMStatusNone)
      return (s1 == XIMStatusNone) ? style1 : style2;
  }
  return 0; /* Get rid of stupid warning */
}

static void
reinitialize_all_ics (GtkXIMInfo *info)
{
  GSList *tmp_list;

  for (tmp_list = info->ics; tmp_list; tmp_list = tmp_list->next)
    reinitialize_ic (tmp_list->data, TRUE);
}

static void
status_style_change (GtkXIMInfo *info)
{
  GtkIMStatusStyle status_style;
  
  g_object_get (info->settings,
		"gtk-im-status-style", &status_style,
		NULL);
  if (status_style == GTK_IM_STATUS_CALLBACK)
    info->status_style_setting = XIMStatusCallbacks;
  else if (status_style == GTK_IM_STATUS_NOTHING)
    info->status_style_setting = XIMStatusNothing;
  else
    return;

  setup_styles (info);
  
  reinitialize_all_ics (info);
}

static void
preedit_style_change (GtkXIMInfo *info)
{
  GtkIMPreeditStyle preedit_style;
  g_object_get (info->settings,
		"gtk-im-preedit-style", &preedit_style,
		NULL);
  if (preedit_style == GTK_IM_PREEDIT_CALLBACK)
    info->preedit_style_setting = XIMPreeditCallbacks;
  else if (preedit_style == GTK_IM_PREEDIT_NOTHING)
    info->preedit_style_setting = XIMPreeditNothing;
  else
    return;

  setup_styles (info);
  
  reinitialize_all_ics (info);
}

static void
setup_styles (GtkXIMInfo *info)
{
  int i;
  unsigned long settings_preference;
  XIMStyles *xim_styles = info->xim_styles;

  settings_preference = info->status_style_setting|info->preedit_style_setting;
  if (xim_styles)
    {
      for (i = 0; i < xim_styles->count_styles; i++)
	if ((xim_styles->supported_styles[i] & ALLOWED_MASK) == xim_styles->supported_styles[i])
	  {
	    if (settings_preference == xim_styles->supported_styles[i])
	      {
		info->style = settings_preference;
		break;
	      }
	    info->style = choose_better_style (info->style,
					       xim_styles->supported_styles[i]);
	  }
    }
}

static void
setup_im (GtkXIMInfo *info)
{
  XIMValuesList *ic_values = NULL;

  if (info->im == NULL)
    return;

  XGetIMValues (info->im,
		XNQueryInputStyle, &info->xim_styles,
		XNQueryICValuesList, &ic_values,
		NULL);

  info->settings = gtk_settings_get_for_screen (info->screen);

  if (!g_object_class_find_property (G_OBJECT_GET_CLASS (info->settings),
				     "gtk-im-preedit-style"))
    gtk_settings_install_property (g_param_spec_enum ("gtk-im-preedit-style",
						      _("IM Preedit style"),
						      _("How to draw the input method preedit string"),
						      GTK_TYPE_IM_PREEDIT_STYLE,
						      GTK_IM_PREEDIT_CALLBACK,
						      G_PARAM_READWRITE));

  if (!g_object_class_find_property (G_OBJECT_GET_CLASS (info->settings),
				     "gtk-im-status-style"))
    gtk_settings_install_property (g_param_spec_enum ("gtk-im-status-style",
						      _("IM Status style"),
						      _("How to draw the input method statusbar"),
						      GTK_TYPE_IM_STATUS_STYLE,
						      GTK_IM_STATUS_CALLBACK,
						      G_PARAM_READWRITE));

  info->status_set = g_signal_connect_swapped (info->settings,
					       "notify::gtk-im-status-style",
					       G_CALLBACK (status_style_change),
					       info);
  info->preedit_set = g_signal_connect_swapped (info->settings,
						"notify::gtk-im-preedit-style",
						G_CALLBACK (preedit_style_change),
						info);

  status_style_change (info);
  preedit_style_change (info);

#if 0
  if (ic_values)
    {
      for (i = 0; i < ic_values->count_values; i++)
	g_print ("%s\n", ic_values->supported_values[i]);
      for (i = 0; i < xim_styles->count_styles; i++)
	g_print ("%#x\n", xim_styles->supported_styles[i]);
    }
#endif

  if (ic_values)
    XFree (ic_values);
}

static void
xim_info_display_closed (GdkDisplay *display,
			 gboolean    is_error,
			 GtkXIMInfo *info)
{
  GSList *ics, *tmp_list;

  open_ims = g_slist_remove (open_ims, info);

  ics = info->ics;
  info->ics = NULL;

  for (tmp_list = ics; tmp_list; tmp_list = tmp_list->next)
    set_ic_client_window (tmp_list->data, NULL, TRUE);

  g_slist_free (tmp_list);
  
  g_signal_handler_disconnect (info->settings, info->status_set);
  g_signal_handler_disconnect (info->settings, info->preedit_set);
  
  XFree (info->xim_styles->supported_styles);
  XFree (info->xim_styles);
  g_free (info->locale);

  if (info->im)
    XCloseIM (info->im);

  g_free (info);
}

static GtkXIMInfo *
get_im (GdkWindow *client_window,
	const char *locale)
{
  GSList *tmp_list;
  GtkXIMInfo *info;
  XIM im = NULL;
  GdkScreen *screen = gdk_drawable_get_screen (client_window);
  GdkDisplay *display = gdk_screen_get_display (screen);

  tmp_list = open_ims;
  while (tmp_list)
    {
      info = tmp_list->data;
      if (info->screen == screen &&
	  strcmp (info->locale, locale) == 0)
	return info;

      tmp_list = tmp_list->next;
    }

  info = NULL;

  if (XSupportsLocale ())
    {
      if (!XSetLocaleModifiers (""))
	g_warning ("Unable to set locale modifiers with XSetLocaleModifiers()");
      
      im = XOpenIM (GDK_DISPLAY_XDISPLAY (display), NULL, NULL, NULL);
      
      if (!im)
	g_warning ("Unable to open XIM input method, falling back to XLookupString()");

      info = g_new (GtkXIMInfo, 1);
      open_ims = g_slist_prepend (open_ims, info);

      info->screen = screen;
      info->locale = g_strdup (locale);
      info->im = im;
      info->xim_styles = NULL;
      info->preedit_style_setting = 0;
      info->status_style_setting = 0;
      info->settings = NULL;
      info->preedit_set = 0;
      info->status_set = 0;
      info->ics = NULL;

      setup_im (info);

      g_signal_connect (display, "closed",
			G_CALLBACK (xim_info_display_closed), info);
    }

  return info;
}

static void
gtk_im_context_xim_class_init (GtkIMContextXIMClass *class)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  im_context_class->set_client_window = gtk_im_context_xim_set_client_window;
  im_context_class->filter_keypress = gtk_im_context_xim_filter_keypress;
  im_context_class->reset = gtk_im_context_xim_reset;
  im_context_class->get_preedit_string = gtk_im_context_xim_get_preedit_string;
  im_context_class->focus_in = gtk_im_context_xim_focus_in;
  im_context_class->focus_out = gtk_im_context_xim_focus_out;
  im_context_class->set_cursor_location = gtk_im_context_xim_set_cursor_location;
  im_context_class->set_use_preedit = gtk_im_context_xim_set_use_preedit;
  gobject_class->finalize = gtk_im_context_xim_finalize;
}

static void
gtk_im_context_xim_init (GtkIMContextXIM *im_context_xim)
{
  im_context_xim->use_preedit = TRUE;
  im_context_xim->filter_key_release = FALSE;
  im_context_xim->status_visible = FALSE;
}

static void
gtk_im_context_xim_finalize (GObject *obj)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (obj);

  set_ic_client_window (context_xim, NULL, FALSE);

  g_free (context_xim->locale);
  g_free (context_xim->mb_charset);
}

static void
reinitialize_ic (GtkIMContextXIM *context_xim,
		 gboolean         send_signal)
{
  if (context_xim->ic)
    {
      XDestroyIC (context_xim->ic);
      status_window_hide (context_xim);
      context_xim->ic = NULL;

      if (context_xim->preedit_length)
	{
	  context_xim->preedit_length = 0;
	  if (send_signal)
	    g_signal_emit_by_name (context_xim, "preedit_changed");
	}
    }
}

static void
set_ic_client_window (GtkIMContextXIM *context_xim,
		      GdkWindow       *client_window,
		      gboolean         send_signal)
{
  reinitialize_ic (context_xim, send_signal);
  if (context_xim->client_window)
    {
      context_xim->im_info->ics = g_slist_remove (context_xim->im_info->ics, context_xim);
      context_xim->im_info = NULL;
    }
  
  context_xim->client_window = client_window;

  if (context_xim->client_window)
    {
      context_xim->im_info = get_im (context_xim->client_window, context_xim->locale);
      context_xim->im_info->ics = g_slist_prepend (context_xim->im_info->ics, context_xim);
    }
}

static void
gtk_im_context_xim_set_client_window (GtkIMContext          *context,
				      GdkWindow             *client_window)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);

  set_ic_client_window (context_xim, client_window, TRUE);
}

GtkIMContext *
gtk_im_context_xim_new (void)
{
  GtkIMContextXIM *result;
  const gchar *charset;

  result = GTK_IM_CONTEXT_XIM (g_object_new (GTK_TYPE_IM_CONTEXT_XIM, NULL));

  result->locale = g_strdup (setlocale (LC_CTYPE, NULL));
  
  g_get_charset (&charset);
  result->mb_charset = g_strdup (charset);

  return GTK_IM_CONTEXT (result);
}

static char *
mb_to_utf8 (GtkIMContextXIM *context_xim,
	    const char      *str)
{
  GError *error = NULL;
  gchar *result;

  if (strcmp (context_xim->mb_charset, "UTF-8") == 0)
    result = g_strdup (str);
  else
    {
      result = g_convert (str, -1,
			  "UTF-8", context_xim->mb_charset,
			  NULL, NULL, &error);
      if (!result)
	{
	  g_warning ("Error converting text from IM to UTF-8: %s\n", error->message);
	  g_error_free (error);
	}
    }
  
  return result;
}

static gboolean
gtk_im_context_xim_filter_keypress (GtkIMContext *context,
				    GdkEventKey  *event)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);
  XIC ic = gtk_im_context_xim_get_ic (context_xim);
  gchar static_buffer[256];
  gchar *buffer = static_buffer;
  gint buffer_size = sizeof(static_buffer) - 1;
  gint num_bytes = 0;
  KeySym keysym;
  Status status;
  gboolean result = FALSE;
  GdkWindow *root_window = gdk_screen_get_root_window (gdk_drawable_get_screen (event->window));

  XKeyPressedEvent xevent;

  if (event->type == GDK_KEY_RELEASE && !context_xim->filter_key_release)
    return FALSE;

  xevent.type = (event->type == GDK_KEY_PRESS) ? KeyPress : KeyRelease;
  xevent.serial = 0;		/* hope it doesn't matter */
  xevent.send_event = event->send_event;
  xevent.display = GDK_DRAWABLE_XDISPLAY (event->window);
  xevent.window = GDK_DRAWABLE_XID (event->window);
  xevent.root = GDK_DRAWABLE_XID (root_window);
  xevent.subwindow = xevent.window;
  xevent.time = event->time;
  xevent.x = xevent.x_root = 0;
  xevent.y = xevent.y_root = 0;
  xevent.state = event->state;
  xevent.keycode = event->hardware_keycode;
  xevent.same_screen = True;
  
  if (XFilterEvent ((XEvent *)&xevent, GDK_DRAWABLE_XID (context_xim->client_window)))
    return TRUE;
  
 again:
  if (ic)
    num_bytes = XmbLookupString (ic, &xevent, buffer, buffer_size, &keysym, &status);
  else
    {
      num_bytes = XLookupString (&xevent, buffer, buffer_size, &keysym, NULL);
      status = XLookupBoth;
    }

  if (status == XBufferOverflow)
    {
      buffer_size = num_bytes;
      if (buffer != static_buffer) 
	g_free (buffer);
      buffer = g_malloc (num_bytes + 1);
      goto again;
    }

  /* I don't know how we should properly handle XLookupKeysym or XLookupBoth
   * here ... do input methods actually change the keysym? we can't really
   * feed it back to accelerator processing at this point...
   */
  if (status == XLookupChars || status == XLookupBoth)
    {
      char *result_utf8;

      buffer[num_bytes] = '\0';

      result_utf8 = mb_to_utf8 (context_xim, buffer);
      if (result_utf8)
	{
	  if ((guchar)result_utf8[0] >= 0x20 &&
	      result_utf8[0] != 0x7f) /* Some IM have a nasty habit of converting
				       * control characters into strings
				       */
	    {
	      g_signal_emit_by_name (context, "commit", result_utf8);
	      result = TRUE;
	    }
	  
	  g_free (result_utf8);
	}
    }

  if (buffer != static_buffer) 
    g_free (buffer);

  return result;
}

static void
gtk_im_context_xim_focus_in (GtkIMContext *context)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);
  XIC ic = gtk_im_context_xim_get_ic (context_xim);

  if (!ic)
    return;

  XSetICFocus (ic);

  status_window_show (context_xim);

  return;
}

static void
gtk_im_context_xim_focus_out (GtkIMContext *context)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);
  XIC ic = gtk_im_context_xim_get_ic (context_xim);

  if (!ic)
    return;

  XUnsetICFocus (ic);

  status_window_hide (context_xim);

  return;
}

static void
gtk_im_context_xim_set_cursor_location (GtkIMContext *context,
					GdkRectangle *area)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);
  XIC ic = gtk_im_context_xim_get_ic (context_xim);

  XVaNestedList preedit_attr;
  XPoint          spot;

  if (!ic)
    return;

  spot.x = area->x;
  spot.y = area->y;

  preedit_attr = XVaCreateNestedList (0,
				      XNSpotLocation, &spot,
				      0);
  XSetICValues (ic,
		XNPreeditAttributes, preedit_attr,
		NULL);
  XFree(preedit_attr);

  return;
}

static void
gtk_im_context_xim_set_use_preedit (GtkIMContext *context,
				    gboolean      use_preedit)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);

  use_preedit = use_preedit != FALSE;

  if (context_xim->use_preedit != use_preedit)
    {
      context_xim->use_preedit = use_preedit;
      reinitialize_ic (context_xim, TRUE);
    }

  return;
}

static void
gtk_im_context_xim_reset (GtkIMContext *context)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);
  XIC ic = gtk_im_context_xim_get_ic (context_xim);
  gchar *result;

  /* restore conversion state after resetting ic later */
  XIMPreeditState preedit_state = XIMPreeditUnKnown;
  XVaNestedList preedit_attr;
  gboolean have_preedit_state = FALSE;

  if (!ic)
    return;
  

  if (context_xim->preedit_length == 0)
    return;

  preedit_attr = XVaCreateNestedList(0,
                                     XNPreeditState, &preedit_state,
                                     0);
  if (!XGetICValues(ic,
                    XNPreeditAttributes, preedit_attr,
                    NULL))
    have_preedit_state = TRUE;

  XFree(preedit_attr);

  result = XmbResetIC (ic);

  preedit_attr = XVaCreateNestedList(0,
                                     XNPreeditState, preedit_state,
                                     0);
  if (have_preedit_state)
    XSetICValues(ic,
		 XNPreeditAttributes, preedit_attr,
		 NULL);

  XFree(preedit_attr);

  if (result)
    {
      char *result_utf8 = mb_to_utf8 (context_xim, result);
      if (result_utf8)
	{
	  g_signal_emit_by_name (context, "commit", result_utf8);
	  g_free (result_utf8);
	}
    }

  if (context_xim->preedit_length)
    {
      context_xim->preedit_length = 0;
      g_signal_emit_by_name (context, "preedit_changed");
    }

  XFree (result);
}

/* Mask of feedback bits that we render
 */
#define FEEDBACK_MASK (XIMReverse | XIMUnderline)

static void
add_feedback_attr (PangoAttrList *attrs,
		   const gchar   *str,
		   XIMFeedback    feedback,
		   gint           start_pos,
		   gint           end_pos)
{
  PangoAttribute *attr;
  
  gint start_index = g_utf8_offset_to_pointer (str, start_pos) - str;
  gint end_index = g_utf8_offset_to_pointer (str, end_pos) - str;

  if (feedback & XIMUnderline)
    {
      attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
      attr->start_index = start_index;
      attr->end_index = end_index;

      pango_attr_list_change (attrs, attr);
    }

  if (feedback & XIMReverse)
    {
      attr = pango_attr_foreground_new (0xffff, 0xffff, 0xffff);
      attr->start_index = start_index;
      attr->end_index = end_index;

      pango_attr_list_change (attrs, attr);

      attr = pango_attr_background_new (0, 0, 0);
      attr->start_index = start_index;
      attr->end_index = end_index;

      pango_attr_list_change (attrs, attr);
    }

  if (feedback & ~FEEDBACK_MASK)
    g_warning ("Unrendered feedback style: %#lx", feedback & ~FEEDBACK_MASK);
}

static void     
gtk_im_context_xim_get_preedit_string (GtkIMContext   *context,
				       gchar         **str,
				       PangoAttrList **attrs,
				       gint           *cursor_pos)
{
  GtkIMContextXIM *context_xim = GTK_IM_CONTEXT_XIM (context);
  gchar *utf8 = g_ucs4_to_utf8 (context_xim->preedit_chars, context_xim->preedit_length, NULL, NULL, NULL);

  if (attrs)
    {
      int i;
      XIMFeedback last_feedback = 0;
      gint start = -1;
      
      *attrs = pango_attr_list_new ();

      for (i = 0; i < context_xim->preedit_length; i++)
	{
	  XIMFeedback new_feedback = context_xim->feedbacks[i] & FEEDBACK_MASK;
	  if (new_feedback != last_feedback)
	    {
	      if (start >= 0)
		add_feedback_attr (*attrs, utf8, last_feedback, start, i);
	      
	      last_feedback = new_feedback;
	      start = i;
	    }
	}

      if (start >= 0)
	add_feedback_attr (*attrs, utf8, last_feedback, start, i);
    }

  if (str)
    *str = utf8;
  else
    g_free (utf8);

  if (cursor_pos)
    *cursor_pos = context_xim->preedit_cursor;
}

static void
preedit_start_callback (XIC      xic,
			XPointer client_data,
			XPointer call_data)
{
  GtkIMContext *context = GTK_IM_CONTEXT (client_data);
  
  g_signal_emit_by_name (context, "preedit_start");
}		     

static void
preedit_done_callback (XIC      xic,
		     XPointer client_data,
		     XPointer call_data)
{
  GtkIMContext *context = GTK_IM_CONTEXT (client_data);
  
  g_signal_emit_by_name (context, "preedit_end");  
}		     

static gint
xim_text_to_utf8 (GtkIMContextXIM *context, XIMText *xim_text, gchar **text)
{
  gint text_length = 0;
  GError *error = NULL;
  gchar *result = NULL;

  if (xim_text && xim_text->string.multi_byte)
    {
      if (xim_text->encoding_is_wchar)
	{
	  g_warning ("Wide character return from Xlib not currently supported");
	  *text = NULL;
	  return 0;
	}

      if (strcmp (context->mb_charset, "UTF-8") == 0)
	result = g_strdup (xim_text->string.multi_byte);
      else
	result = g_convert (xim_text->string.multi_byte,
			    -1,
			    "UTF-8",
			    context->mb_charset,
			    NULL, NULL, &error);
      
      if (result)
	{
	  text_length = g_utf8_strlen (result, -1);
	  
	  if (text_length != xim_text->length)
	    {
	      g_warning ("Size mismatch when converting text from input method: supplied length = %d\n, result length = %d", xim_text->length, text_length);
	    }
	}
      else
	{
	  g_warning ("Error converting text from IM to UCS-4: %s", error->message);
	  g_error_free (error);

	  *text = NULL;
	  return 0;
	}

      *text = result;
      return text_length;
    }
  else
    {
      *text = NULL;
      return 0;
    }
}

static void
preedit_draw_callback (XIC                           xic, 
		       XPointer                      client_data,
		       XIMPreeditDrawCallbackStruct *call_data)
{
  GtkIMContextXIM *context = GTK_IM_CONTEXT_XIM (client_data);

  XIMText *new_xim_text = call_data->text;
  gint new_text_length;
  gunichar *new_text = NULL;
  gint i;
  gint diff;
  gint new_length;
  gchar *tmp;
  
  gint chg_first = CLAMP (call_data->chg_first, 0, context->preedit_length);
  gint chg_length = CLAMP (call_data->chg_length, 0, context->preedit_length - chg_first);

  context->preedit_cursor = call_data->caret;
  
  if (chg_first != call_data->chg_first || chg_length != call_data->chg_length)
    g_warning ("Invalid change to preedit string, first=%d length=%d (orig length == %d)",
	       call_data->chg_first, call_data->chg_length, context->preedit_length);

  new_text_length = xim_text_to_utf8 (context, new_xim_text, &tmp);
  if (tmp)
    {
      new_text = g_utf8_to_ucs4_fast (tmp, -1, NULL);
      g_free (tmp);
    }
  
  diff = new_text_length - chg_length;
  new_length = context->preedit_length + diff;

  if (new_length > context->preedit_size)
    {
      context->preedit_size = new_length;
      context->preedit_chars = g_renew (gunichar, context->preedit_chars, new_length);
      context->feedbacks = g_renew (XIMFeedback, context->feedbacks, new_length);
    }

  if (diff < 0)
    {
      for (i = chg_first + chg_length ; i < context->preedit_length; i++)
	{
	  context->preedit_chars[i + diff] = context->preedit_chars[i];
	  context->feedbacks[i + diff] = context->feedbacks[i];
	}
    }
  else
    {
      for (i = context->preedit_length - 1; i >= chg_first + chg_length ; i--)
	{
	  context->preedit_chars[i + diff] = context->preedit_chars[i];
	  context->feedbacks[i + diff] = context->feedbacks[i];
	}
    }

  for (i = 0; i < new_text_length; i++)
    {
      context->preedit_chars[chg_first + i] = new_text[i];
      context->feedbacks[chg_first + i] = new_xim_text->feedback[i];
    }

  context->preedit_length += diff;

  if (new_text)
    g_free (new_text);

  g_signal_emit_by_name (context, "preedit_changed");
}
    

static void
preedit_caret_callback (XIC                            xic,
			XPointer                       client_data,
			XIMPreeditCaretCallbackStruct *call_data)
{
  GtkIMContextXIM *context = GTK_IM_CONTEXT_XIM (client_data);
  
  if (call_data->direction == XIMAbsolutePosition)
    {
      context->preedit_cursor = call_data->position;
      g_signal_emit_by_name (context, "preedit_changed");
    }
  else
    {
      g_warning ("Caret movement command: %d %d %d not supported",
		 call_data->position, call_data->direction, call_data->style);
    }
}	     

static void
status_start_callback (XIC      xic,
		       XPointer client_data,
		       XPointer call_data)
{
  return;
} 

static void
status_done_callback (XIC      xic,
		      XPointer client_data,
		      XPointer call_data)
{
  return;
}

static void
status_draw_callback (XIC      xic,
		      XPointer client_data,
		      XIMStatusDrawCallbackStruct *call_data)
{
  GtkIMContextXIM *context = GTK_IM_CONTEXT_XIM (client_data);

  if (context->status_visible == FALSE)
    return;

  if (call_data->type == XIMTextType)
    {
      gchar *text;
      xim_text_to_utf8 (context, call_data->data.text, &text);

      if (text)
	status_window_set_text (context, text);
      else
	status_window_set_text (context, "");
    }
  else				/* bitmap */
    {
      g_print ("Status drawn with bitmap - id = %#lx\n", call_data->data.bitmap);
    }
}

static XVaNestedList
set_preedit_callback (GtkIMContextXIM *context_xim)
{
  context_xim->preedit_start_callback.client_data = (XPointer)context_xim;
  context_xim->preedit_start_callback.callback = (XIMProc)preedit_start_callback;
  context_xim->preedit_done_callback.client_data = (XPointer)context_xim;
  context_xim->preedit_done_callback.callback = (XIMProc)preedit_done_callback;
  context_xim->preedit_draw_callback.client_data = (XPointer)context_xim;
  context_xim->preedit_draw_callback.callback = (XIMProc)preedit_draw_callback;
  context_xim->preedit_caret_callback.client_data = (XPointer)context_xim;
  context_xim->preedit_caret_callback.callback = (XIMProc)preedit_caret_callback;
  return XVaCreateNestedList (0,
			      XNPreeditStartCallback, &context_xim->preedit_start_callback,
			      XNPreeditDoneCallback, &context_xim->preedit_done_callback,
			      XNPreeditDrawCallback, &context_xim->preedit_draw_callback,
			      XNPreeditCaretCallback, &context_xim->preedit_caret_callback,
			      NULL);
}

static XVaNestedList
set_status_callback (GtkIMContextXIM *context_xim)
{
  context_xim->status_start_callback.client_data = (XPointer)context_xim;
  context_xim->status_start_callback.callback = (XIMProc)status_start_callback;
  context_xim->status_done_callback.client_data = (XPointer)context_xim;
  context_xim->status_done_callback.callback = (XIMProc)status_done_callback;
  context_xim->status_draw_callback.client_data = (XPointer)context_xim;
  context_xim->status_draw_callback.callback = (XIMProc)status_draw_callback;
	  
  return XVaCreateNestedList (0,
			      XNStatusStartCallback, &context_xim->status_start_callback,
			      XNStatusDoneCallback, &context_xim->status_done_callback,
			      XNStatusDrawCallback, &context_xim->status_draw_callback,
			      NULL);
}

static XIC
get_ic_real (GtkIMContextXIM *context_xim)
{
  XIC xic = 0;
  const char *name1 = NULL;
  XVaNestedList list1 = NULL;
  const char *name2 = NULL;
  XVaNestedList list2 = NULL;
  XIMStyle im_style = 0;

  if (context_xim->im_info->im == NULL)
    return (XIC)0;

  if (context_xim->use_preedit &&
      (context_xim->im_info->style & PREEDIT_MASK) == XIMPreeditCallbacks)
    {
      im_style |= XIMPreeditCallbacks;
      name1 = XNPreeditAttributes;
      list1 = set_preedit_callback (context_xim);
    }
  else
    im_style |= XIMPreeditNothing;

  if ((context_xim->im_info->style & STATUS_MASK) == XIMStatusCallbacks)
    {
      im_style |= XIMStatusCallbacks;
      if (name1 == NULL)
	{
	  name1 = XNStatusAttributes;
	  list1 = set_status_callback (context_xim);
	}
      else
	{
	  name2 = XNStatusAttributes;
	  list2 = set_status_callback (context_xim);
	}
    }
  else
    im_style |= XIMStatusNothing;

  xic = XCreateIC (context_xim->im_info->im,
		   XNInputStyle, im_style,
		   XNClientWindow, GDK_DRAWABLE_XID (context_xim->client_window),
		   name1, list1,
		   name2, list2,
		   NULL);
  if (list1)
    XFree (list1);
  if (list2)
    XFree (list2);

  if (xic)
    {
      /* Don't filter key released events with XFilterEvents unless
       * input methods ask for. This is a workaround for Solaris input
       * method bug in C and European locales. It doubles each key
       * stroke if both key pressed and released events are filtered.
       * (bugzilla #81759)
       */
      gulong mask = 0;
      XGetICValues (xic,
		    XNFilterEvents, &mask,
		    NULL);
      context_xim->filter_key_release = (mask & KeyReleaseMask);
    }

  return xic;
}

static XIC
gtk_im_context_xim_get_ic (GtkIMContextXIM *context_xim)
{
  if (!context_xim->ic && context_xim->im_info)
    context_xim->ic = get_ic_real (context_xim);
  
  return context_xim->ic;
}

/**************************
 *                        *
 * Status Window handling *
 *                        *
 **************************/

static gboolean
status_window_expose_event (GtkWidget      *widget,
			    GdkEventExpose *event)
{
  gdk_draw_rectangle (widget->window,
		      widget->style->base_gc [GTK_STATE_NORMAL],
		      TRUE,
		      0, 0,
		      widget->allocation.width, widget->allocation.height);
  gdk_draw_rectangle (widget->window,
		      widget->style->text_gc [GTK_STATE_NORMAL],
		      FALSE,
		      0, 0,
		      widget->allocation.width - 1, widget->allocation.height - 1);

  return FALSE;
}

static void
status_window_style_set (GtkWidget *toplevel,
			 GtkStyle  *previous_style,
			 GtkWidget *label)
{
  gint i;
  
  for (i = 0; i < 5; i++)
    gtk_widget_modify_fg (label, i, &toplevel->style->text[i]);
}

/* Frees a status window and removes its link from the status_windows list */
static void
status_window_free (StatusWindow *status_window)
{
  status_windows = g_slist_remove (status_windows, status_window);
 
  g_signal_handler_disconnect (status_window->toplevel, status_window->destroy_handler_id);
  g_signal_handler_disconnect (status_window->toplevel, status_window->configure_handler_id);
  gtk_widget_destroy (status_window->window);
  g_object_set_data (G_OBJECT (status_window->toplevel), "gtk-im-xim-status-window", NULL);
 
  g_free (status_window);
}

static gboolean
status_window_configure (GtkWidget         *toplevel,
			 GdkEventConfigure *event,
  			 StatusWindow      *status_window)
{
  GdkRectangle rect;
  GtkRequisition requisition;
  gint y;
  gint height = gdk_screen_get_height (gtk_widget_get_screen (toplevel));
  
  gdk_window_get_frame_extents (toplevel->window, &rect);
  gtk_widget_size_request (status_window->window, &requisition);

  if (rect.y + rect.height + requisition.height < height)
    y = rect.y + rect.height;
  else
    y = height - requisition.height;
  
  gtk_window_move (GTK_WINDOW (status_window->window), rect.x, y);

  return FALSE;
}

static GtkWidget *
status_window_get (GtkIMContextXIM *context_xim,
		   gboolean         create)
{
  GdkWindow *toplevel_gdk;
  GtkWidget *toplevel;
  GtkWidget *window;
  StatusWindow *status_window;
  GtkWidget *status_label;
  GdkScreen *screen;
  GdkWindow *root_window;
  
  if (!context_xim->client_window)
    return NULL;

  toplevel_gdk = context_xim->client_window;
  screen = gdk_drawable_get_screen (toplevel_gdk);
  root_window = gdk_screen_get_root_window (screen);
  
  while (TRUE)
    {
      GdkWindow *parent = gdk_window_get_parent (toplevel_gdk);
      if (parent == root_window)
	break;
      else
	toplevel_gdk = parent;
    }

  gdk_window_get_user_data (toplevel_gdk, (gpointer *)&toplevel);
  if (!toplevel)
    return NULL;

  status_window = g_object_get_data (G_OBJECT (toplevel), "gtk-im-xim-status-window");
  if (status_window)
    return status_window->window;
  else if (!create)
    return NULL;

  status_window = g_new (StatusWindow, 1);
  status_window->window = gtk_window_new (GTK_WINDOW_POPUP);
  status_window->toplevel = toplevel;

  status_windows = g_slist_prepend (status_windows, status_window);

  window = status_window->window;

  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_widget_set_app_paintable (window, TRUE);

  status_label = gtk_label_new ("");
  gtk_misc_set_padding (GTK_MISC (status_label), 1, 1);
  gtk_widget_show (status_label);
  
  gtk_container_add (GTK_CONTAINER (window), status_label);

  status_window->destroy_handler_id = g_signal_connect_swapped (toplevel, "destroy",
								G_CALLBACK (status_window_free),
								status_window);
  status_window->configure_handler_id = g_signal_connect (toplevel, "configure_event",
							  G_CALLBACK (status_window_configure),
							  status_window);

  status_window_configure (toplevel, NULL, status_window);

  g_signal_connect (window, "style_set",
		    G_CALLBACK (status_window_style_set), status_label);
  g_signal_connect (window, "expose_event",
		    G_CALLBACK (status_window_expose_event), NULL);

  g_object_set_data (G_OBJECT (toplevel), "gtk-im-xim-status-window", status_window);

  return window;
}

static gboolean
status_window_has_text (GtkWidget *status_window)
{
  GtkWidget *label = GTK_BIN (status_window)->child;
  const gchar *text = gtk_label_get_text (GTK_LABEL (label));

  return text[0] != '\0';
}

static void
status_window_show (GtkIMContextXIM *context_xim)
{
  context_xim->status_visible = TRUE;
}

static void
status_window_hide (GtkIMContextXIM *context_xim)
{
  GtkWidget *status_window = status_window_get (context_xim, FALSE);

  context_xim->status_visible = FALSE;

  if (status_window)
    status_window_set_text (context_xim, "");
}

static void
status_window_set_text (GtkIMContextXIM *context_xim,
			const gchar     *text)
{
  GtkWidget *status_window = status_window_get (context_xim, TRUE);

  if (status_window)
    {
      GtkWidget *label = GTK_BIN (status_window)->child;
      gtk_label_set_text (GTK_LABEL (label), text);
      
      if (context_xim->status_visible && status_window_has_text (status_window))
	gtk_widget_show (status_window);
      else
	gtk_widget_hide (status_window);
    }
}

/**
 * gtk_im_context_xim_shutdown:
 * 
 * Destroys all the status windows that are kept by the XIM contexts.  This
 * function should only be called by the XIM module exit routine.
 **/
void
gtk_im_context_xim_shutdown (void)
{
  while (status_windows)
    status_window_free (status_windows->data);
}
