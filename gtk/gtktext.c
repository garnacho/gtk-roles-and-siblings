/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <ctype.h>
#include <string.h>
#include "gdk/gdkkeysyms.h"
#ifdef USE_XIM
#include "gdk/gdkx.h"
#endif
#include "gtkmain.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtktext.h"
#include "line-wrap.xbm"
#include "line-arrow.xbm"


#define INITIAL_BUFFER_SIZE      1024
#define INITIAL_LINE_CACHE_SIZE  256
#define MIN_GAP_SIZE             256
#define LINE_DELIM               '\n'
#define MIN_TEXT_WIDTH_LINES     20
#define MIN_TEXT_HEIGHT_LINES    10
#define TEXT_BORDER_ROOM         1
#define LINE_WRAP_ROOM           8           /* The bitmaps are 6 wide. */
#define DEFAULT_TAB_STOP_WIDTH   4
#define SCROLL_PIXELS            5
#define KEY_SCROLL_PIXELS        10
#define SCROLL_TIME              100
#define FREEZE_LENGTH            1024        
/* Freeze text when inserting or deleting more than this many characters */

#define SET_PROPERTY_MARK(m, p, o)  do {                   \
                                      (m)->property = (p); \
			              (m)->offset = (o);   \
			            } while (0)
#define MARK_CURRENT_PROPERTY(mark) ((TextProperty*)(mark)->property->data)
#define MARK_NEXT_PROPERTY(mark)    ((TextProperty*)(mark)->property->next->data)
#define MARK_PREV_PROPERTY(mark)    ((TextProperty*)((mark)->property->prev ?     \
						     (mark)->property->prev->data \
						     : NULL))
#define MARK_PREV_LIST_PTR(mark)    ((mark)->property->prev)
#define MARK_LIST_PTR(mark)         ((mark)->property)
#define MARK_NEXT_LIST_PTR(mark)    ((mark)->property->next)
#define MARK_OFFSET(mark)           ((mark)->offset)
#define MARK_PROPERTY_LENGTH(mark)  (MARK_CURRENT_PROPERTY(mark)->length)


#define MARK_CURRENT_FONT(text, mark) \
  ((MARK_CURRENT_PROPERTY(mark)->flags & PROPERTY_FONT) ? \
         MARK_CURRENT_PROPERTY(mark)->font->gdk_font : \
         GTK_WIDGET (text)->style->font)
#define MARK_CURRENT_FORE(text, mark) \
  ((MARK_CURRENT_PROPERTY(mark)->flags & PROPERTY_FOREGROUND) ? \
         &MARK_CURRENT_PROPERTY(mark)->fore_color : \
         &((GtkWidget *)text)->style->text[((GtkWidget *)text)->state])
#define MARK_CURRENT_BACK(text, mark) \
  ((MARK_CURRENT_PROPERTY(mark)->flags & PROPERTY_BACKGROUND) ? \
         &MARK_CURRENT_PROPERTY(mark)->back_color : \
         &((GtkWidget *)text)->style->base[((GtkWidget *)text)->state])
#define MARK_CURRENT_TEXT_FONT(text, mark) \
  ((MARK_CURRENT_PROPERTY(mark)->flags & PROPERTY_FONT) ? \
         MARK_CURRENT_PROPERTY(mark)->font : \
         text->current_font)

#define TEXT_LENGTH(t)              ((t)->text_end - (t)->gap_size)
#define FONT_HEIGHT(f)              ((f)->ascent + (f)->descent)
#define LINE_HEIGHT(l)              ((l).font_ascent + (l).font_descent)
#define LINE_CONTAINS(l, i)         ((l).start.index <= (i) && (l).end.index >= (i))
#define LINE_STARTS_AT(l, i)        ((l).start.index == (i))
#define LINE_START_PIXEL(l)         ((l).tab_cont.pixel_offset)
#define LAST_INDEX(t, m)            ((m).index == TEXT_LENGTH(t))
#define CACHE_DATA(c)               (*(LineParams*)(c)->data)

typedef struct _TextProperty          TextProperty;
typedef struct _TabStopMark           TabStopMark;
typedef struct _PrevTabCont           PrevTabCont;
typedef struct _FetchLinesData        FetchLinesData;
typedef struct _LineParams            LineParams;
typedef struct _SetVerticalScrollData SetVerticalScrollData;

typedef gint (*LineIteratorFunction) (GtkText* text, LineParams* lp, void* data);

typedef enum
{
  FetchLinesPixels,
  FetchLinesCount
} FLType;

struct _SetVerticalScrollData {
  gint pixel_height;
  gint last_didnt_wrap;
  gint last_line_start;
  GtkPropertyMark mark;
};

struct _GtkTextFont
{
  /* The actual font. */
  GdkFont *gdk_font;
  guint ref_count;

  gint16 char_widths[256];
};

typedef enum {
  PROPERTY_FONT =       1 << 0,
  PROPERTY_FOREGROUND = 1 << 1,
  PROPERTY_BACKGROUND = 1 << 2
} TextPropertyFlags;

struct _TextProperty
{
  /* Font. */
  GtkTextFont* font;

  /* Background Color. */
  GdkColor back_color;
  
  /* Foreground Color. */
  GdkColor fore_color;

  /* Show which properties are set */
  TextPropertyFlags flags;

  /* Length of this property. */
  guint length;
};

struct _TabStopMark
{
  GList* tab_stops; /* Index into list containing the next tab position.  If
		     * NULL, using default widths. */
  gint to_next_tab;
};

struct _PrevTabCont
{
  guint pixel_offset;
  TabStopMark tab_start;
};

struct _FetchLinesData
{
  GList* new_lines;
  FLType fl_type;
  gint data;
  gint data_max;
};

struct _LineParams
{
  guint font_ascent;
  guint font_descent;
  guint pixel_width;
  guint displayable_chars;
  guint wraps : 1;
  
  PrevTabCont tab_cont;
  PrevTabCont tab_cont_next;
  
  GtkPropertyMark start;
  GtkPropertyMark end;
};


static void  gtk_text_class_init     (GtkTextClass   *klass);
static void  gtk_text_init           (GtkText        *text);
static void  gtk_text_destroy        (GtkObject      *object);
static void  gtk_text_finalize       (GtkObject      *object);
static void  gtk_text_realize        (GtkWidget      *widget);
static void  gtk_text_unrealize      (GtkWidget      *widget);
static void  gtk_text_style_set	     (GtkWidget      *widget,
				      GtkStyle       *previous_style);
static void  gtk_text_draw_focus     (GtkWidget      *widget);
static void  gtk_text_size_request   (GtkWidget      *widget,
				      GtkRequisition *requisition);
static void  gtk_text_size_allocate  (GtkWidget      *widget,
				      GtkAllocation  *allocation);
static void  gtk_text_adjustment     (GtkAdjustment  *adjustment,
				      GtkText        *text);
static void  gtk_text_disconnect     (GtkAdjustment  *adjustment,
				      GtkText        *text);

static void gtk_text_insert_text       (GtkEditable       *editable,
					const gchar       *new_text,
					gint               new_text_length,
					gint               *position);
static void gtk_text_delete_text       (GtkEditable        *editable,
					gint               start_pos,
					gint               end_pos);
static void gtk_text_update_text       (GtkEditable       *editable,
					gint               start_pos,
					gint               end_pos);
static gchar *gtk_text_get_chars       (GtkEditable       *editable,
					gint               start,
					gint               end);
static void gtk_text_set_selection     (GtkEditable       *editable,
					gint               start,
					gint               end);
static void gtk_text_real_set_editable (GtkEditable       *editable,
					gboolean           is_editable);

/* Event handlers */
static void  gtk_text_draw              (GtkWidget         *widget,
					 GdkRectangle      *area);
static gint  gtk_text_expose            (GtkWidget         *widget,
					 GdkEventExpose    *event);
static gint  gtk_text_button_press      (GtkWidget         *widget,
					 GdkEventButton    *event);
static gint  gtk_text_button_release    (GtkWidget         *widget,
					 GdkEventButton    *event);
static gint  gtk_text_motion_notify     (GtkWidget         *widget,
					 GdkEventMotion    *event);
static gint  gtk_text_key_press         (GtkWidget         *widget,
					 GdkEventKey       *event);
static gint  gtk_text_focus_in          (GtkWidget         *widget,
					 GdkEventFocus     *event);
static gint  gtk_text_focus_out         (GtkWidget         *widget,
				         GdkEventFocus     *event);

static void move_gap_to_point (GtkText* text);
static void make_forward_space (GtkText* text, guint len);

/* Property management */
static GtkTextFont* get_text_font (GdkFont* gfont);
static void         text_font_unref (GtkTextFont *text_font);

static void insert_text_property (GtkText* text, GdkFont* font,
				  GdkColor *fore, GdkColor* back, guint len);
static TextProperty* new_text_property (GtkText *text, GdkFont* font, 
					GdkColor* fore, GdkColor* back, guint length);
static void destroy_text_property (TextProperty *prop);
static void init_properties      (GtkText *text);
static void realize_property     (GtkText *text, TextProperty *prop);
static void realize_properties   (GtkText *text);
static void unrealize_property   (GtkText *text, TextProperty *prop);
static void unrealize_properties (GtkText *text);

static void delete_text_property (GtkText* text, guint len);

static guint pixel_height_of (GtkText* text, GList* cache_line);

/* Property Movement and Size Computations */
static void advance_mark (GtkPropertyMark* mark);
static void decrement_mark (GtkPropertyMark* mark);
static void advance_mark_n (GtkPropertyMark* mark, gint n);
static void decrement_mark_n (GtkPropertyMark* mark, gint n);
static void move_mark_n (GtkPropertyMark* mark, gint n);
static GtkPropertyMark find_mark (GtkText* text, guint mark_position);
static GtkPropertyMark find_mark_near (GtkText* text, guint mark_position, const GtkPropertyMark* near);
static void find_line_containing_point (GtkText* text, guint point,
					gboolean scroll);

/* Display */
static void compute_lines_pixels (GtkText* text, guint char_count,
				  guint *lines, guint *pixels);

static gint total_line_height (GtkText* text,
			       GList* line,
			       gint line_count);
static LineParams find_line_params (GtkText* text,
				    const GtkPropertyMark *mark,
				    const PrevTabCont *tab_cont,
				    PrevTabCont *next_cont);
static void recompute_geometry (GtkText* text);
static void insert_expose (GtkText* text, guint old_pixels, gint nchars, guint new_line_count);
static void delete_expose (GtkText* text,
			   guint nchars,
			   guint old_lines, 
			   guint old_pixels);
static void clear_area (GtkText *text, GdkRectangle *area);
static void draw_line (GtkText* text,
		       gint pixel_height,
		       LineParams* lp);
static void draw_line_wrap (GtkText* text,
			    guint height);
static void draw_cursor (GtkText* text, gint absolute);
static void undraw_cursor (GtkText* text, gint absolute);
static gint drawn_cursor_min (GtkText* text);
static gint drawn_cursor_max (GtkText* text);
static void expose_text (GtkText* text, GdkRectangle *area, gboolean cursor);

/* Search and Placement. */
static void find_cursor (GtkText* text,
			 gboolean scroll);
static void find_cursor_at_line (GtkText* text,
				 const LineParams* start_line,
				 gint pixel_height);
static void find_mouse_cursor (GtkText* text, gint x, gint y);

/* Scrolling. */
static void adjust_adj  (GtkText* text, GtkAdjustment* adj);
static void scroll_up   (GtkText* text, gint diff);
static void scroll_down (GtkText* text, gint diff);
static void scroll_int  (GtkText* text, gint diff);

static void process_exposes (GtkText *text);

/* Cache Management. */
static void   free_cache        (GtkText* text);
static GList* remove_cache_line (GtkText* text, GList* list);

/* Key Motion. */
static void move_cursor_buffer_ver (GtkText *text, int dir);
static void move_cursor_page_ver (GtkText *text, int dir);
static void move_cursor_ver (GtkText *text, int count);
static void move_cursor_hor (GtkText *text, int count);

/* Binding actions */
static void gtk_text_move_cursor         (GtkEditable *editable,
					  gint         x,
					  gint         y);
static void gtk_text_move_word           (GtkEditable *editable,
					  gint         n);
static void gtk_text_move_page           (GtkEditable *editable,
					  gint         x,
					  gint         y);
static void gtk_text_move_to_row         (GtkEditable *editable,
					  gint         row);
static void gtk_text_move_to_column      (GtkEditable *editable,
					  gint         row);
static void gtk_text_kill_char           (GtkEditable *editable,
					  gint         direction);
static void gtk_text_kill_word           (GtkEditable *editable,
					  gint         direction);
static void gtk_text_kill_line           (GtkEditable *editable,
					  gint         direction);

/* To be removed */
static void gtk_text_move_forward_character    (GtkText          *text);
static void gtk_text_move_backward_character   (GtkText          *text);
static void gtk_text_move_forward_word         (GtkText          *text);
static void gtk_text_move_backward_word        (GtkText          *text);
static void gtk_text_move_beginning_of_line    (GtkText          *text);
static void gtk_text_move_end_of_line          (GtkText          *text);
static void gtk_text_move_next_line            (GtkText          *text);
static void gtk_text_move_previous_line        (GtkText          *text);

static void gtk_text_delete_forward_character  (GtkText          *text);
static void gtk_text_delete_backward_character (GtkText          *text);
static void gtk_text_delete_forward_word       (GtkText          *text);
static void gtk_text_delete_backward_word      (GtkText          *text);
static void gtk_text_delete_line               (GtkText          *text);
static void gtk_text_delete_to_line_end        (GtkText          *text);
static void gtk_text_select_word               (GtkText          *text,
						guint32           time);
static void gtk_text_select_line               (GtkText          *text,
						guint32           time);

static void gtk_text_set_position  (GtkEditable       *editable,
				    gint               position);

/* #define DEBUG_GTK_TEXT */

#if defined(DEBUG_GTK_TEXT) && defined(__GNUC__)
/* Debugging utilities. */
static void gtk_text_assert_mark (GtkText         *text,
				  GtkPropertyMark *mark,
				  GtkPropertyMark *before,
				  GtkPropertyMark *after,
				  const gchar     *msg,
				  const gchar     *where,
				  gint             line);

static void gtk_text_assert (GtkText         *text,
			     const gchar     *msg,
			     gint             line);
static void gtk_text_show_cache_line (GtkText *text, GList *cache,
				      const char* what, const char* func, gint line);
static void gtk_text_show_cache (GtkText *text, const char* func, gint line);
static void gtk_text_show_adj (GtkText *text,
			       GtkAdjustment *adj,
			       const char* what,
			       const char* func,
			       gint line);
static void gtk_text_show_props (GtkText* test,
				 const char* func,
				 int line);

#define TDEBUG(args) g_message args
#define TEXT_ASSERT(text) gtk_text_assert (text,__PRETTY_FUNCTION__,__LINE__)
#define TEXT_ASSERT_MARK(text,mark,msg) gtk_text_assert_mark (text,mark, \
					   __PRETTY_FUNCTION__,msg,__LINE__)
#define TEXT_SHOW(text) gtk_text_show_cache (text, __PRETTY_FUNCTION__,__LINE__)
#define TEXT_SHOW_LINE(text,line,msg) gtk_text_show_cache_line (text,line,msg,\
					   __PRETTY_FUNCTION__,__LINE__)
#define TEXT_SHOW_ADJ(text,adj,msg) gtk_text_show_adj (text,adj,msg, \
					  __PRETTY_FUNCTION__,__LINE__)
#else
#define TDEBUG(args)
#define TEXT_ASSERT(text)
#define TEXT_ASSERT_MARK(text,mark,msg)
#define TEXT_SHOW(text)
#define TEXT_SHOW_LINE(text,line,msg)
#define TEXT_SHOW_ADJ(text,adj,msg)
#endif

/* Memory Management. */
static GMemChunk  *params_mem_chunk    = NULL;
static GMemChunk  *text_property_chunk = NULL;

static GtkWidgetClass *parent_class = NULL;


static GtkTextFunction control_keys[26] =
{
  (GtkTextFunction)gtk_text_move_beginning_of_line,    /* a */
  (GtkTextFunction)gtk_text_move_backward_character,   /* b */
  (GtkTextFunction)gtk_editable_copy_clipboard,        /* c */
  (GtkTextFunction)gtk_text_delete_forward_character,  /* d */
  (GtkTextFunction)gtk_text_move_end_of_line,          /* e */
  (GtkTextFunction)gtk_text_move_forward_character,    /* f */
  NULL,                                                /* g */
  (GtkTextFunction)gtk_text_delete_backward_character, /* h */
  NULL,                                                /* i */
  NULL,                                                /* j */
  (GtkTextFunction)gtk_text_delete_to_line_end,        /* k */
  NULL,                                                /* l */
  NULL,                                                /* m */
  (GtkTextFunction)gtk_text_move_next_line,            /* n */
  NULL,                                                /* o */
  (GtkTextFunction)gtk_text_move_previous_line,        /* p */
  NULL,                                                /* q */
  NULL,                                                /* r */
  NULL,                                                /* s */
  NULL,                                                /* t */
  (GtkTextFunction)gtk_text_delete_line,               /* u */
  (GtkTextFunction)gtk_editable_paste_clipboard,       /* v */
  (GtkTextFunction)gtk_text_delete_backward_word,      /* w */
  (GtkTextFunction)gtk_editable_cut_clipboard,         /* x */
  NULL,                                                /* y */
  NULL,                                                /* z */
};

static GtkTextFunction alt_keys[26] =
{
  NULL,                                                /* a */
  (GtkTextFunction)gtk_text_move_backward_word,        /* b */
  NULL,                                                /* c */
  (GtkTextFunction)gtk_text_delete_forward_word,       /* d */
  NULL,                                           /* e */
  (GtkTextFunction)gtk_text_move_forward_word,         /* f */
  NULL,                                           /* g */
  NULL,                                           /* h */
  NULL,                                           /* i */
  NULL,                                           /* j */
  NULL,                                           /* k */
  NULL,                                           /* l */
  NULL,                                           /* m */
  NULL,                                           /* n */
  NULL,                                           /* o */
  NULL,                                           /* p */
  NULL,                                           /* q */
  NULL,                                           /* r */
  NULL,                                           /* s */
  NULL,                                           /* t */
  NULL,                                           /* u */
  NULL,                                           /* v */
  NULL,                                           /* w */
  NULL,                                           /* x */
  NULL,                                           /* y */
  NULL,                                           /* z */
};


/**********************************************************************/
/*			        Widget Crap                           */
/**********************************************************************/

