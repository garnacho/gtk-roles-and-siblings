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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <netinet/in.h>
#include "gdk.h"
#include "gdkinput.h"
#include "gdkprivate.h"
#include <stdlib.h>

int nevent_masks = 16;
int event_mask_table[18] =
{
  ExposureMask,
  PointerMotionMask,
  PointerMotionHintMask,
  ButtonMotionMask,
  Button1MotionMask,
  Button2MotionMask,
  Button3MotionMask,
  ButtonPressMask | OwnerGrabButtonMask,
  ButtonReleaseMask | OwnerGrabButtonMask,
  KeyPressMask,
  KeyReleaseMask,
  EnterWindowMask,
  LeaveWindowMask,
  FocusChangeMask,
  StructureNotifyMask,
  PropertyChangeMask,
  0,				/* PROXIMITY_IN */
  0				/* PROXIMTY_OUT */
};


void
gdk_window_init ()
{
  XWindowAttributes xattributes;
  unsigned int width;
  unsigned int height;
  unsigned int border_width;
  unsigned int depth;
  int x, y;

  XGetGeometry (gdk_display, gdk_root_window, &gdk_root_window,
		&x, &y, &width, &height, &border_width, &depth);
  XGetWindowAttributes (gdk_display, gdk_root_window, &xattributes);

  gdk_root_parent.xdisplay = gdk_display;
  gdk_root_parent.xwindow = gdk_root_window;
  gdk_root_parent.window_type = GDK_WINDOW_ROOT;
  gdk_root_parent.window.user_data = NULL;
}

GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowPrivate *private;
  GdkWindowPrivate *parent_private;
  GdkVisual *visual;
  GdkColormap *colormap;
  Display *parent_display;
  Window xparent;
  Visual *xvisual;
  XSetWindowAttributes xattributes;
  long xattributes_mask;
  XSizeHints size_hints;
  XWMHints wm_hints;
  XClassHint *class_hint;
  int x, y, depth;
  unsigned int class;
  char *title;
  int i;

  g_return_val_if_fail (attributes != NULL, NULL);

  if (!parent)
    parent = (GdkWindow*) &gdk_root_parent;

  parent_private = (GdkWindowPrivate*) parent;
  xparent = parent_private->xwindow;
  parent_display = parent_private->xdisplay;

  private = g_new (GdkWindowPrivate, 1);
  window = (GdkWindow*) private;

  private->parent = parent;
  private->xdisplay = parent_display;
  private->destroyed = FALSE;
  private->resize_count = 0;
  private->ref_count = 1;
  xattributes_mask = 0;

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = 0;

  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else
    y = 0;

  private->x = x;
  private->y = y;
  private->width = (attributes->width > 1) ? (attributes->width) : (1);
  private->height = (attributes->height > 1) ? (attributes->height) : (1);
  private->window_type = attributes->window_type;
  private->extension_events = FALSE;
  private->dnd_drag_data_type = None;
  private->dnd_drag_data_typesavail =
    private->dnd_drop_data_typesavail = NULL;
  private->dnd_drop_enabled = private->dnd_drag_enabled =
    private->dnd_drag_accepted = private->dnd_drag_datashow =
    private->dnd_drop_data_numtypesavail =
    private->dnd_drag_data_numtypesavail = 0;
  private->dnd_drag_eventmask = private->dnd_drag_savedeventmask = 0;

  window->user_data = NULL;

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_visual_get_system ();
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;

  xattributes.event_mask = StructureNotifyMask;
  for (i = 0; i < nevent_masks; i++)
    {
      if (attributes->event_mask & (1 << (i + 1)))
	xattributes.event_mask |= event_mask_table[i];
    }

  if (xattributes.event_mask)
    xattributes_mask |= CWEventMask;

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      class = InputOutput;
      depth = visual->depth;

      if (attributes_mask & GDK_WA_COLORMAP)
	colormap = attributes->colormap;
      else
	colormap = gdk_colormap_get_system ();

      xattributes.background_pixel = BlackPixel (gdk_display, gdk_screen);
      xattributes.border_pixel = BlackPixel (gdk_display, gdk_screen);
      xattributes_mask |= CWBorderPixel | CWBackPixel;

      switch (private->window_type)
	{
	case GDK_WINDOW_TOPLEVEL:
	  xattributes.colormap = ((GdkColormapPrivate*) colormap)->xcolormap;
	  xattributes_mask |= CWColormap;

	  xparent = gdk_root_window;
	  break;

	case GDK_WINDOW_CHILD:
	  xattributes.colormap = ((GdkColormapPrivate*) colormap)->xcolormap;
	  xattributes_mask |= CWColormap;
	  break;

	case GDK_WINDOW_DIALOG:
	  xattributes.colormap = ((GdkColormapPrivate*) colormap)->xcolormap;
	  xattributes_mask |= CWColormap;

	  xparent = gdk_root_window;
	  break;

	case GDK_WINDOW_TEMP:
	  xattributes.colormap = ((GdkColormapPrivate*) colormap)->xcolormap;
	  xattributes_mask |= CWColormap;

	  xparent = gdk_root_window;

	  xattributes.save_under = True;
	  xattributes.override_redirect = True;
	  xattributes.cursor = None;
	  xattributes_mask |= CWSaveUnder | CWOverrideRedirect;
	  break;
	case GDK_WINDOW_ROOT:
	  g_error ("cannot make windows of type GDK_WINDOW_ROOT");
	  break;
	case GDK_WINDOW_PIXMAP:
	  g_error ("cannot make windows of type GDK_WINDOW_PIXMAP (use gdk_pixmap_new)");
	  break;
	}
    }
  else
    {
      depth = 1;
      class = InputOnly;
      colormap = NULL;
    }

  private->xwindow = XCreateWindow (private->xdisplay, xparent,
				    x, y, private->width, private->height,
				    0, depth, class, xvisual,
				    xattributes_mask, &xattributes);
  gdk_xid_table_insert (&private->xwindow, window);

  switch (private->window_type)
    {
    case GDK_WINDOW_DIALOG:
      XSetTransientForHint (private->xdisplay, private->xwindow, xparent);
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      XSetWMProtocols (private->xdisplay, private->xwindow, gdk_wm_window_protocols, 2);
      break;
    case GDK_WINDOW_CHILD:
      if ((attributes->wclass == GDK_INPUT_OUTPUT) &&
	  (colormap != gdk_colormap_get_system ()) &&
	  (colormap != gdk_window_get_colormap (gdk_window_get_toplevel (window))))
	{
	  g_print ("adding colormap window\n");
	  gdk_window_add_colormap_windows (window);
	}
      break;
    default:
      break;
    }

  size_hints.flags = PSize | PBaseSize;
  size_hints.width = private->width;
  size_hints.height = private->height;
  size_hints.base_width = private->width;
  size_hints.base_height = private->height;

  wm_hints.flags = InputHint | StateHint | WindowGroupHint;
  wm_hints.window_group = gdk_leader_window;
  wm_hints.input = True;
  wm_hints.initial_state = NormalState;

  XSetWMNormalHints (private->xdisplay, private->xwindow, &size_hints);
  XSetWMHints (private->xdisplay, private->xwindow, &wm_hints);

  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = gdk_progname;

  XmbSetWMProperties (private->xdisplay, private->xwindow,
                      title, title,
                      NULL, 0,
                      NULL, NULL, NULL);

  if (attributes_mask & GDK_WA_WMCLASS)
    {
      class_hint = XAllocClassHint ();
      class_hint->res_name = attributes->wmclass_name;
      class_hint->res_class = attributes->wmclass_class;
      XSetClassHint (private->xdisplay, private->xwindow, class_hint);
      XFree (class_hint);
    }

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  return window;
}

