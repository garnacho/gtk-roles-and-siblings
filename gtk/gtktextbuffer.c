/*  gtktextbuffer.c - the "model" in the MVC text widget architecture 
 *  Copyright (c) 2000 Red Hat, Inc.
 *  Developed by Havoc Pennington
 */

#include "gtkinvisible.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtktextbuffer.h"
#include "gtktextbtree.h"
#include "gtktextiterprivate.h"

enum {
  INSERT_TEXT,
  DELETE_TEXT,
  CHANGED,
  MODIFIED_CHANGED,
  MARK_SET,
  MARK_DELETED,
  APPLY_TAG,
  REMOVE_TAG,
  LAST_SIGNAL
};

enum {
  ARG_0,
  LAST_ARG
};

enum {
  TARGET_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT,
  TARGET_UTF8_STRING
};

static void gtk_text_buffer_init       (GtkTextBuffer      *tkxt_buffer);
static void gtk_text_buffer_class_init (GtkTextBufferClass *klass);
static void gtk_text_buffer_destroy    (GtkObject          *object);
static void gtk_text_buffer_finalize   (GObject            *object);


static void gtk_text_buffer_update_primary_selection   (GtkTextBuffer     *buffer);
static void gtk_text_buffer_update_clipboard_selection (GtkTextBuffer     *buffer);
static void gtk_text_buffer_real_insert_text           (GtkTextBuffer     *buffer,
                                                        GtkTextIter       *iter,
                                                        const gchar       *text,
                                                        gint               len);
static void gtk_text_buffer_real_delete_text           (GtkTextBuffer     *buffer,
                                                        GtkTextIter       *start,
                                                        GtkTextIter       *end);
static void gtk_text_buffer_real_apply_tag             (GtkTextBuffer     *buffer,
                                                        GtkTextTag        *tag,
                                                        const GtkTextIter *start_char,
                                                        const GtkTextIter *end_char);
static void gtk_text_buffer_real_remove_tag            (GtkTextBuffer     *buffer,
                                                        GtkTextTag        *tag,
                                                        const GtkTextIter *start_char,
                                                        const GtkTextIter *end_char);


void gtk_marshal_NONE__INT_POINTER_INT (GtkObject  *object,
                                        GtkSignalFunc func,
                                        gpointer func_data,
                                        GtkArg  *args);

static GdkAtom clipboard_atom = GDK_NONE;
static GdkAtom text_atom = GDK_NONE;
static GdkAtom ctext_atom = GDK_NONE;
static GdkAtom utf8_atom = GDK_NONE;

static GtkObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_text_buffer_get_type (void)
{
  static GtkType our_type = 0;

  if (our_type == 0)
    {
      static const GtkTypeInfo our_info =
      {
        "GtkTextBuffer",
        sizeof (GtkTextBuffer),
        sizeof (GtkTextBufferClass),
        (GtkClassInitFunc) gtk_text_buffer_class_init,
        (GtkObjectInitFunc) gtk_text_buffer_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

      our_type = gtk_type_unique (GTK_TYPE_OBJECT, &our_info);
    }

  return our_type;
}

static void
gtk_text_buffer_class_init (GtkTextBufferClass *klass)
{
  GtkObjectClass *object_class = (GtkObjectClass*) klass;
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);
  
  signals[INSERT_TEXT] =
    gtk_signal_new ("insert_text",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, insert_text),
                    gtk_marshal_NONE__INT_POINTER_INT,
                    GTK_TYPE_NONE,
                    3,
                    GTK_TYPE_INT,
                    GTK_TYPE_POINTER,
                    GTK_TYPE_INT);

  signals[DELETE_TEXT] =
    gtk_signal_new ("delete_text",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, delete_text),
                    gtk_marshal_NONE__INT_INT,
                    GTK_TYPE_NONE,
                    2,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT);

  signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, changed),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE,
                    0);

  signals[MODIFIED_CHANGED] =
    gtk_signal_new ("modified_changed",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, modified_changed),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE,
                    0);
  
  signals[MARK_SET] =
    gtk_signal_new ("mark_set",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, mark_set),
                    gtk_marshal_NONE__POINTER_POINTER,
                    GTK_TYPE_NONE,
                    2,
                    GTK_TYPE_POINTER,
                    GTK_TYPE_POINTER);

  signals[MARK_DELETED] =
    gtk_signal_new ("mark_deleted",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, mark_deleted),
                    gtk_marshal_NONE__POINTER,
                    GTK_TYPE_NONE,
                    1,
                    GTK_TYPE_POINTER);

  signals[APPLY_TAG] =
    gtk_signal_new ("apply_tag",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, apply_tag),
                    gtk_marshal_NONE__POINTER_INT_INT,
                    GTK_TYPE_NONE,
                    3,
                    GTK_TYPE_POINTER,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT);
    
  signals[REMOVE_TAG] =
    gtk_signal_new ("remove_tag",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextBufferClass, remove_tag),
                    gtk_marshal_NONE__POINTER_INT_INT,
                    GTK_TYPE_NONE,
                    3,
                    GTK_TYPE_POINTER,
                    GTK_TYPE_INT,
                    GTK_TYPE_INT);
  
  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

  object_class->destroy = gtk_text_buffer_destroy;

  gobject_class->finalize = gtk_text_buffer_finalize;

  klass->insert_text = gtk_text_buffer_real_insert_text;
  klass->delete_text = gtk_text_buffer_real_delete_text;
  klass->apply_tag = gtk_text_buffer_real_apply_tag;
  klass->remove_tag = gtk_text_buffer_real_remove_tag;
}


