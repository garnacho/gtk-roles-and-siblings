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

#include "config.h"

#include <stdio.h>
#include <ctype.h>

#include "gdkfont.h"
#include "gdkprivate.h"

static GHashTable *font_name_hash = NULL;
static GHashTable *fontset_name_hash = NULL;

static void
gdk_font_hash_insert (GdkFontType type, GdkFont *font, const gchar *font_name)
{
  GdkFontPrivate *private = (GdkFontPrivate *)font;
  GHashTable **hashp = (type == GDK_FONT_FONT) ?
    &font_name_hash : &fontset_name_hash;

  if (!*hashp)
    *hashp = g_hash_table_new (g_str_hash, g_str_equal);

  private->names = g_slist_prepend (private->names, g_strdup (font_name));
  g_hash_table_insert (*hashp, private->names->data, font);
}

static void
gdk_font_hash_remove (GdkFontType type, GdkFont *font)
{
  GdkFontPrivate *private = (GdkFontPrivate *)font;
  GSList *tmp_list;
  GHashTable *hash = (type == GDK_FONT_FONT) ?
    font_name_hash : fontset_name_hash;

  tmp_list = private->names;
  while (tmp_list)
    {
      g_hash_table_remove (hash, tmp_list->data);
      g_free (tmp_list->data);
      
      tmp_list = tmp_list->next;
    }

  g_slist_free (private->names);
  private->names = NULL;
}

static GdkFont *
gdk_font_hash_lookup (GdkFontType type, const gchar *font_name)
{
  GdkFont *result;
  GHashTable *hash = (type == GDK_FONT_FONT) ?
    font_name_hash : fontset_name_hash;

  if (!hash)
    return NULL;
  else
    {
      result = g_hash_table_lookup (hash, font_name);
      if (result)
	gdk_font_ref (result);
      
      return result;
    }
}

static const char *
charset_name (DWORD charset)
{
  switch (charset)
    {
    case ANSI_CHARSET: return "ANSI";
    case DEFAULT_CHARSET: return "DEFAULT";
    case SYMBOL_CHARSET: return "SYMBOL";
    case SHIFTJIS_CHARSET: return "SHIFTJIS";
    case HANGEUL_CHARSET: return "HANGEUL";
    case GB2312_CHARSET: return "GB2312";
    case CHINESEBIG5_CHARSET: return "CHINESEBIG5";
    case JOHAB_CHARSET: return "JOHAB";
    case HEBREW_CHARSET: return "HEBREW";
    case ARABIC_CHARSET: return "ARABIC";
    case GREEK_CHARSET: return "GREEK";
    case TURKISH_CHARSET: return "TURKISH";
    case VIETNAMESE_CHARSET: return "VIETNAMESE";
    case THAI_CHARSET: return "THAI";
    case EASTEUROPE_CHARSET: return "EASTEUROPE";
    case RUSSIAN_CHARSET: return "RUSSIAN";
    case MAC_CHARSET: return "MAC";
    case BALTIC_CHARSET: return "BALTIC";
    }
  return "unknown";
}

