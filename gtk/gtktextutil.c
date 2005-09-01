/* GTK - The GIMP Toolkit
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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include "gtktextutil.h"
#include "gtkintl.h"
#include "gtkmenuitem.h"
#include "gtkalias.h"

#define DRAG_ICON_MAX_WIDTH 250
#define DRAG_ICON_LAYOUT_BORDER 2
#define DRAG_ICON_MAX_LINES 7
#define ELLIPSIS_CHARACTER "\xe2\x80\xa6"

typedef struct _GtkUnicodeMenuEntry GtkUnicodeMenuEntry;
typedef struct _GtkTextUtilCallbackInfo GtkTextUtilCallbackInfo;

struct _GtkUnicodeMenuEntry {
  const char *label;
  gunichar ch;
};

struct _GtkTextUtilCallbackInfo
{
  GtkTextUtilCharChosenFunc func;
  gpointer data;
};

static const GtkUnicodeMenuEntry bidi_menu_entries[] = {
  { N_("LRM _Left-to-right mark"), 0x200E },
  { N_("RLM _Right-to-left mark"), 0x200F },
  { N_("LRE Left-to-right _embedding"), 0x202A },
  { N_("RLE Right-to-left e_mbedding"), 0x202B },
  { N_("LRO Left-to-right _override"), 0x202D },
  { N_("RLO Right-to-left o_verride"), 0x202E },
  { N_("PDF _Pop directional formatting"), 0x202C },
  { N_("ZWS _Zero width space"), 0x200B },
  { N_("ZWJ Zero width _joiner"), 0x200D },
  { N_("ZWNJ Zero width _non-joiner"), 0x200C }
};

static void
activate_cb (GtkWidget *menu_item,
             gpointer   data)
{
  GtkUnicodeMenuEntry *entry;
  GtkTextUtilCallbackInfo *info = data;
  char buf[7];
  
  entry = g_object_get_data (G_OBJECT (menu_item), "gtk-unicode-menu-entry");

  buf[g_unichar_to_utf8 (entry->ch, buf)] = '\0';
  
  (* info->func) (buf, info->data);
}

/**
 * _gtk_text_util_append_special_char_menuitems
 * @menushell: a #GtkMenuShell
 * @callback:  call this when an item is chosen
 * @data: data for callback
 * 
 * Add menuitems for various bidi control characters  to a menu;
 * the menuitems, when selected, will call the given function
 * with the chosen character.
 *
 * This function is private/internal in GTK 2.0, the functionality may
 * become public sometime, but it probably needs more thought first.
 * e.g. maybe there should be a way to just get the list of items,
 * instead of requiring the menu items to be created.
 **/
void
_gtk_text_util_append_special_char_menuitems (GtkMenuShell              *menushell,
                                              GtkTextUtilCharChosenFunc  func,
                                              gpointer                   data)
{
  int i;
  
  for (i = 0; i < G_N_ELEMENTS (bidi_menu_entries); i++)
    {
      GtkWidget *menuitem;
      GtkTextUtilCallbackInfo *info;

      /* wasteful to have a bunch of copies, but simplifies mem management */
      info = g_new (GtkTextUtilCallbackInfo, 1);
      info->func = func;
      info->data = data;
      
      menuitem = gtk_menu_item_new_with_mnemonic (_(bidi_menu_entries[i].label));
      g_object_set_data (G_OBJECT (menuitem), I_("gtk-unicode-menu-entry"),
                         (gpointer)&bidi_menu_entries[i]);
      
      g_signal_connect_data (menuitem, "activate",
                             G_CALLBACK (activate_cb),
                             info, (GClosureNotify) g_free, 0);
      
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (menushell, menuitem);
    }
}

static void
append_n_lines (GString *str, const gchar *text, GSList *lines, gint n_lines)
{
  PangoLayoutLine *line;
  gint i;

  for (i = 0; i < n_lines; i++)
    {
      line = lines->data;
      g_string_append_len (str, &text[line->start_index], line->length);
      lines = lines->next;
    }
}

static void
limit_layout_lines (PangoLayout *layout)
{
  const gchar *text;
  GString     *str;
  GSList      *lines, *elem;
  gint         n_lines;

  n_lines = pango_layout_get_line_count (layout);
  
  if (n_lines >= DRAG_ICON_MAX_LINES)
    {
      text  = pango_layout_get_text (layout);
      str   = g_string_new (NULL);
      lines = pango_layout_get_lines (layout);

      /* get first lines */
      elem = lines;
      append_n_lines (str, text, elem,
                      DRAG_ICON_MAX_LINES / 2);

      g_string_append (str, "\n" ELLIPSIS_CHARACTER "\n");

      /* get last lines */
      elem = g_slist_nth (lines, n_lines - DRAG_ICON_MAX_LINES / 2);
      append_n_lines (str, text, elem,
                      DRAG_ICON_MAX_LINES / 2);

      pango_layout_set_text (layout, str->str, -1);
      g_string_free (str, TRUE);
    }
}

/**
 * _gtk_text_util_create_drag_icon
 * @widget: #GtkWidget to extract the pango context
 * @text: a #gchar to render the icon
 * @len: length of @text, or -1 for NUL-terminated text
 *
 * Creates a drag and drop icon from @text.
 **/
GdkPixmap*
_gtk_text_util_create_drag_icon (GtkWidget *widget, 
                                 gchar     *text,
                                 gsize      len)
{
  GdkDrawable  *drawable = NULL;
  PangoContext *context;
  PangoLayout  *layout;
  gint          pixmap_height, pixmap_width;
  gint          layout_width, layout_height;
  gint          n_lines;

  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (text != NULL, NULL);

  context = gtk_widget_get_pango_context (widget);
  layout  = pango_layout_new (context);

  pango_layout_set_text (layout, text, len);
  pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
  pango_layout_get_size (layout, &layout_width, &layout_height);

  layout_width = MIN (layout_width, DRAG_ICON_MAX_WIDTH * PANGO_SCALE);
  pango_layout_set_width (layout, layout_width);
  n_lines = pango_layout_get_line_count (layout);

  limit_layout_lines (layout);

  /* get again layout extents, they may have changed */
  pango_layout_get_size (layout, &layout_width, &layout_height);

  pixmap_width  = layout_width  / PANGO_SCALE + DRAG_ICON_LAYOUT_BORDER * 2;
  pixmap_height = layout_height / PANGO_SCALE + DRAG_ICON_LAYOUT_BORDER * 2;

  drawable = gdk_pixmap_new (widget->window,
                             pixmap_width  + 2,
                             pixmap_height + 2,
                             -1);

  gdk_draw_rectangle (drawable,
                      widget->style->base_gc [GTK_WIDGET_STATE (widget)],
                      TRUE,
                      0, 0,
                      pixmap_width + 1,
                      pixmap_height + 1);

  gdk_draw_layout (drawable,
                   widget->style->text_gc [GTK_WIDGET_STATE (widget)],
                   1 + DRAG_ICON_LAYOUT_BORDER,
                   1 + DRAG_ICON_LAYOUT_BORDER,
                   layout);

  gdk_draw_rectangle (drawable,
                      widget->style->black_gc,
                      FALSE,
                      0, 0,
                      pixmap_width + 1,
                      pixmap_height + 1);

  g_object_unref (layout);

  return drawable;
}