GtkType
gtk_text_get_type (void)
{
  static GtkType text_type = 0;
  
  if (!text_type)
    {
      GtkTypeInfo text_info =
      {
	"GtkText",
	sizeof (GtkText),
	sizeof (GtkTextClass),
	(GtkClassInitFunc) gtk_text_class_init,
	(GtkObjectInitFunc) gtk_text_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      text_type = gtk_type_unique (GTK_TYPE_EDITABLE, &text_info);
    }
  
  return text_type;
}

static void
gtk_text_class_init (GtkTextClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkEditableClass *editable_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  editable_class = (GtkEditableClass*) class;
  
  parent_class = gtk_type_class (GTK_TYPE_EDITABLE);
  
  object_class->destroy = gtk_text_destroy;
  object_class->finalize = gtk_text_finalize;
  
  widget_class->realize = gtk_text_realize;
  widget_class->unrealize = gtk_text_unrealize;
  widget_class->style_set = gtk_text_style_set;
  widget_class->draw_focus = gtk_text_draw_focus;
  widget_class->size_request = gtk_text_size_request;
  widget_class->size_allocate = gtk_text_size_allocate;
  widget_class->draw = gtk_text_draw;
  widget_class->expose_event = gtk_text_expose;
  widget_class->button_press_event = gtk_text_button_press;
  widget_class->button_release_event = gtk_text_button_release;
  widget_class->motion_notify_event = gtk_text_motion_notify;
  widget_class->key_press_event = gtk_text_key_press;
  widget_class->focus_in_event = gtk_text_focus_in;
  widget_class->focus_out_event = gtk_text_focus_out;
  
  editable_class->set_editable = gtk_text_real_set_editable;
  editable_class->insert_text = gtk_text_insert_text;
  editable_class->delete_text = gtk_text_delete_text;
  
  editable_class->move_cursor = gtk_text_move_cursor;
  editable_class->move_word = gtk_text_move_word;
  editable_class->move_page = gtk_text_move_page;
  editable_class->move_to_row = gtk_text_move_to_row;
  editable_class->move_to_column = gtk_text_move_to_column;
  
  editable_class->kill_char = gtk_text_kill_char;
  editable_class->kill_word = gtk_text_kill_word;
  editable_class->kill_line = gtk_text_kill_line;
  
  editable_class->update_text = gtk_text_update_text;
  editable_class->get_chars   = gtk_text_get_chars;
  editable_class->set_selection = gtk_text_set_selection;
  editable_class->set_position = gtk_text_set_position;
}

static void
gtk_text_init (GtkText *text)
{
  GTK_WIDGET_SET_FLAGS (text, GTK_CAN_FOCUS);
  
  text->text = g_new (guchar, INITIAL_BUFFER_SIZE);
  text->text_len = INITIAL_BUFFER_SIZE;
  
  if (!params_mem_chunk)
    params_mem_chunk = g_mem_chunk_new ("LineParams",
					sizeof (LineParams),
					256 * sizeof (LineParams),
					G_ALLOC_AND_FREE);
  
  text->default_tab_width = 4;
  text->tab_stops = NULL;
  
  text->tab_stops = g_list_prepend (text->tab_stops, (void*)8);
  text->tab_stops = g_list_prepend (text->tab_stops, (void*)8);
  
  text->line_start_cache = NULL;
  text->first_cut_pixels = 0;
  
  text->line_wrap = TRUE;
  text->word_wrap = FALSE;
  
  text->timer = 0;
  text->button = 0;
  
  text->current_font = NULL;

  init_properties (text);

  GTK_EDITABLE(text)->editable = FALSE;
}

GtkWidget*
gtk_text_new (GtkAdjustment *hadj,
	      GtkAdjustment *vadj)
{
  GtkText *text;
  
  text = gtk_type_new (GTK_TYPE_TEXT);
  
  gtk_text_set_adjustments (text, hadj, vadj);
  gtk_editable_set_position (GTK_EDITABLE (text), 0);
  
  return GTK_WIDGET (text);
}

void
gtk_text_set_word_wrap (GtkText *text,
			gint     word_wrap)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));
  
  text->word_wrap = (word_wrap != FALSE);
  
  if (GTK_WIDGET_REALIZED (text))
    {
      recompute_geometry (text);
      gtk_widget_queue_draw (GTK_WIDGET (text));
    }
}

void
gtk_text_set_editable (GtkText *text,
		       gboolean is_editable)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));
  
  gtk_editable_set_editable (GTK_EDITABLE (text), is_editable);
}

static void
gtk_text_real_set_editable (GtkEditable *editable,
			    gboolean     is_editable)
{
  GtkText *text;
  
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_TEXT (editable));
  
  text = GTK_TEXT (editable);
  
  editable->editable = (is_editable != FALSE);
  
  if (is_editable)
    draw_cursor (text, TRUE);
  else
    undraw_cursor (text, TRUE);
}

void
gtk_text_set_adjustments (GtkText       *text,
			  GtkAdjustment *hadj,
			  GtkAdjustment *vadj)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));
  
  if (text->hadj && (text->hadj != hadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text->hadj), text);
      gtk_object_unref (GTK_OBJECT (text->hadj));
    }
  
  if (text->vadj && (text->vadj != vadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text->vadj), text);
      gtk_object_unref (GTK_OBJECT (text->vadj));
    }
  
  if (!hadj)
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  
  if (!vadj)
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  
  if (text->hadj != hadj)
    {
      text->hadj = hadj;
      gtk_object_ref (GTK_OBJECT (text->hadj));
      gtk_object_sink (GTK_OBJECT (text->hadj));
      
      gtk_signal_connect (GTK_OBJECT (text->hadj), "changed",
			  (GtkSignalFunc) gtk_text_adjustment,
			  text);
      gtk_signal_connect (GTK_OBJECT (text->hadj), "value_changed",
			  (GtkSignalFunc) gtk_text_adjustment,
			  text);
      gtk_signal_connect (GTK_OBJECT (text->hadj), "disconnect",
			  (GtkSignalFunc) gtk_text_disconnect,
			  text);
    }
  
  if (text->vadj != vadj)
    {
      text->vadj = vadj;
      gtk_object_ref (GTK_OBJECT (text->vadj));
      gtk_object_sink (GTK_OBJECT (text->vadj));
      
      gtk_signal_connect (GTK_OBJECT (text->vadj), "changed",
			  (GtkSignalFunc) gtk_text_adjustment,
			  text);
      gtk_signal_connect (GTK_OBJECT (text->vadj), "value_changed",
			  (GtkSignalFunc) gtk_text_adjustment,
			  text);
      gtk_signal_connect (GTK_OBJECT (text->vadj), "disconnect",
			  (GtkSignalFunc) gtk_text_disconnect,
			  text);
    }
}

void
gtk_text_set_point (GtkText *text,
		    guint    index)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));
  g_return_if_fail (index <= TEXT_LENGTH (text));
  
  text->point = find_mark (text, index);
}

guint
gtk_text_get_point (GtkText *text)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);
  
  return text->point.index;
}

guint
gtk_text_get_length (GtkText *text)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);
  
  return TEXT_LENGTH (text);
}

void
gtk_text_freeze (GtkText *text)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));
  
  text->freeze = TRUE;
}

void
gtk_text_thaw (GtkText *text)
{
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));
  
  text->freeze = FALSE;
  
  if (GTK_WIDGET_REALIZED (text))
    {
      recompute_geometry (text);
      gtk_widget_queue_draw (GTK_WIDGET (text));
    }
}

void
gtk_text_insert (GtkText    *text,
		 GdkFont    *font,
		 GdkColor   *fore,
		 GdkColor   *back,
		 const char *chars,
		 gint        nchars)
{
  GtkEditable *editable = GTK_EDITABLE (text);
  gboolean frozen = FALSE;
  
  gint new_line_count = 1;
  guint old_height = 0;
  guint length;
  guint i;
  
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));

  if (nchars < 0)
    length = strlen (chars);
  else
    length = nchars;
  
  if (length == 0)
    return;
  
  if (!text->freeze && (length > FREEZE_LENGTH))
    {
      gtk_text_freeze (text);
      frozen = TRUE;
    }
  
  if (!text->freeze && (text->line_start_cache != NULL))
    {
      find_line_containing_point (text, text->point.index, TRUE);
      old_height = total_line_height (text, text->current_line, 1);
      for (i=0; i<length; i++)
	if (chars[i] == '\n')
	  new_line_count++;
    }
  
  if (text->point.index < text->first_line_start_index)
    text->first_line_start_index += length;
  
  if (text->point.index < editable->selection_start_pos)
    editable->selection_start_pos += length;
  if (text->point.index < editable->selection_end_pos)
    editable->selection_end_pos += length;
  /* We'll reset the cursor later anyways if we aren't frozen */
  if (text->point.index < text->cursor_mark.index)
    text->cursor_mark.index += length;
  
  move_gap_to_point (text);

  make_forward_space (text, length);
  memcpy (text->text + text->gap_position, chars, length);
  
  insert_text_property (text, font, fore, back, length);
  
  text->gap_size -= length;
  text->gap_position += length;
  
  advance_mark_n (&text->point, length);
  
  if (!text->freeze && (text->line_start_cache != NULL))
    insert_expose (text, old_height, length, new_line_count);
  
  if (frozen)
    gtk_text_thaw (text);
}

gint
gtk_text_backward_delete (GtkText *text,
			  guint    nchars)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);
  
  if (nchars > text->point.index || nchars <= 0)
    return FALSE;
  
  gtk_text_set_point (text, text->point.index - nchars);
  
  return gtk_text_forward_delete (text, nchars);
}

gint
gtk_text_forward_delete (GtkText *text,
			 guint    nchars)
{
  guint old_lines, old_height;
  GtkEditable *editable = GTK_EDITABLE (text);
  gboolean frozen = FALSE;
  
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (GTK_IS_TEXT (text), 0);
  
  if (text->point.index + nchars > TEXT_LENGTH (text) || nchars <= 0)
    return FALSE;
  
  if (!text->freeze && (nchars > FREEZE_LENGTH))
    {
      gtk_text_freeze (text);
      frozen = TRUE;
    }
  
  if (!text->freeze && (text->line_start_cache != NULL))
    {
      /* We need to undraw the cursor here, since we may later
       * delete the cursor's property
       */
      undraw_cursor (text, FALSE);
      find_line_containing_point (text, text->point.index, TRUE);
      compute_lines_pixels (text, nchars, &old_lines, &old_height);
    }
  
  /* FIXME, or resizing after deleting will be odd */
  if (text->point.index < text->first_line_start_index)
    {
      if (text->point.index + nchars >= text->first_line_start_index)
	{
	  text->first_line_start_index = text->point.index;
	  while ((text->first_line_start_index > 0) &&
		 (GTK_TEXT_INDEX (text, text->first_line_start_index - 1) != LINE_DELIM))
	    text->first_line_start_index -= 1;
	  
	}
      else
	text->first_line_start_index -= nchars;
    }
  
  if (text->point.index < editable->selection_start_pos)
    editable->selection_start_pos -= 
      MIN(nchars, editable->selection_start_pos - text->point.index);
  if (text->point.index < editable->selection_end_pos)
    editable->selection_end_pos -= 
      MIN(nchars, editable->selection_end_pos - text->point.index);
  /* We'll reset the cursor later anyways if we aren't frozen */
  if (text->point.index < text->cursor_mark.index)
    move_mark_n (&text->cursor_mark, 
		 -MIN(nchars, text->cursor_mark.index - text->point.index));
  
  move_gap_to_point (text);
  
  text->gap_size += nchars;
  
  delete_text_property (text, nchars);
  
  if (!text->freeze && (text->line_start_cache != NULL))
    {
      delete_expose (text, nchars, old_lines, old_height);
      draw_cursor (text, FALSE);
    }
  
  if (frozen)
    gtk_text_thaw (text);
  
  return TRUE;
}

static void
gtk_text_set_position (GtkEditable *editable,
		       gint position)
{
  GtkText *text = (GtkText *) editable;
  
  undraw_cursor (text, FALSE);
  text->cursor_mark = find_mark (text, position);
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
  gtk_editable_select_region (editable, 0, 0);
}

static gchar *    
gtk_text_get_chars (GtkEditable   *editable,
		    gint           start_pos,
		    gint           end_pos)
{
  GtkText *text;
  
  gchar *retval;
  gchar *p;
  guint n, nchars;
  
  g_return_val_if_fail (editable != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TEXT (editable), NULL);
  text = GTK_TEXT (editable);
  
  if (end_pos < 0)
    end_pos = TEXT_LENGTH (text);
  
  if ((start_pos < 0) || 
      (end_pos > TEXT_LENGTH (text)) || 
      (end_pos < start_pos))
    return NULL;
  
  nchars = end_pos - start_pos;
  
  retval = g_new (gchar, nchars+1);
  p = retval;
  
  if (start_pos < text->gap_position) 
    {
      n = MIN (text->gap_position - start_pos, nchars);
      memcpy (p, &text->text[start_pos], n);
      p += n;
      start_pos += n;
      nchars -= n;
    }
  
  if (start_pos+nchars >= text->gap_position)
    {
      memcpy (p, 
	      text->text + MAX (text->gap_position + text->gap_size, 
				start_pos + text->gap_size),
	      nchars);
      p += nchars;
    }
  
  *p = 0;
  
  return retval;
}


static void
gtk_text_destroy (GtkObject *object)
{
  GtkText *text;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_TEXT (object));
  
  text = (GtkText *)object;
  if (text->hadj)
    {
      gtk_object_unref (GTK_OBJECT (text->hadj));
      text->hadj = NULL;
    }
  if (text->vadj)
    {
      gtk_object_unref (GTK_OBJECT (text->vadj));
      text->vadj = NULL;
    }
  if (text->timer)
    {
      gtk_timeout_remove (text->timer);
      text->timer = 0;
    }
  
  GTK_OBJECT_CLASS(parent_class)->destroy (object);
}

static void
gtk_text_finalize (GtkObject *object)
{
  GtkText *text;
  GList *tmp_list;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_TEXT (object));
  
  text = (GtkText *)object;
  
  /* Clean up the internal structures */
  g_free (text->text);
  free_cache (text);
  
  tmp_list = text->text_properties;
  while (tmp_list)
    {
      destroy_text_property (tmp_list->data);
      tmp_list = tmp_list->next;
    }

  text_font_unref (text->current_font);
  
  g_list_free (text->text_properties);
  
  if (text->scratch_buffer)
    g_free (text->scratch_buffer);
  
  g_list_free (text->tab_stops);
  
  GTK_OBJECT_CLASS(parent_class)->finalize (object);
}

static void
gtk_text_realize (GtkWidget *widget)
{
  GtkText *text;
  GtkEditable *editable;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  
  text = GTK_TEXT (widget);
  editable = GTK_EDITABLE (widget);
  GTK_WIDGET_SET_FLAGS (text, GTK_REALIZED);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_BUTTON_MOTION_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, text);
  
  attributes.x = (widget->style->klass->xthickness + TEXT_BORDER_ROOM);
  attributes.y = (widget->style->klass->ythickness + TEXT_BORDER_ROOM);
  attributes.width = MAX (1, (gint)widget->allocation.width - (gint)attributes.x * 2);
  attributes.height = MAX (1, (gint)widget->allocation.height - (gint)attributes.y * 2);
  
  text->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (text->text_area, text);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  /* Can't call gtk_style_set_background here because it's handled specially */
  gdk_window_set_background (widget->window, &widget->style->base[GTK_STATE_NORMAL]);
  gdk_window_set_background (text->text_area, &widget->style->base[GTK_STATE_NORMAL]);
  
  text->line_wrap_bitmap = gdk_bitmap_create_from_data (text->text_area,
							(gchar*) line_wrap_bits,
							line_wrap_width,
							line_wrap_height);
  
  text->line_arrow_bitmap = gdk_bitmap_create_from_data (text->text_area,
							 (gchar*) line_arrow_bits,
							 line_arrow_width,
							 line_arrow_height);
  
  text->gc = gdk_gc_new (text->text_area);
  gdk_gc_set_exposures (text->gc, TRUE);
  gdk_gc_set_foreground (text->gc, &widget->style->text[GTK_STATE_NORMAL]);
  
#ifdef USE_XIM
  if (gdk_im_ready ())
    {
      GdkPoint spot;
      GdkRectangle rect;
      gint width, height;
      GdkEventMask mask;
      GdkIMStyle style;
      GdkIMStyle supported_style = GDK_IM_PREEDIT_NONE | 
	                           GDK_IM_PREEDIT_NOTHING |
	                           GDK_IM_PREEDIT_POSITION |
	                           GDK_IM_STATUS_NONE |
	                           GDK_IM_STATUS_NOTHING;
      
      if (widget->style && widget->style->font->type != GDK_FONT_FONTSET)
	supported_style &= ~GDK_IM_PREEDIT_POSITION;
      
      style = gdk_im_decide_style (supported_style);
      switch (style & GDK_IM_PREEDIT_MASK)
	{
	case GDK_IM_PREEDIT_POSITION:
	  if (widget->style && widget->style->font->type != GDK_FONT_FONTSET)
	    {
	      g_warning ("over-the-spot style requires fontset");
	      break;
	    }
	  gdk_window_get_size (text->text_area, &width, &height);
	  rect.x = 0;
	  rect.y = 0;
	  rect.width = width;
	  rect.height = height;
	  spot.x = 0;
	  spot.y = height;
	  
	  editable->ic = gdk_ic_new (text->text_area, text->text_area,
				     style,
				     "spotLocation", &spot,
				     "area", &rect,
				     "fontSet", GDK_FONT_XFONT (widget->style->font),
				     NULL);
	  break;
	default:
	  editable->ic = gdk_ic_new (text->text_area, text->text_area,
				     style, NULL);
	}
      
      if (editable->ic == NULL)
	g_warning ("Can't create input context.");
      else
	{
	  GdkColormap *colormap;
	  
	  mask = gdk_window_get_events (text->text_area);
	  mask |= gdk_ic_get_events (editable->ic);
	  gdk_window_set_events (text->text_area, mask);
	  
	  if ((colormap = gtk_widget_get_colormap (widget)) !=
	      gtk_widget_get_default_colormap ())
	    {
	      gdk_ic_set_attr (editable->ic, "preeditAttributes",
	      		       "colorMap", GDK_COLORMAP_XCOLORMAP (colormap),
			       NULL);
	    }
	}
    }
#endif

  realize_properties (text);
  gdk_window_show (text->text_area);
  init_properties (text);

  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_claim_selection (editable, TRUE, GDK_CURRENT_TIME);
  
  recompute_geometry (text);
}

