/* MS-Windows Engine (aka GTK-Wimp)
 *
 * Copyright (C) 2003, 2004 Raymond Penners <raymond@dotsphinx.com>
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

#include "xp_theme.h"

#include <windows.h>
#include <math.h>
#include <string.h>
#include <gdk/gdkwin32.h>

#include <stdio.h>

#ifdef DONT_HAVE_UXTHEME_H
#include "xp_theme_defs.h"
#else
#include <uxtheme.h>
#include <tmschema.h>
#endif

static const LPCWSTR class_descriptors[] =
{
  L"Scrollbar", /* XP_THEME_CLASS_SCROLLBAR */
  L"Button",    /* XP_THEME_CLASS_BUTTON */
  L"Header",    /* XP_THEME_CLASS_HEADER */
  L"ComboBox",  /* XP_THEME_CLASS_COMBOBOX */
  L"Tab",       /* XP_THEME_CLASS_TAB */
  L"Edit",      /* XP_THEME_CLASS_EDIT */
  L"TreeView",  /* XP_THEME_CLASS_TREEVIEW */
  L"Spin",      /* XP_THEME_CLASS_SPIN */
  L"Progress",  /* XP_THEME_CLASS_PROGRESS */
  L"Tooltip",   /* XP_THEME_CLASS_TOOLTIP */
  L"Rebar",     /* XP_THEME_CLASS_REBAR */
  L"Toolbar",   /* XP_THEME_CLASS_TOOLBAR */
  L"Globals",   /* XP_THEME_CLASS_GLOBALS */
  L"Menu",      /* XP_THEME_CLASS_MENU */
  L"Window",    /* XP_THEME_CLASS_WINDOW */
  L"Status"     /* XP_THEME_CLASS_STATUS */
};

static const short element_part_map[]=
{
  BP_CHECKBOX,
  BP_CHECKBOX,
  BP_PUSHBUTTON,
  HP_HEADERITEM,
  CP_DROPDOWNBUTTON,
  TABP_BODY,
  TABP_TABITEM,
  TABP_TABITEMLEFTEDGE,
  TABP_PANE,
  SBP_THUMBBTNHORZ,
  SBP_THUMBBTNVERT,
  SBP_ARROWBTN,
  SBP_ARROWBTN,
  SBP_ARROWBTN,
  SBP_ARROWBTN,
  SBP_GRIPPERHORZ,
  SBP_GRIPPERVERT,
  SBP_LOWERTRACKHORZ,
  SBP_LOWERTRACKVERT,
  EP_EDITTEXT,
  BP_PUSHBUTTON,
  SPNP_UP,
  SPNP_DOWN,
  BP_RADIOBUTTON,
  BP_RADIOBUTTON,
  TVP_GLYPH,
  TVP_GLYPH,
  PP_CHUNK,
  PP_CHUNKVERT,
  PP_BAR,
  PP_BARVERT,
  TTP_STANDARD,
  RP_BAND,
  RP_GRIPPER,
  RP_GRIPPERVERT,
  RP_CHEVRON,
  TP_BUTTON,
  MP_MENUITEM,
  MP_SEPARATOR,
  SP_GRIPPER,
  SP_PANE
};

static HINSTANCE uxtheme_dll = NULL;
static HTHEME open_themes[XP_THEME_CLASS__SIZEOF];

typedef HRESULT (FAR PASCAL *GetThemeSysFontFunc)
     (HTHEME hTheme, int iFontID, OUT LOGFONT *plf);
typedef int (FAR PASCAL *GetThemeSysSizeFunc)
	(HTHEME hTheme, int iSizeId);
typedef COLORREF (FAR PASCAL *GetThemeSysColorFunc)(HTHEME hTheme, int iColorID);
typedef HTHEME (FAR PASCAL *OpenThemeDataFunc)
     (HWND hwnd, LPCWSTR pszClassList);
typedef HRESULT (FAR PASCAL *CloseThemeDataFunc)(HTHEME theme);
typedef HRESULT (FAR PASCAL *DrawThemeBackgroundFunc)
     (HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
      const RECT *pRect, const RECT *pClipRect);