typedef gint (*GtkSignal_NONE__INT_POINTER_INT) (GtkObject  *object,
                                                 gint pos,
                                                 const gchar *text,
                                                 gint len,
                                                 gpointer user_data);

void 
gtk_marshal_NONE__INT_POINTER_INT (GtkObject  *object,
                                   GtkSignalFunc func,
                                   gpointer func_data,
                                   GtkArg  *args)
{
  GtkSignal_NONE__INT_POINTER_INT rfunc;

  rfunc = (GtkSignal_NONE__INT_POINTER_INT) func;

  (*rfunc) (object,
            GTK_VALUE_INT (args[0]),
            GTK_VALUE_POINTER (args[1]),
            GTK_VALUE_INT (args[2]),
            func_data);
}

void
gtk_text_buffer_init (GtkTextBuffer *buffer)
{
  static const GtkTargetEntry targets[] = {
    { "STRING", 0, TARGET_STRING },
    { "TEXT",   0, TARGET_TEXT }, 
    { "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
    { "UTF8_STRING", 0, TARGET_UTF8_STRING }
  };
  static const gint n_targets = sizeof(targets) / sizeof(targets[0]);

  if (!clipboard_atom)
    clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

  if (!text_atom)
    text_atom = gdk_atom_intern ("TEXT", FALSE);

  if (!ctext_atom)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

  if (!utf8_atom)
    utf8_atom = gdk_atom_intern ("UTF8_STRING", FALSE);
  
  buffer->selection_widget = gtk_invisible_new();
  
  gtk_selection_add_targets (buffer->selection_widget,
                             GDK_SELECTION_PRIMARY,
			     targets, n_targets);
  gtk_selection_add_targets (buffer->selection_widget,
                             clipboard_atom,
			     targets, n_targets);
}

GtkTextBuffer*
gtk_text_buffer_new (GtkTextTagTable *table)
{
  GtkTextBuffer *text_buffer;

  /* This is broken, need construct_only argument for the tag table
     so language bindings can set it.
  */
  
  text_buffer = GTK_TEXT_BUFFER (gtk_type_new (gtk_text_buffer_get_type ()));

  if (table)
    text_buffer->tag_table = table;
  else
    text_buffer->tag_table = gtk_text_tag_table_new();

  gtk_object_ref (GTK_OBJECT(text_buffer->tag_table));
  gtk_object_sink (GTK_OBJECT(text_buffer->tag_table));
  
  text_buffer->tree = gtk_text_btree_new(text_buffer->tag_table,
                                         text_buffer);  
  
  return text_buffer;
}

static void
gtk_text_buffer_destroy (GtkObject *object)
{
  GtkTextBuffer *buffer;

  buffer = GTK_TEXT_BUFFER (object);

  gtk_widget_destroy(buffer->selection_widget);
  buffer->selection_widget = NULL;

  gtk_object_unref(GTK_OBJECT(buffer->tag_table));
  buffer->tag_table = NULL;
  
  gtk_text_btree_unref(buffer->tree);
  buffer->tree = NULL;
  
  (* parent_class->destroy) (object);
}

static void
gtk_text_buffer_finalize (GObject *object)
{
  GtkTextBuffer *tkxt_buffer;

  tkxt_buffer = GTK_TEXT_BUFFER (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*
 * Insertion
 */

static void
gtk_text_buffer_real_insert_text(GtkTextBuffer *buffer,
                                 GtkTextIter *iter,
                                 const gchar *text,
                                 gint len)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(iter != NULL);

  gtk_text_btree_insert(iter, text, len);
  
  gtk_signal_emit(GTK_OBJECT(buffer), signals[CHANGED]);

  gtk_text_buffer_set_modified(buffer, TRUE);
}

static void
gtk_text_buffer_emit_insert(GtkTextBuffer *buffer,
                            GtkTextIter *iter,
                            const gchar *text,
                            gint len)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(iter != NULL);
  g_return_if_fail(text != NULL);

  if (len < 0)
    len = strlen(text);
  
  if (len > 0)
    {
      gtk_signal_emit(GTK_OBJECT(buffer), signals[INSERT_TEXT],
                      iter, text, len);
    }
}

void
gtk_text_buffer_insert (GtkTextBuffer *buffer,
                        GtkTextIter *iter,
                        const gchar *text,
                        gint len)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(iter != NULL);
  g_return_if_fail(text != NULL);
  
  gtk_text_buffer_emit_insert(buffer, iter, text, len);
}

void
gtk_text_buffer_insert_at_cursor (GtkTextBuffer *buffer,
                                  const gchar *text,
                                  gint len)
{
  GtkTextIter iter;

  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(text != NULL);

  gtk_text_buffer_get_iter_at_mark(buffer, &iter,
                                   gtk_text_buffer_get_mark (buffer,
                                                             "insert"));

  gtk_text_buffer_insert(buffer, &iter, text, len);
}

void
gtk_text_buffer_insert_at_char         (GtkTextBuffer      *buffer,
                                        gint                 char_pos,
                                        const gchar         *text,
                                        gint                 len)
{
  GtkTextIter iter;

  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(text != NULL);
  
  gtk_text_buffer_get_iter_at_char(buffer, &iter, char_pos);

  gtk_text_buffer_insert(buffer, &iter, text, len);
}

void
gtk_text_buffer_insert_after_line(GtkTextBuffer *buffer,
                                  gint after_this_line,
                                  const gchar *text,
                                  gint len)
{
  GtkTextIter line;
  
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(text != NULL);
  
  gtk_text_buffer_get_iter_at_line(buffer,
                                   &line,
                                   after_this_line);

  /* Start of the next line */
  gtk_text_iter_forward_line(&line);

  gtk_text_buffer_insert(buffer, &line, text, len);
}

