/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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

#include <stdlib.h>

#include "gdk.h" /* gdk_rectangle_intersect */
#include "gdkevents.h"
#include "gdkpixmap.h"
#include "gdkwindow.h"
#include "gdkdisplay.h"
#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"

static GdkColormap* gdk_window_impl_win32_get_colormap (GdkDrawable *drawable);
static void         gdk_window_impl_win32_set_colormap (GdkDrawable *drawable,
							GdkColormap *cmap);
static void         gdk_window_impl_win32_get_size     (GdkDrawable *drawable,
							gint *width,
							gint *height);
static GdkRegion*   gdk_window_impl_win32_get_visible_region (GdkDrawable *drawable);
static void gdk_window_impl_win32_init       (GdkWindowImplWin32      *window);
static void gdk_window_impl_win32_class_init (GdkWindowImplWin32Class *klass);
static void gdk_window_impl_win32_finalize   (GObject                 *object);

static gpointer parent_class = NULL;

GType
_gdk_window_impl_win32_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkWindowImplWin32Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_window_impl_win32_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkWindowImplWin32),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_window_impl_win32_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_WIN32,
                                            "GdkWindowImplWin32",
                                            &object_info, 0);
    }
  
  return object_type;
}

GType
_gdk_window_impl_get_type (void)
{
  return _gdk_window_impl_win32_get_type ();
}

static void
gdk_window_impl_win32_init (GdkWindowImplWin32 *impl)
{
  impl->width = 1;
  impl->height = 1;

  impl->hcursor = NULL;
  impl->hint_flags = 0;
  impl->extension_events_selected = FALSE;
}

static void
gdk_window_impl_win32_class_init (GdkWindowImplWin32Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_impl_win32_finalize;

  drawable_class->set_colormap = gdk_window_impl_win32_set_colormap;
  drawable_class->get_colormap = gdk_window_impl_win32_get_colormap;
  drawable_class->get_size = gdk_window_impl_win32_get_size;

  /* Visible and clip regions are the same */
  drawable_class->get_clip_region = gdk_window_impl_win32_get_visible_region;
  drawable_class->get_visible_region = gdk_window_impl_win32_get_visible_region;
}

static void
gdk_window_impl_win32_finalize (GObject *object)
{
  GdkWindowObject *wrapper;
  GdkDrawableImplWin32 *draw_impl;
  GdkWindowImplWin32 *window_impl;
  
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (object));

  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (object);
  window_impl = GDK_WINDOW_IMPL_WIN32 (object);
  
  wrapper = (GdkWindowObject*) draw_impl->wrapper;

  if (!GDK_WINDOW_DESTROYED (wrapper))
    {
      gdk_win32_handle_table_remove (draw_impl->handle);
    }

  if (window_impl->hcursor != NULL)
    {
      if (GetCursor () == window_impl->hcursor)
	SetCursor (NULL);
      if (!DestroyCursor (window_impl->hcursor))
        WIN32_GDI_FAILED("DestroyCursor");
      window_impl->hcursor = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GdkColormap*
gdk_window_impl_win32_get_colormap (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *drawable_impl;
  
  g_return_val_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (drawable), NULL);

  drawable_impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  if (!((GdkWindowObject *) drawable_impl->wrapper)->input_only && 
      drawable_impl->colormap == NULL)
    {
      drawable_impl->colormap = gdk_colormap_get_system ();
      gdk_colormap_ref (drawable_impl->colormap);
    }
  
  return drawable_impl->colormap;
}

static void
gdk_window_impl_win32_set_colormap (GdkDrawable *drawable,
				    GdkColormap *cmap)
{
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;
  
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (drawable));

  impl = GDK_WINDOW_IMPL_WIN32 (drawable);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  /* chain up */
  GDK_DRAWABLE_CLASS (parent_class)->set_colormap (drawable, cmap);
  
  if (cmap)
    {
      /* XXX */
      g_print("gdk_window_impl_win32_set_colormap: XXX\n");
    }
}

static void
gdk_window_impl_win32_get_size (GdkDrawable *drawable,
				gint        *width,
				gint        *height)
{
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (drawable));

  if (width)
    *width = GDK_WINDOW_IMPL_WIN32 (drawable)->width;
  if (height)
    *height = GDK_WINDOW_IMPL_WIN32 (drawable)->height;
}

static GdkRegion*
gdk_window_impl_win32_get_visible_region (GdkDrawable *drawable)
{
  GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (drawable);
  GdkRectangle result_rect;

  result_rect.x = 0;
  result_rect.y = 0;
  result_rect.width = impl->width;
  result_rect.height = impl->height;

  gdk_rectangle_intersect (&result_rect, &impl->position_info.clip_rect, &result_rect);

  return gdk_region_rectangle (&result_rect);
}

void
_gdk_windowing_window_init (void)
{
  GdkWindowObject *private;
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;
  RECT rect;
  guint width;
  guint height;

  g_assert (_gdk_parent_root == NULL);
  
  SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
  width  = rect.right - rect.left;
  height = rect.bottom - rect.top;

  _gdk_parent_root = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)_gdk_parent_root;
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (private->impl);
  
  draw_impl->handle = _gdk_root_window;
  draw_impl->wrapper = GDK_DRAWABLE (private);
  draw_impl->colormap = gdk_colormap_get_system ();
  gdk_colormap_ref (draw_impl->colormap);
  
  private->window_type = GDK_WINDOW_ROOT;
  private->depth = gdk_visual_get_system ()->depth;
  impl->width = width;
  impl->height = height;

  _gdk_window_init_position (GDK_WINDOW (private));

  gdk_win32_handle_table_insert (&_gdk_root_window, _gdk_parent_root);
}

static const gchar *
get_default_title (void)
{
  const char *title;
  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();

  return title;
}

/* The Win API function AdjustWindowRect may return negative values
 * resulting in obscured title bars. This helper function is coreccting it.
 */
static BOOL
SafeAdjustWindowRectEx (RECT* lpRect,
			DWORD dwStyle, 
			BOOL  bMenu, 
			DWORD dwExStyle)
{
  if (!AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle))
    {
      WIN32_API_FAILED ("AdjustWindowRectEx");
      return FALSE;
    }
  if (lpRect->left < 0)
    {
      lpRect->right -= lpRect->left;
      lpRect->left = 0;
    }
  if (lpRect->top < 0)
    {
      lpRect->bottom -= lpRect->top;
      lpRect->top = 0;
    }
  return TRUE;
}

/* RegisterGdkClass
 *   is a wrapper function for RegisterWindowClassEx.
 *   It creates at least one unique class for every 
 *   GdkWindowType. If support for single window-specific icons
 *   is ever needed (e.g Dialog specific), every such window should
 *   get its own class
 */