GdkWindow *
gdk_window_foreign_new (guint32 anid)
{
  GdkWindow *window;
  GdkWindowPrivate *private;
  XWindowAttributes attrs;

  private = g_new (GdkWindowPrivate, 1);
  window = (GdkWindow*) private;

  XGetWindowAttributes (gdk_display, anid, &attrs);

  private->parent = NULL;
  private->xwindow = anid;
  private->xdisplay = gdk_display;
  private->x = attrs.x;
  private->y = attrs.y;
  private->width = attrs.width;
  private->height = attrs.height;
  private->resize_count = 0;
  private->ref_count = 1;
  if (anid == attrs.root)
    private->window_type = GDK_WINDOW_ROOT;
  else
    private->window_type = GDK_WINDOW_TOPLEVEL;
  /* the above is probably wrong, but it may not be worth the extra
     X call to get it right */
    
  private->destroyed = FALSE;
  private->extension_events = 0;

  window->user_data = NULL;

  gdk_xid_table_insert (&private->xwindow, window);

  return window;
}

void
gdk_window_destroy (GdkWindow *window)
{
  GdkWindowPrivate *private;
  GdkWindowPrivate *temp_private;
  GdkWindow *temp_window;
  GList *children;
  GList *tmp;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if(private->dnd_drag_data_numtypesavail > 0) 
    {
      free(private->dnd_drag_data_typesavail);
      private->dnd_drag_data_typesavail = NULL;
    }
  if(private->dnd_drop_data_numtypesavail > 0) 
    {
      free(private->dnd_drop_data_typesavail);
      private->dnd_drop_data_typesavail = NULL;
    }
  
  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      if (private->ref_count >= 1)
	private->ref_count -= 1;

      if (!private->destroyed || (private->destroyed == 2))
	{
	  children = gdk_window_get_children (window);
	  tmp = children;

	  while (tmp)
	    {
	      temp_window = tmp->data;
	      tmp = tmp->next;

	      temp_private = (GdkWindowPrivate*) temp_window;
	      if (temp_private && !temp_private->destroyed)
		/* Removes some nice coredumps... /David */
		{
		  temp_private->destroyed = 2;
		  temp_private->ref_count += 1;
		  gdk_window_destroy (temp_window);
		}
	    }

	  g_list_free (children);

	  if (!private->destroyed)
	    XDestroyWindow (private->xdisplay, private->xwindow);
	  private->destroyed = TRUE;
	}
      break;

    case GDK_WINDOW_ROOT:
      g_error ("attempted to destroy root window");
      break;

    case GDK_WINDOW_PIXMAP:
      g_warning ("called gdk_window_destroy on a pixmap (use gdk_pixmap_destroy)");
      gdk_pixmap_destroy (window);
      break;
    }
}

void
gdk_window_real_destroy (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  if (private->extension_events != 0)
    gdk_input_window_destroy (window);

  if (private->ref_count == 0)
    {
      gdk_xid_table_remove (private->xwindow);
      g_free (window);
    }
}

GdkWindow*
gdk_window_ref (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  g_return_if_fail (window != NULL);

  private->ref_count += 1;
  return window;
}

void
gdk_window_unref (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  g_return_if_fail (window != NULL);

  private->ref_count -= 1;
  if (private->ref_count == 0)
    gdk_window_real_destroy (window);
}

void
gdk_window_show (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      XRaiseWindow (private->xdisplay, private->xwindow);
      XMapWindow (private->xdisplay, private->xwindow);
    }
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    XUnmapWindow (private->xdisplay, private->xwindow);
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  XMoveWindow (private->xdisplay, private->xwindow, x, y);

  if (private->window_type == GDK_WINDOW_CHILD)
    {
      private->x = x;
      private->y = y;
    }
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed &&
      ((private->resize_count > 0) ||
       (private->width != (guint16) width) ||
       (private->height != (guint16) height)))
    {
      XResizeWindow (private->xdisplay, private->xwindow, width, height);
      private->resize_count += 1;

      if (private->window_type == GDK_WINDOW_CHILD)
	{
	  private->width = width;
	  private->height = height;
	}
    }
}

