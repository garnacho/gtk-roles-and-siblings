/* GTK - The GIMP Toolkit
 * gtktextiter.h Copyright (C) 2000 Red Hat, Inc.
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

#ifndef GTK_TEXT_ITER_H
#define GTK_TEXT_ITER_H

#include <gtk/gtktexttag.h>
#include <gtk/gtktextchild.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Iter: represents a location in the text. Becomes invalid if the
 * characters/pixmaps/widgets (indexable objects) in the text buffer
 * are changed.
 */

typedef struct _GtkTextBuffer GtkTextBuffer;

struct _GtkTextIter {
  gpointer dummy1;
  gpointer dummy2;
  gint dummy3;
  gint dummy4;
  gint dummy5;
  gint dummy6;
  gint dummy7;
  gint dummy8;
  gpointer dummy9;
  gpointer dummy10;
  gint dummy11;
  gint dummy12;
};


/* This is primarily intended for language bindings that want to avoid
   a "buffer" argument to text insertions, deletions, etc. */
GtkTextBuffer *gtk_text_iter_get_buffer (const GtkTextIter *iter);

/*
 * Life cycle
 */

GtkTextIter *gtk_text_iter_copy     (const GtkTextIter *iter);
void         gtk_text_iter_free     (GtkTextIter       *iter);

/*
 * Convert to different kinds of index
 */

gint gtk_text_iter_get_offset      (const GtkTextIter *iter);
gint gtk_text_iter_get_line        (const GtkTextIter *iter);
gint gtk_text_iter_get_line_offset (const GtkTextIter *iter);
gint gtk_text_iter_get_line_index  (const GtkTextIter *iter);

gint gtk_text_iter_get_visible_line_offset (const GtkTextIter *iter);
gint gtk_text_iter_get_visible_line_index (const GtkTextIter *iter);


/*
 * "Dereference" operators
 */
gunichar gtk_text_iter_get_char          (const GtkTextIter  *iter);

/* includes the 0xFFFC char for pixmaps/widgets, so char offsets
 * into the returned string map properly into buffer char offsets
 */
gchar   *gtk_text_iter_get_slice         (const GtkTextIter  *start,
                                          const GtkTextIter  *end);

/* includes only text, no 0xFFFC */
gchar   *gtk_text_iter_get_text          (const GtkTextIter  *start,
                                          const GtkTextIter  *end);
/* exclude invisible chars */
gchar   *gtk_text_iter_get_visible_slice (const GtkTextIter  *start,
                                          const GtkTextIter  *end);
gchar   *gtk_text_iter_get_visible_text  (const GtkTextIter  *start,
                                          const GtkTextIter  *end);

GdkPixbuf* gtk_text_iter_get_pixbuf (const GtkTextIter *iter);
GSList  *  gtk_text_iter_get_marks  (const GtkTextIter *iter);

GtkTextChildAnchor* gtk_text_iter_get_child_anchor (const GtkTextIter *iter);

/* Return list of tags toggled at this point (toggled_on determines
 * whether the list is of on-toggles or off-toggles)
 */
GSList  *gtk_text_iter_get_toggled_tags  (const GtkTextIter  *iter,
                                          gboolean            toggled_on);

gboolean gtk_text_iter_begins_tag        (const GtkTextIter  *iter,
                                          GtkTextTag         *tag);

gboolean gtk_text_iter_ends_tag          (const GtkTextIter  *iter,
                                          GtkTextTag         *tag);

gboolean gtk_text_iter_toggles_tag       (const GtkTextIter  *iter,
                                          GtkTextTag         *tag);

gboolean gtk_text_iter_has_tag           (const GtkTextIter   *iter,
                                          GtkTextTag          *tag);
GSList  *gtk_text_iter_get_tags          (const GtkTextIter   *iter);

gboolean gtk_text_iter_editable          (const GtkTextIter   *iter,
                                          gboolean             default_setting);
gboolean gtk_text_iter_can_insert        (const GtkTextIter   *iter,
                                          gboolean             default_editability);

gboolean gtk_text_iter_starts_word        (const GtkTextIter   *iter);
gboolean gtk_text_iter_ends_word          (const GtkTextIter   *iter);
gboolean gtk_text_iter_inside_word        (const GtkTextIter   *iter);
gboolean gtk_text_iter_starts_sentence    (const GtkTextIter   *iter);
gboolean gtk_text_iter_ends_sentence      (const GtkTextIter   *iter);
gboolean gtk_text_iter_inside_sentence    (const GtkTextIter   *iter);
gboolean gtk_text_iter_starts_line        (const GtkTextIter   *iter);
gboolean gtk_text_iter_ends_line          (const GtkTextIter   *iter);
gboolean gtk_text_iter_is_cursor_position (const GtkTextIter   *iter);

gint     gtk_text_iter_get_chars_in_line (const GtkTextIter   *iter);
gint     gtk_text_iter_get_bytes_in_line (const GtkTextIter   *iter);

gboolean       gtk_text_iter_get_attributes (const GtkTextIter *iter,
					     GtkTextAttributes *values);