static ATOM
RegisterGdkClass (GdkWindowType wtype)
{
  static ATOM klassTOPLEVEL = 0;
  static ATOM klassDIALOG   = 0;
  static ATOM klassCHILD    = 0;
  static ATOM klassTEMP     = 0;
  static HICON hAppIcon = NULL;
  static WNDCLASSEX wcl; 
  ATOM klass = 0;

  wcl.cbSize = sizeof (WNDCLASSEX);
  wcl.style = 0; /* DON'T set CS_<H,V>REDRAW. It causes total redraw
                  * on WM_SIZE and WM_MOVE. Flicker, Performance!
                  */
  wcl.lpfnWndProc = _gdk_win32_window_procedure;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = _gdk_app_hmodule;
  wcl.hIcon = 0;
  /* initialize once! */
  if (0 == hAppIcon)
    {
      gchar sLoc [MAX_PATH+1];

      if (0 != GetModuleFileName (_gdk_app_hmodule, sLoc, MAX_PATH))
	{
	  hAppIcon = ExtractIcon (_gdk_app_hmodule, sLoc, 0);
	  if (0 == hAppIcon)
	    {
	      if (0 != GetModuleFileName (_gdk_dll_hinstance, sLoc, MAX_PATH))
		hAppIcon = ExtractIcon (_gdk_dll_hinstance, sLoc, 0);
	    }
	}
      if (0 == hAppIcon) 
	hAppIcon = LoadIcon (NULL, IDI_APPLICATION);
    }

  wcl.lpszMenuName = NULL;
  wcl.hIconSm = 0;

  /* initialize once per class */
  /*
   * HB: Setting the background brush leads to flicker, because we
   * don't get asked how to clear the background. This is not what
   * we want, at least not for input_only windows ...
   */
#define ONCE_PER_CLASS() \
  wcl.hIcon = CopyIcon (hAppIcon); \
  wcl.hIconSm = CopyIcon (hAppIcon); \
  wcl.hbrBackground = NULL; \
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW); 
  
  switch (wtype)
    {
    case GDK_WINDOW_TOPLEVEL:
      if (0 == klassTOPLEVEL)
	{
	  wcl.lpszClassName = "gdkWindowToplevel";
	  
	  ONCE_PER_CLASS();
	  klassTOPLEVEL = RegisterClassEx (&wcl);
	}
      klass = klassTOPLEVEL;
      break;
      
    case GDK_WINDOW_CHILD:
      if (0 == klassCHILD)
	{
	  wcl.lpszClassName = "gdkWindowChild";
	  
	  wcl.style |= CS_PARENTDC; /* MSDN: ... enhances system performance. */
	  ONCE_PER_CLASS();
	  klassCHILD = RegisterClassEx (&wcl);
	}
      klass = klassCHILD;
      break;
      
    case GDK_WINDOW_DIALOG:
      if (0 == klassDIALOG)
	{
	  wcl.lpszClassName = "gdkWindowDialog";
	  wcl.style |= CS_SAVEBITS;
	  ONCE_PER_CLASS();
	  klassDIALOG = RegisterClassEx (&wcl);
	}
      klass = klassDIALOG;
      break;
      
    case GDK_WINDOW_TEMP:
      if (0 == klassTEMP)
	{
	  wcl.lpszClassName = "gdkWindowTemp";
	  wcl.style |= CS_SAVEBITS;
	  ONCE_PER_CLASS();
	  klassTEMP = RegisterClassEx (&wcl);
	}
      klass = klassTEMP;
      break;
      
    default:
      g_assert_not_reached ();
      break;
    }
  
  if (klass == 0)
    {
      WIN32_API_FAILED ("RegisterClassEx");
      g_error ("That is a fatal error");
    }
  return klass;
}

GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  HANDLE hparent;
  ATOM klass = 0;
  DWORD dwStyle, dwExStyle;
  RECT rect;
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;
  GdkVisual *visual;
  int width, height;
  int x, y;
  char *title;
  char *mbtitle;

  g_return_val_if_fail (attributes != NULL, NULL);

  if (!parent)
    parent = _gdk_parent_root;

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);
  
  GDK_NOTE (MISC,
	    g_print ("gdk_window_new: %s\n",
		     (attributes->window_type == GDK_WINDOW_TOPLEVEL ? "TOPLEVEL" :
		      (attributes->window_type == GDK_WINDOW_CHILD ? "CHILD" :
		       (attributes->window_type == GDK_WINDOW_DIALOG ? "DIALOG" :
			(attributes->window_type == GDK_WINDOW_TEMP ? "TEMP" :
			 "???"))))));

  if (GDK_WINDOW_DESTROYED (parent))
    return NULL;
  
  hparent = GDK_WINDOW_HWND (parent);

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);

  /* Windows with a foreign parent are treated as if they are children
   * of the root window, except for actual creation.
   */
  if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_FOREIGN)
    parent = _gdk_parent_root;
  
  private->parent = (GdkWindowObject *)parent;

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = CW_USEDEFAULT;
  
  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else if (attributes_mask & GDK_WA_X)
    y = 100;			/* ??? We must put it somewhere... */
  else
    y = 0;			/* x is CW_USEDEFAULT, y doesn't matter then */
  
  private->x = x;
  private->y = y;
  impl->width = (attributes->width > 1) ? (attributes->width) : (1);
  impl->height = (attributes->height > 1) ? (attributes->height) : (1);
  impl->extension_events_selected = FALSE;
  private->window_type = attributes->window_type;

  _gdk_window_init_position (GDK_WINDOW (private));
  if (impl->position_info.big)
    private->guffaw_gravity = TRUE;

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_visual_get_system ();

  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = get_default_title ();
  if (!title || !*title)
    title = "GDK client window";

  private->event_mask = GDK_STRUCTURE_MASK | attributes->event_mask;
      
  if (private->parent && private->parent->guffaw_gravity)
    {
      /* XXX ??? */
    }

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      dwExStyle = 0;

      private->input_only = FALSE;
      private->depth = visual->depth;
      
      if (attributes_mask & GDK_WA_COLORMAP)
	{
	  draw_impl->colormap = attributes->colormap;
          gdk_colormap_ref (attributes->colormap);
        }
      else
	{
	  draw_impl->colormap = gdk_colormap_get_system ();
	}
    }
  else
    {
      dwExStyle = WS_EX_TRANSPARENT;
      private->depth = 0;
      private->input_only = TRUE;
      draw_impl->colormap = gdk_colormap_get_system ();
      gdk_colormap_ref (draw_impl->colormap);
      GDK_NOTE (MISC, g_print ("...GDK_INPUT_ONLY, system colormap\n"));
    }

  if (private->parent)
    private->parent->children = g_list_prepend (private->parent->children, window);

  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
      dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
      hparent = _gdk_root_window;
      break;

    case GDK_WINDOW_CHILD:
      dwStyle = WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      break;

    case GDK_WINDOW_DIALOG:
      dwStyle = WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_CLIPCHILDREN;
#if 0
      dwExStyle |= WS_EX_TOPMOST; /* //HB: want this? */
#endif
      hparent = _gdk_root_window;
      break;

    case GDK_WINDOW_TEMP:
      dwStyle = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      /* a temp window is not necessarily a top level window */
      dwStyle |= (_gdk_parent_root == parent ? WS_POPUP : WS_CHILDWINDOW);
      dwExStyle |= WS_EX_TOOLWINDOW;
      break;

    case GDK_WINDOW_ROOT:
      g_error ("cannot make windows of type GDK_WINDOW_ROOT");
      break;
    }

  klass = RegisterGdkClass (private->window_type);

  if (private->window_type != GDK_WINDOW_CHILD)
    {
      if (x == CW_USEDEFAULT)
	{
	  rect.left = 100;
	  rect.top = 100;
	}
      else
	{
	  rect.left = x;
	  rect.top = y;
	}

      rect.right = rect.left + impl->width;
      rect.bottom = rect.top + impl->height;

      SafeAdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);

      if (x != CW_USEDEFAULT)
	{
	  x = rect.left;
	  y = rect.top;
	}
      width = rect.right - rect.left;
      height = rect.bottom - rect.top;
    }
  else
    {
      width = impl->width;
      height = impl->height;
    }

  mbtitle = g_locale_from_utf8 (title, -1, NULL, NULL, NULL);
  
#ifdef WITHOUT_WM_CREATE
  draw_impl->handle = CreateWindowEx (dwExStyle,
				      MAKEINTRESOURCE(klass),
				      mbtitle,
				      dwStyle,
				      impl->position_info.x, impl->position_info.y, 
				      impl->position_info.width, impl->position_info.height,
				      hparent,
				      NULL,
				      _gdk_app_hmodule,
				      NULL);
