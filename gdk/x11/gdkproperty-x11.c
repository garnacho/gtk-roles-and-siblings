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

#include "gdk.h"          /* For gdk_error_trap_push/pop() */
#include "gdkx.h"
#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"
#include "gdkselection.h"	/* only from predefined atom */

static GPtrArray *virtual_atom_array;
static GHashTable *virtual_atom_hash;

static gchar *XAtomsStrings[] = {
  /* These are all the standard predefined X atoms */
  "NONE",
  "PRIMARY",
  "SECONDARY",
  "ARC",
  "ATOM",
  "BITMAP",
  "CARDINAL",
  "COLORMAP",
  "CURSOR",
  "CUT_BUFFER0",
  "CUT_BUFFER1",
  "CUT_BUFFER2",
  "CUT_BUFFER3",
  "CUT_BUFFER4",
  "CUT_BUFFER5",
  "CUT_BUFFER6",
  "CUT_BUFFER7",
  "DRAWABLE",
  "FONT",
  "INTEGER",
  "PIXMAP",
  "POINT",
  "RECTANGLE",
  "RESOURCE_MANAGER",
  "RGB_COLOR_MAP",
  "RGB_BEST_MAP",
  "RGB_BLUE_MAP",
  "RGB_DEFAULT_MAP",
  "RGB_GRAY_MAP",
  "RGB_GREEN_MAP",
  "RGB_RED_MAP",
  "STRING",
  "VISUALID",
  "WINDOW",
  "WM_COMMAND",
  "WM_HINTS",
  "WM_CLIENT_MACHINE",
  "WM_ICON_NAME",
  "WM_ICON_SIZE",
  "WM_NAME",
  "WM_NORMAL_HINTS",
  "WM_SIZE_HINTS",
  "WM_ZOOM_HINTS",
  "MIN_SPACE",
  "NORM_SPACE",
  "MAX_SPACE",
  "END_SPACE",
  "SUPERSCRIPT_X",
  "SUPERSCRIPT_Y",
  "SUBSCRIPT_X",
  "SUBSCRIPT_Y",
  "UNDERLINE_POSITION",
  "UNDERLINE_THICKNESS",
  "STRIKEOUT_ASCENT",
  "STRIKEOUT_DESCENT",
  "ITALIC_ANGLE",
  "X_HEIGHT",
  "QUAD_WIDTH",
  "WEIGHT",
  "POINT_SIZE",
  "RESOLUTION",
  "COPYRIGHT",
  "NOTICE",
  "FONT_NAME",
  "FAMILY_NAME",
  "FULL_NAME",
  "CAP_HEIGHT",
  "WM_CLASS",
  "WM_TRANSIENT_FOR",
  /* Below here, these are our additions. Increment N_CUSTOM_PREDEFINED
   * if you add any.
   */
  "CLIPBOARD"			/* = 69 */
};

#define N_CUSTOM_PREDEFINED 1

#define ATOM_TO_INDEX(atom) (GPOINTER_TO_UINT(atom))
#define INDEX_TO_ATOM(atom) ((GdkAtom)GUINT_TO_POINTER(atom))

static void
insert_atom_pair (GdkDisplay *display,
		  GdkAtom     virtual_atom,
		  Atom        xatom)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);  
  
  if (!display_x11->atom_from_virtual)
    {
      display_x11->atom_from_virtual = g_hash_table_new (g_direct_hash, NULL);
      display_x11->atom_to_virtual = g_hash_table_new (g_direct_hash, NULL);
    }
  
  g_hash_table_insert (display_x11->atom_from_virtual, 
		       GDK_ATOM_TO_POINTER (virtual_atom), 
		       GUINT_TO_POINTER (xatom));
  g_hash_table_insert (display_x11->atom_to_virtual,
		       GUINT_TO_POINTER (xatom), 
		       GDK_ATOM_TO_POINTER (virtual_atom));
}
/**
 * gdk_x11_atom_to_xatom_for_display:
 * @display: A #GdkDisplay
 * @atom: A #GdkAtom 
 * 
 * Converts from a #GdkAtom to the X atom for a #GdkDisplay
 * with the same string value.
 * 
 * Return value: the X atom corresponding to @atom.
 *
 * Since: 2.2
 **/
