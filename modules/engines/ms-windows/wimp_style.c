/* Wimp "Windows Impersonator" Engine
 *
 * Copyright (C) 2003 Raymond Penners <raymond@dotsphinx.com>
 * Includes code adapted from redmond95 by Owen Taylor, and
 * gtk-nativewin by Evan Martin
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

#include "wimp_style.h"
#include "xp_theme.h"

#include <windows.h>
#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>

#include <stdio.h>

/* Default values, not normally used
 */
static const GtkRequisition default_option_indicator_size = { 9, 8 };
static const GtkBorder default_option_indicator_spacing = { 7, 5, 2, 2 };

static GtkStyleClass *parent_class;

typedef enum {
  CHECK_AA,
  CHECK_BASE,
  CHECK_BLACK,
  CHECK_DARK,
  CHECK_LIGHT,
  CHECK_MID,
  CHECK_TEXT,
  RADIO_BASE,
  RADIO_BLACK,
  RADIO_DARK,
  RADIO_LIGHT,
  RADIO_MID,
  RADIO_TEXT
} Part;

#define PART_SIZE 13

static const char check_aa_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const char check_base_bits[] = {
 0x00,0x00,0x00,0x00,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,
 0x07,0xfc,0x07,0xfc,0x07,0xfc,0x07,0x00,0x00,0x00,0x00};
static const char check_black_bits[] = {
 0x00,0x00,0xfe,0x0f,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,
 0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x00,0x00};
static const char check_dark_bits[] = {
 0xff,0x1f,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,
 0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00};
static const char check_light_bits[] = {
 0x00,0x00,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,
 0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0xfe,0x1f};
static const char check_mid_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,
 0x08,0x00,0x08,0x00,0x08,0x00,0x08,0xfc,0x0f,0x00,0x00};
static const char check_text_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x03,0x88,0x03,0xd8,0x01,0xf8,
 0x00,0x70,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const char radio_base_bits[] = {
 0x00,0x00,0x00,0x00,0xf0,0x01,0xf8,0x03,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,
 0x07,0xfc,0x07,0xf8,0x03,0xf0,0x01,0x00,0x00,0x00,0x00};
static const char radio_black_bits[] = {
 0x00,0x00,0xf0,0x01,0x0c,0x02,0x04,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,
 0x00,0x02,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const char radio_dark_bits[] = {
 0xf0,0x01,0x0c,0x06,0x02,0x00,0x02,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,
 0x00,0x01,0x00,0x02,0x00,0x02,0x00,0x00,0x00,0x00,0x00};
static const char radio_light_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x10,0x00,0x10,0x00,0x10,0x00,
 0x10,0x00,0x10,0x00,0x08,0x00,0x08,0x0c,0x06,0xf0,0x01};
static const char radio_mid_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x04,0x00,0x08,0x00,0x08,0x00,0x08,0x00,
 0x08,0x00,0x08,0x00,0x04,0x0c,0x06,0xf0,0x01,0x00,0x00};
static const char radio_text_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0xf0,0x01,0xf0,0x01,0xf0,
 0x01,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static struct {
  const char *bits;
  GdkBitmap  *bmap;
} parts[] = {
  { check_aa_bits, NULL },
  { check_base_bits, NULL },
  { check_black_bits, NULL },
  { check_dark_bits, NULL },
  { check_light_bits, NULL },
  { check_mid_bits, NULL },
  { check_text_bits, NULL },
  { radio_base_bits, NULL },
  { radio_black_bits, NULL },
  { radio_dark_bits, NULL },
  { radio_light_bits, NULL },
  { radio_mid_bits, NULL },
  { radio_text_bits, NULL }
};

static gboolean
get_system_font(XpThemeClass klazz, XpThemeFont type, LOGFONT *out_lf)
{
#if 0
  /* TODO: this crashes. need to figure out why and how to fix it */
  if (xp_theme_get_system_font(klazz, type, out_lf)) {
	  return TRUE;
  } else
#endif
  {
	  NONCLIENTMETRICS ncm;
	  ncm.cbSize = sizeof(NONCLIENTMETRICS);

	  if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
				   sizeof(NONCLIENTMETRICS), &ncm, 0))
	    {
	      if (type == XP_THEME_FONT_CAPTION)
		*out_lf = ncm.lfCaptionFont;
	      else if (type == XP_THEME_FONT_MENU)
		*out_lf = ncm.lfMenuFont;
	      else if (type == XP_THEME_FONT_STATUS)
		*out_lf = ncm.lfStatusFont;
	      else
		*out_lf = ncm.lfMessageFont;

	      return TRUE;
	    }
  }
  return FALSE;
}

static char *
sys_font_to_pango_font (XpThemeClass klazz, XpThemeFont type, char * buf, size_t bufsiz)
{
  HDC hDC;
  HWND hwnd;
  LOGFONT lf;
  int pt_size;
  const char * weight;
  const char * style;

  if (get_system_font(klazz, type, &lf))
    {
      switch (lf.lfWeight) {
      case FW_THIN:
      case FW_EXTRALIGHT:
	weight = "Ultra-Light";
	break;

      case FW_LIGHT:
	weight = "Light";
	break;

      case FW_BOLD:
	weight = "Bold";
	break;

      case FW_SEMIBOLD:
	weight = "Semi-Bold";
	break;

      case FW_ULTRABOLD:
	weight = "Ultra-Bold";
	break;

      case FW_HEAVY:
	weight = "Heavy";
	break;

      default:
	weight = "";
	break;
      }

      if (lf.lfItalic)
	style="Italic";
      else
	style="";

	hwnd = GetDesktopWindow();
	hDC = GetDC(hwnd);
	if (hDC) {
		pt_size = -MulDiv(lf.lfHeight, 72,
	                      GetDeviceCaps(hDC,LOGPIXELSY));
		ReleaseDC(hwnd, hDC);
	} else
		pt_size = 10;

    g_snprintf(buf, bufsiz, "%s %s %s %d", lf.lfFaceName, style, weight, pt_size);

    return buf;
   }

  return NULL;
}

/* missing from ms's header files */
#ifndef SPI_GETMENUSHOWDELAY
#define SPI_GETMENUSHOWDELAY 106
#endif

/* I don't know the proper XP theme class for things like
   HIGHLIGHTTEXT, so we'll just define it to be "BUTTON"
   for now */
#define XP_THEME_CLASS_TEXT XP_THEME_CLASS_BUTTON

static void
setup_menu_settings (void)
{
  int menu_delay;
  gboolean win95 = FALSE;

  GtkSettings * settings;
  OSVERSIONINFOEX osvi;

  settings = gtk_settings_get_default ();
  if (!settings)
    return;

  ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

  if (!GetVersionEx ( (OSVERSIONINFO *) &osvi))
    win95 = TRUE; /* assume the worst */

  if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
      win95 = TRUE;

  if (!win95) {
    if (SystemParametersInfo (SPI_GETMENUSHOWDELAY, 0, &menu_delay, 0)) {
      GObjectClass * klazz = G_OBJECT_GET_CLASS(G_OBJECT(settings));

      if (klazz) {
	if (g_object_class_find_property (klazz, "gtk-menu-bar-popup-delay")) {
	  g_object_set (G_OBJECT (settings), "gtk-menu-bar-popup-delay",
			0, NULL);
	}
	if (g_object_class_find_property (klazz, "gtk-menu-popup-delay")) {
	  g_object_set (G_OBJECT (settings), "gtk-menu-popup-delay",
			menu_delay, NULL);
	}
	if (g_object_class_find_property (klazz, "gtk-menu-popdown-delay")) {
	  g_object_set (G_OBJECT (settings), "gtk-menu-popdown-delay",
			menu_delay, NULL);
	}
      }
    }
  }
}

