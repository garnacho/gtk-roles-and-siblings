/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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
#include <errno.h>
#include <string.h>
#include <pango/pango-utils.h>	/* For pango_scan_* */
#include "gtkiconfactory.h"
#include "stock-icons/gtkstockpixbufs.h"
#include "gtkdebug.h"
#include "gtksettings.h"
#include "gtkstock.h"
#include "gtkintl.h"

static GSList *all_icon_factories = NULL;

struct _GtkIconSource
{
  /* Either filename or pixbuf can be NULL. If both are non-NULL,
   * the pixbuf is assumed to be the already-loaded contents of the
   * file.
   */
  gchar *filename;
  GdkPixbuf *pixbuf;

  GtkTextDirection direction;
  GtkStateType state;
  GtkIconSize size;

  /* If TRUE, then the parameter is wildcarded, and the above
   * fields should be ignored. If FALSE, the parameter is
   * specified, and the above fields should be valid.
   */
  guint any_direction : 1;
  guint any_state : 1;
  guint any_size : 1;
};

static gpointer parent_class = NULL;

static void gtk_icon_factory_init       (GtkIconFactory      *icon_factory);
static void gtk_icon_factory_class_init (GtkIconFactoryClass *klass);
static void gtk_icon_factory_finalize   (GObject             *object);
static void get_default_icons           (GtkIconFactory      *icon_factory);

static GtkIconSize icon_size_register_intern (const gchar *name,
					      gint         width,
					      gint         height);

GType
gtk_icon_factory_get_type (void)
{
  static GType icon_factory_type = 0;

  if (!icon_factory_type)
    {
      static const GTypeInfo icon_factory_info =
      {
        sizeof (GtkIconFactoryClass),
        NULL,		/* base_init */
        NULL,		/* base_finalize */
        (GClassInitFunc) gtk_icon_factory_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkIconFactory),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_icon_factory_init,
      };
      
      icon_factory_type =
	g_type_register_static (G_TYPE_OBJECT, "GtkIconFactory",
				&icon_factory_info, 0);
    }
  
  return icon_factory_type;
}

static void
gtk_icon_factory_init (GtkIconFactory *factory)
{
  factory->icons = g_hash_table_new (g_str_hash, g_str_equal);
  all_icon_factories = g_slist_prepend (all_icon_factories, factory);
}

static void
gtk_icon_factory_class_init (GtkIconFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gtk_icon_factory_finalize;
}

static void
free_icon_set (gpointer key, gpointer value, gpointer data)
{
  g_free (key);
  gtk_icon_set_unref (value);
}