static void 
gtk_text_style_set	(GtkWidget      *widget,
			 GtkStyle       *previous_style)
{
  GtkText *text;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  
  text = GTK_TEXT (widget);
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_set_background (widget->window, &widget->style->base[GTK_STATE_NORMAL]);
      gdk_window_set_background (text->text_area, &widget->style->base[GTK_STATE_NORMAL]);
      
      recompute_geometry (text);
    }

  if (text->current_font)
    text_font_unref (text->current_font);
  text->current_font = get_text_font (widget->style->font);

  if (GTK_WIDGET_DRAWABLE (widget))
    gdk_window_clear (widget->window);
}

static void
gtk_text_unrealize (GtkWidget *widget)
{
  GtkText *text;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  
  text = GTK_TEXT (widget);
  
  gdk_window_set_user_data (text->text_area, NULL);
  gdk_window_destroy (text->text_area);
  text->text_area = NULL;
  
  gdk_gc_destroy (text->gc);
  text->gc = NULL;
  
  gdk_pixmap_unref (text->line_wrap_bitmap);
  gdk_pixmap_unref (text->line_arrow_bitmap);

  unrealize_properties (text);

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
clear_focus_area (GtkText *text, gint area_x, gint area_y, gint area_width, gint area_height)
{
  GtkWidget *widget = GTK_WIDGET (text);
  
  gint ythick = TEXT_BORDER_ROOM + widget->style->klass->ythickness;
  gint xthick = TEXT_BORDER_ROOM + widget->style->klass->xthickness;
  
  gint width, height;
  gint xorig, yorig;
  gint x, y;
  
  gdk_window_get_size (widget->style->bg_pixmap[GTK_STATE_NORMAL], &width, &height);
  
  yorig = - text->first_onscreen_ver_pixel + ythick;
  xorig = - text->first_onscreen_hor_pixel + xthick;
  
  while (yorig > 0)
    yorig -= height;
  
  while (xorig > 0)
    xorig -= width;
  
  for (y = area_y; y < area_y + area_height; )
    {
      gint yoff = (y - yorig) % height;
      gint yw = MIN(height - yoff, (area_y + area_height) - y);
      
      for (x = area_x; x < area_x + area_width; )
	{
	  gint xoff = (x - xorig) % width;
	  gint xw = MIN(width - xoff, (area_x + area_width) - x);
	  
	  gdk_draw_pixmap (widget->window,
			   text->gc,
			   widget->style->bg_pixmap[GTK_STATE_NORMAL],
			   xoff,
			   yoff,
			   x,
			   y,
			   xw,
			   yw);
	  
	  x += width - xoff;
	}
      y += height - yoff;
    }
}



static void
gtk_text_draw_focus (GtkWidget *widget)
{
  GtkText *text;
  gint width, height;
  gint x, y;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  
  text = GTK_TEXT (widget);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gint ythick = widget->style->klass->ythickness;
      gint xthick = widget->style->klass->xthickness;
      gint xextra = TEXT_BORDER_ROOM;
      gint yextra = TEXT_BORDER_ROOM;
      
      TDEBUG (("in gtk_text_draw_focus\n"));
      
      x = 0;
      y = 0;
      width = widget->allocation.width;
      height = widget->allocation.height;
      
      if (GTK_WIDGET_HAS_FOCUS (widget))
	{
	  x += 1;
	  y += 1;
	  width -=  2;
	  height -= 2;
	  xextra -= 1;
	  yextra -= 1;
	  
	  gdk_draw_rectangle (widget->window,
			      widget->style->fg_gc[GTK_STATE_NORMAL],
			      FALSE, 0, 0,
			      widget->allocation.width - 1,
			      widget->allocation.height - 1);
	}
      
      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, GTK_SHADOW_IN,
		       x, y, width, height);
      
      x += xthick; 
      y += ythick;
      width -= 2 * xthick;
      height -= 2 * ythick;
      
      if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  /* top rect */
	  clear_focus_area (text, x, y, width, yextra);
	  /* left rect */
	  clear_focus_area (text, x, y + yextra, 
			    xextra, y + height - 2 * yextra);
	  /* right rect */
	  clear_focus_area (text, x + width - xextra, y + yextra, 
			    xextra, height - 2 * ythick);
	  /* bottom rect */
	  clear_focus_area (text, x, x + height - yextra, width, yextra);
	}
      else if (!GTK_WIDGET_HAS_FOCUS (widget))
	{
	  gdk_draw_rectangle (widget->window,
			      widget->style->base_gc[GTK_STATE_NORMAL], FALSE,
			      x, y,
			      width - 1,
			      height - 1);
	}
    }
  else
    {
      TDEBUG (("in gtk_text_draw_focus (undrawable !!!)\n"));
    }
}

static void
gtk_text_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  gint xthickness;
  gint ythickness;
  gint char_height;
  gint char_width;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  g_return_if_fail (requisition != NULL);
  
  xthickness = widget->style->klass->xthickness + TEXT_BORDER_ROOM;
  ythickness = widget->style->klass->ythickness + TEXT_BORDER_ROOM;
  
  char_height = MIN_TEXT_HEIGHT_LINES * (widget->style->font->ascent +
					 widget->style->font->descent);
  
  char_width = MIN_TEXT_WIDTH_LINES * (gdk_text_width (widget->style->font,
						       "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
						       26)
				       / 26);
  
  requisition->width  = char_width  + xthickness * 2;
  requisition->height = char_height + ythickness * 2;
}

static void
gtk_text_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkText *text;
  GtkEditable *editable;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  g_return_if_fail (allocation != NULL);
  
  text = GTK_TEXT (widget);
  editable = GTK_EDITABLE (widget);
  
  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      
      gdk_window_move_resize (text->text_area,
			      widget->style->klass->xthickness + TEXT_BORDER_ROOM,
			      widget->style->klass->ythickness + TEXT_BORDER_ROOM,
			      MAX (1, (gint)widget->allocation.width - (gint)(widget->style->klass->xthickness +
							  (gint)TEXT_BORDER_ROOM) * 2),
			      MAX (1, (gint)widget->allocation.height - (gint)(widget->style->klass->ythickness +
							   (gint)TEXT_BORDER_ROOM) * 2));
      
#ifdef USE_XIM
      if (editable->ic && (gdk_ic_get_style (editable->ic) & GDK_IM_PREEDIT_POSITION))
	{
	  gint width, height;
	  GdkRectangle rect;
	  
	  gdk_window_get_size (text->text_area, &width, &height);
	  rect.x = 0;
	  rect.y = 0;
	  rect.width = width;
	  rect.height = height;
	  gdk_ic_set_attr (editable->ic, "preeditAttributes", "area", &rect, NULL);
	}
#endif
      
      recompute_geometry (text);
    }
}

static void
gtk_text_draw (GtkWidget    *widget,
	       GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEXT (widget));
  g_return_if_fail (area != NULL);
  
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      expose_text (GTK_TEXT (widget), area, TRUE);
      gtk_widget_draw_focus (widget);
    }
}

static gint
gtk_text_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (event->window == GTK_TEXT (widget)->text_area)
    {
      TDEBUG (("in gtk_text_expose (expose)\n"));
      expose_text (GTK_TEXT (widget), &event->area, TRUE);
    }
  else if (event->count == 0)
    {
      TDEBUG (("in gtk_text_expose (focus)\n"));
      gtk_widget_draw_focus (widget);
    }
  
  return FALSE;
}

static gint
gtk_text_scroll_timeout (gpointer data)
{
  GtkText *text;
  GdkEventMotion event;
  
  gint x, y;
  GdkModifierType mask;
  
  g_return_val_if_fail (GTK_IS_TEXT (data), FALSE);
  
  text = GTK_TEXT (data);
  
  text->timer = 0;
  gdk_window_get_pointer (text->text_area, &x, &y, &mask);
  
  if (!(mask & (GDK_BUTTON1_MASK | GDK_BUTTON3_MASK)))
    return FALSE;
  
  event.is_hint = 0;
  event.x = x;
  event.y = y;
  event.state = mask;
  
  gtk_text_motion_notify (GTK_WIDGET (text), &event);
  
  return FALSE;
}

static gint
gtk_text_button_press (GtkWidget      *widget,
		       GdkEventButton *event)
{
  GtkText *text;
  GtkEditable *editable;
  static GdkAtom ctext_atom = GDK_NONE;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (ctext_atom == GDK_NONE)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);
  
  text = GTK_TEXT (widget);
  editable = GTK_EDITABLE (widget);
  
  if (text->button && (event->button != text->button))
    return FALSE;
  
  text->button = event->button;
  
  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);
  
  if (event->button == 1)
    {
      switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	  gtk_grab_add (widget);
	  
	  undraw_cursor (text, FALSE);
	  find_mouse_cursor (text, (gint)event->x, (gint)event->y);
	  draw_cursor (text, FALSE);
	  
	  /* Set it now, so we display things right. We'll unset it
	   * later if things don't work out */
	  editable->has_selection = TRUE;
	  gtk_text_set_selection (GTK_EDITABLE(text),
				  text->cursor_mark.index,
				  text->cursor_mark.index);
	  
	  break;
	  
	case GDK_2BUTTON_PRESS:
	  gtk_text_select_word (text, event->time);
	  break;
	  
	case GDK_3BUTTON_PRESS:
	  gtk_text_select_line (text, event->time);
	  break;
	  
	default:
	  break;
	}
    }
  else if (event->type == GDK_BUTTON_PRESS)
    {
      if ((event->button == 2) && editable->editable)
	{
	  if (editable->selection_start_pos == editable->selection_end_pos ||
	      editable->has_selection)
	    {
	      undraw_cursor (text, FALSE);
	      find_mouse_cursor (text, (gint)event->x, (gint)event->y);
	      draw_cursor (text, FALSE);
	      
	    }
	  
	  gtk_selection_convert (widget, GDK_SELECTION_PRIMARY,
				 ctext_atom, event->time);
	}
      else
	{
	  gtk_grab_add (widget);
	  
	  undraw_cursor (text, FALSE);
	  find_mouse_cursor (text, event->x, event->y);
	  draw_cursor (text, FALSE);
	  
	  gtk_text_set_selection (GTK_EDITABLE(text),
				  text->cursor_mark.index,
				  text->cursor_mark.index);
	  
	  editable->has_selection = FALSE;
	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
	}
    }
  
  return FALSE;
}

static gint
gtk_text_button_release (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkText *text;
  GtkEditable *editable;
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  text = GTK_TEXT (widget);
  
  gtk_grab_remove (widget);
  
  if (text->button != event->button)
    return FALSE;
  
  text->button = 0;
  
  if (text->timer)
    {
      gtk_timeout_remove (text->timer);
      text->timer = 0;
    }
  
  if (event->button == 1)
    {
      text = GTK_TEXT (widget);
      editable = GTK_EDITABLE (widget);
      
      gtk_grab_remove (widget);
      
      editable->has_selection = FALSE;
      if (editable->selection_start_pos != editable->selection_end_pos)
	{
	  if (gtk_selection_owner_set (widget,
				       GDK_SELECTION_PRIMARY,
				       event->time))
	    editable->has_selection = TRUE;
	  else
	    gtk_text_update_text (editable, editable->selection_start_pos,
				  editable->selection_end_pos);
	}
      else
	{
	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
	}
    }
  else if (event->button == 3)
    {
      gtk_grab_remove (widget);
    }
  
  undraw_cursor (text, FALSE);
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
  
  return FALSE;
}

static gint
gtk_text_motion_notify (GtkWidget      *widget,
			GdkEventMotion *event)
{
  GtkText *text;
  gint x, y;
  gint height;
  GdkModifierType mask;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  text = GTK_TEXT (widget);
  
  x = event->x;
  y = event->y;
  mask = event->state;
  if (event->is_hint || (text->text_area != event->window))
    {
      gdk_window_get_pointer (text->text_area, &x, &y, &mask);
    }
  
  if ((text->button == 0) ||
      !(mask & (GDK_BUTTON1_MASK | GDK_BUTTON3_MASK)))
    return FALSE;
  
  gdk_window_get_size (text->text_area, NULL, &height);
  
  if ((y < 0) || (y > height))
    {
      if (text->timer == 0)
	{
	  text->timer = gtk_timeout_add (SCROLL_TIME, 
					 gtk_text_scroll_timeout,
					 text);
	  
	  if (y < 0)
	    scroll_int (text, y/2);
	  else
	    scroll_int (text, (y - height)/2);
	}
      else
	return FALSE;
    }
  
  undraw_cursor (GTK_TEXT (widget), FALSE);
  find_mouse_cursor (GTK_TEXT (widget), x, y);
  draw_cursor (GTK_TEXT (widget), FALSE);
  
  gtk_text_set_selection (GTK_EDITABLE(text), 
			  GTK_EDITABLE(text)->selection_start_pos,
			  text->cursor_mark.index);
  
  return FALSE;
}

static void 
gtk_text_insert_text    (GtkEditable       *editable,
			 const gchar       *new_text,
			 gint               new_text_length,
			 gint              *position)
{
  GtkText *text = GTK_TEXT (editable);
  GdkFont *font;
  GdkColor *fore, *back;

  TextProperty *property;

  gtk_text_set_point (text, *position);

  property = MARK_CURRENT_PROPERTY (&text->point);
  font = property->flags & PROPERTY_FONT ? property->font->gdk_font : NULL; 
  fore = property->flags & PROPERTY_FOREGROUND ? &property->fore_color : NULL; 
  back = property->flags & PROPERTY_BACKGROUND ? &property->back_color : NULL; 
  
  gtk_text_insert (text, font, fore, back, new_text, new_text_length);

  *position = text->point.index;
}

static void 
gtk_text_delete_text    (GtkEditable       *editable,
			 gint               start_pos,
			 gint               end_pos)
{
  GtkText *text;
  
  g_return_if_fail (start_pos >= 0);
  
  text = GTK_TEXT (editable);
  
  gtk_text_set_point (text, start_pos);
  if (end_pos < 0)
    end_pos = TEXT_LENGTH (text);
  
  if (end_pos > start_pos)
    gtk_text_forward_delete (text, end_pos - start_pos);
}

static gint
gtk_text_key_press (GtkWidget   *widget,
		    GdkEventKey *event)
{
  GtkText *text;
  GtkEditable *editable;
  gchar key;
  gint return_val;
  gint position;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  return_val = FALSE;
  
  text = GTK_TEXT (widget);
  editable = GTK_EDITABLE (widget);
  
  key = event->keyval;
  return_val = TRUE;
  
  if ((GTK_EDITABLE(text)->editable == FALSE))
    {
      switch (event->keyval)
	{
	case GDK_Home:      
	  if (event->state & GDK_CONTROL_MASK)
	    scroll_int (text, -text->vadj->value); 
	  break;
	case GDK_End:
	  if (event->state & GDK_CONTROL_MASK)
	    scroll_int (text, +text->vadj->upper); 
	  break;
	case GDK_Page_Up:   scroll_int (text, -text->vadj->page_increment); break;
	case GDK_Page_Down: scroll_int (text, +text->vadj->page_increment); break;
	case GDK_Up:        scroll_int (text, -KEY_SCROLL_PIXELS); break;
	case GDK_Down:      scroll_int (text, +KEY_SCROLL_PIXELS); break;
	case GDK_Return:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_signal_emit_by_name (GTK_OBJECT (text), "activate");
	  break;
	default:
	  return_val = FALSE;
	  break;
	}
    }
  else
    {
      gint extend_selection;
      gint extend_start;
      guint initial_pos = editable->current_pos;
      
      text->point = find_mark (text, text->cursor_mark.index);
      
      extend_selection = event->state & GDK_SHIFT_MASK;
      extend_start = FALSE;
      
      if (extend_selection)
	{
	  editable->has_selection = TRUE;
	  
	  if (editable->selection_start_pos == editable->selection_end_pos)
	    {
	      editable->selection_start_pos = text->point.index;
	      editable->selection_end_pos = text->point.index;
	    }
	  
	  extend_start = (text->point.index == editable->selection_start_pos);
	}
      
      switch (event->keyval)
	{
	case GDK_Home:
	  if (event->state & GDK_CONTROL_MASK)
	    move_cursor_buffer_ver (text, -1);
	  else
	    gtk_text_move_beginning_of_line (text);
	  break;
	case GDK_End:
	  if (event->state & GDK_CONTROL_MASK)
	    move_cursor_buffer_ver (text, +1);
	  else
	    gtk_text_move_end_of_line (text);
	  break;
	case GDK_Page_Up:   move_cursor_page_ver (text, -1); break;
	case GDK_Page_Down: move_cursor_page_ver (text, +1); break;
	  /* CUA has Ctrl-Up/Ctrl-Down as paragraph up down */
	case GDK_Up:        move_cursor_ver (text, -1); break;
	case GDK_Down:      move_cursor_ver (text, +1); break;
	case GDK_Left:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_text_move_backward_word (text);
	  else
	    move_cursor_hor (text, -1); 
	  break;
	case GDK_Right:     
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_text_move_forward_word (text);
	  else
	    move_cursor_hor (text, +1); 
	  break;
	  
	case GDK_BackSpace:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_text_delete_backward_word (text);
	  else
	    gtk_text_delete_backward_character (text);
	  break;
	case GDK_Clear:
	  gtk_text_delete_line (text);
	  break;
	case GDK_Insert:
	  if (event->state & GDK_SHIFT_MASK)
	    {
	      extend_selection = FALSE;
	      gtk_editable_paste_clipboard (editable);
	    }
	  else if (event->state & GDK_CONTROL_MASK)
	    {
	      gtk_editable_copy_clipboard (editable);
	    }
	  else
	    {
	      /* gtk_toggle_insert(text) -- IMPLEMENT */
	    }
	  break;
	case GDK_Delete:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_text_delete_forward_word (text);
	  else if (event->state & GDK_SHIFT_MASK)
	    {
	      extend_selection = FALSE;
	      gtk_editable_cut_clipboard (editable);
	    }
	  else
	    gtk_text_delete_forward_character (text);
	  break;
	case GDK_Tab:
	  position = text->point.index;
	  gtk_editable_insert_text (editable, "\t", 1, &position);
	  break;
	case GDK_Return:
	  if (event->state & GDK_CONTROL_MASK)
	    gtk_signal_emit_by_name (GTK_OBJECT (text), "activate");
	  else
	    {
	      position = text->point.index;
	      gtk_editable_insert_text (editable, "\n", 1, &position);
	    }
	  break;
	case GDK_Escape:
	  /* Don't insert literally */
	  return_val = FALSE;
	  break;
	  
	default:
	  return_val = FALSE;
	  
	  if (event->state & GDK_CONTROL_MASK)
	    {
	      if ((key >= 'A') && (key <= 'Z'))
		key -= 'A' - 'a';
	      
	      if ((key >= 'a') && (key <= 'z') && control_keys[(int) (key - 'a')])
		{
		  (* control_keys[(int) (key - 'a')]) (editable, event->time);
		  return_val = TRUE;
		}
	      
	      break;
	    }
	  else if (event->state & GDK_MOD1_MASK)
	    {
	      if ((key >= 'A') && (key <= 'Z'))
		key -= 'A' - 'a';
	      
	      if ((key >= 'a') && (key <= 'z') && alt_keys[(int) (key - 'a')])
		{
		  (* alt_keys[(int) (key - 'a')]) (editable, event->time);
		  return_val = TRUE;
		}
	      
	      break;
	    }
	  else if (event->length > 0)
	    {
	      extend_selection = FALSE;
	      
	      if (event->length == 1)
		{
		  gtk_editable_delete_selection (editable);
		  position = text->point.index;
		  gtk_editable_insert_text (editable, &(event->string[0]), 1, &position);
		}
	      
	      return_val = TRUE;
	    }
	  else
	    return_val = FALSE;
	}
      
      if (return_val && (editable->current_pos != initial_pos))
	{
	  if (extend_selection)
	    {
	      if (editable->current_pos < editable->selection_start_pos)
		gtk_text_set_selection (editable, editable->current_pos,
					editable->selection_end_pos);
	      else if (editable->current_pos > editable->selection_end_pos)
		gtk_text_set_selection (editable, editable->selection_start_pos,
					editable->current_pos);
	      else
		{
		  if (extend_start)
		    gtk_text_set_selection (editable, editable->current_pos,
					    editable->selection_end_pos);
		  else
		    gtk_text_set_selection (editable, editable->selection_start_pos,
					    editable->current_pos);
		}
	    }
	  else
	    gtk_text_set_selection (editable, 0, 0);
	  
	  gtk_editable_claim_selection (editable,
					editable->selection_start_pos != editable->selection_end_pos,
					event->time);
	}
    }
  
  return return_val;
}