/*
 * Deletion
 */

static void
gtk_text_buffer_real_delete_text(GtkTextBuffer *buffer,
                                 GtkTextIter *start,
                                 GtkTextIter *end)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(start != NULL);
  g_return_if_fail(end != NULL);

  gtk_text_btree_delete(start, end);

  /* may have deleted the selection... */
  gtk_text_buffer_update_primary_selection(buffer);
  
  gtk_signal_emit(GTK_OBJECT(buffer), signals[CHANGED]);
  
  gtk_text_buffer_set_modified(buffer, TRUE);
}

static void
gtk_text_buffer_emit_delete(GtkTextBuffer *buffer,
                            GtkTextIter *start,
                            GtkTextIter *end)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(start != NULL);
  g_return_if_fail(end != NULL);

  if (gtk_text_iter_equal(start, end))
    return;

  gtk_signal_emit(GTK_OBJECT(buffer),
                  signals[DELETE_TEXT],
                  start, end);
}

void
gtk_text_buffer_delete (GtkTextBuffer *buffer,
                        GtkTextIter *start,
                        GtkTextIter *end)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(start != NULL);
  g_return_if_fail(end != NULL);
  
  gtk_text_buffer_emit_delete(buffer, start, end);
}

void
gtk_text_buffer_delete_chars (GtkTextBuffer      *buffer,
                              gint                 start_char,
                              gint                 end_char)
{
  GtkTextIter start;
  GtkTextIter end;
  
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  if (start_char == end_char)
    return;

  gtk_text_buffer_get_iter_at_char(buffer, &start, start_char);
  gtk_text_buffer_get_iter_at_char(buffer, &end, end_char);

  gtk_text_buffer_emit_delete(buffer, &start, &end);
}

void
gtk_text_buffer_delete_lines(GtkTextBuffer *buffer,
                             gint start_line,
                             gint end_line)
{
  GtkTextIter start;
  GtkTextIter end;
  
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  if (start_line == end_line)
    return;

  /* start of the start line */
  gtk_text_buffer_get_iter_at_line(buffer, &start, start_line);
  
  /* start of the end line; note that we don't delete the end_line, we
     want to delete up to the start of it */
  gtk_text_buffer_get_iter_at_line(buffer, &end, end_line);
  
  gtk_text_buffer_delete (buffer, &start, &end);
}

void
gtk_text_buffer_delete_from_line(GtkTextBuffer *buffer,
                                 gint line,
                                 gint start_char, gint end_char)
{
  GtkTextIter start;
  GtkTextIter end;
  
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  
  if (start_char == end_char)
    return;

  gtk_text_buffer_get_iter_at_line_char(buffer, &start, line, start_char);
  gtk_text_buffer_get_iter_at_line_char(buffer, &end, line, end_char);

  gtk_text_buffer_delete(buffer, &start, &end);
}

/*
 * Extracting textual buffer contents
 */

gchar*
gtk_text_buffer_get_text (GtkTextBuffer      *buffer,
                          const GtkTextIter *start,
                          const GtkTextIter *end,
                          gboolean             include_hidden_chars)
{
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), NULL);
  g_return_val_if_fail(start != NULL, NULL);
  g_return_val_if_fail(end != NULL, NULL);

  if (include_hidden_chars)
    return gtk_text_iter_get_text(start, end);
  else
    return gtk_text_iter_get_visible_text(start, end);
}

gchar*
gtk_text_buffer_get_text_chars         (GtkTextBuffer      *buffer,
                                        gint                 start_char,
                                        gint                 end_char,
                                        gboolean             include_hidden_chars)
{
  GtkTextIter start;
  GtkTextIter end;
  
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), NULL);
  
  if (start_char == end_char)
    return g_strdup("");
  
  gtk_text_buffer_get_iter_at_char (buffer, &start, start_char);
  gtk_text_buffer_get_iter_at_char (buffer, &end, end_char);
  
  return gtk_text_buffer_get_text(buffer, &start, &end,
                                  include_hidden_chars);
}

gchar*
gtk_text_buffer_get_text_from_line    (GtkTextBuffer      *buffer,
                                       gint                 line,
                                       gint                 start_char,
                                       gint                 end_char,
                                       gboolean             include_hidden_chars)
{
  GtkTextIter start;
  GtkTextIter end;
  
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), NULL);
  
  if (start_char == end_char)
    return g_strdup("");
  
  gtk_text_buffer_get_iter_at_line_char (buffer, &start, line, start_char);
  gtk_text_buffer_get_iter_at_line_char (buffer, &end, line, end_char);
  
  return gtk_text_buffer_get_text(buffer, &start, &end,
                                  include_hidden_chars);
}

gchar*
gtk_text_buffer_get_slice (GtkTextBuffer      *buffer,
                           const GtkTextIter *start,
                           const GtkTextIter *end,
                           gboolean             include_hidden_chars)
{
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), NULL);
  g_return_val_if_fail(start != NULL, NULL);
  g_return_val_if_fail(end != NULL, NULL);

  if (include_hidden_chars)
    return gtk_text_iter_get_slice(start, end);
  else
    return gtk_text_iter_get_visible_slice(start, end);
}