static void
gtk_icon_factory_finalize (GObject *object)
{
  GtkIconFactory *factory = GTK_ICON_FACTORY (object);

  all_icon_factories = g_slist_remove (all_icon_factories, factory);
  
  g_hash_table_foreach (factory->icons, free_icon_set, NULL);
  
  g_hash_table_destroy (factory->icons);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtk_icon_factory_new:
 *
 * Creates a new #GtkIconFactory. An icon factory manages a collection
 * of #GtkIconSet<!-- -->s; a #GtkIconSet manages a set of variants of a
 * particular icon (i.e. a #GtkIconSet contains variants for different
 * sizes and widget states). Icons in an icon factory are named by a
 * stock ID, which is a simple string identifying the icon. Each
 * #GtkStyle has a list of #GtkIconFactory<!-- -->s derived from the current
 * theme; those icon factories are consulted first when searching for
 * an icon. If the theme doesn't set a particular icon, GTK+ looks for
 * the icon in a list of default icon factories, maintained by
 * gtk_icon_factory_add_default() and
 * gtk_icon_factory_remove_default(). Applications with icons should
 * add a default icon factory with their icons, which will allow
 * themes to override the icons for the application.
 * 
 * Return value: a new #GtkIconFactory
 **/
GtkIconFactory*
gtk_icon_factory_new (void)
{
  return g_object_new (GTK_TYPE_ICON_FACTORY, NULL);
}

/**
 * gtk_icon_factory_add:
 * @factory: a #GtkIconFactory
 * @stock_id: icon name
 * @icon_set: icon set
 *
 * Adds the given @icon_set to the icon factory, under the name
 * @stock_id.  @stock_id should be namespaced for your application,
 * e.g. "myapp-whatever-icon".  Normally applications create a
 * #GtkIconFactory, then add it to the list of default factories with
 * gtk_icon_factory_add_default(). Then they pass the @stock_id to
 * widgets such as #GtkImage to display the icon. Themes can provide
 * an icon with the same name (such as "myapp-whatever-icon") to
 * override your application's default icons. If an icon already
 * existed in @factory for @stock_id, it is unreferenced and replaced
 * with the new @icon_set.
 * 
 **/
void
gtk_icon_factory_add (GtkIconFactory *factory,
                      const gchar    *stock_id,
                      GtkIconSet     *icon_set)
{
  gpointer old_key = NULL;
  gpointer old_value = NULL;

  g_return_if_fail (GTK_IS_ICON_FACTORY (factory));
  g_return_if_fail (stock_id != NULL);
  g_return_if_fail (icon_set != NULL);  

  g_hash_table_lookup_extended (factory->icons, stock_id,
                                &old_key, &old_value);

  if (old_value == icon_set)
    return;
  
  gtk_icon_set_ref (icon_set);

  /* GHashTable key memory management is so fantastically broken. */
  if (old_key)
    g_hash_table_insert (factory->icons, old_key, icon_set);
  else
    g_hash_table_insert (factory->icons, g_strdup (stock_id), icon_set);

  if (old_value)
    gtk_icon_set_unref (old_value);
}

/**
 * gtk_icon_factory_lookup:
 * @factory: a #GtkIconFactory
 * @stock_id: an icon name
 * 
 * Looks up @stock_id in the icon factory, returning an icon set
 * if found, otherwise %NULL. For display to the user, you should
 * use gtk_style_lookup_icon_set() on the #GtkStyle for the
 * widget that will display the icon, instead of using this
 * function directly, so that themes are taken into account.
 * 
 * Return value: icon set of @stock_id.
 **/
GtkIconSet *
gtk_icon_factory_lookup (GtkIconFactory *factory,
                         const gchar    *stock_id)
{
  g_return_val_if_fail (GTK_IS_ICON_FACTORY (factory), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  
  return g_hash_table_lookup (factory->icons, stock_id);
}

static GtkIconFactory *gtk_default_icons = NULL;
static GSList *default_factories = NULL;

/**
 * gtk_icon_factory_add_default:
 * @factory: a #GtkIconFactory
 * 
 * Adds an icon factory to the list of icon factories searched by
 * gtk_style_lookup_icon_set(). This means that, for example,
 * gtk_image_new_from_stock() will be able to find icons in @factory.
 * There will normally be an icon factory added for each library or
 * application that comes with icons. The default icon factories
 * can be overridden by themes.
 * 
 **/
void
gtk_icon_factory_add_default (GtkIconFactory *factory)
{
  g_return_if_fail (GTK_IS_ICON_FACTORY (factory));

  g_object_ref (factory);
  
  default_factories = g_slist_prepend (default_factories, factory);
}

/**
 * gtk_icon_factory_remove_default:
 * @factory: a #GtkIconFactory previously added with gtk_icon_factory_add_default()
 *
 * Removes an icon factory from the list of default icon
 * factories. Not normally used; you might use it for a library that
 * can be unloaded or shut down.
 * 
 **/
void
gtk_icon_factory_remove_default (GtkIconFactory  *factory)
{
  g_return_if_fail (GTK_IS_ICON_FACTORY (factory));

  default_factories = g_slist_remove (default_factories, factory);

  g_object_unref (factory);
}

static void
ensure_default_icons (void)
{
  if (gtk_default_icons == NULL)
    {
      gtk_default_icons = gtk_icon_factory_new ();

      get_default_icons (gtk_default_icons);
    }
}

/**
 * gtk_icon_factory_lookup_default:
 * @stock_id: an icon name
 *
 * Looks for an icon in the list of default icon factories.  For
 * display to the user, you should use gtk_style_lookup_icon_set() on
 * the #GtkStyle for the widget that will display the icon, instead of
 * using this function directly, so that themes are taken into
 * account.
 * 
 * 
 * Return value: a #GtkIconSet, or %NULL
 **/
GtkIconSet *
gtk_icon_factory_lookup_default (const gchar *stock_id)
{
  GSList *tmp_list;

  g_return_val_if_fail (stock_id != NULL, NULL);
  
  tmp_list = default_factories;
  while (tmp_list != NULL)
    {
      GtkIconSet *icon_set =
        gtk_icon_factory_lookup (GTK_ICON_FACTORY (tmp_list->data),
                                 stock_id);

      if (icon_set)
        return icon_set;
      
      tmp_list = g_slist_next (tmp_list);
    }

  ensure_default_icons ();
  
  return gtk_icon_factory_lookup (gtk_default_icons, stock_id);
}

static void
add_source (GtkIconSet    *set,
	    GtkIconSource *source,
	    const gchar   *inline_data)
{
  source->pixbuf = gdk_pixbuf_new_from_inline (-1, inline_data, FALSE, NULL);
  g_assert (source->pixbuf);

  gtk_icon_set_add_source (set, source);

  g_object_unref (source->pixbuf);
}

#if 0
static GtkIconSet *
sized_icon_set_from_inline (const guchar *inline_data,
                            GtkIconSize   size)
{
  GtkIconSet *set;

  GtkIconSource source = { NULL, NULL, 0, 0, 0,
                           TRUE, TRUE, FALSE };

  source.size = size;

  set = gtk_icon_set_new ();

  add_source (set, source, inline_data);
  
  return set;
}
#endif

static GtkIconSet *
sized_with_fallback_icon_set_from_inline (const guchar *fallback_data_ltr,
                                          const guchar *fallback_data_rtl,
                                          const guchar *inline_data_ltr,
                                          const guchar *inline_data_rtl,
                                          GtkIconSize   size)
{
  GtkIconSet *set;

  GtkIconSource source = { NULL, NULL, 0, 0, 0,
                           TRUE, TRUE, FALSE };

  set = gtk_icon_set_new ();

  source.size = size;
  source.any_direction = inline_data_rtl == NULL;
  
  source.direction = GTK_TEXT_DIR_LTR;
  add_source (set, &source, inline_data_ltr);
  
  if (inline_data_rtl != NULL)
    {
      source.direction = GTK_TEXT_DIR_RTL;
      add_source (set, &source, inline_data_rtl);
    }
  
  source.any_size = TRUE;
  source.any_direction = fallback_data_rtl == NULL;

  source.direction = GTK_TEXT_DIR_LTR;
  add_source (set, &source, fallback_data_ltr);
    
  if (fallback_data_rtl != NULL)
    {
      source.direction = GTK_TEXT_DIR_RTL;
      add_source (set, &source, fallback_data_rtl);
    }
  
  return set;
}

static GtkIconSet *
unsized_icon_set_from_inline (const guchar *inline_data)
{
  GtkIconSet *set;

  /* This icon can be used for any direction/state/size */
  GtkIconSource source = { NULL, NULL, 0, 0, 0,
                           TRUE, TRUE, TRUE };

  set = gtk_icon_set_new ();

  add_source (set, &source, inline_data);
  
  return set;
}

#if 0
static void
add_sized (GtkIconFactory *factory,
           const guchar   *inline_data,
           GtkIconSize     size,
           const gchar    *stock_id)
{
  GtkIconSet *set;
  
  set = sized_icon_set_from_inline (inline_data, size);
  
  gtk_icon_factory_add (factory, stock_id, set);

  gtk_icon_set_unref (set);
}
#endif

static void
add_sized_with_fallback_and_rtl (GtkIconFactory *factory,
				 const guchar   *fallback_data_ltr,
				 const guchar   *fallback_data_rtl,
				 const guchar   *inline_data_ltr,
				 const guchar   *inline_data_rtl,
				 GtkIconSize     size,
				 const gchar    *stock_id)
{
  GtkIconSet *set;

  set = sized_with_fallback_icon_set_from_inline (fallback_data_ltr, fallback_data_rtl,
						  inline_data_ltr, inline_data_rtl,
						  size);
  
  gtk_icon_factory_add (factory, stock_id, set);

  gtk_icon_set_unref (set);
}

static void
add_sized_with_fallback (GtkIconFactory *factory,
			 const guchar   *fallback_data,
			 const guchar   *inline_data,
			 GtkIconSize     size,
			 const gchar     *stock_id)
{
  add_sized_with_fallback_and_rtl (factory,
				   fallback_data, NULL,
				   inline_data, NULL,
				   size, stock_id);
}

static void
add_unsized (GtkIconFactory *factory,
             const guchar   *inline_data,
             const gchar    *stock_id)
{
  GtkIconSet *set;
  
  set = unsized_icon_set_from_inline (inline_data);
  
  gtk_icon_factory_add (factory, stock_id, set);

  gtk_icon_set_unref (set);
}

static void
get_default_icons (GtkIconFactory *factory)
{
  /* KEEP IN SYNC with gtkstock.c */

  /* We add all stock icons unsized, since it's confusing if icons only
   * can be loaded at certain sizes.
   */

  /* Have dialog size */
  add_unsized (factory, stock_dialog_error_48, GTK_STOCK_DIALOG_ERROR);
  add_unsized (factory, stock_dialog_info_48, GTK_STOCK_DIALOG_INFO);
  add_unsized (factory, stock_dialog_question_48, GTK_STOCK_DIALOG_QUESTION);
  add_unsized (factory, stock_dialog_warning_48,GTK_STOCK_DIALOG_WARNING);
  
  /* Have dnd size */
  add_unsized (factory, stock_dnd_32, GTK_STOCK_DND);
  add_unsized (factory, stock_dnd_multiple_32, GTK_STOCK_DND_MULTIPLE);
  
  /* Have button sizes */
  add_unsized (factory, stock_apply_20, GTK_STOCK_APPLY);
  add_unsized (factory, stock_cancel_20, GTK_STOCK_CANCEL);
  add_unsized (factory, stock_no_20, GTK_STOCK_NO);
  add_unsized (factory, stock_ok_20, GTK_STOCK_OK);
  add_unsized (factory, stock_yes_20, GTK_STOCK_YES);

  /* Generic + button sizes */
  add_sized_with_fallback (factory,
                           stock_close_24,
                           stock_close_20,
                           GTK_ICON_SIZE_BUTTON,
                           GTK_STOCK_CLOSE);

  /* Generic + menu sizes */  
  add_sized_with_fallback (factory,
			   stock_add_24,
			   stock_add_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_ADD);

  add_sized_with_fallback (factory,
			   stock_align_center_24,
			   stock_align_center_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_JUSTIFY_CENTER);

  add_sized_with_fallback (factory,
			   stock_align_justify_24,
			   stock_align_justify_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_JUSTIFY_FILL);

  add_sized_with_fallback (factory,
			   stock_align_left_24,
			   stock_align_left_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_JUSTIFY_LEFT);

  add_sized_with_fallback (factory,
			   stock_align_right_24,
			   stock_align_right_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_JUSTIFY_RIGHT);

  add_sized_with_fallback (factory,
			   stock_bottom_24,
			   stock_bottom_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_GOTO_BOTTOM);

  add_sized_with_fallback (factory,
                           stock_cdrom_24,
                           stock_cdrom_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_CDROM);

  add_sized_with_fallback (factory,
                           stock_convert_24,
                           stock_convert_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_CONVERT);

  add_sized_with_fallback (factory,
			   stock_copy_24,
			   stock_copy_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_COPY);

  add_sized_with_fallback (factory,
			   stock_cut_24,
			   stock_cut_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_CUT);

  add_sized_with_fallback (factory,
                           stock_down_arrow_24,
                           stock_down_arrow_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_GO_DOWN);

  add_sized_with_fallback (factory,
			   stock_exec_24,
			   stock_exec_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_EXECUTE);

  add_sized_with_fallback (factory,
			   stock_exit_24,
			   stock_exit_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_QUIT);

  add_sized_with_fallback_and_rtl (factory,
				   stock_first_24,
				   stock_last_24,
				   stock_first_16,
				   stock_last_16,
				   GTK_ICON_SIZE_MENU,
				   GTK_STOCK_GOTO_FIRST);

  add_sized_with_fallback (factory,
			   stock_font_24,
                           stock_font_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_SELECT_FONT);

  add_sized_with_fallback (factory,
                           stock_help_24,
                           stock_help_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_HELP);

  add_sized_with_fallback (factory,
                           stock_home_24,
                           stock_home_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_HOME);

  add_sized_with_fallback_and_rtl (factory,
				   stock_jump_to_24,
				   stock_jump_to_rtl_24,
				   stock_jump_to_16,
				   stock_jump_to_rtl_16,
				   GTK_ICON_SIZE_MENU,
				   GTK_STOCK_JUMP_TO);

  add_sized_with_fallback_and_rtl (factory,
				   stock_last_24,
				   stock_first_24,
				   stock_last_16,
				   stock_first_16,
				   GTK_ICON_SIZE_MENU,
				   GTK_STOCK_GOTO_LAST);

  add_sized_with_fallback_and_rtl (factory,
				   stock_left_arrow_24,
				   stock_right_arrow_24,
				   stock_left_arrow_16,
				   stock_right_arrow_16,
				   GTK_ICON_SIZE_MENU,
				   GTK_STOCK_GO_BACK);

  add_sized_with_fallback (factory,
			   stock_missing_image_24,
			   stock_missing_image_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_MISSING_IMAGE);

  add_sized_with_fallback (factory,
			   stock_new_24,
			   stock_new_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_NEW);

  add_sized_with_fallback (factory,
			   stock_open_24,
			   stock_open_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_OPEN);

  add_sized_with_fallback (factory,
                           stock_paste_24,
                           stock_paste_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_PASTE);

  add_sized_with_fallback (factory,
                           stock_preferences_24,
                           stock_preferences_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_PREFERENCES);

  add_sized_with_fallback (factory,
                           stock_print_24,
                           stock_print_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_PRINT);

  add_sized_with_fallback (factory,
                           stock_print_preview_24,
                           stock_print_preview_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_PRINT_PREVIEW);

  add_sized_with_fallback (factory,
			   stock_properties_24,
			   stock_properties_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_PROPERTIES);
  
  add_sized_with_fallback (factory,
			   stock_redo_24,
			   stock_redo_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_REDO);

  add_sized_with_fallback (factory,
			   stock_remove_24,
			   stock_remove_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_REMOVE);

  add_sized_with_fallback (factory,
			   stock_refresh_24,
			   stock_refresh_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_REFRESH);

  add_sized_with_fallback (factory,
			   stock_revert_24,
			   stock_revert_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_REVERT_TO_SAVED);

  add_sized_with_fallback_and_rtl (factory,
				   stock_right_arrow_24,
				   stock_left_arrow_24,
				   stock_right_arrow_16,
				   stock_left_arrow_16,
				   GTK_ICON_SIZE_MENU,
				   GTK_STOCK_GO_FORWARD);

  add_sized_with_fallback (factory,
			   stock_save_24,
			   stock_save_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_SAVE);

  add_sized_with_fallback (factory,
			   stock_save_24,
			   stock_save_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_FLOPPY);

  add_sized_with_fallback (factory,
			   stock_save_as_24,
			   stock_save_as_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_SAVE_AS);

  add_sized_with_fallback (factory,
			   stock_search_24,
			   stock_search_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_FIND);

  add_sized_with_fallback (factory,
			   stock_search_replace_24,
			   stock_search_replace_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_FIND_AND_REPLACE);

  add_sized_with_fallback (factory,
                           stock_sort_descending_24,
                           stock_sort_descending_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_SORT_DESCENDING);

  add_sized_with_fallback (factory,
                           stock_sort_ascending_24,
                           stock_sort_ascending_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_SORT_ASCENDING);

  add_sized_with_fallback (factory,
			   stock_spellcheck_24,
			   stock_spellcheck_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_SPELL_CHECK);

  add_sized_with_fallback (factory,
			   stock_stop_24,
			   stock_stop_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_STOP);

  add_sized_with_fallback (factory,
                           stock_text_bold_24,
                           stock_text_bold_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_BOLD);

  add_sized_with_fallback (factory,
                           stock_text_italic_24,
                           stock_text_italic_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_ITALIC);

  add_sized_with_fallback (factory,
                           stock_text_strikethrough_24,
                           stock_text_strikethrough_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_STRIKETHROUGH);

  add_sized_with_fallback (factory,
                           stock_text_underline_24,
                           stock_text_underline_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_UNDERLINE);

  add_sized_with_fallback (factory,
			   stock_top_24,
			   stock_top_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_GOTO_TOP);

  add_sized_with_fallback (factory,
                           stock_trash_24,
                           stock_trash_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_DELETE);

  add_sized_with_fallback (factory,
                           stock_undelete_24,
                           stock_undelete_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_UNDELETE);

  add_sized_with_fallback (factory,
			   stock_undo_24,
			   stock_undo_16,
			   GTK_ICON_SIZE_MENU,
			   GTK_STOCK_UNDO);

  add_sized_with_fallback (factory,
                           stock_up_arrow_24,
                           stock_up_arrow_16,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_GO_UP);

/* Generic size only */

  add_unsized (factory, stock_clear_24, GTK_STOCK_CLEAR);
  add_unsized (factory, stock_colorselector_24, GTK_STOCK_SELECT_COLOR);
  add_unsized (factory, stock_color_picker_25, GTK_STOCK_COLOR_PICKER);
  add_unsized (factory, stock_index_24, GTK_STOCK_INDEX);
  add_unsized (factory, stock_zoom_1_24, GTK_STOCK_ZOOM_100);
  add_unsized (factory, stock_zoom_fit_24, GTK_STOCK_ZOOM_FIT);
  add_unsized (factory, stock_zoom_in_24, GTK_STOCK_ZOOM_IN);
  add_unsized (factory, stock_zoom_out_24, GTK_STOCK_ZOOM_OUT);
}

/************************************************************
 *                    Icon size handling                    *
 ************************************************************/

typedef struct _IconSize IconSize;

struct _IconSize
{
  gint size;
  gchar *name;
  
  gint width;
  gint height;
};

typedef struct _IconAlias IconAlias;

struct _IconAlias
{
  gchar *name;
  gint   target;
};

typedef struct _SettingsIconSize SettingsIconSize;

struct _SettingsIconSize
{
  gint width;
  gint height;
};

static GHashTable *icon_aliases = NULL;
static IconSize *icon_sizes = NULL;
static gint      icon_sizes_allocated = 0;
static gint      icon_sizes_used = 0;

static void
init_icon_sizes (void)
{
  if (icon_sizes == NULL)
    {
#define NUM_BUILTIN_SIZES 7
      gint i;

      icon_aliases = g_hash_table_new (g_str_hash, g_str_equal);
      
      icon_sizes = g_new (IconSize, NUM_BUILTIN_SIZES);
      icon_sizes_allocated = NUM_BUILTIN_SIZES;
      icon_sizes_used = NUM_BUILTIN_SIZES;

      icon_sizes[GTK_ICON_SIZE_INVALID].size = 0;
      icon_sizes[GTK_ICON_SIZE_INVALID].name = NULL;
      icon_sizes[GTK_ICON_SIZE_INVALID].width = 0;
      icon_sizes[GTK_ICON_SIZE_INVALID].height = 0;

      /* the name strings aren't copied since we don't ever remove
       * icon sizes, so we don't need to know whether they're static.
       * Even if we did I suppose removing the builtin sizes would be
       * disallowed.
       */
      
      icon_sizes[GTK_ICON_SIZE_MENU].size = GTK_ICON_SIZE_MENU;
      icon_sizes[GTK_ICON_SIZE_MENU].name = "gtk-menu";
      icon_sizes[GTK_ICON_SIZE_MENU].width = 16;
      icon_sizes[GTK_ICON_SIZE_MENU].height = 16;

      icon_sizes[GTK_ICON_SIZE_BUTTON].size = GTK_ICON_SIZE_BUTTON;
      icon_sizes[GTK_ICON_SIZE_BUTTON].name = "gtk-button";
      icon_sizes[GTK_ICON_SIZE_BUTTON].width = 20;
      icon_sizes[GTK_ICON_SIZE_BUTTON].height = 20;

      icon_sizes[GTK_ICON_SIZE_SMALL_TOOLBAR].size = GTK_ICON_SIZE_SMALL_TOOLBAR;
      icon_sizes[GTK_ICON_SIZE_SMALL_TOOLBAR].name = "gtk-small-toolbar";
      icon_sizes[GTK_ICON_SIZE_SMALL_TOOLBAR].width = 18;
      icon_sizes[GTK_ICON_SIZE_SMALL_TOOLBAR].height = 18;
      
      icon_sizes[GTK_ICON_SIZE_LARGE_TOOLBAR].size = GTK_ICON_SIZE_LARGE_TOOLBAR;
      icon_sizes[GTK_ICON_SIZE_LARGE_TOOLBAR].name = "gtk-large-toolbar";
      icon_sizes[GTK_ICON_SIZE_LARGE_TOOLBAR].width = 24;
      icon_sizes[GTK_ICON_SIZE_LARGE_TOOLBAR].height = 24;

      icon_sizes[GTK_ICON_SIZE_DND].size = GTK_ICON_SIZE_DND;
      icon_sizes[GTK_ICON_SIZE_DND].name = "gtk-dnd";
      icon_sizes[GTK_ICON_SIZE_DND].width = 32;
      icon_sizes[GTK_ICON_SIZE_DND].height = 32;

      icon_sizes[GTK_ICON_SIZE_DIALOG].size = GTK_ICON_SIZE_DIALOG;
      icon_sizes[GTK_ICON_SIZE_DIALOG].name = "gtk-dialog";
      icon_sizes[GTK_ICON_SIZE_DIALOG].width = 48;
      icon_sizes[GTK_ICON_SIZE_DIALOG].height = 48;

      g_assert ((GTK_ICON_SIZE_DIALOG + 1) == NUM_BUILTIN_SIZES);

      /* Alias everything to itself. */
      i = 1; /* skip invalid size */
      while (i < NUM_BUILTIN_SIZES)
        {
          gtk_icon_size_register_alias (icon_sizes[i].name, icon_sizes[i].size);
          
          ++i;
        }
      
#undef NUM_BUILTIN_SIZES
    }
}

static void
free_settings_sizes (gpointer data)
{
  g_array_free (data, TRUE);
}

static GArray *
get_settings_sizes (GtkSettings *settings,
		    gboolean    *created)
{
  GArray *settings_sizes;
  static GQuark sizes_quark = 0;

  if (!sizes_quark)
    sizes_quark = g_quark_from_static_string ("gtk-icon-sizes");

  settings_sizes = g_object_get_qdata (G_OBJECT (settings), sizes_quark);
  if (!settings_sizes)
    {
      settings_sizes = g_array_new (FALSE, FALSE, sizeof (SettingsIconSize));
      g_object_set_qdata_full (G_OBJECT (settings), sizes_quark,
			       settings_sizes, free_settings_sizes);
      if (created)
	*created = TRUE;
    }

  return settings_sizes;
}

static void
icon_size_set_for_settings (GtkSettings *settings,
			    const gchar *size_name,
			    gint         width,
			    gint         height)
{
  GtkIconSize size;
  GArray *settings_sizes;
  SettingsIconSize *settings_size;

  g_return_if_fail (size_name != NULL);

  size = gtk_icon_size_from_name (size_name);
  if (size == GTK_ICON_SIZE_INVALID)
    /* Reserve a place */
    size = icon_size_register_intern (size_name, -1, -1);
  
  settings_sizes = get_settings_sizes (settings, NULL);
  if (size >= settings_sizes->len)
    {
      SettingsIconSize unset = { -1, -1 };
      gint i;

      for (i = settings_sizes->len; i <= size; i++)
	g_array_append_val (settings_sizes, unset);
    }

  settings_size = &g_array_index (settings_sizes, SettingsIconSize, size);
  
  settings_size->width = width;
  settings_size->height = height;
}

/* Like pango_parse_word, but accept - as well
 */
static gboolean
scan_icon_size_name (const char **pos, GString *out)
{
  const char *p = *pos;

  while (g_ascii_isspace (*p))
    p++;
  
  if (!((*p >= 'A' && *p <= 'Z') ||
	(*p >= 'a' && *p <= 'z') ||
	*p == '_' || *p == '-'))
    return FALSE;

  g_string_truncate (out, 0);
  g_string_append_c (out, *p);
  p++;

  while ((*p >= 'A' && *p <= 'Z') ||
	 (*p >= 'a' && *p <= 'z') ||
	 (*p >= '0' && *p <= '9') ||
	 *p == '_' || *p == '-')
    {
      g_string_append_c (out, *p);
      p++;
    }

  *pos = p;

  return TRUE;
}

static void
icon_size_setting_parse (GtkSettings *settings,
			 const gchar *icon_size_string)
{
  GString *name_buf = g_string_new (NULL);
  const gchar *p = icon_size_string;

  while (pango_skip_space (&p))
    {
      gint width, height;
      
      if (!scan_icon_size_name (&p, name_buf))
	goto err;

      if (!pango_skip_space (&p))
	goto err;

      if (*p != '=')
	goto err;

      p++;

      if (!pango_scan_int (&p, &width))
	goto err;

      if (!pango_skip_space (&p))
	goto err;

      if (*p != ',')
	goto err;

      p++;

      if (!pango_scan_int (&p, &height))
	goto err;

      if (width > 0 && height > 0)
	{
	  icon_size_set_for_settings (settings, name_buf->str,
				      width, height);
	}
      else
	{
	  g_warning ("Invalid size in gtk-icon-sizes: %d,%d\n", width, height);
	}

      pango_skip_space (&p);
      if (*p == '\0')
	break;
      if (*p == ':')
	p++;
      else
	goto err;
    }

  g_string_free (name_buf, TRUE);
  return;

 err:
  g_warning ("Error parsing gtk-icon-sizes string:\n\t'%s'", icon_size_string);
  g_string_free (name_buf, TRUE);
}

static void
icon_size_set_all_from_settings (GtkSettings *settings)
{
  GArray *settings_sizes;
  gchar *icon_size_string;

  /* Reset old settings */
  settings_sizes = get_settings_sizes (settings, NULL);
  g_array_set_size (settings_sizes, 0);

  g_object_get (settings,
		"gtk-icon-sizes", &icon_size_string,
		NULL);

  if (icon_size_string)
    {
      icon_size_setting_parse (settings, icon_size_string);
      g_free (icon_size_string);
    }
}

static void
icon_size_settings_changed (GtkSettings  *settings,
			    GParamSpec   *pspec)
{
  icon_size_set_all_from_settings (settings);

  _gtk_rc_reset_styles (settings);
}

static void
icon_sizes_init_for_settings (GtkSettings *settings)
{
  g_signal_connect (settings,
		    "notify::gtk-icon-sizes",
		    G_CALLBACK (icon_size_settings_changed),
		    NULL);
  
  icon_size_set_all_from_settings (settings);
}
     
gboolean
icon_size_lookup_intern (GtkSettings *settings,
			 GtkIconSize  size,
			 gint        *widthp,
			 gint        *heightp)
{
  GArray *settings_sizes;
  gint width_for_settings = -1;
  gint height_for_settings = -1;
  
  init_icon_sizes ();

  if (size >= icon_sizes_used)
    return FALSE;

  if (size == GTK_ICON_SIZE_INVALID)
    return FALSE;

  if (settings)
    {
      gboolean initial = FALSE;
      
      settings_sizes = get_settings_sizes (settings, &initial);
      if (initial)
	icon_sizes_init_for_settings (settings);
  
      if (size < settings_sizes->len)
	{
	  SettingsIconSize *settings_size;
	  
	  settings_size = &g_array_index (settings_sizes, SettingsIconSize, size);
	  
	  width_for_settings = settings_size->width;
	  height_for_settings = settings_size->height;
	}
    }

  if (widthp)
    *widthp = width_for_settings >= 0 ? width_for_settings : icon_sizes[size].width;

  if (heightp)
    *heightp = height_for_settings >= 0 ? height_for_settings : icon_sizes[size].height;

  return TRUE;
}

/**
 * gtk_icon_size_lookup_for_settings:
 * @settings: a #GtkSettings object, used to determine
 *   which set of user preferences to used.
 * @size: an icon size
 * @width: location to store icon width
 * @height: location to store icon height
 *
 * Obtains the pixel size of a semantic icon size, possibly
 * modified by user preferences for a particular 
 * #GtkSettings. Normally @size would be
 * #GTK_ICON_SIZE_MENU, #GTK_ICON_SIZE_BUTTON, etc.  This function
 * isn't normally needed, gtk_widget_render_icon() is the usual
 * way to get an icon for rendering, then just look at the size of
 * the rendered pixbuf. The rendered pixbuf may not even correspond to
 * the width/height returned by gtk_icon_size_lookup(), because themes
 * are free to render the pixbuf however they like, including changing
 * the usual size.
 * 
 * Return value: %TRUE if @size was a valid size
 *
 * Since: 2.2
 **/
gboolean
gtk_icon_size_lookup_for_settings (GtkSettings *settings,
				   GtkIconSize  size,
				   gint        *width,
				   gint        *height)
{
  g_return_val_if_fail (GTK_IS_SETTINGS (settings), FALSE);

  return icon_size_lookup_intern (settings, size, width, height);
}

/**
 * gtk_icon_size_lookup:
 * @size: an icon size
 * @width: location to store icon width
 * @height: location to store icon height
 *
 * Obtains the pixel size of a semantic icon size, possibly
 * modified by user preferences for the default #GtkSettings.
 * (See gtk_icon_size_lookup_for_settings().)
 * Normally @size would be
 * #GTK_ICON_SIZE_MENU, #GTK_ICON_SIZE_BUTTON, etc.  This function
 * isn't normally needed, gtk_widget_render_icon() is the usual
 * way to get an icon for rendering, then just look at the size of
 * the rendered pixbuf. The rendered pixbuf may not even correspond to
 * the width/height returned by gtk_icon_size_lookup(), because themes
 * are free to render the pixbuf however they like, including changing
 * the usual size.
 * 
 * Return value: %TRUE if @size was a valid size
 **/
gboolean
gtk_icon_size_lookup (GtkIconSize  size,
                      gint        *widthp,
                      gint        *heightp)
{
  GTK_NOTE (MULTIHEAD,
	    g_warning ("gtk_icon_size_lookup ()) is not multihead safe"));

  return gtk_icon_size_lookup_for_settings (gtk_settings_get_default (),
					    size, widthp, heightp);
}

static GtkIconSize
icon_size_register_intern (const gchar *name,
			   gint         width,
			   gint         height)
{
  IconAlias *old_alias;
  GtkIconSize size;
  
  init_icon_sizes ();

  old_alias = g_hash_table_lookup (icon_aliases, name);
  if (old_alias && icon_sizes[old_alias->target].width > 0)
    {
      g_warning ("Icon size name '%s' already exists", name);
      return GTK_ICON_SIZE_INVALID;
    }

  if (old_alias)
    {
      size = old_alias->target;
    }
  else
    {
      if (icon_sizes_used == icon_sizes_allocated)
	{
	  icon_sizes_allocated *= 2;
	  icon_sizes = g_renew (IconSize, icon_sizes, icon_sizes_allocated);
	}

      size = icon_sizes_used++;

      /* alias to self. */
      gtk_icon_size_register_alias (name, size);

      icon_sizes[size].size = size;
      icon_sizes[size].name = g_strdup (name);
    }

  icon_sizes[size].width = width;
  icon_sizes[size].height = height;

  return size;
}

/**
 * gtk_icon_size_register:
 * @name: name of the icon size
 * @width: the icon width
 * @height: the icon height
 *
 * Registers a new icon size, along the same lines as #GTK_ICON_SIZE_MENU,
 * etc. Returns the integer value for the size.
 *
 * Returns: integer value representing the size
 * 
 **/
GtkIconSize
gtk_icon_size_register (const gchar *name,
                        gint         width,
                        gint         height)
{
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (width > 0, 0);
  g_return_val_if_fail (height > 0, 0);
  
  return icon_size_register_intern (name, width, height);
}

/**
 * gtk_icon_size_register_alias:
 * @alias: an alias for @target
 * @target: an existing icon size
 *
 * Registers @alias as another name for @target.
 * So calling gtk_icon_size_from_name() with @alias as argument
 * will return @target.
 *
 **/
void
gtk_icon_size_register_alias (const gchar *alias,
                              GtkIconSize  target)
{
  IconAlias *ia;
  
  g_return_if_fail (alias != NULL);

  init_icon_sizes ();

  if (!icon_size_lookup_intern (NULL, target, NULL, NULL))
    g_warning ("gtk_icon_size_register_alias: Icon size %d does not exist", target);

  ia = g_hash_table_lookup (icon_aliases, alias);
  if (ia)
    {
      if (icon_sizes[ia->target].width > 0)
	{
	  g_warning ("gtk_icon_size_register_alias: Icon size name '%s' already exists", alias);
	  return;
	}

      ia->target = target;
    }

  if (!ia)
    {
      ia = g_new (IconAlias, 1);
      ia->name = g_strdup (alias);
      ia->target = target;

      g_hash_table_insert (icon_aliases, ia->name, ia);
    }
}

/** 
 * gtk_icon_size_from_name:
 * @name: the name to look up.
 * @returns: the icon size with the given name.
 * 
 * Looks up the icon size associated with @name.
 **/
GtkIconSize
gtk_icon_size_from_name (const gchar *name)
{
  IconAlias *ia;

  init_icon_sizes ();
  
  ia = g_hash_table_lookup (icon_aliases, name);

  if (ia && icon_sizes[ia->target].width > 0)
    return ia->target;
  else
    return GTK_ICON_SIZE_INVALID;
}

/**
 * gtk_icon_size_get_name:
 * @size: a #GtkIconSize.
 * @returns: the name of the given icon size.
 * 
 * Gets the canonical name of the given icon size. The returned string 
 * is statically allocated and should not be freed.
 **/
G_CONST_RETURN gchar*
gtk_icon_size_get_name (GtkIconSize  size)
{
  if (size >= icon_sizes_used)
    return NULL;
  else
    return icon_sizes[size].name;
}

/************************************************************/

/* Icon Set */


/* Clear icon set contents, drop references to all contained
 * GdkPixbuf objects and forget all GtkIconSources. Used to
 * recycle an icon set.
 */
static GdkPixbuf *find_in_cache     (GtkIconSet       *icon_set,
                                     GtkStyle         *style,
                                     GtkTextDirection  direction,
                                     GtkStateType      state,
                                     GtkIconSize       size);
static void       add_to_cache      (GtkIconSet       *icon_set,
                                     GtkStyle         *style,
                                     GtkTextDirection  direction,
                                     GtkStateType      state,
                                     GtkIconSize       size,
                                     GdkPixbuf        *pixbuf);
static void       clear_cache       (GtkIconSet       *icon_set,
                                     gboolean          style_detach);
static GSList*    copy_cache        (GtkIconSet       *icon_set,
                                     GtkIconSet       *copy_recipient);
static void       attach_to_style   (GtkIconSet       *icon_set,
                                     GtkStyle         *style);
static void       detach_from_style (GtkIconSet       *icon_set,
                                     GtkStyle         *style);
static void       style_dnotify     (gpointer          data);

struct _GtkIconSet
{
  guint ref_count;

  GSList *sources;

  /* Cache of the last few rendered versions of the icon. */
  GSList *cache;

  guint cache_size;

  guint cache_serial;
};

static guint cache_serial = 0;

/**
 * gtk_icon_set_new:
 * 
 * Creates a new #GtkIconSet. A #GtkIconSet represents a single icon
 * in various sizes and widget states. It can provide a #GdkPixbuf
 * for a given size and state on request, and automatically caches
 * some of the rendered #GdkPixbuf objects.
 *
 * Normally you would use gtk_widget_render_icon() instead of
 * using #GtkIconSet directly. The one case where you'd use
 * #GtkIconSet is to create application-specific icon sets to place in
 * a #GtkIconFactory.
 * 
 * Return value: a new #GtkIconSet
 **/
GtkIconSet*
gtk_icon_set_new (void)
{
  GtkIconSet *icon_set;

  icon_set = g_new (GtkIconSet, 1);

  icon_set->ref_count = 1;
  icon_set->sources = NULL;
  icon_set->cache = NULL;
  icon_set->cache_size = 0;
  icon_set->cache_serial = cache_serial;
  
  return icon_set;
}

/**
 * gtk_icon_set_new_from_pixbuf:
 * @pixbuf: a #GdkPixbuf
 * 
 * Creates a new #GtkIconSet with @pixbuf as the default/fallback
 * source image. If you don't add any additional #GtkIconSource to the
 * icon set, all variants of the icon will be created from @pixbuf,
 * using scaling, pixelation, etc. as required to adjust the icon size
 * or make the icon look insensitive/prelighted.
 * 
 * Return value: a new #GtkIconSet
 **/
GtkIconSet *
gtk_icon_set_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  GtkIconSet *set;

  GtkIconSource source = { NULL, NULL, 0, 0, 0,
                           TRUE, TRUE, TRUE };

  g_return_val_if_fail (pixbuf != NULL, NULL);

  set = gtk_icon_set_new ();

  source.pixbuf = pixbuf;

  gtk_icon_set_add_source (set, &source);
  
  return set;
}


/**
 * gtk_icon_set_ref:
 * @icon_set: a #GtkIconSet.
 * 
 * Increments the reference count on @icon_set.
 * 
 * Return value: @icon_set.
 **/
GtkIconSet*
gtk_icon_set_ref (GtkIconSet *icon_set)
{
  g_return_val_if_fail (icon_set != NULL, NULL);
  g_return_val_if_fail (icon_set->ref_count > 0, NULL);

  icon_set->ref_count += 1;

  return icon_set;
}

/**
 * gtk_icon_set_unref:
 * @icon_set: a #GtkIconSet
 * 
 * Decrements the reference count on @icon_set, and frees memory
 * if the reference count reaches 0.
 **/
void
gtk_icon_set_unref (GtkIconSet *icon_set)
{
  g_return_if_fail (icon_set != NULL);
  g_return_if_fail (icon_set->ref_count > 0);

  icon_set->ref_count -= 1;

  if (icon_set->ref_count == 0)
    {
      GSList *tmp_list = icon_set->sources;
      while (tmp_list != NULL)
        {
          gtk_icon_source_free (tmp_list->data);

          tmp_list = g_slist_next (tmp_list);
        }

      clear_cache (icon_set, TRUE);

      g_free (icon_set);
    }
}

GType
gtk_icon_set_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("GtkIconSet",
					     (GBoxedCopyFunc) gtk_icon_set_ref,
					     (GBoxedFreeFunc) gtk_icon_set_unref);

  return our_type;
}

