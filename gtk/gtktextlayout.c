/* GTK - The GIMP Toolkit
 * gtktextlayout.c - calculate the layout of the text
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000 Red Hat, Inc.
 * Tk->Gtk port by Havoc Pennington
 * Pango support by Owen Taylor
 *
 * This file can be used under your choice of two licenses, the LGPL
 * and the original Tk license.
 *
 * LGPL:
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Original Tk license:
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 *
 */
/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "gtksignal.h"
#include "gtktextlayout.h"
#include "gtktextbtree.h"
#include "gtktextiterprivate.h"

#include <stdlib.h>
#include <string.h>

static GtkTextLineData    *gtk_text_line_data_new                 (GtkTextLayout     *layout,
                                                                   GtkTextLine       *line);

static GtkTextLineData *gtk_text_layout_real_wrap (GtkTextLayout *layout,
                                                   GtkTextLine *line,
                                                   /* may be NULL */
                                                   GtkTextLineData *line_data);

static void gtk_text_layout_invalidated     (GtkTextLayout     *layout);

static void gtk_text_layout_real_invalidate     (GtkTextLayout     *layout,
                                                 const GtkTextIter *start,
                                                 const GtkTextIter *end);
static void gtk_text_layout_invalidate_cache    (GtkTextLayout     *layout,
                                                 GtkTextLine       *line);
static void gtk_text_layout_real_free_line_data (GtkTextLayout     *layout,
                                                 GtkTextLine       *line,
                                                 GtkTextLineData   *line_data);

static void gtk_text_layout_invalidate_all (GtkTextLayout *layout);

static PangoAttribute *gtk_text_attr_appearance_new (const GtkTextAppearance *appearance);

enum {
  INVALIDATED,
  CHANGED,
  ALLOCATE_CHILD,
  LAST_SIGNAL
};

enum {
  ARG_0,
  LAST_ARG
};

static void gtk_text_layout_init       (GtkTextLayout      *text_layout);
static void gtk_text_layout_class_init (GtkTextLayoutClass *klass);
static void gtk_text_layout_finalize   (GObject            *object);


static GtkObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

PangoAttrType gtk_text_attr_appearance_type = 0;

GType
gtk_text_layout_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    {
      static const GTypeInfo our_info =
      {
        sizeof (GtkTextLayoutClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_text_layout_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkTextLayout),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_text_layout_init
      };

      our_type = g_type_register_static (G_TYPE_OBJECT,
                                         "GtkTextLayout",
                                         &our_info,
                                         0);
    }

  return our_type;
}

static void
gtk_text_layout_class_init (GtkTextLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = gtk_text_layout_finalize;

  klass->wrap = gtk_text_layout_real_wrap;
  klass->invalidate = gtk_text_layout_real_invalidate;
  klass->free_line_data = gtk_text_layout_real_free_line_data;

  signals[INVALIDATED] =
    g_signal_newc ("invalidated",
                   G_TYPE_FROM_CLASS (object_class),
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET (GtkTextLayoutClass, invalidated),
                   NULL,
                   gtk_marshal_VOID__VOID,
                   GTK_TYPE_NONE,
                   0);

  signals[CHANGED] =
    g_signal_newc ("changed",
                   G_TYPE_FROM_CLASS (object_class),
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET (GtkTextLayoutClass, changed),
                   NULL,
                   gtk_marshal_VOID__INT_INT_INT,
                   GTK_TYPE_NONE,
                   3,
                   GTK_TYPE_INT,
                   GTK_TYPE_INT,
                   GTK_TYPE_INT);

  signals[ALLOCATE_CHILD] =
    g_signal_newc ("allocate_child",
                   G_TYPE_FROM_CLASS (object_class),
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET (GtkTextLayoutClass, allocate_child),
                   NULL,
                   gtk_marshal_VOID__OBJECT_INT_INT,
                   GTK_TYPE_NONE,
                   3,
                   GTK_TYPE_OBJECT,
                   GTK_TYPE_INT,
                   GTK_TYPE_INT);
}

void
gtk_text_layout_init (GtkTextLayout *text_layout)
{
  text_layout->cursor_visible = TRUE;
}

GtkTextLayout*
gtk_text_layout_new (void)
{
  return GTK_TEXT_LAYOUT (g_object_new (gtk_text_layout_get_type (), NULL));
}

static void
free_style_cache (GtkTextLayout *text_layout)
{
  if (text_layout->one_style_cache)
    {
      gtk_text_attributes_unref (text_layout->one_style_cache);
      text_layout->one_style_cache = NULL;
    }
}

static void
gtk_text_layout_finalize (GObject *object)
{
  GtkTextLayout *layout;

  layout = GTK_TEXT_LAYOUT (object);

  gtk_text_layout_set_buffer (layout, NULL);

  if (layout->default_style)
    gtk_text_attributes_unref (layout->default_style);
  layout->default_style = NULL;

  if (layout->ltr_context)
    {
      g_object_unref (G_OBJECT (layout->ltr_context));
      layout->ltr_context = NULL;
    }
  if (layout->rtl_context)
    {
      g_object_unref (G_OBJECT (layout->rtl_context));
      layout->rtl_context = NULL;
    }
  
  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

void
gtk_text_layout_set_buffer (GtkTextLayout *layout,
                            GtkTextBuffer *buffer)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (buffer == NULL || GTK_IS_TEXT_BUFFER (buffer));

  if (layout->buffer == buffer)
    return;

  free_style_cache (layout);

  if (layout->buffer)
    {
      _gtk_text_btree_remove_view (_gtk_text_buffer_get_btree (layout->buffer),
                                  layout);

      g_object_unref (G_OBJECT (layout->buffer));
      layout->buffer = NULL;
    }

  if (buffer)
    {
      layout->buffer = buffer;

      g_object_ref (G_OBJECT (buffer));

      _gtk_text_btree_add_view (_gtk_text_buffer_get_btree (buffer), layout);
    }
}

void
gtk_text_layout_default_style_changed (GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  gtk_text_layout_invalidate_all (layout);
}

void
gtk_text_layout_set_default_style (GtkTextLayout *layout,
                                   GtkTextAttributes *values)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (values != NULL);

  if (values == layout->default_style)
    return;

  gtk_text_attributes_ref (values);

  if (layout->default_style)
    gtk_text_attributes_unref (layout->default_style);

  layout->default_style = values;

  gtk_text_layout_default_style_changed (layout);
}

void
gtk_text_layout_set_contexts (GtkTextLayout *layout,
                              PangoContext  *ltr_context,
                              PangoContext  *rtl_context)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  if (layout->ltr_context)
    g_object_unref (G_OBJECT (ltr_context));

  layout->ltr_context = ltr_context;
  g_object_ref (G_OBJECT (ltr_context));

  if (layout->rtl_context)
    g_object_unref (G_OBJECT (rtl_context));

  layout->rtl_context = rtl_context;
  g_object_ref (G_OBJECT (rtl_context));

  gtk_text_layout_invalidate_all (layout);
}

void
gtk_text_layout_set_screen_width (GtkTextLayout *layout, gint width)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (width >= 0);
  g_return_if_fail (layout->wrap_loop_count == 0);

  if (layout->screen_width == width)
    return;

  layout->screen_width = width;

  gtk_text_layout_invalidate_all (layout);
}

/**
 * gtk_text_layout_set_cursor_visible:
 * @layout: a #GtkTextLayout
 * @cursor_visible: If %FALSE, then the insertion cursor will not
 *   be shown, even if the text is editable.
 *
 * Sets whether the insertion cursor should be shown. Generally,
 * widgets using #GtkTextLayout will hide the cursor when the
 * widget does not have the input focus.
 **/
void
gtk_text_layout_set_cursor_visible (GtkTextLayout *layout,
                                    gboolean       cursor_visible)
{
  cursor_visible = (cursor_visible != FALSE);

  if (layout->cursor_visible != cursor_visible)
    {
      GtkTextIter iter;
      gint y, height;

      layout->cursor_visible = cursor_visible;

      /* Now queue a redraw on the paragraph containing the cursor
       */
      gtk_text_buffer_get_iter_at_mark (layout->buffer, &iter,
                                        gtk_text_buffer_get_mark (layout->buffer, "insert"));

      gtk_text_layout_get_line_yrange (layout, &iter, &y, &height);
      gtk_text_layout_changed (layout, y, height, height);

      gtk_text_layout_invalidate_cache (layout, _gtk_text_iter_get_text_line (&iter));
    }
}

/**
 * gtk_text_layout_get_cursor_visible:
 * @layout: a #GtkTextLayout
 *
 * Returns whether the insertion cursor will be shown.
 *
 * Return value: if %FALSE, the insertion cursor will not be
    shown, even if the text is editable.
 **/
gboolean
gtk_text_layout_get_cursor_visible (GtkTextLayout *layout)
{
  return layout->cursor_visible;
}

/**
 * gtk_text_layout_set_preedit_string:
 * @layout: a #PangoLayout
 * @preedit_string: a string to display at the insertion point
 * @preedit_attrs: a #PangoAttrList of attributes that apply to @preedit_string
 * @cursor_pos: position of cursor within preedit string in chars
 * 
 * Set the preedit string and attributes. The preedit string is a
 * string showing text that is currently being edited and not
 * yet committed into the buffer.
 **/
