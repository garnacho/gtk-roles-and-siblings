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
#include <X11/cursorfont.h>

#include "gdkprivate-x11.h"
#include "gdkcursor.h"
#include "gdkpixmap-x11.h"
#include "gdkx.h"
#include <gdk/gdkpixmap.h>

/**
 * gdk_cursor_new_for_display:
 * @display: the #GdkDisplay for which the cursor will be created
 * @cursor_type: cursor to create
 * 
 * Creates a new cursor from the set of builtin cursors.
 * Some useful ones are:
 * <itemizedlist>
 * <listitem><para>
 *  <inlinegraphic format="PNG" fileref="right_ptr.png"></inlinegraphic> #GDK_RIGHT_PTR (right-facing arrow)
 * </para></listitem>
 * <listitem><para>
 *  <inlinegraphic format="PNG" fileref="crosshair.png"></inlinegraphic> #GDK_CROSSHAIR (crosshair)
 * </para></listitem>
 * <listitem><para>
 *  <inlinegraphic format="PNG" fileref="xterm.png"></inlinegraphic> #GDK_XTERM (I-beam)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="watch.png"></inlinegraphic> #GDK_WATCH (busy)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="fleur.png"></inlinegraphic> #GDK_FLEUR (for moving objects)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="hand1.png"></inlinegraphic> #GDK_HAND1 (a right-pointing hand)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="hand2.png"></inlinegraphic> #GDK_HAND2 (a left-pointing hand)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="left_side.png"></inlinegraphic> #GDK_LEFT_SIDE (resize left side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="right_side.png"></inlinegraphic> #GDK_RIGHT_SIDE (resize right side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="top_left_corner.png"></inlinegraphic> #GDK_TOP_LEFT_CORNER (resize northwest corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="top_right_corner.png"></inlinegraphic> #GDK_TOP_RIGHT_CORNER (resize northeast corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="bottom_left_corner.png"></inlinegraphic> #GDK_BOTTOM_LEFT_CORNER (resize southwest corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="bottom_right_corner.png"></inlinegraphic> #GDK_BOTTOM_RIGHT_CORNER (resize southeast corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="top_side.png"></inlinegraphic> #GDK_TOP_SIDE (resize top side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="bottom_side.png"></inlinegraphic> #GDK_BOTTOM_SIDE (resize bottom side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="sb_h_double_arrow.png"></inlinegraphic> #GDK_SB_H_DOUBLE_ARROW (move vertical splitter)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="sb_v_double_arrow.png"></inlinegraphic> #GDK_SB_V_DOUBLE_ARROW (move horizontal splitter)
 * </para></listitem>
 * </itemizedlist>
 *
 * To make the cursor invisible, use gdk_cursor_new_from_pixmap() to create
 * a cursor with no pixels in it.
 * 
 * Return value: a new #GdkCursor
 *
 * Since: 2.2
 **/
GdkCursor*
gdk_cursor_new_for_display (GdkDisplay    *display,
			    GdkCursorType  cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  Cursor xcursor;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (display->closed)
    xcursor = None;
  else
    xcursor = XCreateFontCursor (GDK_DISPLAY_XDISPLAY (display), cursor_type);
  
  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  cursor = (GdkCursor *) private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;
  
  return cursor;
}