/**
 * gtk_icon_set_copy:
 * @icon_set: a #GtkIconSet
 * 
 * Copies @icon_set by value. 
 * 
 * Return value: a new #GtkIconSet identical to the first.
 **/
GtkIconSet*
gtk_icon_set_copy (GtkIconSet *icon_set)
{
  GtkIconSet *copy;
  GSList *tmp_list;
  
  copy = gtk_icon_set_new ();

  tmp_list = icon_set->sources;
  while (tmp_list != NULL)
    {
      copy->sources = g_slist_prepend (copy->sources,
                                       gtk_icon_source_copy (tmp_list->data));

      tmp_list = g_slist_next (tmp_list);
    }

  copy->sources = g_slist_reverse (copy->sources);

  copy->cache = copy_cache (icon_set, copy);
  copy->cache_size = icon_set->cache_size;
  copy->cache_serial = icon_set->cache_serial;
  
  return copy;
}

static gboolean
sizes_equivalent (GtkIconSize lhs,
                  GtkIconSize rhs)
{
  /* We used to consider sizes equivalent if they were
   * the same pixel size, but we don't have the GtkSettings
   * here, so we can't do that. Plus, it's not clear that
   * it is right... it was just a workaround for the fact
   * that we register icons by logical size, not pixel size.
   */
#if 1
  return lhs == rhs;
#else  
  
  gint r_w, r_h, l_w, l_h;

  icon_size_lookup_intern (NULL, rhs, &r_w, &r_h);
  icon_size_lookup_intern (NULL, lhs, &l_w, &l_h);

  return r_w == l_w && r_h == l_h;
#endif
}