#else
  {
  HWND hwndNew =
    CreateWindowEx (dwExStyle,
		    MAKEINTRESOURCE(klass),
		    mbtitle,
		    dwStyle,
		    impl->position_info.x, impl->position_info.y, 
		    impl->position_info.width, impl->position_info.height,
		    hparent,
		    NULL,
		    _gdk_app_hmodule,
		    window);
  if (GDK_WINDOW_HWND (window) != hwndNew)
    {
      g_warning("gdk_window_new: gdk_event_translate::WM_CREATE (%p, %p) HWND mismatch.",
		GDK_WINDOW_HWND (window),
		hwndNew);

      /* HB: IHMO due to a race condition the handle was increased by
       * one, which causes much trouble. Because I can't find the 
       * real bug, try to workaround it ...
       * To reproduce: compile with MSVC 5, DEBUG=1
       */
# if 0
      gdk_win32_handle_table_remove (GDK_WINDOW_HWND (window));
      GDK_WINDOW_HWND (window) = hwndNew;
      gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);
# else
      /* the old behaviour, but with warning */
      GDK_WINDOW_HWND (window) = hwndNew;
# endif

    }
  }
  gdk_drawable_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);
#endif

  GDK_NOTE (MISC,
	    g_print ("... \"%s\" %dx%d@+%d+%d %p = %p\n",
		     mbtitle,
		     width, height, (x == CW_USEDEFAULT ? -9999 : x), y, 
		     hparent,
		     GDK_WINDOW_HWND (window)));

  g_free (mbtitle);

  if (draw_impl->handle == NULL)
    {
      WIN32_API_FAILED ("CreateWindowEx");
      g_object_unref ((GObject *) window);
      return NULL;
    }

#ifdef WITHOUT_WM_CREATE
  gdk_drawable_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);
#endif

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  return window;
}

GdkWindow *
gdk_window_foreign_new_for_display (GdkDisplay      *display,
                                    GdkNativeWindow  anid)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplWin32 *impl;
  GdkDrawableImplWin32 *draw_impl;

  HANDLE parent;
  RECT rect;
  POINT point;

  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);
  parent = GetParent ((HWND)anid);
  
  private->parent = gdk_win32_handle_table_lookup ((GdkNativeWindow) parent);
  if (!private->parent || GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_FOREIGN)
    private->parent = (GdkWindowObject *)_gdk_parent_root;
  
  private->parent->children = g_list_prepend (private->parent->children, window);

  draw_impl->handle = (HWND) anid;
  GetClientRect ((HWND) anid, &rect);
  point.x = rect.left;
  point.y = rect.right;
  ClientToScreen ((HWND) anid, &point);
  if (parent != _gdk_root_window)
    ScreenToClient (parent, &point);
  private->x = point.x;
  private->y = point.y;
  impl->width = rect.right - rect.left;
  impl->height = rect.bottom - rect.top;
  private->window_type = GDK_WINDOW_FOREIGN;
  private->destroyed = FALSE;
  private->event_mask = GDK_ALL_EVENTS_MASK; /* XXX */
  if (IsWindowVisible ((HWND) anid))
    private->state &= (~GDK_WINDOW_STATE_WITHDRAWN);
  else
    private->state |= GDK_WINDOW_STATE_WITHDRAWN;
  private->depth = gdk_visual_get_system ()->depth;

  _gdk_window_init_position (GDK_WINDOW (private));

  gdk_drawable_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);

  return window;
}

GdkWindow*
gdk_window_lookup (GdkNativeWindow hwnd)
{
  return (GdkWindow*) gdk_win32_handle_table_lookup (hwnd); 
}

void
_gdk_windowing_window_destroy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("_gdk_windowing_window_destroy %p\n",
			   GDK_WINDOW_HWND (window)));

  if (private->extension_events != 0)
    _gdk_input_window_destroy (window);

  if (private->window_type == GDK_WINDOW_FOREIGN)
    {
      if (!foreign_destroy && (private->parent != NULL))
	{
	  /* It's somebody else's window, but in our hierarchy,
	   * so reparent it to the root window, and then call
	   * DestroyWindow() on it.
	   */
	  gdk_window_hide (window);
	  gdk_window_reparent (window, NULL, 0, 0);
	  
	  /* Is this too drastic? Many (most?) applications
	   * quit if any window receives WM_QUIT I think.
	   * OTOH, I don't think foreign windows are much
	   * used, so the question is maybe academic.
	   */
	  PostMessage (GDK_WINDOW_HWND (window), WM_QUIT, 0, 0);
	}
    }
  else if (!recursing && !foreign_destroy)
    {
      private->destroyed = TRUE;
      DestroyWindow (GDK_WINDOW_HWND (window));
    }
}

/* This function is called when the window really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (EVENTS,
	    g_print ("gdk_window_destroy_notify: %p  %s\n",
		     GDK_WINDOW_HWND (window),
		     (GDK_WINDOW_DESTROYED (window) ? "(destroyed)" : "")));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("window %p unexpectedly destroyed",
		   GDK_WINDOW_HWND (window));

      _gdk_window_destroy (window, TRUE);
    }
  
  gdk_win32_handle_table_remove (GDK_WINDOW_HWND (window));
  gdk_drawable_unref (window);
}

static void
show_window_internal (GdkWindow *window,
                      gboolean   raise,
		      gboolean   deiconify)
{
  GdkWindowObject *private;
  HWND old_active_window;
  
  private = GDK_WINDOW_OBJECT (window);

  if (private->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("show_window_internal: %p %s%s%s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (private->state),
			   (raise ? " raise" : ""),
			   (deiconify ? " deiconify" : "")));
  
  /* If asked to show (not deiconify) an withdrawn and iconified
   * window, do that.
   */
  if (!deiconify &&
      !GDK_WINDOW_IS_MAPPED (window) &&
      (private->state & GDK_WINDOW_STATE_ICONIFIED))
    {	
      ShowWindow (GDK_WINDOW_HWND (window), SW_MINIMIZE);
      return;
    }
  
  /* If asked to just show an iconified window, do nothing. */
  if (!deiconify && (private->state & GDK_WINDOW_STATE_ICONIFIED))
    return;
  
  /* If asked to deiconify an already noniconified window, do
   * nothing. (Especially, don't cause the window to rise and
   * activate. There are different calls for that.)
   */
  if (deiconify && !(private->state & GDK_WINDOW_STATE_ICONIFIED))
    return;
  
  /* If asked to show (but not raise) a window that is already
   * visible, do nothing.
   */
  if (!deiconify && !raise && IsWindowVisible (GDK_WINDOW_HWND (window)))
    return;

  /* Other cases */
  
  if (!GDK_WINDOW_IS_MAPPED (window))
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_WITHDRAWN,
				 0);
  if (GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE) & WS_EX_TRANSPARENT)
    {
      /* Don't really know if this makes sense, can't remember whether
       * this case is handled like this because it is necessary, or
       * if this is just old crap.
       */
      SetWindowPos(GDK_WINDOW_HWND (window), HWND_TOP, 0, 0, 0, 0,
		   SWP_SHOWWINDOW | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE);
      return;
    }

  old_active_window = GetActiveWindow ();

  if (private->state & GDK_WINDOW_STATE_MAXIMIZED)
    ShowWindow (GDK_WINDOW_HWND (window), SW_MAXIMIZE);
  else if (private->state & GDK_WINDOW_STATE_ICONIFIED)
    ShowWindow (GDK_WINDOW_HWND (window), SW_RESTORE);
  else
    ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNORMAL);

  if (raise)
    BringWindowToTop (GDK_WINDOW_HWND (window));
  else if (old_active_window != GDK_WINDOW_HWND (window))
    SetActiveWindow (old_active_window);
}

void
gdk_window_show_unraised (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  show_window_internal (window, FALSE, FALSE);
}