void
wimp_style_setup_system_settings (void)
{
  GtkSettings * settings;
  int cursor_blink_time;

  settings = gtk_settings_get_default ();
  if (!settings)
    return;

  cursor_blink_time = GetCaretBlinkTime ();
  g_object_set (G_OBJECT (settings), "gtk-cursor-blink",
		        cursor_blink_time > 0, NULL);

  if (cursor_blink_time > 0)
  {
  	g_object_set (G_OBJECT (settings), "gtk-cursor-blink-time",
					2*cursor_blink_time, NULL);
  }

  g_object_set (G_OBJECT (settings), "gtk-double-click-time",
		GetDoubleClickTime(), NULL);
  g_object_set (G_OBJECT (settings), "gtk-dnd-drag-threshold",
		GetSystemMetrics(SM_CXDRAG), NULL);

  setup_menu_settings ();

  /*
     http://developer.gnome.org/doc/API/2.0/gtk/GtkSettings.html
     http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sysinfo/base/systemparametersinfo.asp
     http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sysinfo/base/getsystemmetrics.asp
  */
}

static void
setup_system_font(GtkStyle *style)
{
  char buf[256], * font; /* It's okay, lfFaceName is smaller than 32 chars */

  if ((font = sys_font_to_pango_font(XP_THEME_CLASS_TEXT, XP_THEME_FONT_MESSAGE, buf, sizeof (buf))) != NULL)
    style->font_desc = pango_font_description_from_string(font);
}

static void
sys_color_to_gtk_color(XpThemeClass klazz, int id, GdkColor *pcolor)
{
  DWORD color;

  if (!xp_theme_get_system_color (klazz, id, &color))
  	color = GetSysColor(id);

  pcolor->pixel = color;
  pcolor->red   = (GetRValue(color) << 8) | GetRValue(color);
  pcolor->green = (GetGValue(color) << 8) | GetGValue(color);
  pcolor->blue  = (GetBValue(color) << 8) | GetBValue(color);
}

static int
get_system_metric(XpThemeClass klazz, int id)
{
  int rval;

  if (!xp_theme_get_system_metric(klazz, id, &rval))
    rval = GetSystemMetrics (id);

  return rval;
}

#if 0
static void
setup_default_style (void)
{
	GdkColor btnface;
	GdkColor highlight;
	GdkColor window;
	GdkColor windowtext;
	GdkColor highlighttext;
	GdkColor graytext;
	GdkColor btntext;
	GdkColor dark;
	GdkColor light;
	GdkColor mid;
	GdkColor text_aa[5];

	char buf[2048];

	sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNFACE,       &btnface);
	sys_color_to_gtk_color(XP_THEME_CLASS_TEXT,    COLOR_HIGHLIGHT,     &highlight);
	sys_color_to_gtk_color(XP_THEME_CLASS_WINDOW,  COLOR_WINDOW,        &window);
	sys_color_to_gtk_color(XP_THEME_CLASS_WINDOW,  COLOR_WINDOWTEXT,    &windowtext);
	sys_color_to_gtk_color(XP_THEME_CLASS_TEXT,    COLOR_HIGHLIGHTTEXT, &highlighttext);
	sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_GRAYTEXT,      &graytext);
	sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNTEXT,       &btntext);
	sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_3DSHADOW,      &dark);
	sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_3DHILIGHT,     &light);

	mid.red = (light.red + dark.red) / 2;
	mid.green = (light.green + dark.green) / 2;
	mid.blue = (light.blue + dark.blue) / 2;

	text_aa[0].red = (windowtext.red + window.red) / 2;
	text_aa[0].green = (windowtext.green + window.green) / 2;
	text_aa[0].blue = (windowtext.blue + window.blue) / 2;
	text_aa[1].red = (highlighttext.red + highlight.red) / 2;
	text_aa[1].green = (highlighttext.green + highlight.green) / 2;
	text_aa[1].blue = (highlighttext.blue + highlight.blue) / 2;

	text_aa[2].red = (graytext.red + btnface.red) / 2;
	text_aa[2].green = (graytext.green + btnface.green) / 2;
	text_aa[2].blue = (graytext.blue + btnface.blue) / 2;

	text_aa[3].red = (btntext.red + btnface.red) / 2;
	text_aa[3].green = (btntext.green + btnface.green) / 2;
	text_aa[3].blue = (btntext.blue + btnface.blue) / 2;

	text_aa[4].red = (windowtext.red + window.red) / 2;
	text_aa[4].green = (windowtext.green + window.green) / 2;
	text_aa[4].blue = (windowtext.blue + window.blue) / 2;

  	g_snprintf(buf, sizeof (buf),
  		  "style \"wimp-base\" = \"wimp-default\"\n"
		  "{\n"
		  "bg[NORMAL] = { %d, %d, %d }\n"
		  "bg[SELECTED] = { %d, %d, %d }\n"
		  "bg[INSENSITIVE] = { %d, %d, %d }\n"
		  "bg[ACTIVE] = { %d, %d, %d }\n"
		  "bg[PRELIGHT] = { %d, %d, %d }\n"
		  "base[NORMAL] = { %d, %d, %d }\n"
		  "base[SELECTED] = { %d, %d, %d }\n"
		  "base[INSENSITIVE] = { %d, %d, %d }\n"
		  "base[ACTIVE] = { %d, %d, %d }\n"
		  "base[PRELIGHT] = { %d, %d, %d }\n"
  		  "text[NORMAL] = { %d, %d, %d }\n"
  		  "text[SELECTED] = { %d, %d, %d }\n"
  		  "text[INSENSITIVE] = { %d, %d, %d }\n"
  		  "text[ACTIVE] = { %d, %d, %d }\n"
  		  "text[PRELIGHT] = { %d, %d, %d }\n"
		  "fg[NORMAL] = { %d, %d, %d }\n"
		  "fg[SELECTED] = { %d, %d, %d }\n"
		  "fg[INSENSITIVE] = { %d, %d, %d }\n"
		  "fg[ACTIVE] = { %d, %d, %d }\n"
		  "fg[PRELIGHT] = { %d, %d, %d }\n"
		  "dark[NORMAL] = { %d, %d, %d }\n"
		  "dark[SELECTED] = { %d, %d, %d }\n"
		  "dark[INSENSITIVE] = { %d, %d, %d }\n"
		  "dark[ACTIVE] = { %d, %d, %d }\n"
		  "dark[PRELIGHT] = { %d, %d, %d }\n"
		  "light[NORMAL] = { %d, %d, %d }\n"
		  "light[SELECTED] = { %d, %d, %d }\n"
		  "light[INSENSITIVE] = { %d, %d, %d }\n"
		  "light[ACTIVE] = { %d, %d, %d }\n"
		  "light[PRELIGHT] = { %d, %d, %d }\n"
		  "text_aa[NORMAL] = { %d, %d, %d }\n"
		  "text_aa[SELECTED] = { %d, %d, %d }\n"
		  "text_aa[INSENSITIVE] = { %d, %d, %d }\n"
		  "text_aa[ACTIVE] = { %d, %d, %d }\n"
		  "text_aa[PRELIGHT] = { %d, %d, %d }\n"
		  "}widget_class \"*\" style \"wimp-base\"\n",

		  /* bg */
		  btnface.red, btnface.green, btnface.blue,
		  highlight.red, highlight.green, highlight.blue,
		  btnface.red, btnface.green, btnface.blue,
		  btnface.red, btnface.green, btnface.blue,
		  btnface.red, btnface.green, btnface.blue,

		  /* base */
		  window.red, window.green, window.blue,
		  highlight.red, highlight.green, highlight.blue,
		  btnface.red, btnface.green, btnface.blue,
		  btnface.red, btnface.green, btnface.blue,
		  window.red, window.green, window.blue,

		  /* text */
		  windowtext.red, windowtext.green, windowtext.blue,
		  highlighttext.red, highlighttext.green, highlighttext.blue,
		  graytext.red, graytext.green, graytext.blue,
		  btntext.red, btntext.green, btntext.blue,
		  windowtext.red, windowtext.green, windowtext.blue,

		  /* fg */
		  btntext.red, btntext.green, btntext.blue,
		  highlighttext.red, highlighttext.green, highlighttext.blue,
		  graytext.red, graytext.green, graytext.blue,
		  btntext.red, btntext.green, btntext.blue,
		  windowtext.red, windowtext.green, windowtext.blue,

		  /* dark */
		  dark.red, dark.green, dark.blue,
		  dark.red, dark.green, dark.blue,
		  dark.red, dark.green, dark.blue,
		  dark.red, dark.green, dark.blue,
		  dark.red, dark.green, dark.blue,

		  /* light */
		  light.red, light.green, light.blue,
		  light.red, light.green, light.blue,
		  light.red, light.green, light.blue,
		  light.red, light.green, light.blue,
		  light.red, light.green, light.blue,

		  /* text_aa */
		  text_aa[0].red, text_aa[0].green, text_aa[0].blue,
		  text_aa[1].red, text_aa[1].green, text_aa[1].blue,
		  text_aa[2].red, text_aa[2].green, text_aa[2].blue,
		  text_aa[3].red, text_aa[3].green, text_aa[3].blue,
		  text_aa[4].red, text_aa[4].green, text_aa[4].blue
		  );
		  gtk_rc_parse_string(buf);
}
#endif

