/* GDK - The GIMP Drawing Kit
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

#include "gdkx.h"
#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkprivate.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"

typedef struct _OwnerInfo OwnerInfo;

struct _OwnerInfo
{
  GdkAtom    selection;
  GdkWindow *owner;
  gulong     serial;
};

static GSList *owner_list;

/* When a window is destroyed we check if it is the owner
 * of any selections. This is somewhat inefficient, but
 * owner_list is typically short, and it is a low memory,
 * low code solution
 */
void
_gdk_selection_window_destroyed (GdkWindow *window)
{
  GSList *tmp_list = owner_list;
  while (tmp_list)
    {
      OwnerInfo *info = tmp_list->data;
      tmp_list = tmp_list->next;
      
      if (info->owner == window)
	{
	  owner_list = g_slist_remove (owner_list, info);
	  g_free (info);
	}
    }
}

/* We only pass through those SelectionClear events that actually
 * reflect changes to the selection owner that we didn't make ourself.
 */
gboolean
_gdk_selection_filter_clear_event (XSelectionClearEvent *event)
{
  GSList *tmp_list = owner_list;
  GdkDisplay *display = gdk_x11_lookup_xdisplay (event->display);
  
  while (tmp_list)
    {
      OwnerInfo *info = tmp_list->data;

      if (gdk_drawable_get_display (info->owner) == display &&
	  info->selection == gdk_x11_xatom_to_atom_for_display (display, event->selection))
	{
	  if ((GDK_DRAWABLE_XID (info->owner) == event->window &&
	       event->serial >= info->serial))
	    {
	      owner_list = g_slist_remove (owner_list, info);
	      g_free (info);
	      return TRUE;
	    }
	  else
	    return FALSE;
	}
      tmp_list = tmp_list->next;
    }

  return FALSE;
}
/**
 * gdk_selection_owner_set_for_display:
 * @display : the #GdkDisplay.
 * @owner : a GdkWindow or NULL to indicate that the the owner for
 * the given should be unset.
 * @selection : an atom identifying a selection.
 * @time : timestamp to use when setting the selection. 
 * If this is older than the timestamp given last time the owner was 
 * set for the given selection, the request will be ignored.
 * @send_event : if TRUE, and the new owner is different from the current
 * owner, the current owner will be sent a SelectionClear event.
 *
 * Sets the #GdkWindow @owner as the current owner of the selection @selection.
 * 
 * Returns : TRUE if the selection owner was succesfully changed to owner,
 *	     otherwise FALSE. 
 */
gboolean
gdk_selection_owner_set_for_display (GdkDisplay *display,
				     GdkWindow  *owner,
				     GdkAtom     selection,
				     guint32     time, 
				     gboolean    send_event)
{
  Display *xdisplay;
  Window xwindow;
  Atom xselection;
  GSList *tmp_list;
  OwnerInfo *info;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  if (owner) 
    {
      if (GDK_WINDOW_DESTROYED (owner))
	return FALSE;
      
      xdisplay = GDK_WINDOW_XDISPLAY (owner);
      xwindow = GDK_WINDOW_XID (owner);
    }
  else 
    {
      xdisplay = GDK_DISPLAY_XDISPLAY (display);
      xwindow = None;
    }
  
  xselection = gdk_x11_atom_to_xatom_for_display (display, selection);

  tmp_list = owner_list;
  while (tmp_list)
    {
      info = tmp_list->data;
      if (info->selection == selection) 
	{
	  owner_list = g_slist_remove (owner_list, info);
	  g_free (info);
	  break;
	}
      tmp_list = tmp_list->next;
    }

  if (owner)
    {
      info = g_new (OwnerInfo, 1);
      info->owner = owner;
      info->serial = NextRequest (GDK_WINDOW_XDISPLAY (owner));
      info->selection = selection;

      owner_list = g_slist_prepend (owner_list, info);
    }

  XSetSelectionOwner (xdisplay, xselection, xwindow, time);

  return (XGetSelectionOwner (xdisplay, xselection) == xwindow);
}

