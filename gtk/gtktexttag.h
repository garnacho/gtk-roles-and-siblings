#ifndef GTK_TEXT_TAG_H
#define GTK_TEXT_TAG_H

#include <gtk/gtkobject.h>
#include <gdk/gdk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkTextIter GtkTextIter;
typedef struct _GtkTextBTreeNode GtkTextBTreeNode;
typedef struct _GtkTextTagTable GtkTextTagTable;

typedef enum
{
  GTK_WRAPMODE_NONE,
  GTK_WRAPMODE_CHAR,
  GTK_WRAPMODE_WORD
} GtkWrapMode;

typedef struct _GtkTextAttributes GtkTextAttributes;

#define GTK_TYPE_TEXT_TAG            (gtk_text_tag_get_type())
#define GTK_TEXT_TAG(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_TEXT_TAG, GtkTextTag))
#define GTK_TEXT_TAG_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_TAG, GtkTextTagClass))
#define GTK_IS_TEXT_TAG(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_TEXT_TAG))
#define GTK_IS_TEXT_TAG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_TAG))
#define GTK_TEXT_TAG_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_TEXT_TAG, GtkTextTagClass))

typedef struct _GtkTextTag GtkTextTag;
typedef struct _GtkTextTagClass GtkTextTagClass;

struct _GtkTextTag
{
  GtkObject parent_instance;

  GtkTextTagTable *table;
  
  char *name;			/* Name of this tag.  This field is actually
				 * a pointer to the key from the entry in
				 * tkxt->tagTable, so it needn't be freed
				 * explicitly. */
  int priority;		/* Priority of this tag within widget.  0
                         * means lowest priority.  Exactly one tag
                         * has each integer value between 0 and
                         * numTags-1. */
  /*
   * Information for displaying text with this tag.  The information
   * belows acts as an override on information specified by lower-priority
   * tags.  If no value is specified, then the next-lower-priority tag
   * on the text determins the value.  The text widget itself provides
   * defaults if no tag specifies an override.
   */

  GtkTextAttributes *values;  
  
  /* Flags for whether a given value is set; if a value is unset, then
   * this tag does not affect it.
   */
  guint bg_color_set : 1;
  guint bg_stipple_set : 1;
  guint fg_color_set : 1;
  guint font_set : 1;
  guint fg_stipple_set : 1;
  guint justify_set : 1;
  guint left_margin_set : 1;
  guint left_wrapped_line_margin_set : 1;
  guint offset_set : 1;
  guint strikethrough_set : 1;
  guint right_margin_set : 1;
  guint pixels_above_lines_set : 1;
  guint pixels_below_lines_set : 1;
  guint pixels_inside_wrap_set : 1;
  guint tabs_set : 1;
  guint underline_set : 1;
  guint wrap_mode_set : 1;
  guint bg_full_height_set : 1;
  guint invisible_set : 1;
  guint editable_set : 1;
  guint language_set : 1;
  guint pad1 : 1;
  guint pad2 : 1;
  guint pad3 : 1;
};

struct _GtkTextTagClass {
  GtkObjectClass parent_class;

  gint (* event) (GtkTextTag *tag,
                  GtkObject *event_object,         /* widget, canvas item, whatever */
                  GdkEvent *event,                 /* the event itself */
                  const GtkTextIter *iter);        /* location of event in buffer */
};

GtkType      gtk_text_tag_get_type     (void) G_GNUC_CONST;
GtkTextTag  *gtk_text_tag_new          (const gchar       *name);
gint         gtk_text_tag_get_priority (GtkTextTag        *tag);
void         gtk_text_tag_set_priority (GtkTextTag        *tag,
					gint               priority);
gint         gtk_text_tag_event        (GtkTextTag        *tag,
					GtkObject         *event_object,
					GdkEvent          *event,
					const GtkTextIter *iter);

/*
 * Style object created by folding a set of tags together
 */

typedef struct _GtkTextAppearance GtkTextAppearance;

struct _GtkTextAppearance
{
  GdkColor bg_color;
  GdkColor fg_color;
  GdkBitmap *bg_stipple;
  GdkBitmap *fg_stipple;

  guint underline : 4;		/* PangoUnderline */
  guint strikethrough : 1;

  /* Whether to use background-related values; this is irrelevant for
   * the values struct when in a tag, but is used for the composite
   * values struct; it's true if any of the tags being composited
   * had background stuff set.
   */
  guint draw_bg : 1;

  /* This is only used when we are actually laying out and rendering
   * a paragraph; not when a GtkTextAppearance is part of a
   * GtkTextAttributes.
   */
  guint inside_selection : 1;
};

struct _GtkTextAttributes
{
  guint refcount;

  GtkTextAppearance appearance;
  
  GtkJustification justify;
  GtkTextDirection direction;
  
  PangoFontDescription *font_desc;
  
  /* lMargin1 */
  gint left_margin;
  
  /* lMargin2 */
  gint left_wrapped_line_margin;

  /* super/subscript offset, can be negative */
  gint offset;
  
  gint right_margin;

  gint pixels_above_lines;

  gint pixels_below_lines;

  gint pixels_inside_wrap;

  PangoTabArray *tabs;
  
  GtkWrapMode wrap_mode;	/* How to handle wrap-around for this tag.
				 * Must be GTK_WRAPMODE_CHAR,
				 * GTK_WRAPMODE_NONE, GTK_WRAPMODE_WORD
                                 */

  gchar *language;
  
  /* hide the text  */
  guint invisible : 1;

  /* Background is fit to full line height rather than
   * baseline +/- ascent/descent (font height)
   */
  guint bg_full_height : 1;
  
  /* can edit this text */
  guint editable : 1;

  /* colors are allocated etc. */
  guint realized : 1;

  guint pad1 : 1;
  guint pad2 : 1;
  guint pad3 : 1;
  guint pad4 : 1;
};

GtkTextAttributes  *gtk_text_attributes_new       (void);
void                gtk_text_attributes_copy      (GtkTextAttributes *src,
                                                   GtkTextAttributes *dest);
void                gtk_text_attributes_unref     (GtkTextAttributes *values);
void                gtk_text_attributes_ref       (GtkTextAttributes *values);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