static GtkIconSource*
find_and_prep_icon_source (GtkIconSet       *icon_set,
                           GtkTextDirection  direction,
                           GtkStateType      state,
                           GtkIconSize       size)
{
  GtkIconSource *source;
  GSList *tmp_list;


  /* We need to find the best icon source.  Direction matters more
   * than state, state matters more than size. icon_set->sources
   * is sorted according to wildness, so if we take the first
   * match we find it will be the least-wild match (if there are
   * multiple matches for a given "wildness" then the RC file contained
   * dumb stuff, and we end up with an arbitrary matching source)
   */
  
  source = NULL;
  tmp_list = icon_set->sources;
  while (tmp_list != NULL)
    {
      GtkIconSource *s = tmp_list->data;
      
      if ((s->any_direction || (s->direction == direction)) &&
          (s->any_state || (s->state == state)) &&
          (s->any_size || (sizes_equivalent (size, s->size))))
        {
          source = s;
          break;
        }
      
      tmp_list = g_slist_next (tmp_list);
    }

  if (source == NULL)
    return NULL;
  
  if (source->pixbuf == NULL)
    {
      GError *error = NULL;
      
      g_assert (source->filename);
      source->pixbuf = gdk_pixbuf_new_from_file (source->filename, &error);

      if (source->pixbuf == NULL)
        {
          /* Remove this icon source so we don't keep trying to
           * load it.
           */
          g_warning (_("Error loading icon: %s"), error->message);
          g_error_free (error);
          
          icon_set->sources = g_slist_remove (icon_set->sources, source);

          gtk_icon_source_free (source);

          /* Try to fall back to other sources */
          if (icon_set->sources != NULL)
            return find_and_prep_icon_source (icon_set,
                                              direction,
                                              state,
                                              size);
          else
            return NULL;
        }
    }

  return source;
}