gboolean
gdk_selection_owner_set (GdkWindow *owner,
			 GdkAtom    selection,
			 guint32    time,
			 gboolean   send_event)
{
  return gdk_selection_owner_set_for_display (gdk_get_default_display (),
					      owner, selection, 
					      time, send_event);
}

/**
 * gdk_selection_owner_get_for_display :
 * @display : a #GdkDisplay.
 * @selection : an atom indentifying a selection.
 *
 * Determine the owner of the given selection.
 *
 * <para> Note that the return value may be owned by a different 
 * process if a foreign window was previously created for that
 * window, but a new foreign window will never be created by this call. 
 * </para>
 *
 * Returns :if there is a selection owner for this window,
 * and it is a window known to the current process, the GdkWindow that owns 
 * the selection, otherwise NULL.
 */ 

GdkWindow *
gdk_selection_owner_get_for_display (GdkDisplay *display,
				     GdkAtom     selection)
{
  Window xwindow;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  xwindow = XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
				gdk_x11_atom_to_xatom_for_display (display, 
								   selection));
  if (xwindow == None)
    return NULL;

  return gdk_window_lookup_for_display (display, xwindow);
}

GdkWindow*
gdk_selection_owner_get (GdkAtom selection)
{
  return gdk_selection_owner_get_for_display (gdk_get_default_display (), 
					      selection);
}

void
gdk_selection_convert (GdkWindow *requestor,
		       GdkAtom    selection,
		       GdkAtom    target,
		       guint32    time)
{
  GdkDisplay *display;
  
  if (GDK_WINDOW_DESTROYED (requestor))
    return;

  display = GDK_WINDOW_DISPLAY (requestor);

  XConvertSelection (GDK_WINDOW_XDISPLAY (requestor),
		     gdk_x11_atom_to_xatom_for_display (display, selection),
		     gdk_x11_atom_to_xatom_for_display (display, target),
		     gdk_x11_atom_to_xatom_for_display (display, _gdk_selection_property), 
		     GDK_WINDOW_XID (requestor), time);
}

gint
gdk_selection_property_get (GdkWindow  *requestor,
			    guchar    **data,
			    GdkAtom    *ret_type,
			    gint       *ret_format)
{
  gulong nitems;
  gulong nbytes;
  gulong length;
  Atom prop_type;
  gint prop_format;
  guchar *t = NULL;
  GdkDisplay *display; 

  g_return_val_if_fail (requestor != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (requestor), 0);
  
  display = GDK_WINDOW_DISPLAY (requestor);

  /* If retrieved chunks are typically small, (and the ICCCM says the
     should be) it would be a win to try first with a buffer of
     moderate length, to avoid two round trips to the server */

  if (GDK_WINDOW_DESTROYED (requestor))
    return 0;

  t = NULL;
  XGetWindowProperty (GDK_WINDOW_XDISPLAY (requestor),
		      GDK_WINDOW_XID (requestor),
		      gdk_x11_atom_to_xatom_for_display (display, _gdk_selection_property),
		      0, 0, False,
		      AnyPropertyType, &prop_type, 
		      &prop_format, &nitems, &nbytes, &t);
  if (ret_type)
    *ret_type = gdk_x11_xatom_to_atom_for_display (display, prop_type);
  if (ret_format)
    *ret_format = prop_format;

  if (prop_type == None)
    {
      *data = NULL;
      return 0;
    }
  
  if (t)
    {
      XFree (t);
      t = NULL;
    }

  /* Add on an extra byte to handle null termination.  X guarantees
     that t will be 1 longer than nbytes and null terminated */
  length = nbytes + 1;

  /* We can't delete the selection here, because it might be the INCR
     protocol, in which case the client has to make sure they'll be
     notified of PropertyChange events _before_ the property is deleted.
     Otherwise there's no guarantee we'll win the race ... */
  XGetWindowProperty (GDK_DRAWABLE_XDISPLAY (requestor),
		      GDK_DRAWABLE_XID (requestor),
		      gdk_x11_atom_to_xatom_for_display (display, _gdk_selection_property),
		      0, (nbytes + 3) / 4, False,
		      AnyPropertyType, &prop_type, &prop_format,
		      &nitems, &nbytes, &t);

  if (prop_type != None)
    {
      *data = g_new (guchar, length);
      
      if (prop_type == XA_ATOM ||
	  prop_type == gdk_x11_get_xatom_by_name_for_display (display, "ATOM_PAIR"))
	{
	  Atom* atoms = (Atom*) t;
	  GdkAtom* atoms_dest;
	  gint num_atom, i;

	  num_atom = (length - 1) / sizeof (Atom);
	  length = sizeof (GdkAtom) * num_atom + 1;
	  *data = g_malloc (length);
	  (*data)[length - 1] = '\0';
	  atoms_dest = (GdkAtom *)(*data);
	  
	  for (i=0; i < num_atom; i++)
	    atoms_dest[i] = gdk_x11_xatom_to_atom_for_display (display, atoms[i]);
	}
      else
	{
	  *data = g_memdup (t, length);
	}
      
      if (t)
	XFree (t);
      return length-1;
    }
  else
    {
      *data = NULL;
      return 0;
    }
}