void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  private = (GdkWindowPrivate*) window;
  XMoveResizeWindow (private->xdisplay, private->xwindow, x, y, width, height);

  if (!private->destroyed &&
      (private->window_type == GDK_WINDOW_CHILD))
    {
      private->x = x;
      private->y = y;
      private->width = width;
      private->height = height;
    }
}

void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *parent_private;

  g_return_if_fail (window != NULL);

  if (!new_parent)
    new_parent = (GdkWindow*) &gdk_root_parent;

  window_private = (GdkWindowPrivate*) window;
  parent_private = (GdkWindowPrivate*) new_parent;

  XReparentWindow (window_private->xdisplay,
		   window_private->xwindow,
		   parent_private->xwindow,
		   x, y);
}

void
gdk_window_clear (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  XClearWindow (private->xdisplay, private->xwindow);
}

void
gdk_window_clear_area (GdkWindow *window,
		       gint       x,
		       gint       y,
		       gint       width,
		       gint       height)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed)
    XClearArea (private->xdisplay, private->xwindow,
		x, y, width, height, False);
}

void
gdk_window_clear_area_e (GdkWindow *window,
		         gint       x,
		         gint       y,
		         gint       width,
		         gint       height)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed)
    XClearArea (private->xdisplay, private->xwindow,
		x, y, width, height, True);
}

void
gdk_window_copy_area (GdkWindow    *window,
		      GdkGC        *gc,
		      gint          x,
		      gint          y,
		      GdkWindow    *source_window,
		      gint          source_x,
		      gint          source_y,
		      gint          width,
		      gint          height)
{
  GdkWindowPrivate *src_private;
  GdkWindowPrivate *dest_private;
  GdkGCPrivate *gc_private;

  g_return_if_fail (window != NULL);
  g_return_if_fail (gc != NULL);
  
  if (source_window == NULL)
    source_window = window;

  src_private = (GdkWindowPrivate*) source_window;
  dest_private = (GdkWindowPrivate*) window;
  gc_private = (GdkGCPrivate*) gc;
  
  if (!src_private->destroyed && !dest_private->destroyed)
  {
    XCopyArea (dest_private->xdisplay, src_private->xwindow, dest_private->xwindow,
	       gc_private->xgc,
	       source_x, source_y,
	       width, height,
	       x, y);
  }
}

void
gdk_window_raise (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed)
    XRaiseWindow (private->xdisplay, private->xwindow);
}

void
gdk_window_lower (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed)
    XLowerWindow (private->xdisplay, private->xwindow);
}

void
gdk_window_set_user_data (GdkWindow *window,
			  gpointer   user_data)
{
  g_return_if_fail (window != NULL);

  window->user_data = user_data;
}

void
gdk_window_set_hints (GdkWindow *window,
		      gint       x,
		      gint       y,
		      gint       min_width,
		      gint       min_height,
		      gint       max_width,
		      gint       max_height,
		      gint       flags)
{
  GdkWindowPrivate *private;
  XSizeHints size_hints;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  size_hints.flags = 0;

  if (flags & GDK_HINT_POS)
    {
      size_hints.flags |= PPosition;
      size_hints.x = x;
      size_hints.y = y;
    }

  if (flags & GDK_HINT_MIN_SIZE)
    {
      size_hints.flags |= PMinSize;
      size_hints.min_width = min_width;
      size_hints.min_height = min_height;
    }

  if (flags & GDK_HINT_MAX_SIZE)
    {
      size_hints.flags |= PMaxSize;
      size_hints.max_width = max_width;
      size_hints.max_height = max_height;
    }

  if (flags)
    XSetWMNormalHints (private->xdisplay, private->xwindow, &size_hints);
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  XmbSetWMProperties (private->xdisplay, private->xwindow,
                      title, title, NULL, 0, NULL, NULL, NULL);
}

void
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  XSetWindowBackground (private->xdisplay, private->xwindow, color->pixel);
}