static GdkPixbuf*
render_fallback_image (GtkStyle          *style,
                       GtkTextDirection   direction,
                       GtkStateType       state,
                       GtkIconSize        size,
                       GtkWidget         *widget,
                       const char        *detail)
{
  /* This icon can be used for any direction/state/size */
  static GtkIconSource fallback_source = { NULL, NULL, 0, 0, 0, TRUE, TRUE, TRUE };

  if (fallback_source.pixbuf == NULL)
    fallback_source.pixbuf = gdk_pixbuf_new_from_inline (-1, stock_missing_image_24, FALSE, NULL);
  
  return gtk_style_render_icon (style,
                                &fallback_source,
                                direction,
                                state,
                                size,
                                widget,
                                detail);
}

/**
 * gtk_icon_set_render_icon:
 * @icon_set: a #GtkIconSet
 * @style: a #GtkStyle associated with @widget, or %NULL
 * @direction: text direction
 * @state: widget state
 * @size: icon size
 * @widget: widget that will display the icon, or %NULL
 * @detail: detail to pass to the theme engine, or %NULL
 * 
 * Renders an icon using gtk_style_render_icon(). In most cases,
 * gtk_widget_render_icon() is better, since it automatically provides
 * most of the arguments from the current widget settings.  This
 * function never returns %NULL; if the icon can't be rendered
 * (perhaps because an image file fails to load), a default "missing
 * image" icon will be returned instead.
 * 
 * Return value: a #GdkPixbuf to be displayed
 **/