/**
 * gdk_selection_send_notify_for_display :
 * @display : the #GdkDisplay where @requestor is realized
 * @requestor : window to which to deliver response.
 * @selection : selection that was requested.
 * @target : target that was selected.
 * @property : property in which the selection owner stored the data,
 * or GDK_NONE to indicate that the request was rejected.
 * @time : timestamp. 
 *
 * Send a response to SelectionRequest event.
 **/
void
gdk_selection_send_notify_for_display (GdkDisplay *display,
				       guint32     requestor,
				       GdkAtom     selection,
				       GdkAtom     target,
				       GdkAtom     property, 
				       guint32     time)
{
  XSelectionEvent xevent;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));

  xevent.type = SelectionNotify;
  xevent.serial = 0;
  xevent.send_event = True;
  xevent.requestor = requestor;
  xevent.selection = gdk_x11_atom_to_xatom_for_display (display, selection);
  xevent.target = gdk_x11_atom_to_xatom_for_display (display, target);
  xevent.property = gdk_x11_atom_to_xatom_for_display (display, property);
  xevent.time = time;

  _gdk_send_xevent (display, requestor, False, NoEventMask, (XEvent*) & xevent);
}

void
gdk_selection_send_notify (guint32  requestor,
			   GdkAtom  selection,
			   GdkAtom  target,
			   GdkAtom  property,
			   guint32  time)
{
  gdk_selection_send_notify_for_display (gdk_get_default_display (), 
					 requestor, selection, 
					 target, property, time);
}

/**
 * gdk_text_property_to_text_list_for_display:
 * @display: The #GdkDisplay where the encoding is defined.
 * @encoding: an atom representing the encoding. The most 
 * common values for this are STRING, or COMPOUND_TEXT. 
 * This is value used as the type for the property.
 * @format: the format of the property.
 * @text: The text data.
 * @length: The number of items to transform.
 * @list: location to store a terminated array of strings in 
 * the encoding of the current locale. This array should be 
 * freed using gdk_free_text_list().
 *
 * Convert a text string from the encoding as it is stored 
 * in a property into an array of strings in the encoding of
 * the current local. (The elements of the array represent the
 * null-separated elements of the original text string.)
 *
 * Returns : he number of strings stored in list, or 0, 
 * if the conversion failed. 
 */