typedef HRESULT (FAR PASCAL *EnableThemeDialogTextureFunc)(HWND hwnd, DWORD dwFlags);
typedef BOOL (FAR PASCAL *IsThemeActiveFunc)(VOID);

static GetThemeSysFontFunc get_theme_sys_font_func = NULL;
static GetThemeSysColorFunc get_theme_sys_color_func = NULL;
static GetThemeSysSizeFunc get_theme_sys_metric_func = NULL;
static OpenThemeDataFunc open_theme_data_func = NULL;
static CloseThemeDataFunc close_theme_data_func = NULL;
static DrawThemeBackgroundFunc draw_theme_background_func = NULL;
static EnableThemeDialogTextureFunc enable_theme_dialog_texture_func = NULL;
static IsThemeActiveFunc is_theme_active_func = NULL;

static gboolean was_theming_active = FALSE;

static void
xp_theme_close_open_handles (void)
{
  int i;

  for (i=0; i < XP_THEME_CLASS__SIZEOF; i++)
    {
      if (open_themes[i])
        {
          close_theme_data_func (open_themes[i]);
          open_themes[i] = NULL;
	}
    }
}

void
xp_theme_init (void)
{
  if (uxtheme_dll)
    return;

  memset(open_themes, 0, sizeof(open_themes));

  uxtheme_dll = LoadLibrary("uxtheme.dll");
  if (!uxtheme_dll) {
	  was_theming_active = FALSE;
	  return;
  }

  is_theme_active_func = (IsThemeActiveFunc) GetProcAddress(uxtheme_dll, "IsThemeActive");
  open_theme_data_func = (OpenThemeDataFunc) GetProcAddress(uxtheme_dll, "OpenThemeData");
  close_theme_data_func = (CloseThemeDataFunc) GetProcAddress(uxtheme_dll, "CloseThemeData");
  draw_theme_background_func = (DrawThemeBackgroundFunc) GetProcAddress(uxtheme_dll, "DrawThemeBackground");
  enable_theme_dialog_texture_func = (EnableThemeDialogTextureFunc) GetProcAddress(uxtheme_dll, "EnableThemeDialogTexture");
  get_theme_sys_font_func = (GetThemeSysFontFunc) GetProcAddress(uxtheme_dll, "GetThemeSysFont");
  get_theme_sys_color_func = (GetThemeSysColorFunc) GetProcAddress(uxtheme_dll, "GetThemeSysColor");
  get_theme_sys_metric_func = (GetThemeSysSizeFunc) GetProcAddress(uxtheme_dll, "GetThemeSysSize");

  if (is_theme_active_func)
    {
      was_theming_active = (*is_theme_active_func) ();
    }
}

void
xp_theme_reset (void)
{
  xp_theme_close_open_handles ();
  was_theming_active = is_theme_active_func
    ? (*is_theme_active_func) () : FALSE;
}

void
xp_theme_exit (void)
{
  if (! uxtheme_dll)
    return;

  xp_theme_close_open_handles ();

  FreeLibrary (uxtheme_dll);
  uxtheme_dll = NULL;

  is_theme_active_func = NULL;
  open_theme_data_func = NULL;
  close_theme_data_func = NULL;
  draw_theme_background_func = NULL;
  enable_theme_dialog_texture_func = NULL;
  get_theme_sys_font_func = NULL;
  get_theme_sys_color_func = NULL;
  get_theme_sys_metric_func = NULL;
}

static HTHEME
xp_theme_get_handle_by_class (XpThemeClass klazz)
{
  if (!open_themes[klazz] && open_theme_data_func)
    {
      open_themes[klazz] = open_theme_data_func(NULL, class_descriptors[klazz]);
    }
  return open_themes[klazz];
}