void
gtk_text_layout_set_preedit_string (GtkTextLayout *layout,
				    const gchar   *preedit_string,
				    PangoAttrList *preedit_attrs,
				    gint           cursor_pos)
{
  GtkTextIter iter;
  GtkTextLine *line;
  GtkTextLineData *line_data;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (preedit_attrs != NULL || preedit_string == NULL);

  if (layout->preedit_string)
    g_free (layout->preedit_string);

  if (layout->preedit_attrs)
    pango_attr_list_unref (layout->preedit_attrs);

  if (preedit_string)
    {
      layout->preedit_string = g_strdup (preedit_string);
      layout->preedit_len = strlen (layout->preedit_string);
      pango_attr_list_ref (preedit_attrs);
      layout->preedit_attrs = preedit_attrs;

      cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (layout->preedit_string, -1));
      layout->preedit_cursor = g_utf8_offset_to_pointer (layout->preedit_string, cursor_pos) - layout->preedit_string;
    }
  else
    {
      layout->preedit_string = NULL;
      layout->preedit_len = 0;
      layout->preedit_attrs = NULL;
      layout->preedit_cursor = 0;
    }

  /* Now invalidate the paragraph containing the cursor
   */
  gtk_text_buffer_get_iter_at_mark (layout->buffer, &iter,
				    gtk_text_buffer_get_mark (layout->buffer, "insert"));
  
  line = _gtk_text_iter_get_text_line (&iter);
  line_data = _gtk_text_line_get_data (line, layout);
  if (line_data)
    {
      gtk_text_layout_invalidate_cache (layout, line);
      _gtk_text_line_invalidate_wrap (line, line_data);
      gtk_text_layout_invalidated (layout);
    }
}

void
gtk_text_layout_get_size (GtkTextLayout *layout,
                          gint *width,
                          gint *height)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  if (width)
    *width = layout->width;

  if (height)
    *height = layout->height;
}

static void
gtk_text_layout_invalidated (GtkTextLayout *layout)
{
  g_signal_emit (G_OBJECT (layout), signals[INVALIDATED], 0);
}

void
gtk_text_layout_changed (GtkTextLayout *layout,
                         gint           y,
                         gint           old_height,
                         gint           new_height)
{
  g_signal_emit (G_OBJECT (layout), signals[CHANGED], 0,
                 y, old_height, new_height);
}

void
gtk_text_layout_free_line_data (GtkTextLayout     *layout,
                                GtkTextLine       *line,
                                GtkTextLineData   *line_data)
{
  (* GTK_TEXT_LAYOUT_GET_CLASS (layout)->free_line_data)
    (layout, line, line_data);
}

void
gtk_text_layout_invalidate (GtkTextLayout *layout,
                            const GtkTextIter *start_index,
                            const GtkTextIter *end_index)
{
  (* GTK_TEXT_LAYOUT_GET_CLASS (layout)->invalidate)
    (layout, start_index, end_index);
}

GtkTextLineData*
gtk_text_layout_wrap (GtkTextLayout *layout,
                      GtkTextLine  *line,
                      /* may be NULL */
                      GtkTextLineData *line_data)
{
  return (* GTK_TEXT_LAYOUT_GET_CLASS (layout)->wrap) (layout, line, line_data);
}

GSList*
gtk_text_layout_get_lines (GtkTextLayout *layout,
                           /* [top_y, bottom_y) */
                           gint top_y,
                           gint bottom_y,
                           gint *first_line_y)
{
  GtkTextLine *first_btree_line;
  GtkTextLine *last_btree_line;
  GtkTextLine *line;
  GSList *retval;

  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), NULL);
  g_return_val_if_fail (bottom_y > top_y, NULL);

  retval = NULL;

  first_btree_line =
    _gtk_text_btree_find_line_by_y (_gtk_text_buffer_get_btree (layout->buffer),
                                   layout, top_y, first_line_y);
  if (first_btree_line == NULL)
    {
      g_assert (top_y > 0);
      /* off the bottom */
      return NULL;
    }

  /* -1 since bottom_y is one past */
  last_btree_line =
    _gtk_text_btree_find_line_by_y (_gtk_text_buffer_get_btree (layout->buffer),
                                   layout, bottom_y - 1, NULL);

  if (!last_btree_line)
    last_btree_line =
      _gtk_text_btree_get_line (_gtk_text_buffer_get_btree (layout->buffer),
                               _gtk_text_btree_line_count (_gtk_text_buffer_get_btree (layout->buffer)) - 1,
                               NULL);

  {
    GtkTextLineData *ld = _gtk_text_line_get_data (last_btree_line, layout);
    if (ld->height == 0)
      G_BREAKPOINT ();
  }

  g_assert (last_btree_line != NULL);

  line = first_btree_line;
  while (TRUE)
    {
      retval = g_slist_prepend (retval, line);

      if (line == last_btree_line)
        break;

      line = _gtk_text_line_next (line);
    }

  retval = g_slist_reverse (retval);

  return retval;
}

static void
invalidate_cached_style (GtkTextLayout *layout)
{
  free_style_cache (layout);
}

/* These should be called around a loop which wraps a CONTIGUOUS bunch
 * of display lines. If the lines aren't contiguous you can't call
 * these.
 */
void
gtk_text_layout_wrap_loop_start (GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->one_style_cache == NULL);

  layout->wrap_loop_count += 1;
}

void
gtk_text_layout_wrap_loop_end (GtkTextLayout *layout)
{
  g_return_if_fail (layout->wrap_loop_count > 0);

  layout->wrap_loop_count -= 1;

  if (layout->wrap_loop_count == 0)
    {
      /* We cache a some stuff if we're iterating over some lines wrapping
       * them. This cleans it up.
       */
      /* Nuke our cached style */
      invalidate_cached_style (layout);
      g_assert (layout->one_style_cache == NULL);
    }
}

static void
gtk_text_layout_invalidate_all (GtkTextLayout *layout)
{
  GtkTextIter start;
  GtkTextIter end;

  if (layout->buffer == NULL)
    return;

  gtk_text_buffer_get_bounds (layout->buffer, &start, &end);

  gtk_text_layout_invalidate (layout, &start, &end);
}

static void
gtk_text_layout_invalidate_cache (GtkTextLayout *layout,
                                  GtkTextLine   *line)
{
  if (layout->one_display_cache && line == layout->one_display_cache->line)
    {
      GtkTextLineDisplay *tmp_display = layout->one_display_cache;
      layout->one_display_cache = NULL;
      gtk_text_layout_free_line_display (layout, tmp_display);
    }
}

static void
gtk_text_layout_real_invalidate (GtkTextLayout *layout,
                                 const GtkTextIter *start,
                                 const GtkTextIter *end)
{
  GtkTextLine *line;
  GtkTextLine *last_line;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->wrap_loop_count == 0);

#if 0
  gtk_text_view_index_spew (start_index, "invalidate start");
  gtk_text_view_index_spew (end_index, "invalidate end");
#endif

  last_line = _gtk_text_iter_get_text_line (end);
  line = _gtk_text_iter_get_text_line (start);

  while (TRUE)
    {
      GtkTextLineData *line_data = _gtk_text_line_get_data (line, layout);

      if (line_data &&
          (line != last_line || !gtk_text_iter_starts_line (end)))
        {
          gtk_text_layout_invalidate_cache (layout, line);
          _gtk_text_line_invalidate_wrap (line, line_data);
        }

      if (line == last_line)
        break;

      line = _gtk_text_line_next (line);
    }

  gtk_text_layout_invalidated (layout);
}

static void
gtk_text_layout_real_free_line_data (GtkTextLayout     *layout,
                                     GtkTextLine       *line,
                                     GtkTextLineData   *line_data)
{
  if (layout->one_display_cache && line == layout->one_display_cache->line)
    {
      GtkTextLineDisplay *tmp_display = layout->one_display_cache;
      layout->one_display_cache = NULL;
      gtk_text_layout_free_line_display (layout, tmp_display);
    }

  g_free (line_data);
}



/**
 * gtk_text_layout_is_valid:
 * @layout: a #GtkTextLayout
 *
 * Check if there are any invalid regions in a #GtkTextLayout's buffer
 *
 * Return value: #TRUE if any invalid regions were found
 **/
gboolean
gtk_text_layout_is_valid (GtkTextLayout *layout)
{
  g_return_val_if_fail (layout != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);

  return _gtk_text_btree_is_valid (_gtk_text_buffer_get_btree (layout->buffer),
                                  layout);
}

static void
update_layout_size (GtkTextLayout *layout)
{
  _gtk_text_btree_get_view_size (_gtk_text_buffer_get_btree (layout->buffer),
				layout,
				&layout->width, &layout->height);
}

/**
 * gtk_text_layout_validate_yrange:
 * @layout: a #GtkTextLayout
 * @anchor: iter pointing into a line that will be used as the
 *          coordinate origin
 * @y0: offset from the top of the line pointed to by @anchor at
 *      which to begin validation. (The offset here is in pixels
 *      after validation.)
 * @y1: offset from the top of the line pointed to by @anchor at
 *      which to end validation. (The offset here is in pixels
 *      after validation.)
 *
 * Ensure that a region of a #GtkTextLayout is valid. The ::changed
 * signal will be emitted if any lines are validated.
 **/
void
gtk_text_layout_validate_yrange (GtkTextLayout *layout,
                                 GtkTextIter   *anchor,
                                 gint           y0,
                                 gint           y1)
{
  GtkTextLine *line;
  GtkTextLine *first_line = NULL;
  GtkTextLine *last_line = NULL;
  gint seen;
  gint delta_height = 0;
  gint first_line_y = 0;        /* Quiet GCC */
  gint last_line_y = 0;         /* Quiet GCC */

  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  if (y0 > 0)
    y0 = 0;
  if (y1 < 0)
    y1 = 0;
  
  /* Validate backwards from the anchor line to y0
   */
  line = _gtk_text_iter_get_text_line (anchor);
  seen = 0;
  while (line && seen < -y0)
    {
      GtkTextLineData *line_data = _gtk_text_line_get_data (line, layout);
      if (!line_data || !line_data->valid)
        {
          gint old_height = line_data ? line_data->height : 0;

          _gtk_text_btree_validate_line (_gtk_text_buffer_get_btree (layout->buffer),
                                         line, layout);
          line_data = _gtk_text_line_get_data (line, layout);

          delta_height += line_data->height - old_height;
          
          first_line = line;
          first_line_y = -seen;
          if (!last_line)
            {
              last_line = line;
              last_line_y = -seen + line_data->height;
            }
        }

      seen += line_data->height;
      line = _gtk_text_line_previous (line);
    }

  /* Validate forwards to y1 */
  line = _gtk_text_iter_get_text_line (anchor);
  seen = 0;
  while (line && seen < y1)
    {
      GtkTextLineData *line_data = _gtk_text_line_get_data (line, layout);
      if (!line_data || !line_data->valid)
        {
          gint old_height = line_data ? line_data->height : 0;

          _gtk_text_btree_validate_line (_gtk_text_buffer_get_btree (layout->buffer),
                                         line, layout);
          line_data = _gtk_text_line_get_data (line, layout);

          delta_height += line_data->height - old_height;
          
          if (!first_line)
            {
              first_line = line;
              first_line_y = seen;
            }
          last_line = line;
          last_line_y = seen + line_data->height;
        }

      seen += line_data->height;
      line = _gtk_text_line_next (line);
    }

  /* If we found and validated any invalid lines, update size and
   * emit the changed signal
   */
  if (first_line)
    {
      gint line_top;

      update_layout_size (layout);

      line_top = _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer),
                                                first_line, layout);

      gtk_text_layout_changed (layout,
                               line_top,
                               last_line_y - first_line_y - delta_height,
                               last_line_y - first_line_y);
    }
}