gchar*
gtk_text_buffer_get_slice_chars         (GtkTextBuffer      *buffer,
                                         gint                 start_char,
                                         gint                 end_char,
                                         gboolean             include_hidden_chars)
{
  GtkTextIter start;
  GtkTextIter end;
  
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), NULL);
  
  if (start_char == end_char)
    return g_strdup("");
  
  gtk_text_buffer_get_iter_at_char (buffer, &start, start_char);
  gtk_text_buffer_get_iter_at_char (buffer, &end, end_char);
  
  return gtk_text_buffer_get_slice(buffer, &start, &end,
                                   include_hidden_chars);
}

gchar*
gtk_text_buffer_get_slice_from_line    (GtkTextBuffer      *buffer,
                                        gint                 line,
                                        gint                 start_char,
                                        gint                 end_char,
                                        gboolean             include_hidden_chars)
{
  GtkTextIter start;
  GtkTextIter end;
  
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), NULL);
  
  if (start_char == end_char)
    return g_strdup("");
  
  gtk_text_buffer_get_iter_at_line_char (buffer, &start, line, start_char);
  gtk_text_buffer_get_iter_at_line_char (buffer, &end, line, end_char);
  
  return gtk_text_buffer_get_slice(buffer, &start, &end,
                                   include_hidden_chars);
}

/*
 * Pixmaps
 */

void
gtk_text_buffer_insert_pixmap         (GtkTextBuffer      *buffer,
                                       GtkTextIter *iter,
                                       GdkPixmap           *pixmap,
                                       GdkBitmap           *mask)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(iter != NULL);
  g_return_if_fail(pixmap != NULL);

  gtk_text_btree_insert_pixmap(iter, pixmap, mask);

  /* FIXME pixmap-specific signal like insert_text */
  
  gtk_signal_emit(GTK_OBJECT(buffer), signals[CHANGED]);
  
  gtk_text_buffer_set_modified(buffer, TRUE);
}

void
gtk_text_buffer_insert_pixmap_at_char (GtkTextBuffer      *buffer,
                                       gint                 char_pos,
                                       GdkPixmap           *pixmap,
                                       GdkBitmap           *mask)
{
  GtkTextIter iter;

  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(pixmap != NULL);
  
  gtk_text_buffer_get_iter_at_char(buffer, &iter, char_pos);

  gtk_text_buffer_insert_pixmap(buffer, &iter, pixmap, mask);
}

/*
 * Mark manipulation
 */

static void
gtk_text_buffer_mark_set (GtkTextBuffer     *buffer,
			  const GtkTextIter *location,
                          GtkTextMark       *mark)
{
  /* IMO this should NOT work like insert_text and delete_text,
     where the real action happens in the default handler.

     The reason is that the default handler would be _required_,
     i.e. the whole widget would start breaking and segfaulting
     if the default handler didn't get run. So you can't really
     override the default handler or stop the emission; that is,
     this signal is purely for notification, and not to allow users
     to modify the default behavior. */
  gtk_signal_emit(GTK_OBJECT(buffer),
                  signals[MARK_SET],
                  &location,
                  mark);

}

/**
 * gtk_text_buffer_set_mark:
 * @buffer:       a #GtkTextBuffer
 * @mark_name:    name of the mark
 * @iter:         location for the mark.
 * @left_gravity: if the mark is created by this function, gravity for the new
 *                mark.
 * @should_exist: if %TRUE, warn if the mark does not exist, and return
 *                immediately.
 * 
 * Move the mark to the given position, if not @should_exist, create the mark.
 * 
 * Return value: 
 **/
static GtkTextMark*
gtk_text_buffer_set_mark(GtkTextBuffer *buffer,
                         GtkTextMark *existing_mark,
                         const gchar *mark_name,
                         const GtkTextIter *iter,
                         gboolean left_gravity,
                         gboolean should_exist)
{
  GtkTextIter location;
  GtkTextMark *mark;
  
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), NULL);
  
  mark = gtk_text_btree_set_mark(buffer->tree,
                                 existing_mark,
                                 mark_name,
                                 left_gravity,
                                 iter,
                                 should_exist);

  if (gtk_text_btree_mark_is_insert(buffer->tree, mark) ||
      gtk_text_btree_mark_is_selection_bound(buffer->tree, mark))
    {
      gtk_text_buffer_update_primary_selection(buffer);
    }
  
  gtk_text_btree_get_iter_at_mark(buffer->tree,
                                  &location,
                                  mark);
  
  gtk_text_buffer_mark_set (buffer, &location, mark);

  return (GtkTextMark*)mark;
}

GtkTextMark*
gtk_text_buffer_create_mark(GtkTextBuffer *buffer,
                            const gchar *mark_name,
                            const GtkTextIter *where,
                            gboolean left_gravity)
{
  return gtk_text_buffer_set_mark(buffer, NULL, mark_name, where,
                                  left_gravity, FALSE);
}

void
gtk_text_buffer_move_mark(GtkTextBuffer *buffer,
                          GtkTextMark *mark,
                          const GtkTextIter *where)
{
  g_return_if_fail (mark != NULL);
  
  gtk_text_buffer_set_mark(buffer, mark, NULL, where, FALSE, TRUE);
}

void
gtk_text_buffer_get_iter_at_mark(GtkTextBuffer *buffer,
                                 GtkTextIter *iter,
                                 GtkTextMark *mark)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  gtk_text_btree_get_iter_at_mark(buffer->tree,
                                  iter,
                                  mark);
}

void
gtk_text_buffer_delete_mark(GtkTextBuffer *buffer,
                            GtkTextMark *mark)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  gtk_text_btree_remove_mark (buffer->tree, mark);

  /* See rationale above for MARK_SET on why we emit this after
     removing the mark, rather than removing the mark in a default
     handler. */
  gtk_signal_emit(GTK_OBJECT(buffer), signals[MARK_DELETED],
                  mark);
}

