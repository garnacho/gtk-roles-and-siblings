/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <string.h>
#include <time.h>

#include "gdkfb.h"
#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkprivate-fb.h"

GdkAtom
gdk_atom_intern (const gchar *atom_name,
		 gboolean     only_if_exists)
{
  g_return_val_if_fail (atom_name != NULL, GDK_NONE);

  return g_quark_from_string(atom_name);
}

gchar*
gdk_atom_name (GdkAtom atom)
{
  return g_quark_to_string(atom);
}

static void
gdk_property_delete_2 (GdkWindow *window,
		       GdkAtom property,
		       GdkWindowProperty *prop)
{
  GdkWindowFBData *fbd = GDK_WINDOW_IMPL_FBDATA(window);
  GdkEvent *event;
  
  g_hash_table_remove(fbd->properties, GUINT_TO_POINTER(property));
  g_free(prop);

  event = gdk_event_make(window, GDK_PROPERTY_NOTIFY, TRUE);
  if(event)
    {
      event->property.atom = property;
      event->property.state = GDK_PROPERTY_DELETE;
    }
}

void
gdk_property_delete (GdkWindow *window,
		     GdkAtom    property)
{
  GdkWindowFBData *fbd = GDK_WINDOW_FBDATA(window);
  GdkWindowProperty *prop;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if(!fbd->properties)
    return;

  prop = g_hash_table_lookup(fbd->properties, GUINT_TO_POINTER(property));
  if(!prop)
    return;

  gdk_property_delete_2(window, property, prop);
}

gint
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
  GdkWindowFBData *fbd = GDK_WINDOW_FBDATA(window);
  GdkWindowProperty *prop;
  int nbytes;

  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (actual_length != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if(!fbd->properties)
    return FALSE;

  prop = g_hash_table_lookup(fbd->properties, GUINT_TO_POINTER(property));
  if(!prop)
    return FALSE;

  nbytes = (offset + length * (prop->format >> 3)) - prop->length;
  nbytes = MAX(nbytes, 0);
  if(nbytes > 0)
    {
      *data = g_malloc(nbytes+1);
      memcpy(data, prop->data + offset, nbytes);
      (*data)[nbytes] = 0;
    }
  else
    *data = NULL;
  *actual_length = nbytes / (prop->format >> 3);
  *actual_property_type = prop->type;
  *actual_format_type = prop->format;

  if(pdelete)
    gdk_property_delete_2(window, property, prop);

  return TRUE;
}

void
gdk_property_change (GdkWindow   *window,
		     GdkAtom      property,
		     GdkAtom      type,
		     gint         format,
		     GdkPropMode  mode,
		     const guchar *data,
		     gint         nelements)
{
  GdkWindowFBData *fbd = GDK_WINDOW_FBDATA(window);
  GdkWindowProperty *prop, *new_prop;
  int new_size;
  GdkEvent *event;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if(!fbd->properties)
    fbd->properties = g_hash_table_new(NULL, NULL);

  prop = g_hash_table_lookup(fbd->properties, GUINT_TO_POINTER(property));

  switch(mode)
    {
    case GDK_PROP_MODE_REPLACE:
      new_size = nelements * (format >> 3);
      break;
    case GDK_PROP_MODE_PREPEND:
    case GDK_PROP_MODE_APPEND:
      new_size = nelements * (format >> 3);
      if(prop)
	new_size += prop->length;
    default:
      break;
    }

  new_prop = g_malloc(G_STRUCT_OFFSET(GdkWindowProperty, data) + new_size);
  new_prop->length = new_size;
  new_prop->type = type;
  new_prop->format = format;

  switch(mode)
    {
    case GDK_PROP_MODE_REPLACE:
      memcpy(new_prop->data, data, new_size);
      break;
    case GDK_PROP_MODE_APPEND:
      if(prop)
	memcpy(new_prop->data, prop->data, prop->length);
      memcpy(new_prop->data + prop->length, data, (nelements * (format >> 3)));
      break;
    case GDK_PROP_MODE_PREPEND:
      memcpy(new_prop->data, data, (nelements * (format >> 3)));
      if(prop)
	memcpy(new_prop->data + (nelements * (format >> 3)), prop->data, prop->length);
      break;
    }

  g_hash_table_insert(fbd->properties, GUINT_TO_POINTER(property), new_prop);
  g_free(prop);

  event = gdk_event_make(window, GDK_PROPERTY_NOTIFY, TRUE);
  if(event)
    {
      event->property.atom = property;
      event->property.state = GDK_PROPERTY_NEW_VALUE;
    }
}