static gint
gtk_text_focus_in (GtkWidget     *widget,
		   GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  TDEBUG (("in gtk_text_focus_in\n"));
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);
  
#ifdef USE_XIM
  if (GTK_EDITABLE(widget)->ic)
    gdk_im_begin (GTK_EDITABLE(widget)->ic, GTK_TEXT(widget)->text_area);
#endif
  
  draw_cursor (GTK_TEXT(widget), TRUE);
  
  return FALSE;
}

static gint
gtk_text_focus_out (GtkWidget     *widget,
		    GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEXT (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  TDEBUG (("in gtk_text_focus_out\n"));
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);
  
  undraw_cursor (GTK_TEXT(widget), TRUE);
  
#ifdef USE_XIM
  gdk_im_end ();
#endif
  
  return FALSE;
}

static void
gtk_text_adjustment (GtkAdjustment *adjustment,
		     GtkText       *text)
{
  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));
  
  /* Just ignore it if we haven't been size-allocated and realized yet */
  if (text->line_start_cache == NULL) 
    return;
  
  if (adjustment == text->hadj)
    {
      g_warning ("horizontal scrolling not implemented");
    }
  else
    {
      gint diff = ((gint)adjustment->value) - text->last_ver_value;
      
      if (diff != 0)
	{
	  undraw_cursor (text, FALSE);
	  
	  if (diff > 0)
	    scroll_down (text, diff);
	  else /* if (diff < 0) */
	    scroll_up (text, diff);
	  
	  draw_cursor (text, FALSE);
	  
	  text->last_ver_value = adjustment->value;
	}
    }
}

static void
gtk_text_disconnect (GtkAdjustment *adjustment,
		     GtkText       *text)
{
  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (text != NULL);
  g_return_if_fail (GTK_IS_TEXT (text));
  
  if (adjustment == text->hadj)
    text->hadj = NULL;
  if (adjustment == text->vadj)
    text->vadj = NULL;
}


static GtkPropertyMark
find_this_line_start_mark (GtkText* text, guint point_position, const GtkPropertyMark* near)
{
  GtkPropertyMark mark;
  
  mark = find_mark_near (text, point_position, near);
  
  while (mark.index > 0 &&
	 GTK_TEXT_INDEX (text, mark.index - 1) != LINE_DELIM)
    decrement_mark (&mark);
  
  return mark;
}

static void
init_tab_cont (GtkText* text, PrevTabCont* tab_cont)
{
  tab_cont->pixel_offset          = 0;
  tab_cont->tab_start.tab_stops   = text->tab_stops;
  tab_cont->tab_start.to_next_tab = (gulong) text->tab_stops->data;
  
  if (!tab_cont->tab_start.to_next_tab)
    tab_cont->tab_start.to_next_tab = text->default_tab_width;
}

static void
line_params_iterate (GtkText* text,
		     const GtkPropertyMark* mark0,
		     const PrevTabCont* tab_mark0,
		     gint8 alloc,
		     void* data,
		     LineIteratorFunction iter)
     /* mark0 MUST be a real line start.  if ALLOC, allocate line params
      * from a mem chunk.  DATA is passed to ITER_CALL, which is called
      * for each line following MARK, iteration continues unless ITER_CALL
      * returns TRUE. */
{
  GtkPropertyMark mark = *mark0;
  PrevTabCont  tab_conts[2];
  LineParams   *lp, lpbuf;
  gint         tab_cont_index = 0;
  
  if (tab_mark0)
    tab_conts[0] = *tab_mark0;
  else
    init_tab_cont (text, tab_conts);
  
  for (;;)
    {
      if (alloc)
	lp = g_chunk_new (LineParams, params_mem_chunk);
      else
	lp = &lpbuf;
      
      *lp = find_line_params (text, &mark, tab_conts + tab_cont_index,
			      tab_conts + (tab_cont_index + 1) % 2);
      
      if ((*iter) (text, lp, data))
	return;
      
      if (LAST_INDEX (text, lp->end))
	break;
      
      mark = lp->end;
      advance_mark (&mark);
      tab_cont_index = (tab_cont_index + 1) % 2;
    }
}

static gint
fetch_lines_iterator (GtkText* text, LineParams* lp, void* data)
{
  FetchLinesData *fldata = (FetchLinesData*) data;
  
  fldata->new_lines = g_list_prepend (fldata->new_lines, lp);
  
  switch (fldata->fl_type)
    {
    case FetchLinesCount:
      if (!text->line_wrap || !lp->wraps)
	fldata->data += 1;
      
      if (fldata->data >= fldata->data_max)
	return TRUE;
      
      break;
    case FetchLinesPixels:
      
      fldata->data += LINE_HEIGHT(*lp);
      
      if (fldata->data >= fldata->data_max)
	return TRUE;
      
      break;
    }
  
  return FALSE;
}

static GList*
fetch_lines (GtkText* text,
	     const GtkPropertyMark* mark0,
	     const PrevTabCont* tab_cont0,
	     FLType fl_type,
	     gint data)
{
  FetchLinesData fl_data;
  
  fl_data.new_lines = NULL;
  fl_data.data      = 0;
  fl_data.data_max  = data;
  fl_data.fl_type   = fl_type;
  
  line_params_iterate (text, mark0, tab_cont0, TRUE, &fl_data, fetch_lines_iterator);
  
  return g_list_reverse (fl_data.new_lines);
}

static void
fetch_lines_backward (GtkText* text)
{
  GList* new_lines = NULL, *new_line_start;
  GtkPropertyMark mark;
  
  if (CACHE_DATA(text->line_start_cache).start.index == 0)
    return;
  
  mark = find_this_line_start_mark (text,
				    CACHE_DATA(text->line_start_cache).start.index - 1,
				    &CACHE_DATA(text->line_start_cache).start);
  
  new_line_start = new_lines = fetch_lines (text, &mark, NULL, FetchLinesCount, 1);
  
  while (new_line_start->next)
    new_line_start = new_line_start->next;
  
  new_line_start->next = text->line_start_cache;
  text->line_start_cache->prev = new_line_start;
}

static void
fetch_lines_forward (GtkText* text, gint line_count)
{
  GtkPropertyMark mark;
  GList* line = text->line_start_cache;
  
  while(line->next)
    line = line->next;
  
  mark = CACHE_DATA(line).end;
  
  if (LAST_INDEX (text, mark))
    return;
  
  advance_mark(&mark);
  
  line->next = fetch_lines (text, &mark, &CACHE_DATA(line).tab_cont_next, FetchLinesCount, line_count);
  
  if (line->next)
    line->next->prev = line;
}

/* Compute the number of lines, and vertical pixels for n characters
 * starting from the point 
 */
static void
compute_lines_pixels (GtkText* text, guint char_count,
		      guint *lines, guint *pixels)
{
  GList *line = text->current_line;
  gint chars_left = char_count;
  
  *lines = 0;
  *pixels = 0;
  
  /* If chars_left == 0, that means we're joining two lines in a
   * deletion, so add in the values for the next line as well 
   */
  for (; line && chars_left >= 0; line = line->next)
    {
      *pixels += LINE_HEIGHT(CACHE_DATA(line));
      
      if (line == text->current_line)
	chars_left -= CACHE_DATA(line).end.index - text->point.index + 1;
      else
	chars_left -= CACHE_DATA(line).end.index - CACHE_DATA(line).start.index + 1;
      
      if (!text->line_wrap || !CACHE_DATA(line).wraps)
	*lines += 1;
      else
	if (chars_left < 0)
	  chars_left = 0;	/* force another loop */
      
      if (!line->next)
	fetch_lines_forward (text, 1);
    }
}

static gint
total_line_height (GtkText* text, GList* line, gint line_count)
{
  gint height = 0;
  
  for (; line && line_count > 0; line = line->next)
    {
      height += LINE_HEIGHT(CACHE_DATA(line));
      
      if (!text->line_wrap || !CACHE_DATA(line).wraps)
	line_count -= 1;
      
      if (!line->next)
	fetch_lines_forward (text, line_count);
    }
  
  return height;
}

static void
swap_lines (GtkText* text, GList* old, GList* new, guint old_line_count)
{
  if (old == text->line_start_cache)
    {
      GList* last;
      
      for (; old_line_count > 0; old_line_count -= 1)
	{
	  while (text->line_start_cache &&
		 text->line_wrap &&
		 CACHE_DATA(text->line_start_cache).wraps)
	    remove_cache_line(text, text->line_start_cache);
	  
	  remove_cache_line(text, text->line_start_cache);
	}
      
      last = g_list_last (new);
      
      last->next = text->line_start_cache;
      
      if (text->line_start_cache)
	text->line_start_cache->prev = last;
      
      text->line_start_cache = new;
    }
  else
    {
      GList *last;
      
      g_assert (old->prev);
      
      last = old->prev;
      
      for (; old_line_count > 0; old_line_count -= 1)
	{
	  while (old && text->line_wrap && CACHE_DATA(old).wraps)
	    old = remove_cache_line (text, old);
	  
	  old = remove_cache_line (text, old);
	}
      
      last->next = new;
      new->prev = last;
      
      last = g_list_last (new);
      
      last->next = old;
      
      if (old)
	old->prev = last;
    }
}

static void
correct_cache_delete (GtkText* text, gint nchars, gint lines)
{
  GList* cache = text->current_line;
  gint i;
  
  for (i = 0; cache && i < lines; i += 1, cache = cache->next)
    /* nothing */;
  
  for (; cache; cache = cache->next)
    {
      GtkPropertyMark *start = &CACHE_DATA(cache).start;
      GtkPropertyMark *end = &CACHE_DATA(cache).end;
      
      start->index -= nchars;
      end->index -= nchars;
      
      if (LAST_INDEX (text, text->point) &&
	  start->index == text->point.index)
	*start = text->point;
      else if (start->property == text->point.property)
	start->offset = start->index - (text->point.index - text->point.offset);
      
      if (LAST_INDEX (text, text->point) &&
	  end->index == text->point.index)
	*end = text->point;
      if (end->property == text->point.property)
	end->offset = end->index - (text->point.index - text->point.offset);
      
      /*TEXT_ASSERT_MARK(text, start, "start");*/
      /*TEXT_ASSERT_MARK(text, end, "end");*/
    }
}

static void
delete_expose (GtkText* text, guint nchars, guint old_lines, guint old_pixels)
{
  GtkWidget *widget = GTK_WIDGET (text);
  
  gint pixel_height;
  guint new_pixels = 0;
  GdkRectangle rect;
  GList* new_line = NULL;
  gint width, height;
  
  text->cursor_virtual_x = 0;
  
  correct_cache_delete (text, nchars, old_lines);
  
  pixel_height = pixel_height_of(text, text->current_line) -
    LINE_HEIGHT(CACHE_DATA(text->current_line));
  
  if (CACHE_DATA(text->current_line).start.index == text->point.index)
    CACHE_DATA(text->current_line).start = text->point;
  
  new_line = fetch_lines (text,
			  &CACHE_DATA(text->current_line).start,
			  &CACHE_DATA(text->current_line).tab_cont,
			  FetchLinesCount,
			  1);
  
  swap_lines (text, text->current_line, new_line, old_lines);
  
  text->current_line = new_line;
  
  new_pixels = total_line_height (text, new_line, 1);
  
  gdk_window_get_size (text->text_area, &width, &height);
  
  if (old_pixels != new_pixels)
    {
      if (!widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  gdk_draw_pixmap (text->text_area,
			   text->gc,
			   text->text_area,
			   0,
			   pixel_height + old_pixels,
			   0,
			   pixel_height + new_pixels,
			   width,
			   height);
	}
      text->vadj->upper += new_pixels;
      text->vadj->upper -= old_pixels;
      adjust_adj (text, text->vadj);
    }
  
  rect.x = 0;
  rect.y = pixel_height;
  rect.width = width;
  rect.height = new_pixels;
  
  expose_text (text, &rect, FALSE);
  gtk_text_draw_focus ( (GtkWidget *) text);
  
  text->cursor_mark = text->point;
  
  find_cursor (text, TRUE);
  
  if (old_pixels != new_pixels)
    {
      if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  rect.x = 0;
	  rect.y = pixel_height + new_pixels;
	  rect.width = width;
	  rect.height = height - rect.y;
	  
	  expose_text (text, &rect, FALSE);
	}
      else
	process_exposes (text);
    }
  
  TEXT_ASSERT (text);
  TEXT_SHOW(text);
}

/* note, the point has already been moved forward */
static void
correct_cache_insert (GtkText* text, gint nchars)
{
  GList *cache;
  GtkPropertyMark *start;
  GtkPropertyMark *end;
  gboolean was_split = FALSE;
  
  /* We need to distinguish whether the property was split in the
   * insert or not, so we check if the point (which points after
   * the insertion here), points to the same character as the
   * point before. Ugh.
   */
  if (nchars > 0)
    {
      GtkPropertyMark tmp_mark = text->point;
      move_mark_n (&tmp_mark, -1);
      
      if (tmp_mark.property != text->point.property)
	was_split = TRUE;
    }
  
  /* If we inserted a property exactly at the beginning of the
   * line, we have to correct here, or fetch_lines will
   * fetch junk.
   */
  start = &CACHE_DATA(text->current_line).start;

  /* Check if if we split exactly at the beginning of the line:
   * (was_split won't be set if we are inserting at the end of the text, 
   *  so we don't check)
   */
  if (start->offset ==  MARK_CURRENT_PROPERTY (start)->length)
    SET_PROPERTY_MARK (start, start->property->next, 0);
  /* Check if we inserted a property at the beginning of the text: */
  else if (was_split &&
	   (start->property == text->point.property) &&
	   (start->index == text->point.index - nchars))
    SET_PROPERTY_MARK (start, start->property->prev, 0);

  /* Now correct the offsets, and check for start or end marks that
   * are after the point, yet point to a property before the point's
   * property. This indicates that they are meant to point to the
   * second half of a property we split in insert_text_property(), so
   * we fix them up that way.  
   */
  cache = text->current_line->next;
  
  for (; cache; cache = cache->next)
    {
      start = &CACHE_DATA(cache).start;
      end = &CACHE_DATA(cache).end;
      
      if (LAST_INDEX (text, text->point) &&
	  start->index == text->point.index)
	*start = text->point;
      else if (start->index >= text->point.index - nchars)
	{
	  if (!was_split && start->property == text->point.property)
	    move_mark_n(start, nchars);
	  else
	    {
	      if (start->property->next &&
		  (start->property->next->next == text->point.property))
		{
		  g_assert (start->offset >=  MARK_CURRENT_PROPERTY (start)->length);
		  start->offset -= MARK_CURRENT_PROPERTY (start)->length;
		  start->property = text->point.property;
		}
	      start->index += nchars;
	    }
	}
      
      if (LAST_INDEX (text, text->point) &&
	  end->index == text->point.index)
	*end = text->point;
      if (end->index >= text->point.index - nchars)
	{
	  if (!was_split && end->property == text->point.property)
	    move_mark_n(end, nchars);
	  else 
	    {
	      if (end->property->next &&
		  (end->property->next->next == text->point.property))
		{
		  g_assert (end->offset >=  MARK_CURRENT_PROPERTY (end)->length);
		  end->offset -= MARK_CURRENT_PROPERTY (end)->length;
		  end->property = text->point.property;
		}
	      end->index += nchars;
	    }
	}
      
      /*TEXT_ASSERT_MARK(text, start, "start");*/
      /*TEXT_ASSERT_MARK(text, end, "end");*/
    }
}