GtkTextMark*
gtk_text_buffer_get_mark (GtkTextBuffer      *buffer,
                          const gchar         *name)
{
  GtkTextMark *mark;

  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), NULL);
  g_return_val_if_fail(name != NULL, NULL);
  
  mark = gtk_text_btree_get_mark_by_name(buffer->tree, name);

  return mark;
}

void
gtk_text_buffer_place_cursor (GtkTextBuffer     *buffer,
                              const GtkTextIter *where)
{
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  
  gtk_text_btree_place_cursor(buffer->tree, where);
  gtk_text_buffer_mark_set (buffer, where,
                            gtk_text_buffer_get_mark (buffer,
                                                      "insert"));
  gtk_text_buffer_mark_set (buffer, where,
                            gtk_text_buffer_get_mark (buffer,
                                                      "selection_bound"));
}

/*
 * Tags
 */

GtkTextTag*
gtk_text_buffer_create_tag(GtkTextBuffer *buffer,
                           const gchar *tag_name)
{
  GtkTextTag *tag;
  
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), NULL);
  g_return_val_if_fail(tag_name != NULL, NULL);
  
  tag = gtk_text_tag_new(tag_name);

  gtk_text_tag_table_add(buffer->tag_table, tag);

  return tag;
}

static void
gtk_text_buffer_real_apply_tag (GtkTextBuffer *buffer,
                                GtkTextTag *tag,
                                const GtkTextIter *start,
                                const GtkTextIter *end)
{
  gtk_text_btree_tag(start, end, tag, TRUE);
}

static void
gtk_text_buffer_real_remove_tag (GtkTextBuffer *buffer,
                                 GtkTextTag *tag,
                                 const GtkTextIter *start,
                                 const GtkTextIter *end)
{
  gtk_text_btree_tag(start, end, tag, FALSE);
}


static void
gtk_text_buffer_emit_tag(GtkTextBuffer *buffer,
                         const gchar *name,
                         gboolean apply,
                         const GtkTextIter *start,
                         const GtkTextIter *end)
{
  GtkTextTag *tag;
  
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));
  g_return_if_fail(name != NULL);
  g_return_if_fail(start != NULL);
  g_return_if_fail(end != NULL);
  
  tag = gtk_text_tag_table_lookup(buffer->tag_table,
                                  name);

  if (tag == NULL)
    {
      g_warning("Unknown tag `%s'", name);
      return;
    }

  if (apply)
    gtk_signal_emit(GTK_OBJECT(buffer), signals[APPLY_TAG],
                    tag, start, end);
  else
    gtk_signal_emit(GTK_OBJECT(buffer), signals[REMOVE_TAG],
                    tag, start, end);
}


void
gtk_text_buffer_apply_tag(GtkTextBuffer *buffer,
                          const gchar *name,
                          const GtkTextIter *start,
                          const GtkTextIter *end)
{
  gtk_text_buffer_emit_tag(buffer, name, TRUE, start, end);
}

void
gtk_text_buffer_remove_tag(GtkTextBuffer *buffer,
                           const gchar *name,
                           const GtkTextIter *start,
                           const GtkTextIter *end)

{
  gtk_text_buffer_emit_tag(buffer, name, FALSE, start, end);
}

void
gtk_text_buffer_apply_tag_to_chars(GtkTextBuffer *buffer,
                                   const gchar *name,
                                   gint start_char, gint end_char)
{
  GtkTextIter start;
  GtkTextIter end;

  gtk_text_buffer_get_iter_at_char(buffer, &start, start_char);
  gtk_text_buffer_get_iter_at_char(buffer, &end, end_char);
  
  gtk_text_buffer_apply_tag(buffer, name, &start, &end);
}

void
gtk_text_buffer_remove_tag_from_chars(GtkTextBuffer *buffer,
                                      const gchar *name,
                                      gint start_char, gint end_char)
{
  GtkTextIter start;
  GtkTextIter end;

  gtk_text_buffer_get_iter_at_char(buffer, &start, start_char);
  gtk_text_buffer_get_iter_at_char(buffer, &end, end_char);
  
  gtk_text_buffer_remove_tag(buffer, name, &start, &end);
}

/*
 * Obtain various iterators
 */

void
gtk_text_buffer_get_iter_at_line_char    (GtkTextBuffer      *buffer,
                                          GtkTextIter        *iter,
                                          gint                 line_number,
                                          gint                 char_number)
{  
  g_return_if_fail(iter != NULL);
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  gtk_text_btree_get_iter_at_line_char(buffer->tree,
                                       iter, line_number, char_number);
}

void
gtk_text_buffer_get_iter_at_line    (GtkTextBuffer      *buffer,
                                     GtkTextIter        *iter,
                                     gint                 line_number)
{
  g_return_if_fail(iter != NULL);
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  gtk_text_buffer_get_iter_at_line_char(buffer, iter, line_number, 0);
}

void
gtk_text_buffer_get_iter_at_char         (GtkTextBuffer      *buffer,
                                          GtkTextIter        *iter,
                                          gint                 char_index)
{
  g_return_if_fail(iter != NULL);
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  gtk_text_btree_get_iter_at_char(buffer->tree, iter, char_index);
}

void
gtk_text_buffer_get_last_iter         (GtkTextBuffer      *buffer,
                                       GtkTextIter        *iter)
{  
  g_return_if_fail(iter != NULL);
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  gtk_text_btree_get_last_iter(buffer->tree, iter);
}