/**
 * gtk_text_layout_validate:
 * @tree: a #GtkTextLayout
 * @max_pixels: the maximum number of pixels to validate. (No more
 *              than one paragraph beyond this limit will be validated)
 *
 * Validate regions of a #GtkTextLayout. The ::changed signal will
 * be emitted for each region validated.
 **/
void
gtk_text_layout_validate (GtkTextLayout *layout,
                          gint           max_pixels)
{
  gint y, old_height, new_height;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  while (max_pixels > 0 &&
         _gtk_text_btree_validate (_gtk_text_buffer_get_btree (layout->buffer),
                                   layout,  max_pixels,
                                   &y, &old_height, &new_height))
    {
      max_pixels -= new_height;

      update_layout_size (layout);
      gtk_text_layout_changed (layout, y, old_height, new_height);
    }
}

static GtkTextLineData*
gtk_text_layout_real_wrap (GtkTextLayout   *layout,
                           GtkTextLine     *line,
                           /* may be NULL */
                           GtkTextLineData *line_data)
{
  GtkTextLineDisplay *display;

  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), NULL);

  if (line_data == NULL)
    {
      line_data = gtk_text_line_data_new (layout, line);
      _gtk_text_line_add_data (line, line_data);
    }

  display = gtk_text_layout_get_line_display (layout, line, TRUE);
  line_data->width = display->width;
  line_data->height = display->height;
  line_data->valid = TRUE;
  gtk_text_layout_free_line_display (layout, display);

  return line_data;
}

/*
 * Layout utility functions
 */

/* If you get the style with get_style () you need to call
   release_style () to free it. */
static GtkTextAttributes*
get_style (GtkTextLayout *layout,
           const GtkTextIter *iter)
{
  GtkTextTag** tags;
  gint tag_count = 0;
  GtkTextAttributes *style;

  /* If we have the one-style cache, then it means
     that we haven't seen a toggle since we filled in the
     one-style cache.
  */
  if (layout->one_style_cache != NULL)
    {
      gtk_text_attributes_ref (layout->one_style_cache);
      return layout->one_style_cache;
    }

  g_assert (layout->one_style_cache == NULL);

  /* Get the tags at this spot */
  tags = _gtk_text_btree_get_tags (iter, &tag_count);

  /* No tags, use default style */
  if (tags == NULL || tag_count == 0)
    {
      /* One ref for the return value, one ref for the
         layout->one_style_cache reference */
      gtk_text_attributes_ref (layout->default_style);
      gtk_text_attributes_ref (layout->default_style);
      layout->one_style_cache = layout->default_style;

      if (tags)
        g_free (tags);

      return layout->default_style;
    }

  /* Sort tags in ascending order of priority */
  _gtk_text_tag_array_sort (tags, tag_count);

  style = gtk_text_attributes_new ();

  gtk_text_attributes_copy_values (layout->default_style,
                                   style);

  _gtk_text_attributes_fill_from_tags (style,
                                       tags,
                                       tag_count);

  g_free (tags);

  g_assert (style->refcount == 1);

  /* Leave this style as the last one seen */
  g_assert (layout->one_style_cache == NULL);
  gtk_text_attributes_ref (style); /* ref held by layout->one_style_cache */
  layout->one_style_cache = style;

  /* Returning yet another refcount */
  return style;
}

static void
release_style (GtkTextLayout *layout,
               GtkTextAttributes *style)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->refcount > 0);

  gtk_text_attributes_unref (style);
}

/*
 * Lines
 */

/* This function tries to optimize the case where a line
   is completely invisible */
static gboolean
totally_invisible_line (GtkTextLayout *layout,
                        GtkTextLine   *line,
                        GtkTextIter   *iter)
{
  GtkTextLineSegment *seg;
  int bytes = 0;

  /* If we have a cached style, then we know it does actually apply
     and we can just see if it is invisible. */
  if (layout->one_style_cache &&
      !layout->one_style_cache->invisible)
    return FALSE;
  /* Without the cache, we check if the first char is visible, if so
     we are partially visible.  Note that we have to check this since
     we don't know the current invisible/noninvisible toggle state; this
     function can use the whole btree to get it right. */
  else
    {
      _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                       iter, line, 0);

      if (!_gtk_text_btree_char_is_invisible (iter))
        return FALSE;
    }

  bytes = 0;
  seg = line->segments;

  while (seg != NULL)
    {
      if (seg->byte_count > 0)
        bytes += seg->byte_count;

      /* Note that these two tests can cause us to bail out
         when we shouldn't, because a higher-priority tag
         may override these settings. However the important
         thing is to only invisible really-invisible lines, rather
         than to invisible all really-invisible lines. */

      else if (seg->type == &gtk_text_toggle_on_type)
        {
          invalidate_cached_style (layout);

          /* Bail out if an elision-unsetting tag begins */
          if (seg->body.toggle.info->tag->invisible_set &&
              !seg->body.toggle.info->tag->values->invisible)
            break;
        }
      else if (seg->type == &gtk_text_toggle_off_type)
        {
          invalidate_cached_style (layout);

          /* Bail out if an elision-setting tag ends */
          if (seg->body.toggle.info->tag->invisible_set &&
              seg->body.toggle.info->tag->values->invisible)
            break;
        }

      seg = seg->next;
    }

  if (seg != NULL)       /* didn't reach line end */
    return FALSE;

  return TRUE;
}