void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gint       parent_relative)
{
  GdkWindowPrivate *window_private;
  GdkPixmapPrivate *pixmap_private;
  Pixmap xpixmap;

  g_return_if_fail (window != NULL);

  window_private = (GdkWindowPrivate*) window;
  pixmap_private = (GdkPixmapPrivate*) pixmap;

  if (pixmap)
    xpixmap = pixmap_private->xwindow;
  else
    xpixmap = None;

  if (parent_relative)
    xpixmap = ParentRelative;

  XSetWindowBackgroundPixmap (window_private->xdisplay, window_private->xwindow, xpixmap);
}

void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkWindowPrivate *window_private;
  GdkCursorPrivate *cursor_private;
  Cursor xcursor;

  g_return_if_fail (window != NULL);

  window_private = (GdkWindowPrivate*) window;
  cursor_private = (GdkCursorPrivate*) cursor;

  if (!cursor)
    xcursor = None;
  else
    xcursor = cursor_private->xcursor;

  XDefineCursor (window_private->xdisplay, window_private->xwindow, xcursor);
}

void
gdk_window_set_colormap (GdkWindow   *window,
			 GdkColormap *colormap)
{
  GdkWindowPrivate *window_private;
  GdkColormapPrivate *colormap_private;

  g_return_if_fail (window != NULL);
  g_return_if_fail (colormap != NULL);

  window_private = (GdkWindowPrivate*) window;
  colormap_private = (GdkColormapPrivate*) colormap;

  XSetWindowColormap (window_private->xdisplay,
		      window_private->xwindow,
		      colormap_private->xcolormap);

  if (window_private->window_type != GDK_WINDOW_TOPLEVEL)
    gdk_window_add_colormap_windows (window);
}

void
gdk_window_get_user_data (GdkWindow *window,
			  gpointer  *data)
{
  g_return_if_fail (window != NULL);

  *data = window->user_data;
}

void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  GdkWindowPrivate *window_private;
  Window root;
  gint tx;
  gint ty;
  guint twidth;
  guint theight;
  guint tborder_width;
  guint tdepth;

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  window_private = (GdkWindowPrivate*) window;

  XGetGeometry (window_private->xdisplay, window_private->xwindow,
		&root, &tx, &ty, &twidth, &theight, &tborder_width, &tdepth);

  if (x)
    *x = tx;
  if (y)
    *y = ty;
  if (width)
    *width = twidth;
  if (height)
    *height = theight;
  if (depth)
    *depth = tdepth;
}

void
gdk_window_get_position (GdkWindow *window,
			 gint      *x,
			 gint      *y)
{
  GdkWindowPrivate *window_private;

  g_return_if_fail (window != NULL);

  window_private = (GdkWindowPrivate*) window;

  if (x)
    *x = window_private->x;
  if (y)
    *y = window_private->y;
}

void
gdk_window_get_size (GdkWindow *window,
		     gint       *width,
		     gint       *height)
{
  GdkWindowPrivate *window_private;

  g_return_if_fail (window != NULL);

  window_private = (GdkWindowPrivate*) window;

  if (width)
    *width = window_private->width;
  if (height)
    *height = window_private->height;
}


GdkVisual*
gdk_window_get_visual (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
  XWindowAttributes window_attributes;

  g_return_val_if_fail (window != NULL, NULL);

  window_private = (GdkWindowPrivate*) window;
  while (window_private && (window_private->window_type == GDK_WINDOW_PIXMAP))
    window_private = (GdkWindowPrivate*) window_private->parent;

  if (window_private)
    {
      XGetWindowAttributes (window_private->xdisplay,
			    window_private->xwindow,
			    &window_attributes);

      return gdk_visual_lookup (window_attributes.visual);
    }

  return NULL;
}

GdkColormap*
gdk_window_get_colormap (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
  XWindowAttributes window_attributes;

  g_return_val_if_fail (window != NULL, NULL);

  window_private = (GdkWindowPrivate*) window;

  XGetWindowAttributes (window_private->xdisplay,
			window_private->xwindow,
			&window_attributes);

  return gdk_colormap_lookup (window_attributes.colormap);
}