void
gdk_window_show (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  show_window_internal (window, TRUE, FALSE);
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);

  private = (GdkWindowObject*) window;
  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_hide: %p %s\n",
			       GDK_WINDOW_HWND (window),
			       _gdk_win32_window_state_to_string (private->state)));

      if (GDK_WINDOW_IS_MAPPED (window))
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_WITHDRAWN);

      _gdk_window_clear_update_area (window);

      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL)
	ShowOwnedPopups (GDK_WINDOW_HWND (window), FALSE);

      if (GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE) & WS_EX_TRANSPARENT)
	{
	  SetWindowPos(GDK_WINDOW_HWND (window), HWND_BOTTOM, 0, 0, 0, 0,
		       SWP_HIDEWINDOW | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
	}
      else
	{
	  ShowWindow (GDK_WINDOW_HWND (window), SW_HIDE);
	}
    }
}

void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowObject *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowObject*) window;
  if (!private->destroyed)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_withdraw: %p %s\n",
			       GDK_WINDOW_HWND (window),
			       _gdk_win32_window_state_to_string (private->state)));

      gdk_window_hide (window);	/* ??? */
    }
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplWin32 *impl;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	_gdk_window_move_resize_child (window, x, y,
				       impl->width, impl->height);
      else
	{
	  if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
	                     x, y, impl->width, impl->height,
	                     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER))
	    WIN32_API_FAILED ("SetWindowPos");
	}
    }
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowObject *private = (GdkWindowObject*) window;
  GdkWindowImplWin32 *impl;
  int x, y;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	_gdk_window_move_resize_child (window, private->x, private->y,
				       width, height);
      else
	{
	  POINT pt;
	  RECT rect;
	  DWORD dwStyle;
	  DWORD dwExStyle;

	  pt.x = 0;
	  pt.y = 0; 
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  rect.left = pt.x;
	  rect.top = pt.y;
	  rect.right = pt.x + width;
	  rect.bottom = pt.y + height;

	  dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
	  dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
	  if (!AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle))
	    WIN32_API_FAILED ("AdjustWindowRectEx");

	  x = rect.left;
	  y = rect.top;
	  width = rect.right - rect.left;
	  height = rect.bottom - rect.top;
	  if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
	                     x, y, width, height,
	                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER))
	    WIN32_API_FAILED ("SetWindowPos");
	}
      private->resize_count += 1;
    }
}

void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  GdkWindowObject *private = (GdkWindowObject*) window;
  GdkWindowImplWin32 *impl;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;
  
  impl = GDK_WINDOW_IMPL_WIN32 (private->impl);

  if (!private->destroyed)
    {
      RECT rect;
      DWORD dwStyle;
      DWORD dwExStyle;

      GDK_NOTE (MISC, g_print ("gdk_window_move_resize: %p %dx%d@+%d+%d\n",
			       GDK_WINDOW_HWND (window),
			       width, height, x, y));
      
      if (GDK_WINDOW_TYPE (private) == GDK_WINDOW_CHILD)
	_gdk_window_move_resize_child (window, x, y, width, height);
      else
	{
	  rect.left = x;
	  rect.top = y;
	  rect.right = x + width;
	  rect.bottom = y + height;

	  dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
	  dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
	  if (!AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle))
	    WIN32_API_FAILED ("AdjustWindowRectEx");

	  GDK_NOTE (MISC, g_print ("...SetWindowPos(%p,%ldx%ld@+%ld+%ld)\n",
				   GDK_WINDOW_HWND (window),
				   rect.right - rect.left, rect.bottom - rect.top,
				   rect.left, rect.top));
	  if (!SetWindowPos (GDK_WINDOW_HWND (window), NULL,
	                     rect.left, rect.top,
	                     rect.right - rect.left, rect.bottom - rect.top,
	                     SWP_NOACTIVATE | SWP_NOZORDER))
	    WIN32_API_FAILED ("SetWindowPos");
	}
    }
}

void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GdkWindowObject *window_private;
  GdkWindowObject *parent_private;
  GdkWindowObject *old_parent_private;
  GdkWindowImplWin32 *impl;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent == NULL || GDK_IS_WINDOW (new_parent));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_ROOT);

  if (!new_parent)
    new_parent = _gdk_parent_root;

  window_private = (GdkWindowObject*) window;
  old_parent_private = (GdkWindowObject *) window_private->parent;
  parent_private = (GdkWindowObject*) new_parent;
  impl = GDK_WINDOW_IMPL_WIN32 (window_private->impl);

  if (!GDK_WINDOW_DESTROYED (window) && !GDK_WINDOW_DESTROYED (new_parent))
    {
      GDK_NOTE (MISC, g_print ("gdk_window_reparent: %p %p\n",
			       GDK_WINDOW_HWND (window),
			       GDK_WINDOW_HWND (new_parent)));
      if (!SetParent (GDK_WINDOW_HWND (window),
		      GDK_WINDOW_HWND (new_parent)))
	WIN32_API_FAILED ("SetParent");

      if (!MoveWindow (GDK_WINDOW_HWND (window),
		       x, y, impl->width, impl->height, TRUE))
	WIN32_API_FAILED ("MoveWindow");
    }

  /* From here on, we treat parents of type GDK_WINDOW_FOREIGN like
   * the root window
   */
  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
    new_parent = _gdk_parent_root;
  
  window_private->parent = (GdkWindowObject *)new_parent;

  if (old_parent_private)
    old_parent_private->children =
      g_list_remove (old_parent_private->children, window);

#if 0
  if ((old_parent_private &&
       (!old_parent_private->guffaw_gravity != !parent_private->guffaw_gravity)) ||
      (!old_parent_private && parent_private->guffaw_gravity))
    gdk_window_set_static_win_gravity (window, parent_private->guffaw_gravity);
#endif

  parent_private->children = g_list_prepend (parent_private->children, window);
  _gdk_window_init_position (GDK_WINDOW (window_private));
}

void
_gdk_windowing_window_clear_area (GdkWindow *window,
				  gint       x,
				  gint       y,
				  gint       width,
				  gint       height)
{
  GdkWindowImplWin32 *impl;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      HDC hdc;

      if (width == 0)
	width = impl->width - x;
      if (height == 0)
	height = impl->height - y;
      GDK_NOTE (MISC, g_print ("_gdk_windowing_window_clear_area: "
			       "%p %dx%d@+%d+%d\n",
			       GDK_WINDOW_HWND (window),
			       width, height, x, y));
      hdc = GetDC (GDK_WINDOW_HWND (window));
      IntersectClipRect (hdc, x, y, x + width + 1, y + height + 1);
      SendMessage (GDK_WINDOW_HWND (window), WM_ERASEBKGND, (WPARAM) hdc, 0);
      if (!ReleaseDC (GDK_WINDOW_HWND (window), hdc))
	WIN32_GDI_FAILED ("ReleaseDC");
    }
}

void
_gdk_windowing_window_clear_area_e (GdkWindow *window,
				    gint       x,
				    gint       y,
				    gint       width,
				    gint       height)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      RECT rect;

      GDK_NOTE (MISC, g_print ("_gdk_windowing_window_clear_area_e: "
			       "%p %dx%d@+%d+%d\n",
			       GDK_WINDOW_HWND (window),
			       width, height, x, y));

      rect.left = x;
      rect.right = x + width + 1;
      rect.top = y;
      rect.bottom = y + height + 1;
      if (!InvalidateRect (GDK_WINDOW_HWND (window), &rect, TRUE))
	WIN32_GDI_FAILED ("InvalidateRect");
      UpdateWindow (GDK_WINDOW_HWND (window));
    }
}

void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_window_raise: %p\n",
			       GDK_WINDOW_HWND (window)));

      if (!BringWindowToTop (GDK_WINDOW_HWND (window)))
	WIN32_API_FAILED ("BringWindowToTop");
    }
}