GdkFont*
gdk_font_load_internal (GdkFontType  type,
			const gchar *font_name)
{
  GdkFont *font;
  GdkFontPrivate *private;
  HFONT hfont;
  LOGFONT logfont;
  HGDIOBJ oldfont;
  TEXTMETRIC textmetric;
  CHARSETINFO csi;
  HANDLE *f;
  DWORD fdwItalic, fdwUnderline, fdwStrikeOut, fdwCharSet,
    fdwOutputPrecision, fdwClipPrecision, fdwQuality, fdwPitchAndFamily;
  const char *lpszFace;

  int numfields, n1, n2, tries;
  char foundry[32], family[100], weight[32], slant[32], set_width[32],
    spacing[32], registry[32], encoding[32];
  char pixel_size[10], point_size[10], res_x[10], res_y[10], avg_width[10];
  int c;
  char *p;
  int nHeight, nWidth, nEscapement, nOrientation, fnWeight;
  int logpixelsy;

  g_return_val_if_fail (font_name != NULL, NULL);

  GDK_NOTE (MISC, g_print ("gdk_font_load_internal: %s\n", font_name));

  font = gdk_font_hash_lookup (type, font_name);
  if (font)
    return font;

  numfields = sscanf (font_name,
		      "-%30[^-]-%100[^-]-%30[^-]-%30[^-]-%30[^-]-%n",
		      foundry,
		      family,
		      weight,
		      slant,
		      set_width,
		      &n1);
  if (numfields == 0)
    {
      /* Probably a plain Windows font name */
      nHeight = 0;
      nWidth = 0;
      nEscapement = 0;
      nOrientation = 0;
      fnWeight = FW_DONTCARE;
      fdwItalic = FALSE;
      fdwUnderline = FALSE;
      fdwStrikeOut = FALSE;
      fdwCharSet = ANSI_CHARSET;
      fdwOutputPrecision = OUT_TT_PRECIS;
      fdwClipPrecision = CLIP_DEFAULT_PRECIS;
      fdwQuality = PROOF_QUALITY;
      fdwPitchAndFamily = DEFAULT_PITCH;
      lpszFace = font_name;
    }
  else if (numfields != 5)
    {
      g_warning ("gdk_font_load: font name %s illegal", font_name);
      g_free (font);
      return NULL;
    }
  else
    {
      /* It must be a XLFD name */

      /* Check for hex escapes in the font family,
       * put in there by gtkfontsel.
       */
      p = family;
      while (*p)
	{
	  if (*p == '%' && isxdigit (p[1]) && isxdigit (p[2]))
	    {
	      sscanf (p+1, "%2x", &c);
	      *p = c;
	      strcpy (p+1, p+3);
	    }
	  p++;
	}

      /* Skip add_style which often is empty in the requested font name */
      while (font_name[n1] && font_name[n1] != '-')
	n1++;
      numfields++;

      numfields += sscanf (font_name + n1,
			   "-%8[^-]-%8[^-]-%8[^-]-%8[^-]-%30[^-]-%8[^-]-%30[^-]-%30[^-]%n",
			   pixel_size,
			   point_size,
			   res_x,
			   res_y,
			   spacing,
			   avg_width,
			   registry,
			   encoding,
			   &n2);

      if (numfields != 14 || font_name[n1 + n2] != '\0')
	{
	  g_warning ("gdk_font_load: font name %s illegal", font_name);
	  g_free (font);
	  return NULL;
	}

      logpixelsy = GetDeviceCaps (gdk_DC, LOGPIXELSY);

      if (strcmp (pixel_size, "*") == 0)
	if (strcmp (point_size, "*") == 0)
	  nHeight = 0;
	else
	  nHeight = (int) (((double) atoi (point_size))/720.*logpixelsy);
      else
	nHeight = atoi (pixel_size);

      nWidth = 0;
      nEscapement = 0;
      nOrientation = 0;

      if (g_strcasecmp (weight, "thin") == 0)
	fnWeight = FW_THIN;
      else if (g_strcasecmp (weight, "extralight") == 0)
	fnWeight = FW_EXTRALIGHT;
      else if (g_strcasecmp (weight, "ultralight") == 0)
#ifdef FW_ULTRALIGHT
	fnWeight = FW_ULTRALIGHT;
#else
	fnWeight = FW_EXTRALIGHT; /* In fact, FW_ULTRALIGHT really is 
				   * defined as FW_EXTRALIGHT anyway.
				   */
#endif
      else if (g_strcasecmp (weight, "light") == 0)
	fnWeight = FW_LIGHT;
      else if (g_strcasecmp (weight, "normal") == 0)
	fnWeight = FW_NORMAL;
      else if (g_strcasecmp (weight, "regular") == 0)
	fnWeight = FW_REGULAR;
      else if (g_strcasecmp (weight, "medium") == 0)
	fnWeight = FW_MEDIUM;
      else if (g_strcasecmp (weight, "semibold") == 0)
	fnWeight = FW_SEMIBOLD;
      else if (g_strcasecmp (weight, "demibold") == 0)
#ifdef FW_DEMIBOLD
	fnWeight = FW_DEMIBOLD;
#else
	fnWeight = FW_SEMIBOLD;	/* As above */
#endif
      else if (g_strcasecmp (weight, "bold") == 0)
	fnWeight = FW_BOLD;
      else if (g_strcasecmp (weight, "extrabold") == 0)
	fnWeight = FW_EXTRABOLD;
      else if (g_strcasecmp (weight, "ultrabold") == 0)
#ifdef FW_ULTRABOLD
	fnWeight = FW_ULTRABOLD;
#else
	fnWeight = FW_EXTRABOLD; /* As above */
#endif
      else if (g_strcasecmp (weight, "heavy") == 0)
	fnWeight = FW_HEAVY;
      else if (g_strcasecmp (weight, "black") == 0)
#ifdef FW_BLACK
	fnWeight = FW_BLACK;
#else
	fnWeight = FW_HEAVY;	/* As above */
#endif
      else
	fnWeight = FW_DONTCARE;

      if (g_strcasecmp (slant, "italic") == 0
	  || g_strcasecmp (slant, "oblique") == 0
	  || g_strcasecmp (slant, "i") == 0
	  || g_strcasecmp (slant, "o") == 0)
	fdwItalic = TRUE;
      else
	fdwItalic = FALSE;
      fdwUnderline = FALSE;
      fdwStrikeOut = FALSE;
      if (g_strcasecmp (registry, "iso8859") == 0)
	if (strcmp (encoding, "1") == 0)
	  fdwCharSet = ANSI_CHARSET;
	else
	  fdwCharSet = ANSI_CHARSET; /* XXX ??? */
      else if (g_strcasecmp (registry, "windows") == 0)
	if (g_strcasecmp (encoding, "symbol") == 0)
	  fdwCharSet = SYMBOL_CHARSET;
	else if (g_strcasecmp (encoding, "shiftjis") == 0)
	  fdwCharSet = SHIFTJIS_CHARSET;
	else if (g_strcasecmp (encoding, "gb2312") == 0)
	  fdwCharSet = GB2312_CHARSET;
	else if (g_strcasecmp (encoding, "hangeul") == 0)
	  fdwCharSet = HANGEUL_CHARSET;
	else if (g_strcasecmp (encoding, "chinesebig5") == 0)
	  fdwCharSet = CHINESEBIG5_CHARSET;
	else if (g_strcasecmp (encoding, "johab") == 0)
	  fdwCharSet = JOHAB_CHARSET;
	else if (g_strcasecmp (encoding, "hebrew") == 0)
	  fdwCharSet = HEBREW_CHARSET;
	else if (g_strcasecmp (encoding, "arabic") == 0)
	  fdwCharSet = ARABIC_CHARSET;
	else if (g_strcasecmp (encoding, "greek") == 0)
	  fdwCharSet = GREEK_CHARSET;
	else if (g_strcasecmp (encoding, "turkish") == 0)
	  fdwCharSet = TURKISH_CHARSET;
	else if (g_strcasecmp (encoding, "easteurope") == 0)
	  fdwCharSet = EASTEUROPE_CHARSET;
	else if (g_strcasecmp (encoding, "russian") == 0)
	  fdwCharSet = RUSSIAN_CHARSET;
	else if (g_strcasecmp (encoding, "mac") == 0)
	  fdwCharSet = MAC_CHARSET;
	else if (g_strcasecmp (encoding, "baltic") == 0)
	  fdwCharSet = BALTIC_CHARSET;
	else
	  fdwCharSet = ANSI_CHARSET; /* XXX ??? */
      else
	fdwCharSet = ANSI_CHARSET; /* XXX ??? */
      fdwOutputPrecision = OUT_TT_PRECIS;
      fdwClipPrecision = CLIP_DEFAULT_PRECIS;
      fdwQuality = PROOF_QUALITY;
      if (g_strcasecmp (spacing, "m") == 0)
	fdwPitchAndFamily = FIXED_PITCH;
      else if (g_strcasecmp (spacing, "p") == 0)
	fdwPitchAndFamily = VARIABLE_PITCH;
      else 
	fdwPitchAndFamily = DEFAULT_PITCH;
      lpszFace = family;
    }

  for (tries = 0; ; tries++)
    {
      GDK_NOTE (MISC, g_print ("...trying CreateFont(%d,%d,%d,%d,"
			       "%d,%d,%d,%d,"
			       "%d,%d,%d,"
			       "%d,%#.02x,\"%s\")\n",
			       nHeight, nWidth, nEscapement, nOrientation,
			       fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
			       fdwCharSet, fdwOutputPrecision, fdwClipPrecision,
			       fdwQuality, fdwPitchAndFamily, lpszFace));
      if ((hfont =
	   CreateFont (nHeight, nWidth, nEscapement, nOrientation,
		       fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
		       fdwCharSet, fdwOutputPrecision, fdwClipPrecision,
		       fdwQuality, fdwPitchAndFamily, lpszFace)) != NULL)
	break;

      /* If we fail, try some similar fonts often found on Windows. */

      if (tries == 0)
	{
	  if (g_strcasecmp (family, "helvetica") == 0)
	    lpszFace = "arial";
	  else if (g_strcasecmp (family, "new century schoolbook") == 0)
	    lpszFace = "century schoolbook";
	  else if (g_strcasecmp (family, "courier") == 0)
	    lpszFace = "courier new";
	  else if (g_strcasecmp (family, "lucida") == 0)
	    lpszFace = "lucida sans unicode";
	  else if (g_strcasecmp (family, "lucidatypewriter") == 0)
	    lpszFace = "lucida console";
	  else if (g_strcasecmp (family, "times") == 0)
	    lpszFace = "times new roman";
	}
      else if (tries == 1)
	{
	  if (g_strcasecmp (family, "courier") == 0)
	    {
	      lpszFace = "";
	      fdwPitchAndFamily |= FF_MODERN;
	    }
	  else if (g_strcasecmp (family, "times new roman") == 0)
	    {
	      lpszFace = "";
	      fdwPitchAndFamily |= FF_ROMAN;
	    }
	  else if (g_strcasecmp (family, "helvetica") == 0
		   || g_strcasecmp (family, "lucida") == 0)
	    {
	      lpszFace = "";
	      fdwPitchAndFamily |= FF_SWISS;
	    }
	  else
	    {
	      lpszFace = "";
	      fdwPitchAndFamily = (fdwPitchAndFamily & 0x0F) | FF_DONTCARE;
	    }
	}
      else
	break;
      tries++;
    }
  
  if (!hfont)
    return NULL;
      
  private = g_new (GdkFontPrivate, 1);
  font = (GdkFont*) private;

  private->xfont = hfont;
  private->ref_count = 1;
  private->names = NULL;
  GetObject (private->xfont, sizeof (logfont), &logfont);
  oldfont = SelectObject (gdk_DC, private->xfont);
  GetTextMetrics (gdk_DC, &textmetric);
  private->charset = GetTextCharsetInfo (gdk_DC, &private->fs, 0);
  SelectObject (gdk_DC, oldfont);
  TranslateCharsetInfo ((DWORD *) private->charset, &csi, TCI_SRCCHARSET);
  private->codepage = csi.ciACP;
  GetCPInfo (private->codepage, &private->cpinfo);
  font->type = type;
  font->ascent = textmetric.tmAscent;
  font->descent = textmetric.tmDescent;

  GDK_NOTE (MISC, g_print ("... = %#x charset %s codepage %d (max %d bytes) "
			   "asc %d desc %d\n",
			   private->xfont,
			   charset_name (private->charset),
			   private->codepage,
			   private->cpinfo.MaxCharSize,
			   font->ascent, font->descent));

  /* This memory is leaked, so shoot me. */
  f = g_new (HANDLE, 1);
  *f = (HANDLE) ((guint) private->xfont + HFONT_DITHER);
  gdk_xid_table_insert (f, font);

  gdk_font_hash_insert (type, font, font_name);

  return font;
}

