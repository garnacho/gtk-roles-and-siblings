#ifndef GTK_TEXT_TYPES_H
#define GTK_TEXT_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <glib.h>

#include <gtk/gtktextbuffer.h>
#include <gtk/gtktexttagprivate.h>


typedef struct _GtkTextCounter GtkTextCounter;
typedef struct _GtkTextLineSegment GtkTextLineSegment;
typedef struct _GtkTextLineSegmentClass GtkTextLineSegmentClass;
typedef struct _GtkTextToggleBody GtkTextToggleBody;
typedef struct _GtkTextViewSearch GtkTextViewSearch;
typedef struct _GtkTextTab GtkTextTab;
typedef struct _GtkTextViewStyle GtkTextViewStyle;
typedef struct _GtkTextMarkBody GtkTextMarkBody;

/*
 * Search
 */

/*
 * The data structure below is used for searching a B-tree for transitions
 * on a single tag (or for all tag transitions).  No code outside of
 * tkTextBTree.c should ever modify any of the fields in these structures,
 * but it's OK to use them for read-only information.
 */

struct _GtkTextViewSearch {
  GtkTextBTree *tree;
  
  GtkTextIter curIndex;		/* Position of last tag transition
					 * returned by gtk_text_btree_next_tag, or
					 * index of start of segment
					 * containing starting position for
					 * search if gtk_text_btree_next_tag hasn't
					 * been called yet, or same as
					 * stopIndex if search is over. */

  GtkTextLineSegment *segPtr;		/* Actual tag segment returned
                                           by last call to
                                           gtk_text_btree_next_tag,
                                           or NULL if 
                                           gtk_text_btree_next_tag
                                           hasn't returned  anything
                                           yet. */
  
  GtkTextLineSegment *lastPtr;		/* Stop search before just before
					 * considering this segment. */
  GtkTextTag *tag;			/* Tag to search for (or tag found, if
					 * allTags is non-zero). */
  int linesLeft;			/* Lines left to search (including
					 * curIndex and stopIndex).  When
					 * this becomes <= 0 the search is
					 * over. */
  int allTags;			/* Non-zero means ignore tag check:
                                 * search for transitions on all
                                 * tags. */
};

/*
 * The following data structure describes a single tab stop.
 */

typedef enum {
  GTK_TEXT_TAB_LEFT,
  GTK_TEXT_TAB_RIGHT,
  GTK_TEXT_TAB_CENTER,
  GTK_TEXT_TAB_NUMERIC
} GtkTextTabAlign;

struct _GtkTextTab {
    int location;			/* Offset in pixels of this tab stop
					 * from the left margin (lmargin2) of
					 * the text. */
    GtkTextTabAlign alignment;		/* Where the tab stop appears relative
					 * to the text. */
};

struct _GtkTextTabArray {
  guint refcount;
  int numTabs;			/* Number of tab stops. */
  GtkTextTab *tabs;
};

GtkTextTabArray *gtk_text_view_tab_array_new   (guint             size);
void              gtk_text_view_tab_array_ref   (GtkTextTabArray *tab_array);
void              gtk_text_view_tab_array_unref (GtkTextTabArray *tab_array);

/*
 * Declarations for variables shared among the text-related files:
 */

/* In gtktextbtree.c */
extern GtkTextLineSegmentClass gtk_text_char_type;
extern GtkTextLineSegmentClass gtk_text_toggle_on_type;
extern GtkTextLineSegmentClass gtk_text_toggle_off_type;

/* In gtktextmark.c */
extern GtkTextLineSegmentClass gtk_text_left_mark_type;
extern GtkTextLineSegmentClass gtk_text_right_mark_type;

/* In gtktextchild.c */
extern GtkTextLineSegmentClass gtk_text_pixmap_type;
extern GtkTextLineSegmentClass gtk_text_view_child_type;

/*
 * UTF 8 Stubs
 */

extern const gunichar gtk_text_unknown_char;
extern const gchar gtk_text_unknown_char_utf8[];

gboolean gtk_text_byte_begins_utf8_char (const gchar *byte);
guint    gtk_text_utf_to_latin1_char    (const gchar *p,
                                         guchar      *l1_ch);
gchar*   gtk_text_utf_to_latin1         (const gchar *p,
                                         gint         len);
gchar*   gtk_text_latin1_to_utf         (const gchar *latin1,
                                         gint         len);


gchar*   g_convert (const gchar *str,
                    gint         len,
                    const gchar *to_codeset,
                    const gchar *from_codeset,
                    gint        *bytes_converted);
       
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