GdkPixbuf*
gtk_icon_set_render_icon (GtkIconSet        *icon_set,
                          GtkStyle          *style,
                          GtkTextDirection   direction,
                          GtkStateType       state,
                          GtkIconSize        size,
                          GtkWidget         *widget,
                          const char        *detail)
{
  GdkPixbuf *icon;
  GtkIconSource *source;
  
  g_return_val_if_fail (icon_set != NULL, NULL);
  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);

  if (icon_set->sources == NULL)
    return render_fallback_image (style, direction, state, size, widget, detail);
  
  icon = find_in_cache (icon_set, style, direction,
                        state, size);

  if (icon)
    {
      g_object_ref (icon);
      return icon;
    }

  
  source = find_and_prep_icon_source (icon_set, direction, state, size);

  if (source == NULL)
    return render_fallback_image (style, direction, state, size, widget, detail);

  g_assert (source->pixbuf != NULL);
  
  icon = gtk_style_render_icon (style,
                                source,
                                direction,
                                state,
                                size,
                                widget,
                                detail);

  if (icon == NULL)
    {
      g_warning ("Theme engine failed to render icon");
      return NULL;
    }
  
  add_to_cache (icon_set, style, direction, state, size, icon);
  
  return icon;
}

/* Order sources by their "wildness", so that "wilder" sources are
 * greater than "specific" sources; for determining ordering,
 * direction beats state beats size.
 */

static int
icon_source_compare (gconstpointer ap, gconstpointer bp)
{
  const GtkIconSource *a = ap;
  const GtkIconSource *b = bp;

  if (!a->any_direction && b->any_direction)
    return -1;
  else if (a->any_direction && !b->any_direction)
    return 1;
  else if (!a->any_state && b->any_state)
    return -1;
  else if (a->any_state && !b->any_state)
    return 1;
  else if (!a->any_size && b->any_size)
    return -1;
  else if (a->any_size && !b->any_size)
    return 1;
  else
    return 0;
}

/**
 * gtk_icon_set_add_source:
 * @icon_set: a #GtkIconSet
 * @source: a #GtkIconSource
 *
 * Icon sets have a list of #GtkIconSource, which they use as base
 * icons for rendering icons in different states and sizes. Icons are
 * scaled, made to look insensitive, etc. in
 * gtk_icon_set_render_icon(), but #GtkIconSet needs base images to
 * work with. The base images and when to use them are described by
 * a #GtkIconSource.
 * 
 * This function copies @source, so you can reuse the same source immediately
 * without affecting the icon set.
 *
 * An example of when you'd use this function: a web browser's "Back
 * to Previous Page" icon might point in a different direction in
 * Hebrew and in English; it might look different when insensitive;
 * and it might change size depending on toolbar mode (small/large
 * icons). So a single icon set would contain all those variants of
 * the icon, and you might add a separate source for each one.
 *
 * You should nearly always add a "default" icon source with all
 * fields wildcarded, which will be used as a fallback if no more
 * specific source matches. #GtkIconSet always prefers more specific
 * icon sources to more generic icon sources. The order in which you
 * add the sources to the icon set does not matter.
 *
 * gtk_icon_set_new_from_pixbuf() creates a new icon set with a
 * default icon source based on the given pixbuf.
 * 
 **/
void
gtk_icon_set_add_source (GtkIconSet *icon_set,
                         const GtkIconSource *source)
{
  g_return_if_fail (icon_set != NULL);
  g_return_if_fail (source != NULL);

  if (source->pixbuf == NULL &&
      source->filename == NULL)
    {
      g_warning ("Useless GtkIconSource contains NULL filename and pixbuf");
      return;
    }
  
  icon_set->sources = g_slist_insert_sorted (icon_set->sources,
                                             gtk_icon_source_copy (source),
                                             icon_source_compare);
}

/**
 * gtk_icon_set_get_sizes:
 * @icon_set: a #GtkIconSet
 * @sizes: return location for array of sizes
 * @n_sizes: location to store number of elements in returned array
 *
 * Obtains a list of icon sizes this icon set can render. The returned
 * array must be freed with g_free().
 * 
 **/
void
gtk_icon_set_get_sizes (GtkIconSet   *icon_set,
                        GtkIconSize **sizes,
                        gint         *n_sizes)
{
  GSList *tmp_list;
  gboolean all_sizes = FALSE;
  GSList *specifics = NULL;
  
  g_return_if_fail (icon_set != NULL);
  g_return_if_fail (sizes != NULL);
  g_return_if_fail (n_sizes != NULL);
  
  tmp_list = icon_set->sources;
  while (tmp_list != NULL)
    {
      GtkIconSource *source;

      source = tmp_list->data;

      if (source->any_size)
        {
          all_sizes = TRUE;
          break;
        }
      else
        specifics = g_slist_prepend (specifics, GINT_TO_POINTER (source->size));
      
      tmp_list = g_slist_next (tmp_list);
    }

  if (all_sizes)
    {
      /* Need to find out what sizes exist */
      gint i;

      init_icon_sizes ();
      
      *sizes = g_new (GtkIconSize, icon_sizes_used);
      *n_sizes = icon_sizes_used - 1;
      
      i = 1;      
      while (i < icon_sizes_used)
        {
          (*sizes)[i - 1] = icon_sizes[i].size;
          ++i;
        }
    }
  else
    {
      gint i;
      
      *n_sizes = g_slist_length (specifics);
      *sizes = g_new (GtkIconSize, *n_sizes);

      i = 0;
      tmp_list = specifics;
      while (tmp_list != NULL)
        {
          (*sizes)[i] = GPOINTER_TO_INT (tmp_list->data);

          ++i;
          tmp_list = g_slist_next (tmp_list);
        }
    }

  g_slist_free (specifics);
}


/**
 * gtk_icon_source_new:
 * 
 * Creates a new #GtkIconSource. A #GtkIconSource contains a #GdkPixbuf (or
 * image filename) that serves as the base image for one or more of the
 * icons in a #GtkIconSet, along with a specification for which icons in the
 * icon set will be based on that pixbuf or image file. An icon set contains
 * a set of icons that represent "the same" logical concept in different states,
 * different global text directions, and different sizes.
 * 
 * So for example a web browser's "Back to Previous Page" icon might
 * point in a different direction in Hebrew and in English; it might
 * look different when insensitive; and it might change size depending
 * on toolbar mode (small/large icons). So a single icon set would
 * contain all those variants of the icon. #GtkIconSet contains a list
 * of #GtkIconSource from which it can derive specific icon variants in
 * the set. 
 *
 * In the simplest case, #GtkIconSet contains one source pixbuf from
 * which it derives all variants. The convenience function
 * gtk_icon_set_new_from_pixbuf() handles this case; if you only have
 * one source pixbuf, just use that function.
 *
 * If you want to use a different base pixbuf for different icon
 * variants, you create multiple icon sources, mark which variants
 * they'll be used to create, and add them to the icon set with
 * gtk_icon_set_add_source().
 *
 * By default, the icon source has all parameters wildcarded. That is,
 * the icon source will be used as the base icon for any desired text
 * direction, widget state, or icon size.
 * 
 * Return value: a new #GtkIconSource
 **/