GdkFont*
gdk_font_load (const gchar *font_name)
{
  /* Load all fonts as fontsets... Gtktext and gtkentry work better
   * that way, they use wide chars, which is necessary for non-ASCII
   * chars to work. (Yes, even Latin-1, as we use Unicode internally.)
   */
  return gdk_font_load_internal (GDK_FONT_FONTSET, font_name);
}

GdkFont*
gdk_fontset_load (gchar *fontset_name)
{
  return gdk_font_load_internal (GDK_FONT_FONTSET, fontset_name);
}

GdkFont*
gdk_font_ref (GdkFont *font)
{
  GdkFontPrivate *private;

  g_return_val_if_fail (font != NULL, NULL);

  private = (GdkFontPrivate*) font;
  private->ref_count += 1;

  GDK_NOTE (MISC, g_print ("gdk_font_ref %#x %d\n",
			   private->xfont, private->ref_count));
  return font;
}

void
gdk_font_unref (GdkFont *font)
{
  GdkFontPrivate *private;
  private = (GdkFontPrivate*) font;

  g_return_if_fail (font != NULL);
  g_return_if_fail (private->ref_count > 0);

  private->ref_count -= 1;

  GDK_NOTE (MISC, g_print ("gdk_font_unref %#x %d%s\n",
			   private->xfont,
			   private->ref_count,
			   (private->ref_count == 0 ? " freeing" : "")));

  if (private->ref_count == 0)
    {
      gdk_font_hash_remove (font->type, font);
      
      switch (font->type)
	{
	case GDK_FONT_FONT:
	case GDK_FONT_FONTSET:	/* XXX */
	  gdk_xid_table_remove ((HANDLE) ((guint) private->xfont + HFONT_DITHER));
	  DeleteObject (private->xfont);
	  break;

	default:
	  g_assert_not_reached ();
	}
      g_free (font);
    }
}