static HTHEME
xp_theme_get_handle_by_element (XpThemeElement element)
{
  HTHEME ret = NULL;
  XpThemeClass klazz = XP_THEME_CLASS__SIZEOF;

  switch(element)
    {
    case XP_THEME_ELEMENT_TOOLTIP:
      klazz = XP_THEME_CLASS_TOOLTIP;
      break;

    case XP_THEME_ELEMENT_REBAR:
    case XP_THEME_ELEMENT_REBAR_GRIPPER_H:
    case XP_THEME_ELEMENT_REBAR_GRIPPER_V:
    case XP_THEME_ELEMENT_REBAR_CHEVRON:
      klazz = XP_THEME_CLASS_REBAR;
      break;

    case XP_THEME_ELEMENT_STATUS_GRIPPER:
    case XP_THEME_ELEMENT_STATUS_PANE:
      klazz = XP_THEME_CLASS_STATUS;
      break;

	case XP_THEME_ELEMENT_TOOLBAR_BUTTON:
      klazz = XP_THEME_CLASS_TOOLBAR;
      break;

    case XP_THEME_ELEMENT_MENU_ITEM:
    case XP_THEME_ELEMENT_MENU_SEPARATOR:
      klazz = XP_THEME_CLASS_MENU;
      break;

    case XP_THEME_ELEMENT_PRESSED_CHECKBOX:
    case XP_THEME_ELEMENT_CHECKBOX:
    case XP_THEME_ELEMENT_BUTTON:
    case XP_THEME_ELEMENT_DEFAULT_BUTTON:
    case XP_THEME_ELEMENT_PRESSED_RADIO_BUTTON:
    case XP_THEME_ELEMENT_RADIO_BUTTON:
      klazz = XP_THEME_CLASS_BUTTON;
      break;

    case XP_THEME_ELEMENT_LIST_HEADER:
      klazz = XP_THEME_CLASS_HEADER;
      break;

    case XP_THEME_ELEMENT_COMBOBUTTON:
      klazz = XP_THEME_CLASS_COMBOBOX;
      break;

    case XP_THEME_ELEMENT_BODY:
    case XP_THEME_ELEMENT_TAB_ITEM:
    case XP_THEME_ELEMENT_TAB_ITEM_LEFT_EDGE:
    case XP_THEME_ELEMENT_TAB_PANE:
      klazz = XP_THEME_CLASS_TAB;
      break;

    case XP_THEME_ELEMENT_SCROLLBAR_V:
    case XP_THEME_ELEMENT_SCROLLBAR_H:
    case XP_THEME_ELEMENT_ARROW_UP:
    case XP_THEME_ELEMENT_ARROW_DOWN:
    case XP_THEME_ELEMENT_ARROW_LEFT:
    case XP_THEME_ELEMENT_ARROW_RIGHT:
    case XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_V:
    case XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_H:
    case XP_THEME_ELEMENT_TROUGH_V:
    case XP_THEME_ELEMENT_TROUGH_H:
      klazz = XP_THEME_CLASS_SCROLLBAR;
      break;

    case XP_THEME_ELEMENT_EDIT_TEXT:
      klazz = XP_THEME_CLASS_EDIT;
      break;

    case XP_THEME_ELEMENT_SPIN_BUTTON_UP:
    case XP_THEME_ELEMENT_SPIN_BUTTON_DOWN:
      klazz = XP_THEME_CLASS_SPIN;
      break;

    case XP_THEME_ELEMENT_PROGRESS_BAR_H:
    case XP_THEME_ELEMENT_PROGRESS_BAR_V:
    case XP_THEME_ELEMENT_PROGRESS_TROUGH_H:
    case XP_THEME_ELEMENT_PROGRESS_TROUGH_V:
      klazz = XP_THEME_CLASS_PROGRESS;
      break;

    case XP_THEME_ELEMENT_TREEVIEW_EXPANDER_OPENED:
    case XP_THEME_ELEMENT_TREEVIEW_EXPANDER_CLOSED:
      klazz = XP_THEME_CLASS_TREEVIEW;
      break;
    }

  if (klazz != XP_THEME_CLASS__SIZEOF)
    {
      ret = xp_theme_get_handle_by_class (klazz);
    }
  return ret;
}