static void
insert_expose (GtkText* text, guint old_pixels, gint nchars,
	       guint new_line_count)
{
  GtkWidget *widget = GTK_WIDGET (text);
  
  gint pixel_height;
  guint new_pixels = 0;
  GdkRectangle rect;
  GList* new_lines = NULL;
  gint width, height;
  
  text->cursor_virtual_x = 0;
  
  undraw_cursor (text, FALSE);
  
  correct_cache_insert (text, nchars);
  
  TEXT_SHOW_ADJ (text, text->vadj, "vadj");
  
  pixel_height = pixel_height_of(text, text->current_line) -
    LINE_HEIGHT(CACHE_DATA(text->current_line));
  
  new_lines = fetch_lines (text,
			   &CACHE_DATA(text->current_line).start,
			   &CACHE_DATA(text->current_line).tab_cont,
			   FetchLinesCount,
			   new_line_count);
  
  swap_lines (text, text->current_line, new_lines, 1);
  
  text->current_line = new_lines;
  
  new_pixels = total_line_height (text, new_lines, new_line_count);
  
  gdk_window_get_size (text->text_area, &width, &height);
  
  if (old_pixels != new_pixels)
    {
      if (!widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  gdk_draw_pixmap (text->text_area,
			   text->gc,
			   text->text_area,
			   0,
			   pixel_height + old_pixels,
			   0,
			   pixel_height + new_pixels,
			   width,
			   height + (old_pixels - new_pixels) - pixel_height);
	  
	}
      text->vadj->upper += new_pixels;
      text->vadj->upper -= old_pixels;
      adjust_adj (text, text->vadj);
    }
  
  rect.x = 0;
  rect.y = pixel_height;
  rect.width = width;
  rect.height = new_pixels;
  
  expose_text (text, &rect, FALSE);
  gtk_text_draw_focus ( (GtkWidget *) text);
  
  text->cursor_mark = text->point;
  
  find_cursor (text, TRUE);
  
  draw_cursor (text, FALSE);
  
  if (old_pixels != new_pixels)
    {
      if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  rect.x = 0;
	  rect.y = pixel_height + new_pixels;
	  rect.width = width;
	  rect.height = height - rect.y;
	  
	  expose_text (text, &rect, FALSE);
	}
      else
	process_exposes (text);
    }
  
  TEXT_SHOW_ADJ (text, text->vadj, "vadj");
  TEXT_ASSERT (text);
  TEXT_SHOW(text);
}

/* Text property functions */

static guint
font_hash (gconstpointer font)
{
  return gdk_font_id ((const GdkFont*) font);
}

static GHashTable *font_cache_table = NULL;

static GtkTextFont*
get_text_font (GdkFont* gfont)
{
  GtkTextFont* tf;
  gint i;
  
  if (!font_cache_table)
    font_cache_table = g_hash_table_new (font_hash, (GCompareFunc) gdk_font_equal);
  
  tf = g_hash_table_lookup (font_cache_table, gfont);
  
  if (tf)
    {
      tf->ref_count++;
      return tf;
    }

  tf = g_new (GtkTextFont, 1);
  tf->ref_count = 1;

  tf->gdk_font = gfont;
  gdk_font_ref (gfont);
  
  for(i = 0; i < 256; i += 1)
    tf->char_widths[i] = gdk_char_width (gfont, (char)i);
  
  g_hash_table_insert (font_cache_table, gfont, tf);
  
  return tf;
}

static void
text_font_unref (GtkTextFont *text_font)
{
  text_font->ref_count--;
  if (text_font->ref_count == 0)
    {
      g_hash_table_remove (font_cache_table, text_font->gdk_font);
      gdk_font_unref (text_font->gdk_font);
      g_free (text_font);
    }
}

static gint
text_properties_equal (TextProperty* prop, GdkFont* font, GdkColor *fore, GdkColor *back)
{
  if (prop->flags & PROPERTY_FONT)
    {
      gboolean retval;
      GtkTextFont *text_font;

      if (!font)
	return FALSE;

      text_font = get_text_font (font);

      retval = (prop->font == text_font);
      text_font_unref (text_font);
      
      if (!retval)
	return FALSE;
    }
  else
    if (font != NULL)
      return FALSE;

  if (prop->flags & PROPERTY_FOREGROUND)
    {
      if (!fore || !gdk_color_equal (&prop->fore_color, fore))
	return FALSE;
    }
  else
    if (fore != NULL)
      return FALSE;

  if (prop->flags & PROPERTY_BACKGROUND)
    {
      if (!back || !gdk_color_equal (&prop->fore_color, fore))
	return FALSE;
    }
  else
    if (fore != NULL)
      return FALSE;
  
  return TRUE;
}

static void
realize_property (GtkText *text, TextProperty *prop)
{
  GdkColormap *colormap = gtk_widget_get_colormap (GTK_WIDGET (text));

  if (prop->flags & PROPERTY_FOREGROUND)
    gdk_colormap_alloc_color (colormap, &prop->fore_color, FALSE, FALSE);
  
  if (prop->flags & PROPERTY_BACKGROUND)
    gdk_colormap_alloc_color (colormap, &prop->back_color, FALSE, FALSE);
}

static void
realize_properties (GtkText *text)
{
  GList *tmp_list = text->text_properties;

  while (tmp_list)
    {
      realize_property (text, tmp_list->data);
      
      tmp_list = tmp_list->next;
    }
}

static void
unrealize_property (GtkText *text, TextProperty *prop)
{
  GdkColormap *colormap = gtk_widget_get_colormap (GTK_WIDGET (text));

  if (prop->flags & PROPERTY_FOREGROUND)
    gdk_colormap_free_colors (colormap, &prop->fore_color, 1);
  
  if (prop->flags & PROPERTY_BACKGROUND)
    gdk_colormap_free_colors (colormap, &prop->back_color, 1);
}

static void
unrealize_properties (GtkText *text)
{
  GList *tmp_list = text->text_properties;

  while (tmp_list)
    {
      unrealize_property (text, tmp_list->data);

      tmp_list = tmp_list->next;
    }
}

static TextProperty*
new_text_property (GtkText *text, GdkFont *font, GdkColor* fore, 
		   GdkColor* back, guint length)
{
  TextProperty *prop;
  
  if (text_property_chunk == NULL)
    {
      text_property_chunk = g_mem_chunk_new ("text property mem chunk",
					     sizeof(TextProperty),
					     1024*sizeof(TextProperty),
					     G_ALLOC_AND_FREE);
    }
  
  prop = g_chunk_new(TextProperty, text_property_chunk);

  prop->flags = 0;
  if (font)
    {
      prop->flags |= PROPERTY_FONT;
      prop->font = get_text_font (font);
    }
  else
    prop->font = NULL;
  
  if (fore)
    {
      prop->flags |= PROPERTY_FOREGROUND;
      prop->fore_color = *fore;
    }
      
  if (back)
    {
      prop->flags |= PROPERTY_BACKGROUND;
      prop->back_color = *back;
    }

  prop->length = length;

  if (GTK_WIDGET_REALIZED (text))
    realize_property (text, prop);

  return prop;
}

static void
destroy_text_property (TextProperty *prop)
{
  if (prop->font)
    text_font_unref (prop->font);
  
  g_mem_chunk_free (text_property_chunk, prop);
}

/* Flop the memory between the point and the gap around like a
 * dead fish. */
static void
move_gap_to_point (GtkText* text)
{
  if (text->gap_position < text->point.index)
    {
      gint diff = text->point.index - text->gap_position;
      
      g_memmove (text->text + text->gap_position,
		 text->text + text->gap_position + text->gap_size,
		 diff);
      
      text->gap_position = text->point.index;
    }
  else if (text->gap_position > text->point.index)
    {
      gint diff = text->gap_position - text->point.index;
      
      g_memmove (text->text + text->point.index + text->gap_size,
		 text->text + text->point.index,
		 diff);
      
      text->gap_position = text->point.index;
    }
}

/* Increase the gap size. */
static void
make_forward_space (GtkText* text, guint len)
{
  if (text->gap_size < len)
    {
      guint sum = MAX(2*len, MIN_GAP_SIZE) + text->text_end;
      
      if (sum >= text->text_len)
	{
	  guint i = 1;
	  
	  while (i <= sum) i <<= 1;
	  
	  text->text = (guchar*)g_realloc(text->text, i);
	}
      
      g_memmove (text->text + text->gap_position + text->gap_size + 2*len,
		 text->text + text->gap_position + text->gap_size,
		 text->text_end - (text->gap_position + text->gap_size));
      
      text->text_end += len*2;
      text->gap_size += len*2;
    }
}

/* Inserts into the text property list a list element that guarantees
 * that for len characters following the point, text has the correct
 * property.  does not move point.  adjusts text_properties_point and
 * text_properties_point_offset relative to the current value of
 * point. */
static void
insert_text_property (GtkText* text, GdkFont* font,
		      GdkColor *fore, GdkColor* back, guint len)
{
  GtkPropertyMark *mark = &text->point;
  TextProperty* forward_prop = MARK_CURRENT_PROPERTY(mark);
  TextProperty* backward_prop = MARK_PREV_PROPERTY(mark);
  
  if (MARK_OFFSET(mark) == 0)
    {
      /* Point is on the boundary of two properties.
       * If it is the same as either, grow, else insert
       * a new one. */
      
      if (text_properties_equal(forward_prop, font, fore, back))
	{
	  /* Grow the property in front of us. */
	  
	  MARK_PROPERTY_LENGTH(mark) += len;
	}
      else if (backward_prop &&
	       text_properties_equal(backward_prop, font, fore, back))
	{
	  /* Grow property behind us, point property and offset
	   * change. */
	  
	  SET_PROPERTY_MARK (&text->point,
			     MARK_PREV_LIST_PTR (mark),
			     backward_prop->length);
	  
	  backward_prop->length += len;
	}
      else if ((MARK_NEXT_LIST_PTR(mark) == NULL) &&
	       (forward_prop->length == 1))
	{
	  /* Next property just has last position, take it over */

	  if (GTK_WIDGET_REALIZED (text))
	    unrealize_property (text, forward_prop);

	  forward_prop->flags = 0;
	  if (font)
	    {
	      forward_prop->flags |= PROPERTY_FONT;
	      forward_prop->font = get_text_font (font);
	    }
	  else
	    forward_prop->font = NULL;
	    
	  if (fore)
	    {
	      forward_prop->flags |= PROPERTY_FOREGROUND;
	      forward_prop->fore_color = *fore;
	    }
	  if (back)
	    {
	      forward_prop->flags |= PROPERTY_BACKGROUND;
	      forward_prop->back_color = *back;
	    }
	  forward_prop->length += len;

	  if (GTK_WIDGET_REALIZED (text))
	    realize_property (text, forward_prop);
	}
      else
	{
	  /* Splice a new property into the list. */
	  
	  GList* new_prop = g_list_alloc();
	  
	  new_prop->next = MARK_LIST_PTR(mark);
	  new_prop->prev = MARK_PREV_LIST_PTR(mark);
	  new_prop->next->prev = new_prop;
	  
	  if (new_prop->prev)
	    new_prop->prev->next = new_prop;

	  new_prop->data = new_text_property (text, font, fore, back, len);

	  SET_PROPERTY_MARK (mark, new_prop, 0);
	}
    }
  else
    {
      /* The following will screw up the line_start cache,
       * we'll fix it up in correct_cache_insert
       */
      
      /* In the middle of forward_prop, if properties are equal,
       * just add to its length, else split it into two and splice
       * in a new one. */
      if (text_properties_equal (forward_prop, font, fore, back))
	{
	  forward_prop->length += len;
	}
      else if ((MARK_NEXT_LIST_PTR(mark) == NULL) &&
	       (MARK_OFFSET(mark) + 1 == forward_prop->length))
	{
	  /* Inserting before only the last position in the text */
	  
	  GList* new_prop;
	  forward_prop->length -= 1;
	  
	  new_prop = g_list_alloc();
	  new_prop->data = new_text_property (text, font, fore, back, len+1);
	  new_prop->prev = MARK_LIST_PTR(mark);
	  new_prop->next = NULL;
	  MARK_NEXT_LIST_PTR(mark) = new_prop;
	  
	  SET_PROPERTY_MARK (mark, new_prop, 0);
	}
      else
	{
	  GList* new_prop = g_list_alloc();
	  GList* new_prop_forward = g_list_alloc();
	  gint old_length = forward_prop->length;
	  GList* next = MARK_NEXT_LIST_PTR(mark);
	  
	  /* Set the new lengths according to where they are split.  Construct
	   * two new properties. */
	  forward_prop->length = MARK_OFFSET(mark);

	  new_prop_forward->data = 
	    new_text_property(text,
			      forward_prop->flags & PROPERTY_FONT ? 
                                     forward_prop->font->gdk_font : NULL,
			      forward_prop->flags & PROPERTY_FOREGROUND ? 
  			             &forward_prop->fore_color : NULL,
			      forward_prop->flags & PROPERTY_BACKGROUND ? 
  			             &forward_prop->back_color : NULL,
			      old_length - forward_prop->length);

	  new_prop->data = new_text_property(text, font, fore, back, len);

	  /* Now splice things in. */
	  MARK_NEXT_LIST_PTR(mark) = new_prop;
	  new_prop->prev = MARK_LIST_PTR(mark);
	  
	  new_prop->next = new_prop_forward;
	  new_prop_forward->prev = new_prop;
	  
	  new_prop_forward->next = next;
	  
	  if (next)
	    next->prev = new_prop_forward;
	  
	  SET_PROPERTY_MARK (mark, new_prop, 0);
	}
    }
  
  while (text->text_properties_end->next)
    text->text_properties_end = text->text_properties_end->next;
  
  while (text->text_properties->prev)
    text->text_properties = text->text_properties->prev;
}

static void
delete_text_property (GtkText* text, guint nchars)
{
  /* Delete nchars forward from point. */
  
  /* Deleting text properties is problematical, because we
   * might be storing around marks pointing to a property.
   *
   * The marks in question and how we handle them are:
   *
   *  point: We know the new value, since it will be at the
   *         end of the deleted text, and we move it there
   *         first.
   *  cursor: We just remove the mark and set it equal to the
   *         point after the operation.
   *  line-start cache: We replace most affected lines.
   *         The current line gets used to fetch the new
   *         lines so, if necessary, (delete at the beginning
   *         of a line) we fix it up by setting it equal to the
   *         point.
   */
  
  TextProperty *prop;
  GList        *tmp;
  gint          is_first;
  
  for(; nchars; nchars -= 1)
    {
      prop = MARK_CURRENT_PROPERTY(&text->point);
      
      prop->length -= 1;
      
      if (prop->length == 0)
	{
	  tmp = MARK_LIST_PTR (&text->point);
	  
	  is_first = tmp == text->text_properties;
	  
	  MARK_LIST_PTR (&text->point) = g_list_remove_link (tmp, tmp);
	  text->point.offset = 0;

	  if (GTK_WIDGET_REALIZED (text))
	    unrealize_property (text, prop);

	  destroy_text_property (prop);
	  g_list_free_1 (tmp);
	  
	  prop = MARK_CURRENT_PROPERTY (&text->point);
	  
	  if (is_first)
	    text->text_properties = MARK_LIST_PTR (&text->point);
	  
	  g_assert (prop->length != 0);
	}
      else if (prop->length == text->point.offset)
	{
	  MARK_LIST_PTR (&text->point) = MARK_NEXT_LIST_PTR (&text->point);
	  text->point.offset = 0;
	}
    }
  
  /* Check to see if we have just the single final position remaining
   * along in a property; if so, combine it with the previous property
   */
  if (LAST_INDEX (text, text->point) && 
      (MARK_OFFSET (&text->point) == 0) &&
      (MARK_PREV_LIST_PTR(&text->point) != NULL))
    {
      tmp = MARK_LIST_PTR (&text->point);
      prop = MARK_CURRENT_PROPERTY(&text->point);
      
      MARK_LIST_PTR (&text->point) = MARK_PREV_LIST_PTR (&text->point);
      MARK_CURRENT_PROPERTY(&text->point)->length += 1;
      MARK_NEXT_LIST_PTR(&text->point) = NULL;
      
      text->point.offset = MARK_CURRENT_PROPERTY(&text->point)->length - 1;
      
      if (GTK_WIDGET_REALIZED (text))
	unrealize_property (text, prop);

      destroy_text_property (prop);
      g_list_free_1 (tmp);
    }
}

static void
init_properties (GtkText *text)
{
  if (!text->text_properties)
    {
      text->text_properties = g_list_alloc();
      text->text_properties->next = NULL;
      text->text_properties->prev = NULL;
      text->text_properties->data = new_text_property (text, NULL, NULL, NULL, 1);
      text->text_properties_end = text->text_properties;
      
      SET_PROPERTY_MARK (&text->point, text->text_properties, 0);
      
      text->point.index = 0;
    }
}


/**********************************************************************/
/*			   Property Movement                          */
/**********************************************************************/

static void
move_mark_n (GtkPropertyMark* mark, gint n)
{
  if (n > 0)
    advance_mark_n(mark, n);
  else if (n < 0)
    decrement_mark_n(mark, -n);
}

static void
advance_mark_n (GtkPropertyMark* mark, gint n)
{
  gint i;
  
  g_assert (n > 0);
  
  for (i = 0; i < n; i += 1)
    advance_mark (mark);
}

static void
advance_mark (GtkPropertyMark* mark)
{
  TextProperty* prop = MARK_CURRENT_PROPERTY (mark);
  
  mark->index += 1;
  
  if (prop->length > mark->offset + 1)
    mark->offset += 1;
  else
    {
      mark->property = MARK_NEXT_LIST_PTR (mark);
      mark->offset   = 0;
    }
}

static void
decrement_mark (GtkPropertyMark* mark)
{
  mark->index -= 1;
  
  if (mark->offset > 0)
    mark->offset -= 1;
  else
    {
      mark->property = MARK_PREV_LIST_PTR (mark);
      mark->offset   = MARK_CURRENT_PROPERTY (mark)->length - 1;
    }
}

static void
decrement_mark_n (GtkPropertyMark* mark, gint n)
{
  gint i;
  
  g_assert (n > 0);
  
  for (i = 0; i < n; i += 1)
    decrement_mark (mark);
}

static GtkPropertyMark
find_mark (GtkText* text, guint mark_position)
{
  return find_mark_near (text, mark_position, &text->point);
}

/* This can be optimized in two ways.
 * First, advances can be made in units of the current TextProperty
 * length, when possible.  This will reduce computation and function
 * call overhead.
 *
 * You can also start from the end, what a drag.
 */
static GtkPropertyMark
find_mark_near (GtkText* text, guint mark_position, const GtkPropertyMark* near)
{
  gint diffa;
  gint diffb;
  
  GtkPropertyMark mark;
  
  if (!near)
    diffa = mark_position + 1;
  else
    diffa = mark_position - near->index;
  
  diffb = mark_position;
  
  if (diffa < 0)
    diffa = -diffa;
  
  if (diffa <= diffb)
    {
      mark = *near;
    }
  else
    {
      mark.index = 0;
      mark.property = text->text_properties;
      mark.offset = 0;
    }
  
  if (mark.index > mark_position)
    {
      while (mark.index > mark_position)
	decrement_mark (&mark);
    }
  else
    {
      while (mark_position > mark.index)
	advance_mark (&mark);
    }
  
  return mark;
}

/* This routine must be called with scroll == FALSE, only when
 * point is at least partially on screen
 */