Atom
gdk_x11_atom_to_xatom_for_display (GdkDisplay *display, 
				   GdkAtom atom)
{
  GdkDisplayX11 *display_x11;
  Atom xatom = None;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), None);

  if (display->closed)
    return None;
  
  display_x11 = GDK_DISPLAY_X11 (display); 
  
  if (ATOM_TO_INDEX (atom) < G_N_ELEMENTS (XAtomsStrings) - N_CUSTOM_PREDEFINED)
    return ATOM_TO_INDEX (atom);
  
  if (display_x11->atom_from_virtual)
    xatom = GPOINTER_TO_UINT (g_hash_table_lookup (display_x11->atom_from_virtual,
						   GDK_ATOM_TO_POINTER (atom)));
  if (!xatom)
    {
      char *name;
      
      g_return_val_if_fail (ATOM_TO_INDEX (atom) < virtual_atom_array->len, None);

      name = g_ptr_array_index (virtual_atom_array, ATOM_TO_INDEX (atom));
      
      xatom = XInternAtom (GDK_DISPLAY_XDISPLAY (display), name, FALSE);
      insert_atom_pair (display, atom, xatom);
    }

  return xatom;
}

/**
 * gdk_x11_atom_to_xatom:
 * @atom: A #GdkAtom 
 * 
 * Converts from a #GdkAtom to the X atom for the default GDK display
 * with the same string value.
 * 
 * Return value: the X atom corresponding to @atom.
 **/
Atom
gdk_x11_atom_to_xatom (GdkAtom atom)
{
  return gdk_x11_atom_to_xatom_for_display (gdk_display_get_default (), atom);
}

/**
 * gdk_x11_xatom_to_atom_for_display:
 * @display: A #GdkDisplay
 * @xatom: an X atom 
 * 
 * Convert from an X atom for a #GdkDisplay to the corresponding
 * #GdkAtom.
 * 
 * Return value: the corresponding #GdkAtom.
 *
 * Since: 2.2
 **/
GdkAtom
gdk_x11_xatom_to_atom_for_display (GdkDisplay *display,
				   Atom	       xatom)
{
  GdkDisplayX11 *display_x11;
  GdkAtom virtual_atom = GDK_NONE;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), GDK_NONE);

  if (display->closed)
    return GDK_NONE;

  display_x11 = GDK_DISPLAY_X11 (display);
  
  if (xatom < G_N_ELEMENTS (XAtomsStrings) - N_CUSTOM_PREDEFINED)
    return INDEX_TO_ATOM (xatom);
  
  if (display_x11->atom_to_virtual)
    virtual_atom = GDK_POINTER_TO_ATOM (g_hash_table_lookup (display_x11->atom_to_virtual,
							     GUINT_TO_POINTER (xatom)));
  
  if (!virtual_atom)
    {
      /* If this atom doesn't exist, we'll die with an X error unless
       * we take precautions
       */
      char *name;
      gdk_error_trap_push ();
      name = XGetAtomName (GDK_DISPLAY_XDISPLAY (display), xatom);
      if (gdk_error_trap_pop ())
	{
	  g_warning (G_STRLOC " invalid X atom: %ld", xatom);
	}
      else
	{
	  virtual_atom = gdk_atom_intern (name, FALSE);
	  XFree (name);
	  
	  insert_atom_pair (display, virtual_atom, xatom);
	}
    }

  return virtual_atom;
}

/**
 * gdk_x11_xatom_to_atom:
 * @xatom: an X atom for the default GDK display
 * 
 * Convert from an X atom for the default display to the corresponding
 * #GdkAtom.
 * 
 * Return value: the corresponding G#dkAtom.
 **/
GdkAtom
gdk_x11_xatom_to_atom (Atom xatom)
{
  return gdk_x11_xatom_to_atom_for_display (gdk_display_get_default (), xatom);
}

static void
virtual_atom_check_init (void)
{
  if (!virtual_atom_hash)
    {
      gint i;
      
      virtual_atom_hash = g_hash_table_new (g_str_hash, g_str_equal);
      virtual_atom_array = g_ptr_array_new ();
      
      for (i = 0; i < G_N_ELEMENTS (XAtomsStrings); i++)
	{
	  g_ptr_array_add (virtual_atom_array, XAtomsStrings[i]);
	  g_hash_table_insert (virtual_atom_hash, XAtomsStrings[i],
			       GUINT_TO_POINTER (i));
	}
    }
}

GdkAtom
gdk_atom_intern (const gchar *atom_name, 
		 gboolean     only_if_exists)
{
  GdkAtom result;

  virtual_atom_check_init ();
  
  result = GDK_POINTER_TO_ATOM (g_hash_table_lookup (virtual_atom_hash, atom_name));
  if (!result)
    {
      result = INDEX_TO_ATOM (virtual_atom_array->len);
      
      g_ptr_array_add (virtual_atom_array, g_strdup (atom_name));
      g_hash_table_insert (virtual_atom_hash, 
			   g_ptr_array_index (virtual_atom_array,
					      ATOM_TO_INDEX (result)),
			   GDK_ATOM_TO_POINTER (result));
    }

  return result;
}