static int
xp_theme_map_gtk_state (XpThemeElement element, GtkStateType state)
{
  int ret;

  switch(element)
    {
    case XP_THEME_ELEMENT_TOOLTIP:
      ret = TTSS_NORMAL;
      break;

    case XP_THEME_ELEMENT_REBAR:
    case XP_THEME_ELEMENT_REBAR_GRIPPER_H:
    case XP_THEME_ELEMENT_REBAR_GRIPPER_V:
      ret = CHEVS_NORMAL;
      break;

	case XP_THEME_ELEMENT_STATUS_GRIPPER:
	case XP_THEME_ELEMENT_STATUS_PANE:
		ret = 1;
		break;

    case XP_THEME_ELEMENT_REBAR_CHEVRON:
      switch (state)
	{
        case GTK_STATE_PRELIGHT:
          ret =  CHEVS_HOT;
          break;
        case GTK_STATE_SELECTED:
        case GTK_STATE_ACTIVE:
          ret =  CHEVS_PRESSED;
          break;
        default:
          ret =  CHEVS_NORMAL;
	}
      break;

	case XP_THEME_ELEMENT_TOOLBAR_BUTTON:
		switch (state)
		{
        case GTK_STATE_ACTIVE:
          ret =  TS_PRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  TS_HOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  TS_DISABLED;
          break;
        default:
          ret =  TS_NORMAL;
		}
		break;

    case XP_THEME_ELEMENT_TAB_PANE:
      ret = 1;
      break;

    case XP_THEME_ELEMENT_TAB_ITEM_LEFT_EDGE:
    case XP_THEME_ELEMENT_TAB_ITEM:
      switch(state)
        {
        case GTK_STATE_PRELIGHT:
          ret =  TIS_HOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  TIS_DISABLED;
          break;
        case GTK_STATE_SELECTED:
        case GTK_STATE_ACTIVE:
          ret =  TIS_NORMAL;
          break;
        default:
          ret =  TIS_SELECTED;
        }
      break;

    case XP_THEME_ELEMENT_EDIT_TEXT:
      switch(state)
        {
        case GTK_STATE_PRELIGHT:
          ret =  ETS_FOCUSED;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  ETS_READONLY;
          break;
        case GTK_STATE_SELECTED:
        case GTK_STATE_ACTIVE:
        default:
          ret =  ETS_NORMAL;
        }
      break;

    case XP_THEME_ELEMENT_SCROLLBAR_H:
    case XP_THEME_ELEMENT_SCROLLBAR_V:
    case XP_THEME_ELEMENT_TROUGH_H:
    case XP_THEME_ELEMENT_TROUGH_V:
      switch(state)
        {
        case GTK_STATE_SELECTED:
        case GTK_STATE_ACTIVE:
          ret =  SCRBS_PRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  SCRBS_HOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  SCRBS_DISABLED;
          break;
        default:
          ret =  SCRBS_NORMAL;
        }
      break;

    case XP_THEME_ELEMENT_ARROW_DOWN:
      switch(state)
        {
        case GTK_STATE_ACTIVE:
          ret =  ABS_DOWNPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  ABS_DOWNHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  ABS_DOWNDISABLED;
          break;
        default:
          ret =  ABS_DOWNNORMAL;
        }
      break;

    case XP_THEME_ELEMENT_ARROW_UP:
      switch(state)
        {
        case GTK_STATE_ACTIVE:
          ret =  ABS_UPPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  ABS_UPHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  ABS_UPDISABLED;
          break;
        default:
          ret =  ABS_UPNORMAL;
        }
      break;

    case XP_THEME_ELEMENT_ARROW_LEFT:
      switch(state)
        {
        case GTK_STATE_ACTIVE:
          ret =  ABS_LEFTPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  ABS_LEFTHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  ABS_LEFTDISABLED;
          break;
        default:
          ret =  ABS_LEFTNORMAL;
        }
      break;

    case XP_THEME_ELEMENT_ARROW_RIGHT:
      switch(state)
        {
        case GTK_STATE_ACTIVE:
          ret =  ABS_RIGHTPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  ABS_RIGHTHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  ABS_RIGHTDISABLED;
          break;
        default:
          ret =  ABS_RIGHTNORMAL;
        }
      break;

    case XP_THEME_ELEMENT_CHECKBOX:
    case XP_THEME_ELEMENT_RADIO_BUTTON:
      switch(state)
        {
        case GTK_STATE_SELECTED:
          ret =  CBS_UNCHECKEDPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  CBS_UNCHECKEDHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  CBS_UNCHECKEDDISABLED;
          break;
        default:
          ret =  CBS_UNCHECKEDNORMAL;
        }
      break;

    case XP_THEME_ELEMENT_PRESSED_CHECKBOX:
    case XP_THEME_ELEMENT_PRESSED_RADIO_BUTTON:
      switch(state)
        {
        case GTK_STATE_SELECTED:
          ret =  CBS_CHECKEDPRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  CBS_CHECKEDHOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  CBS_CHECKEDDISABLED;
          break;
        default:
          ret =  CBS_CHECKEDNORMAL;
        }
      break;

    XP_THEME_ELEMENT_DEFAULT_BUTTON:
      switch(state)
        {
        case GTK_STATE_ACTIVE:
          ret =  PBS_PRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  PBS_HOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  PBS_DISABLED;
          break;
        default:
          ret =  PBS_DEFAULTED;
        }
      break;

    case XP_THEME_ELEMENT_SPIN_BUTTON_DOWN:
      switch(state)
        {
        case GTK_STATE_ACTIVE:
          ret =  DNS_PRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  DNS_HOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  DNS_DISABLED;
          break;
        default:
          ret =  DNS_NORMAL;
        }
      break;

    case XP_THEME_ELEMENT_SPIN_BUTTON_UP:
      switch(state)
        {
        case GTK_STATE_ACTIVE:
          ret =  UPS_PRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  UPS_HOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  UPS_DISABLED;
          break;
        default:
          ret =  UPS_NORMAL;
        }
      break;

    case XP_THEME_ELEMENT_TREEVIEW_EXPANDER_OPENED:
      ret = GLPS_OPENED;
      break;

    case XP_THEME_ELEMENT_TREEVIEW_EXPANDER_CLOSED:
      ret = GLPS_CLOSED;
      break;

    case XP_THEME_ELEMENT_PROGRESS_BAR_H:
    case XP_THEME_ELEMENT_PROGRESS_BAR_V:
    case XP_THEME_ELEMENT_PROGRESS_TROUGH_H:
    case XP_THEME_ELEMENT_PROGRESS_TROUGH_V:
      ret = 1;
      break;

	case XP_THEME_ELEMENT_MENU_ITEM:
	case XP_THEME_ELEMENT_MENU_SEPARATOR:
		switch(state) {
			case GTK_STATE_SELECTED:
				ret = MS_SELECTED;
				break;
			case GTK_STATE_INSENSITIVE:
				ret = MS_DEMOTED;
				break;
			default:
				ret = MS_NORMAL;
		}
		break;

    default:
      switch(state)
        {
        case GTK_STATE_ACTIVE:
          ret =  PBS_PRESSED;
          break;
        case GTK_STATE_PRELIGHT:
          ret =  PBS_HOT;
          break;
        case GTK_STATE_INSENSITIVE:
          ret =  PBS_DISABLED;
          break;
        default:
          ret =  PBS_NORMAL;
        }
    }
  return ret;
}

