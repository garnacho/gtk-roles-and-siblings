#ifndef GTK_TEXT_BUFFER_H
#define GTK_TEXT_BUFFER_H

#include <gtk/gtkwidget.h>
#include <gtk/gtktexttagtable.h>
#include <gtk/gtktextiter.h>
#include <gtk/gtktextmark.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * This is the PUBLIC representation of a text buffer.
 * GtkTextBTree is the PRIVATE internal representation of it.
 */

typedef struct _GtkTextBTree GtkTextBTree;

#define GTK_TYPE_TEXT_BUFFER            (gtk_text_buffer_get_type())
#define GTK_TEXT_BUFFER(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_TEXT_BUFFER, GtkTextBuffer))
#define GTK_TEXT_BUFFER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_BUFFER, GtkTextBufferClass))
#define GTK_IS_TEXT_BUFFER(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_TEXT_BUFFER))
#define GTK_IS_TEXT_BUFFER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_BUFFER))
#define GTK_TEXT_BUFFER_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TEXT_BUFFER, GtkTextBufferClass))

typedef struct _GtkTextBufferClass GtkTextBufferClass;

struct _GtkTextBuffer {
  GtkObject parent_instance;

  GtkTextTagTable *tag_table;
  GtkTextBTree *btree;

  /* Whether the buffer has been modified since last save */
  gboolean modified;
};

struct _GtkTextBufferClass {
  GtkObjectClass parent_class;

  void (* insert_text)     (GtkTextBuffer *buffer,
                            GtkTextIter *pos,
                            const gchar *text,
                            gint length,
                            gboolean interactive);


  void (* delete_text)     (GtkTextBuffer *buffer,
                            GtkTextIter *start,
                            GtkTextIter *end,
                            gboolean interactive);

  /* Only for text changed, marks/tags don't cause this
     to be emitted */
  void (* changed)         (GtkTextBuffer *buffer);


  /* New value for the modified flag */
  void (* modified_changed)   (GtkTextBuffer *buffer);

  /* Mark moved or created */
  void (* mark_set)           (GtkTextBuffer *buffer,
                               const GtkTextIter *location,
                               GtkTextMark *mark);

  void (* mark_deleted)       (GtkTextBuffer *buffer,
                               GtkTextMark *mark);

  void (* apply_tag)          (GtkTextBuffer *buffer,
                               GtkTextTag *tag,
                               const GtkTextIter *start_char,
                               const GtkTextIter *end_char);

  void (* remove_tag)         (GtkTextBuffer *buffer,
                               GtkTextTag *tag,
                               const GtkTextIter *start_char,
                               const GtkTextIter *end_char);

};

GtkType        gtk_text_buffer_get_type       (void) G_GNUC_CONST;



/* table is NULL to create a new one */
GtkTextBuffer *gtk_text_buffer_new            (GtkTextTagTable *table);
gint           gtk_text_buffer_get_line_count (GtkTextBuffer   *buffer);
gint           gtk_text_buffer_get_char_count (GtkTextBuffer   *buffer);


GtkTextTagTable* gtk_text_buffer_get_tag_table (GtkTextBuffer  *buffer);

/* Delete whole buffer, then insert */
void gtk_text_buffer_set_text          (GtkTextBuffer *buffer,
                                        const gchar   *text,
                                        gint           len);

/* Insert into the buffer */
void gtk_text_buffer_insert            (GtkTextBuffer *buffer,
                                        GtkTextIter   *iter,
                                        const gchar   *text,
                                        gint           len);
void gtk_text_buffer_insert_at_cursor  (GtkTextBuffer *buffer,
                                        const gchar   *text,
                                        gint           len);

gboolean gtk_text_buffer_insert_interactive           (GtkTextBuffer *buffer,
                                                       GtkTextIter   *iter,
                                                       const gchar   *text,
                                                       gint           len,
                                                       gboolean       default_editable);
gboolean gtk_text_buffer_insert_interactive_at_cursor (GtkTextBuffer *buffer,
                                                       const gchar   *text,
                                                       gint           len,
                                                       gboolean       default_editable);

void     gtk_text_buffer_insert_range             (GtkTextBuffer     *buffer,
                                                   GtkTextIter       *iter,
                                                   const GtkTextIter *start,
                                                   const GtkTextIter *end);
gboolean gtk_text_buffer_insert_range_interactive (GtkTextBuffer     *buffer,
                                                   GtkTextIter       *iter,
                                                   const GtkTextIter *start,
                                                   const GtkTextIter *end,
                                                   gboolean           default_editable);

void    gtk_text_buffer_insert_with_tags          (GtkTextBuffer     *buffer,
                                                   GtkTextIter       *iter,
                                                   const gchar       *text,
                                                   gint               len,
                                                   GtkTextTag        *first_tag,
                                                   ...);

void    gtk_text_buffer_insert_with_tags_by_name  (GtkTextBuffer     *buffer,
                                                   GtkTextIter       *iter,
                                                   const gchar       *text,
                                                   gint               len,
                                                   const gchar       *first_tag_name,
                                                   ...);

/* Delete from the buffer */
void     gtk_text_buffer_delete             (GtkTextBuffer *buffer,
                                             GtkTextIter   *start,
                                             GtkTextIter   *end);
gboolean gtk_text_buffer_delete_interactive (GtkTextBuffer *buffer,
                                             GtkTextIter   *start_iter,
                                             GtkTextIter   *end_iter,
                                             gboolean       default_editable);



/* Obtain strings from the buffer */
gchar          *gtk_text_buffer_get_text            (GtkTextBuffer     *buffer,
                                                     const GtkTextIter *start,
                                                     const GtkTextIter *end,
                                                     gboolean           include_hidden_chars);