gint
gdk_font_id (const GdkFont *font)
{
  const GdkFontPrivate *font_private;

  g_return_val_if_fail (font != NULL, 0);

  font_private = (const GdkFontPrivate*) font;

  if (font->type == GDK_FONT_FONT)
    return (gint) font_private->xfont;
  else
    return 0;
}

gint
gdk_font_equal (const GdkFont *fonta,
                const GdkFont *fontb)
{
  const GdkFontPrivate *privatea;
  const GdkFontPrivate *privateb;

  g_return_val_if_fail (fonta != NULL, FALSE);
  g_return_val_if_fail (fontb != NULL, FALSE);

  privatea = (const GdkFontPrivate*) fonta;
  privateb = (const GdkFontPrivate*) fontb;

  if (fonta->type == GDK_FONT_FONT && fontb->type == GDK_FONT_FONT)
    return (privatea->xfont == privateb->xfont);
  else if (fonta->type == GDK_FONT_FONTSET && fontb->type == GDK_FONT_FONTSET)
    return (privatea->xfont == privateb->xfont);
  else
    return 0;
}

gint
gdk_string_width (GdkFont     *font,
		  const gchar *string)
{
  return gdk_text_width (font, string, strlen (string));
}

static gboolean
gdk_text_size (GdkFont      *font,
	       const gchar  *text,
	       gint          text_length,
	       SIZE	     *sizep)
{
  GdkFontPrivate *private;
  HGDIOBJ oldfont;
  gint wlen;
  wchar_t *wcstr;

  g_return_val_if_fail (font != NULL, FALSE);
  g_return_val_if_fail (text != NULL, FALSE);

  if (text_length == 0)
    return 0;

  private = (GdkFontPrivate*) font;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  if ((oldfont = SelectObject (gdk_DC, private->xfont)) == NULL)
    {
      g_warning ("gdk_text_width: SelectObject failed");
      return FALSE;
    }

  wcstr = g_new (wchar_t, text_length);
  if ((wlen = gdk_nmbstowchar_ts (wcstr, text, text_length, text_length)) == -1)
    {
      g_warning ("gdk_text_size: gdk_nmbstowchar_ts failed");
      return FALSE;
    }

  GetTextExtentPoint32W (gdk_DC, wcstr, wlen, sizep);

  g_free (wcstr);
  SelectObject (gdk_DC, oldfont);

  return TRUE;
}