gboolean
xp_theme_draw (GdkWindow *win, XpThemeElement element, GtkStyle *style,
               int x, int y, int width, int height,
               GtkStateType state_type, GdkRectangle *area)
{
  HTHEME theme;
  RECT rect, clip, *pClip;
  int xoff, yoff, state;
  HDC dc;
  GdkDrawable *drawable;
  int part_state;

  if (! xp_theme_is_drawable (element))
    return FALSE;

  theme = xp_theme_get_handle_by_element(element);
  if (!theme)
    return FALSE;

  /* FIXME: Recheck its function */
  enable_theme_dialog_texture_func(GDK_WINDOW_HWND(win), ETDT_ENABLETAB);

  if (!GDK_IS_WINDOW(win))
    {
      xoff = 0;
      yoff = 0;
      drawable = win;
    }
  else
    {
      gdk_window_get_internal_paint_info(win, &drawable, &xoff, &yoff);
    }
  rect.left = x - xoff;
  rect.top = y - yoff;
  rect.right = rect.left + width;
  rect.bottom = rect.top + height;

  if (area)
    {
      clip.left = area->x - xoff;
      clip.top = area->y - yoff;
      clip.right = clip.left + area->width;
      clip.bottom = clip.top + area->height;

      pClip = &clip;
    }
  else
    {
      pClip = NULL;
    }

  gdk_gc_set_clip_rectangle (style->dark_gc[state_type], NULL);
  dc = gdk_win32_hdc_get(drawable, style->dark_gc[state_type], 0);
  if (!dc)
    return FALSE;

  part_state = xp_theme_map_gtk_state(element, state_type);

#ifdef GNATS_HACK
  if (element == XP_THEME_ELEMENT_REBAR_GRIPPER_V
      || element == XP_THEME_ELEMENT_REBAR_GRIPPER_H)
    {
      /* Hack alert: when XP draws a gripper, it does not seem to fill
         up the whole rectangle. It only fills the gripper line
         itself. Therefore we manually fill up the background here
         ourselves. I still have to look into this a bit further, as
         tests with GNAT Programming System show some awkward
         interference between this FillRect and the subsequent
         DrawThemeBackground(). */
      FillRect (dc, &rect, (HBRUSH) (COLOR_3DFACE+1));
    }
#endif

  draw_theme_background_func(theme, dc, element_part_map[element], part_state, &rect, pClip);
  gdk_win32_hdc_release(drawable, style->dark_gc[state_type], 0);

  return TRUE;
}