static void
set_para_values (GtkTextLayout      *layout,
                 GtkTextAttributes *style,
                 GtkTextLineDisplay *display,
                 gdouble            *align)
{
  PangoAlignment pango_align = PANGO_ALIGN_LEFT;
  int layout_width;

  display->direction = style->direction;

  if (display->direction == GTK_TEXT_DIR_LTR)
    display->layout = pango_layout_new (layout->ltr_context);
  else
    display->layout = pango_layout_new (layout->rtl_context);

  switch (style->justify)
    {
    case GTK_JUSTIFY_LEFT:
      pango_align = (style->direction == GTK_TEXT_DIR_LTR) ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT;
      break;
    case GTK_JUSTIFY_RIGHT:
      pango_align = (style->direction == GTK_TEXT_DIR_LTR) ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;
      break;
    case GTK_JUSTIFY_CENTER:
      pango_align = PANGO_ALIGN_CENTER;
      break;
    case GTK_JUSTIFY_FILL:
      g_warning ("FIXME we don't support GTK_JUSTIFY_FILL yet");
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  switch (pango_align)
    {
    case PANGO_ALIGN_LEFT:
      *align = 0.0;
      break;
    case PANGO_ALIGN_RIGHT:
      *align = 1.0;
      break;
    case PANGO_ALIGN_CENTER:
      *align = 0.5;
      break;
    }

  pango_layout_set_alignment (display->layout, pango_align);
  pango_layout_set_spacing (display->layout,
                            style->pixels_inside_wrap * PANGO_SCALE);

  if (style->tabs)
    pango_layout_set_tabs (display->layout, style->tabs);

  display->top_margin = style->pixels_above_lines;
  display->height = style->pixels_above_lines + style->pixels_below_lines;
  display->bottom_margin = style->pixels_below_lines;
  display->left_margin = style->left_margin;
  display->right_margin = style->right_margin;
  
  display->x_offset = display->left_margin;

  pango_layout_set_indent (display->layout,
                           style->indent * PANGO_SCALE);

  switch (style->wrap_mode)
    {
    case GTK_WRAPMODE_CHAR:
      /* FIXME: Handle this; for now, fall-through */
    case GTK_WRAPMODE_WORD:
      layout_width = layout->screen_width - display->left_margin - display->right_margin;
      pango_layout_set_width (display->layout, layout_width * PANGO_SCALE);
      break;
    case GTK_WRAPMODE_NONE:
      break;
    }
  
  display->total_width = MAX (layout->screen_width, layout->width) - display->left_margin - display->right_margin;
}

static PangoAttribute *
gtk_text_attr_appearance_copy (const PangoAttribute *attr)
{
  const GtkTextAttrAppearance *appearance_attr = (const GtkTextAttrAppearance *)attr;

  return gtk_text_attr_appearance_new (&appearance_attr->appearance);
}

static void
gtk_text_attr_appearance_destroy (PangoAttribute *attr)
{
  GtkTextAppearance *appearance = &((GtkTextAttrAppearance *)attr)->appearance;

  if (appearance->bg_stipple)
    gdk_drawable_unref (appearance->bg_stipple);
  if (appearance->fg_stipple)
    gdk_drawable_unref (appearance->fg_stipple);

  g_free (attr);
}

static gboolean
gtk_text_attr_appearance_compare (const PangoAttribute *attr1,
                                  const PangoAttribute *attr2)
{
  const GtkTextAppearance *appearance1 = &((const GtkTextAttrAppearance *)attr1)->appearance;
  const GtkTextAppearance *appearance2 = &((const GtkTextAttrAppearance *)attr2)->appearance;

  return (gdk_color_equal (&appearance1->fg_color, &appearance2->fg_color) &&
          gdk_color_equal (&appearance1->bg_color, &appearance2->bg_color) &&
          appearance1->fg_stipple ==  appearance2->fg_stipple &&
          appearance1->bg_stipple ==  appearance2->bg_stipple &&
          appearance1->underline == appearance2->underline &&
          appearance1->strikethrough == appearance2->strikethrough &&
          appearance1->draw_bg == appearance2->draw_bg);

}

/**
 * gtk_text_attr_appearance_new:
 * @desc:
 *
 * Create a new font description attribute. (This attribute
 * allows setting family, style, weight, variant, stretch,
 * and size simultaneously.)
 *
 * Return value:
 **/
static PangoAttribute *
gtk_text_attr_appearance_new (const GtkTextAppearance *appearance)
{
  static PangoAttrClass klass = {
    0,
    gtk_text_attr_appearance_copy,
    gtk_text_attr_appearance_destroy,
    gtk_text_attr_appearance_compare
  };

  GtkTextAttrAppearance *result;

  if (!klass.type)
    klass.type = gtk_text_attr_appearance_type =
      pango_attr_type_register ("GtkTextAttrAppearance");

  result = g_new (GtkTextAttrAppearance, 1);
  result->attr.klass = &klass;

  result->appearance = *appearance;

  if (appearance->bg_stipple)
    gdk_drawable_ref (appearance->bg_stipple);
  if (appearance->fg_stipple)
    gdk_drawable_ref (appearance->fg_stipple);

  return (PangoAttribute *)result;
}


static void
add_generic_attrs (GtkTextLayout      *layout,
                   GtkTextAppearance  *appearance,
                   gint                byte_count,
                   PangoAttrList      *attrs,
                   gint                start,
                   gboolean            size_only,
                   gboolean            is_text)
{
  PangoAttribute *attr;

  if (appearance->underline != PANGO_UNDERLINE_NONE)
    {
      attr = pango_attr_underline_new (appearance->underline);
      
      attr->start_index = start;
      attr->end_index = start + byte_count;
      
      pango_attr_list_insert (attrs, attr);
    }

  if (appearance->rise != 0)
    {
      attr = pango_attr_rise_new (appearance->rise);
      
      attr->start_index = start;
      attr->end_index = start + byte_count;
      
      pango_attr_list_insert (attrs, attr);
    }
  
  if (!size_only)
    {
      attr = gtk_text_attr_appearance_new (appearance);
      
      attr->start_index = start;
      attr->end_index = start + byte_count;

      ((GtkTextAttrAppearance *)attr)->appearance.is_text = is_text;
      
      pango_attr_list_insert (attrs, attr);
    }
}

static void
add_text_attrs (GtkTextLayout      *layout,
                GtkTextAttributes  *style,
                gint                byte_count,
                PangoAttrList      *attrs,
                gint                start,
                gboolean            size_only)
{
  PangoAttribute *attr;

  attr = pango_attr_font_desc_new (&style->font);
  attr->start_index = start;
  attr->end_index = start + byte_count;

  pango_attr_list_insert (attrs, attr);
}

static void
add_pixbuf_attrs (GtkTextLayout      *layout,
                  GtkTextLineDisplay *display,
                  GtkTextAttributes  *style,
                  GtkTextLineSegment *seg,
                  PangoAttrList      *attrs,
                  gint                start)
{
  PangoAttribute *attr;
  PangoRectangle logical_rect;
  GtkTextPixbuf *pixbuf = &seg->body.pixbuf;
  gint width, height;

  width = gdk_pixbuf_get_width (pixbuf->pixbuf);
  height = gdk_pixbuf_get_height (pixbuf->pixbuf);

  logical_rect.x = 0;
  logical_rect.y = -height * PANGO_SCALE;
  logical_rect.width = width * PANGO_SCALE;
  logical_rect.height = height * PANGO_SCALE;

  attr = pango_attr_shape_new (&logical_rect, &logical_rect);
  attr->start_index = start;
  attr->end_index = start + seg->byte_count;
  pango_attr_list_insert (attrs, attr);

  display->shaped_objects =
    g_slist_append (display->shaped_objects, pixbuf->pixbuf);
}

static void
add_child_attrs (GtkTextLayout      *layout,
                 GtkTextLineDisplay *display,
                 GtkTextAttributes  *style,
                 GtkTextLineSegment *seg,
                 PangoAttrList      *attrs,
                 gint                start)
{
  PangoAttribute *attr;
  PangoRectangle logical_rect;
  GtkTextChildAnchor *anchor;
  gint width, height;
  GSList *tmp_list;

  width = 1;
  height = 1;
  
  anchor = seg->body.child.obj;

  tmp_list = seg->body.child.widgets;
  while (tmp_list != NULL)
    {
      GtkWidget *child = tmp_list->data;

      if (_gtk_anchored_child_get_layout (child) == layout)
        {
          /* Found it */
          GtkRequisition req;

          gtk_widget_get_child_requisition (child, &req);
          
          width = req.width;
          height = req.height;

          display->shaped_objects =
            g_slist_append (display->shaped_objects, child);
          
          break;
        }
      
      tmp_list = g_slist_next (tmp_list);
    }

  if (tmp_list == NULL)
    {
      /* No widget at this anchor in this display;
       * not an error.
       */

      return;
    }

  if (layout->preedit_string)
    {
      g_free (layout->preedit_string);
      layout->preedit_string = NULL;
    }

  if (layout->preedit_attrs)
    {
      pango_attr_list_unref (layout->preedit_attrs);
      layout->preedit_attrs = NULL;
    }
  
  logical_rect.x = 0;
  logical_rect.y = -height * PANGO_SCALE;
  logical_rect.width = width * PANGO_SCALE;
  logical_rect.height = height * PANGO_SCALE;

  attr = pango_attr_shape_new (&logical_rect, &logical_rect);
  attr->start_index = start;
  attr->end_index = start + seg->byte_count;
  pango_attr_list_insert (attrs, attr);
}

static void
add_cursor (GtkTextLayout      *layout,
            GtkTextLineDisplay *display,
            GtkTextLineSegment *seg,
            gint                start)
{
  PangoRectangle strong_pos, weak_pos;
  GtkTextCursorDisplay *cursor;

  /* Hide insertion cursor when we have a selection or the layout
   * user has hidden the cursor.
   */
  if (_gtk_text_btree_mark_is_insert (_gtk_text_buffer_get_btree (layout->buffer),
                                     seg->body.mark.obj) &&
      (!layout->cursor_visible ||
       gtk_text_buffer_get_selection_bounds (layout->buffer, NULL, NULL)))
    return;

  pango_layout_get_cursor_pos (display->layout, start, &strong_pos, &weak_pos);

  cursor = g_new (GtkTextCursorDisplay, 1);

  cursor->x = PANGO_PIXELS (strong_pos.x);
  cursor->y = PANGO_PIXELS (strong_pos.y);
  cursor->height = PANGO_PIXELS (strong_pos.height);
  cursor->is_strong = TRUE;
  display->cursors = g_slist_prepend (display->cursors, cursor);

  if (weak_pos.x == strong_pos.x)
    cursor->is_weak = TRUE;
  else
    {
      cursor->is_weak = FALSE;

      cursor = g_new (GtkTextCursorDisplay, 1);

      cursor->x = PANGO_PIXELS (weak_pos.x);
      cursor->y = PANGO_PIXELS (weak_pos.y);
      cursor->height = PANGO_PIXELS (weak_pos.height);
      cursor->is_strong = FALSE;
      cursor->is_weak = TRUE;
      display->cursors = g_slist_prepend (display->cursors, cursor);
    }
}

static gboolean
is_shape (PangoLayoutRun *run)
{
  GSList *tmp_list = run->item->extra_attrs;
    
  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      if (attr->klass->type == PANGO_ATTR_SHAPE)
        return TRUE;

      tmp_list = tmp_list->next;
    }

  return FALSE;
}

static void
allocate_child_widgets (GtkTextLayout      *text_layout,
                        GtkTextLineDisplay *display)
{
  GSList *shaped = display->shaped_objects;
  PangoLayout *layout = display->layout;
  PangoLayoutIter *iter;
  
  iter = pango_layout_get_iter (layout);
  
  do
    {
      PangoLayoutRun *run = pango_layout_iter_get_run (iter);

      if (run && is_shape (run))
        {
          GObject *shaped_object = shaped->data;
          shaped = shaped->next;

          if (GTK_IS_WIDGET (shaped_object))
            {
              PangoRectangle extents;

              /* We emit "allocate_child" with the x,y of
               * the widget with respect to the top of the line
               * and the left side of the buffer
               */
              
              pango_layout_iter_get_run_extents (iter,
                                                 NULL,
                                                 &extents);

              g_print ("extents at %d,%d\n", extents.x, extents.y);
              
              g_signal_emit (G_OBJECT (text_layout),
                             signals[ALLOCATE_CHILD],
                             0,
                             shaped_object,
                             PANGO_PIXELS (extents.x) + display->x_offset,
                             PANGO_PIXELS (extents.y) + display->top_margin);
            }
        }
    }
  while (pango_layout_iter_next_run (iter));
  
  pango_layout_iter_free (iter);
}

static void
convert_color (GdkColor       *result,
	       PangoAttrColor *attr)
{
  result->red = attr->red;
  result->blue = attr->blue;
  result->green = attr->green;
}

/* This function is used to convert the preedit string attributes, which are
 * standard PangoAttributes, into the custom attributes used by the text
 * widget and insert them into a attr list with a given offset.
 */