gint
gdk_text_property_to_text_list_for_display (GdkDisplay   *display,
					    GdkAtom       encoding,
					    gint          format, 
					    const guchar *text,
					    gint          length,
					    gchar      ***list)
{
  XTextProperty property;
  gint count = 0;
  gint res;
  gchar **local_list;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  property.value = (guchar *)text;
  property.encoding = gdk_x11_atom_to_xatom_for_display (display, encoding);
  property.format = format;
  property.nitems = length;
  res = XmbTextPropertyToTextList (GDK_DISPLAY_XDISPLAY (display), &property, 
				   &local_list, &count);

  if (res == XNoMemory || res == XLocaleNotSupported || 
      res == XConverterNotFound)
    {
      if (list)
	*list = NULL;

      return 0;
    }
  else
    {
      if (list)
	*list = local_list;
      else
	XFreeStringList (local_list);
      
      return count;
    }
}

gint
gdk_text_property_to_text_list (GdkAtom       encoding,
				gint          format, 
				const guchar *text,
				gint          length,
				gchar      ***list)
{
  return gdk_text_property_to_text_list_for_display (gdk_get_default_display (),
						     encoding, format, text, length, list);
}

void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  XFreeStringList (list);
}

static gint
make_list (const gchar  *text,
	   gint          length,
	   gboolean      latin1,
	   gchar      ***list)
{
  GSList *strings = NULL;
  gint n_strings = 0;
  gint i;
  const gchar *p = text;
  const gchar *q;
  GSList *tmp_list;
  GError *error = NULL;

  while (p < text + length)
    {
      gchar *str;
      
      q = p;
      while (*q && q < text + length)
	q++;

      if (latin1)
	{
	  str = g_convert (p, q - p,
			   "UTF-8", "ISO-8859-1",
			   NULL, NULL, &error);

	  if (!str)
	    {
	      g_warning ("Error converting selection from STRING: %s",
			 error->message);
	      g_error_free (error);
	    }
	}
      else
	str = g_strndup (p, q - p);

      if (str)
	{
	  strings = g_slist_prepend (strings, str);
	  n_strings++;
	}

      p = q + 1;
    }

  if (list)
    *list = g_new (gchar *, n_strings + 1);

  (*list)[n_strings] = NULL;
  
  i = n_strings;
  tmp_list = strings;
  while (tmp_list)
    {
      if (list)
	(*list)[--i] = tmp_list->data;
      else
	g_free (tmp_list->data);

      tmp_list = tmp_list->next;
    }

  g_slist_free (strings);

  return n_strings;
}

/**
 * gdk_text_property_to_utf8_list_for_display:
 * @display:  a #GdkDisplay
 * @encoding: an atom representing the encoding of the text
 * @format:   the format of the property
 * @text:     the text to convert
 * @length:   the length of @text, in bytes
 * @list:     location to store the list of strings or %NULL. The
 *            list should be freed with g_strfreev().
 * 
 * Converts a text property in the giving encoding to
 * a list of UTF-8 strings. 
 * 
 * Return value: the number of strings in the resulting
 *               list.
 **/
gint 
gdk_text_property_to_utf8_list_for_display (GdkDisplay    *display,
					    GdkAtom        encoding,
					    gint           format,
					    const guchar  *text,
					    gint           length,
					    gchar       ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);
  
  if (encoding == GDK_TARGET_STRING)
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == gdk_atom_intern ("UTF8_STRING", FALSE))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar **local_list;
      gint local_count;
      gint i;
      const gchar *charset = NULL;
      gboolean need_conversion = !g_get_charset (&charset);
      gint count = 0;
      GError *error = NULL;
      
      /* Probably COMPOUND text, we fall back to Xlib routines
       */
      local_count = gdk_text_property_to_text_list_for_display (display,
								encoding,
								format, 
								text,
								length,
								&local_list);
      if (list)
	*list = g_new (gchar *, local_count + 1);
      
      for (i=0; i<local_count; i++)
	{
	  /* list contains stuff in our default encoding
	   */
	  if (need_conversion)
	    {
	      gchar *utf = g_convert (local_list[i], -1,
				      "UTF-8", charset,
				      NULL, NULL, &error);
	      if (utf)
		{
		  if (list)
		    (*list)[count++] = utf;
		  else
		    g_free (utf);
		}
	      else
		{
		  g_warning ("Error converting to UTF-8 from '%s': %s",
			     charset, error->message);
		  g_error_free (error);
		  error = NULL;
		}
	    }
	  else
	    {
	      if (list)
		(*list)[count++] = g_strdup (local_list[i]);
	    }
	}

      if (local_count)
	gdk_free_text_list (local_list);
      
      (*list)[count] = NULL;

      return count;
    }
}

