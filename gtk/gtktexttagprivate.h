#ifndef GTK_TEXT_TAG_PRIVATE_H
#define GTK_TEXT_TAG_PRIVATE_H

#include <gtk/gtktexttag.h>

typedef struct _GtkTextBTreeNode GtkTextBTreeNode;

/* values should already have desired defaults; this function will override
 * the defaults with settings in the given tags, which should be sorted in
 * ascending order of priority
*/
void _gtk_text_attributes_fill_from_tags   (GtkTextAttributes   *values,
                                            GtkTextTag         **tags,
                                            guint                n_tags);
void _gtk_text_tag_array_sort              (GtkTextTag         **tag_array_p,
                                            guint                len);

/* ensure colors are allocated, etc. for drawing */
void                _gtk_text_attributes_realize   (GtkTextAttributes *values,
                                                    GdkColormap       *cmap,
                                                    GdkVisual         *visual);

/* free the stuff again */
void                _gtk_text_attributes_unrealize (GtkTextAttributes *values,
                                                    GdkColormap       *cmap,
                                                    GdkVisual         *visual);

gboolean _gtk_text_tag_affects_size               (GtkTextTag *tag);
gboolean _gtk_text_tag_affects_nonsize_appearance (GtkTextTag *tag);

#endif