static void
add_preedit_attrs (GtkTextLayout     *layout,
		   GtkTextAttributes *style,
		   PangoAttrList     *attrs,
		   gint               offset,
		   gboolean           size_only)
{
  PangoAttrIterator *iter = pango_attr_list_get_iterator (layout->preedit_attrs);

  do
    {
      GtkTextAppearance appearance = style->appearance;
      PangoFontDescription font_desc;
      PangoAttribute *insert_attr;
      GSList *extra_attrs = NULL;
      GSList *tmp_list;
      gint start, end;

      pango_attr_iterator_range (iter, &start, &end);

      if (end == G_MAXINT)
	end = layout->preedit_len;
      
      pango_attr_iterator_get_font (iter, &style->font,
				    &font_desc, &extra_attrs);
      
      tmp_list = extra_attrs;
      while (tmp_list)
	{
	  PangoAttribute *attr = tmp_list->data;
	  
	  switch (attr->klass->type)
	    {
	    case PANGO_ATTR_FOREGROUND:
	      convert_color (&appearance.fg_color, (PangoAttrColor *)attr);
	      break;
	    case PANGO_ATTR_BACKGROUND:
	      convert_color (&appearance.bg_color, (PangoAttrColor *)attr);
	      appearance.draw_bg = TRUE;
	      break;
	    case PANGO_ATTR_UNDERLINE:
	      appearance.underline = ((PangoAttrInt *)attr)->value;
	      break;
	    case PANGO_ATTR_STRIKETHROUGH:
	      appearance.strikethrough = ((PangoAttrInt *)attr)->value;
	      break;
            case PANGO_ATTR_RISE:
              appearance.rise = ((PangoAttrInt *)attr)->value;
              break;
	    default:
	      break;
	    }
	  
	  pango_attribute_destroy (attr);
	  tmp_list = tmp_list->next;
	}
      
      g_slist_free (extra_attrs);
      
      insert_attr = pango_attr_font_desc_new (&font_desc);
      insert_attr->start_index = start + offset;
      insert_attr->end_index = end + offset;
      
      pango_attr_list_insert (attrs, insert_attr);

      add_generic_attrs (layout, &appearance, end - start,
                         attrs, start + offset,
                         size_only, TRUE);
    }
  while (pango_attr_iterator_next (iter));

  pango_attr_iterator_destroy (iter);
}

GtkTextLineDisplay *
gtk_text_layout_get_line_display (GtkTextLayout *layout,
                                  GtkTextLine   *line,
                                  gboolean       size_only)
{
  GtkTextLineDisplay *display;
  GtkTextLineSegment *seg;
  GtkTextIter iter;
  GtkTextAttributes *style;
  gchar *text;
  PangoAttrList *attrs;
  gint byte_count, byte_offset;
  gdouble align;
  PangoRectangle extents;
  gboolean para_values_set = FALSE;
  GSList *cursor_byte_offsets = NULL;
  GSList *cursor_segs = NULL;
  GSList *tmp_list1, *tmp_list2;
  gboolean saw_widget = FALSE;
  
  g_return_val_if_fail (line != NULL, NULL);

  if (layout->one_display_cache)
    {
      if (line == layout->one_display_cache->line &&
          (size_only || !layout->one_display_cache->size_only))
        return layout->one_display_cache;
      else
        {
          GtkTextLineDisplay *tmp_display = layout->one_display_cache;
          layout->one_display_cache = NULL;
          gtk_text_layout_free_line_display (layout, tmp_display);
        }
    }

  display = g_new0 (GtkTextLineDisplay, 1);

  display->size_only = size_only;
  display->line = line;
  display->insert_index = -1;

  _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                    &iter, line, 0);

  /* Special-case optimization for completely
   * invisible lines; makes it faster to deal
   * with sequences of invisible lines.
   */
  if (totally_invisible_line (layout, line, &iter))
    return display;

  /* Allocate space for flat text for buffer
   */
  byte_count = _gtk_text_line_byte_count (line);
  text = g_malloc (byte_count);

  attrs = pango_attr_list_new ();

  /* Iterate over segments, creating display chunks for them. */
  byte_offset = 0;
  seg = _gtk_text_iter_get_any_segment (&iter);
  while (seg != NULL)
    {
      /* Displayable segments */
      if (seg->type == &gtk_text_char_type ||
          seg->type == &gtk_text_pixbuf_type ||
          seg->type == &gtk_text_child_type)
        {
          _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                            &iter, line,
                                            byte_offset);
          style = get_style (layout, &iter);

          /* We have to delay setting the paragraph values until we
           * hit the first pixbuf or text segment because toggles at
           * the beginning of the paragraph should affect the
           * paragraph-global values
           */
          if (!para_values_set)
            {
              set_para_values (layout, style, display, &align);
              para_values_set = TRUE;
            }

          /* First see if the chunk is invisible, and ignore it if so. Tk
           * looked at tabs, wrap mode, etc. before doing this, but
           * that made no sense to me, so I am just skipping the
           * invisible chunks
           */
          if (!style->invisible)
            {
              if (seg->type == &gtk_text_char_type)
                {
                  /* We don't want to split segments because of marks,
                   * so we scan forward for more segments only
                   * separated from us by marks. In theory, we should
                   * also merge segments with identical styles, even
                   * if there are toggles in-between
                   */

                  gint bytes = 0;
 		  GtkTextLineSegment *prev_seg = NULL;
  
 		  while (seg)
                    {
                      if (seg->type == &gtk_text_char_type)
                        {
                          memcpy (text + byte_offset, seg->body.chars, seg->byte_count);
                          byte_offset += seg->byte_count;
                          bytes += seg->byte_count;
                        }
 		      else if (seg->type == &gtk_text_right_mark_type ||
 			       seg->type == &gtk_text_left_mark_type)
                        {
 			  /* If we have preedit string, break out of this loop - we'll almost
 			   * certainly have different attributes on the preedit string
 			   */

 			  if (layout->preedit_len > 0 &&
 			      _gtk_text_btree_mark_is_insert (_gtk_text_buffer_get_btree (layout->buffer),
 							     seg->body.mark.obj))
			    break;

 			  if (seg->body.mark.visible)
 			    {
			      cursor_byte_offsets = g_slist_prepend (cursor_byte_offsets, GINT_TO_POINTER (byte_offset));
			      cursor_segs = g_slist_prepend (cursor_segs, seg);
			    }
                        }
		      else
			break;

 		      prev_seg = seg;
                      seg = seg->next;
                    }

 		  seg = prev_seg; /* Back up one */
                  add_generic_attrs (layout, &style->appearance,
                                     bytes,
                                     attrs, byte_offset - bytes,
                                     size_only, TRUE);
                  add_text_attrs (layout, style, bytes, attrs,
                                  byte_offset - bytes, size_only);
                }
              else if (seg->type == &gtk_text_pixbuf_type)
                {
                  add_generic_attrs (layout,
                                     &style->appearance,
                                     seg->byte_count,
                                     attrs, byte_offset,
                                     size_only, FALSE);
                  add_pixbuf_attrs (layout, display, style,
                                    seg, attrs, byte_offset);
                  memcpy (text + byte_offset, gtk_text_unknown_char_utf8,
                          seg->byte_count);
                  byte_offset += seg->byte_count;
                }
              else if (seg->type == &gtk_text_child_type)
                {
                  saw_widget = TRUE;
                  
                  add_generic_attrs (layout, &style->appearance,
                                     seg->byte_count,
                                     attrs, byte_offset,
                                     size_only, FALSE);
                  add_child_attrs (layout, display, style,
                                   seg, attrs, byte_offset);
                  memcpy (text + byte_offset, gtk_text_unknown_char_utf8,
                          seg->byte_count);
                  byte_offset += seg->byte_count;
                }
              else
                {
                  g_assert_not_reached ();
                }
            }

          release_style (layout, style);
        }

      /* Toggles */
      else if (seg->type == &gtk_text_toggle_on_type ||
               seg->type == &gtk_text_toggle_off_type)
        {
          /* Style may have changed, drop our
             current cached style */
          invalidate_cached_style (layout);
        }

      /* Marks */
      else if (seg->type == &gtk_text_right_mark_type ||
               seg->type == &gtk_text_left_mark_type)
        {
	  gint cursor_offset = 0;
 	  
	  /* At the insertion point, add the preedit string, if any */
	  
	  if (_gtk_text_btree_mark_is_insert (_gtk_text_buffer_get_btree (layout->buffer),
					     seg->body.mark.obj))
	    {
	      display->insert_index = byte_offset;
	      
	      if (layout->preedit_len > 0)
		{
		  byte_count += layout->preedit_len;
		  text = g_realloc (text, byte_count);

		  style = get_style (layout, &iter);
		  add_preedit_attrs (layout, style, attrs, byte_offset, size_only);
		  release_style (layout, style);
                  
		  memcpy (text + byte_offset, layout->preedit_string, layout->preedit_len);
		  byte_offset += layout->preedit_len;

		  cursor_offset = layout->preedit_cursor - layout->preedit_len;
		}
	    }
	  

          /* Display visible marks */

          if (seg->body.mark.visible)
            {
              cursor_byte_offsets = g_slist_prepend (cursor_byte_offsets,
                                                     GINT_TO_POINTER (byte_offset + cursor_offset));
              cursor_segs = g_slist_prepend (cursor_segs, seg);
            }
        }

      else
        g_error ("Unknown segment type: %s", seg->type->name);

      seg = seg->next;
    }
  
  if (!para_values_set)
    {
      style = get_style (layout, &iter);
      set_para_values (layout, style, display, &align);
      release_style (layout, style);
    }

  g_assert (byte_offset == byte_count);
  
  /* Pango doesn't want the trailing paragraph delimiters */

  {
    /* Only one character has type G_UNICODE_PARAGRAPH_SEPARATOR in
     * Unicode 3.0; update this if that changes.
     */
#define PARAGRAPH_SEPARATOR 0x2029
    gunichar ch = 0;

    if (byte_offset > 0)
      {
        const char *prev = g_utf8_prev_char (text + byte_offset);
        ch = g_utf8_get_char (prev);
        if (ch == PARAGRAPH_SEPARATOR || ch == '\r' || ch == '\n')
          byte_offset = prev - text; /* chop off */

        if (ch == '\n' && byte_offset > 0)
          {
            /* Possibly chop a CR as well */
            prev = g_utf8_prev_char (text + byte_offset);
            if (*prev == '\r')
              --byte_offset;
          }
      }
  }
  
  pango_layout_set_text (display->layout, text, byte_offset);
  pango_layout_set_attributes (display->layout, attrs);

  tmp_list1 = cursor_byte_offsets;
  tmp_list2 = cursor_segs;
  while (tmp_list1)
    {
      add_cursor (layout, display, tmp_list2->data,
                  GPOINTER_TO_INT (tmp_list1->data));
      tmp_list1 = tmp_list1->next;
      tmp_list2 = tmp_list2->next;
    }
  g_slist_free (cursor_byte_offsets);
  g_slist_free (cursor_segs);

  pango_layout_get_extents (display->layout, NULL, &extents);

  display->x_offset += (display->total_width - PANGO_PIXELS (extents.x + extents.width)) * align;

  display->width = PANGO_PIXELS (extents.width) + display->left_margin + display->right_margin;
  display->height += PANGO_PIXELS (extents.height);
  
  /* Free this if we aren't in a loop */
  if (layout->wrap_loop_count == 0)
    invalidate_cached_style (layout);

  g_free (text);
  pango_attr_list_unref (attrs);

  layout->one_display_cache = display;

  if (saw_widget)
    allocate_child_widgets (layout, display);
  
  return display;
}