GdkWindowType
gdk_window_get_type (GdkWindow *window)
{
  GdkWindowPrivate *window_private;

  g_return_val_if_fail (window != NULL, (GdkWindowType) -1);

  window_private = (GdkWindowPrivate*) window;
  return window_private->window_type;
}

gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  GdkWindowPrivate *private;
  gint return_val;
  Window child;
  gint tx, ty;

  g_return_val_if_fail (window != NULL, 0);

  private = (GdkWindowPrivate*) window;

  return_val = XTranslateCoordinates (private->xdisplay,
				      private->xwindow,
				      gdk_root_window,
				      0, 0, &tx, &ty,
				      &child);

  if (x)
    *x = tx;
  if (y)
    *y = ty;

  return return_val;
}

GdkWindow*
gdk_window_get_pointer (GdkWindow       *window,
			gint            *x,
			gint            *y,
			GdkModifierType *mask)
{
  GdkWindowPrivate *private;
  GdkWindow *return_val;
  Window root;
  Window child;
  int rootx, rooty;
  int winx, winy;
  unsigned int xmask;

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  private = (GdkWindowPrivate*) window;

  return_val = NULL;
  if (XQueryPointer (private->xdisplay, private->xwindow, &root, &child,
		     &rootx, &rooty, &winx, &winy, &xmask))
    {
      if (x) *x = winx;
      if (y) *y = winy;
      if (mask) *mask = xmask;

      if (child)
	return_val = gdk_window_lookup (child);
    }

  return return_val;
}

GdkWindow*
gdk_window_get_parent (GdkWindow *window)
{
  g_return_val_if_fail (window != NULL, NULL);

  return ((GdkWindowPrivate*) window)->parent;
}

GdkWindow*
gdk_window_get_toplevel (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_val_if_fail (window != NULL, NULL);

  private = (GdkWindowPrivate*) window;

  while (private->window_type == GDK_WINDOW_CHILD)
    {
      window = ((GdkWindowPrivate*) window)->parent;
      private = (GdkWindowPrivate*) window;
    }

  return window;
}

GList*
gdk_window_get_children (GdkWindow *window)
{
  GdkWindowPrivate *private;
  GdkWindow *child;
  GList *children;
  Window root;
  Window parent;
  Window *xchildren;
  unsigned int nchildren;
  unsigned int i;

  g_return_val_if_fail (window != NULL, NULL);

  private = (GdkWindowPrivate*) window;

  XQueryTree (private->xdisplay, private->xwindow,
	      &root, &parent, &xchildren, &nchildren);

  children = NULL;

  if (nchildren > 0)
    {
      for (i = 0; i < nchildren; i++)
	{
	  child = gdk_window_lookup (xchildren[i]);
          if (child)
            children = g_list_prepend (children, child);
	}

      XFree (xchildren);
    }

  return children;
}

GdkEventMask  
gdk_window_get_events      (GdkWindow       *window)
{
  XWindowAttributes attrs;
  GdkEventMask event_mask;
  int i;

  XGetWindowAttributes (gdk_display, ((GdkWindowPrivate *)window)->xwindow, 
			&attrs);

  event_mask = 0;
  for (i = 0; i < nevent_masks; i++)
    {
      if (attrs.your_event_mask & event_mask_table[i])
	event_mask |= 1 << (i + 1);
    }

  return event_mask;
}

void          
gdk_window_set_events      (GdkWindow       *window,
			    GdkEventMask     event_mask)
{
  long xevent_mask;
  int i;

  xevent_mask = StructureNotifyMask;
  for (i = 0; i < nevent_masks; i++)
    {
      if (event_mask & (1 << (i + 1)))
	xevent_mask |= event_mask_table[i];
    }
  
  XSelectInput (gdk_display, ((GdkWindowPrivate *)window)->xwindow, 
		xevent_mask);
}