/**
 * gdk_cursor_new_from_pixmap:
 * @source: the pixmap specifying the cursor.
 * @mask: the pixmap specifying the mask, which must be the same size as 
 *    @source.
 * @fg: the foreground color, used for the bits in the source which are 1.
 *    The color does not have to be allocated first. 
 * @bg: the background color, used for the bits in the source which are 0.
 *    The color does not have to be allocated first.
 * @x: the horizontal offset of the 'hotspot' of the cursor. 
 * @y: the vertical offset of the 'hotspot' of the cursor.
 * 
 * Creates a new cursor from a given pixmap and mask. Both the pixmap and mask
 * must have a depth of 1 (i.e. each pixel has only 2 values - on or off).
 * The standard cursor size is 16 by 16 pixels. You can create a bitmap 
 * from inline data as in the below example.
 * 
 * <example><title>Creating a custom cursor</title>
 * <programlisting>
 * /<!-- -->* This data is in X bitmap format, and can be created with the 'bitmap'
 *    utility. *<!-- -->/
 * &num;define cursor1_width 16
 * &num;define cursor1_height 16
 * static unsigned char cursor1_bits[] = {
 *   0x80, 0x01, 0x40, 0x02, 0x20, 0x04, 0x10, 0x08, 0x08, 0x10, 0x04, 0x20,
 *   0x82, 0x41, 0x41, 0x82, 0x41, 0x82, 0x82, 0x41, 0x04, 0x20, 0x08, 0x10,
 *   0x10, 0x08, 0x20, 0x04, 0x40, 0x02, 0x80, 0x01};
 *  
 * static unsigned char cursor1mask_bits[] = {
 *   0x80, 0x01, 0xc0, 0x03, 0x60, 0x06, 0x30, 0x0c, 0x18, 0x18, 0x8c, 0x31,
 *   0xc6, 0x63, 0x63, 0xc6, 0x63, 0xc6, 0xc6, 0x63, 0x8c, 0x31, 0x18, 0x18,
 *   0x30, 0x0c, 0x60, 0x06, 0xc0, 0x03, 0x80, 0x01};
 *  
 *  
 *  GdkCursor *cursor;
 *  GdkPixmap *source, *mask;
 *  GdkColor fg = { 0, 65535, 0, 0 }; /<!-- -->* Red. *<!-- -->/
 *  GdkColor bg = { 0, 0, 0, 65535 }; /<!-- -->* Blue. *<!-- -->/
 *  
 *  
 *  source = gdk_bitmap_create_from_data (NULL, cursor1_bits,
 *                                        cursor1_width, cursor1_height);
 *  mask = gdk_bitmap_create_from_data (NULL, cursor1mask_bits,
 *                                      cursor1_width, cursor1_height);
 *  cursor = gdk_cursor_new_from_pixmap (source, mask, &amp;fg, &amp;bg, 8, 8);
 *  gdk_pixmap_unref (source);
 *  gdk_pixmap_unref (mask);
 *  
 *  
 *  gdk_window_set_cursor (widget->window, cursor);
 * </programlisting>
 * </example>
 *
 * Return value: a new #GdkCursor.
 **/
GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap *source,
			    GdkPixmap *mask,
			    GdkColor  *fg,
			    GdkColor  *bg,
			    gint       x,
			    gint       y)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  Pixmap source_pixmap, mask_pixmap;
  Cursor xcursor;
  XColor xfg, xbg;
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_PIXMAP (source), NULL);
  g_return_val_if_fail (GDK_IS_PIXMAP (mask), NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  source_pixmap = GDK_PIXMAP_XID (source);
  mask_pixmap   = GDK_PIXMAP_XID (mask);
  display = GDK_PIXMAP_DISPLAY (source);

  xfg.pixel = fg->pixel;
  xfg.red = fg->red;
  xfg.blue = fg->blue;
  xfg.green = fg->green;
  xbg.pixel = bg->pixel;
  xbg.red = bg->red;
  xbg.blue = bg->blue;
  xbg.green = bg->green;
  
  if (display->closed)
    xcursor = None;
  else
    xcursor = XCreatePixmapCursor (GDK_DISPLAY_XDISPLAY (display),
				   source_pixmap, mask_pixmap, &xfg, &xbg, x, y);
  private = g_new (GdkCursorPrivate, 1);
  private->display = display;
  private->xcursor = xcursor;
  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;
  
  return cursor;
}

void
_gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivate *private;

  g_return_if_fail (cursor != NULL);
  g_return_if_fail (cursor->ref_count == 0);

  private = (GdkCursorPrivate *) cursor;
  if (!private->display->closed && private->xcursor)
    XFreeCursor (GDK_DISPLAY_XDISPLAY (private->display), private->xcursor);

  g_free (private);
}

/**
 * gdk_x11_cursor_get_xdisplay:
 * @cursor: a #GdkCursor.
 * 
 * Returns the display of a #GdkCursor.
 * 
 * Return value: an Xlib <type>Display*</type>.
 **/
Display *
gdk_x11_cursor_get_xdisplay (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return GDK_DISPLAY_XDISPLAY(((GdkCursorPrivate *)cursor)->display);
}

/**
 * gdk_x11_cursor_get_xcursor:
 * @cursor: a #GdkCursor.
 * 
 * Returns the X cursor belonging to a #GdkCursor.
 * 
 * Return value: an Xlib <type>Cursor</type>.
 **/
Cursor
gdk_x11_cursor_get_xcursor (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, None);

  return ((GdkCursorPrivate *)cursor)->xcursor;
}

/** 
 * gdk_cursor_get_display:
 * @cursor: a #GdkCursor.
 *
 * Returns the display on which the #GdkCursor is defined.
 *
 * Returns: the #GdkDisplay associated to @cursor
 *
 * Since: 2.2
 */

GdkDisplay *
gdk_cursor_get_display (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return ((GdkCursorPrivate *)cursor)->display;
}