GtkIconSource*
gtk_icon_source_new (void)
{
  GtkIconSource *src;
  
  src = g_new0 (GtkIconSource, 1);

  src->direction = GTK_TEXT_DIR_NONE;
  src->size = GTK_ICON_SIZE_INVALID;
  src->state = GTK_STATE_NORMAL;
  
  src->any_direction = TRUE;
  src->any_state = TRUE;
  src->any_size = TRUE;
  
  return src;
}

/**
 * gtk_icon_source_copy:
 * @source: a #GtkIconSource
 * 
 * Creates a copy of @source; mostly useful for language bindings.
 * 
 * Return value: a new #GtkIconSource
 **/
GtkIconSource*
gtk_icon_source_copy (const GtkIconSource *source)
{
  GtkIconSource *copy;
  
  g_return_val_if_fail (source != NULL, NULL);

  copy = g_new (GtkIconSource, 1);

  *copy = *source;
  
  copy->filename = g_strdup (source->filename);
  copy->size = source->size;
  if (copy->pixbuf)
    g_object_ref (copy->pixbuf);

  return copy;
}

/**
 * gtk_icon_source_free:
 * @source: a #GtkIconSource
 * 
 * Frees a dynamically-allocated icon source, along with its
 * filename, size, and pixbuf fields if those are not %NULL.
 **/
void
gtk_icon_source_free (GtkIconSource *source)
{
  g_return_if_fail (source != NULL);

  g_free ((char*) source->filename);
  if (source->pixbuf)
    g_object_unref (source->pixbuf);

  g_free (source);
}

GType
gtk_icon_source_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("GtkIconSource",
					     (GBoxedCopyFunc) gtk_icon_source_copy,
					     (GBoxedFreeFunc) gtk_icon_source_free);

  return our_type;
}

/**
 * gtk_icon_source_set_filename:
 * @source: a #GtkIconSource
 * @filename: image file to use
 *
 * Sets the name of an image file to use as a base image when creating
 * icon variants for #GtkIconSet. The filename must be absolute. 
 **/
void
gtk_icon_source_set_filename (GtkIconSource *source,
			      const gchar   *filename)
{
  g_return_if_fail (source != NULL);
  g_return_if_fail (filename == NULL || g_path_is_absolute (filename));

  if (source->filename == filename)
    return;
  
  if (source->filename)
    g_free (source->filename);

  source->filename = g_strdup (filename);  
}

/**
 * gtk_icon_source_set_pixbuf:
 * @source: a #GtkIconSource
 * @pixbuf: pixbuf to use as a source
 *
 * Sets a pixbuf to use as a base image when creating icon variants
 * for #GtkIconSet. If an icon source has both a filename and a pixbuf
 * set, the pixbuf will take priority.
 * 
 **/
void
gtk_icon_source_set_pixbuf (GtkIconSource *source,
                            GdkPixbuf     *pixbuf)
{
  g_return_if_fail (source != NULL);

  if (pixbuf)
    g_object_ref (pixbuf);

  if (source->pixbuf)
    g_object_unref (source->pixbuf);

  source->pixbuf = pixbuf;
}

/**
 * gtk_icon_source_get_filename:
 * @source: a #GtkIconSource
 * 
 * Retrieves the source filename, or %NULL if none is set. The
 * filename is not a copy, and should not be modified or expected to
 * persist beyond the lifetime of the icon source.
 * 
 * Return value: image filename. This string must not be modified
 * or freed.
 **/
G_CONST_RETURN gchar*
gtk_icon_source_get_filename (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, NULL);
  
  return source->filename;
}

/**
 * gtk_icon_source_get_pixbuf:
 * @source: a #GtkIconSource
 * 
 * Retrieves the source pixbuf, or %NULL if none is set.
 * The reference count on the pixbuf is not incremented.
 * 
 * Return value: source pixbuf
 **/
GdkPixbuf*
gtk_icon_source_get_pixbuf (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, NULL);
  
  return source->pixbuf;
}

/**
 * gtk_icon_source_set_direction_wildcarded:
 * @source: a #GtkIconSource
 * @setting: %TRUE to wildcard the text direction
 *
 * If the text direction is wildcarded, this source can be used
 * as the base image for an icon in any #GtkTextDirection.
 * If the text direction is not wildcarded, then the
 * text direction the icon source applies to should be set
 * with gtk_icon_source_set_direction(), and the icon source
 * will only be used with that text direction.
 *
 * #GtkIconSet prefers non-wildcarded sources (exact matches) over
 * wildcarded sources, and will use an exact match when possible.
 * 
 **/
void
gtk_icon_source_set_direction_wildcarded (GtkIconSource *source,
                                          gboolean       setting)
{
  g_return_if_fail (source != NULL);

  source->any_direction = setting != FALSE;
}

/**
 * gtk_icon_source_set_state_wildcarded:
 * @source: a #GtkIconSource
 * @setting: %TRUE to wildcard the widget state
 *
 * If the widget state is wildcarded, this source can be used as the
 * base image for an icon in any #GtkStateType.  If the widget state
 * is not wildcarded, then the state the source applies to should be
 * set with gtk_icon_source_set_state() and the icon source will
 * only be used with that specific state.
 *
 * #GtkIconSet prefers non-wildcarded sources (exact matches) over
 * wildcarded sources, and will use an exact match when possible.
 *
 * #GtkIconSet will normally transform wildcarded source images to
 * produce an appropriate icon for a given state, for example
 * lightening an image on prelight, but will not modify source images
 * that match exactly.
 **/
void
gtk_icon_source_set_state_wildcarded (GtkIconSource *source,
                                      gboolean       setting)
{
  g_return_if_fail (source != NULL);

  source->any_state = setting != FALSE;
}


/**
 * gtk_icon_source_set_size_wildcarded:
 * @source: a #GtkIconSource
 * @setting: %TRUE to wildcard the widget state
 *
 * If the icon size is wildcarded, this source can be used as the base
 * image for an icon of any size.  If the size is not wildcarded, then
 * the size the source applies to should be set with
 * gtk_icon_source_set_size() and the icon source will only be used
 * with that specific size.
 *
 * #GtkIconSet prefers non-wildcarded sources (exact matches) over
 * wildcarded sources, and will use an exact match when possible.
 *
 * #GtkIconSet will normally scale wildcarded source images to produce
 * an appropriate icon at a given size, but will not change the size
 * of source images that match exactly.
 **/
void
gtk_icon_source_set_size_wildcarded (GtkIconSource *source,
                                     gboolean       setting)
{
  g_return_if_fail (source != NULL);

  source->any_size = setting != FALSE;  
}

/**
 * gtk_icon_source_get_size_wildcarded:
 * @source: a #GtkIconSource
 * 
 * Gets the value set by gtk_icon_source_set_size_wildcarded().
 * 
 * Return value: %TRUE if this icon source is a base for any icon size variant
 **/
gboolean
gtk_icon_source_get_size_wildcarded (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, TRUE);
  
  return source->any_size;
}

/**
 * gtk_icon_source_get_state_wildcarded:
 * @source: a #GtkIconSource
 * 
 * Gets the value set by gtk_icon_source_set_state_wildcarded().
 * 
 * Return value: %TRUE if this icon source is a base for any widget state variant
 **/
gboolean
gtk_icon_source_get_state_wildcarded (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, TRUE);

  return source->any_state;
}

/**
 * gtk_icon_source_get_direction_wildcarded:
 * @source: a #GtkIconSource
 * 
 * Gets the value set by gtk_icon_source_set_direction_wildcarded().
 * 
 * Return value: %TRUE if this icon source is a base for any text direction variant
 **/
gboolean
gtk_icon_source_get_direction_wildcarded (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, TRUE);

  return source->any_direction;
}

/**
 * gtk_icon_source_set_direction:
 * @source: a #GtkIconSource
 * @direction: text direction this source applies to
 *
 * Sets the text direction this icon source is intended to be used
 * with.
 * 
 * Setting the text direction on an icon source makes no difference
 * if the text direction is wildcarded. Therefore, you should usually
 * call gtk_icon_source_set_direction_wildcarded() to un-wildcard it
 * in addition to calling this function.
 * 
 **/
void
gtk_icon_source_set_direction (GtkIconSource   *source,
                               GtkTextDirection direction)
{
  g_return_if_fail (source != NULL);

  source->direction = direction;
}

/**
 * gtk_icon_source_set_state:
 * @source: a #GtkIconSource
 * @state: widget state this source applies to
 *
 * Sets the widget state this icon source is intended to be used
 * with.
 * 
 * Setting the widget state on an icon source makes no difference
 * if the state is wildcarded. Therefore, you should usually
 * call gtk_icon_source_set_state_wildcarded() to un-wildcard it
 * in addition to calling this function.
 * 
 **/
void
gtk_icon_source_set_state (GtkIconSource *source,
                           GtkStateType   state)
{
  g_return_if_fail (source != NULL);

  source->state = state;
}

/**
 * gtk_icon_source_set_size:
 * @source: a #GtkIconSource
 * @size: icon size this source applies to
 *
 * Sets the icon size this icon source is intended to be used
 * with.
 * 
 * Setting the icon size on an icon source makes no difference
 * if the size is wildcarded. Therefore, you should usually
 * call gtk_icon_source_set_size_wildcarded() to un-wildcard it
 * in addition to calling this function.
 * 
 **/