static void
setup_wimp_rc_style(void)
{
  /* TODO: Owen says:
  	 "If your setup_system_styles() function called gtk_rc_parse_string(), then you are just piling a new set of strings on top each time the theme changes .. the old ones won't be removed" */

  char buf[1024], font_buf[256], *font_ptr;

  GdkColor menu_color;
  GdkColor menu_text_color;
  GdkColor tooltip_back;
  GdkColor tooltip_fore;
  GdkColor btn_fore;
  GdkColor btn_face;
  GdkColor progress_back;

  GdkColor fg_prelight;
  GdkColor bg_prelight;
  GdkColor base_prelight;
  GdkColor text_prelight;

  NONCLIENTMETRICS nc;

  /* Prelight */
  sys_color_to_gtk_color(XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHTTEXT, &fg_prelight);
  sys_color_to_gtk_color(XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHT, &bg_prelight);
  sys_color_to_gtk_color(XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHT, &base_prelight);
  sys_color_to_gtk_color(XP_THEME_CLASS_TEXT, COLOR_HIGHLIGHTTEXT, &text_prelight);

  sys_color_to_gtk_color(XP_THEME_CLASS_MENU, COLOR_MENUTEXT, &menu_text_color);
  sys_color_to_gtk_color(XP_THEME_CLASS_MENU, COLOR_MENU, &menu_color);

  /* tooltips */
  sys_color_to_gtk_color(XP_THEME_CLASS_TOOLTIP, COLOR_INFOTEXT, &tooltip_fore);
  sys_color_to_gtk_color(XP_THEME_CLASS_TOOLTIP, COLOR_INFOBK, &tooltip_back);

  /* text on push buttons. TODO: button shadows, backgrounds, and highlights */
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON, COLOR_BTNTEXT, &btn_fore);
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON, COLOR_BTNFACE, &btn_face);

  /* progress bar background color */
  sys_color_to_gtk_color(XP_THEME_CLASS_PROGRESS, COLOR_HIGHLIGHT, &progress_back);

  /* Enable coloring for menus. */
  font_ptr = sys_font_to_pango_font (XP_THEME_CLASS_MENU, XP_THEME_FONT_MENU,font_buf, sizeof (font_buf));
  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-menu\" = \"wimp-default\"\n"
	     "{\n"
	     "fg[PRELIGHT] = { %d, %d, %d }\n"
	     "bg[PRELIGHT] = { %d, %d, %d }\n"
	     "text[PRELIGHT] = { %d, %d, %d }\n"
	     "base[PRELIGHT] = { %d, %d, %d }\n"
	     "fg[NORMAL] = { %d, %d, %d }\n"
	     "bg[NORMAL] = { %d, %d, %d }\n"
	     "%s = \"%s\"\n"
	     "}widget_class \"*MenuItem*\" style \"wimp-menu\"\n",
	     fg_prelight.red,
	     fg_prelight.green,
	     fg_prelight.blue,
	     bg_prelight.red,
	     bg_prelight.green,
	     bg_prelight.blue,
	     text_prelight.red,
	     text_prelight.green,
	     text_prelight.blue,
	     base_prelight.red,
	     base_prelight.green,
	     base_prelight.blue,
	     menu_text_color.red,
	     menu_text_color.green,
	     menu_text_color.blue,
	     menu_color.red,
	     menu_color.green,
	     menu_color.blue,
	     (font_ptr ? "font_name" : "#"),
	     (font_ptr ? font_ptr : " font name should go here"));
  gtk_rc_parse_string(buf);

  /* Enable coloring for menu bars. */
  font_ptr = sys_font_to_pango_font (XP_THEME_CLASS_MENU, XP_THEME_FONT_MENU,font_buf, sizeof (font_buf));
  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-menu-bar\" = \"wimp-menu\"\n"
	     "{\n"
	     "bg[NORMAL] = { %d, %d, %d }\n"
	     "}widget_class \"*MenuBar*\" style \"wimp-menu-bar\"\n",
	     btn_face.red,
	     btn_face.green,
	     btn_face.blue);
  gtk_rc_parse_string(buf);

  /* enable tooltip fonts */
  font_ptr = sys_font_to_pango_font (XP_THEME_CLASS_STATUS, XP_THEME_FONT_STATUS,font_buf, sizeof (font_buf));
  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-tooltips-caption\" = \"wimp-default\"\n"
	     "{fg[NORMAL] = { %d, %d, %d }\n"
	     "%s = \"%s\"\n"
	     "}widget \"gtk-tooltips.GtkLabel\" style \"wimp-tooltips-caption\"\n",
	     tooltip_fore.red,
	     tooltip_fore.green,
	     tooltip_fore.blue,
	     (font_ptr ? "font_name" : "#"),
	     (font_ptr ? font_ptr : " font name should go here"));
  gtk_rc_parse_string(buf);

  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-tooltips\" = \"wimp-default\"\n"
	     "{bg[NORMAL] = { %d, %d, %d }\n"
	     "}widget \"gtk-tooltips*\" style \"wimp-tooltips\"\n",
	     tooltip_back.red,
	     tooltip_back.green,
	     tooltip_back.blue);
  gtk_rc_parse_string(buf);

  /* enable font theming for status bars */
  font_ptr = sys_font_to_pango_font (XP_THEME_CLASS_STATUS, XP_THEME_FONT_STATUS,font_buf, sizeof (font_buf));
  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-status\" = \"wimp-default\"\n"
	     "{%s = \"%s\"\n"
	     "bg[NORMAL] = { %d, %d, %d }\n"
	     "}widget_class \"*Status*\" style \"wimp-status\"\n",
	     (font_ptr ? "font_name" : "#"),
	     (font_ptr ? font_ptr : " font name should go here"),
	     btn_face.red, btn_face.green, btn_face.blue);
  gtk_rc_parse_string(buf);

  /* enable coloring for text on buttons
     TODO: use GetThemeMetric for the border and outside border */
  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-button\" = \"wimp-default\"\n"
	     "{\n"
	     "bg[NORMAL] = { %d, %d, %d }\n"
	     "bg[PRELIGHT] = { %d, %d, %d }\n"
	     "bg[INSENSITIVE] = { %d, %d, %d }\n"
	     "fg[PRELIGHT] = { %d, %d, %d }\n"
	     "GtkButton::default-border = { 1, 1, 1, 1 }\n"
	     "GtkButton::default-outside-border = { 0, 0, 0, 0 }\n"
	     "GtkButton::child-displacement-x = 1\n"
	     "GtkButton::child-displacement-y = 1\n"
	     "}widget_class \"*Button*\" style \"wimp-button\"\n",
	     btn_face.red, btn_face.green, btn_face.blue,
	     btn_face.red, btn_face.green, btn_face.blue,
	     btn_face.red, btn_face.green, btn_face.blue,
	     btn_fore.red, btn_fore.green, btn_fore.blue
	     );
  gtk_rc_parse_string(buf);

  /* enable coloring for progress bars */
  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-progress\" = \"wimp-default\"\n"
	     "{bg[PRELIGHT] = { %d, %d, %d }\n"
	     "bg[NORMAL] = { %d, %d, %d }\n"
	     "}widget_class \"*Progress*\" style \"wimp-progress\"\n",
	     progress_back.red,
	     progress_back.green,
	     progress_back.blue,
	     btn_face.red, btn_face.green, btn_face.blue);
  gtk_rc_parse_string(buf);

  /* scrollbar thumb width and height */
  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-vscrollbar\" = \"wimp-default\"\n"
	     "{GtkRange::slider-width = %d\n"
	     "GtkRange::stepper-size = %d\n"
	     "GtkRange::stepper-spacing = 0\n"
	     "GtkRange::trough_border = 0\n"
	     "}widget_class \"*VScrollbar*\" style \"wimp-vscrollbar\"\n",
	     GetSystemMetrics(SM_CYVTHUMB),
	     get_system_metric(XP_THEME_CLASS_SCROLLBAR, SM_CXVSCROLL));
  gtk_rc_parse_string(buf);

  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-hscrollbar\" = \"wimp-default\"\n"
	     "{GtkRange::slider-width = %d\n"
	     "GtkRange::stepper-size = %d\n"
	     "GtkRange::stepper-spacing = 0\n"
	     "GtkRange::trough_border = 0\n"
	     "}widget_class \"*HScrollbar*\" style \"wimp-hscrollbar\"\n",
	     GetSystemMetrics(SM_CXHTHUMB),
	     get_system_metric(XP_THEME_CLASS_SCROLLBAR, SM_CYHSCROLL));
  gtk_rc_parse_string(buf);

  /* radio/check button sizes */
  g_snprintf(buf, sizeof (buf),
	     "style \"wimp-checkbutton\" = \"wimp-button\"\n"
	     "{GtkCheckButton::indicator-size = 13\n"
	     "}widget_class \"*CheckButton*\" style \"wimp-checkbutton\"\n"
	     "widget_class \"*RadioButton*\" style \"wimp-checkbutton\"\n");
  gtk_rc_parse_string(buf);
}