static G_CONST_RETURN char *
get_atom_name (GdkAtom atom)
{
  virtual_atom_check_init ();

  if (ATOM_TO_INDEX (atom) < virtual_atom_array->len)
    return g_ptr_array_index (virtual_atom_array, ATOM_TO_INDEX (atom));
  else
    return NULL;
}

gchar *
gdk_atom_name (GdkAtom atom)
{
  return g_strdup (get_atom_name (atom));
}

/**
 * gdk_x11_get_xatom_by_name_for_display:
 * @display: a #GdkDisplay
 * @atom_name: a string
 * 
 * Returns the X atom for a #GdkDisplay corresponding to @atom_name.
 * This function caches the result, so if called repeatedly it is much
 * faster than XInternAtom(), which is a round trip to the server each time.
 * 
 * Return value: a X atom for a #GdkDisplay
 *
 * Since: 2.2
 **/
Atom
gdk_x11_get_xatom_by_name_for_display (GdkDisplay  *display,
				       const gchar *atom_name)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), None);
  return gdk_x11_atom_to_xatom_for_display (display,
					    gdk_atom_intern (atom_name, FALSE));
}

/**
 * gdk_x11_get_xatom_by_name:
 * @atom_name: a string
 * 
 * Returns the X atom for GDK's default display corresponding to @atom_name.
 * This function caches the result, so if called repeatedly it is much
 * faster than <function>XInternAtom()</function>, which is a round trip to 
 * the server each time.
 * 
 * Return value: a X atom for GDK's default display.
 **/
Atom
gdk_x11_get_xatom_by_name (const gchar *atom_name)
{
  return gdk_x11_get_xatom_by_name_for_display (gdk_display_get_default (),
						atom_name);
}

/**
 * gdk_x11_get_xatom_name_for_display:
 * @display: the #GdkDisplay where @xatom is defined
 * @xatom: an X atom 
 * 
 * Returns the name of an X atom for its display. This
 * function is meant mainly for debugging, so for convenience, unlike
 * XAtomName() and gdk_atom_name(), the result doesn't need to
 * be freed. 
 *
 * Return value: name of the X atom; this string is owned by GDK,
 *   so it shouldn't be modifed or freed. 
 *
 * Since: 2.2
 **/
G_CONST_RETURN gchar *
gdk_x11_get_xatom_name_for_display (GdkDisplay *display,
				    Atom        xatom)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return get_atom_name (gdk_x11_xatom_to_atom_for_display (display, xatom));
}

/**
 * gdk_x11_get_xatom_name:
 * @xatom: an X atom for GDK's default display
 * 
 * Returns the name of an X atom for GDK's default display. This
 * function is meant mainly for debugging, so for convenience, unlike
 * <function>XAtomName()</function> and gdk_atom_name(), the result 
 * doesn't need to be freed. Also, this function will never return %NULL, 
 * even if @xatom is invalid.
 * 
 * Return value: name of the X atom; this string is owned by GTK+,
 *   so it shouldn't be modifed or freed. 
 **/
G_CONST_RETURN gchar *
gdk_x11_get_xatom_name (Atom xatom)
{
  return get_atom_name (gdk_x11_xatom_to_atom (xatom));
}

