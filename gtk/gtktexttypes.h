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
typedef struct _GtkTextMarkBody GtkTextMarkBody;

/*
 * Declarations for variables shared among the text-related files:
 */

#ifdef G_OS_WIN32
#ifdef GTK_COMPILATION
#define VARIABLE __declspec(dllexport)
#else
#define VARIABLE extern __declspec(dllimport)
#endif
#else
#define VARIABLE extern
#endif

/* In gtktextbtree.c */
extern GtkTextLineSegmentClass gtk_text_char_type;
extern GtkTextLineSegmentClass gtk_text_toggle_on_type;
extern GtkTextLineSegmentClass gtk_text_toggle_off_type;

/* In gtktextmark.c */
extern GtkTextLineSegmentClass gtk_text_left_mark_type;
extern GtkTextLineSegmentClass gtk_text_right_mark_type;

/* In gtktextchild.c */
extern GtkTextLineSegmentClass gtk_text_pixbuf_type;
extern GtkTextLineSegmentClass gtk_text_child_type;

/*
 * UTF 8 Stubs
 */

#define GTK_TEXT_UNKNOWN_CHAR 0xFFFC
VARIABLE const gchar gtk_text_unknown_char_utf8[];

gboolean gtk_text_byte_begins_utf8_char (const gchar *byte);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