void
gdk_window_add_colormap_windows (GdkWindow *window)
{
  GdkWindow *toplevel;
  GdkWindowPrivate *toplevel_private;
  GdkWindowPrivate *window_private;
  Window *old_windows;
  Window *new_windows;
  int i, count;

  g_return_if_fail (window != NULL);

  toplevel = gdk_window_get_toplevel (window);
  toplevel_private = (GdkWindowPrivate*) toplevel;
  window_private = (GdkWindowPrivate*) window;

  if (!XGetWMColormapWindows (toplevel_private->xdisplay,
			      toplevel_private->xwindow,
			      &old_windows, &count))
    {
      old_windows = NULL;
      count = 0;
    }

  for (i = 0; i < count; i++)
    if (old_windows[i] == window_private->xwindow)
      return;

  new_windows = g_new (Window, count + 1);

  for (i = 0; i < count; i++)
    new_windows[i] = old_windows[i];
  new_windows[count] = window_private->xwindow;

  XSetWMColormapWindows (toplevel_private->xdisplay,
			 toplevel_private->xwindow,
			 new_windows, count + 1);

  g_free (new_windows);
  if (old_windows)
    XFree (old_windows);
}

/*
 * This needs the X11 shape extension.
 * If not available, simply remove the call to
 * XShapeCombineMask. Shaped windows will look
 * ugly, but programs still work.    Stefan Wille
 */
void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint x, gint y)
{
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *pixmap_private;

  g_return_if_fail (window != NULL);
  g_return_if_fail (mask != NULL);

  window_private = (GdkWindowPrivate*) window;
  pixmap_private = (GdkWindowPrivate*) mask;
	
  XShapeCombineMask  (window_private->xdisplay,
		      window_private->xwindow,
		      ShapeBounding,
		      x, y, /* offset */
		      (Pixmap)pixmap_private->xwindow,
		      ShapeSet);
}

void
gdk_dnd_drag_addwindow (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
  
  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate *) window;
  
  if (window_private->dnd_drag_enabled == 1 && gdk_dnd.drag_really == 0)
    {
      gdk_dnd.drag_numwindows++;
      gdk_dnd.drag_startwindows = g_realloc (gdk_dnd.drag_startwindows,
					     gdk_dnd.drag_numwindows
					     * sizeof(GdkWindow *));
      gdk_dnd.drag_startwindows[gdk_dnd.drag_numwindows - 1] = window;
      window_private->dnd_drag_accepted = 0;
    } 
  else
    g_warning ("dnd_really is 1 or drag is not enabled! can't addwindow\n");
}

void
gdk_window_dnd_drag_set (GdkWindow   *window,
			 guint8       drag_enable,
			 gchar      **typelist,
			 guint        numtypes)
{
  GdkWindowPrivate *window_private;
  int i, wasset = 0;
  
  g_return_if_fail (window != NULL);
  window_private = (GdkWindowPrivate *) window;
  
  window_private->dnd_drag_enabled = drag_enable ? 1 : 0;
  
  if (drag_enable)
    {
      g_return_if_fail(typelist != NULL);
      
      if (window_private->dnd_drag_data_numtypesavail > 3)
	wasset = 1;
      window_private->dnd_drag_data_numtypesavail = numtypes;
      
      window_private->dnd_drag_data_typesavail =
	g_realloc (window_private->dnd_drag_data_typesavail,
		   (numtypes + 1) * sizeof (GdkAtom));
      
      for (i = 0; i < numtypes; i++)
	{
	  /* Allow blanket use of ALL to get anything... */
	  if (strcmp (typelist[i], "ALL"))
	    window_private->dnd_drag_data_typesavail[i] =
	      gdk_atom_intern (typelist[i], FALSE);
	  else
	    window_private->dnd_drag_data_typesavail[i] = None;
	}
      
      /* 
       * set our extended type list if we need to 
       */
      if (numtypes > 3)
	gdk_property_change(window, gdk_dnd.gdk_XdeTypelist,
			    XA_PRIMARY, 32, GDK_PROP_MODE_REPLACE,
			    (guchar *)(window_private->dnd_drag_data_typesavail
			     + (sizeof(GdkAtom) * 3)),
			    (numtypes - 3) * sizeof(GdkAtom));
      else if (wasset)
	gdk_property_delete (window, gdk_dnd.gdk_XdeTypelist);
    }
  else
    {
      free (window_private->dnd_drag_data_typesavail);
      window_private->dnd_drag_data_typesavail = NULL;
      window_private->dnd_drag_data_numtypesavail = 0;
    }
}