void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_window_lower: %p\n",
			       GDK_WINDOW_HWND (window)));

      if (!SetWindowPos (GDK_WINDOW_HWND (window), HWND_BOTTOM, 0, 0, 0, 0,
			 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE))
	WIN32_API_FAILED ("SetWindowPos");
    }
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
  GdkWindowImplWin32 *impl;
  WINDOWPLACEMENT size_hints;
  RECT rect;
  DWORD dwStyle;
  DWORD dwExStyle;
  int diff;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  GDK_NOTE (MISC, g_print ("gdk_window_set_hints: %p %dx%d..%dx%d @+%d+%d\n",
			   GDK_WINDOW_HWND (window),
			   min_width, min_height, max_width, max_height,
			   x, y));

  impl->hint_flags = flags;
  size_hints.length = sizeof (size_hints);

  if (flags)
    {
      GdkGeometry geom;
      gint        geom_mask = 0;

      geom.min_width  = min_width;
      geom.min_height = min_height;
      geom.max_width  = max_width;
      geom.max_height = max_height;

      if (flags & GDK_HINT_POS)
	{
	  if (!GetWindowPlacement (GDK_WINDOW_HWND (window), &size_hints))
	    WIN32_API_FAILED ("GetWindowPlacement");
	  else
	    {
	      GDK_NOTE (MISC, g_print ("...rcNormalPosition:"
				       " (%ld,%ld)--(%ld,%ld)\n",
				       size_hints.rcNormalPosition.left,
				       size_hints.rcNormalPosition.top,
				       size_hints.rcNormalPosition.right,
				       size_hints.rcNormalPosition.bottom));
	      /* What are the corresponding window coordinates for client
	       * area coordinates x, y
	       */
	      rect.left = x;
	      rect.top = y;
	      rect.right = rect.left + 200;	/* dummy */
	      rect.bottom = rect.top + 200;
	      dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
	      dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
	      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
	      size_hints.flags = 0;
	      size_hints.showCmd = SW_SHOWNA;
	      
	      /* Set the normal position hint to that location, with unchanged
	       * width and height.
	       */
	      diff = size_hints.rcNormalPosition.left - rect.left;
	      size_hints.rcNormalPosition.left = rect.left;
	      size_hints.rcNormalPosition.right -= diff;
	      diff = size_hints.rcNormalPosition.top - rect.top;
	      size_hints.rcNormalPosition.top = rect.top;
	      size_hints.rcNormalPosition.bottom -= diff;
	      GDK_NOTE (MISC, g_print ("...setting: (%ld,%ld)--(%ld,%ld)\n",
				       size_hints.rcNormalPosition.left,
				       size_hints.rcNormalPosition.top,
				       size_hints.rcNormalPosition.right,
				       size_hints.rcNormalPosition.bottom));
	      if (!SetWindowPlacement (GDK_WINDOW_HWND (window), &size_hints))
		WIN32_API_FAILED ("SetWindowPlacement");
	      impl->hint_x = rect.left;
	      impl->hint_y = rect.top;
	    }
	}

      if (flags & GDK_HINT_MIN_SIZE)
        geom_mask |= GDK_HINT_MIN_SIZE;

      if (flags & GDK_HINT_MAX_SIZE)
        geom_mask |= GDK_HINT_MAX_SIZE;

      gdk_window_set_geometry_hints (window, &geom, geom_mask);
    }
}

void 
gdk_window_set_geometry_hints (GdkWindow      *window,
			       GdkGeometry    *geometry,
			       GdkWindowHints  geom_mask)
{
  GdkWindowImplWin32 *impl;
  WINDOWPLACEMENT size_hints;
  RECT rect;
  DWORD dwStyle;
  DWORD dwExStyle;
  gint new_width = 0, new_height = 0;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  size_hints.length = sizeof (size_hints);

  impl->hint_flags = geom_mask;

  if (geom_mask & GDK_HINT_POS)
    ; /* even the X11 mplementation doesn't care */

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      rect.left = 0;
      rect.top = 0;
      rect.right = geometry->min_width;
      rect.bottom = geometry->min_height;
      dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
      dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
      impl->hint_min_width = rect.right - rect.left;
      impl->hint_min_height = rect.bottom - rect.top;

      /* Also check if he current size of the window is in bounds */
      GetClientRect (GDK_WINDOW_HWND (window), &rect);

      if (rect.right < geometry->min_width
	  && rect.bottom < geometry->min_height)
	{
	  new_width = geometry->min_width; new_height = geometry->min_height;
	}
      else if (rect.right < geometry->min_width)
	{
	  new_width = geometry->min_width; new_height = rect.bottom;
	}
      else if (rect.bottom < geometry->min_height)
	{
	  new_width = rect.right; new_height = geometry->min_height;
	}
    }
  
  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      rect.left = 0;
      rect.top = 0;
      rect.right = geometry->max_width;
      rect.bottom = geometry->max_height;
      dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
      dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
      /* HB: dont' know why AdjustWindowRectEx is called here, ... */
      SafeAdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
      impl->hint_max_width = rect.right - rect.left;
      impl->hint_max_height = rect.bottom - rect.top;
      /* ... but negative sizes are always wrong */
      if (impl->hint_max_width < 0) impl->hint_max_width = G_MAXSHORT;
      if (impl->hint_max_height < 0) impl->hint_max_height = G_MAXSHORT;

      /* Again, check if the window is too large currently. */
      GetClientRect (GDK_WINDOW_HWND (window), &rect);
      if (rect.right > geometry->max_width
	  && rect.bottom > geometry->max_height)
	{
	  new_width = geometry->max_width; new_height = geometry->max_height;
	}
      else if (rect.right > geometry->max_width)
	{
	  new_width = geometry->max_width; new_height = rect.bottom;
	}
      else if (rect.bottom > geometry->max_height)
	{
	  new_width = rect.right; new_height = geometry->max_height;
	}
    }

  /* finally apply new size constraints */
  if (new_width != 0 && new_height != 0)
    gdk_window_resize (window, new_width, new_height);
  
  /* I don't know what to do when called with zero base_width and height. */
  if (geom_mask & GDK_HINT_BASE_SIZE
      && geometry->base_width > 0
      && geometry->base_height > 0)
    {
      if (!GetWindowPlacement (GDK_WINDOW_HWND (window), &size_hints))
	WIN32_API_FAILED ("GetWindowPlacement");
      else
	{
	  GDK_NOTE (MISC, g_print ("gdk_window_set_geometry_hints:"
				   " rcNormalPosition: (%ld,%ld)--(%ld,%ld)\n",
				   size_hints.rcNormalPosition.left,
				   size_hints.rcNormalPosition.top,
				   size_hints.rcNormalPosition.right,
				   size_hints.rcNormalPosition.bottom));
	  size_hints.rcNormalPosition.right =
	    size_hints.rcNormalPosition.left + geometry->base_width;
	  size_hints.rcNormalPosition.bottom =
	    size_hints.rcNormalPosition.top + geometry->base_height;
	  GDK_NOTE (MISC, g_print ("...setting: rcNormal: (%ld,%ld)--(%ld,%ld)\n",
				   size_hints.rcNormalPosition.left,
				   size_hints.rcNormalPosition.top,
				   size_hints.rcNormalPosition.right,
				   size_hints.rcNormalPosition.bottom));
	  if (!SetWindowPlacement (GDK_WINDOW_HWND (window), &size_hints))
	    WIN32_API_FAILED ("SetWindowPlacement");
	}
    }
  
  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      /* XXX */
    }
  
  if (geom_mask & GDK_HINT_ASPECT)
    {
      /* XXX */
    }
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  char *mbtitle;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (title != NULL);

  /* Empty window titles not allowed, so set it to just a period. */
  if (!title[0])
    title = ".";
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_title: %p %s\n",
			   GDK_WINDOW_HWND (window), title));
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      /* As the title is in UTF-8 we must translate it
       * to the system codepage.
       */
      mbtitle = g_locale_from_utf8 (title, -1, NULL, NULL, NULL);
      if (!SetWindowText (GDK_WINDOW_HWND (window), mbtitle))
	WIN32_API_FAILED ("SetWindowText");

      g_free (mbtitle);
    }
}