void
gtk_text_buffer_get_bounds (GtkTextBuffer *buffer,
                            GtkTextIter   *start,
                            GtkTextIter   *end)
{
  g_return_if_fail(start != NULL);
  g_return_if_fail(end != NULL);
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  gtk_text_btree_get_iter_at_char(buffer->tree, start, 0);
  gtk_text_btree_get_last_iter(buffer->tree, end);
}


gboolean
gtk_text_buffer_get_iter_from_string(GtkTextBuffer *buffer,
                                     GtkTextIter *iter,
                                     const gchar *index_string)
{
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), FALSE);

  return gtk_text_btree_get_iter_from_string(buffer->tree,
                                             iter,
                                             index_string);
}

/*
 * Modified flag
 */

gboolean
gtk_text_buffer_modified (GtkTextBuffer      *buffer)
{
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), FALSE);
  
  return buffer->modified;
}

void
gtk_text_buffer_set_modified (GtkTextBuffer      *buffer,
                              gboolean             setting)
{
  gboolean fixed_setting;
  
  g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

  fixed_setting = setting != FALSE;
  
  if (buffer->modified == fixed_setting)
    return;
  else
    {
      buffer->modified = fixed_setting;
      gtk_signal_emit(GTK_OBJECT(buffer), signals[MODIFIED_CHANGED]);
    }
}


/*
 * Clipboard
 */

static void
set_clipboard_contents_nocopy(GtkTextBuffer *buffer,
                              gchar *text)
{
  if (text && *text == '\0')
    {
      g_free(text);
      text = NULL;
    }
  
  if (buffer->clipboard_text != NULL)
    g_free(buffer->clipboard_text);  

  buffer->clipboard_text = text;
  
  gtk_text_buffer_update_clipboard_selection(buffer);
}

void
gtk_text_buffer_set_clipboard_contents (GtkTextBuffer      *buffer,
                                        const gchar         *text)
{
  set_clipboard_contents_nocopy(buffer, text ? g_strdup(text) : NULL);
}

const gchar*
gtk_text_buffer_get_clipboard_contents (GtkTextBuffer      *buffer)
{
  return buffer->clipboard_text;
}

/*
 * Assorted other stuff
 */

gint
gtk_text_buffer_get_line_count(GtkTextBuffer *buffer)
{
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), 0);
  
  return gtk_text_btree_line_count(buffer->tree);
}

gint
gtk_text_buffer_get_char_count(GtkTextBuffer *buffer)
{
  g_return_val_if_fail(GTK_IS_TEXT_BUFFER(buffer), 0);

  return gtk_text_btree_char_count(buffer->tree);
}

GSList*
gtk_text_buffer_get_tags               (GtkTextBuffer      *buffer,
                                        const GtkTextIter  *iter)
{
  GSList *retval = NULL;
  GtkTextTag** tags;
  gint count = 0;
  
  tags = gtk_text_btree_get_tags(iter, &count);

  if (count > 0)
    {
      gint i;

      gtk_text_tag_array_sort(tags, count);
      
      i = 0;
      while (i < count)
        {
          retval = g_slist_prepend(retval, tags[i]);
          ++i;
        }

      retval = g_slist_reverse(retval);
    }

  if (tags)
    g_free(tags);

  return retval;
}

/*
 * Selection
 */

static gboolean
have_primary_x_selection(GtkWidget *widget)
{
  return (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == 
          widget->window);
}

static gboolean
request_primary_x_selection(GtkWidget *widget, guint32 time)
{
  return gtk_selection_owner_set (widget, GDK_SELECTION_PRIMARY, time);
}

static gboolean
release_primary_x_selection(GtkWidget *widget, guint32 time)
{
  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == 
      widget->window)
    {
      gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, time);
      return TRUE;
    }
  else
    return FALSE;
}


static gboolean
have_clipboard_x_selection(GtkWidget *widget)
{
  return (gdk_selection_owner_get (clipboard_atom) == 
          widget->window);
}

static gboolean
request_clipboard_x_selection(GtkWidget *widget, guint32 time)
{
  return gtk_selection_owner_set (widget, clipboard_atom, time);
}

static gboolean
release_clipboard_x_selection(GtkWidget *widget, guint32 time)
{
  if (gdk_selection_owner_get (clipboard_atom) == 
      widget->window)
    {
      gtk_selection_owner_set (NULL, clipboard_atom, time);
      return TRUE;
    }
  else
    return FALSE;
}

/* Called when we lose the selection */
static gint
selection_clear_event(GtkWidget *widget, GdkEventSelection *event,
                      gpointer data)
{
  GtkTextBuffer *buffer;
  
  buffer = GTK_TEXT_BUFFER(data);

  /* Let the selection handling code know that the selection
   * has been changed, since we've overriden the default handler */
  if (!gtk_selection_clear (widget, event))
    return FALSE;

  buffer->have_selection = FALSE;
  
  if (event->selection == GDK_SELECTION_PRIMARY)
    {
      /* Move selection_bound to the insertion point */
      GtkTextIter insert;
      GtkTextIter selection_bound;

      gtk_text_buffer_get_iter_at_mark(buffer, &insert,
                                       gtk_text_buffer_get_mark (buffer, "insert"));
      gtk_text_buffer_get_iter_at_mark(buffer, &selection_bound,
                                       gtk_text_buffer_get_mark (buffer, "selection_bound"));
      
      if (!gtk_text_iter_equal(&insert, &selection_bound))
        gtk_text_buffer_move_mark(buffer,
                                  gtk_text_buffer_get_mark (buffer, "selection_bound"),
                                  &insert);
    }
  else if (event->selection == clipboard_atom)
    {
      gtk_text_buffer_set_clipboard_contents(buffer, NULL);
    }

  return TRUE;
}