void
gdk_window_dnd_drop_set (GdkWindow   *window,
			 guint8       drop_enable,
			 gchar      **typelist,
			 guint        numtypes,
			 guint8       destructive_op)
{
  GdkWindowPrivate *window_private;
  int i;
  
  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate *) window;
  
  window_private->dnd_drop_enabled = drop_enable ? 1 : 0;
  if (drop_enable)
    {
      g_return_if_fail(typelist != NULL);
      
      window_private->dnd_drop_data_numtypesavail = numtypes;
      
      window_private->dnd_drop_data_typesavail =
	g_realloc (window_private->dnd_drop_data_typesavail,
		   (numtypes + 1) * sizeof (GdkAtom));
      
      for (i = 0; i < numtypes; i++)
	window_private->dnd_drop_data_typesavail[i] =
	  gdk_atom_intern (typelist[i], FALSE);
      
      window_private->dnd_drop_destructive_op = destructive_op;
    }
}

/* 
 * This is used to reply to a GDK_DRAG_REQUEST event
 * (which may be generated by XdeRequest or a confirmed drop... 
 */
void
gdk_window_dnd_data_set (GdkWindow       *window,
			 GdkEvent        *event,
			 gpointer         data,
			 gulong           data_numbytes)
{
  GdkWindowPrivate *window_private;
  XEvent sev;
  GdkEventDropDataAvailable tmp_ev;
  gchar *tmp;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (event != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (data_numbytes > 0);
  g_return_if_fail (event->type == GDK_DRAG_REQUEST);

  g_free (event->dragrequest.data_type);
  event->dragrequest.data_type = NULL;
  
  window_private = (GdkWindowPrivate *) window;
  g_return_if_fail (window_private->dnd_drag_accepted != 0);    
  
  /* We set the property on our window... */
  gdk_property_change (window, window_private->dnd_drag_data_type,
		       XA_PRIMARY, 8, GDK_PROP_MODE_REPLACE, data,
		       data_numbytes);
  tmp = gdk_atom_name(window_private->dnd_drag_data_type);
#ifdef DEBUG_DND
  g_print("DnD type %s on window %ld\n", tmp, window_private->xwindow);
#endif
  g_free(tmp);
  
  /* 
   * Then we send the event to tell the receiving window that the
   * drop has happened 
   */
  tmp_ev.u.allflags = 0;
  tmp_ev.u.flags.protocol_version = DND_PROTOCOL_VERSION;
  tmp_ev.u.flags.isdrop = event->dragrequest.isdrop;
  
  sev.xclient.type = ClientMessage;
  sev.xclient.format = 32;
  sev.xclient.window = event->dragrequest.requestor;
  sev.xclient.message_type = gdk_dnd.gdk_XdeDataAvailable;
  sev.xclient.data.l[0] = window_private->xwindow;
  sev.xclient.data.l[1] = tmp_ev.u.allflags;
  sev.xclient.data.l[2] = window_private->dnd_drag_data_type;

  if (event->dragrequest.isdrop)
    sev.xclient.data.l[3] = event->dragrequest.drop_coords.x +
      (event->dragrequest.drop_coords.y << 16);
  else
    sev.xclient.data.l[3] = 0;
  
  sev.xclient.data.l[4] = 0;
  
  XSendEvent (gdk_display, event->dragrequest.requestor, False,
	      NoEventMask, &sev);
}