void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_role: %p %s\n",
			   GDK_WINDOW_HWND (window),
			   (role ? role : "NULL")));
  /* XXX */
}

void          
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  HWND window_id, parent_id;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_transient_for: %p %p\n",
			   GDK_WINDOW_HWND (window),
			   GDK_WINDOW_HWND (parent)));

  if (GDK_WINDOW_DESTROYED (window) || GDK_WINDOW_DESTROYED (parent))
    return;

  if (((GdkWindowObject *) window)->window_type == GDK_WINDOW_CHILD)
    {
      GDK_NOTE (MISC, g_print ("...a child window!\n"));
      return;
    }
  
  window_id = GDK_WINDOW_HWND (window);
  parent_id = GDK_WINDOW_HWND (parent);

  /* This changes the *owner* of the window, despite the misleading
   * name. (Owner and parent are unrelated concepts.) At least that's
   * what people who seem to know what they talk about say on
   * USENET. Search on Google.
   */
  SetLastError (0);
  if (SetWindowLong (window_id, GWL_HWNDPARENT, (long) parent_id) == 0 &&
      GetLastError () != 0)
    WIN32_API_FAILED ("SetWindowLong");
}

void
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_background: %p %s\n",
			   GDK_WINDOW_HWND (window), 
			   _gdk_win32_color_to_string (color)));

  private->bg_color = *color;

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    {
      gdk_drawable_unref (private->bg_pixmap);
      private->bg_pixmap = NULL;
    }
}

void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gint       parent_relative)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (pixmap == NULL || !parent_relative);
  g_return_if_fail (pixmap == NULL || gdk_drawable_get_depth (window) == gdk_drawable_get_depth (pixmap));
  
  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    gdk_drawable_unref (private->bg_pixmap);

  if (parent_relative)
    {
      private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
      GDK_NOTE (MISC, g_print (G_STRLOC ": setting background pixmap to parent_relative\n"));
    }
  else
    {
      if (pixmap)
	{
	  gdk_drawable_ref (pixmap);
	  private->bg_pixmap = pixmap;
	}
      else
	{
	  private->bg_pixmap = GDK_NO_BG;
	}
    }
}

void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkWindowImplWin32 *impl;
  GdkCursorPrivate *cursor_private;
  GdkWindowObject *parent_window;
  HCURSOR hcursor;
  HCURSOR hprevcursor;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);
  cursor_private = (GdkCursorPrivate*) cursor;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!cursor)
    hcursor = NULL;
  else
    hcursor = cursor_private->hcursor;
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_cursor: %p %p\n",
			   GDK_WINDOW_HWND (window),
			   hcursor));

  /* First get the old cursor, if any (we wait to free the old one
   * since it may be the current cursor set in the Win32 API right
   * now).
   */
  hprevcursor = impl->hcursor;

  if (hcursor == NULL)
    impl->hcursor = NULL;
  else
    {
      /* We must copy the cursor as it is OK to destroy the GdkCursor
       * while still in use for some window. See for instance
       * gimp_change_win_cursor() which calls gdk_window_set_cursor
       * (win, cursor), and immediately afterwards gdk_cursor_destroy
       * (cursor).
       */
      if ((impl->hcursor = CopyCursor (hcursor)) == NULL)
	WIN32_API_FAILED ("CopyCursor");
      GDK_NOTE (MISC, g_print ("...CopyCursor (%p) = %p\n",
			       hcursor, impl->hcursor));
    }

  /* If the pointer is over our window, set new cursor if given */
  if (gdk_window_get_pointer(window, NULL, NULL, NULL) == window)
    if (impl->hcursor != NULL)
      SetCursor (impl->hcursor);

  /* Destroy the previous cursor: Need to make sure it's no longer in
   * use before we destroy it, in case we're not over our window but
   * the cursor is still set to our old one.
   */
  if (hprevcursor != NULL)
    {
      if (GetCursor() == hprevcursor)
	{
	  /* Look for a suitable cursor to use instead */
	  hcursor = NULL;
          parent_window = GDK_WINDOW_OBJECT (window)->parent;
          while (hcursor == NULL)
	    {
	      if (parent_window)
		{
		  impl = GDK_WINDOW_IMPL_WIN32 (parent_window->impl);
		  hcursor = impl->hcursor;
		  parent_window = parent_window->parent;
		}
	      else
		{
		  hcursor = LoadCursor (NULL, IDC_ARROW);
		}
	    }
          SetCursor (hcursor);
        }

      GDK_NOTE (MISC, g_print ("...DestroyCursor (%p)\n",
			       hprevcursor));
      
      if (!DestroyCursor (hprevcursor))
	WIN32_API_FAILED ("DestroyCursor");
    }
}

void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));
  
  if (!window)
    window = _gdk_parent_root;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      RECT rect;

      if (!GetClientRect (GDK_WINDOW_HWND (window), &rect))
	WIN32_API_FAILED ("GetClientRect");

      if (window != _gdk_parent_root)
	{
	  POINT pt;
	  GdkWindow *parent = gdk_window_get_parent (window);

	  pt.x = rect.left;
	  pt.y = rect.top;
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  ScreenToClient (GDK_WINDOW_HWND (parent), &pt);
	  rect.left = pt.x;
	  rect.top = pt.y;

	  pt.x = rect.right;
	  pt.y = rect.bottom;
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  ScreenToClient (GDK_WINDOW_HWND (parent), &pt);
	  rect.right = pt.x;
	  rect.bottom = pt.y;
	}

      if (x)
	*x = rect.left;
      if (y)
	*y = rect.top;
      if (width)
	*width = rect.right - rect.left;
      if (height)
	*height = rect.bottom - rect.top;
      if (depth)
	*depth = gdk_drawable_get_visual (window)->depth;
    }
}

gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  gint return_val;
  gint tx = 0;
  gint ty = 0;

  g_return_val_if_fail (window != NULL, 0);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      POINT pt;

      pt.x = 0;
      pt.y = 0;
      ClientToScreen (GDK_WINDOW_HWND (window), &pt);
      tx = pt.x;
      ty = pt.y;
      return_val = 1;
    }
  else
    return_val = 0;
  
  if (x)
    *x = tx;
  if (y)
    *y = ty;

  GDK_NOTE (MISC, g_print ("gdk_window_get_origin: %p: +%d+%d\n",
			   GDK_WINDOW_HWND (window),
			   tx, ty));
  return return_val;
}

gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
				    gint      *x,
				    gint      *y)
{
  return gdk_window_get_origin (window, x, y);
}

void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  GdkRectangle rect;

  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_window_get_frame_extents (window, &rect);

  if (x)
    *x = rect.x;

  if (y)
    *y = rect.y;
}

void
gdk_window_get_frame_extents (GdkWindow    *window,
                              GdkRectangle *rect)
{
  GdkWindowObject *private;
  HWND hwnd;
  RECT r;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (rect != NULL);

  private = GDK_WINDOW_OBJECT (window);

  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  while (private->parent && ((GdkWindowObject*) private->parent)->parent)
    private = (GdkWindowObject*) private->parent;

  hwnd = GDK_WINDOW_HWND (window);

  /* find the frame window */
  while (HWND_DESKTOP != GetParent (hwnd))
    {
      hwnd = GetParent (hwnd);
      g_return_if_fail (NULL != hwnd);
    }

  if (!GetWindowRect (hwnd, &r))
    WIN32_API_FAILED ("GetWindowRect");

  rect->x = r.left;
  rect->y = r.top;
  rect->width = r.right - r.left;
  rect->height = r.bottom - r.top;
}