gint
gdk_text_width (GdkFont      *font,
		const gchar  *text,
		gint          text_length)
{
  SIZE size;

  if (!gdk_text_size (font, text, text_length, &size))
    return -1;

  return size.cx;
}

gint
gdk_text_width_wc (GdkFont	  *font,
		   const GdkWChar *text,
		   gint		   text_length)
{
  GdkFontPrivate *private;
  HGDIOBJ oldfont;
  SIZE size;
  wchar_t *wcstr;
  guchar *str;
  gint i;

  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (text != NULL, -1);

  if (text_length == 0)
    return 0;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  private = (GdkFontPrivate*) font;

  if ((oldfont = SelectObject (gdk_DC, private->xfont)) == NULL)
    
    g_warning ("gdk_text_width_wc: SelectObject failed");

  if (sizeof (wchar_t) != sizeof (GdkWChar))
    {
      wcstr = g_new (wchar_t, text_length);
      for (i = 0; i < text_length; i++)
	wcstr[i] = text[i];
    }
  else
    wcstr = (wchar_t *) text;

  GetTextExtentPoint32W (gdk_DC, wcstr, text_length, &size);

  if (sizeof (wchar_t) != sizeof (GdkWChar))
    g_free (wcstr);

  if (oldfont != NULL)
    SelectObject (gdk_DC, oldfont);

  return size.cx;
}