gchar          *gtk_text_buffer_get_slice           (GtkTextBuffer     *buffer,
                                                     const GtkTextIter *start,
                                                     const GtkTextIter *end,
                                                     gboolean           include_hidden_chars);

/* Insert a pixbuf */
void gtk_text_buffer_insert_pixbuf         (GtkTextBuffer *buffer,
                                            GtkTextIter   *iter,
                                            GdkPixbuf     *pixbuf);

/* Mark manipulation */
GtkTextMark   *gtk_text_buffer_create_mark (GtkTextBuffer     *buffer,
                                            const gchar       *mark_name,
                                            const GtkTextIter *where,
                                            gboolean           left_gravity);
void           gtk_text_buffer_move_mark   (GtkTextBuffer     *buffer,
                                            GtkTextMark       *mark,
                                            const GtkTextIter *where);
void           gtk_text_buffer_delete_mark (GtkTextBuffer     *buffer,
                                            GtkTextMark       *mark);
GtkTextMark*   gtk_text_buffer_get_mark    (GtkTextBuffer     *buffer,
                                            const gchar       *name);

void gtk_text_buffer_move_mark_by_name   (GtkTextBuffer     *buffer,
                                          const gchar       *name,
                                          const GtkTextIter *where);
void gtk_text_buffer_delete_mark_by_name (GtkTextBuffer     *buffer,
                                          const gchar       *name);

GtkTextMark* gtk_text_buffer_get_insert          (GtkTextBuffer *buffer);
GtkTextMark* gtk_text_buffer_get_selection_bound (GtkTextBuffer *buffer);


/* efficiently move insert and selection_bound to same location */
void gtk_text_buffer_place_cursor (GtkTextBuffer     *buffer,
                                   const GtkTextIter *where);



/* Tag manipulation */
void gtk_text_buffer_apply_tag             (GtkTextBuffer     *buffer,
                                            GtkTextTag        *tag,
                                            const GtkTextIter *start_index,
                                            const GtkTextIter *end_index);
void gtk_text_buffer_remove_tag            (GtkTextBuffer     *buffer,
                                            GtkTextTag        *tag,
                                            const GtkTextIter *start_index,
                                            const GtkTextIter *end_index);
void gtk_text_buffer_apply_tag_by_name     (GtkTextBuffer     *buffer,
                                            const gchar       *name,
                                            const GtkTextIter *start_index,
                                            const GtkTextIter *end_index);
void gtk_text_buffer_remove_tag_by_name    (GtkTextBuffer     *buffer,
                                            const gchar       *name,
                                            const GtkTextIter *start_index,
                                            const GtkTextIter *end_index);


/* You can either ignore the return value, or use it to
 * set the attributes of the tag. tag_name can be NULL
 */
GtkTextTag    *gtk_text_buffer_create_tag (GtkTextBuffer *buffer,
                                           const gchar   *tag_name);

/* Obtain iterators pointed at various places, then you can move the
   iterator around using the GtkTextIter operators */
void gtk_text_buffer_get_iter_at_line_offset (GtkTextBuffer *buffer,
                                              GtkTextIter   *iter,
                                              gint           line_number,
                                              gint           char_offset);
void gtk_text_buffer_get_iter_at_offset      (GtkTextBuffer *buffer,
                                              GtkTextIter   *iter,
                                              gint           char_offset);
void gtk_text_buffer_get_iter_at_line        (GtkTextBuffer *buffer,
                                              GtkTextIter   *iter,
                                              gint           line_number);
void gtk_text_buffer_get_last_iter           (GtkTextBuffer *buffer,
                                              GtkTextIter   *iter);
void gtk_text_buffer_get_bounds              (GtkTextBuffer *buffer,
                                              GtkTextIter   *start,
                                              GtkTextIter   *end);
void gtk_text_buffer_get_iter_at_mark        (GtkTextBuffer *buffer,
                                              GtkTextIter   *iter,
                                              GtkTextMark   *mark);



/* There's no get_first_iter because you just get the iter for
   line or char 0 */

GSList         *gtk_text_buffer_get_tags (GtkTextBuffer     *buffer,
                                          const GtkTextIter *iter);


/* Used to keep track of whether the buffer needs saving; anytime the
   buffer contents change, the modified flag is turned on. Whenever
   you save, turn it off. Tags and marks do not affect the modified
   flag, but if you would like them to you can connect a handler to
   the tag/mark signals and call set_modified in your handler */

gboolean        gtk_text_buffer_modified                (GtkTextBuffer *buffer);
void            gtk_text_buffer_set_modified            (GtkTextBuffer *buffer,
                                                         gboolean       setting);

void            gtk_text_buffer_paste_primary           (GtkTextBuffer *buffer,
                                                         GtkTextIter   *override_location,
                                                         gboolean       default_editable);
void            gtk_text_buffer_cut_clipboard           (GtkTextBuffer *buffer,
                                                         gboolean       default_editable);
void            gtk_text_buffer_copy_clipboard          (GtkTextBuffer *buffer);
void            gtk_text_buffer_paste_clipboard         (GtkTextBuffer *buffer,
                                                         gboolean       default_editable);

gboolean        gtk_text_buffer_get_selection_bounds    (GtkTextBuffer *buffer,
                                                         GtkTextIter   *start,
                                                         GtkTextIter   *end);
gboolean        gtk_text_buffer_delete_selection        (GtkTextBuffer *buffer,
                                                         gboolean       interactive,
                                                         gboolean       default_editable);

/* INTERNAL private stuff */
void            _gtk_text_buffer_spew                  (GtkTextBuffer      *buffer);

GtkTextBTree*   _gtk_text_buffer_get_btree             (GtkTextBuffer      *buffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