GdkWindow*
_gdk_windowing_window_get_pointer (GdkDisplay      *display,
				   GdkWindow       *window,
				   gint            *x,
				   gint            *y,
				   GdkModifierType *mask)
{
  GdkWindow *return_val;
  POINT screen_point, point;
  HWND hwnd, hwndc;
  BYTE kbd[256];

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);
  
  return_val = NULL;
  GetCursorPos (&screen_point);
  point = screen_point;
  ScreenToClient (GDK_WINDOW_HWND (window), &point);

  *x = point.x;
  *y = point.y;

  hwnd = WindowFromPoint (point);
  if (hwnd != NULL)
    {
      gboolean done = FALSE;
      
      while (!done)
	{
	  point = screen_point;
	  ScreenToClient (hwnd, &point);
	  hwndc = ChildWindowFromPoint (hwnd, point);
	  if (hwndc == NULL)
	    done = TRUE;
	  else if (hwndc == hwnd)
	    done = TRUE;
	  else
	    hwnd = hwndc;
	}
      
      return_val = gdk_window_lookup ((GdkNativeWindow) hwnd);
    }
  else
    return_val = NULL;
      
  GetKeyboardState (kbd);
  *mask = 0;
  if (kbd[VK_SHIFT] & 0x80)
    *mask |= GDK_SHIFT_MASK;
  if (kbd[VK_CAPITAL] & 0x80)
    *mask |= GDK_LOCK_MASK;
  if (kbd[VK_CONTROL] & 0x80)
    *mask |= GDK_CONTROL_MASK;
  if (kbd[VK_MENU] & 0x80)
    *mask |= GDK_MOD1_MASK;
  if (kbd[VK_LBUTTON] & 0x80)
    *mask |= GDK_BUTTON1_MASK;
  if (kbd[VK_MBUTTON] & 0x80)
    *mask |= GDK_BUTTON2_MASK;
  if (kbd[VK_RBUTTON] & 0x80)
    *mask |= GDK_BUTTON3_MASK;
  
  return return_val;
}

void
_gdk_windowing_get_pointer (GdkDisplay       *display,
			    GdkScreen       **screen,
			    gint             *x,
			    gint             *y,
			    GdkModifierType  *mask)
{
  GdkScreen *default_screen = gdk_display_get_default_screen (display);
  GdkWindow *root_window = gdk_screen_get_root_window (default_screen);
  
  *screen = default_screen;
  _gdk_windowing_window_get_pointer (display, root_window, x, y, mask);
}

GdkWindow*
_gdk_windowing_window_at_pointer (GdkDisplay *display,
				  gint       *win_x,
				  gint       *win_y)
{
  GdkWindow *window;
  POINT point, pointc;
  HWND hwnd, hwndc;
  RECT rect;

  GetCursorPos (&pointc);
  point = pointc;
  hwnd = WindowFromPoint (point);

  if (hwnd == NULL)
    {
      window = _gdk_parent_root;
      *win_x = pointc.x;
      *win_y = pointc.y;
      return window;
    }
      
  ScreenToClient (hwnd, &point);

  do {
    hwndc = ChildWindowFromPoint (hwnd, point);
    ClientToScreen (hwnd, &point);
    ScreenToClient (hwndc, &point);
  } while (hwndc != hwnd && (hwnd = hwndc, 1));

  window = gdk_win32_handle_table_lookup ((GdkNativeWindow) hwnd);

  if (window && (win_x || win_y))
    {
      GetClientRect (hwnd, &rect);
      *win_x = point.x - rect.left;
      *win_y = point.y - rect.top;
    }

  GDK_NOTE (MISC, g_print ("gdk_window_at_pointer: +%ld+%ld %p%s\n",
			   point.x, point.y,
			   hwnd,
			   (window == NULL ? " NULL" : "")));

  return window;
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;

  return GDK_WINDOW_OBJECT (window)->event_mask;
}

void          
gdk_window_set_events (GdkWindow   *window,
		       GdkEventMask event_mask)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* gdk_window_new() always sets the GDK_STRUCTURE_MASK, so better
   * set it here, too. Not that I know or remember why it is
   * necessary, will have to test some day.
   */
  GDK_WINDOW_OBJECT (window)->event_mask = GDK_STRUCTURE_MASK | event_mask;
}

void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint x, gint y)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!mask)
    {
      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_mask: %p none\n",
			       GDK_WINDOW_HWND (window)));
      SetWindowRgn (GDK_WINDOW_HWND (window), NULL, TRUE);
    }
  else
    {
      HRGN hrgn;
      DWORD dwStyle;
      DWORD dwExStyle;
      RECT rect;

      /* Convert mask bitmap to region */
      hrgn = _gdk_win32_bitmap_to_hrgn (mask);

      GDK_NOTE (MISC, g_print ("gdk_window_shape_combine_mask: %p %p\n",
			       GDK_WINDOW_HWND (window),
			       GDK_WINDOW_HWND (mask)));

      /* SetWindowRgn wants window (not client) coordinates */ 
      dwStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
      dwExStyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
      GetClientRect (GDK_WINDOW_HWND (window), &rect);
      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);
      OffsetRgn (hrgn, -rect.left, -rect.top);

      OffsetRgn (hrgn, x, y);

      /* If this is a top-level window, add the title bar to the region */
      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL)
	{
	  HRGN tmp = CreateRectRgn (0, 0, rect.right - rect.left, -rect.top);
	  CombineRgn (hrgn, hrgn, tmp, RGN_OR);
	  DeleteObject (tmp);
	}
      
      SetWindowRgn (GDK_WINDOW_HWND (window), hrgn, TRUE);
    }
}

void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean   override_redirect)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  g_warning ("gdk_window_set_override_redirect not implemented");
}

void          
gdk_window_set_icon_list (GdkWindow *window,
			  GList     *pixbufs)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* We could convert it to a hIcon and DrawIcon () it when getting
   * a WM_PAINT with IsIconic, but is it worth it ? Same probably
   * goes for gdk_window_set_icon (). Patches accepted :-)  --hb
   * Or do we only need to deliver the Icon on WM_GETICON ?
   */
}

void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  /* Nothing to do, really. As we share window classes between windows
   * we can't have window-specific icons, sorry. Don't print any warning
   * either.
   */
}

void
gdk_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (!SetWindowText (GDK_WINDOW_HWND (window), name))
    WIN32_API_FAILED ("SetWindowText");
}