static void
find_line_containing_point (GtkText* text, guint point,
			    gboolean scroll)
{
  GList* cache;
  gint height;
  
  text->current_line = NULL;
  
  if (!text->line_start_cache->next)
    {
      /* @@@ Its visible, right? */
      text->current_line = text->line_start_cache;
      return;
    }
  
  while ( ( scroll && (text->first_cut_pixels != 0) &&
	    (CACHE_DATA(text->line_start_cache->next).start.index > point) ) ||
	  ( (text->first_cut_pixels == 0) &&
	    (CACHE_DATA(text->line_start_cache).start.index > point) ) )
    {
      scroll_int (text, - SCROLL_PIXELS);
      g_assert (text->line_start_cache->next);
    }
  
  TEXT_SHOW (text);
  gdk_window_get_size (text->text_area, NULL, &height);
  
  for (cache = text->line_start_cache; cache; cache = cache->next)
    {
      guint lph;
      
      if (CACHE_DATA(cache).end.index >= point ||
	  LAST_INDEX(text, CACHE_DATA(cache).end))
	{
	  text->current_line = cache; /* LOOK HERE, this proc has an
				       * important side effect. */
	  return;
	}
      
      TEXT_SHOW_LINE (text, cache, "cache");
      
      if (cache->next == NULL)
	fetch_lines_forward (text, 1);
      
      if (scroll)
	{
	  lph = pixel_height_of (text, cache->next);
	  
	  /* Scroll the bottom of the line is on screen, or until
	   * the line is the first onscreen line.
	   */
	  while (cache->next != text->line_start_cache && lph > height)
	    {
	      TEXT_SHOW_LINE (text, cache, "cache");
	      TEXT_SHOW_LINE (text, cache->next, "cache->next");
	      scroll_int (text, LINE_HEIGHT(CACHE_DATA(cache->next)));
	      lph = pixel_height_of (text, cache->next);
	    }
	}
    }
  
  g_assert_not_reached (); /* Must set text->current_line here */
}

static guint
pixel_height_of (GtkText* text, GList* cache_line)
{
  gint pixels = - text->first_cut_pixels;
  GList *cache = text->line_start_cache;
  
  while (TRUE) {
    pixels += LINE_HEIGHT (CACHE_DATA(cache));
    
    if (cache->data == cache_line->data)
      break;
    
    cache = cache->next;
  }
  
  return pixels;
}

/**********************************************************************/
/*			Search and Placement                          */
/**********************************************************************/

static gint
find_char_width (GtkText* text, const GtkPropertyMark *mark, const TabStopMark *tab_mark)
{
  gchar ch;
  gint16* char_widths;
  
  if (LAST_INDEX (text, *mark))
    return 0;
  
  ch = GTK_TEXT_INDEX (text, mark->index);
  char_widths = MARK_CURRENT_TEXT_FONT (text, mark)->char_widths;

  if (ch == '\t')
    {
      return tab_mark->to_next_tab * char_widths[' '];
    }
  else
    {
      return char_widths[ch & 0xff];
    }
}

static void
advance_tab_mark (GtkText* text, TabStopMark* tab_mark, gchar ch)
{
  if (tab_mark->to_next_tab == 1 || ch == '\t')
    {
      if (tab_mark->tab_stops->next)
	{
	  tab_mark->tab_stops = tab_mark->tab_stops->next;
	  tab_mark->to_next_tab = (gulong) tab_mark->tab_stops->data;
	}
      else
	{
	  tab_mark->to_next_tab = text->default_tab_width;
	}
    }
  else
    {
      tab_mark->to_next_tab -= 1;
    }
}

static void
advance_tab_mark_n (GtkText* text, TabStopMark* tab_mark, gint n)
     /* No tabs! */
{
  while (n--)
    advance_tab_mark (text, tab_mark, 0);
}

static void
find_cursor_at_line (GtkText* text, const LineParams* start_line, gint pixel_height)
{
  gchar ch;
  GtkEditable *editable = (GtkEditable *)text;
  
  GtkPropertyMark mark        = start_line->start;
  TabStopMark  tab_mark    = start_line->tab_cont.tab_start;
  gint         pixel_width = LINE_START_PIXEL (*start_line);
  
  while (mark.index < text->cursor_mark.index)
    {
      pixel_width += find_char_width (text, &mark, &tab_mark);
      
      advance_tab_mark (text, &tab_mark, GTK_TEXT_INDEX(text, mark.index));
      advance_mark (&mark);
    }
  
  text->cursor_pos_x       = pixel_width;
  text->cursor_pos_y       = pixel_height;
  text->cursor_char_offset = start_line->font_descent;
  text->cursor_mark        = mark;
  
  ch = LAST_INDEX (text, mark) ? 
    LINE_DELIM : GTK_TEXT_INDEX (text, mark.index);
  
  if (!isspace(ch))
    text->cursor_char = ch;
  else
    text->cursor_char = 0;
  
#ifdef USE_XIM
  if (gdk_im_ready() && editable->ic && 
      gdk_ic_get_style (editable->ic) & GDK_IM_PREEDIT_POSITION)
    {
      GdkPoint spot;
      
      spot.x = text->cursor_pos_x;
      spot.y = text->cursor_pos_y - text->cursor_char_offset;
      if (MARK_CURRENT_FONT (text, &mark)->type == GDK_FONT_FONTSET)
	gdk_ic_set_attr (editable->ic, "preeditAttributes", 
			 "fontSet", GDK_FONT_XFONT (MARK_CURRENT_FONT (text, &mark)),
			 NULL);
      
      gdk_ic_set_attr (editable->ic, "preeditAttributes", 
		       "spotLocation", &spot,
		       "lineSpace", LINE_HEIGHT (*start_line),
		       "foreground", MARK_CURRENT_FORE (text, &mark)->pixel,
		       "background", MARK_CURRENT_BACK (text, &mark)->pixel,
		       NULL);
    }
#endif 
}

static void
find_cursor (GtkText* text, gboolean scroll)
{
  if (GTK_WIDGET_REALIZED (text))
    {
      find_line_containing_point (text, text->cursor_mark.index, scroll);
      
      if (text->current_line)
	find_cursor_at_line (text,
			     &CACHE_DATA(text->current_line),
			     pixel_height_of(text, text->current_line));
    }
  
  GTK_EDITABLE (text)->current_pos = text->cursor_mark.index;
}

static void
find_mouse_cursor_at_line (GtkText *text, const LineParams* lp,
			   guint line_pixel_height,
			   gint button_x)
{
  GtkPropertyMark mark     = lp->start;
  TabStopMark  tab_mark = lp->tab_cont.tab_start;
  
  gint char_width = find_char_width(text, &mark, &tab_mark);
  gint pixel_width = LINE_START_PIXEL (*lp) + (char_width+1)/2;
  
  text->cursor_pos_y = line_pixel_height;
  
  for (;;)
    {
      gchar ch = LAST_INDEX (text, mark) ? 
	LINE_DELIM : GTK_TEXT_INDEX (text, mark.index);
      
      if (button_x < pixel_width || mark.index == lp->end.index)
	{
	  text->cursor_pos_x       = pixel_width - (char_width+1)/2;
	  text->cursor_mark        = mark;
	  text->cursor_char_offset = lp->font_descent;
	  
	  if (!isspace(ch))
	    text->cursor_char = ch;
	  else
	    text->cursor_char = 0;
	  
	  break;
	}
      
      advance_tab_mark (text, &tab_mark, ch);
      advance_mark (&mark);
      
      pixel_width += char_width/2;
      
      char_width = find_char_width (text, &mark, &tab_mark);
      
      pixel_width += (char_width+1)/2;
    }
}

static void
find_mouse_cursor (GtkText* text, gint x, gint y)
{
  gint pixel_height;
  GList* cache = text->line_start_cache;
  
  g_assert (cache);
  
  pixel_height = - text->first_cut_pixels;
  
  for (; cache; cache = cache->next)
    {
      pixel_height += LINE_HEIGHT(CACHE_DATA(cache));
      
      if (y < pixel_height || !cache->next)
	{
	  find_mouse_cursor_at_line (text, &CACHE_DATA(cache), pixel_height, x);
	  
	  find_cursor (text, FALSE);
	  
	  return;
	}
    }
}

/**********************************************************************/
/*			    Cache Manager                             */
/**********************************************************************/

static void
free_cache (GtkText* text)
{
  GList* cache = text->line_start_cache;
  
  if (cache)
    {
      while (cache->prev)
	cache = cache->prev;
      
      text->line_start_cache = cache;
    }
  
  for (; cache; cache = cache->next)
    g_mem_chunk_free (params_mem_chunk, cache->data);
  
  g_list_free (text->line_start_cache);
  
  text->line_start_cache = NULL;
}

static GList*
remove_cache_line (GtkText* text, GList* member)
{
  GList *list;
  
  if (member == NULL)
    return NULL;
  
  if (member == text->line_start_cache)
    text->line_start_cache = text->line_start_cache->next;
  
  if (member->prev)
    member->prev->next = member->next;
  
  if (member->next)
    member->next->prev = member->prev;
  
  list = member->next;
  
  g_mem_chunk_free (params_mem_chunk, member->data);
  g_list_free_1 (member);
  
  return list;
}

/**********************************************************************/
/*			     Key Motion                               */
/**********************************************************************/

static void
move_cursor_buffer_ver (GtkText *text, int dir)
{
  undraw_cursor (text, FALSE);
  
  if (dir > 0)
    {
      scroll_int (text, text->vadj->upper);
      text->cursor_mark = find_this_line_start_mark (text,
						     TEXT_LENGTH (text),
						     &text->cursor_mark);
    }
  else
    {
      scroll_int (text, - text->vadj->value);
      text->cursor_mark = find_this_line_start_mark (text,
						     0,
						     &text->cursor_mark);
    }
  
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
}

static void
move_cursor_page_ver (GtkText *text, int dir)
{
  scroll_int (text, dir * text->vadj->page_increment);
}

static void
move_cursor_ver (GtkText *text, int count)
{
  gint i;
  GtkPropertyMark mark;
  gint offset;
  
  mark = find_this_line_start_mark (text, text->cursor_mark.index, &text->cursor_mark);
  offset = text->cursor_mark.index - mark.index;
  
  if (offset > text->cursor_virtual_x)
    text->cursor_virtual_x = offset;
  
  if (count < 0)
    {
      if (mark.index == 0)
	return;
      
      decrement_mark (&mark);
      mark = find_this_line_start_mark (text, mark.index, &mark);
    }
  else
    {
      mark = text->cursor_mark;
      
      while (!LAST_INDEX(text, mark) && GTK_TEXT_INDEX(text, mark.index) != LINE_DELIM)
	advance_mark (&mark);
      
      if (LAST_INDEX(text, mark))
	return;
      
      advance_mark (&mark);
    }
  
  for (i=0; i < text->cursor_virtual_x; i += 1, advance_mark(&mark))
    if (LAST_INDEX(text, mark) || GTK_TEXT_INDEX(text, mark.index) == LINE_DELIM)
      break;
  
  undraw_cursor (text, FALSE);
  
  text->cursor_mark = mark;
  
  find_cursor (text, TRUE);
  
  draw_cursor (text, FALSE);
}

static void
move_cursor_hor (GtkText *text, int count)
{
  /* count should be +-1. */
  if ( (count > 0 && text->cursor_mark.index + count > TEXT_LENGTH(text)) ||
       (count < 0 && text->cursor_mark.index < (- count)) ||
       (count == 0) )
    return;
  
  text->cursor_virtual_x = 0;
  
  undraw_cursor (text, FALSE);
  
  move_mark_n (&text->cursor_mark, count);
  
  find_cursor (text, TRUE);
  
  draw_cursor (text, FALSE);
}

static void 
gtk_text_move_cursor (GtkEditable *editable,
		      gint         x,
		      gint         y)
{
  if (x > 0)
    {
      while (x-- != 0)
	move_cursor_hor (GTK_TEXT (editable), 1);
    }
  else if (x < 0)
    {
      while (x++ != 0)
	move_cursor_hor (GTK_TEXT (editable), -1);
    }
  
  if (y > 0)
    {
      while (y-- != 0)
	move_cursor_ver (GTK_TEXT (editable), 1);
    }
  else if (x < 0)
    {
      while (y++ != 0)
	move_cursor_ver (GTK_TEXT (editable), -1);
    }
}

static void
gtk_text_move_forward_character (GtkText *text)
{
  move_cursor_hor (text, 1);
}

static void
gtk_text_move_backward_character (GtkText *text)
{
  move_cursor_hor (text, -1);
}

static void
gtk_text_move_next_line (GtkText *text)
{
  move_cursor_ver (text, 1);
}

static void
gtk_text_move_previous_line (GtkText *text)
{
  move_cursor_ver (text, -1);
}

static void 
gtk_text_move_word (GtkEditable *editable,
		    gint         n)
{
  if (n > 0)
    {
      while (n-- != 0)
	gtk_text_move_forward_word (GTK_TEXT (editable));
    }
  else if (n < 0)
    {
      while (n++ != 0)
	gtk_text_move_backward_word (GTK_TEXT (editable));
    }
}

static void
gtk_text_move_forward_word (GtkText *text)
{
  text->cursor_virtual_x = 0;
  
  undraw_cursor (text, FALSE);
  
  while (!LAST_INDEX (text, text->cursor_mark) && 
	 !isalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index)))
    advance_mark (&text->cursor_mark);
  
  while (!LAST_INDEX (text, text->cursor_mark) && 
	 isalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index)))
    advance_mark (&text->cursor_mark);
  
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
}

static void
gtk_text_move_backward_word (GtkText *text)
{
  text->cursor_virtual_x = 0;
  
  undraw_cursor (text, FALSE);
  
  while ((text->cursor_mark.index > 0) &&
	 !isalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index-1)))
    decrement_mark (&text->cursor_mark);
  
  while ((text->cursor_mark.index > 0) &&
	 isalnum (GTK_TEXT_INDEX(text, text->cursor_mark.index-1)))
    decrement_mark (&text->cursor_mark);
  
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
}

static void 
gtk_text_move_page (GtkEditable *editable,
		    gint         x,
		    gint         y)
{
  if (y != 0)
    scroll_int (GTK_TEXT (editable), 
		y * GTK_TEXT(editable)->vadj->page_increment);  
}

static void 
gtk_text_move_to_row (GtkEditable *editable,
		      gint         row)
{
}

static void 
gtk_text_move_to_column (GtkEditable *editable,
			 gint         column)
{
  GtkText *text;
  
  text = GTK_TEXT (editable);
  
  text->cursor_virtual_x = 0;	/* FIXME */
  
  undraw_cursor (text, FALSE);
  
  /* Move to the beginning of the line */
  while ((text->cursor_mark.index > 0) &&
	 (GTK_TEXT_INDEX (text, text->cursor_mark.index - 1) != LINE_DELIM))
    decrement_mark (&text->cursor_mark);
  
  while (!LAST_INDEX (text, text->cursor_mark) &&
	 (GTK_TEXT_INDEX (text, text->cursor_mark.index) != LINE_DELIM))
    {
      if (column > 0)
	column--;
      else if (column == 0)
	break;
      
      advance_mark (&text->cursor_mark);
    }
  
  find_cursor (text, TRUE);
  draw_cursor (text, FALSE);
}

static void
gtk_text_move_beginning_of_line (GtkText *text)
{
  gtk_text_move_to_column (GTK_EDITABLE (text), 0);
  
}

static void
gtk_text_move_end_of_line (GtkText *text)
{
  gtk_text_move_to_column (GTK_EDITABLE (text), -1);
}

static void 
gtk_text_kill_char (GtkEditable *editable,
		    gint         direction)
{
  GtkText *text;
  
  text = GTK_TEXT (editable);
  
  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      if (direction >= 0)
	{
	  if (text->point.index + 1 <= TEXT_LENGTH (text))
	    gtk_editable_delete_text (editable, text->point.index, text->point.index + 1);
	}
      else
	{
	  if (text->point.index > 0)
	    gtk_editable_delete_text (editable, text->point.index - 1, text->point.index);
	}
    }
}

static void
gtk_text_delete_forward_character (GtkText *text)
{
  gtk_text_kill_char (GTK_EDITABLE (text), 1);
}

static void
gtk_text_delete_backward_character (GtkText *text)
{
  gtk_text_kill_char (GTK_EDITABLE (text), -1);
}

static void 
gtk_text_kill_word (GtkEditable *editable,
		    gint         direction)
{
  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      gint old_pos = editable->current_pos;
      if (direction >= 0)
	{
	  gtk_text_move_word (editable, 1);
	  gtk_editable_delete_text (editable, old_pos, editable->current_pos);
	}
      else
	{
	  gtk_text_move_word (editable, -1);
	  gtk_editable_delete_text (editable, editable->current_pos, old_pos);
	}
    }
}

static void
gtk_text_delete_forward_word (GtkText *text)
{
  gtk_text_kill_word (GTK_EDITABLE (text), 1);
}

static void
gtk_text_delete_backward_word (GtkText *text)
{
  gtk_text_kill_word (GTK_EDITABLE (text), -1);
}

static void 
gtk_text_kill_line (GtkEditable *editable,
		    gint         direction)
{
  gint old_pos = editable->current_pos;
  if (direction >= 0)
    {
      gtk_text_move_to_column (editable, -1);
      gtk_editable_delete_text (editable, old_pos, editable->current_pos);
    }
  else
    {
      gtk_text_move_to_column (editable, 0);
      gtk_editable_delete_text (editable, editable->current_pos, old_pos);
    }
}

static void
gtk_text_delete_line (GtkText *text)
{
  gtk_text_move_to_column (GTK_EDITABLE (text), 0);
  gtk_text_kill_line (GTK_EDITABLE (text), 1);
}

static void
gtk_text_delete_to_line_end (GtkText *text)
{
  gtk_text_kill_line (GTK_EDITABLE (text), 1);
}

static void
gtk_text_select_word (GtkText *text, guint32 time)
{
  gint start_pos;
  gint end_pos;
  
  GtkEditable *editable;
  editable = GTK_EDITABLE (text);
  
  gtk_text_move_backward_word (text);
  start_pos = text->cursor_mark.index;
  
  gtk_text_move_forward_word (text);
  end_pos = text->cursor_mark.index;
  
  editable->has_selection = TRUE;
  gtk_text_set_selection (editable, start_pos, end_pos);
  gtk_editable_claim_selection (editable, start_pos != end_pos, time);
}

static void
gtk_text_select_line (GtkText *text, guint32 time)
{
  gint start_pos;
  gint end_pos;
  
  GtkEditable *editable;
  editable = GTK_EDITABLE (text);
  
  gtk_text_move_beginning_of_line (text);
  start_pos = text->cursor_mark.index;
  
  gtk_text_move_end_of_line (text);
  gtk_text_move_forward_character (text);
  end_pos = text->cursor_mark.index;
  
  editable->has_selection = TRUE;
  gtk_text_set_selection (editable, start_pos, end_pos);
  gtk_editable_claim_selection (editable, start_pos != end_pos, time);
}