void
gtk_text_layout_free_line_display (GtkTextLayout      *layout,
                                   GtkTextLineDisplay *display)
{
  if (display != layout->one_display_cache)
    {
      g_object_unref (G_OBJECT (display->layout));

      if (display->cursors)
        {
          g_slist_foreach (display->cursors, (GFunc)g_free, NULL);
          g_slist_free (display->cursors);
          g_slist_free (display->shaped_objects);
        }

      g_free (display);
    }
}

/* Functions to convert iter <=> index for the line of a GtkTextLineDisplay
 * taking into account the preedit string, if necessary.
 */
static gint
line_display_iter_to_index (GtkTextLayout      *layout,
			    GtkTextLineDisplay *display,
			    const GtkTextIter  *iter)
{
  gint index;

  g_return_val_if_fail (_gtk_text_iter_get_text_line (iter) == display->line, 0);

  index = gtk_text_iter_get_line_index (iter);

  if (index >= display->insert_index)
    index += layout->preedit_len;

  return index;
}

static void
line_display_index_to_iter (GtkTextLayout      *layout,
			    GtkTextLineDisplay *display,
			    GtkTextIter        *iter,
			    gint                index,
			    gint                trailing)
{
  gint line_len;
  
  if (index >= display->insert_index + layout->preedit_len)
    index -= layout->preedit_len;
  else if (index > display->insert_index)
    {
      index = display->insert_index;
      trailing = 0;
    }
  
  line_len = _gtk_text_line_byte_count (display->line);
  g_assert (index <= line_len);

  if (index < line_len)
    _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                      iter, display->line, index);
  else
    {
      /* Clamp to end of line - really this clamping should have been done
       * before here, maybe in Pango, this is a broken band-aid I think
       */
      g_assert (index == line_len);
      
      _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                        iter, display->line, 0);

      if (!gtk_text_iter_ends_line (iter))
        gtk_text_iter_forward_to_line_end (iter);
    }

  /* FIXME should this be cursor positions? */
  gtk_text_iter_forward_chars (iter, trailing);
}

/* FIXME: This really doesn't belong in this file ... */
static GtkTextLineData*
gtk_text_line_data_new (GtkTextLayout *layout,
                        GtkTextLine   *line)
{
  GtkTextLineData *line_data;

  line_data = g_new (GtkTextLineData, 1);

  line_data->view_id = layout;
  line_data->next = NULL;
  line_data->width = 0;
  line_data->height = 0;
  line_data->valid = FALSE;

  return line_data;
}

static void
get_line_at_y (GtkTextLayout *layout,
               gint           y,
               GtkTextLine  **line,
               gint          *line_top)
{
  if (y < 0)
    y = 0;
  if (y > layout->height)
    y = layout->height;

  *line = _gtk_text_btree_find_line_by_y (_gtk_text_buffer_get_btree (layout->buffer),
                                         layout, y, line_top);
  if (*line == NULL)
    {
      *line = _gtk_text_btree_get_line (_gtk_text_buffer_get_btree (layout->buffer),
                                       _gtk_text_btree_line_count (_gtk_text_buffer_get_btree (layout->buffer)) - 1, NULL);
      if (line_top)
        *line_top =
          _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer),
                                        *line, layout);
    }
}

/**
 * gtk_text_layout_get_line_at_y:
 * @layout: a #GtkLayout
 * @target_iter: the iterator in which the result is stored
 * @y: the y positition
 * @line_top: location to store the y coordinate of the
 *            top of the line. (Can by %NULL.)
 *
 * Get the iter at the beginning of the line which is displayed
 * at the given y.
 **/
void
gtk_text_layout_get_line_at_y (GtkTextLayout *layout,
                               GtkTextIter   *target_iter,
                               gint           y,
                               gint          *line_top)
{
  GtkTextLine *line;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (target_iter != NULL);

  get_line_at_y (layout, y, &line, line_top);
  _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                   target_iter, line, 0);
}

void
gtk_text_layout_get_iter_at_pixel (GtkTextLayout *layout,
                                   GtkTextIter *target_iter,
                                   gint x, gint y)
{
  GtkTextLine *line;
  gint byte_index, trailing;
  gint line_top;
  GtkTextLineDisplay *display;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (target_iter != NULL);

  /* Adjust pixels to be on-screen. This gives nice
     behavior if the user is dragging with a pointer grab.
  */
  if (x < 0)
    x = 0;
  if (x > layout->width)
    x = layout->width;

  get_line_at_y (layout, y, &line, &line_top);

  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  x -= display->x_offset;
  y -= line_top + display->top_margin;

  /* We clamp y to the area of the actual layout so that the layouts
   * hit testing works OK on the space above and below the layout
   */
  y = CLAMP (y, 0, display->height - display->top_margin - display->bottom_margin - 1);

  if (!pango_layout_xy_to_index (display->layout, x * PANGO_SCALE, y * PANGO_SCALE,
                                 &byte_index, &trailing))
    {
      byte_index = _gtk_text_line_byte_count (line);
      trailing = 0;
    }

  line_display_index_to_iter (layout, display, target_iter, byte_index, trailing);

  gtk_text_layout_free_line_display (layout, display);
}

/**
 * gtk_text_layout_get_cursor_locations
 * @layout: a #GtkTextLayout
 * @iter: a #GtkTextIter
 * @strong_pos: location to store the strong cursor position (may be %NULL)
 * @weak_pos: location to store the weak cursor position (may be %NULL)
 *
 * Given an iterator within a text laout, determine the positions that of the
 * strong and weak cursors if the insertion point is at that
 * iterator. The position of each cursor is stored as a zero-width
 * rectangle. The strong cursor location is the location where
 * characters of the directionality equal to the base direction of the
 * paragraph are inserted.  The weak cursor location is the location
 * where characters of the directionality opposite to the base
 * direction of the paragraph are inserted.
 **/
void
gtk_text_layout_get_cursor_locations (GtkTextLayout  *layout,
                                      GtkTextIter    *iter,
                                      GdkRectangle   *strong_pos,
                                      GdkRectangle   *weak_pos)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_top;
  gint index;

  PangoRectangle pango_strong_pos;
  PangoRectangle pango_weak_pos;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (iter != NULL);

  line = _gtk_text_iter_get_text_line (iter);
  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  index = line_display_iter_to_index (layout, display, iter);
  
  line_top = _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer),
                                           line, layout);
  
  pango_layout_get_cursor_pos (display->layout, index,
                               strong_pos ? &pango_strong_pos : NULL,
                               weak_pos ? &pango_weak_pos : NULL);

  if (strong_pos)
    {
      strong_pos->x = display->x_offset + pango_strong_pos.x / PANGO_SCALE;
      strong_pos->y = line_top + display->top_margin + pango_strong_pos.y / PANGO_SCALE;
      strong_pos->width = 0;
      strong_pos->height = pango_strong_pos.height / PANGO_SCALE;
    }

  if (weak_pos)
    {
      weak_pos->x = display->x_offset + pango_weak_pos.x / PANGO_SCALE;
      weak_pos->y = line_top + display->top_margin + pango_weak_pos.y / PANGO_SCALE;
      weak_pos->width = 0;
      weak_pos->height = pango_weak_pos.height / PANGO_SCALE;
    }

  gtk_text_layout_free_line_display (layout, display);
}

/**
 * gtk_text_layout_get_line_yrange:
 * @layout: a #GtkTextLayout
 * @iter:   a #GtkTextIter
 * @y:      location to store the top of the paragraph in pixels,
 *          or %NULL.
 * @height  location to store the height of the paragraph in pixels,
 *          or %NULL.
 *
 * Find the range of y coordinates for the paragraph containing
 * the given iter.
 **/
void
gtk_text_layout_get_line_yrange (GtkTextLayout     *layout,
                                 const GtkTextIter *iter,
                                 gint              *y,
                                 gint              *height)
{
  GtkTextLine *line;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (_gtk_text_iter_get_btree (iter) == _gtk_text_buffer_get_btree (layout->buffer));

  line = _gtk_text_iter_get_text_line (iter);

  if (y)
    *y = _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer),
                                       line, layout);
  if (height)
    {
      GtkTextLineData *line_data = _gtk_text_line_get_data (line, layout);
      if (line_data)
        *height = line_data->height;
      else
        *height = 0;
    }
}