/* Called when we have the selection and someone else wants our
   data in order to paste it */
static void
selection_get (GtkWidget         *widget,
               GtkSelectionData  *selection_data,
               guint              info,
               guint              time,
               gpointer data)
{
  GtkTextBuffer *buffer;
  gchar *str;
  guint length;
  
  buffer = GTK_TEXT_BUFFER(data);
  
  if (selection_data->selection == GDK_SELECTION_PRIMARY)
    {
      GtkTextIter start;
      GtkTextIter end;

      if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
        {
          /* Extract the selected text */
          str = gtk_text_iter_get_visible_text(&start, &end);
          
          length = strlen(str);
        }
      else
        return;
    }
  else
    {
      const gchar *cstr;
      
      cstr = gtk_text_buffer_get_clipboard_contents(buffer);

      if (cstr == NULL)
        return;

      str = g_strdup(cstr);
      
      length = strlen (str);
    }

  if (str)
    {
      if (info == TARGET_UTF8_STRING)
        {
          /* Pass raw UTF8 */
          gtk_selection_data_set (selection_data,
                                  utf8_atom,
                                  8*sizeof(gchar), (guchar *)str, length);

        }
      else if (info == TARGET_STRING ||
               info == TARGET_TEXT)
        {
          gchar *latin1;

          latin1 = gtk_text_utf_to_latin1(str, length);
          
          gtk_selection_data_set (selection_data,
                                  GDK_SELECTION_TYPE_STRING,
                                  8*sizeof(gchar), latin1, strlen(latin1));
          g_free(latin1);
        }
      else if (info == TARGET_COMPOUND_TEXT)
        {
          /* FIXME convert UTF8 directly to current locale, not via
             latin1 */
          
          guchar *text;
          GdkAtom encoding;
          gint format;
          gint new_length;
          gchar *latin1;

          latin1 = gtk_text_utf_to_latin1(str, length);
          
          gdk_string_to_compound_text (latin1, &encoding, &format, &text, &new_length);
          gtk_selection_data_set (selection_data, encoding, format, text, new_length);
          gdk_free_compound_text (text);

          g_free(latin1);
        }

      g_free (str);
    }
}

/* Called when we request a paste and receive the data */
static void
selection_received (GtkWidget        *widget,
                    GtkSelectionData *selection_data,
                    guint             time,
                    gpointer data)
{
  GtkTextBuffer *buffer;
  gboolean reselect;
  GtkTextIter insert_point;
  GtkTextMark *paste_point_override;
  enum {INVALID, STRING, CTEXT, UTF8} type;

  g_return_if_fail (widget != NULL);
  
  buffer = GTK_TEXT_BUFFER(data);

  if (selection_data->type == GDK_TARGET_STRING)
    type = STRING;
  else if (selection_data->type == ctext_atom)
    type = CTEXT;
  else if (selection_data->type == utf8_atom)
    type = UTF8;
  else
    type = INVALID;

  if (type == INVALID || selection_data->length < 0)
    {
      /* If we asked for UTF8 and didn't get it, try text; if we asked
         for text and didn't get it, try string.  If we asked for
         anything else and didn't get it, give up. */
      if (selection_data->target == utf8_atom)
        gtk_selection_convert (widget, selection_data->selection,
                               GDK_TARGET_STRING, time);
      return;
    }

  paste_point_override = gtk_text_buffer_get_mark (buffer,
                                                   "__paste_point_override");
  
  if (paste_point_override != NULL)
    {
      gtk_text_buffer_get_iter_at_mark(buffer, &insert_point,
                                       paste_point_override);
      gtk_text_buffer_delete_mark(buffer,
                                  gtk_text_buffer_get_mark (buffer,
                                                            "__paste_point_override"));
    }
  else
    {
      gtk_text_buffer_get_iter_at_mark(buffer, &insert_point,
                                       gtk_text_buffer_get_mark (buffer,
                                                                 "insert"));
    }
  
  reselect = FALSE;

  if ((TRUE/* Text is selected FIXME */) && 
      (!buffer->have_selection ||
       (selection_data->selection == clipboard_atom)))
    {
      reselect = TRUE;

      if (buffer->have_selection)
        {
          /* FIXME Delete currently-selected chars but don't give up X
             selection since we'll use the newly-pasted stuff as
             a new X selection */

        }
      else
        ; /* FIXME Delete selected chars and give up X selection */
    }

  switch (type)
    {
    case STRING:
      {
        gchar *utf;
        
        utf = gtk_text_latin1_to_utf((const gchar*)selection_data->data,
                                     selection_data->length);
        gtk_text_buffer_insert (buffer, &insert_point,
                                utf, -1);
        g_free(utf);
      }
      break;
      
    case UTF8:
      gtk_text_buffer_insert (buffer, &insert_point,
                              (const gchar *)selection_data->data,
                              selection_data->length);
      break;
      
    case CTEXT:
      {
	gchar **list;
	gint count;
	gint i;

	count = gdk_text_property_to_text_list (selection_data->type,
						selection_data->format, 
	      					selection_data->data,
						selection_data->length,
						&list);
	for (i=0; i<count; i++)
          {
            /* FIXME this is broken, it assumes the CTEXT is latin1
               when it probably isn't. */
            gchar *utf;

            utf = gtk_text_latin1_to_utf(list[i], strlen(list[i]));
            
            gtk_text_buffer_insert(buffer, &insert_point, utf, -1);

            g_free(utf);
          }

	if (count > 0)
	  gdk_free_text_list (list);
      }
      break;
      
    case INVALID:		/* quiet compiler */
      break;
    }

  if (reselect)
    ; /* FIXME Select the region of text we just inserted */

}