/**
 * gdk_text_property_to_utf8_list:
 * @encoding: an atom representing the encoding of the text
 * @format:   the format of the property
 * @text:     the text to convert
 * @length:   the length of @text, in bytes
 * @list:     location to store the list of strings or %NULL. The
 *            list should be freed with g_strfreev().
 * 
 * Convert a text property in the giving encoding to
 * a list of UTF-8 strings. 
 * 
 * Return value: the number of strings in the resulting
 *               list.
 **/
gint 
gdk_text_property_to_utf8_list (GdkAtom        encoding,
				gint           format,
				const guchar  *text,
				gint           length,
				gchar       ***list)
{
  return gdk_text_property_to_utf8_list_for_display (gdk_get_default_display (),
						     encoding, format, text, length, list);
}

/**
 * gdk_string_to_compound_text_for_display:
 * @display : the #GdkDisplay where the encoding is defined.
 * @str	    : a null-terminated string.
 * @encoding: location to store the encoding atom 
 *	      (to be used as the type for the property).
 * @format:   location to store the format of the property
 * @ctext:    location to store newly allocated data for the property.
 * @length:   the length of @text, in bytes
 * 
 * Convert a string from the encoding of the current 
 * locale into a form suitable for storing in a window property.
 * 
 * Returns : 0 upon sucess, non-zero upon failure. 
 **/
gint
gdk_string_to_compound_text_for_display (GdkDisplay  *display,
					 const gchar *str,
					 GdkAtom     *encoding,
					 gint        *format,
					 guchar     **ctext,
					 gint        *length)
{
  gint res;
  XTextProperty property;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  res = XmbTextListToTextProperty (GDK_DISPLAY_XDISPLAY (display), 
				   (char **)&str, 1, XCompoundTextStyle,
                               	   &property);
  if (res != Success)
    {
      property.encoding = None;
      property.format = None;
      property.value = NULL;
      property.nitems = 0;
    }

  if (encoding)
    *encoding = gdk_x11_xatom_to_atom_for_display (display, property.encoding);
  if (format)
    *format = property.format;
  if (ctext)
    *ctext = property.value;
  if (length)
    *length = property.nitems;

  return res;
}

gint
gdk_string_to_compound_text (const gchar *str,
			     GdkAtom     *encoding,
			     gint        *format,
			     guchar     **ctext,
			     gint        *length)
{
  return gdk_string_to_compound_text_for_display (gdk_get_default_display (),
						  str, encoding, format, 
						  ctext, length);
}

/* The specifications for COMPOUND_TEXT and STRING specify that C0 and
 * C1 are not allowed except for \n and \t, however the X conversions
 * routines for COMPOUND_TEXT only enforce this in one direction,
 * causing cut-and-paste of \r and \r\n separated text to fail.
 * This routine strips out all non-allowed C0 and C1 characters
 * from the input string and also canonicalizes \r, and \r\n to \n
 */
static gchar * 
sanitize_utf8 (const gchar *src)
{
  gint len = strlen (src);
  GString *result = g_string_sized_new (len);
  const gchar *p = src;

  while (*p)
    {
      if (*p == '\r')
	{
	  p++;
	  if (*p == '\n')
	    p++;

	  g_string_append_c (result, '\n');
	}
      else
	{
	  gunichar ch = g_utf8_get_char (p);
	  char buf[7];
	  gint buflen;
	  
	  if (!((ch < 0x20 && ch != '\t' && ch != '\n') || (ch >= 0x7f && ch < 0xa0)))
	    {
	      buflen = g_unichar_to_utf8 (ch, buf);
	      g_string_append_len (result, buf, buflen);
	    }

	  p = g_utf8_next_char (p);
	}
    }

  return g_string_free (result, FALSE);
}