void
gtk_text_layout_get_iter_location (GtkTextLayout     *layout,
                                   const GtkTextIter *iter,
                                   GdkRectangle      *rect)
{
  PangoRectangle pango_rect;
  GtkTextLine *line;
  GtkTextBTree *tree;
  GtkTextLineDisplay *display;
  gint byte_index;
  gint x_offset;
  
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (_gtk_text_iter_get_btree (iter) == _gtk_text_buffer_get_btree (layout->buffer));
  g_return_if_fail (rect != NULL);

  tree = _gtk_text_iter_get_btree (iter);
  line = _gtk_text_iter_get_text_line (iter);

  display = gtk_text_layout_get_line_display (layout, line, FALSE);

  rect->y = _gtk_text_btree_find_line_top (tree, line, layout);

  x_offset = display->x_offset * PANGO_SCALE;

  byte_index = gtk_text_iter_get_line_index (iter);
  
  pango_layout_index_to_pos (display->layout, byte_index, &pango_rect);
  
  rect->x = PANGO_PIXELS (x_offset + pango_rect.x);
  rect->y += PANGO_PIXELS (pango_rect.y) + display->top_margin;
  rect->width = PANGO_PIXELS (pango_rect.width);
  rect->height = PANGO_PIXELS (pango_rect.height);

  gtk_text_layout_free_line_display (layout, display);
}

/* FFIXX */

/* Find the iter for the logical beginning of the first display line whose
 * top y is >= y. If none exists, move the iter to the logical beginning
 * of the last line in the buffer.
 */
static void
find_display_line_below (GtkTextLayout *layout,
                         GtkTextIter   *iter,
                         gint           y)
{
  GtkTextLine *line, *next;
  GtkTextLine *found_line = NULL;
  gint line_top;
  gint found_byte = 0;

  line = _gtk_text_btree_find_line_by_y (_gtk_text_buffer_get_btree (layout->buffer),
                                        layout, y, &line_top);
  if (!line)
    {
      line =
        _gtk_text_btree_get_line (_gtk_text_buffer_get_btree (layout->buffer),
                                 _gtk_text_btree_line_count (_gtk_text_buffer_get_btree (layout->buffer)) - 1,
                                 NULL);
      line_top =
        _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer),
                                      line, layout);
    }

  while (line && !found_line)
    {
      GtkTextLineDisplay *display = gtk_text_layout_get_line_display (layout, line, FALSE);
      PangoLayoutIter *layout_iter;

      layout_iter = pango_layout_get_iter (display->layout);

      line_top += display->top_margin;

      do
        {
          gint first_y, last_y;
          PangoLayoutLine *layout_line = pango_layout_iter_get_line (layout_iter);

          found_byte = layout_line->start_index;
          
          if (line_top >= y)
            {
              found_line = line;
              break;
            }

          pango_layout_iter_get_line_yrange (layout_iter, &first_y, &last_y);
          line_top += (last_y - first_y) / PANGO_SCALE;
        }
      while (pango_layout_iter_next_line (layout_iter));

      pango_layout_iter_free (layout_iter);
      
      line_top += display->bottom_margin;
      gtk_text_layout_free_line_display (layout, display);

      next = _gtk_text_line_next (line);
      if (!next)
        found_line = line;

      line = next;
    }

  _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                   iter, found_line, found_byte);
}

/* Find the iter for the logical beginning of the last display line whose
 * top y is >= y. If none exists, move the iter to the logical beginning
 * of the first line in the buffer.
 */
static void
find_display_line_above (GtkTextLayout *layout,
                         GtkTextIter   *iter,
                         gint           y)
{
  GtkTextLine *line;
  GtkTextLine *found_line = NULL;
  gint line_top;
  gint found_byte = 0;

  line = _gtk_text_btree_find_line_by_y (_gtk_text_buffer_get_btree (layout->buffer), layout, y, &line_top);
  if (!line)
    {
      line = _gtk_text_btree_get_line (_gtk_text_buffer_get_btree (layout->buffer),
                                      _gtk_text_btree_line_count (_gtk_text_buffer_get_btree (layout->buffer)) - 1, NULL);
      line_top = _gtk_text_btree_find_line_top (_gtk_text_buffer_get_btree (layout->buffer), line, layout);
    }

  while (line && !found_line)
    {
      GtkTextLineDisplay *display = gtk_text_layout_get_line_display (layout, line, FALSE);
      PangoRectangle logical_rect;
      PangoLayoutIter *layout_iter;
      gint tmp_top;

      layout_iter = pango_layout_get_iter (display->layout);
      
      line_top -= display->top_margin + display->bottom_margin;
      pango_layout_iter_get_layout_extents (layout_iter, NULL, &logical_rect);
      line_top -= logical_rect.height / PANGO_SCALE;

      tmp_top = line_top + display->top_margin;

      do
        {
          gint first_y, last_y;
          PangoLayoutLine *layout_line = pango_layout_iter_get_line (layout_iter);

          found_byte = layout_line->start_index;

          pango_layout_iter_get_line_yrange (layout_iter, &first_y, &last_y);
          
          tmp_top -= (last_y - first_y) / PANGO_SCALE;

          if (tmp_top < y)
            {
              found_line = line;
              goto done;
            }
        }
      while (pango_layout_iter_next_line (layout_iter));

      pango_layout_iter_free (layout_iter);
      
      gtk_text_layout_free_line_display (layout, display);

      line = _gtk_text_line_previous (line);
    }

 done:
  
  if (found_line)
    _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                     iter, found_line, found_byte);
  else
    gtk_text_buffer_get_iter_at_offset (layout->buffer, iter, 0);
}

/**
 * gtk_text_layout_clamp_iter_to_vrange:
 * @layout: a #GtkTextLayout
 * @iter:   a #GtkTextIter
 * @top:    the top of the range
 * @bottom: the bottom the range
 *
 * If the iterator is not fully in the range @top <= y < @bottom,
 * then, if possible, move it the minimum distance so that the
 * iterator in this range.
 *
 * Returns: %TRUE if the iterator was moved, otherwise %FALSE.
 **/