static void
setup_system_styles(GtkStyle *style)
{
  int i;

  /* Default background */
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNFACE,   &style->bg[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color(XP_THEME_CLASS_TEXT,    COLOR_HIGHLIGHT, &style->bg[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNFACE,   &style->bg[GTK_STATE_INSENSITIVE]);
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNFACE,   &style->bg[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNFACE,   &style->bg[GTK_STATE_PRELIGHT]);

  /* Default base */
  sys_color_to_gtk_color(XP_THEME_CLASS_WINDOW,  COLOR_WINDOW,    &style->base[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color(XP_THEME_CLASS_TEXT,    COLOR_HIGHLIGHT, &style->base[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNFACE,   &style->base[GTK_STATE_INSENSITIVE]);
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNFACE,   &style->base[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color(XP_THEME_CLASS_WINDOW,  COLOR_WINDOW,    &style->base[GTK_STATE_PRELIGHT]);

  /* Default text */
  sys_color_to_gtk_color(XP_THEME_CLASS_WINDOW,  COLOR_WINDOWTEXT,    &style->text[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color(XP_THEME_CLASS_TEXT,    COLOR_HIGHLIGHTTEXT, &style->text[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_GRAYTEXT,      &style->text[GTK_STATE_INSENSITIVE]);
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNTEXT,       &style->text[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color(XP_THEME_CLASS_WINDOW,  COLOR_WINDOWTEXT,    &style->text[GTK_STATE_PRELIGHT]);

  /* Default forgeground */
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNTEXT,       &style->fg[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color(XP_THEME_CLASS_TEXT,    COLOR_HIGHLIGHTTEXT, &style->fg[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(XP_THEME_CLASS_TEXT,    COLOR_GRAYTEXT,      &style->fg[GTK_STATE_INSENSITIVE]);
  sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON,  COLOR_BTNTEXT,       &style->fg[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color(XP_THEME_CLASS_WINDOW,  COLOR_WINDOWTEXT,    &style->fg[GTK_STATE_PRELIGHT]);

  for (i = 0; i < 5; i++)
    {
      sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON, COLOR_3DSHADOW, &style->dark[i]);
      sys_color_to_gtk_color(XP_THEME_CLASS_BUTTON, COLOR_3DHILIGHT, &style->light[i]);

      style->mid[i].red = (style->light[i].red + style->dark[i].red) / 2;
      style->mid[i].green = (style->light[i].green + style->dark[i].green) / 2;
      style->mid[i].blue = (style->light[i].blue + style->dark[i].blue) / 2;

      style->text_aa[i].red = (style->text[i].red + style->base[i].red) / 2;
      style->text_aa[i].green = (style->text[i].green + style->base[i].green) / 2;
      style->text_aa[i].blue = (style->text[i].blue + style->base[i].blue) / 2;
    }
}

static gboolean
sanitize_size (GdkWindow *window,
	       gint      *width,
	       gint      *height)
{
  gboolean set_bg = FALSE;

  if ((*width == -1) && (*height == -1))
    {
      set_bg = GDK_IS_WINDOW (window);
      gdk_window_get_size (window, width, height);
    }
  else if (*width == -1)
    gdk_window_get_size (window, width, NULL);
  else if (*height == -1)
    gdk_window_get_size (window, NULL, height);

  return set_bg;
}

static XpThemeElement
map_gtk_progress_bar_to_xp(GtkProgressBar *progress_bar, gboolean trough)
{
  XpThemeElement ret;
  switch (progress_bar->orientation)
    {
    case GTK_PROGRESS_LEFT_TO_RIGHT:
    case GTK_PROGRESS_RIGHT_TO_LEFT:
      ret = trough
        ? XP_THEME_ELEMENT_PROGRESS_TROUGH_H
        : XP_THEME_ELEMENT_PROGRESS_BAR_H;
      break;
    default:
      ret = trough
        ? XP_THEME_ELEMENT_PROGRESS_TROUGH_V
        : XP_THEME_ELEMENT_PROGRESS_BAR_V;
      break;
    }
  return ret;
}

static void
draw_part (GdkDrawable  *drawable,
	   GdkGC        *gc,
	   GdkRectangle *area,
	   gint          x,
	   gint          y,
	   Part          part)
{
  if (area)
    gdk_gc_set_clip_rectangle (gc, area);

  if (!parts[part].bmap)
      parts[part].bmap = gdk_bitmap_create_from_data (drawable,
						      parts[part].bits,
						      PART_SIZE, PART_SIZE);

  gdk_gc_set_ts_origin (gc, x, y);
  gdk_gc_set_stipple (gc, parts[part].bmap);
  gdk_gc_set_fill (gc, GDK_STIPPLED);

  gdk_draw_rectangle (drawable, gc, TRUE, x, y, PART_SIZE, PART_SIZE);

  gdk_gc_set_fill (gc, GDK_SOLID);

  if (area)
    gdk_gc_set_clip_rectangle (gc, NULL);
}

static void
draw_check(GtkStyle      *style,
	   GdkWindow     *window,
	   GtkStateType   state,
	   GtkShadowType  shadow,
	   GdkRectangle  *area,
	   GtkWidget     *widget,
	   const gchar   *detail,
	   gint           x,
	   gint           y,
	   gint           width,
	   gint           height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;

  if (detail && strcmp (detail, "check") == 0)	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	{
	  draw_part (window, style->black_gc, area, x, y, CHECK_TEXT);
	  draw_part (window, style->dark_gc[state], area, x, y, CHECK_AA);
	}
    }
  else
    {
      if (xp_theme_draw(window, shadow == GTK_SHADOW_IN
                        ? XP_THEME_ELEMENT_PRESSED_CHECKBOX
                        : XP_THEME_ELEMENT_CHECKBOX,
                        style, x, y, width, height, state, area))
        {
        }
      else
        {
          draw_part (window, style->black_gc, area, x, y, CHECK_BLACK);
          draw_part (window, style->dark_gc[state], area, x, y, CHECK_DARK);
          draw_part (window, style->mid_gc[state], area, x, y, CHECK_MID);
          draw_part (window, style->light_gc[state], area, x, y, CHECK_LIGHT);
          draw_part (window, style->base_gc[state], area, x, y, CHECK_BASE);

          if (shadow == GTK_SHADOW_IN)
            {
              draw_part (window, style->text_gc[state], area, x, y, CHECK_TEXT);
              draw_part (window, style->text_aa_gc[state], area, x, y, CHECK_AA);
            }
        }
    }
}

static void
draw_expander(GtkStyle      *style,
	      GdkWindow     *window,
	      GtkStateType   state,
	      GdkRectangle  *area,
	      GtkWidget     *widget,
	      const gchar   *detail,
	      gint           x,
	      gint           y,
	      GtkExpanderStyle expander_style)
{
  gint expander_size;
  gint expander_semi_size;
  GdkColor color;
  GdkGCValues values;
  gboolean success;
  XpThemeElement xp_expander;

  gtk_widget_style_get (widget, "expander_size", &expander_size, NULL);

  switch (expander_style)
    {
    case GTK_EXPANDER_COLLAPSED:
    case GTK_EXPANDER_SEMI_COLLAPSED:
      xp_expander = XP_THEME_ELEMENT_TREEVIEW_EXPANDER_CLOSED;
      break;
    default:
      xp_expander = XP_THEME_ELEMENT_TREEVIEW_EXPANDER_OPENED;
      break;
    }

  if ((expander_size % 2) == 0)
    expander_size--;

  if (expander_size > 2)
    expander_size -= 2;

  if (area)
    gdk_gc_set_clip_rectangle (style->fg_gc[state], area);

  expander_semi_size = expander_size / 2;
  x -= expander_semi_size;
  y -= expander_semi_size;

  gdk_gc_get_values (style->fg_gc[state], &values);

  if (! xp_theme_draw(window, xp_expander, style,
                      x, y,
                      expander_size, expander_size, state, area))
    {
      /* RGB values to emulate Windows Classic style */
      color.red = color.green = color.blue = 128 << 8;

      success = gdk_colormap_alloc_color
        (gtk_widget_get_default_colormap (), &color, FALSE, TRUE);

      if (success)
        gdk_gc_set_foreground (style->fg_gc[state], &color);

      gdk_draw_rectangle
        (window, style->fg_gc[state], FALSE, x, y,
         expander_size - 1, expander_size - 1);

      gdk_draw_line
        (window, style->fg_gc[state], x + 2, y + expander_semi_size,
         x + expander_size - 2, y + expander_semi_size);

      switch (expander_style)
        {
        case GTK_EXPANDER_COLLAPSED:
        case GTK_EXPANDER_SEMI_COLLAPSED:
          gdk_draw_line
            (window, style->fg_gc[state], x + expander_semi_size, y + 2,
             x + expander_semi_size, y + expander_size - 2);
          break;
        }

      if (success)
        gdk_gc_set_foreground (style->fg_gc[state], &values.foreground);
    }

  if (area)
    gdk_gc_set_clip_rectangle (style->fg_gc[state], NULL);
}

static void
draw_option(GtkStyle      *style,
	    GdkWindow     *window,
	    GtkStateType   state,
	    GtkShadowType  shadow,
	    GdkRectangle  *area,
	    GtkWidget     *widget,
	    const gchar   *detail,
	    gint           x,
	    gint           y,
	    gint           width,
	    gint           height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;

  if (detail && strcmp (detail, "option") == 0)	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	draw_part (window, style->fg_gc[state], area, x, y, RADIO_TEXT);
    }
  else
    {
      if (xp_theme_draw (window, shadow == GTK_SHADOW_IN
                        ? XP_THEME_ELEMENT_PRESSED_RADIO_BUTTON
                        : XP_THEME_ELEMENT_RADIO_BUTTON,
                        style, x, y, width, height, state, area))
        {
        }
      else
	{
	  draw_part (window, style->black_gc, area, x, y, RADIO_BLACK);
	  draw_part (window, style->dark_gc[state], area, x, y, RADIO_DARK);
	  draw_part (window, style->mid_gc[state], area, x, y, RADIO_MID);
	  draw_part (window, style->light_gc[state], area, x, y, RADIO_LIGHT);
	  draw_part (window, style->base_gc[state], area, x, y, RADIO_BASE);

	  if (shadow == GTK_SHADOW_IN)
	    draw_part (window, style->text_gc[state], area, x, y, RADIO_TEXT);
	}
    }
}

static void
draw_varrow (GdkWindow     *window,
	     GdkGC         *gc,
	     GtkShadowType  shadow_type,
	     GdkRectangle  *area,
	     GtkArrowType   arrow_type,
	     gint           x,
	     gint           y,
	     gint           width,
	     gint           height)
{
  gint steps, extra;
  gint y_start, y_increment;
  gint i;

  if (area)
    gdk_gc_set_clip_rectangle (gc, area);

  width = width + width % 2 - 1;	/* Force odd */

  steps = 1 + width / 2;

  extra = height - steps;

  if (arrow_type == GTK_ARROW_DOWN)
    {
      y_start = y;
      y_increment = 1;
    }
  else
    {
      y_start = y + height - 1;
      y_increment = -1;
    }

#if 0
  for (i = 0; i < extra; i++)
    {
      gdk_draw_line (window, gc,
		     x,              y_start + i * y_increment,
		     x + width - 1,  y_start + i * y_increment);
    }
#endif
  for (i = extra; i < height; i++)
    {
      gdk_draw_line (window, gc,
		     x + (i - extra),              y_start + i * y_increment,
		     x + width - (i - extra) - 1,  y_start + i * y_increment);
    }


  if (area)
    gdk_gc_set_clip_rectangle (gc, NULL);
}

static void
draw_harrow (GdkWindow     *window,
	     GdkGC         *gc,
	     GtkShadowType  shadow_type,
	     GdkRectangle  *area,
	     GtkArrowType   arrow_type,
	     gint           x,
	     gint           y,
	     gint           width,
	     gint           height)
{
  gint steps, extra;
  gint x_start, x_increment;
  gint i;

  if (area)
    gdk_gc_set_clip_rectangle (gc, area);

  height = height + height % 2 - 1;	/* Force odd */

  steps = 1 + height / 2;

  extra = width - steps;

  if (arrow_type == GTK_ARROW_RIGHT)
    {
      x_start = x;
      x_increment = 1;
    }
  else
    {
      x_start = x + width - 1;
      x_increment = -1;
    }

#if 0
  for (i = 0; i < extra; i++)
    {
      gdk_draw_line (window, gc,
		     x_start + i * x_increment, y,
		     x_start + i * x_increment, y + height - 1);
    }
#endif
  for (i = extra; i < width; i++)
    {
      gdk_draw_line (window, gc,
		     x_start + i * x_increment, y + (i - extra),
		     x_start + i * x_increment, y + height - (i - extra) - 1);
    }


  if (area)
    gdk_gc_set_clip_rectangle (gc, NULL);
}

/* This function makes up for some brokeness in gtkrange.c
 * where we never get the full arrow of the stepper button
 * and the type of button in a single drawing function.
 *
 * It doesn't work correctly when the scrollbar is squished
 * to the point we don't have room for full-sized steppers.
 */
static void
reverse_engineer_stepper_box (GtkWidget    *range,
			      GtkArrowType  arrow_type,
			      gint         *x,
			      gint         *y,
			      gint         *width,
			      gint         *height)
{
  gint slider_width = 14, stepper_size = 14;
  gint box_width;
  gint box_height;

  if (range)
    {
      gtk_widget_style_get (range,
			    "slider_width", &slider_width,
			    "stepper_size", &stepper_size,
			    NULL);
    }

  if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
    {
      box_width = slider_width;
      box_height = stepper_size;
    }
  else
    {
      box_width = stepper_size;
      box_height = slider_width;
    }

  *x = *x - (box_width - *width) / 2;
  *y = *y - (box_height - *height) / 2;
  *width = box_width;
  *height = box_height;
}

static void
draw_arrow (GtkStyle      *style,
	    GdkWindow     *window,
	    GtkStateType   state,
	    GtkShadowType  shadow,
	    GdkRectangle  *area,
	    GtkWidget     *widget,
	    const gchar   *detail,
	    GtkArrowType   arrow_type,
	    gboolean       fill,
	    gint           x,
	    gint           y,
	    gint           width,
	    gint           height)
{
  const gchar * name;

  name = gtk_widget_get_name (widget);

  sanitize_size (window, &width, &height);

  if (detail && strcmp (detail, "spinbutton") == 0)
    {
      if (xp_theme_is_drawable(XP_THEME_ELEMENT_SPIN_BUTTON_UP))
        {
          return;
        }
      else
        {
          x += (width - 7) / 2;

          if (arrow_type == GTK_ARROW_UP)
            y += (height - 4) / 2;
          else
            y += (1 + height - 4) / 2;
          draw_varrow (window, style->fg_gc[state], shadow, area, arrow_type,
                       x, y, 7, 4);
        }
    }
  else if (detail && (!strcmp (detail, "vscrollbar")
                      || !strcmp (detail, "hscrollbar")))
    {
      gint box_x = x;
      gint box_y = y;
      gint box_width = width;
      gint box_height = height;
      XpThemeElement xp_arrow;
      reverse_engineer_stepper_box (widget, arrow_type,
				    &box_x, &box_y, &box_width, &box_height);

      switch (arrow_type)
        {
        case GTK_ARROW_UP:
          xp_arrow = XP_THEME_ELEMENT_ARROW_UP;
          break;
        case GTK_ARROW_DOWN:
          xp_arrow = XP_THEME_ELEMENT_ARROW_DOWN;
          break;
        case GTK_ARROW_LEFT:
          xp_arrow = XP_THEME_ELEMENT_ARROW_LEFT;
          break;
        default:
          xp_arrow = XP_THEME_ELEMENT_ARROW_RIGHT;
          break;
        }
      if (xp_theme_draw(window, xp_arrow, style, box_x, box_y, box_width, box_height, state, area))
        {
        }
      else if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
        {
          x += (width - 7) / 2;
          y += (height - 5) / 2;

          draw_varrow (window, style->fg_gc[state], shadow, area, arrow_type,
                       x, y, 7, 5);
        }
      else
        {
          y += (height - 7) / 2;
          x += (width - 5) / 2;

          draw_harrow (window, style->fg_gc[state], shadow, area, arrow_type,
                       x, y, 5, 7);
        }
    }
  else
    {
      /* draw the toolbar chevrons - waiting for GTK 2.4 */
	  if (name && !strcmp (name, "gtk-toolbar-arrow"))
	  {
		  if (xp_theme_draw(window, XP_THEME_ELEMENT_REBAR_CHEVRON, style, x, y, width, height, state, area))
				return;
	  }

      if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
	{
	  x += (width - 7) / 2;
	  y += (height - 5) / 2;

	  draw_varrow (window, style->fg_gc[state], shadow, area, arrow_type,
		       x, y, 7, 5);
	}
      else
	{
	  x += (width - 5) / 2;
	  y += (height - 7) / 2;

	  draw_harrow (window, style->fg_gc[state], shadow, area, arrow_type,
		       x, y, 5, 7);
	}
    }
}

static void
option_menu_get_props (GtkWidget      *widget,
		       GtkRequisition *indicator_size,
		       GtkBorder      *indicator_spacing)
{
  GtkRequisition *tmp_size = NULL;
  GtkBorder *tmp_spacing = NULL;

  if (widget)
    gtk_widget_style_get (widget,
			  "indicator_size", &tmp_size,
			  "indicator_spacing", &tmp_spacing,
			  NULL);

  if (tmp_size)
    {
      *indicator_size = *tmp_size;
      g_free (tmp_size);
    }
  else
    *indicator_size = default_option_indicator_size;

  if (tmp_spacing)
    {
      *indicator_spacing = *tmp_spacing;
      g_free (tmp_spacing);
    }
  else
    *indicator_spacing = default_option_indicator_spacing;
}

static void
draw_box (GtkStyle      *style,
	  GdkWindow     *window,
	  GtkStateType   state_type,
	  GtkShadowType  shadow_type,
	  GdkRectangle  *area,
	  GtkWidget     *widget,
	  const gchar   *detail,
	  gint           x,
	  gint           y,
	  gint           width,
	  gint           height)
{
  if (detail &&
      (!strcmp (detail, "button") ||
       !strcmp (detail, "buttondefault")))
    {
      if (GTK_IS_TREE_VIEW (widget->parent) || GTK_IS_CLIST (widget->parent))
        {
          if (xp_theme_draw(window, XP_THEME_ELEMENT_LIST_HEADER, style, x, y,
                            width, height, state_type, area))
            return;
        }
      else if (GTK_IS_TOOLBAR (widget->parent))
      {
		    if (xp_theme_draw(window, XP_THEME_ELEMENT_TOOLBAR_BUTTON, style, x, y,
		                              width, height, state_type, area))
            	return;
	  }
      else
        {
          gboolean is_default = !strcmp (detail, "buttondefault");
          if (xp_theme_draw(window, is_default ? XP_THEME_ELEMENT_DEFAULT_BUTTON
                            : XP_THEME_ELEMENT_BUTTON, style, x, y,
                            width, height, state_type, area))
            return;
        }
    }
  else if (detail && !strcmp (detail, "spinbutton"))
    {
      if (xp_theme_is_drawable(XP_THEME_ELEMENT_SPIN_BUTTON_UP))
        {
          return;
        }
    }
  else if (detail && (!strcmp (detail, "spinbutton_up")
                      || !strcmp (detail, "spinbutton_down")))
    {
      if (xp_theme_draw(window,
                        (! strcmp (detail, "spinbutton_up"))
                        ? XP_THEME_ELEMENT_SPIN_BUTTON_UP
                        : XP_THEME_ELEMENT_SPIN_BUTTON_DOWN,
                        style, x, y, width, height, state_type, area))
        {
          return;
        }
    }

  else if (detail && !strcmp (detail, "slider"))
    {
      if (GTK_IS_SCROLLBAR(widget))
        {
          GtkScrollbar * scrollbar = GTK_SCROLLBAR(widget);
          gboolean is_v = GTK_IS_VSCROLLBAR(widget);
          if (xp_theme_draw(window,
                            is_v
                            ? XP_THEME_ELEMENT_SCROLLBAR_V
                            : XP_THEME_ELEMENT_SCROLLBAR_H,
                            style, x, y, width, height, state_type, area))
            {
	      XpThemeElement gripper = (is_v ? XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_V : XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_H);

	      /* Do not display grippers on tiny scroll bars, the limit imposed
		 is rather arbitrary, perhaps we can fetch the gripper geometry
		 from somewhere and use that... */
	      if ((gripper == XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_H && width < 16)
		  || (gripper == XP_THEME_ELEMENT_SCROLLBAR_GRIPPER_V && height < 16))
		{
		  return;
		}

              xp_theme_draw(window, gripper, style, x, y, width, height, state_type, area);
              return;
            }
        }
    }
  else if (detail && !strcmp (detail, "bar"))
    {
      if (widget && GTK_IS_PROGRESS_BAR (widget))
        {
          GtkProgressBar *progress_bar = GTK_PROGRESS_BAR(widget);
          XpThemeElement xp_progress_bar = map_gtk_progress_bar_to_xp (progress_bar, FALSE);
          if (xp_theme_draw (window, xp_progress_bar,
                             style, x, y, width, height, state_type, area))
            {
              return;
            }
        }
    }
  else if (detail && !strcmp (detail, "handlebox_bin")) {
	if (xp_theme_draw (window, XP_THEME_ELEMENT_REBAR, style, x, y, width, height, state_type, area))
	  {
		return;
  	  }
  }
  else if (detail && strcmp (detail, "menuitem") == 0) {
    shadow_type = GTK_SHADOW_NONE;
      if (xp_theme_draw (window, XP_THEME_ELEMENT_MENU_ITEM, style, x, y, width, height, state_type, area))
        {
  		return;
        }
  }
  else if (detail && !strcmp (detail, "trough"))
    {
      if (widget && GTK_IS_PROGRESS_BAR (widget))
        {
          GtkProgressBar *progress_bar = GTK_PROGRESS_BAR(widget);
          XpThemeElement xp_progress_bar = map_gtk_progress_bar_to_xp (progress_bar, TRUE);
          if (xp_theme_draw (window, xp_progress_bar,
                             style, x, y, width, height, state_type, area))
            {
              return;
            }
          else
            {
              /* Blank in classic Windows */
            }
        }
      else if (widget && GTK_IS_SCROLLBAR(widget))
        {
          gboolean is_vertical = GTK_IS_VSCROLLBAR(widget);

          if (GTK_IS_RANGE(widget)
              && xp_theme_draw(window,
                               is_vertical
                               ? XP_THEME_ELEMENT_TROUGH_V
                               : XP_THEME_ELEMENT_TROUGH_H,
                               style,
                               x, y, width, height, state_type, area))
            {
              return;
            }
          else
            {
              GdkGCValues gc_values;
              GdkGC *gc;
              GdkPixmap *pixmap;

              sanitize_size (window, &width, &height);

              pixmap = gdk_pixmap_new (window, 2, 2, -1);

              gdk_draw_point (pixmap, style->bg_gc[GTK_STATE_NORMAL], 0, 0);
              gdk_draw_point (pixmap, style->bg_gc[GTK_STATE_NORMAL], 1, 1);
              gdk_draw_point (pixmap, style->light_gc[GTK_STATE_NORMAL], 1, 0);
              gdk_draw_point (pixmap, style->light_gc[GTK_STATE_NORMAL], 0, 1);

              gc_values.fill = GDK_TILED;
              gc_values.tile = pixmap;
              gc_values.ts_x_origin = x;
              gc_values.ts_y_origin = y;
              gc = gdk_gc_new_with_values (window, &gc_values,
                                           GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN | GDK_GC_FILL | GDK_GC_TILE);

              if (area)
                gdk_gc_set_clip_rectangle (gc, area);

              gdk_draw_rectangle (window, gc, TRUE, x, y, width, height);

              gdk_gc_unref (gc);
              gdk_pixmap_unref (pixmap);

              return;
            }
        }
      else if (widget && GTK_IS_SCALE(widget))
	{
	  gboolean is_vertical = GTK_IS_VSCALE(widget);

	  parent_class->draw_box (style, window, state_type, GTK_SHADOW_NONE, area,
				  widget, detail, x, y, width, height);

	  if(is_vertical)
	    parent_class->draw_box(style, window, state_type, GTK_SHADOW_ETCHED_IN, area, NULL, NULL, (2 * x + width)/2, y, 1, height);
	  else
	    parent_class->draw_box(style, window, state_type, GTK_SHADOW_ETCHED_IN, area, NULL, NULL, x, (2 * y + height)/2, width, 1);

	  return;
	}
    }
  else if (detail && strcmp (detail, "optionmenu") == 0)
    {
      if (xp_theme_draw(window, XP_THEME_ELEMENT_EDIT_TEXT,
                        style, x, y, width, height, state_type, area))
        {
          return;
        }
    }
  else if (detail && (strcmp (detail, "vscrollbar") == 0 || strcmp (detail, "hscrollbar") == 0))
  {
	  if (shadow_type == GTK_SHADOW_IN)
	  	shadow_type = GTK_SHADOW_ETCHED_IN;
  }
  else
  {
	 const gchar * name = gtk_widget_get_name (widget);

	  if (name && !strcmp (name, "gtk-tooltips")) {
     	 if (xp_theme_draw (window, XP_THEME_ELEMENT_TOOLTIP, style, x, y, width, height, state_type, area))
     	   {
  			return;
    	    }
   		}
  }

  parent_class->draw_box (style, window, state_type, shadow_type, area,
			  widget, detail, x, y, width, height);

  if (detail && strcmp (detail, "optionmenu") == 0)
    {
      GtkRequisition indicator_size;
      GtkBorder indicator_spacing;
      gint vline_x;

      option_menu_get_props (widget, &indicator_size, &indicator_spacing);

      sanitize_size (window, &width, &height);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	vline_x = x + indicator_size.width + indicator_spacing.left + indicator_spacing.right;
      else
	vline_x = x + width - (indicator_size.width + indicator_spacing.left + indicator_spacing.right) - style->xthickness;

      parent_class->draw_vline (style, window, state_type, area, widget,
				detail,
				y + style->ythickness + 1,
				y + height - style->ythickness - 3,
				vline_x);
    }
}

static void
draw_tab (GtkStyle      *style,
	  GdkWindow     *window,
	  GtkStateType   state,
	  GtkShadowType  shadow,
	  GdkRectangle  *area,
	  GtkWidget     *widget,
	  const gchar   *detail,
	  gint           x,
	  gint           y,
	  gint           width,
	  gint           height)
{
  GtkRequisition indicator_size;
  GtkBorder indicator_spacing;

  gint arrow_height;

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  if (detail && ! strcmp (detail, "optionmenutab"))
    {
      if (xp_theme_draw(window, XP_THEME_ELEMENT_COMBOBUTTON,
                        style, x-5, widget->allocation.y+1,
                        width+10, widget->allocation.height-2, state, area))
        {
          return;
        }
    }

  if (widget)
    gtk_widget_style_get (widget, "indicator_size", &indicator_size, NULL);

  option_menu_get_props (widget, &indicator_size, &indicator_spacing);

  x += (width - indicator_size.width) / 2;
  arrow_height = (indicator_size.width + 1) / 2;

  y += (height - arrow_height) / 2;

  draw_varrow (window, style->black_gc, shadow, area, GTK_ARROW_DOWN,
	       x, y, indicator_size.width, arrow_height);
}

/* this is an undefined magic value that, according to the mozilla folks,
   worked for all the various themes that they tried */
#define XP_EDGE_SIZE 2

static void
draw_extension(GtkStyle *style,
               GdkWindow *window,
               GtkStateType state_type,
               GtkShadowType shadow_type,
               GdkRectangle *area,
               GtkWidget *widget,
               const gchar *detail,
               gint x,
               gint y,
               gint width,
               gint height,
               GtkPositionType gap_side)
{
  if (detail && !strcmp(detail, "tab"))
    {
      GtkNotebook *notebook = GTK_NOTEBOOK(widget);
      GtkPositionType pos_type = gtk_notebook_get_tab_pos(notebook);

      if (pos_type == GTK_POS_TOP && state_type == GTK_STATE_NORMAL)
          height += XP_EDGE_SIZE;

#if 0
	/* FIXME: pos != TOP to be implemented */
      else if (pos_type == GTK_POS_BOTTOM)
	y -= XP_EDGE_SIZE;
      else if (pos_type == GTK_POS_RIGHT)
	width += XP_EDGE_SIZE;
      else if (pos_type == GTK_POS_LEFT)
	height -= XP_EDGE_SIZE;
#endif

      if (pos_type == GTK_POS_TOP
          && xp_theme_draw
          (window, gtk_notebook_get_current_page(notebook)==0
           ? XP_THEME_ELEMENT_TAB_ITEM_LEFT_EDGE
           : XP_THEME_ELEMENT_TAB_ITEM,
           style, x, y, width, height, state_type, area))
        {
          return;
        }
    }
  parent_class->draw_extension
    (style, window, state_type, shadow_type, area, widget, detail,
     	 x, y, width, height, gap_side);
}

static void
draw_box_gap (GtkStyle *style, GdkWindow *window, GtkStateType state_type,
              GtkShadowType shadow_type, GdkRectangle *area,
              GtkWidget *widget, const gchar *detail, gint x,
              gint y, gint width, gint height, GtkPositionType gap_side,
              gint gap_x, gint gap_width)
{
  if (detail && !strcmp(detail, "notebook"))
    {
      GtkNotebook *notebook = GTK_NOTEBOOK(widget);

      /* FIXME: pos != TOP to be implemented */
      if (gtk_notebook_get_tab_pos(notebook) == GTK_POS_TOP && xp_theme_draw(window, XP_THEME_ELEMENT_TAB_PANE, style,  x, y, width, height,
			state_type, area))
        {
          return;
        }
    }
  parent_class->draw_box_gap(style, window, state_type, shadow_type,
                             area, widget, detail, x, y, width, height,
                             gap_side, gap_x, gap_width);
}

static void
draw_flat_box (GtkStyle *style, GdkWindow *window,
               GtkStateType state_type, GtkShadowType shadow_type,
               GdkRectangle *area, GtkWidget *widget,
               const gchar *detail, gint x, gint y,
               gint width, gint height)
{
  if (detail && ! strcmp (detail, "checkbutton"))
    {
      if (state_type == GTK_STATE_PRELIGHT)
        {
          return;
        }
    }

  parent_class->draw_flat_box(style, window, state_type, shadow_type,
                              area, widget, detail, x, y, width, height);
}

static void
draw_shadow (GtkStyle      *style,
             GdkWindow     *window,
             GtkStateType   state_type,
             GtkShadowType  shadow_type,
             GdkRectangle  *area,
             GtkWidget     *widget,
             const gchar   *detail,
             gint           x,
             gint           y,
             gint           width,
             gint           height)
{
  if(detail && ! strcmp(detail, "entry"))
    {
      if (xp_theme_draw(window, XP_THEME_ELEMENT_EDIT_TEXT, style,
                        x, y, width, height, state_type, area))
        {
          return;
        }
    }
  parent_class->draw_shadow (style, window, state_type, shadow_type, area, widget,
                             detail, x, y, width, height);
}

static void
draw_hline (GtkStyle		*style,
	    GdkWindow		*window,
	    GtkStateType	 state_type,
	    GdkRectangle	*area,
	    GtkWidget		*widget,
	    const gchar		*detail,
	    gint		 x1,
	    gint		 x2,
	    gint		 y)
{

  if (detail && !strcmp(detail, "menuitem")) {
	  if (xp_theme_draw(window, XP_THEME_ELEMENT_MENU_SEPARATOR, style,
	  		x1, y, x2, style->ythickness, state_type, area)) {
			return;
	  }
  }
#if UXTHEME_HAS_LINES
  if (xp_theme_draw(window, XP_THEME_ELEMENT_HLINE, style, x1, y, x2,
		    style->ythickness, state_type, area))
    {
    }
  else
#endif
    {
      parent_class->draw_hline (style, window, state_type, area, widget,
				detail, x1, x2, y);
    }
}

static void
draw_vline (GtkStyle		*style,
	    GdkWindow		*window,
	    GtkStateType	 state_type,
	    GdkRectangle	*area,
	    GtkWidget		*widget,
	    const gchar		*detail,
	    gint		 y1,
	    gint		 y2,
	    gint		 x)
{
#if UXTHEME_HAS_LINES
  if (xp_theme_draw(window, XP_THEME_ELEMENT_VLINE, style, x, y1,
		    style->xthickness, y2, state_type, area))
    {
    }
  else
#endif
    {
      parent_class->draw_vline (style, window, state_type, area, widget,
				detail, y1, y2, x);
    }
}

static void
draw_resize_grip (GtkStyle      *style,
                       GdkWindow     *window,
                       GtkStateType   state_type,
                       GdkRectangle  *area,
                       GtkWidget     *widget,
                       const gchar   *detail,
                       GdkWindowEdge  edge,
                       gint           x,
                       gint           y,
                       gint           width,
                       gint           height)
{
	if (detail && !strcmp(detail, "statusbar")) {
		if (!xp_theme_draw(window, XP_THEME_ELEMENT_STATUS_GRIPPER, style, x, y, width, height,
                           state_type, area))
			return;
	}

	parent_class->draw_resize_grip (style, window, state_type, area,
									widget, detail, edge, x, y, width, height);
}

static void
draw_handle (GtkStyle        *style,
	     GdkWindow       *window,
	     GtkStateType     state_type,
	     GtkShadowType    shadow_type,
	     GdkRectangle    *area,
	     GtkWidget       *widget,
	     const gchar     *detail,
	     gint             x,
	     gint             y,
	     gint             width,
	     gint             height,
	     GtkOrientation   orientation)
{
  if (! GTK_IS_HANDLE_BOX (widget))
    {
      XpThemeElement hndl;

      sanitize_size (window, &width, &height);

      if (orientation == GTK_ORIENTATION_VERTICAL)
        hndl = XP_THEME_ELEMENT_REBAR_GRIPPER_V;
      else
        hndl = XP_THEME_ELEMENT_REBAR_GRIPPER_H;

      if (xp_theme_draw (window, hndl, style, x, y, width, height,
                         state_type, area))
        {
          return;
        }
      /* grippers are just flat boxes when they're not a toolbar */
      parent_class->draw_box (style, window, state_type, shadow_type,
                              area, widget, detail, x, y, width, height);
    }
  else
    {
      /* TODO: Draw handle boxes as double lines: || */
      parent_class->draw_handle (style, window, state_type, shadow_type,
                                 area, widget, detail, x, y, width, height,
                                 orientation);
    }
}

static void
wimp_style_init_from_rc (GtkStyle * style, GtkRcStyle * rc_style)
{
  setup_system_font (style);
  setup_menu_settings ();
  setup_system_styles (style);
  parent_class->init_from_rc(style, rc_style);
}

static void
wimp_style_class_init (WimpStyleClass *klass)
{
  GtkStyleClass *style_class = GTK_STYLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  style_class->init_from_rc = wimp_style_init_from_rc;
  style_class->draw_arrow = draw_arrow;
  style_class->draw_box = draw_box;
  style_class->draw_check = draw_check;
  style_class->draw_option = draw_option;
  style_class->draw_tab = draw_tab;
  style_class->draw_flat_box = draw_flat_box;
  style_class->draw_expander = draw_expander;
  style_class->draw_extension = draw_extension;
  style_class->draw_box_gap = draw_box_gap;
  style_class->draw_shadow = draw_shadow;
  style_class->draw_hline = draw_hline;
  style_class->draw_vline = draw_vline;
  style_class->draw_handle = draw_handle;
  style_class->draw_resize_grip = draw_resize_grip;
}

GType wimp_type_style = 0;

void
wimp_style_register_type (GTypeModule *module)
{
  static const GTypeInfo object_info =
  {
    sizeof (WimpStyleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) wimp_style_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (WimpStyle),
    0,              /* n_preallocs */
    (GInstanceInitFunc) NULL,
  };

  wimp_type_style = g_type_module_register_type (module,
						   GTK_TYPE_STYLE,
						   "WimpStyle",
						   &object_info, 0);
}

void
wimp_style_init (void)
{
  xp_theme_init ();
  wimp_style_setup_system_settings ();
  setup_wimp_rc_style ();
}