/**********************************************************************/
/*			      Scrolling                               */
/**********************************************************************/

static void
adjust_adj (GtkText* text, GtkAdjustment* adj)
{
  gint height;
  
  gdk_window_get_size (text->text_area, NULL, &height);
  
  adj->step_increment = MIN (adj->upper, (float) SCROLL_PIXELS);
  adj->page_increment = MIN (adj->upper, height - (float) KEY_SCROLL_PIXELS);
  adj->page_size      = MIN (adj->upper, height);
  adj->value          = MIN (adj->value, adj->upper - adj->page_size);
  adj->value          = MAX (adj->value, 0.0);
  
  gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
}

static gint
set_vertical_scroll_iterator (GtkText* text, LineParams* lp, void* data)
{
  SetVerticalScrollData *svdata = (SetVerticalScrollData *) data;
  
  if ((text->first_line_start_index >= lp->start.index) &&
      (text->first_line_start_index <= lp->end.index))
    {
      svdata->mark = lp->start;
  
      if (text->first_line_start_index == lp->start.index)
	{
	  text->first_onscreen_ver_pixel = svdata->pixel_height + text->first_cut_pixels;
	}
      else
	{
	  text->first_onscreen_ver_pixel = svdata->pixel_height;
	  text->first_cut_pixels = 0;
	}
      
      text->vadj->value = (float) text->first_onscreen_ver_pixel;
    }
  
  svdata->pixel_height += LINE_HEIGHT (*lp);
  
  return FALSE;
}

static gint
set_vertical_scroll_find_iterator (GtkText* text, LineParams* lp, void* data)
{
  SetVerticalScrollData *svdata = (SetVerticalScrollData *) data;
  gint return_val;
  
  if (svdata->pixel_height <= (gint) text->vadj->value &&
      svdata->pixel_height + LINE_HEIGHT(*lp) > (gint) text->vadj->value)
    {
      svdata->mark = lp->start;
      
      text->first_cut_pixels = (gint)text->vadj->value - svdata->pixel_height;
      text->first_onscreen_ver_pixel = svdata->pixel_height;
      text->first_line_start_index = lp->start.index;
      
      return_val = TRUE;
    }
  else
    {
      svdata->pixel_height += LINE_HEIGHT (*lp);
      
      return_val = FALSE;
    }
  
  return return_val;
}

static GtkPropertyMark
set_vertical_scroll (GtkText* text)
{
  GtkPropertyMark mark = find_mark (text, 0);
  SetVerticalScrollData data;
  gint height;
  gint orig_value;
  
  data.pixel_height = 0;
  line_params_iterate (text, &mark, NULL, FALSE, &data, set_vertical_scroll_iterator);
  
  text->vadj->upper = (float) data.pixel_height;
  orig_value = (gint) text->vadj->value;
  
  gdk_window_get_size (text->text_area, NULL, &height);
  
  text->vadj->step_increment = MIN (text->vadj->upper, (float) SCROLL_PIXELS);
  text->vadj->page_increment = MIN (text->vadj->upper, height - (float) KEY_SCROLL_PIXELS);
  text->vadj->page_size      = MIN (text->vadj->upper, height);
  text->vadj->value          = MIN (text->vadj->value, text->vadj->upper - text->vadj->page_size);
  text->vadj->value          = MAX (text->vadj->value, 0.0);
  
  text->last_ver_value = (gint)text->vadj->value;
  
  gtk_signal_emit_by_name (GTK_OBJECT (text->vadj), "changed");
  
  if (text->vadj->value != orig_value)
    {
      /* We got clipped, and don't really know which line to put first. */
      data.pixel_height = 0;
      data.last_didnt_wrap = TRUE;
      
      line_params_iterate (text, &mark, NULL,
			   FALSE, &data,
			   set_vertical_scroll_find_iterator);
    }

  return data.mark;
}

static void
scroll_int (GtkText* text, gint diff)
{
  gfloat upper;
  
  text->vadj->value += diff;
  
  upper = text->vadj->upper - text->vadj->page_size;
  text->vadj->value = MIN (text->vadj->value, upper);
  text->vadj->value = MAX (text->vadj->value, 0.0);
  
  gtk_signal_emit_by_name (GTK_OBJECT (text->vadj), "value_changed");
}

static void 
process_exposes (GtkText *text)
{
  GdkEvent *event;
  
  /* Make sure graphics expose events are processed before scrolling
   * again */
  
  while ((event = gdk_event_get_graphics_expose (text->text_area)) != NULL)
    {
      gtk_widget_event (GTK_WIDGET (text), event);
      if (event->expose.count == 0)
	{
	  gdk_event_free (event);
	  break;
	}
      gdk_event_free (event);
    }
}

static gint last_visible_line_height (GtkText* text)
{
  GList *cache = text->line_start_cache;
  gint height;
  
  gdk_window_get_size (text->text_area, NULL, &height);
  
  for (; cache->next; cache = cache->next)
    if (pixel_height_of(text, cache->next) > height)
      break;
  
  if (cache)
    return pixel_height_of(text, cache) - 1;
  else
    return 0;
}

static gint first_visible_line_height (GtkText* text)
{
  if (text->first_cut_pixels)
    return pixel_height_of(text, text->line_start_cache) + 1;
  else
    return 1;
}

static void
scroll_down (GtkText* text, gint diff0)
{
  GdkRectangle rect;
  gint real_diff = 0;
  gint width, height;
  
  text->first_onscreen_ver_pixel += diff0;
  
  while (diff0-- > 0)
    {
      g_assert (text->line_start_cache);
      
      if (text->first_cut_pixels < LINE_HEIGHT(CACHE_DATA(text->line_start_cache)) - 1)
	{
	  text->first_cut_pixels += 1;
	}
      else
	{
	  text->first_cut_pixels = 0;
	  
	  text->line_start_cache = text->line_start_cache->next;
	  
	  text->first_line_start_index =
	    CACHE_DATA(text->line_start_cache).start.index;
	  
	  if (!text->line_start_cache->next)
	    fetch_lines_forward (text, 1);
	}
      
      real_diff += 1;
    }
  
  gdk_window_get_size (text->text_area, &width, &height);
  if (height > real_diff)
    gdk_draw_pixmap (text->text_area,
		     text->gc,
		     text->text_area,
		     0,
		     real_diff,
		     0,
		     0,
		     width,
		     height - real_diff);
  
  rect.x      = 0;
  rect.y      = MAX (0, height - real_diff);
  rect.width  = width;
  rect.height = MIN (height, real_diff);
  
  expose_text (text, &rect, FALSE);
  gtk_text_draw_focus ( (GtkWidget *) text);
  
  if (text->current_line)
    {
      gint cursor_min;
      
      text->cursor_pos_y -= real_diff;
      cursor_min = drawn_cursor_min(text);
      
      if (cursor_min < 0)
	find_mouse_cursor (text, text->cursor_pos_x,
			   first_visible_line_height (text));
    }
  
  if (height > real_diff)
    process_exposes (text);
}

static void
scroll_up (GtkText* text, gint diff0)
{
  gint real_diff = 0;
  GdkRectangle rect;
  gint width, height;
  
  text->first_onscreen_ver_pixel += diff0;
  
  while (diff0++ < 0)
    {
      g_assert (text->line_start_cache);
      
      if (text->first_cut_pixels > 0)
	{
	  text->first_cut_pixels -= 1;
	}
      else
	{
	  if (!text->line_start_cache->prev)
	    fetch_lines_backward (text);
	  
	  text->line_start_cache = text->line_start_cache->prev;
	  
	  text->first_line_start_index =
	    CACHE_DATA(text->line_start_cache).start.index;
	  
	  text->first_cut_pixels = LINE_HEIGHT(CACHE_DATA(text->line_start_cache)) - 1;
	}
      
      real_diff += 1;
    }
  
  gdk_window_get_size (text->text_area, &width, &height);
  if (height > real_diff)
    gdk_draw_pixmap (text->text_area,
		     text->gc,
		     text->text_area,
		     0,
		     0,
		     0,
		     real_diff,
		     width,
		     height - real_diff);
  
  rect.x      = 0;
  rect.y      = 0;
  rect.width  = width;
  rect.height = MIN (height, real_diff);
  
  expose_text (text, &rect, FALSE);
  gtk_text_draw_focus ( (GtkWidget *) text);
  
  if (text->current_line)
    {
      gint cursor_max;
      gint height;
      
      text->cursor_pos_y += real_diff;
      cursor_max = drawn_cursor_max(text);
      gdk_window_get_size (text->text_area, NULL, &height);
      
      if (cursor_max >= height)
	find_mouse_cursor (text, text->cursor_pos_x,
			   last_visible_line_height (text));
    }
  
  if (height > real_diff)
    process_exposes (text);
}

/**********************************************************************/
/*			      Display Code                            */
/**********************************************************************/

/* Assumes mark starts a line.  Calculates the height, width, and
 * displayable character count of a single DISPLAYABLE line.  That
 * means that in line-wrap mode, this does may not compute the
 * properties of an entire line. */
static LineParams
find_line_params (GtkText* text,
		  const GtkPropertyMark* mark,
		  const PrevTabCont *tab_cont,
		  PrevTabCont *next_cont)
{
  LineParams lp;
  TabStopMark tab_mark = tab_cont->tab_start;
  guint max_display_pixels;
  gchar ch;
  gint ch_width;
  GdkFont *font;
  
  gdk_window_get_size (text->text_area, (gint*) &max_display_pixels, NULL);
  max_display_pixels -= LINE_WRAP_ROOM;
  
  lp.wraps             = 0;
  lp.tab_cont          = *tab_cont;
  lp.start             = *mark;
  lp.end               = *mark;
  lp.pixel_width       = tab_cont->pixel_offset;
  lp.displayable_chars = 0;
  lp.font_ascent       = 0;
  lp.font_descent      = 0;
  
  init_tab_cont (text, next_cont);
  
  while (!LAST_INDEX(text, lp.end))
    {
      g_assert (lp.end.property);
      
      ch   = GTK_TEXT_INDEX (text, lp.end.index);
      font = MARK_CURRENT_FONT (text, &lp.end);

      if (ch == LINE_DELIM)
	{
	  /* Newline doesn't count in computation of line height, even
	   * if its in a bigger font than the rest of the line.  Unless,
	   * of course, there are no other characters. */
	  
	  if (!lp.font_ascent && !lp.font_descent)
	    {
	      lp.font_ascent = font->ascent;
	      lp.font_descent = font->descent;
	    }
	  
	  lp.tab_cont_next = *next_cont;
	  
	  return lp;
	}
      
      ch_width = find_char_width (text, &lp.end, &tab_mark);
      
      if ((ch_width + lp.pixel_width > max_display_pixels) &&
	  (lp.end.index > lp.start.index))
	{
	  lp.wraps = 1;
	  
	  if (text->line_wrap)
	    {
	      next_cont->tab_start    = tab_mark;
	      next_cont->pixel_offset = 0;
	      
	      if (ch == '\t')
		{
		  /* Here's the tough case, a tab is wrapping. */
		  gint pixels_avail = max_display_pixels - lp.pixel_width;
		  gint space_width  = MARK_CURRENT_TEXT_FONT(text, &lp.end)->char_widths[' '];
		  gint spaces_avail = pixels_avail / space_width;
		  
		  if (spaces_avail == 0)
		    {
		      decrement_mark (&lp.end);
		    }
		  else
		    {
		      advance_tab_mark (text, &next_cont->tab_start, '\t');
		      next_cont->pixel_offset = space_width * (tab_mark.to_next_tab -
							       spaces_avail);
		      lp.displayable_chars += 1;
		    }
		}
	      else
		{
		  if (text->word_wrap)
		    {
		      GtkPropertyMark saved_mark = lp.end;
		      guint saved_characters = lp.displayable_chars;
		      
		      lp.displayable_chars += 1;
		      
		      while (!isspace (GTK_TEXT_INDEX (text, lp.end.index)) &&
			     (lp.end.index > lp.start.index))
			{
			  decrement_mark (&lp.end);
			  lp.displayable_chars -= 1;
			}
		      
		      /* If whole line is one word, revert to char wrapping */
		      if (lp.end.index == lp.start.index)
			{
			  lp.end = saved_mark;
			  lp.displayable_chars = saved_characters;
			  decrement_mark (&lp.end);
			}
		    }
		  else
		    {
		      /* Don't include this character, it will wrap. */
		      decrement_mark (&lp.end);
		    }
		}
	      
	      lp.tab_cont_next = *next_cont;
	      
	      return lp;
	    }
	}
      else
	{
	  lp.displayable_chars += 1;
	}
      
      lp.font_ascent = MAX (font->ascent, lp.font_ascent);
      lp.font_descent = MAX (font->descent, lp.font_descent);
      lp.pixel_width  += ch_width;
      
      advance_mark(&lp.end);
      advance_tab_mark (text, &tab_mark, ch);
    }
  
  if (LAST_INDEX(text, lp.start))
    {
      /* Special case, empty last line. */
      font = MARK_CURRENT_FONT (text, &lp.end);

      lp.font_ascent = font->ascent;
      lp.font_descent = font->descent;
    }
  
  lp.tab_cont_next = *next_cont;
  
  return lp;
}

static void
expand_scratch_buffer (GtkText* text, guint len)
{
  if (len >= text->scratch_buffer_len)
    {
      guint i = 1;
      
      while (i <= len && i < MIN_GAP_SIZE) i <<= 1;
      
      if (text->scratch_buffer)
	text->scratch_buffer = g_new (guchar, i);
      else
	text->scratch_buffer = g_realloc (text->scratch_buffer, i);
      
      text->scratch_buffer_len = i;
    }
}

/* Returns a GC to draw a background for the text at a mark,
 * or NULL, if the mark's background is NULL
 *
 * Side effect: modifies text->gc
 */
static GdkGC *
mark_bg_gc (GtkText* text, const GtkPropertyMark *mark)
{
  GtkEditable *editable = GTK_EDITABLE(text);
  
  if ((mark->index >= MIN(editable->selection_start_pos, editable->selection_end_pos) &&
       mark->index < MAX(editable->selection_start_pos, editable->selection_end_pos)))
    {
      if (editable->has_selection)
	return GTK_WIDGET(text)->style->bg_gc[GTK_STATE_SELECTED];
      else
	return GTK_WIDGET(text)->style->bg_gc[GTK_STATE_ACTIVE];
    }
  else if (!gdk_color_equal(MARK_CURRENT_BACK (text, mark),
			    &GTK_WIDGET(text)->style->base[GTK_STATE_NORMAL]))
    
    {
      gdk_gc_set_foreground (text->gc, MARK_CURRENT_BACK (text, mark));
      return text->gc;
    }
  return NULL;
}

static void
draw_line (GtkText* text,
	   gint pixel_start_height,
	   LineParams* lp)
{
  GdkGCValues gc_values;
  gint i;
  gint len = 0;
  guint running_offset = lp->tab_cont.pixel_offset;
  guchar* buffer;
  GdkGC *fg_gc, *bg_gc;
  
  GtkEditable *editable = GTK_EDITABLE(text);
  
  guint selection_start_pos = MIN (editable->selection_start_pos, editable->selection_end_pos);
  guint selection_end_pos = MAX (editable->selection_start_pos, editable->selection_end_pos);
  
  GtkPropertyMark mark = lp->start;
  TabStopMark tab_mark = lp->tab_cont.tab_start;
  gint pixel_height = pixel_start_height + lp->font_ascent;
  guint chars = lp->displayable_chars;
  
  /* First provide a contiguous segment of memory.  This makes reading
   * the code below *much* easier, and only incurs the cost of copying
   * when the line being displayed spans the gap. */
  if (mark.index <= text->gap_position &&
      mark.index + chars > text->gap_position)
    {
      expand_scratch_buffer (text, chars);
      
      for (i = 0; i < chars; i += 1)
	text->scratch_buffer[i] = GTK_TEXT_INDEX(text, mark.index + i);
      
      buffer = text->scratch_buffer;
    }
  else
    {
      if (mark.index >= text->gap_position)
	buffer = text->text + mark.index + text->gap_size;
      else
	buffer = text->text + mark.index;
    }
  
  
  if (running_offset > 0)
    {
      bg_gc = mark_bg_gc (text, &mark);
      
      if (bg_gc)
	gdk_draw_rectangle (text->text_area,
			    bg_gc,
			    TRUE,
			    0,
			    pixel_start_height,
			    running_offset,
			    LINE_HEIGHT (*lp));
      else if (GTK_WIDGET (text)->style->bg_pixmap[GTK_STATE_NORMAL])
	{
	  GdkRectangle rect;
	  
	  rect.x = 0;
	  rect.y = pixel_start_height;
	  rect.width = running_offset;
	  rect.height = LINE_HEIGHT (*lp);
	  
	  clear_area (text, &rect);
	}
    }
  
  for (; chars > 0; chars -= len, buffer += len, len = 0)
    {
      if (buffer[0] != '\t')
	{
	  guchar* next_tab = memchr (buffer, '\t', chars);
	  gint pixel_width;
	  GdkFont *font;
	  
	  len = MIN (MARK_CURRENT_PROPERTY (&mark)->length - mark.offset, chars);
	  
	  if (next_tab)
	    len = MIN (len, next_tab - buffer);
	  
	  if (mark.index < selection_start_pos)
	    len = MIN (len, selection_start_pos - mark.index);
	  else if (mark.index < selection_end_pos)
	    len = MIN (len, selection_end_pos - mark.index);

	  font = MARK_CURRENT_FONT (text, &mark);
	  if (font->type == GDK_FONT_FONT)
	    {
	      gdk_gc_set_font (text->gc, font);
	      gdk_gc_get_values (text->gc, &gc_values);
	      pixel_width = gdk_text_width (gc_values.font,
					    (gchar*) buffer, len);
	    }
	  else
	    pixel_width = gdk_text_width (font, (gchar*) buffer, len);
	  
	  bg_gc = mark_bg_gc (text, &mark);
	  if (bg_gc)
	    gdk_draw_rectangle (text->text_area,
				bg_gc,
				TRUE,
				running_offset,
				pixel_start_height,
				pixel_width,
				LINE_HEIGHT(*lp));
	  else if (GTK_WIDGET (text)->style->bg_pixmap[GTK_STATE_NORMAL])
	    {
	      GdkRectangle rect;
	      
	      rect.x = running_offset;
	      rect.y = pixel_start_height;
	      rect.width = pixel_width;
	      rect.height = LINE_HEIGHT (*lp);
	      
	      clear_area (text, &rect);
	    }
	  
	  if ((mark.index >= selection_start_pos) && 
	      (mark.index < selection_end_pos))
	    {
	      if (editable->has_selection)
		fg_gc = GTK_WIDGET(text)->style->fg_gc[GTK_STATE_SELECTED];
	      else
		fg_gc = GTK_WIDGET(text)->style->fg_gc[GTK_STATE_ACTIVE];
	    }
	  else
	    {
	      gdk_gc_set_foreground (text->gc, MARK_CURRENT_FORE (text, &mark));
	      fg_gc = text->gc;
	    }

	  gdk_draw_text (text->text_area, MARK_CURRENT_FONT (text, &mark),
			 fg_gc,
			 running_offset,
			 pixel_height,
			 (gchar*) buffer,
			 len);
	  
	  running_offset += pixel_width;
	  
	  advance_tab_mark_n (text, &tab_mark, len);
	}
      else
	{
	  len = 1;
	  
	  bg_gc = mark_bg_gc (text, &mark);
	  if (bg_gc)
	    {
	      gint pixels_remaining;
	      gint space_width;
	      gint spaces_avail;
	      
	      gdk_window_get_size (text->text_area, &pixels_remaining, NULL);
	      pixels_remaining -= (LINE_WRAP_ROOM + running_offset);

	      space_width = MARK_CURRENT_TEXT_FONT(text, &mark)->char_widths[' '];

	      spaces_avail = pixels_remaining / space_width;
	      spaces_avail = MIN (spaces_avail, tab_mark.to_next_tab);
	      
	      gdk_draw_rectangle (text->text_area,
				  bg_gc,
				  TRUE,
				  running_offset,
				  pixel_start_height,
				  spaces_avail * space_width,
				  LINE_HEIGHT (*lp));
	    }
	  
	  running_offset += tab_mark.to_next_tab *
	    MARK_CURRENT_TEXT_FONT(text, &mark)->char_widths[' '];

	  advance_tab_mark (text, &tab_mark, '\t');
	}
      
      advance_mark_n (&mark, len);
    }
}