/**
 * gdk_utf8_to_string_target:
 * @str: a UTF-8 string
 * 
 * Converts an UTF-8 string into the best possible representation
 * as a STRING. The representation of characters not in STRING
 * is not specified; it may be as pseudo-escape sequences
 * \x{ABCD}, or it may be in some other form of approximation.
 * 
 * Return value: the newly-allocated string, or %NULL if the
 *               conversion failed. (It should not fail for
 *               any properly formed UTF-8 string unless system
 *               limits like memory or file descriptors are exceeded.)
 **/
gchar *
gdk_utf8_to_string_target (const gchar *str)
{
  GError *error = NULL;
  
  gchar *tmp_str = sanitize_utf8 (str);
  gchar *result =  g_convert_with_fallback (tmp_str, -1,
					    "ISO-8859-1", "UTF-8",
					    NULL, NULL, NULL, &error);
  if (!result)
    {
      g_warning ("Error converting from UTF-8 to STRING: %s",
		 error->message);
      g_error_free (error);
    }
  
  g_free (tmp_str);
  return result;
}

/**
 * gdk_utf8_to_compound_text_for_display:
 * @display:  a #GdkDisplay
 * @str:      a UTF-8 string
 * @encoding: location to store resulting encoding
 * @format:   location to store format of the result
 * @ctext:    location to store the data of the result
 * @length:   location to store the length of the data
 *            stored in @ctext
 * 
 * Converts from UTF-8 to compound text. 
 * 
 * Return value: %TRUE if the conversion succeeded, otherwise
 *               %FALSE.
 **/
gboolean
gdk_utf8_to_compound_text_for_display (GdkDisplay  *display,
				       const gchar *str,
				       GdkAtom     *encoding,
				       gint        *format,
				       guchar     **ctext,
				       gint        *length)
{
  gboolean need_conversion;
  const gchar *charset;
  gchar *locale_str, *tmp_str;
  GError *error = NULL;
  gboolean result;

  g_return_val_if_fail (str != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  need_conversion = !g_get_charset (&charset);

  tmp_str = sanitize_utf8 (str);

  if (need_conversion)
    {
      locale_str = g_convert_with_fallback (tmp_str, -1,
					    charset, "UTF-8",
					    NULL, NULL, NULL, &error);
      g_free (tmp_str);

      if (!locale_str)
	{
	  g_warning ("Error converting from UTF-8 to '%s': %s",
		     charset, error->message);
	  g_error_free (error);

	  if (encoding)
	    *encoding = None;
	  if (format)
	    *format = None;
	  if (ctext)
	    *ctext = NULL;
	  if (length)
	    *length = 0;

	  return FALSE;
	}
    }
  else
    locale_str = tmp_str;
    
  result = gdk_string_to_compound_text_for_display (display, locale_str,
						    encoding, format, 
						    ctext, length);
  result = (result == Success? TRUE : FALSE);
  
  g_free (locale_str);

  return result;
}

/**
 * gdk_utf8_to_compound_text:
 * @str:      a UTF-8 string
 * @encoding: location to store resulting encoding
 * @format:   location to store format of the result
 * @ctext:    location to store the data of the result
 * @length:   location to store the length of the data
 *            stored in @ctext
 * 
 * Convert from UTF-8 to compound text. 
 * 
 * Return value: %TRUE if the conversion succeeded, otherwise
 *               false.
 **/
gboolean
gdk_utf8_to_compound_text (const gchar *str,
			   GdkAtom     *encoding,
			   gint        *format,
			   guchar     **ctext,
			   gint        *length)
{
  return gdk_utf8_to_compound_text_for_display (gdk_get_default_display (),
						str, encoding, format, 
						ctext, length);
}

void gdk_free_compound_text (guchar *ctext)
{
  if (ctext)
    XFree (ctext);
}