void
gtk_icon_source_set_size (GtkIconSource *source,
                          GtkIconSize    size)
{
  g_return_if_fail (source != NULL);

  source->size = size;
}

/**
 * gtk_icon_source_get_direction:
 * @source: a #GtkIconSource
 * 
 * Obtains the text direction this icon source applies to. The return
 * value is only useful/meaningful if the text direction is <emphasis>not</emphasis> 
 * wildcarded.
 * 
 * Return value: text direction this source matches
 **/
GtkTextDirection
gtk_icon_source_get_direction (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, 0);

  return source->direction;
}

/**
 * gtk_icon_source_get_state:
 * @source: a #GtkIconSource
 * 
 * Obtains the widget state this icon source applies to. The return
 * value is only useful/meaningful if the widget state is <emphasis>not</emphasis>
 * wildcarded.
 * 
 * Return value: widget state this source matches
 **/
GtkStateType
gtk_icon_source_get_state (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, 0);

  return source->state;
}

/**
 * gtk_icon_source_get_size:
 * @source: a #GtkIconSource
 * 
 * Obtains the icon size this source applies to. The return value
 * is only useful/meaningful if the icon size is <emphasis>not</emphasis> wildcarded.
 * 
 * Return value: icon size this source matches.
 **/
GtkIconSize
gtk_icon_source_get_size (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, 0);

  return source->size;
}

/* Note that the logical maximum is 20 per GtkTextDirection, so we could
 * eventually set this to >20 to never throw anything out.
 */
#define NUM_CACHED_ICONS 8

typedef struct _CachedIcon CachedIcon;

struct _CachedIcon
{
  /* These must all match to use the cached pixbuf.
   * If any don't match, we must re-render the pixbuf.
   */
  GtkStyle *style;
  GtkTextDirection direction;
  GtkStateType state;
  GtkIconSize size;

  GdkPixbuf *pixbuf;
};

static void
ensure_cache_up_to_date (GtkIconSet *icon_set)
{
  if (icon_set->cache_serial != cache_serial)
    clear_cache (icon_set, TRUE);
}

static void
cached_icon_free (CachedIcon *icon)
{
  g_object_unref (icon->pixbuf);

  g_free (icon);
}

static GdkPixbuf *
find_in_cache (GtkIconSet      *icon_set,
               GtkStyle        *style,
               GtkTextDirection direction,
               GtkStateType     state,
               GtkIconSize      size)
{
  GSList *tmp_list;
  GSList *prev;

  ensure_cache_up_to_date (icon_set);
  
  prev = NULL;
  tmp_list = icon_set->cache;
  while (tmp_list != NULL)
    {
      CachedIcon *icon = tmp_list->data;

      if (icon->style == style &&
          icon->direction == direction &&
          icon->state == state &&
          icon->size == size)
        {
          if (prev)
            {
              /* Move this icon to the front of the list. */
              prev->next = tmp_list->next;
              tmp_list->next = icon_set->cache;
              icon_set->cache = tmp_list;
            }
          
          return icon->pixbuf;
        }
          
      prev = tmp_list;
      tmp_list = g_slist_next (tmp_list);
    }

  return NULL;
}

static void
add_to_cache (GtkIconSet      *icon_set,
              GtkStyle        *style,
              GtkTextDirection direction,
              GtkStateType     state,
              GtkIconSize      size,
              GdkPixbuf       *pixbuf)
{
  CachedIcon *icon;

  ensure_cache_up_to_date (icon_set);
  
  g_object_ref (pixbuf);

  /* We have to ref the style, since if the style was finalized
   * its address could be reused by another style, creating a
   * really weird bug
   */
  
  if (style)
    g_object_ref (style);
  

  icon = g_new (CachedIcon, 1);
  icon_set->cache = g_slist_prepend (icon_set->cache, icon);

  icon->style = style;
  icon->direction = direction;
  icon->state = state;
  icon->size = size;
  icon->pixbuf = pixbuf;

  if (icon->style)
    attach_to_style (icon_set, icon->style);
  
  if (icon_set->cache_size >= NUM_CACHED_ICONS)
    {
      /* Remove oldest item in the cache */
      
      GSList *tmp_list;
      
      tmp_list = icon_set->cache;

      /* Find next-to-last link */
      g_assert (NUM_CACHED_ICONS > 2);
      while (tmp_list->next->next)
        tmp_list = tmp_list->next;

      g_assert (tmp_list != NULL);
      g_assert (tmp_list->next != NULL);
      g_assert (tmp_list->next->next == NULL);

      /* Free the last icon */
      icon = tmp_list->next->data;

      g_slist_free (tmp_list->next);
      tmp_list->next = NULL;

      cached_icon_free (icon);
    }
}

static void
clear_cache (GtkIconSet *icon_set,
             gboolean    style_detach)
{
  GSList *tmp_list;
  GtkStyle *last_style = NULL;

  tmp_list = icon_set->cache;
  while (tmp_list != NULL)
    {
      CachedIcon *icon = tmp_list->data;

      if (style_detach)
        {
          /* simple optimization for the case where the cache
           * contains contiguous icons from the same style.
           * it's safe to call detach_from_style more than
           * once on the same style though.
           */
          if (last_style != icon->style)
            {
              detach_from_style (icon_set, icon->style);
              last_style = icon->style;
            }
        }
      
      cached_icon_free (icon);      
      
      tmp_list = g_slist_next (tmp_list);
    }

  g_slist_free (icon_set->cache);
  icon_set->cache = NULL;
  icon_set->cache_size = 0;
}

static GSList*
copy_cache (GtkIconSet *icon_set,
            GtkIconSet *copy_recipient)
{
  GSList *tmp_list;
  GSList *copy = NULL;

  ensure_cache_up_to_date (icon_set);
  
  tmp_list = icon_set->cache;
  while (tmp_list != NULL)
    {
      CachedIcon *icon = tmp_list->data;
      CachedIcon *icon_copy = g_new (CachedIcon, 1);

      *icon_copy = *icon;

      if (icon_copy->style)
        attach_to_style (copy_recipient, icon_copy->style);
        
      g_object_ref (icon_copy->pixbuf);

      icon_copy->size = icon->size;
      
      copy = g_slist_prepend (copy, icon_copy);      
      
      tmp_list = g_slist_next (tmp_list);
    }

  return g_slist_reverse (copy);
}

static void
attach_to_style (GtkIconSet *icon_set,
                 GtkStyle   *style)
{
  GHashTable *table;

  table = g_object_get_qdata (G_OBJECT (style),
                              g_quark_try_string ("gtk-style-icon-sets"));

  if (table == NULL)
    {
      table = g_hash_table_new (NULL, NULL);
      g_object_set_qdata_full (G_OBJECT (style),
                               g_quark_from_static_string ("gtk-style-icon-sets"),
                               table,
                               style_dnotify);
    }

  g_hash_table_insert (table, icon_set, icon_set);
}

static void
detach_from_style (GtkIconSet *icon_set,
                   GtkStyle   *style)
{
  GHashTable *table;

  table = g_object_get_qdata (G_OBJECT (style),
                              g_quark_try_string ("gtk-style-icon-sets"));

  if (table != NULL)
    g_hash_table_remove (table, icon_set);
}

static void
iconsets_foreach (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
  GtkIconSet *icon_set = key;

  /* We only need to remove cache entries for the given style;
   * but that complicates things because in destroy notify
   * we don't know which style got destroyed, and 95% of the
   * time all cache entries will have the same style,
   * so this is faster anyway.
   */
  
  clear_cache (icon_set, FALSE);
}

static void
style_dnotify (gpointer data)
{
  GHashTable *table = data;
  
  g_hash_table_foreach (table, iconsets_foreach, NULL);

  g_hash_table_destroy (table);
}

/* This allows the icon set to detect that its cache is out of date. */
void
_gtk_icon_set_invalidate_caches (void)
{
  ++cache_serial;
}

static void
listify_foreach (gpointer key, gpointer value, gpointer data)
{
  GSList **list = data;

  *list = g_slist_prepend (*list, key);
}

static GSList *
g_hash_table_get_keys (GHashTable *table)
{
  GSList *list = NULL;

  g_hash_table_foreach (table, listify_foreach, &list);

  return list;
}

/**
 * _gtk_icon_factory_list_ids:
 * 
 * Gets all known IDs stored in an existing icon factory.
 * The strings in the returned list aren't copied.
 * The list itself should be freed.
 * 
 * Return value: List of ids in icon factories
 **/
GSList*
_gtk_icon_factory_list_ids (void)
{
  GSList *tmp_list;
  GSList *ids;

  ids = NULL;

  ensure_default_icons ();
  
  tmp_list = all_icon_factories;
  while (tmp_list != NULL)
    {
      GSList *these_ids;
      
      GtkIconFactory *factory = GTK_ICON_FACTORY (tmp_list->data);

      these_ids = g_hash_table_get_keys (factory->icons);
      
      ids = g_slist_concat (ids, these_ids);
      
      tmp_list = g_slist_next (tmp_list);
    }

  return ids;
}