gboolean
xp_theme_is_drawable (XpThemeElement element)
{
  if (is_theme_active_func)
    {
      gboolean active = (*is_theme_active_func) ();
      /* A bit of a hack, but it at least detects theme
	 switches between XP and classic looks on systems
	 using older GTK+ version (2.2.0-?) that do not
	 support theme switch detection (gdk_window_add_filter). */
      if (active != was_theming_active)
        {
          xp_theme_reset ();
        }

      if (active)
        {
          return (xp_theme_get_handle_by_element (element) != NULL);
        }
    }

  return FALSE;
}

gboolean
xp_theme_get_system_font (XpThemeClass klazz, XpThemeFont fontId, LOGFONT *lf)
{
  int themeFont;

  if (get_theme_sys_font_func != NULL)
    {
      HTHEME theme = xp_theme_get_handle_by_class(klazz);
      if (!theme)
        return FALSE;

      switch (fontId)
        {
        case XP_THEME_FONT_CAPTION:
          themeFont = TMT_CAPTIONFONT; break;
        case XP_THEME_FONT_MENU:
          themeFont = TMT_MENUFONT; break;
        case XP_THEME_FONT_STATUS:
          themeFont = TMT_STATUSFONT; break;
        case XP_THEME_FONT_MESSAGE:
        default:
          themeFont = TMT_MSGBOXFONT; break;
        }
      /* if theme is NULL, it will just return the GetSystemFont() value */
      return ((*get_theme_sys_font_func)(theme, themeFont, lf) == S_OK);
    }
  return FALSE;
}

gboolean
xp_theme_get_system_color (XpThemeClass klazz, int colorId, DWORD * pColor)
{
  if (get_theme_sys_color_func != NULL)
    {
      HTHEME theme = xp_theme_get_handle_by_class(klazz);

      /* if theme is NULL, it will just return the GetSystemColor() value */
      *pColor = (*get_theme_sys_color_func)(theme, colorId);
      return TRUE;
    }
  return FALSE;
}

gboolean
xp_theme_get_system_metric (XpThemeClass klazz, int metricId, int * pVal)
{
  if (get_theme_sys_metric_func != NULL)
    {
      HTHEME theme = xp_theme_get_handle_by_class(klazz);

      /* if theme is NULL, it will just return the GetSystemMetrics() value */
      *pVal = (*get_theme_sys_metric_func)(theme, metricId);
      return TRUE;
    }
  return FALSE;
}