gboolean
gtk_text_layout_clamp_iter_to_vrange (GtkTextLayout *layout,
                                      GtkTextIter   *iter,
                                      gint           top,
                                      gint           bottom)
{
  GdkRectangle iter_rect;

  gtk_text_layout_get_iter_location (layout, iter, &iter_rect);

  /* If the iter is at least partially above the range, put the iter
   * at the first fully visible line after the range.
   */
  if (iter_rect.y < top)
    {
      find_display_line_below (layout, iter, top);

      return TRUE;
    }
  /* Otherwise, if the iter is at least partially below the screen, put the
   * iter on the last logical position of the last completely visible
   * line on screen
   */
  else if (iter_rect.y + iter_rect.height > bottom)
    {
      find_display_line_above (layout, iter, bottom);

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_layout_move_iter_to_next_line:
 * @layout: a #GtkLayout
 * @iter:   a #GtkTextIter
 *
 * Move the iterator to the beginning of the previous line. The lines
 * of a wrapped paragraph are treated as distinct for this operation.
 **/
gboolean
gtk_text_layout_move_iter_to_previous_line (GtkTextLayout *layout,
                                            GtkTextIter   *iter)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_byte;
  GSList *tmp_list;
  PangoLayoutLine *layout_line;
  GtkTextIter orig;
  
  g_return_val_if_fail (layout != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  orig = *iter;
  
  line = _gtk_text_iter_get_text_line (iter);
  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  line_byte = line_display_iter_to_index (layout, display, iter);

  tmp_list = pango_layout_get_lines (display->layout);
  layout_line = tmp_list->data;

  if (line_byte < layout_line->length || !tmp_list->next) /* first line of paragraph */
    {
      GtkTextLine *prev_line = _gtk_text_line_previous (line);

      if (prev_line)
        {
          gtk_text_layout_free_line_display (layout, display);
          display = gtk_text_layout_get_line_display (layout, prev_line, FALSE);
	  tmp_list = g_slist_last (pango_layout_get_lines (display->layout));
	  layout_line = tmp_list->data;

          line_display_index_to_iter (layout, display, iter,
                                      layout_line->start_index + layout_line->length, 0);
        }
      else
 	line_display_index_to_iter (layout, display, iter, 0, 0);
    }
  else
    {
      gint prev_offset = layout_line->start_index;

      tmp_list = tmp_list->next;
      while (tmp_list)
        {
          layout_line = tmp_list->data;

          if (line_byte < layout_line->start_index + layout_line->length ||
              !tmp_list->next)
            {
 	      line_display_index_to_iter (layout, display, iter, prev_offset, 0);
              break;
            }

          prev_offset = layout_line->start_index;
          tmp_list = tmp_list->next;
        }
    }

  gtk_text_layout_free_line_display (layout, display);

  return
    !gtk_text_iter_equal (iter, &orig) &&
    !gtk_text_iter_is_last (iter);
}

/**
 * gtk_text_layout_move_iter_to_next_line:
 * @layout: a #GtkLayout
 * @iter:   a #GtkTextIter
 *
 * Move the iterator to the beginning of the next line. The
 * lines of a wrapped paragraph are treated as distinct for
 * this operation.
 **/
gboolean
gtk_text_layout_move_iter_to_next_line (GtkTextLayout *layout,
                                        GtkTextIter   *iter)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_byte;
  GtkTextIter orig;
  gboolean found = FALSE;
  gboolean found_after = FALSE;
  gboolean first = TRUE;

  g_return_val_if_fail (layout != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  orig = *iter;
  
  line = _gtk_text_iter_get_text_line (iter);

  while (line && !found_after)
    {
      GSList *tmp_list;

      display = gtk_text_layout_get_line_display (layout, line, FALSE);

      if (first)
	{
	  line_byte = line_display_iter_to_index (layout, display, iter);
	  first = FALSE;
	}
      else
	line_byte = 0;
	
      tmp_list = pango_layout_get_lines (display->layout);
      while (tmp_list && !found_after)
        {
          PangoLayoutLine *layout_line = tmp_list->data;

          if (found)
            {
	      line_display_index_to_iter (layout, display, iter,
                                          layout_line->start_index, 0);
              found_after = TRUE;
            }
          else if (line_byte < layout_line->start_index + layout_line->length || !tmp_list->next)
            found = TRUE;
          
          tmp_list = tmp_list->next;
        }

      gtk_text_layout_free_line_display (layout, display);

      line = _gtk_text_line_next (line);
    }

  return
    !gtk_text_iter_equal (iter, &orig) &&
    !gtk_text_iter_is_last (iter);
}

/**
 * gtk_text_layout_move_iter_to_line_end:
 * @layout: a #GtkTextLayout
 * @direction: if negative, move to beginning of line, otherwise
               move to end of line.
 *
 * Move to the beginning or end of a display line.
 **/
gboolean
gtk_text_layout_move_iter_to_line_end (GtkTextLayout *layout,
                                       GtkTextIter   *iter,
                                       gint           direction)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_byte;
  GSList *tmp_list;
  GtkTextIter orig;
  
  g_return_val_if_fail (layout != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  orig = *iter;
  
  line = _gtk_text_iter_get_text_line (iter);
  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  line_byte = line_display_iter_to_index (layout, display, iter);

  tmp_list = pango_layout_get_lines (display->layout);
  while (tmp_list)
    {
      PangoLayoutLine *layout_line = tmp_list->data;

      if (line_byte < layout_line->start_index + layout_line->length || !tmp_list->next)
        {
 	  line_display_index_to_iter (layout, display, iter,
 				      direction < 0 ? layout_line->start_index : layout_line->start_index + layout_line->length,
 				      0);

          /* FIXME: As a bad hack, we move back one position when we
	   * are inside a paragraph to avoid going to next line on a
	   * forced break not at whitespace. Real fix is to keep track
	   * of whether marks are at leading or trailing edge?  */
          if (direction > 0 && layout_line->length > 0 && !gtk_text_iter_ends_line (iter))
            gtk_text_iter_backward_char (iter);

          break;
        }
      
      tmp_list = tmp_list->next;
    }

  gtk_text_layout_free_line_display (layout, display);

  return
    !gtk_text_iter_equal (iter, &orig) &&
    !gtk_text_iter_is_last (iter);
}


/**
 * gtk_text_layout_iter_starts_line:
 * @layout: a #GtkTextLayout
 * @iter: iterator to test
 *
 * Tests whether an iterator is at the start of a display line.
 **/
gboolean
gtk_text_layout_iter_starts_line (GtkTextLayout       *layout,
                                  const GtkTextIter   *iter)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_byte;
  GSList *tmp_list;
  
  g_return_val_if_fail (layout != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_LAYOUT (layout), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  line = _gtk_text_iter_get_text_line (iter);
  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  line_byte = line_display_iter_to_index (layout, display, iter);

  tmp_list = pango_layout_get_lines (display->layout);
  while (tmp_list)
    {
      PangoLayoutLine *layout_line = tmp_list->data;

      if (line_byte < layout_line->start_index + layout_line->length ||
          !tmp_list->next)
        {
          /* We're located on this line of the para delimiters before
           * it
           */
          gtk_text_layout_free_line_display (layout, display);
          
          if (line_byte == layout_line->start_index)
            return TRUE;
          else
            return FALSE;
        }
      
      tmp_list = tmp_list->next;
    }

  g_assert_not_reached ();
  return FALSE;
}

void
gtk_text_layout_get_iter_at_line (GtkTextLayout  *layout,
                                  GtkTextIter    *iter,
                                  GtkTextLine    *line,
                                  gint            byte_offset)
{
  _gtk_text_btree_get_iter_at_line (_gtk_text_buffer_get_btree (layout->buffer),
                                    iter, line, byte_offset);
}


/**
 * gtk_text_layout_move_iter_to_x:
 * @layout: a #GtkTextLayout
 * @iter:   a #GtkTextIter
 * @x:      X coordinate
 *
 * Keeping the iterator on the same line of the layout, move it to the
 * specified X coordinate. The lines of a wrapped paragraph are
 * treated as distinct for this operation.
 **/
void
gtk_text_layout_move_iter_to_x (GtkTextLayout *layout,
                                GtkTextIter   *iter,
                                gint           x)
{
  GtkTextLine *line;
  GtkTextLineDisplay *display;
  gint line_byte;
  PangoLayoutIter *layout_iter;
  
  g_return_if_fail (layout != NULL);
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (iter != NULL);

  line = _gtk_text_iter_get_text_line (iter);

  display = gtk_text_layout_get_line_display (layout, line, FALSE);
  line_byte = line_display_iter_to_index (layout, display, iter);

  layout_iter = pango_layout_get_iter (display->layout);

  do
    {
      PangoLayoutLine *layout_line = pango_layout_iter_get_line (layout_iter);

      if (line_byte < layout_line->start_index + layout_line->length ||
          pango_layout_iter_at_last_line (layout_iter))
        {
          PangoRectangle logical_rect;
          gint byte_index, trailing;
          gint x_offset = display->x_offset * PANGO_SCALE;

          pango_layout_iter_get_line_extents (layout_iter, NULL, &logical_rect);

          pango_layout_line_x_to_index (layout_line,
                                        x * PANGO_SCALE - x_offset - logical_rect.x,
                                        &byte_index, &trailing);

 	  line_display_index_to_iter (layout, display, iter, byte_index, trailing);

          break;
        }
    }
  while (pango_layout_iter_next_line (layout_iter));

  pango_layout_iter_free (layout_iter);
  
  gtk_text_layout_free_line_display (layout, display);
}

/**
 * gtk_text_layout_move_iter_visually:
 * @layout:  a #GtkTextLayout
 * @iter:    a #GtkTextIter
 * @count:   number of characters to move (negative moves left, positive moves right)
 *
 * Move the iterator a given number of characters visually, treating
 * it as the strong cursor position. If @count is positive, then the
 * new strong cursor position will be @count positions to the right of
 * the old cursor position. If @count is negative then the new strong
 * cursor position will be @count positions to the left of the old
 * cursor position.
 *
 * In the presence of bidirection text, the correspondence
 * between logical and visual order will depend on the direction
 * of the current run, and there may be jumps when the cursor
 * is moved off of the end of a run.
 **/

gboolean
gtk_text_layout_move_iter_visually (GtkTextLayout *layout,
                                    GtkTextIter   *iter,
                                    gint           count)
{
  GtkTextLineDisplay *display = NULL;
  GtkTextIter orig;
  
  g_return_val_if_fail (layout != NULL, FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  orig = *iter;
  
  while (count != 0)
    {
      GtkTextLine *line = _gtk_text_iter_get_text_line (iter);
      gint line_byte;
      gint extra_back = 0;

      int byte_count = _gtk_text_line_byte_count (line);

      int new_index;
      int new_trailing;

 
      if (!display)
	display = gtk_text_layout_get_line_display (layout, line, FALSE);
      line_byte = line_display_iter_to_index (layout, display, iter);

      if (count > 0)
        {
          pango_layout_move_cursor_visually (display->layout, line_byte, 0, 1, &new_index, &new_trailing);
          count--;
        }
      else
        {
          pango_layout_move_cursor_visually (display->layout, line_byte, 0, -1, &new_index, &new_trailing);
          count++;
        }

      /* We need to handle the preedit string specially. Well, we don't really need to
       * handle it specially, since hopefully calling gtk_im_context_reset() will
       * remove the preedit string; but if we start off in front of the preedit
       * string (logically) and end up in or on the back edge of the preedit string,
       * we should move the iter one place farther.
       */
      if (layout->preedit_len > 0 && display->insert_index >= 0)
	{
	  if (line_byte == display->insert_index + layout->preedit_len &&
	      new_index < display->insert_index + layout->preedit_len)
	    {
	      line_byte = display->insert_index;
	      extra_back = 1;
	    }
	}
      
      if (new_index < 0 || (new_index == 0 && extra_back))
        {
          line = _gtk_text_line_previous (line);

          if (!line)
            goto done;
          
 	  gtk_text_layout_free_line_display (layout, display);
 	  display = gtk_text_layout_get_line_display (layout, line, FALSE);
          new_index = _gtk_text_line_byte_count (line);
        }
      else if (new_index > byte_count)
        {
          line = _gtk_text_line_next (line);
          if (!line)
            goto done;

 	  gtk_text_layout_free_line_display (layout, display);
 	  display = gtk_text_layout_get_line_display (layout, line, FALSE);
          new_index = 0;
        }
      
       line_display_index_to_iter (layout, display, iter, new_index, new_trailing);
       if (extra_back)
	 gtk_text_iter_backward_char (iter);
    }

  gtk_text_layout_free_line_display (layout, display);

 done:
  
  return
    !gtk_text_iter_equal (iter, &orig) &&
    !gtk_text_iter_is_last (iter);
}

void
gtk_text_layout_spew (GtkTextLayout *layout)
{
#if 0
  GtkTextDisplayLine *iter;
  guint wrapped = 0;
  guint paragraphs = 0;
  GtkTextLine *last_line = NULL;

  iter = layout->line_list;
  while (iter != NULL)
    {
      if (iter->line != last_line)
        {
          printf ("%5u  paragraph (%p)\n", paragraphs, iter->line);
          ++paragraphs;
          last_line = iter->line;
        }

      printf ("  %5u  y: %d len: %d start: %d bytes: %d\n",
              wrapped, iter->y, iter->length, iter->byte_offset,
              iter->byte_count);

      ++wrapped;
      iter = iter->next;
    }

  printf ("Layout %s recompute\n",
          layout->need_recompute ? "needs" : "doesn't need");

  printf ("Layout pars: %u lines: %u size: %d x %d Screen width: %d\n",
          paragraphs, wrapped, layout->width,
          layout->height, layout->screen_width);
#endif
}