static void
draw_line_wrap (GtkText* text, guint height /* baseline height */)
{
  gint width;
  GdkPixmap *bitmap;
  gint bitmap_width;
  gint bitmap_height;
  
  if (text->line_wrap)
    {
      bitmap = text->line_wrap_bitmap;
      bitmap_width = line_wrap_width;
      bitmap_height = line_wrap_height;
    }
  else
    {
      bitmap = text->line_arrow_bitmap;
      bitmap_width = line_arrow_width;
      bitmap_height = line_arrow_height;
    }
  
  gdk_window_get_size (text->text_area, &width, NULL);
  width -= LINE_WRAP_ROOM;
  
  gdk_gc_set_stipple (text->gc,
		      bitmap);
  
  gdk_gc_set_fill (text->gc, GDK_STIPPLED);
  
  gdk_gc_set_foreground (text->gc, &GTK_WIDGET (text)->style->text[GTK_STATE_NORMAL]);
  
  gdk_gc_set_ts_origin (text->gc,
			width + 1,
			height - bitmap_height - 1);
  
  gdk_draw_rectangle (text->text_area,
		      text->gc,
		      TRUE,
		      width + 1,
		      height - bitmap_height - 1 /* one pixel above the baseline. */,
		      bitmap_width,
		      bitmap_height);
  
  gdk_gc_set_ts_origin (text->gc, 0, 0);
  
  gdk_gc_set_fill (text->gc, GDK_SOLID);
}

static void
undraw_cursor (GtkText* text, gint absolute)
{
  GtkEditable *editable = (GtkEditable *)text;
  GdkGC *gc;

  TDEBUG (("in undraw_cursor\n"));
  
  if (absolute)
    text->cursor_drawn_level = 0;
  
  if ((text->cursor_drawn_level ++ == 0) &&
      (editable->selection_start_pos == editable->selection_end_pos) &&
      GTK_WIDGET_DRAWABLE (text) && text->line_start_cache)
    {
      GdkFont* font;
      
      g_assert(text->cursor_mark.property);

      font = MARK_CURRENT_FONT(text, &text->cursor_mark);
      gc = mark_bg_gc (text, &text->cursor_mark);

      if (!gc && (GTK_WIDGET (text)->style->bg_pixmap[GTK_STATE_NORMAL]))
	{
	  GdkRectangle rect;
	  
	  rect.x = text->cursor_pos_x;
	  rect.y = text->cursor_pos_y - text->cursor_char_offset - font->ascent;
	  rect.width = 1;
	  rect.height = font->ascent + 1; /* @@@ I add one here because draw_line is inclusive, right? */
	  
	  clear_area (text, &rect);
	}
      else
	{
	  if (!gc)
	    {
	      gdk_gc_set_foreground (text->gc, MARK_CURRENT_BACK (text, &text->cursor_mark));
	      gc = text->gc;
	    }
	  gdk_draw_line (text->text_area, gc, text->cursor_pos_x,
			 text->cursor_pos_y - text->cursor_char_offset, text->cursor_pos_x,
			 text->cursor_pos_y - text->cursor_char_offset - font->ascent);
	}
      
      if (text->cursor_char)
	{
	  if (font->type == GDK_FONT_FONT)
	    gdk_gc_set_font (text->gc, font);

	  gdk_gc_set_foreground (text->gc, MARK_CURRENT_FORE (text, &text->cursor_mark));

	  gdk_draw_text (text->text_area, font,
			 text->gc,
			 text->cursor_pos_x,
			 text->cursor_pos_y - text->cursor_char_offset,
			 &text->cursor_char,
			 1);
	}
    }
}

static gint
drawn_cursor_min (GtkText* text)
{
  GdkFont* font;
  
  g_assert(text->cursor_mark.property);
  
  font = MARK_CURRENT_FONT(text, &text->cursor_mark);
  
  return text->cursor_pos_y - text->cursor_char_offset - font->ascent;
}

static gint
drawn_cursor_max (GtkText* text)
{
  GdkFont* font;
  
  g_assert(text->cursor_mark.property);
  
  font = MARK_CURRENT_FONT(text, &text->cursor_mark);
  
  return text->cursor_pos_y - text->cursor_char_offset;
}

static void
draw_cursor (GtkText* text, gint absolute)
{
  GtkEditable *editable = (GtkEditable *)text;
  
  TDEBUG (("in draw_cursor\n"));
  
  if (absolute)
    text->cursor_drawn_level = 1;
  
  if ((--text->cursor_drawn_level == 0) &&
      editable->editable &&
      (editable->selection_start_pos == editable->selection_end_pos) &&
      GTK_WIDGET_DRAWABLE (text) && text->line_start_cache)
    {
      GdkFont* font;
      
      g_assert (text->cursor_mark.property);

      font = MARK_CURRENT_FONT (text, &text->cursor_mark);

      gdk_gc_set_foreground (text->gc, &GTK_WIDGET (text)->style->text[GTK_STATE_NORMAL]);
      
      gdk_draw_line (text->text_area, text->gc, text->cursor_pos_x,
		     text->cursor_pos_y - text->cursor_char_offset,
		     text->cursor_pos_x,
		     text->cursor_pos_y - text->cursor_char_offset - font->ascent);
    }
}

static void
clear_area (GtkText *text, GdkRectangle *area)
{
  GtkWidget *widget = GTK_WIDGET (text);
  
  if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
    {
      gint width, height;
      gint x = area->x, y = area->y;
      gint xorig, yorig;
      
      gdk_window_get_size (widget->style->bg_pixmap[GTK_STATE_NORMAL], &width, &height);
      
      yorig = - text->first_onscreen_ver_pixel;
      xorig = - text->first_onscreen_hor_pixel;
      
      for (y = area->y; y < area->y + area->height; )
	{
	  gint yoff = (y - yorig) % height;
	  gint yw = MIN(height - yoff, (area->y + area->height) - y);
	  
	  for (x = area->x; x < area->x + area->width; )
	    {
	      gint xoff = (x - xorig) % width;
	      gint xw = MIN(width - xoff, (area->x + area->width) - x);
	      
	      gdk_draw_pixmap (text->text_area,
			       text->gc,
			       widget->style->bg_pixmap[GTK_STATE_NORMAL],
			       xoff,
			       yoff,
			       x,
			       y,
			       xw,
			       yw);
	      
	      x += width - xoff;
	    }
	  y += height - yoff;
	}
    }
  else
    gdk_window_clear_area (text->text_area, area->x, area->y, area->width, area->height);
}

static void
expose_text (GtkText* text, GdkRectangle *area, gboolean cursor)
{
  GList *cache = text->line_start_cache;
  gint pixels = - text->first_cut_pixels;
  gint min_y = MAX (0, area->y);
  gint max_y = MAX (0, area->y + area->height);
  gint height;
  
  gdk_window_get_size (text->text_area, NULL, &height);
  max_y = MIN (max_y, height);
  
  TDEBUG (("in expose x=%d y=%d w=%d h=%d\n", area->x, area->y, area->width, area->height));
  
  clear_area (text, area);
  
  for (; pixels < height; cache = cache->next)
    {
      if (pixels < max_y && (pixels + (gint)LINE_HEIGHT(CACHE_DATA(cache))) >= min_y)
	{
	  draw_line (text, pixels, &CACHE_DATA(cache));
	  
	  if (CACHE_DATA(cache).wraps)
	    draw_line_wrap (text, pixels + CACHE_DATA(cache).font_ascent);
	}
      
      if (cursor && GTK_WIDGET_HAS_FOCUS (text))
	{
	  if (CACHE_DATA(cache).start.index <= text->cursor_mark.index &&
	      CACHE_DATA(cache).end.index >= text->cursor_mark.index)
	    {
	      /* We undraw and draw the cursor here to get the drawn
	       * level right ... FIXME - maybe the second parameter
	       * of draw_cursor should work differently
	       */
	      undraw_cursor (text, FALSE);
	      draw_cursor (text, FALSE);
	    }
	}
      
      pixels += LINE_HEIGHT(CACHE_DATA(cache));
      
      if (!cache->next)
	{
	  fetch_lines_forward (text, 1);
	  
	  if (!cache->next)
	    break;
	}
    }
}

static void 
gtk_text_update_text    (GtkEditable       *editable,
			 gint               start_pos,
			 gint               end_pos)
{
  GtkText *text = GTK_TEXT (editable);
  
  GList *cache = text->line_start_cache;
  gint pixels = - text->first_cut_pixels;
  GdkRectangle area;
  gint width;
  gint height;
  
  if (end_pos < 0)
    end_pos = TEXT_LENGTH (text);
  
  if (end_pos < start_pos)
    return;
  
  gdk_window_get_size (text->text_area, &width, &height);
  area.x = 0;
  area.y = -1;
  area.width = width;
  area.height = 0;
  
  TDEBUG (("in expose span start=%d stop=%d\n", start_pos, end_pos));
  
  for (; pixels < height; cache = cache->next)
    {
      if (CACHE_DATA(cache).start.index < end_pos)
	{
	  if (CACHE_DATA(cache).end.index >= start_pos)
	    {
	      if (area.y < 0)
		area.y = MAX(0,pixels);
	      area.height = pixels + LINE_HEIGHT(CACHE_DATA(cache)) - area.y;
	    }
	}
      else
	break;
      
      pixels += LINE_HEIGHT(CACHE_DATA(cache));
      
      if (!cache->next)
	{
	  fetch_lines_forward (text, 1);
	  
	  if (!cache->next)
	    break;
	}
    }
  
  if (area.y >= 0)
    expose_text (text, &area, TRUE);
}

static void
recompute_geometry (GtkText* text)
{
  GtkPropertyMark mark, start_mark;
  GList *new_lines;
  gint height;
  gint width;
  
  free_cache (text);
  
  mark = start_mark = set_vertical_scroll (text);

  /* We need a real start of a line when calling fetch_lines().
   * not the start of a wrapped line.
   */
  while (mark.index > 0 &&
	 GTK_TEXT_INDEX (text, mark.index - 1) != LINE_DELIM)
    decrement_mark (&mark);

  gdk_window_get_size (text->text_area, &width, &height);

  /* Fetch an entire line, to make sure that we get all the text
   * we backed over above, in addition to enough text to fill up
   * the space vertically
   */

  new_lines = fetch_lines (text,
			   &mark,
			   NULL,
			   FetchLinesCount,
			   1);

  mark = CACHE_DATA (g_list_last (new_lines)).end;
  if (!LAST_INDEX (text, mark))
    {
      advance_mark (&mark);

      new_lines = g_list_concat (new_lines, 
				 fetch_lines (text,
					      &mark,
					      NULL,
					      FetchLinesPixels,
					      height + text->first_cut_pixels));
    }

  /* Now work forward to the actual first onscreen line */

  while (CACHE_DATA (new_lines).start.index < start_mark.index)
    new_lines = new_lines->next;
  
  text->line_start_cache = new_lines;
  
  find_cursor (text, TRUE);
}

/**********************************************************************/
/*                            Selection                               */
/**********************************************************************/

static void 
gtk_text_set_selection  (GtkEditable   *editable,
			 gint           start,
			 gint           end)
{
  GtkText *text = GTK_TEXT (editable);
  
  guint start1, end1, start2, end2;
  
  if (end < 0)
    end = TEXT_LENGTH (text);
  
  start1 = MIN(start,end);
  end1 = MAX(start,end);
  start2 = MIN(editable->selection_start_pos, editable->selection_end_pos);
  end2 = MAX(editable->selection_start_pos, editable->selection_end_pos);
  
  if (start2 < start1)
    {
      guint tmp;
      
      tmp = start1; start1 = start2; start2 = tmp;
      tmp = end1;   end1   = end2;   end2   = tmp;
    }
  
  undraw_cursor (text, FALSE);
  editable->selection_start_pos = start;
  editable->selection_end_pos = end;
  draw_cursor (text, FALSE);
  
  /* Expose only what changed */
  
  if (start1 < start2)
    gtk_text_update_text (editable, start1, MIN(end1, start2));
  
  if (end2 > end1)
    gtk_text_update_text (editable, MAX(end1, start2), end2);
  else if (end2 < end1)
    gtk_text_update_text (editable, end2, end1);
}


/**********************************************************************/
/*                              Debug                                 */
/**********************************************************************/

#ifdef DEBUG_GTK_TEXT
static void
gtk_text_show_cache_line (GtkText *text, GList *cache,
			  const char* what, const char* func, gint line)
{
  LineParams *lp = &CACHE_DATA(cache);
  gint i;
  
  if (cache == text->line_start_cache)
    g_message ("Line Start Cache: ");
  
  if (cache == text->current_line)
    g_message("Current Line: ");
  
  g_message ("%s:%d: cache line %s s=%d,e=%d,lh=%d (",
	     func,
	     line,
	     what,
	     lp->start.index,
	     lp->end.index,
	     LINE_HEIGHT(*lp));
  
  for (i = lp->start.index; i < (lp->end.index + lp->wraps); i += 1)
    g_message ("%c", GTK_TEXT_INDEX (text, i));
  
  g_message (")\n");
}

static void
gtk_text_show_cache (GtkText *text, const char* func, gint line)
{
  GList *l = text->line_start_cache;
  
  if (!l) {
    return;
  }
  
  /* back up to the absolute beginning of the line cache */
  while (l->prev)
    l = l->prev;
  
  g_message ("*** line cache ***\n");
  for (; l; l = l->next)
    gtk_text_show_cache_line (text, l, "all", func, line);
}

static void
gtk_text_assert_mark (GtkText         *text,
		      GtkPropertyMark *mark,
		      GtkPropertyMark *before,
		      GtkPropertyMark *after,
		      const gchar     *msg,
		      const gchar     *where,
		      gint             line)
{
  GtkPropertyMark correct_mark = find_mark (text, mark->index);
  
  if (mark->offset != correct_mark.offset ||
      mark->property != correct_mark.property)
    g_warning ("incorrect %s text property marker in %s:%d, index %d -- bad!", where, msg, line, mark->index);
}

static void
gtk_text_assert (GtkText         *text,
		 const gchar     *msg,
		 gint             line)
{
  GList* cache = text->line_start_cache;
  GtkPropertyMark* before_mark = NULL;
  GtkPropertyMark* after_mark = NULL;
  
  gtk_text_show_props (text, msg, line);
  
  for (; cache->prev; cache = cache->prev)
    /* nothing */;
  
  g_message ("*** line markers ***\n");
  
  for (; cache; cache = cache->next)
    {
      after_mark = &CACHE_DATA(cache).end;
      gtk_text_assert_mark (text, &CACHE_DATA(cache).start, before_mark, after_mark, msg, "start", line);
      before_mark = &CACHE_DATA(cache).start;
      
      if (cache->next)
	after_mark = &CACHE_DATA(cache->next).start;
      else
	after_mark = NULL;
      
      gtk_text_assert_mark (text, &CACHE_DATA(cache).end, before_mark, after_mark, msg, "end", line);
      before_mark = &CACHE_DATA(cache).end;
    }
}

static void
gtk_text_show_adj (GtkText *text,
		   GtkAdjustment *adj,
		   const char* what,
		   const char* func,
		   gint line)
{
  g_message ("*** adjustment ***\n");
  
  g_message ("%s:%d: %s adjustment l=%.1f u=%.1f v=%.1f si=%.1f pi=%.1f ps=%.1f\n",
	     func,
	     line,
	     what,
	     adj->lower,
	     adj->upper,
	     adj->value,
	     adj->step_increment,
	     adj->page_increment,
	     adj->page_size);
}

static void
gtk_text_show_props (GtkText *text,
		     const char* msg,
		     int line)
{
  GList* props = text->text_properties;
  int proplen = 0;
  
  g_message ("%s:%d: ", msg, line);
  
  for (; props; props = props->next)
    {
      TextProperty *p = (TextProperty*)props->data;
      
      proplen += p->length;

      g_message ("[%d,%p,", p->length, p);
      if (p->flags & PROPERTY_FONT)
	g_message ("%p,", p->font);
      else
	g_message ("-,");
      if (p->flags & PROPERTY_FOREGROUND)
	g_message ("%ld, ", p->fore_color.pixel);
      else
	g_message ("-,");
      if (p->flags & PROPERTY_BACKGROUND)
	g_message ("%ld] ", p->back_color.pixel);
      else
	g_message ("-] ");
    }
  
  g_message ("\n");
  
  if (proplen - 1 != TEXT_LENGTH(text))
    g_warning ("incorrect property list length in %s:%d -- bad!", msg, line);
}
#endif