gint
gdk_char_width (GdkFont *font,
		gchar    character)
{
  if (((guchar) character) >= 128)
    {
      /* gtktext calls us with non-ASCII characters, sigh */
      GdkWChar wc = (guchar) character;
      return gdk_text_width_wc (font, &wc, 1);
    }
  return gdk_text_width (font, &character, 1);
}

gint
gdk_char_width_wc (GdkFont *font,
		   GdkWChar character)
{
  return gdk_text_width_wc (font, &character, 1);
}

gint
gdk_string_measure (GdkFont     *font,
                    const gchar *string)
{
  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (string != NULL, -1);

  return gdk_text_measure (font, string, strlen (string));
}

void
gdk_text_extents (GdkFont     *font,
                  const gchar *text,
                  gint         text_length,
		  gint        *lbearing,
		  gint        *rbearing,
		  gint        *width,
		  gint        *ascent,
		  gint        *descent)
{
  GdkFontPrivate *private;
  HGDIOBJ oldfont;
  SIZE size;
  gint wlen;
  wchar_t *wcstr;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  if (text_length == 0)
    {
      if (lbearing)
	*lbearing = 0;
      if (rbearing)
	*rbearing = 0;
      if (width)
	*width = 0;
      if (ascent)
	*ascent = 0;
      if (descent)
	*descent = 0;
      return;
    }

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  private = (GdkFontPrivate*) font;

  if ((oldfont = SelectObject (gdk_DC, private->xfont)) == NULL)
    g_warning ("gdk_text_extents: SelectObject failed");

  wcstr = g_new (wchar_t, text_length);
  if ((wlen = gdk_nmbstowchar_ts (wcstr, text, text_length, text_length)) == -1)
    {
      g_warning ("gdk_text_extents: gdk_nmbstowchar_ts failed");
      size.cx = 0;
      size.cy = 0;
    }
  else
    GetTextExtentPoint32W (gdk_DC, wcstr, wlen, &size);

  if (oldfont != NULL)
    SelectObject (gdk_DC, oldfont);

  /* XXX This is all quite bogus */
  if (lbearing)
    *lbearing = 0;
  if (rbearing)
    *rbearing = 0;
  if (width)
    *width = size.cx;
  if (ascent)
    *ascent = size.cy + 1;
  if (descent)
    *descent = font->descent + 1;
}