static void
ensure_handlers(GtkTextBuffer *buffer)
{
  if (!buffer->selection_handlers_installed)
    {
      buffer->selection_handlers_installed = TRUE;

      gtk_signal_connect(GTK_OBJECT(buffer->selection_widget),
                         "selection_clear_event",
                         GTK_SIGNAL_FUNC(selection_clear_event),
                         buffer);

      gtk_signal_connect(GTK_OBJECT(buffer->selection_widget),
                         "selection_received",
                         GTK_SIGNAL_FUNC(selection_received),
                         buffer);

      gtk_signal_connect(GTK_OBJECT(buffer->selection_widget),
                         "selection_get",
                         GTK_SIGNAL_FUNC(selection_get),
                         buffer);
    }
}

/* FIXME GDK_CURRENT_TIME should probably go away and we should
   figure out how to get the events in here */
static void
gtk_text_buffer_update_primary_selection(GtkTextBuffer *buffer)
{
  GtkTextIter start;
  GtkTextIter end;

  ensure_handlers(buffer);
  
  /* Determine whether we have a selection and adjust X selection
     accordingly. */
  
  if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
    {
      release_primary_x_selection(buffer->selection_widget, GDK_CURRENT_TIME);
      buffer->have_selection = FALSE;
    }
  else
    {
      /* Even if we already have the selection, we need to update our
         timestamp. */
      buffer->have_selection = FALSE;
      request_primary_x_selection(buffer->selection_widget, GDK_CURRENT_TIME);
      if (have_primary_x_selection(buffer->selection_widget))
        buffer->have_selection = TRUE;
    }
}

static void
gtk_text_buffer_update_clipboard_selection(GtkTextBuffer *buffer)
{ 
  if (buffer->clipboard_text == NULL ||
      buffer->clipboard_text[0] == '\0')
    {
      release_clipboard_x_selection(buffer->selection_widget, GDK_CURRENT_TIME);
    }
  else
    {
      /* Even if we already have the selection, we need to update our
         timestamp. */
      request_clipboard_x_selection(buffer->selection_widget, GDK_CURRENT_TIME);
    }
}

static void
paste(GtkTextBuffer *buffer, GdkAtom selection, guint32 time)
{
  ensure_handlers(buffer);
  
  gtk_selection_convert (buffer->selection_widget, selection,
                         utf8_atom, time);
}

void
gtk_text_buffer_paste_primary_selection(GtkTextBuffer      *buffer,
                                        GtkTextIter        *override_location,
                                        guint32 time)
{
  if (override_location != NULL)
    gtk_text_buffer_create_mark(buffer,
                                "__paste_point_override",
                                override_location, FALSE);
  
  paste(buffer, GDK_SELECTION_PRIMARY, time);
}

void
gtk_text_buffer_paste_clipboard        (GtkTextBuffer      *buffer,
                                        guint32              time)
{
  paste(buffer, clipboard_atom, time);
}

gboolean
gtk_text_buffer_delete_selection (GtkTextBuffer      *buffer)
{
  GtkTextIter start;
  GtkTextIter end;
  
  if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
    {
      return FALSE; /* No selection */
    }
  else
    {
      gtk_text_buffer_delete(buffer, &start, &end);
      gtk_text_buffer_update_primary_selection(buffer);
      return TRUE; /* We deleted stuff */
    }
}

static void
cut_or_copy(GtkTextBuffer *buffer,
            guint32 time,
            gboolean delete_region_after)
{
  /* We prefer to cut the selected region between selection_bound and
     insertion point. If that region is empty, then we cut the region
     between the "anchor" and the insertion point (this is for C-space
     and M-w and other Emacs-style copy/yank keys). Note that insert
     and selection_bound are guaranteed to exist, but the anchor only
     exists sometimes. */
  GtkTextIter start;
  GtkTextIter end;

  if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
    {
      /* Let's try the anchor thing */
      GtkTextMark * anchor = gtk_text_buffer_get_mark (buffer, "anchor");
      
      if (anchor == NULL)
        return;
      else
        {
          gtk_text_buffer_get_iter_at_mark(buffer, &end, anchor);
          gtk_text_iter_reorder(&start, &end);
        }
    }

  if (!gtk_text_iter_equal(&start, &end))
    {
      gchar *text;
      
      text = gtk_text_iter_get_visible_text(&start, &end);
      
      set_clipboard_contents_nocopy(buffer, text);
      
      if (delete_region_after)
        gtk_text_buffer_delete(buffer, &start, &end);
    }
}

void
gtk_text_buffer_cut (GtkTextBuffer      *buffer,
                     guint32              time)
{
  cut_or_copy(buffer, time, TRUE);
}

void
gtk_text_buffer_copy                   (GtkTextBuffer      *buffer,
                                        guint32              time)
{
  cut_or_copy(buffer, time, FALSE);
}


gboolean
gtk_text_buffer_get_selection_bounds   (GtkTextBuffer      *buffer,
                                        GtkTextIter        *start,
                                        GtkTextIter        *end)
{
  g_return_val_if_fail (buffer != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  return gtk_text_btree_get_selection_bounds (buffer->tree, start, end);
}


/*
 * Debug spew
 */

void
gtk_text_buffer_spew(GtkTextBuffer *buffer)
{
  gtk_text_btree_spew(buffer->tree);
}