void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (leader != NULL);
  g_return_if_fail (GDK_IS_WINDOW (leader));

  if (GDK_WINDOW_DESTROYED (window) || GDK_WINDOW_DESTROYED (leader))
    return;
  
  g_warning ("gdk_window_set_group not implemented");
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  LONG style;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_decorations: %p %s%s%s%s%s%s%s\n",
			   GDK_WINDOW_HWND (window),
			   (decorations & GDK_DECOR_ALL ? "ALL " : ""),
			   (decorations & GDK_DECOR_BORDER ? "BORDER " : ""),
			   (decorations & GDK_DECOR_RESIZEH ? "RESIZEH " : ""),
			   (decorations & GDK_DECOR_TITLE ? "TITLE " : ""),
			   (decorations & GDK_DECOR_MENU ? "MENU " : ""),
			   (decorations & GDK_DECOR_MINIMIZE ? "MINIMIZE " : ""),
			   (decorations & GDK_DECOR_MAXIMIZE ? "MAXIMIZE " : "")));

  style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);

  style &= (WS_OVERLAPPED|WS_POPUP|WS_CHILD|WS_MINIMIZE|WS_VISIBLE|WS_DISABLED
	    |WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZE);

  if (decorations & GDK_DECOR_ALL)
    style |= (WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
  if (decorations & GDK_DECOR_BORDER)
    style |= (WS_BORDER);
  if (decorations & GDK_DECOR_RESIZEH)
    style |= (WS_THICKFRAME);
  if (decorations & GDK_DECOR_TITLE)
    style |= (WS_CAPTION);
  if (decorations & GDK_DECOR_MENU)
    style |= (WS_SYSMENU);
  if (decorations & GDK_DECOR_MINIMIZE)
    style |= (WS_MINIMIZEBOX);
  if (decorations & GDK_DECOR_MAXIMIZE)
    style |= (WS_MAXIMIZEBOX);
  
  SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, style);
  SetWindowPos (GDK_WINDOW_HWND (window), NULL, 0, 0, 0, 0,
		SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE |
		SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  LONG style;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_functions: %p %s%s%s%s%s%s\n",
			   GDK_WINDOW_HWND (window),
			   (functions & GDK_FUNC_ALL ? "ALL " : ""),
			   (functions & GDK_FUNC_RESIZE ? "RESIZE " : ""),
			   (functions & GDK_FUNC_MOVE ? "MOVE " : ""),
			   (functions & GDK_FUNC_MINIMIZE ? "MINIMIZE " : ""),
			   (functions & GDK_FUNC_MAXIMIZE ? "MAXIMIZE " : ""),
			   (functions & GDK_FUNC_CLOSE ? "CLOSE " : "")));

  style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);

  style &= (WS_OVERLAPPED|WS_POPUP|WS_CHILD|WS_MINIMIZE|WS_VISIBLE|WS_DISABLED
	    |WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZE|WS_CAPTION|WS_BORDER
	    |WS_SYSMENU);

  if (functions & GDK_FUNC_ALL)
    style |= (WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
  if (functions & GDK_FUNC_RESIZE)
    style |= (WS_THICKFRAME);
  if (functions & GDK_FUNC_MOVE)
    style |= (WS_THICKFRAME);
  if (functions & GDK_FUNC_MINIMIZE)
    style |= (WS_MINIMIZEBOX);
  if (functions & GDK_FUNC_MAXIMIZE)
    style |= (WS_MAXIMIZEBOX);
  
  SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, style);
  SetWindowPos (GDK_WINDOW_HWND (window), NULL, 0, 0, 0, 0,
		SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE |
		SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
}

static void
QueryTree (HWND   hwnd,
	   HWND **children,
	   gint  *nchildren)
{
  guint i, n;
  HWND child;

  n = 0;
  do {
    if (n == 0)
      child = GetWindow (hwnd, GW_CHILD);
    else
      child = GetWindow (child, GW_HWNDNEXT);
    if (child != NULL)
      n++;
  } while (child != NULL);

  if (n > 0)
    {
      *children = g_new (HWND, n);
      for (i = 0; i < n; i++)
	{
	  if (i == 0)
	    child = GetWindow (hwnd, GW_CHILD);
	  else
	    child = GetWindow (child, GW_HWNDNEXT);
	  *children[i] = child;
	}
    }
}

static void
gdk_propagate_shapes (HANDLE   win,
		      gboolean merge)
{
   RECT emptyRect;
   HRGN region, childRegion;
   HWND *list = NULL;
   gint i, num;

   SetRectEmpty (&emptyRect);
   region = CreateRectRgnIndirect (&emptyRect);
   if (merge)
     GetWindowRgn (win, region);
   
   QueryTree (win, &list, &num);
   if (list != NULL)
     {
       WINDOWPLACEMENT placement;

       placement.length = sizeof (WINDOWPLACEMENT);
       /* go through all child windows and combine regions */
       for (i = 0; i < num; i++)
	 {
	   GetWindowPlacement (list[i], &placement);
	   if (placement.showCmd == SW_SHOWNORMAL)
	     {
	       childRegion = CreateRectRgnIndirect (&emptyRect);
	       GetWindowRgn (list[i], childRegion);
	       CombineRgn (region, region, childRegion, RGN_OR);
	       DeleteObject (childRegion);
	     }
	  }
       SetWindowRgn (win, region, TRUE);
       g_free (list);
     }
   else
     DeleteObject (region);
}

void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
   
  if (GDK_WINDOW_DESTROYED (window))
    return;

  gdk_propagate_shapes (GDK_WINDOW_HWND (window), FALSE);
}

void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  gdk_propagate_shapes (GDK_WINDOW_HWND (window), TRUE);
}

gboolean 
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (!use_static == !private->guffaw_gravity)
    return TRUE;
  
  if (use_static)
    return FALSE;
  
  private->guffaw_gravity = use_static;
  
  return TRUE;
}

/*
 * Setting window states
 */
void
gdk_window_iconify (GdkWindow *window)
{
  HWND old_active_window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_iconify: %p %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      old_active_window = GetActiveWindow ();
      ShowWindow (GDK_WINDOW_HWND (window), SW_MINIMIZE);
      if (old_active_window != GDK_WINDOW_HWND (window))
	SetActiveWindow (old_active_window);
    }
  else
    {
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_ICONIFIED);
    }
}

void
gdk_window_deiconify (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_deiconify: %p %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    {  
      show_window_internal (window, FALSE, TRUE);
    }
  else
    {
      gdk_synthesize_window_state (window,
                                   GDK_WINDOW_STATE_ICONIFIED,
                                   0);
    }
}

void
gdk_window_stick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* FIXME: Do something? */
}

void
gdk_window_unstick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* FIXME: Do something? */
}

void
gdk_window_maximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_maximize: %p %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    ShowWindow (GDK_WINDOW_HWND (window), SW_MAXIMIZE);
  else
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_MAXIMIZED);
}

void
gdk_window_unmaximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_unmaximize: %p %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    ShowWindow (GDK_WINDOW_HWND (window), SW_RESTORE);
  else
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_MAXIMIZED,
				 0);
}

void
gdk_window_fullscreen (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  g_warning ("gdk_window_fullscreen() not implemented.\n");
}

void
gdk_window_unfullscreen (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}

void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  GDK_NOTE (MISC, g_print ("gdk_window_focus: %p %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (((GdkWindowObject *) window)->state)));

  ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNORMAL);
  SetFocus (GDK_WINDOW_HWND (window));
}

void
gdk_window_set_modal_hint (GdkWindow *window,
			   gboolean   modal)
{
  GdkWindowObject *private;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = (GdkWindowObject*) window;

  private->modal_hint = modal;

  if (GDK_WINDOW_IS_MAPPED (window))
    if (!SetWindowPos (GDK_WINDOW_HWND (window), HWND_TOPMOST,
		       0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE))
      WIN32_API_FAILED ("SetWindowPos");
}

void
gdk_window_set_skip_taskbar_hint (GdkWindow *window,
				  gboolean   skips_taskbar)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}

void
gdk_window_set_skip_pager_hint (GdkWindow *window,
				gboolean   skips_pager)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}

void
gdk_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_DIALOG:
      break;
    case GDK_WINDOW_TYPE_HINT_MENU:
      break;
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
      break;
    default:
      g_warning ("Unknown hint %d passed to gdk_window_set_type_hint", hint);
      /* Fall thru */
    case GDK_WINDOW_TYPE_HINT_NORMAL:
      break;
    }
  /*
   * XXX ???
   */
  GDK_NOTE (MISC, g_print ("gdk_window_set_type_hint: %p\n",
			   GDK_WINDOW_HWND (window)));
}

void
gdk_window_shape_combine_region (GdkWindow *window,
                                 GdkRegion *shape_region,
                                 gint       offset_x,
                                 gint       offset_y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* XXX: even on X implemented conditional ... */  
}

void
gdk_window_begin_resize_drag (GdkWindow     *window,
                              GdkWindowEdge  edge,
                              gint           button,
                              gint           root_x,
                              gint           root_y,
                              guint32        timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* XXX: isn't all this default on win32 ... */  
}

void
gdk_window_begin_move_drag (GdkWindow *window,
                            gint       button,
                            gint       root_x,
                            gint       root_y,
                            guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* XXX: isn't all this default on win32 ... */  
}

GdkWindow *
gdk_window_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  g_return_val_if_fail (display == gdk_display_get_default(), NULL);

  return gdk_window_lookup (anid);
}