void
gdk_text_extents_wc (GdkFont        *font,
		     const GdkWChar *text,
		     gint            text_length,
		     gint           *lbearing,
		     gint           *rbearing,
		     gint           *width,
		     gint           *ascent,
		     gint           *descent)
{
  GdkFontPrivate *private;
  HGDIOBJ oldfont;
  SIZE size;
  wchar_t *wcstr;
  gint i;

  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  if (text_length == 0)
    {
      if (lbearing)
	*lbearing = 0;
      if (rbearing)
	*rbearing = 0;
      if (width)
	*width = 0;
      if (ascent)
	*ascent = 0;
      if (descent)
	*descent = 0;
      return;
    }

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  private = (GdkFontPrivate*) font;

  if (sizeof (wchar_t) != sizeof (GdkWChar))
    {
      wcstr = g_new (wchar_t, text_length);
      for (i = 0; i < text_length; i++)
	wcstr[i] = text[i];
    }
  else
    wcstr = (wchar_t *) text;

  if ((oldfont = SelectObject (gdk_DC, private->xfont)) == NULL)
    g_warning ("gdk_text_extents_wc: SelectObject failed");

  GetTextExtentPoint32W (gdk_DC, wcstr, text_length, &size);

  if (sizeof (wchar_t) != sizeof (GdkWChar))
    g_free (wcstr);

  if (oldfont != NULL)
    SelectObject (gdk_DC, oldfont);

  /* XXX This is all quite bogus */
  if (lbearing)
    *lbearing = 0;
  if (rbearing)
    *rbearing = 0;
  if (width)
    *width = size.cx;
  if (ascent)
    *ascent = size.cy + 1;
  if (descent)
    *descent = font->descent + 1;
}

void
gdk_string_extents (GdkFont     *font,
		    const gchar *string,
		    gint        *lbearing,
		    gint        *rbearing,
		    gint        *width,
		    gint        *ascent,
		    gint        *descent)
{
  g_return_if_fail (font != NULL);
  g_return_if_fail (string != NULL);

  gdk_text_extents (font, string, strlen (string),
		    lbearing, rbearing, width, ascent, descent);
}


gint
gdk_text_measure (GdkFont     *font,
                  const gchar *text,
                  gint         text_length)
{
  return gdk_text_width (font, text, text_length); /* ??? */
}

gint
gdk_char_measure (GdkFont *font,
                  gchar    character)
{
  return gdk_text_measure (font, &character, 1);
}

gint
gdk_string_height (GdkFont     *font,
		   const gchar *string)
{
  g_return_val_if_fail (font != NULL, -1);
  g_return_val_if_fail (string != NULL, -1);

  return gdk_text_height (font, string, strlen (string));
}

gint
gdk_text_height (GdkFont     *font,
		 const gchar *text,
		 gint         text_length)
{
  SIZE size;

  if (!gdk_text_size (font, text, text_length, &size))
    return -1;

  return size.cy;
}

gint
gdk_char_height (GdkFont *font,
		 gchar    character)
{
  return gdk_text_height (font, &character, 1);
}