PangoLanguage* gtk_text_iter_get_language   (const GtkTextIter *iter);
gboolean       gtk_text_iter_is_end         (const GtkTextIter *iter);
gboolean       gtk_text_iter_is_start       (const GtkTextIter *iter);

/*
 * Moving around the buffer
 */

gboolean gtk_text_iter_forward_char         (GtkTextIter *iter);
gboolean gtk_text_iter_backward_char        (GtkTextIter *iter);
gboolean gtk_text_iter_forward_chars        (GtkTextIter *iter,
                                             gint         count);
gboolean gtk_text_iter_backward_chars       (GtkTextIter *iter,
                                             gint         count);
gboolean gtk_text_iter_forward_line         (GtkTextIter *iter);
gboolean gtk_text_iter_backward_line        (GtkTextIter *iter);
gboolean gtk_text_iter_forward_lines        (GtkTextIter *iter,
                                             gint         count);
gboolean gtk_text_iter_backward_lines       (GtkTextIter *iter,
                                             gint         count);
gboolean gtk_text_iter_forward_word_end     (GtkTextIter *iter);
gboolean gtk_text_iter_backward_word_start  (GtkTextIter *iter);
gboolean gtk_text_iter_forward_word_ends    (GtkTextIter *iter,
                                             gint         count);
gboolean gtk_text_iter_backward_word_starts (GtkTextIter *iter,
                                             gint         count);

gboolean gtk_text_iter_forward_sentence_end     (GtkTextIter *iter);
gboolean gtk_text_iter_backward_sentence_start  (GtkTextIter *iter);
gboolean gtk_text_iter_forward_sentence_ends    (GtkTextIter *iter,
                                                 gint         count);
gboolean gtk_text_iter_backward_sentence_starts (GtkTextIter *iter,
                                                 gint         count);
/* cursor positions are almost equivalent to chars, but not quite;
 * in some languages, you can't put the cursor between certain
 * chars. Also, you can't put the cursor between \r\n at the end
 * of a line.
 */
gboolean gtk_text_iter_forward_cursor_position   (GtkTextIter *iter);
gboolean gtk_text_iter_backward_cursor_position  (GtkTextIter *iter);
gboolean gtk_text_iter_forward_cursor_positions  (GtkTextIter *iter,
                                                  gint         count);
gboolean gtk_text_iter_backward_cursor_positions (GtkTextIter *iter,
                                                  gint         count);


void     gtk_text_iter_set_offset         (GtkTextIter *iter,
                                           gint         char_offset);
void     gtk_text_iter_set_line           (GtkTextIter *iter,
                                           gint         line_number);
void     gtk_text_iter_set_line_offset    (GtkTextIter *iter,
                                           gint         char_on_line);
void     gtk_text_iter_set_line_index     (GtkTextIter *iter,
                                           gint         byte_on_line);
void     gtk_text_iter_forward_to_end     (GtkTextIter *iter);
gboolean gtk_text_iter_forward_to_line_end (GtkTextIter *iter);

void     gtk_text_iter_set_visible_line_offset (GtkTextIter *iter,
                                                gint         char_on_line);
void     gtk_text_iter_set_visible_line_index  (GtkTextIter *iter,
                                                gint         byte_on_line);

/* returns TRUE if a toggle was found; NULL for the tag pointer
 * means "any tag toggle", otherwise the next toggle of the
 * specified tag is located.
 */
gboolean gtk_text_iter_forward_to_tag_toggle (GtkTextIter *iter,
                                              GtkTextTag  *tag);

gboolean gtk_text_iter_backward_to_tag_toggle (GtkTextIter *iter,
                                               GtkTextTag  *tag);

typedef gboolean (* GtkTextCharPredicate) (gunichar ch, gpointer user_data);

gboolean gtk_text_iter_forward_find_char  (GtkTextIter          *iter,
                                           GtkTextCharPredicate  pred,
                                           gpointer              user_data,
                                           const GtkTextIter    *limit);
gboolean gtk_text_iter_backward_find_char (GtkTextIter          *iter,
                                           GtkTextCharPredicate  pred,
                                           gpointer              user_data,
                                           const GtkTextIter    *limit);

gboolean gtk_text_iter_forward_search  (const GtkTextIter *iter,
                                        const gchar       *str,
                                        gboolean           visible_only,
                                        gboolean           slice,
                                        GtkTextIter       *match_start,
                                        GtkTextIter       *match_end,
                                        const GtkTextIter *limit);

gboolean gtk_text_iter_backward_search (const GtkTextIter *iter,
                                        const gchar       *str,
                                        gboolean           visible_only,
                                        gboolean           slice,
                                        GtkTextIter       *match_start,
                                        GtkTextIter       *match_end,
                                        const GtkTextIter *limit);


/*
 * Comparisons
 */
gboolean gtk_text_iter_equal           (const GtkTextIter *lhs,
                                        const GtkTextIter *rhs);
gint     gtk_text_iter_compare         (const GtkTextIter *lhs,
                                        const GtkTextIter *rhs);
gboolean gtk_text_iter_in_range        (const GtkTextIter *iter,
                                        const GtkTextIter *start,
                                        const GtkTextIter *end);

/* Put these two in ascending order */
void     gtk_text_iter_order           (GtkTextIter *first,
                                        GtkTextIter *second);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