gboolean
gdk_property_get (GdkWindow   *window,
		  GdkAtom      property,
		  GdkAtom      type,
		  gulong       offset,
		  gulong       length,
		  gint         pdelete,
		  GdkAtom     *actual_property_type,
		  gint        *actual_format_type,
		  gint        *actual_length,
		  guchar     **data)
{
  GdkDisplay *display;
  Atom ret_prop_type;
  gint ret_format;
  gulong ret_nitems;
  gulong ret_bytes_after;
  gulong ret_length;
  guchar *ret_data;
  Atom xproperty;
  Atom xtype;
  int res;

  g_return_val_if_fail (!window || GDK_IS_WINDOW (window), FALSE);

  if (!window)
    {
      GdkScreen *screen = gdk_screen_get_default ();
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD, g_message ("gdk_property_get(): window is NULL\n"));
    }

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  display = gdk_drawable_get_display (window);
  xproperty = gdk_x11_atom_to_xatom_for_display (display, property);
  xtype = gdk_x11_atom_to_xatom_for_display (display, type);

  ret_data = NULL;
  
  res = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
			    GDK_WINDOW_XWINDOW (window), xproperty,
			    offset, (length + 3) / 4, pdelete,
			    xtype, &ret_prop_type, &ret_format,
			    &ret_nitems, &ret_bytes_after,
			    &ret_data);

  if (res != Success || (ret_prop_type == None && ret_format == 0))
    {
      return FALSE;
    }

  if (actual_property_type)
    *actual_property_type = gdk_x11_xatom_to_atom_for_display (display, ret_prop_type);
  if (actual_format_type)
    *actual_format_type = ret_format;

  if ((xtype != AnyPropertyType) && (ret_prop_type != xtype))
    {
      XFree (ret_data);
      g_warning ("Couldn't match property type %s to %s\n", 
		 gdk_x11_get_xatom_name_for_display (display, ret_prop_type), 
		 gdk_x11_get_xatom_name_for_display (display, xtype));
      return FALSE;
    }

  /* FIXME: ignoring bytes_after could have very bad effects */

  if (data)
    {
      if (ret_prop_type == XA_ATOM ||
	  ret_prop_type == gdk_x11_get_xatom_by_name_for_display (display, "ATOM_PAIR"))
	{
	  /*
	   * data is an array of X atom, we need to convert it
	   * to an array of GDK Atoms
	   */
	  gint i;
	  GdkAtom *ret_atoms = g_new (GdkAtom, ret_nitems);
	  Atom *xatoms = (Atom *)ret_data;

	  *data = (guchar *)ret_atoms;

	  for (i = 0; i < ret_nitems; i++)
	    ret_atoms[i] = gdk_x11_xatom_to_atom_for_display (display, xatoms[i]);
	  
	  if (actual_length)
	    *actual_length = ret_nitems * sizeof (GdkAtom);
	}
      else
	{
	  switch (ret_format)
	    {
	    case 8:
	      ret_length = ret_nitems;
	      break;
	    case 16:
	      ret_length = sizeof(short) * ret_nitems;
	      break;
	    case 32:
	      ret_length = sizeof(long) * ret_nitems;
	      break;
	    default:
	      g_warning ("unknown property return format: %d", ret_format);
	      XFree (ret_data);
	      return FALSE;
	    }
	  
	  *data = g_new (guchar, ret_length);
	  memcpy (*data, ret_data, ret_length);
	  if (actual_length)
	    *actual_length = ret_length;
	}
    }

  XFree (ret_data);

  return TRUE;
}

void
gdk_property_change (GdkWindow    *window,
		     GdkAtom       property,
		     GdkAtom       type,
		     gint          format,
		     GdkPropMode   mode,
		     const guchar *data,
		     gint          nelements)
{
  GdkDisplay *display;
  Window xwindow;
  Atom xproperty;
  Atom xtype;

  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  if (!window)
    {
      GdkScreen *screen;
      
      screen = gdk_screen_get_default ();
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD, g_message ("gdk_property_delete(): window is NULL\n"));
    }


  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (window);
  
  xproperty = gdk_x11_atom_to_xatom_for_display (display, property);
  xtype = gdk_x11_atom_to_xatom_for_display (display, type);
  xwindow = GDK_WINDOW_XID (window);

  if (xtype == XA_ATOM ||
      xtype == gdk_x11_get_xatom_by_name_for_display (display, "ATOM_PAIR"))
    {
      /*
       * data is an array of GdkAtom, we need to convert it
       * to an array of X Atoms
       */
      gint i;
      GdkAtom *atoms = (GdkAtom*) data;
      Atom *xatoms;

      xatoms = g_new (Atom, nelements);
      for (i = 0; i < nelements; i++)
	xatoms[i] = gdk_x11_atom_to_xatom_for_display (display, atoms[i]);

      XChangeProperty (GDK_DISPLAY_XDISPLAY (display), xwindow,
		       xproperty, xtype,
		       format, mode, (guchar *)xatoms, nelements);
      g_free (xatoms);
    }
  else
    XChangeProperty (GDK_DISPLAY_XDISPLAY (display), xwindow, xproperty, 
		     xtype, format, mode, (guchar *)data, nelements);
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  if (!window)
    {
      GdkScreen *screen = gdk_screen_get_default ();
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD, 
		g_message ("gdk_property_delete(): window is NULL\n"));
    }

  if (GDK_WINDOW_DESTROYED (window))
    return;

  XDeleteProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XWINDOW (window),
		   gdk_x11_atom_to_xatom_for_display (GDK_WINDOW_DISPLAY (window),
						      property));
}
