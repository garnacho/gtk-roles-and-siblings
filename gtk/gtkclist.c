/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald, 
 * Copyright (C) 1997-1998 Jay Painter <jpaint@serv.net><jpaint@gimp.org>  
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
#include <stdlib.h>
#include <string.h>
#include "../config.h"
#include "gtkmain.h"
#include "gtkclist.h"
#include "gtkbindings.h"
#include <gdk/gdkkeysyms.h>

/* the number rows memchunk expands at a time */
#define CLIST_OPTIMUM_SIZE 512

/* the width of the column resize windows */
#define DRAG_WIDTH  6

/* minimum allowed width of a column */
#define COLUMN_MIN_WIDTH 5

/* this defigns the base grid spacing */
#define CELL_SPACING 1

/* added the horizontal space at the beginning and end of a row*/
#define COLUMN_INSET 3

/* used for auto-scrolling */
#define SCROLL_TIME  100

/* scrollbar spacing class macro */
#define SCROLLBAR_SPACING(w) (GTK_CLIST_CLASS (GTK_OBJECT (w)->klass)->scrollbar_spacing)

/* gives the top pixel of the given row in context of
 * the clist's voffset */
#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
				    (((row) + 1) * CELL_SPACING) + \
				    (clist)->voffset)

/* returns the row index from a y pixel location in the 
 * context of the clist's voffset */
#define ROW_FROM_YPIXEL(clist, y)  (((y) - (clist)->voffset) / \
				    ((clist)->row_height + CELL_SPACING))

/* gives the left pixel of the given column in context of
 * the clist's hoffset */
#define COLUMN_LEFT_XPIXEL(clist, colnum)  ((clist)->column[(colnum)].area.x + \
					    (clist)->hoffset)

/* returns the column index from a x pixel location in the 
 * context of the clist's hoffset */
static inline gint
COLUMN_FROM_XPIXEL (GtkCList * clist,
		    gint x)
{
  gint i, cx;

  for (i = 0; i < clist->columns; i++)
    {
      cx = clist->column[i].area.x + clist->hoffset;

      if (x >= (cx - (COLUMN_INSET + CELL_SPACING)) &&
	  x <= (cx + clist->column[i].area.width + COLUMN_INSET))
	return i;
    }

  /* no match */
  return -1;
}

/* returns the top pixel of the given row in the context of
 * the list height */
#define ROW_TOP(clist, row)        (((clist)->row_height + CELL_SPACING) * (row))

/* returns the left pixel of the given column in the context of
 * the list width */
#define COLUMN_LEFT(clist, colnum) ((clist)->column[(colnum)].area.x)

/* returns the total height of the list */
#define LIST_HEIGHT(clist)         (((clist)->row_height * ((clist)->rows)) + \
				    (CELL_SPACING * ((clist)->rows + 1)))

/* returns the total width of the list */
#define LIST_WIDTH(clist)          ((clist)->column[(clist)->columns - 1].area.x + \
				    (clist)->column[(clist)->columns - 1].area.width + \
				    COLUMN_INSET + CELL_SPACING)


#define GTK_CLIST_CLASS_FW(_widget_) GTK_CLIST_CLASS (GTK_OBJECT (_widget_)->klass)


/* Signals */
enum
{
  SELECT_ROW,
  UNSELECT_ROW,
  CLICK_COLUMN,
  TOGGLE_FOCUS_ROW,
  SELECT_ALL,
  UNSELECT_ALL,
  UNDO_SELECTION,
  START_SELECTION,
  END_SELECTION,
  TOGGLE_ADD_MODE,
  EXTEND_SELECTION,
  SCROLL_VERTICAL,
  SCROLL_HORIZONTAL,
  ABORT_COLUMN_RESIZE,
  LAST_SIGNAL
};

enum
{
  SYNC_REMOVE,
  SYNC_INSERT
};


typedef void (*GtkCListSignal1) (GtkObject * object,
				 gint arg1,
				 gint arg2,
				 GdkEvent * arg3,
				 gpointer data);

typedef void (*GtkCListSignal2) (GtkObject *object,
				 gpointer data); 

typedef void (*GtkCListSignal3) (GtkObject * object,
				 gint arg1,
				 gpointer data);
typedef void (*GtkCListSignal4) (GtkObject * object,
				 gint arg1,
				 gfloat arg2,
				 gpointer data);
typedef void (*GtkCListSignal5) (GtkObject * object,
				 gint arg1,
				 gfloat arg2,
				 gboolean arg3,
				 gpointer data);


static void sync_selection (GtkCList * clist,
	                    gint row,
                            gint mode);

/* GtkCList Methods */
static void gtk_clist_class_init (GtkCListClass * klass);
static void gtk_clist_init (GtkCList * clist);
static void real_clear (GtkCList * clist);

/* GtkObject Methods */
static void gtk_clist_destroy (GtkObject * object);
static void gtk_clist_finalize (GtkObject * object);


/* GtkWidget Methods */
static void gtk_clist_realize (GtkWidget * widget);
static void gtk_clist_unrealize (GtkWidget * widget);
static void gtk_clist_map (GtkWidget * widget);
static void gtk_clist_unmap (GtkWidget * widget);
static void gtk_clist_draw (GtkWidget * widget,
			    GdkRectangle * area);
static gint gtk_clist_expose (GtkWidget * widget,
			      GdkEventExpose * event);
static gint gtk_clist_button_press (GtkWidget * widget,
				    GdkEventButton * event);
static gint gtk_clist_button_release (GtkWidget * widget,
				      GdkEventButton * event);
static gint gtk_clist_motion (GtkWidget * widget, 
			      GdkEventMotion * event);
static void gtk_clist_size_request (GtkWidget * widget,
				    GtkRequisition * requisition);
static void gtk_clist_size_allocate (GtkWidget * widget,
				     GtkAllocation * allocation);
static gint get_selection_info (GtkCList * clist,
				gint x,
				gint y,
				gint * row,
				gint * column);

/* GtkContainer Methods */
static void gtk_clist_foreach (GtkContainer * container,
			       GtkCallback callback,
			       gpointer callback_data);

/* Drawing */
static void draw_row (GtkCList * clist,
		      GdkRectangle * area,
		      gint row,
		      GtkCListRow * clist_row);
static void draw_rows (GtkCList * clist,
		       GdkRectangle * area);

/* Size Allocation */
static void size_allocate_title_buttons (GtkCList * clist);
static void size_allocate_columns (GtkCList * clist);

/* Selection */
static void toggle_row (GtkCList * clist,
			gint row,
			gint column,
			GdkEvent * event);
static void select_row (GtkCList * clist,
			gint row,
			gint column,
			GdkEvent * event);
static void unselect_row (GtkCList * clist,
			  gint row,
			  gint column,
			  GdkEvent * event);
static void real_select_row (GtkCList * clist,
			     gint row,
			     gint column,
			     GdkEvent * event);
static void real_unselect_row (GtkCList * clist,
			       gint row,
			       gint column,
			       GdkEvent * event);
static void update_extended_selection (GtkCList *clist,
				       gint row);
static GList * selection_find (GtkCList *clist,
			       gint row_number,
			       GList *row_list_element);
static void real_select_all (GtkCList * clist);
static void real_unselect_all (GtkCList * clist);
static void move_vertical (GtkCList *clist,
			   gint row,
			   gfloat align);
static void move_horizontal (GtkCList *clist,
			     gint diff);
static void real_undo_selection (GtkCList * clist);
static void fake_unselect_all (GtkCList *clist,
			       gint row);
static void fake_toggle_row (GtkCList *clist,
			     gint row);
static void resync_selection (GtkCList *clist,
			      GdkEvent *event);

/* Resize Columns */
static void draw_xor_line (GtkCList * clist);
static gint new_column_width (GtkCList * clist,
			      gint column,
			      gint * x,
			      gint * visible);
static void resize_column (GtkCList * clist,
			   gint column,
			   gint width);
static void abort_column_resize (GtkCList *clist);

/* Buttons */
static void column_button_create (GtkCList * clist,
				  gint column);
static void column_button_clicked (GtkWidget * widget,
				   gpointer data);

/* Scrollbars */
static void create_scrollbars (GtkCList * clist);
static void adjust_scrollbars (GtkCList * clist);
static void check_exposures   (GtkCList * clist);
static void vadjustment_changed (GtkAdjustment * adjustment,
				 gpointer data);
static void vadjustment_value_changed (GtkAdjustment * adjustment,
				       gpointer data);
static void hadjustment_changed (GtkAdjustment * adjustment,
				 gpointer data);
static void hadjustment_value_changed (GtkAdjustment * adjustment,
				       gpointer data);

/* Memory Allocation/Distruction Routines */
static GtkCListColumn *columns_new (GtkCList * clist);

static void column_title_new (GtkCList * clist,
			      gint column,
			      gchar * title);
static void columns_delete (GtkCList * clist);

static GtkCListRow *row_new (GtkCList * clist);

static void row_delete (GtkCList * clist,
			GtkCListRow * clist_row);
static void set_cell_contents (GtkCList * clist,
			       GtkCListRow * clist_row,
			       gint column,
			       GtkCellType type,
			       gchar * text,
			       guint8 spacing,
			       GdkPixmap * pixmap,
			       GdkBitmap * mask);
static gint real_insert_row (GtkCList * clist,
			     gint row,
			     gchar * text[]);
static void real_remove_row (GtkCList * clist,
			     gint row);

/* Focus handling */
static void gtk_clist_draw_focus (GtkWidget *widget);
static gint gtk_clist_focus_in   (GtkWidget     *widget,
				  GdkEventFocus *event);
static gint gtk_clist_focus_out  (GtkWidget     *widget,
				  GdkEventFocus *event);
static gint gtk_clist_focus      (GtkContainer     *container,
				  GtkDirectionType  direction);
static void gtk_clist_set_focus_child (GtkContainer *container,
				       GtkWidget    *child);
static gint gtk_clist_key_press  (GtkWidget   *widget,
				  GdkEventKey *event);

/* Selection handling */
static void set_anchor (GtkCList *clist,
			gboolean  add_mode,
			gint      anchor,
			gint      undo_anchor);
static void start_selection (GtkCList *clist);
static void end_selection (GtkCList *clist);

static void toggle_add_mode (GtkCList *clist);
static void toggle_focus_row (GtkCList *clist);
static void move_focus_row (GtkCList      *clist,
			    GtkScrollType  scroll_type,
			    gfloat         position);
static void scroll_horizontal (GtkCList      *clist,
			       GtkScrollType  scroll_type,
			       gfloat         position);
static void scroll_vertical (GtkCList      *clist,
			     GtkScrollType  scroll_type,
			     gfloat         position);
static void extend_selection (GtkCList      *clist,
			      GtkScrollType  scroll_type,
			      gfloat         position,
			      gboolean       auto_start_selection);

/* Sorting */
static gint default_compare (GtkCList     *clist,
			     gconstpointer row1,
			     gconstpointer row2);
static GList * gtk_clist_merge (GtkCList *clist,
				GList    *a,
				GList    *b);
static GList * gtk_clist_mergesort (GtkCList *clist,
				    GList    *list,
				    gint      num);
static void real_sort_list (GtkCList *clist);

/* Fill in data after widget is realized and has style */

static void add_style_data (GtkCList * clist);

static GtkContainerClass *parent_class = NULL;
static guint clist_signals[LAST_SIGNAL] = {0};


GtkType
gtk_clist_get_type (void)
{
  static GtkType clist_type = 0;

  if (!clist_type)
    {
      GtkTypeInfo clist_info =
      {
	"GtkCList",
	sizeof (GtkCList),
	sizeof (GtkCListClass),
	(GtkClassInitFunc) gtk_clist_class_init,
	(GtkObjectInitFunc) gtk_clist_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      clist_type = gtk_type_unique (GTK_TYPE_CONTAINER, &clist_info);
    }

  return clist_type;
}

static void
gtk_clist_class_init (GtkCListClass * klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  clist_signals[SELECT_ROW] =
    gtk_signal_new ("select_row",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, select_row),
		    gtk_marshal_NONE__INT_INT_POINTER,
		    GTK_TYPE_NONE, 3,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_GDK_EVENT);
  clist_signals[UNSELECT_ROW] =
    gtk_signal_new ("unselect_row",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, unselect_row),
		    gtk_marshal_NONE__INT_INT_POINTER,
	    GTK_TYPE_NONE, 3, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_GDK_EVENT);
  clist_signals[CLICK_COLUMN] =
    gtk_signal_new ("click_column",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, click_column),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, GTK_TYPE_INT);

  clist_signals[TOGGLE_FOCUS_ROW] =
    gtk_signal_new ("toggle_focus_row",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkCListClass, toggle_focus_row),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);
  clist_signals[SELECT_ALL] =
    gtk_signal_new ("select_all",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkCListClass, select_all),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);
  clist_signals[UNSELECT_ALL] =
    gtk_signal_new ("unselect_all",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkCListClass, unselect_all),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);
  clist_signals[UNDO_SELECTION] =
    gtk_signal_new ("undo_selection",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, undo_selection),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  clist_signals[START_SELECTION] =
    gtk_signal_new ("start_selection",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, start_selection),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  clist_signals[END_SELECTION] =
    gtk_signal_new ("end_selection",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, end_selection),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  clist_signals[TOGGLE_ADD_MODE] =
    gtk_signal_new ("toggle_add_mode",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkCListClass, toggle_add_mode),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  clist_signals[EXTEND_SELECTION] =
    gtk_signal_new ("extend_selection",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkCListClass, extend_selection),
                    gtk_marshal_NONE__ENUM_FLOAT_BOOL,
                    GTK_TYPE_NONE, 3,
		    GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT, GTK_TYPE_BOOL);
  clist_signals[SCROLL_VERTICAL] =
    gtk_signal_new ("scroll_vertical",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkCListClass, scroll_vertical),
                    gtk_marshal_NONE__ENUM_FLOAT,
                    GTK_TYPE_NONE, 2, GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT);
  clist_signals[SCROLL_HORIZONTAL] =
    gtk_signal_new ("scroll_horizontal",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkCListClass, scroll_horizontal),
                    gtk_marshal_NONE__ENUM_FLOAT,
                    GTK_TYPE_NONE, 2, GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT);
  clist_signals[ABORT_COLUMN_RESIZE] =
    gtk_signal_new ("abort_column_resize",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkCListClass, abort_column_resize),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);


  gtk_object_class_add_signals (object_class, clist_signals, LAST_SIGNAL);

  object_class->destroy = gtk_clist_destroy;
  object_class->finalize = gtk_clist_finalize;

  widget_class->realize = gtk_clist_realize;
  widget_class->unrealize = gtk_clist_unrealize;
  widget_class->map = gtk_clist_map;
  widget_class->unmap = gtk_clist_unmap;
  widget_class->draw = gtk_clist_draw;
  widget_class->button_press_event = gtk_clist_button_press;
  widget_class->button_release_event = gtk_clist_button_release;
  widget_class->motion_notify_event = gtk_clist_motion;
  widget_class->expose_event = gtk_clist_expose;
  widget_class->size_request = gtk_clist_size_request;
  widget_class->size_allocate = gtk_clist_size_allocate;
  widget_class->key_press_event = gtk_clist_key_press;
  widget_class->focus_in_event = gtk_clist_focus_in;
  widget_class->focus_out_event = gtk_clist_focus_out;
  widget_class->draw_focus = gtk_clist_draw_focus;

  /* container_class->add = NULL; use the default GtkContainerClass warning */
  /* container_class->remove = NULL; use the default GtkContainerClass warning */
  container_class->foreach = gtk_clist_foreach;
  container_class->focus = gtk_clist_focus;
  container_class->set_focus_child = gtk_clist_set_focus_child;

  klass->select_row = real_select_row;
  klass->unselect_row = real_unselect_row;
  klass->undo_selection = real_undo_selection;
  klass->resync_selection = resync_selection;
  klass->selection_find = selection_find;
  klass->click_column = NULL;
  klass->draw_row = draw_row;
  klass->insert_row = real_insert_row;
  klass->remove_row = real_remove_row;
  klass->clear = real_clear;
  klass->sort_list = real_sort_list;
  klass->select_all = real_select_all;
  klass->unselect_all = real_unselect_all;
  klass->fake_unselect_all = fake_unselect_all;
  klass->scroll_horizontal = scroll_horizontal;
  klass->scroll_vertical = scroll_vertical;
  klass->extend_selection = extend_selection;
  klass->toggle_focus_row = toggle_focus_row;
  klass->toggle_add_mode = toggle_add_mode;
  klass->start_selection = start_selection;
  klass->end_selection = end_selection;
  klass->abort_column_resize = abort_column_resize;
  klass->set_cell_contents = set_cell_contents;

  klass->scrollbar_spacing = 5;

  {
    GtkBindingSet *binding_set;

    binding_set = gtk_binding_set_by_class (klass);
    gtk_binding_entry_add_signal (binding_set, GDK_Up, 0,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Down, 0,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, 0,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, 0,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Home, GDK_CONTROL_MASK,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_End, GDK_CONTROL_MASK,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 1.0);

    gtk_binding_entry_add_signal (binding_set, GDK_Up, GDK_SHIFT_MASK,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_Down, GDK_SHIFT_MASK,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, GDK_SHIFT_MASK,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, GDK_SHIFT_MASK,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_Home,
				  GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_End,
				  GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 1.0, GTK_TYPE_BOOL, TRUE);


    gtk_binding_entry_add_signal (binding_set, GDK_Left, 0,
				  "scroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Right, 0,
				  "scroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Home, 0,
				  "scroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_End, 0,
				  "scroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 1.0);


    gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0,
				  "undo_selection", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0,
				  "abort_column_resize", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_space, 0,
				  "toggle_focus_row", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_CONTROL_MASK,
				  "toggle_add_mode", 0);
    gtk_binding_entry_add_signal (binding_set, '/', GDK_CONTROL_MASK,
				  "select_all", 0);
    gtk_binding_entry_add_signal (binding_set, '\\', GDK_CONTROL_MASK,
				  "unselect_all", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_Shift_L,
				  GDK_RELEASE_MASK | GDK_SHIFT_MASK,
				  "end_selection", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_Shift_R,
				  GDK_RELEASE_MASK | GDK_SHIFT_MASK,
				  "end_selection", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_Shift_L,
				  GDK_RELEASE_MASK | GDK_SHIFT_MASK |
				  GDK_CONTROL_MASK,
				  "end_selection", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_Shift_R,
				  GDK_RELEASE_MASK | GDK_SHIFT_MASK |
				  GDK_CONTROL_MASK,
				  "end_selection", 0);
  }

  /*
  {
    GtkBindingSet *binding_set;

    binding_set = gtk_binding_set_by_class (klass);
    gtk_binding_entry_add_signal (binding_set, GDK_Up, 0,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Down, 0,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, 0,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, 0,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Home, GDK_CONTROL_MASK,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_End, GDK_CONTROL_MASK,
				  "scroll_vertical", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 0);

    gtk_binding_entry_add_signal (binding_set, GDK_KP_Up, 0,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Down, 0,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Page_Up, 0,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Page_Down, 0,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Home, 0,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_End, 0,
				  "extend_selection", 3,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 1.0, GTK_TYPE_BOOL, TRUE);

    gtk_binding_entry_add_signal (binding_set, GDK_Left, 0,
				  "scroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Left, 0,
				  "scroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Right, 0,
				  "scroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Right, 0,
				  "scroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_Home, 0,
				  "scroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 0.0);
    gtk_binding_entry_add_signal (binding_set, GDK_End, 0,
				  "sroll_horizontal", 2,
				  GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
                                  GTK_TYPE_FLOAT, 1.0);

    gtk_binding_entry_add_signal (binding_set, GDK_KP_Divide, 0,
				  "undo_selection", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0,
				  "abort_column_resize", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_space, 0,
				  "toggle_focus_row", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Multiply, 0,
				  "toggle_add_mode", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Add, 0,
				  "select_all", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Subtract, 0,
				  "unselect_all", 0);
    gtk_binding_entry_add_signal (binding_set, GDK_KP_Enter, 0,
				  "end_selection", 0);
  }
  */
}

static void
gtk_clist_init (GtkCList * clist)
{
  clist->flags = 0;

  GTK_WIDGET_UNSET_FLAGS (clist, GTK_NO_WINDOW);
  GTK_CLIST_SET_FLAG (clist, CLIST_FROZEN);

  clist->row_mem_chunk = NULL;
  clist->cell_mem_chunk = NULL;

  clist->rows = 0;
  clist->row_center_offset = 0;
  clist->row_height = 0;
  clist->row_list = NULL;
  clist->row_list_end = NULL;

  clist->columns = 0;

  clist->title_window = NULL;
  clist->column_title_area.x = 0;
  clist->column_title_area.y = 0;
  clist->column_title_area.width = 1;
  clist->column_title_area.height = 1;

  clist->clist_window = NULL;
  clist->clist_window_width = 1;
  clist->clist_window_height = 1;

  clist->hoffset = 0;
  clist->voffset = 0;

  clist->shadow_type = GTK_SHADOW_IN;
  clist->hscrollbar_policy = GTK_POLICY_ALWAYS;
  clist->vscrollbar_policy = GTK_POLICY_ALWAYS;

  clist->cursor_drag = NULL;
  clist->xor_gc = NULL;
  clist->fg_gc = NULL;
  clist->bg_gc = NULL;
  clist->x_drag = 0;

  clist->selection_mode = GTK_SELECTION_SINGLE;
  clist->selection = NULL;
  clist->selection_end = NULL;
  clist->undo_selection = NULL;
  clist->undo_unselection = NULL;

  GTK_WIDGET_SET_FLAGS (clist, GTK_CAN_FOCUS);
  GTK_CLIST_SET_FLAG (clist, CLIST_CHILD_HAS_FOCUS);
  clist->focus_row = -1;
  clist->undo_anchor = -1;

  clist->anchor = -1;
  clist->anchor_state = GTK_STATE_SELECTED;
  clist->drag_pos = -1;
  clist->htimer = 0;
  clist->vtimer = 0;

  clist->compare = default_compare;
  clist->sort_type = GTK_SORT_ASCENDING;
  clist->sort_column = 0;
}

/* Constructors */
void
gtk_clist_construct (GtkCList * clist,
		     gint columns,
		     gchar *titles[])
{
  int i;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));
  g_return_if_fail (GTK_CLIST_CONSTRUCTED (clist) == FALSE);

  GTK_CLIST_SET_FLAG (clist, CLIST_CONSTRUCTED);

  /* initalize memory chunks, if this has not been done by any
   * possibly derived widget
   */
  if (!clist->row_mem_chunk)
    clist->row_mem_chunk = g_mem_chunk_new ("clist row mem chunk",
					    sizeof (GtkCListRow),
					    sizeof (GtkCListRow) * CLIST_OPTIMUM_SIZE, 
					    G_ALLOC_AND_FREE);

  if (!clist->cell_mem_chunk)
    clist->cell_mem_chunk = g_mem_chunk_new ("clist cell mem chunk",
					     sizeof (GtkCell) * columns,
					     sizeof (GtkCell) * columns * CLIST_OPTIMUM_SIZE, 
					     G_ALLOC_AND_FREE);

  /* set number of columns, allocate memory */
  clist->columns = columns;
  clist->column = columns_new (clist);

  /* there needs to be at least one column button 
   * because there is alot of code that will break if it
   * isn't there*/
  column_button_create (clist, 0);

  /* create scrollbars */
  create_scrollbars (clist);

  if (titles)
    {
      GTK_CLIST_SET_FLAG (clist, CLIST_SHOW_TITLES);
      for (i = 0; i < columns; i++)
	gtk_clist_set_column_title (clist, i, titles[i]);
    }
  else
    {
      GTK_CLIST_UNSET_FLAG (clist, CLIST_SHOW_TITLES);
    }
}

/*
 * GTKCLIST PUBLIC INTERFACE
 *   gtk_clist_new_with_titles
 *   gtk_clist_new
 */
GtkWidget *
gtk_clist_new_with_titles (gint columns,
			   gchar * titles[])
{
  GtkWidget *widget;

  g_return_val_if_fail (titles != NULL, NULL);
  
  widget = gtk_type_new (GTK_TYPE_CLIST);
  
  gtk_clist_construct (GTK_CLIST (widget), columns, titles);

  return widget;
}

GtkWidget *
gtk_clist_new (gint columns)
{
  GtkCList *clist;

  if (columns < 1)
    return NULL;

  clist = gtk_type_new (GTK_TYPE_CLIST);
  gtk_clist_construct (clist, columns, NULL);
  return GTK_WIDGET (clist);
}

void
gtk_clist_set_border (GtkCList * clist,
		      GtkShadowType border)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  clist->shadow_type = border;

  if (GTK_WIDGET_VISIBLE (clist))
    gtk_widget_queue_resize (GTK_WIDGET (clist));
}

void
gtk_clist_set_selection_mode (GtkCList * clist,
			      GtkSelectionMode mode)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (mode == clist->selection_mode)
    return;

  clist->selection_mode = mode;
  clist->anchor = -1;
  clist->anchor_state = GTK_STATE_SELECTED;
  clist->drag_pos = -1;
  clist->undo_anchor = clist->focus_row;

  g_list_free (clist->undo_selection);
  g_list_free (clist->undo_unselection);
  clist->undo_selection = NULL;
  clist->undo_unselection = NULL;

  switch (mode)
    {
    case GTK_SELECTION_MULTIPLE:
    case GTK_SELECTION_EXTENDED:
      return;
    case GTK_SELECTION_BROWSE:
    case GTK_SELECTION_SINGLE:
      gtk_clist_unselect_all (clist);
      break;
    }
}

void
gtk_clist_freeze (GtkCList * clist)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  GTK_CLIST_SET_FLAG (clist, CLIST_FROZEN);
}

void
gtk_clist_thaw (GtkCList * clist)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  GTK_CLIST_UNSET_FLAG (clist, CLIST_FROZEN);

  adjust_scrollbars (clist);
  draw_rows (clist, NULL);
}

void
gtk_clist_column_titles_show (GtkCList * clist)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (!GTK_CLIST_SHOW_TITLES (clist))
    {
      GTK_CLIST_SET_FLAG (clist, CLIST_SHOW_TITLES);
      if (clist->title_window)
	      gdk_window_show (clist->title_window);
      gtk_widget_queue_resize (GTK_WIDGET (clist));
    }
}

void 
gtk_clist_column_titles_hide (GtkCList * clist)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (GTK_CLIST_SHOW_TITLES (clist))
    {
      GTK_CLIST_UNSET_FLAG (clist, CLIST_SHOW_TITLES);
      if (clist->title_window)
	      gdk_window_hide (clist->title_window);
      gtk_widget_queue_resize (GTK_WIDGET (clist));
    }
}

void
gtk_clist_column_title_active (GtkCList * clist,
			       gint column)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (column < 0 || column >= clist->columns)
    return;

  if (!GTK_WIDGET_SENSITIVE (clist->column[column].button) ||
      !GTK_WIDGET_CAN_FOCUS (clist->column[column].button))
    {
      GTK_WIDGET_SET_FLAGS (clist->column[column].button, GTK_SENSITIVE | GTK_CAN_FOCUS);
      if (GTK_WIDGET_VISIBLE (clist))
	gtk_widget_queue_draw (clist->column[column].button);
    }
}

void
gtk_clist_column_title_passive (GtkCList * clist,
				gint column)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (column < 0 || column >= clist->columns)
    return;

  if (GTK_WIDGET_SENSITIVE (clist->column[column].button) ||
      GTK_WIDGET_CAN_FOCUS (clist->column[column].button))
    {
      GTK_WIDGET_UNSET_FLAGS (clist->column[column].button, GTK_SENSITIVE | GTK_CAN_FOCUS);
      if (GTK_WIDGET_VISIBLE (clist))
	gtk_widget_queue_draw (clist->column[column].button);
    }
}

void
gtk_clist_column_titles_active (GtkCList * clist)
{
  gint i;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      gtk_clist_column_title_active (clist, i);
}

void
gtk_clist_column_titles_passive (GtkCList * clist)
{
  gint i;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      gtk_clist_column_title_passive (clist, i);
}

void
gtk_clist_set_column_title (GtkCList * clist,
			    gint column,
			    gchar * title)
{
  gint new_button = 0;
  GtkWidget *old_widget;
  GtkWidget *alignment = NULL;
  GtkWidget *label;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (column < 0 || column >= clist->columns)
    return;

  /* if the column button doesn't currently exist,
   * it has to be created first */
  if (!clist->column[column].button)
    {
      column_button_create (clist, column);
      new_button = 1;
    }

  column_title_new (clist, column, title);

  /* remove and destroy the old widget */
  old_widget = GTK_BIN (clist->column[column].button)->child;
  if (old_widget)
    gtk_container_remove (GTK_CONTAINER (clist->column[column].button), old_widget);

  /* create new alignment based no column justification */
  switch (clist->column[column].justification)
    {
    case GTK_JUSTIFY_LEFT:
      alignment = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
      break;

    case GTK_JUSTIFY_RIGHT:
      alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
      break;

    case GTK_JUSTIFY_CENTER:
      alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      break;

    case GTK_JUSTIFY_FILL:
      alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      break;
    }

  label = gtk_label_new (clist->column[column].title);
  gtk_container_add (GTK_CONTAINER (alignment), label);
  gtk_container_add (GTK_CONTAINER (clist->column[column].button), alignment);
  gtk_widget_show (label);
  gtk_widget_show (alignment);

  /* if this button didn't previously exist, then the
   * column button positions have to be re-computed */
  if (GTK_WIDGET_VISIBLE (clist) && new_button)
    size_allocate_title_buttons (clist);
}

void
gtk_clist_set_column_widget (GtkCList * clist,
			     gint column,
			     GtkWidget * widget)
{
  gint new_button = 0;
  GtkWidget *old_widget;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (column < 0 || column >= clist->columns)
    return;

  /* if the column button doesn't currently exist,
   * it has to be created first */
  if (!clist->column[column].button)
    {
      column_button_create (clist, column);
      new_button = 1;
    }

  column_title_new (clist, column, NULL);

  /* remove and destroy the old widget */
  old_widget = GTK_BIN (clist->column[column].button)->child;
  if (old_widget)
    gtk_container_remove (GTK_CONTAINER (clist->column[column].button), old_widget);

  /* add and show the widget */
  if (widget)
    {
      gtk_container_add (GTK_CONTAINER (clist->column[column].button), widget);
      gtk_widget_show (widget);
    }

  /* if this button didn't previously exist, then the
   * column button positions have to be re-computed */
  if (GTK_WIDGET_VISIBLE (clist) && new_button)
    size_allocate_title_buttons (clist);
}

void
gtk_clist_set_column_justification (GtkCList * clist,
				    gint column,
				    GtkJustification justification)
{
  GtkWidget *alignment;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (column < 0 || column >= clist->columns)
    return;

  clist->column[column].justification = justification;

  /* change the alinment of the button title if it's not a
   * custom widget */
  if (clist->column[column].title)
    {
      alignment = GTK_BIN (clist->column[column].button)->child;

      switch (clist->column[column].justification)
	{
	case GTK_JUSTIFY_LEFT:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.0, 0.5, 0.0, 0.0);
	  break;

	case GTK_JUSTIFY_RIGHT:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 1.0, 0.5, 0.0, 0.0);
	  break;

	case GTK_JUSTIFY_CENTER:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.5, 0.5, 0.0, 0.0);
	  break;

	case GTK_JUSTIFY_FILL:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.5, 0.5, 0.0, 0.0);
	  break;

	default:
	  break;
	}
    }

  if (!GTK_CLIST_FROZEN (clist))
    draw_rows (clist, NULL);
}

void
gtk_clist_set_column_width (GtkCList * clist,
			    gint column,
			    gint width)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (column < 0 || column >= clist->columns)
    return;

  clist->column[column].width = width;
  clist->column[column].width_set = TRUE;

  /* FIXME: this is quite expensive to do if the widget hasn't
   *        been size_allocated yet, and pointless. Should
   *        a flag be kept
   */
  size_allocate_columns (clist);
  size_allocate_title_buttons (clist);

  if (!GTK_CLIST_FROZEN (clist))
    {
      adjust_scrollbars (clist);
      draw_rows (clist, NULL);
    }
}

void
gtk_clist_set_row_height (GtkCList * clist,
			  gint height)
{
  gint text_height;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (height > 0)
    clist->row_height = height;
  else
    return;

  GTK_CLIST_SET_FLAG (clist, CLIST_ROW_HEIGHT_SET);
  
  if (GTK_WIDGET_REALIZED (clist))
    {
      text_height = height - (GTK_WIDGET (clist)->style->font->ascent +
			      GTK_WIDGET (clist) ->style->font->descent + 1);
      clist->row_center_offset = (text_height / 2) + GTK_WIDGET (clist)->style->font->ascent + 1.5;
    }
      
  if (!GTK_CLIST_FROZEN (clist))
    {
      adjust_scrollbars (clist);
      draw_rows (clist, NULL);
    }
}

void
gtk_clist_moveto (GtkCList * clist,
		  gint row,
		  gint column,
		  gfloat row_align,
		  gfloat col_align)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < -1 || row >= clist->rows)
    return;
  if (column < -1 || column >= clist->columns)
    return;

  row_align = CLAMP (row_align, 0, 1);
  col_align = CLAMP (col_align, 0, 1);

  /* adjust horizontal scrollbar */
  if (column >= 0)
    {
      GtkAdjustment *adj;
      gint x;

      adj = GTK_RANGE (clist->hscrollbar)->adjustment;

      x = COLUMN_LEFT (clist, column) - CELL_SPACING - COLUMN_INSET -
	(col_align * (clist->clist_window_width - 2 * COLUMN_INSET -
		      CELL_SPACING - clist->column[column].area.width));
      if (x < 0)
	gtk_adjustment_set_value (adj, 0.0);
      else if (x > LIST_WIDTH (clist) - clist->clist_window_width)
	gtk_adjustment_set_value 
	  (adj, LIST_WIDTH (clist) - clist->clist_window_width);
      else
	gtk_adjustment_set_value (adj, x);
    }

  /* adjust vertical scrollbar */
  if (row >= 0)
    move_vertical (clist, row, row_align);
}

GtkCellType 
gtk_clist_get_cell_type (GtkCList * clist,
			 gint row,
			 gint column)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, -1);
  g_return_val_if_fail (GTK_IS_CLIST (clist), -1);

  if (row < 0 || row >= clist->rows)
    return -1;
  if (column < 0 || column >= clist->columns)
    return -1;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  return clist_row->cell[column].type;
}

void
gtk_clist_set_text (GtkCList * clist,
		    gint row,
		    gint column,
		    gchar * text)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row >= clist->rows)
    return;
  if (column < 0 || column >= clist->columns)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  /* if text is null, then the cell is empty */
  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, clist_row, column, GTK_CELL_TEXT, text, 0, NULL, NULL);

  /* redraw the list if it's not frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      if (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row, clist_row);
    }
}

gint
gtk_clist_get_text (GtkCList * clist,
		    gint row,
		    gint column,
		    gchar ** text)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, 0);
  g_return_val_if_fail (GTK_IS_CLIST (clist), 0);

  if (row < 0 || row >= clist->rows)
    return 0;
  if (column < 0 || column >= clist->columns)
    return 0;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (clist_row->cell[column].type != GTK_CELL_TEXT)
    return 0;

  if (text)
    *text = GTK_CELL_TEXT (clist_row->cell[column])->text;

  return 1;
}

void
gtk_clist_set_pixmap (GtkCList * clist,
		      gint row,
		      gint column,
		      GdkPixmap * pixmap,
		      GdkBitmap * mask)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row >= clist->rows)
    return;
  if (column < 0 || column >= clist->columns)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  
  gdk_pixmap_ref (pixmap);
  
  if (mask) gdk_pixmap_ref (mask);
  
  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, clist_row, column, GTK_CELL_PIXMAP, NULL, 0, pixmap, mask);

  /* redraw the list if it's not frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      if (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row, clist_row);
    }
}

gint
gtk_clist_get_pixmap (GtkCList * clist,
		      gint row,
		      gint column,
		      GdkPixmap ** pixmap,
		      GdkBitmap ** mask)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, 0);
  g_return_val_if_fail (GTK_IS_CLIST (clist), 0);

  if (row < 0 || row >= clist->rows)
    return 0;
  if (column < 0 || column >= clist->columns)
    return 0;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (clist_row->cell[column].type != GTK_CELL_PIXMAP)
    return 0;

  if (pixmap)
  {
    *pixmap = GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap;
    /* mask can be NULL */
    *mask = GTK_CELL_PIXMAP (clist_row->cell[column])->mask;
  }

  return 1;
}

void
gtk_clist_set_pixtext (GtkCList * clist,
		       gint row,
		       gint column,
		       gchar * text,
		       guint8 spacing,
		       GdkPixmap * pixmap,
		       GdkBitmap * mask)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row >= clist->rows)
    return;
  if (column < 0 || column >= clist->columns)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  
  gdk_pixmap_ref (pixmap);
  if (mask) gdk_pixmap_ref (mask);
  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, clist_row, column, GTK_CELL_PIXTEXT, text, spacing, pixmap, mask);

  /* redraw the list if it's not frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      if (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row, clist_row);
    }
}

gint
gtk_clist_get_pixtext (GtkCList * clist,
		       gint row,
		       gint column,
		       gchar ** text,
		       guint8 * spacing,
		       GdkPixmap ** pixmap,
		       GdkBitmap ** mask)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, 0);
  g_return_val_if_fail (GTK_IS_CLIST (clist), 0);

  if (row < 0 || row >= clist->rows)
    return 0;
  if (column < 0 || column >= clist->columns)
    return 0;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (clist_row->cell[column].type != GTK_CELL_PIXTEXT)
    return 0;

  if (text)
    *text = GTK_CELL_PIXTEXT (clist_row->cell[column])->text;
  if (spacing)
    *spacing = GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing;
  if (pixmap)
    *pixmap = GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap;

  /* mask can be NULL */
  *mask = GTK_CELL_PIXTEXT (clist_row->cell[column])->mask;

  return 1;
}

void
gtk_clist_set_foreground (GtkCList * clist,
			  gint row,
			  GdkColor * color)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row >= clist->rows)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (color)
    {
      clist_row->foreground = *color;
      clist_row->fg_set = TRUE;
    }
  else
    clist_row->fg_set = FALSE;

  if (!GTK_CLIST_FROZEN (clist)
      && (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE))
    GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row, clist_row);
}

void
gtk_clist_set_background (GtkCList * clist,
			  gint row,
			  GdkColor * color)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row >= clist->rows)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (color)
    {
      clist_row->background = *color;
      clist_row->bg_set = TRUE;
    }
  else
    clist_row->bg_set = FALSE;

  if (!GTK_CLIST_FROZEN (clist)
      && (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE))
    GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row, clist_row);
}

void
gtk_clist_set_shift (GtkCList * clist,
		     gint row,
		     gint column,
		     gint vertical,
		     gint horizontal)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row >= clist->rows)
    return;
  if (column < 0 || column >= clist->columns)
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  clist_row->cell[column].vertical = vertical;
  clist_row->cell[column].horizontal = horizontal;

  if (!GTK_CLIST_FROZEN (clist)
      && (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE))
    GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row, clist_row);
}

gint
gtk_clist_append (GtkCList * clist,
		  gchar * text[])
{
  g_return_val_if_fail (clist != NULL, -1);
  g_return_val_if_fail (GTK_IS_CLIST (clist), -1);
  g_return_val_if_fail (text != NULL, -1);

  return GTK_CLIST_CLASS_FW (clist)->insert_row (clist, clist->rows, text);
}

gint
gtk_clist_insert (GtkCList * clist,
		  gint row,
		  gchar * text[])
{
  return GTK_CLIST_CLASS_FW (clist)->insert_row (clist, row, text);
}

static gint
real_insert_row (GtkCList * clist,
		 gint row,
		 gchar * text[])
{
  gint i;
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, -1);
  g_return_val_if_fail (GTK_IS_CLIST (clist), -1);
  g_return_val_if_fail (text != NULL, -1);

  /* return if out of bounds */
  if (row < 0 || row > clist->rows)
    return -1;

  /* create the row */
  clist_row = row_new (clist);

  /* set the text in the row's columns */
  for (i = 0; i < clist->columns; i++)
    if (text[i])
      GTK_CLIST_CLASS_FW (clist)->set_cell_contents
	(clist, clist_row, i, GTK_CELL_TEXT, text[i], 0, NULL ,NULL);

  if (!clist->rows)
    {
      clist->row_list = g_list_append (clist->row_list, clist_row);
      clist->row_list_end = clist->row_list;
    }
  else
    {
      if (GTK_CLIST_AUTO_SORT (clist))   /* override insertion pos */
	{
	  GList *work;
	  
	  row = 0;
	  work = clist->row_list;
	  
	  if (clist->sort_type == GTK_SORT_ASCENDING)
	    {
	      while (row < clist->rows &&
		     clist->compare (clist, clist_row,
				     GTK_CLIST_ROW (work)) > 0)
		{
		  row++;
		  work = work->next;
		}
	    }
	  else
	    {
	      while (row < clist->rows &&
		     clist->compare (clist, clist_row,
				     GTK_CLIST_ROW (work)) < 0)
		{
		  row++;
		  work = work->next;
		}
	    }
	}
      
      /* reset the row end pointer if we're inserting at the end of the list */
      if (row == clist->rows)
	clist->row_list_end = (g_list_append (clist->row_list_end, clist_row))->next;
      else
	clist->row_list = g_list_insert (clist->row_list, clist_row, row);
    }
  
  clist->rows++;

  if (row < ROW_FROM_YPIXEL (clist, 0))
    clist->voffset -= (clist->row_height + CELL_SPACING);

  /* syncronize the selection list */
  sync_selection (clist, row, SYNC_INSERT);

  /* redraw the list if it isn't frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      adjust_scrollbars (clist);

      if (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
	draw_rows (clist, NULL);
    }

  return row;
}

void
gtk_clist_remove (GtkCList * clist,
		  gint row)
{
  GTK_CLIST_CLASS_FW (clist)->remove_row (clist, row);
}

static void
real_remove_row (GtkCList * clist,
		 gint row)
{
  gint was_visible, was_selected;
  GList *list;
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  /* return if out of bounds */
  if (row < 0 || row > (clist->rows - 1))
    return;

  was_visible = (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE);
  was_selected = 0;

  /* get the row we're going to delete */
  list = g_list_nth (clist->row_list, row);
  clist_row = list->data;

  /* if we're removing a selected row, we have to make sure
   * it's properly unselected, and then sync up the clist->selected
   * list to reflect the deincrimented indexies of rows after the
   * removal */
  if (clist_row->state == GTK_STATE_SELECTED)
    {
      switch (clist->selection_mode)
	{
	case GTK_SELECTION_SINGLE:
	case GTK_SELECTION_MULTIPLE:
	case GTK_SELECTION_EXTENDED:
	  unselect_row (clist, row, -1, NULL);
	  break;

	case GTK_SELECTION_BROWSE:
	  select_row (clist, row - 1, -1, NULL);
	  break;

	default:
	  break;
	}
    }

  /* reset the row end pointer if we're removing at the
   * end of the list */
  if (row == clist->rows - 1)
    clist->row_list_end = list->prev;
  if (row >= clist->focus_row && clist->focus_row >=0)
    clist->focus_row--;

  clist->row_list = g_list_remove (clist->row_list, clist_row);
  clist->rows--;
  
  if (row < ROW_FROM_YPIXEL (clist, 0))
    clist->voffset += clist->row_height + CELL_SPACING;

  sync_selection (clist, row, SYNC_REMOVE);

  /* toast the row */
  row_delete (clist, clist_row);

  /* redraw the row if it isn't frozen */
  if (!GTK_CLIST_FROZEN (clist))
    {
      adjust_scrollbars (clist);

      if (was_visible)
	draw_rows (clist, NULL);
    }
}

static void
sync_selection (GtkCList * clist,
		gint row,
		gint mode)
{
  GList *list;
  gint d;

  if (mode == SYNC_INSERT)
    d = 1;
  else
    d = -1;
      
  if (clist->focus_row >= row)
    {
      clist->focus_row += d;
      if (clist->focus_row == -1 && clist->rows >= 1)
	clist->focus_row = 0;
    }

  if (clist->selection_mode == GTK_SELECTION_BROWSE && clist->anchor != -1)
    GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);

  g_list_free (clist->undo_selection);
  g_list_free (clist->undo_unselection);
  clist->undo_selection = NULL;
  clist->undo_unselection = NULL;

  clist->anchor = -1;
  clist->drag_pos = -1;
  clist->undo_anchor = clist->focus_row;

  list = clist->selection;
  while (list)
    {
      if (GPOINTER_TO_INT (list->data) >= row)
	list->data = ((gchar*) list->data) + d;
      list = list->next;
    }
}

void
gtk_clist_clear (GtkCList * clist)
{
  GTK_CLIST_CLASS_FW (clist)->clear (clist);
}

static void
real_clear (GtkCList * clist)
{
  GList *list;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  /* remove all the rows */
  for (list = clist->row_list; list; list = list->next)
    row_delete (clist, GTK_CLIST_ROW (list));

  g_list_free (clist->row_list);

  /* free up the selection list */
  g_list_free (clist->selection);
  g_list_free (clist->undo_selection);
  g_list_free (clist->undo_unselection);

  clist->row_list = NULL;
  clist->row_list_end = NULL;
  clist->selection = NULL;
  clist->selection_end = NULL;
  clist->undo_selection = NULL;
  clist->undo_unselection = NULL;
  clist->voffset = 0;
  clist->rows = 0;
  clist->focus_row = -1;
  clist->anchor = -1;
  clist->undo_anchor = -1;
  clist->anchor_state = GTK_STATE_SELECTED;
  clist->drag_pos = -1;

  /* zero-out the scrollbars */
  if (clist->vscrollbar)
    {
      GTK_RANGE (clist->vscrollbar)->adjustment->value = 0.0;
      gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->vscrollbar)->adjustment), "changed");
      
      if (!GTK_CLIST_FROZEN (clist))
	gtk_clist_thaw (clist);
    }
}

void 
gtk_clist_swap_rows (GtkCList * clist,
		     gint row1, 
		     gint row2)
{
  gint first, last;
  GList *list, *link1, *link2;
  gpointer swap;
  
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (GTK_CLIST_AUTO_SORT (clist))
    return;

  if (row1 < 0 || row1 > (clist->rows - 1))
    return;

  if (row2 < 0 || row2 > (clist->rows - 1))
    return;

  first = MIN (row1, row2);
  last = MAX (row1, row2);

  link1 = g_list_nth (clist->row_list, first);
  link2 = g_list_nth (link1, row2 - row1);

  swap = link1->data;
  link1->data = link2->data;
  link2->data = swap;
  
  list = clist->selection;
  while (list)
    {
      if (GPOINTER_TO_INT (list->data) == row1)
	list->data = GINT_TO_POINTER (row2);
      
      if (GPOINTER_TO_INT (list->data) == row2)
	list->data = GINT_TO_POINTER (row1);
      
      list = list->next;
    }
  
  if (!GTK_CLIST_FROZEN (clist))
    {
      if (gtk_clist_row_is_visible (clist, row1) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row
	  (clist, NULL, row1, GTK_CLIST_ROW (link2));

      if (gtk_clist_row_is_visible (clist, row2) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row
	  (clist, NULL, row2, GTK_CLIST_ROW (link1));
    }
}

void
gtk_clist_set_row_data (GtkCList * clist,
			gint row,
			gpointer data)
{
  gtk_clist_set_row_data_full (clist, row, data, NULL);
}

void
gtk_clist_set_row_data_full (GtkCList * clist,
			     gint row,
			     gpointer data,
			     GtkDestroyNotify destroy)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row > (clist->rows - 1))
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  clist_row->data = data;
  clist_row->destroy = destroy;
}

gpointer
gtk_clist_get_row_data (GtkCList * clist,
			gint row)
{
  GtkCListRow *clist_row;

  g_return_val_if_fail (clist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CLIST (clist), NULL);

  if (row < 0 || row > (clist->rows - 1))
    return NULL;

  clist_row = (g_list_nth (clist->row_list, row))->data;
  return clist_row->data;
}

gint
gtk_clist_find_row_from_data (GtkCList * clist,
			      gpointer data)
{
  GList *list;
  gint n;

  g_return_val_if_fail (clist != NULL, -1);
  g_return_val_if_fail (GTK_IS_CLIST (clist), -1);

  if (clist->rows < 1)
    return -1; /* is this an optimization or just worthless? */

  for (n = 0, list = clist->row_list; list; n++, list = list->next)
    if (GTK_CLIST_ROW (list)->data == data)
      return n;

  return -1;
}

void
gtk_clist_select_row (GtkCList * clist,
		      gint row,
		      gint column)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row >= clist->rows)
    return;

  if (column < -1 || column >= clist->columns)
    return;

  select_row (clist, row, column, NULL);
}

void
gtk_clist_unselect_row (GtkCList * clist,
			gint row,
			gint column)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row >= clist->rows)
    return;

  if (column < -1 || column >= clist->columns)
    return;

  unselect_row (clist, row, column, NULL);
}

GtkVisibility
gtk_clist_row_is_visible (GtkCList * clist,
			  gint row)
{
  gint top;

  g_return_val_if_fail (clist != NULL, 0);
  g_return_val_if_fail (GTK_IS_CLIST (clist), 0);

  if (row < 0 || row >= clist->rows)
    return GTK_VISIBILITY_NONE;

  if (clist->row_height == 0)
    return GTK_VISIBILITY_NONE;

  if (row < ROW_FROM_YPIXEL (clist, 0))
    return GTK_VISIBILITY_NONE;

  if (row > ROW_FROM_YPIXEL (clist, clist->clist_window_height))
    return GTK_VISIBILITY_NONE;

  top = ROW_TOP_YPIXEL (clist, row);

  if ((top < 0)
      || ((top + clist->row_height) >= clist->clist_window_height))
    return GTK_VISIBILITY_PARTIAL;

  return GTK_VISIBILITY_FULL;
}

static GtkAdjustment*
gtk_clist_get_vadjustment (GtkCList * clist)
{
  g_return_val_if_fail (clist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CLIST (clist), NULL);

  return gtk_range_get_adjustment (GTK_RANGE (clist->vscrollbar));
}

static GtkAdjustment*
gtk_clist_get_hadjustment (GtkCList * clist)
{
  g_return_val_if_fail (clist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_CLIST (clist), NULL);

  return gtk_range_get_adjustment (GTK_RANGE (clist->hscrollbar));
}

void
gtk_clist_set_policy (GtkCList * clist,
		      GtkPolicyType vscrollbar_policy,
		      GtkPolicyType hscrollbar_policy)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (clist->vscrollbar_policy != vscrollbar_policy)
    {
      clist->vscrollbar_policy = vscrollbar_policy;

      if (GTK_WIDGET (clist)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (clist));
    }

  if (clist->hscrollbar_policy != hscrollbar_policy)
    {
      clist->hscrollbar_policy = hscrollbar_policy;

      if (GTK_WIDGET (clist)->parent)
	gtk_widget_queue_resize (GTK_WIDGET (clist));
    }
}

void
gtk_clist_undo_selection (GtkCList *clist)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (clist->selection_mode == GTK_SELECTION_EXTENDED &&
      (clist->undo_selection || clist->undo_unselection))
    gtk_signal_emit (GTK_OBJECT (clist), clist_signals[UNDO_SELECTION]);
}

static void
real_undo_selection (GtkCList *clist)
{
  GList *work;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist)) ||
      clist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  if (clist->anchor >= 0)
    GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);

  if (!(clist->undo_selection || clist->undo_unselection))
    {
      gtk_clist_unselect_all (clist);
      return;
    }

  for (work = clist->undo_selection; work; work = work->next)
    gtk_signal_emit (GTK_OBJECT (clist), clist_signals[SELECT_ROW],
		     GPOINTER_TO_INT (work->data), -1, NULL);

  for (work = clist->undo_unselection; work; work = work->next)
    gtk_signal_emit (GTK_OBJECT (clist), clist_signals[UNSELECT_ROW], 
		     GPOINTER_TO_INT (work->data), -1, NULL);

  if (GTK_WIDGET_HAS_FOCUS (clist) && clist->focus_row != clist->undo_anchor)
    {
      gtk_clist_draw_focus (GTK_WIDGET (clist));
      clist->focus_row = clist->undo_anchor;
      gtk_clist_draw_focus (GTK_WIDGET (clist));
    }
  else
    clist->focus_row = clist->undo_anchor;
  
  clist->undo_anchor = -1;
 
  g_list_free (clist->undo_selection);
  g_list_free (clist->undo_unselection);
  clist->undo_selection = NULL;
  clist->undo_unselection = NULL;

  if (ROW_TOP_YPIXEL (clist, clist->focus_row) + clist->row_height >
      clist->clist_window_height)
    gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);
  else if (ROW_TOP_YPIXEL (clist, clist->focus_row) < 0)
    gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);
}

/*
 * GTKOBJECT
 *   gtk_clist_destroy
 *   gtk_clist_finalize
 */
static void
gtk_clist_destroy (GtkObject * object)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_CLIST (object));

  clist = GTK_CLIST (object);

  /* freeze the list */
  GTK_CLIST_SET_FLAG (clist, CLIST_FROZEN);

  /* get rid of all the rows */
  gtk_clist_clear (clist);

  /* Since we don't have a _remove method, unparent the children
   * instead of destroying them so the focus will be unset properly.
   * (For other containers, the _remove method takes care of the
   * unparent) The destroy will happen when the refcount drops
   * to zero.
   */

  /* destroy the scrollbars */
  if (clist->vscrollbar)
    {
      gtk_widget_unparent (clist->vscrollbar);
      clist->vscrollbar = NULL;
    }
  if (clist->hscrollbar)
    {
      gtk_widget_unparent (clist->hscrollbar);
      clist->hscrollbar = NULL;
    }

  if (clist->htimer)
    {
      gtk_timeout_remove (clist->htimer);
      clist->htimer = 0;
    }
  if (clist->vtimer)
    {
      gtk_timeout_remove (clist->vtimer);
      clist->vtimer = 0;
    }

  /* destroy the column buttons */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      {
	gtk_widget_unparent (clist->column[i].button);
	clist->column[i].button = NULL;
      }

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_clist_finalize (GtkObject * object)
{
  GtkCList *clist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_CLIST (object));

  clist = GTK_CLIST (object);

  columns_delete (clist);

  g_mem_chunk_destroy (clist->cell_mem_chunk);
  g_mem_chunk_destroy (clist->row_mem_chunk);

  if (GTK_OBJECT_CLASS (parent_class)->finalize)
    (*GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/*
 * GTKWIDGET
 *   gtk_clist_realize
 *   gtk_clist_unrealize
 *   gtk_clist_map
 *   gtk_clist_unmap
 *   gtk_clist_draw
 *   gtk_clist_expose
 *   gtk_clist_button_press
 *   gtk_clist_button_release
 *   gtk_clist_motion
 *   gtk_clist_size_request
 *   gtk_clist_size_allocate
 */
static void
gtk_clist_realize (GtkWidget * widget)
{
  gint i;
  GtkCList *clist;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkGCValues values;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));

  clist = GTK_CLIST (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  add_style_data (clist);

  border_width = GTK_CONTAINER (widget)->border_width;
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_KEY_RELEASE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  /* main window */
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, clist);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  /* column-title window */

  attributes.x = clist->column_title_area.x;
  attributes.y = clist->column_title_area.y;
  attributes.width = clist->column_title_area.width;
  attributes.height = clist->column_title_area.height;
  
  clist->title_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (clist->title_window, clist);

  gtk_style_set_background (widget->style, clist->title_window, GTK_STATE_SELECTED);
  gdk_window_show (clist->title_window);

  /* set things up so column buttons are drawn in title window */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      gtk_widget_set_parent_window (clist->column[i].button, clist->title_window);

  /* clist-window */
  attributes.x = clist->internal_allocation.x + widget->style->klass->xthickness;
  attributes.y = clist->internal_allocation.y + widget->style->klass->ythickness +
    clist->column_title_area.height;
  attributes.width = clist->clist_window_width;
  attributes.height = clist->clist_window_height;
  
  clist->clist_window = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (clist->clist_window, clist);

  gdk_window_set_background (clist->clist_window, &widget->style->bg[GTK_STATE_PRELIGHT]);
  gdk_window_show (clist->clist_window);
  gdk_window_get_size (clist->clist_window, &clist->clist_window_width,
		       &clist->clist_window_height);

  /* create resize windows */
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = (GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_POINTER_MOTION_MASK |
			   GDK_POINTER_MOTION_HINT_MASK |
			   GDK_KEY_PRESS_MASK);
  attributes.cursor = clist->cursor_drag = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
  attributes_mask = GDK_WA_CURSOR;
  
  for (i = 0; i < clist->columns; i++)
    {
      clist->column[i].window = gdk_window_new (clist->title_window, &attributes, attributes_mask);
      gdk_window_set_user_data (clist->column[i].window, clist);
    }

  /* This is slightly less efficient than creating them with the
   * right size to begin with, but easier
   */
  size_allocate_title_buttons (clist);

  /* GCs */
  clist->fg_gc = gdk_gc_new (widget->window);
  clist->bg_gc = gdk_gc_new (widget->window);
  
  /* We'll use this gc to do scrolling as well */
  gdk_gc_set_exposures (clist->fg_gc, TRUE);

  values.foreground = widget->style->white;
  values.function = GDK_XOR;
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  clist->xor_gc = gdk_gc_new_with_values (widget->window,
					  &values,
					  GDK_GC_FOREGROUND |
					  GDK_GC_FUNCTION |
					  GDK_GC_SUBWINDOW);
}

static void
gtk_clist_unrealize (GtkWidget * widget)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));

  clist = GTK_CLIST (widget);

  GTK_CLIST_SET_FLAG (clist, CLIST_FROZEN);

  gdk_cursor_destroy (clist->cursor_drag);
  gdk_gc_destroy (clist->xor_gc);
  gdk_gc_destroy (clist->fg_gc);
  gdk_gc_destroy (clist->bg_gc);

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].window)
      {
	gdk_window_set_user_data (clist->column[i].window, NULL);
	gdk_window_destroy (clist->column[i].window);
	clist->column[i].window = NULL;
      }

  gdk_window_set_user_data (clist->clist_window, NULL);
  gdk_window_destroy (clist->clist_window);
  clist->clist_window = NULL;

  gdk_window_set_user_data (clist->title_window, NULL);
  gdk_window_destroy (clist->title_window);
  clist->title_window = NULL;

  clist->cursor_drag = NULL;
  clist->xor_gc = NULL;
  clist->fg_gc = NULL;
  clist->bg_gc = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_clist_map (GtkWidget * widget)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));

  clist = GTK_CLIST (widget);

  if (!GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

      gdk_window_show (widget->window);
      gdk_window_show (clist->title_window);
      gdk_window_show (clist->clist_window);

      /* map column buttons */
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].button &&
	    GTK_WIDGET_VISIBLE (clist->column[i].button) &&
	    !GTK_WIDGET_MAPPED (clist->column[i].button))
	  gtk_widget_map (clist->column[i].button);
      
      /* map resize windows AFTER column buttons (above) */
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].window && clist->column[i].button)
	  gdk_window_show (clist->column[i].window);
       
      /* map vscrollbars */
      if (GTK_WIDGET_VISIBLE (clist->vscrollbar) &&
	  !GTK_WIDGET_MAPPED (clist->vscrollbar))
	gtk_widget_map (clist->vscrollbar);

      if (GTK_WIDGET_VISIBLE (clist->hscrollbar) &&
	  !GTK_WIDGET_MAPPED (clist->hscrollbar))
	gtk_widget_map (clist->hscrollbar);

      /* unfreeze the list */
      GTK_CLIST_UNSET_FLAG (clist, CLIST_FROZEN);
    }
}

static void
gtk_clist_unmap (GtkWidget * widget)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));

  clist = GTK_CLIST (widget);

  if (GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].window)
	  gdk_window_hide (clist->column[i].window);

      gdk_window_hide (clist->clist_window);
      gdk_window_hide (clist->title_window);
      gdk_window_hide (widget->window);

      /* unmap scrollbars */
      if (GTK_WIDGET_MAPPED (clist->vscrollbar))
	gtk_widget_unmap (clist->vscrollbar);

      if (GTK_WIDGET_MAPPED (clist->hscrollbar))
	gtk_widget_unmap (clist->hscrollbar);

      /* unmap column buttons */
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].button &&
	    GTK_WIDGET_MAPPED (clist->column[i].button))
	  gtk_widget_unmap (clist->column[i].button);

      /* freeze the list */
      GTK_CLIST_SET_FLAG (clist, CLIST_FROZEN);
    }
}

static void
gtk_clist_draw (GtkWidget * widget,
		GdkRectangle * area)
{
  GtkCList *clist;
  gint border_width;
  GdkRectangle child_area;
  int i;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      clist = GTK_CLIST (widget);
      border_width = GTK_CONTAINER (widget)->border_width;

      gdk_window_clear_area (widget->window,
			     area->x - border_width, 
			     area->y - border_width,
			     area->width, area->height);

      /* draw list shadow/border */
      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, clist->shadow_type,
		       0, 0, 
		       clist->clist_window_width + (2 * widget->style->klass->xthickness),
		       clist->clist_window_height + (2 * widget->style->klass->ythickness) +
		       clist->column_title_area.height);

      gdk_window_clear_area (clist->clist_window,
			     0, 0, -1, -1);

      draw_rows (clist, NULL);

      for (i = 0; i < clist->columns; i++)
	{
	  if (gtk_widget_intersect (clist->column[i].button, area, &child_area))
	    gtk_widget_draw (clist->column[i].button, &child_area);
	}
    }
}

static gint
gtk_clist_expose (GtkWidget * widget,
		  GdkEventExpose * event)
{
  GtkCList *clist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      clist = GTK_CLIST (widget);

      /* draw border */
      if (event->window == widget->window)
	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, clist->shadow_type,
			 0, 0,
			 clist->clist_window_width + (2 * widget->style->klass->xthickness),
			 clist->clist_window_height + (2 * widget->style->klass->ythickness) +
			 clist->column_title_area.height);

      /* exposure events on the list */
      if (event->window == clist->clist_window)
	draw_rows (clist, &event->area);
    }

  return FALSE;
}

static gint
gtk_clist_button_press (GtkWidget * widget,
			GdkEventButton * event)
{
  gint i;
  GtkCList *clist;
  gint x;
  gint y;
  gint row;
  gint column;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  clist = GTK_CLIST (widget);

  /* we don't handle button 2 and 3 */
  if (event->button != 1)
    return FALSE;
  
  /* selections on the list */
  if (event->window == clist->clist_window)
    {
      x = event->x;
      y = event->y;

      if (get_selection_info (clist, x, y, &row, &column))
	{
	  gint old_row = clist->focus_row;

	  if (clist->focus_row == -1)
	    old_row = row;

	  if (event->type == GDK_BUTTON_PRESS)
	    {
	      GTK_CLIST_SET_FLAG (clist, CLIST_DRAG_SELECTION);
	      gdk_pointer_grab (clist->clist_window, FALSE,
				GDK_POINTER_MOTION_HINT_MASK |
				GDK_BUTTON1_MOTION_MASK |
				GDK_BUTTON_RELEASE_MASK,
				NULL, NULL, event->time);
	      gtk_grab_add (widget);
	    }
	  else if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (widget))
	    {
	      GTK_CLIST_UNSET_FLAG (clist, CLIST_DRAG_SELECTION);
	      gtk_grab_remove (widget);
	      gdk_pointer_ungrab (event->time);
	    }

	  if (GTK_CLIST_ADD_MODE (clist))
	    {
	      GTK_CLIST_UNSET_FLAG (clist, CLIST_ADD_MODE);
	      if (GTK_WIDGET_HAS_FOCUS (widget))
		{
		  gtk_clist_draw_focus (widget);
		  gdk_gc_set_line_attributes (clist->xor_gc, 1,
					      GDK_LINE_SOLID, 0, 0);
		  clist->focus_row = row;
		  gtk_clist_draw_focus (widget);
		}
	      else
		{
		  gdk_gc_set_line_attributes (clist->xor_gc, 1,
					      GDK_LINE_SOLID, 0, 0);
		  clist->focus_row = row;
		}
	    }
	  else if (row != clist->focus_row)
	    {
	      if (GTK_WIDGET_HAS_FOCUS (widget))
		{
		  gtk_clist_draw_focus (widget);
		  clist->focus_row = row;
		  gtk_clist_draw_focus (widget);
		}
	      else
		clist->focus_row = row;
	    }

	  if (!GTK_WIDGET_HAS_FOCUS (widget))
	    gtk_widget_grab_focus (widget);

	  switch (clist->selection_mode)
	    {
	    case GTK_SELECTION_SINGLE:
	    case GTK_SELECTION_MULTIPLE:
	      if (event->type != GDK_BUTTON_PRESS)
		select_row (clist, row, column, (GdkEvent *) event);
	      else
		clist->anchor = row;
	      break;

	    case GTK_SELECTION_BROWSE:
	      select_row (clist, row, column, (GdkEvent *) event);
	      break;

	    case GTK_SELECTION_EXTENDED:
	      if (event->type != GDK_BUTTON_PRESS)
		{
		  if (clist->anchor != -1)
		    {
		      update_extended_selection (clist, clist->focus_row);
		      GTK_CLIST_CLASS_FW (clist)->resync_selection
			(clist, (GdkEvent *) event);
		    }
		  select_row (clist, row, column, (GdkEvent *) event);
		  break;
		}
	      
	      if (event->state & GDK_CONTROL_MASK)
		{
		  if (event->state & GDK_SHIFT_MASK)
		    {
		      if (clist->anchor < 0)
			{
			  g_list_free (clist->undo_selection);
			  g_list_free (clist->undo_unselection);
			  clist->undo_selection = NULL;
			  clist->undo_unselection = NULL;
			  clist->anchor = old_row;
			  clist->drag_pos = old_row;
			  clist->undo_anchor = old_row;
			}
		      update_extended_selection (clist, clist->focus_row);
		    }
		  else
		    {
		      if (clist->anchor == -1)
			set_anchor (clist, TRUE, row, old_row);
		      else
			update_extended_selection (clist, clist->focus_row);
		    }
		  break;
		}

	      if (event->state & GDK_SHIFT_MASK)
		{
		  set_anchor (clist, FALSE, old_row, old_row);
		  update_extended_selection (clist, clist->focus_row);
		  break;
		}

	      if (clist->anchor == -1)
		set_anchor (clist, FALSE, row, old_row);
	      else
		update_extended_selection (clist, clist->focus_row);
	      break;

	    default:
	      break;
	    }
	}

      return FALSE;
    }

  /* press on resize windows */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].window && event->window == clist->column[i].window)
      {
	gdk_pointer_grab (clist->column[i].window, FALSE,
			  GDK_POINTER_MOTION_HINT_MASK |
			  GDK_BUTTON1_MOTION_MASK |
			  GDK_BUTTON_RELEASE_MASK,
			  NULL, NULL, event->time);
	gtk_grab_add (widget);
	GTK_CLIST_SET_FLAG (clist, CLIST_IN_DRAG);

	if (!GTK_WIDGET_HAS_FOCUS (widget))
	  gtk_widget_grab_focus (widget);

	clist->drag_pos = i;
	clist->x_drag = (COLUMN_LEFT_XPIXEL(clist, i) + COLUMN_INSET +
			 clist->column[i].area.width + CELL_SPACING);

	if (GTK_CLIST_ADD_MODE (clist))
	  gdk_gc_set_line_attributes (clist->xor_gc, 1, GDK_LINE_SOLID, 0, 0);
	draw_xor_line (clist);

	return FALSE;
      }

  return FALSE;
}

static gint
gtk_clist_button_release (GtkWidget * widget,
			  GdkEventButton * event)
{
  GtkCList *clist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  clist = GTK_CLIST (widget);

  /* we don't handle button 2 and 3 */
  if (event->button != 1)
    return FALSE;

  /* release on resize windows */
  if (GTK_CLIST_IN_DRAG (clist))
    {
      gint i, x, width, visible;

      i = clist->drag_pos;
      clist->drag_pos = -1;
      GTK_CLIST_UNSET_FLAG (clist, CLIST_IN_DRAG);
      gtk_widget_get_pointer (widget, &x, NULL);

      width = new_column_width (clist, i, &x, &visible);
      gtk_grab_remove (widget);
      gdk_pointer_ungrab (event->time);

      if (visible)
	draw_xor_line (clist);

      if (GTK_CLIST_ADD_MODE (clist))
	{
	  gdk_gc_set_line_attributes (clist->xor_gc, 1,
				      GDK_LINE_ON_OFF_DASH, 0, 0);
	  gdk_gc_set_dashes (clist->xor_gc, 0, "\4\4", 2);
	}

      resize_column (clist, i, width);
      return FALSE;
    }

  if (GTK_CLIST_DRAG_SELECTION (clist))
    {
      gint row;
      gint column;

      GTK_CLIST_UNSET_FLAG (clist, CLIST_DRAG_SELECTION);
      gtk_grab_remove (widget);
      gdk_pointer_ungrab (event->time);
      if (clist->htimer)
	{
	  gtk_timeout_remove (clist->htimer);
	  clist->htimer = 0;
	}
      if (clist->vtimer)
	{
	  gtk_timeout_remove (clist->vtimer);
	  clist->vtimer = 0;
	}
      switch (clist->selection_mode)
	{
	case GTK_SELECTION_EXTENDED:
	  if (!(event->state & GDK_SHIFT_MASK) ||
	      event->x < 0 || event->x >= clist->clist_window_width ||
	      event->y < 0 || event->y >= clist->clist_window_height)
	    GTK_CLIST_CLASS_FW (clist)->resync_selection
	      (clist, (GdkEvent *) event);
	  break;

	case GTK_SELECTION_SINGLE:
	case GTK_SELECTION_MULTIPLE:
	  if (get_selection_info (clist, event->x, event->y, &row, &column))
	    {
	      if (clist->anchor == clist->focus_row)
		toggle_row (clist, row, column, (GdkEvent *) event);
	    }
	  clist->anchor = -1;
	  break;

	default:
	  break;
	}
    }

  return FALSE;
}

static gint
horizontal_timeout (GtkCList *clist)
{
  gint x, y;
  GdkEventMotion event;
  GdkModifierType mask;

  g_return_val_if_fail (GTK_IS_CLIST (clist), FALSE);

  clist->htimer = 0;
  gdk_window_get_pointer (clist->clist_window, &x, &y, &mask);

  event.is_hint = 0;
  event.x = x;
  event.y = y;
  event.state = mask;

  gtk_clist_motion (GTK_WIDGET (clist), &event);

  return FALSE;
}

static gint
vertical_timeout (GtkCList *clist)
{
  gint x, y;
  GdkEventMotion event;
  GdkModifierType mask;

  g_return_val_if_fail (GTK_IS_CLIST (clist), FALSE);

  clist->vtimer = 0;
  gdk_window_get_pointer (clist->clist_window, &x, &y, &mask);

  event.is_hint = 0;
  event.x = x;
  event.y = y;
  event.state = mask;

  gtk_clist_motion (GTK_WIDGET (clist), &event);

  return FALSE;
}

static void
move_vertical (GtkCList *clist,
	       gint row,
	       gfloat align)
{
  gint y;
  GtkAdjustment *adj;

  adj = GTK_RANGE (clist->vscrollbar)->adjustment;

  y = ROW_TOP_YPIXEL (clist, row) - clist->voffset;
  
  y = y - align * (clist->clist_window_height - clist->row_height)
    + (2 * align - 1) * CELL_SPACING;
  
  if (y + adj->page_size > adj->upper)
    adj->value = adj->upper - adj->page_size;
  else
    adj->value = y;

  gtk_signal_emit_by_name (GTK_OBJECT (adj), "value_changed");
}

static void
move_horizontal (GtkCList *clist,
		 gint diff)
{
  gfloat upper;
  GtkAdjustment *adj;

  adj = GTK_RANGE (clist->hscrollbar)->adjustment;

  adj->value += diff;

  upper = adj->upper - adj->page_size;
  adj->value = MIN (adj->value, upper);
  adj->value = MAX (adj->value, 0.0);

  gtk_signal_emit_by_name (GTK_OBJECT (adj), "value_changed");
}

static gint
gtk_clist_motion (GtkWidget * widget,
		  GdkEventMotion * event)
{
  GtkCList *clist;
  gint x, y, visible;
  gint row;
  gint new_width;
  static gint cc =0;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);

  clist = GTK_CLIST (widget);
  cc++;
  if (!(gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist)))
    return FALSE;

  if (GTK_CLIST_IN_DRAG (clist))
    {
      if (event->is_hint || event->window != widget->window)
	gtk_widget_get_pointer (widget, &x, NULL);
      else
	x = event->x;

      new_width = new_column_width (clist, clist->drag_pos, &x, &visible);
      /* Welcome to my hack! I'm going to use a value of x_drag = -99999
       * to indicate that the xor line is already invisible */
      
      if (!visible && clist->x_drag != -99999)
	{
	  draw_xor_line (clist);
	  clist->x_drag = -99999;
	}

      if (x != clist->x_drag && visible)
	{
	  if (clist->x_drag != -99999)
	    draw_xor_line (clist);

	  clist->x_drag = x;
	  draw_xor_line (clist);
	}

      if (new_width <= COLUMN_MIN_WIDTH + 1)
	{
	  if (COLUMN_LEFT_XPIXEL (clist, clist->drag_pos) && x < 0)
	    gtk_clist_moveto (clist, -1, clist->drag_pos, 0, 0);
	  return FALSE;
	}
    }

      
  if (event->is_hint || event->window != clist->clist_window)
    gdk_window_get_pointer (clist->clist_window, &x, &y, NULL);

  /* horizontal autoscrolling */
  if (LIST_WIDTH (clist) > clist->clist_window_width &&
      (x < 0 || x >= clist->clist_window_width))
    {
      if (clist->htimer)
	return FALSE;

      clist->htimer = gtk_timeout_add
	(SCROLL_TIME, (GtkFunction) horizontal_timeout, clist);

      if (!((x < 0 && GTK_RANGE (clist->hscrollbar)->adjustment->value == 0) ||
	    (x >= clist->clist_window_width &&
	     GTK_RANGE (clist->hscrollbar)->adjustment->value ==
	     LIST_WIDTH (clist) - clist->clist_window_width)))
	{
	  if (x < 0)
	    move_horizontal (clist, -1 + (x/2));
	  else
	    move_horizontal (clist, 1 + (x - clist->clist_window_width) / 2);
	}
    }

  if (GTK_CLIST_IN_DRAG (clist))
    return FALSE;

  /* vertical autoscrolling */
  row = ROW_FROM_YPIXEL (clist, y);

  /* don't scroll on last pixel row if it's a cell spacing */
  if (y == clist->clist_window_height-1 &&
      y == ROW_TOP_YPIXEL (clist, row-1) + clist->row_height)
    return FALSE;

  if (LIST_HEIGHT (clist) > clist->clist_window_height &&
      (y < 0 || y >= clist->clist_window_height))
    {
      if (clist->vtimer)
	return FALSE;

      clist->vtimer = gtk_timeout_add (SCROLL_TIME,
				       (GtkFunction) vertical_timeout, clist);

      if (GTK_CLIST_DRAG_SELECTION (clist))
	{
	  if ((y < 0 && clist->focus_row == 0) ||
	      (y >= clist->clist_window_height &&
	       clist->focus_row == clist->rows-1))
	    return FALSE;
	}
    }

  row = CLAMP (row, 0, clist->rows - 1);

  if (GTK_CLIST_DRAG_SELECTION (clist))
    {
      if (row == clist->focus_row)
	return FALSE;

      gtk_clist_draw_focus (widget);
      clist->focus_row = row;
      gtk_clist_draw_focus (widget);

      switch (clist->selection_mode)
	{
	case GTK_SELECTION_BROWSE:
	  select_row (clist, clist->focus_row, - 1, (GdkEvent *) event);
	  break;
	  
	case GTK_SELECTION_EXTENDED:
	  update_extended_selection (clist, clist->focus_row);
	  break;
	  
	default:
	  break;
	}
    }
  
  if (ROW_TOP_YPIXEL(clist, row) < 0)
    move_vertical (clist, row, 0);
  else if (ROW_TOP_YPIXEL(clist, row) + clist->row_height >
	   clist->clist_window_height)
    move_vertical (clist, row, 1);

  return FALSE;
}

static void
gtk_clist_size_request (GtkWidget * widget,
			GtkRequisition * requisition)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));
  g_return_if_fail (requisition != NULL);

  clist = GTK_CLIST (widget);

  add_style_data (clist);

  requisition->width = 0;
  requisition->height = 0;

  /* compute the size of the column title (title) area */
  clist->column_title_area.height = 0;
  if (GTK_CLIST_SHOW_TITLES (clist))
    for (i = 0; i < clist->columns; i++)
      if (clist->column[i].button)
	{
	  gtk_widget_size_request (clist->column[i].button, &clist->column[i].button->requisition);
	  clist->column_title_area.height = MAX (clist->column_title_area.height,
						 clist->column[i].button->requisition.height);
	}
  requisition->height += clist->column_title_area.height;

  /* add the vscrollbar space */
  if ((clist->vscrollbar_policy == GTK_POLICY_AUTOMATIC) ||
      GTK_WIDGET_VISIBLE (clist->vscrollbar))
    {
      gtk_widget_size_request (clist->vscrollbar, &clist->vscrollbar->requisition);

      requisition->width += clist->vscrollbar->requisition.width + SCROLLBAR_SPACING (clist);
      requisition->height = MAX (requisition->height,
				 clist->vscrollbar->requisition.height);
    }

  /* add the hscrollbar space */
  if ((clist->hscrollbar_policy == GTK_POLICY_AUTOMATIC) ||
      GTK_WIDGET_VISIBLE (clist->hscrollbar))
    {
      gtk_widget_size_request (clist->hscrollbar, &clist->hscrollbar->requisition);

      requisition->height += clist->hscrollbar->requisition.height + SCROLLBAR_SPACING (clist);
      requisition->width = MAX (clist->hscrollbar->requisition.width, 
				requisition->width - 
				clist->vscrollbar->requisition.width);

    }

  requisition->width += widget->style->klass->xthickness * 2 +
    GTK_CONTAINER (widget)->border_width * 2;
  requisition->height += widget->style->klass->ythickness * 2 +
    GTK_CONTAINER (widget)->border_width * 2;
}

static void
gtk_clist_size_allocate (GtkWidget * widget,
			 GtkAllocation * allocation)
{
  GtkCList *clist;
  GtkAllocation clist_allocation;
  GtkAllocation child_allocation;
  gint i, vscrollbar_vis, hscrollbar_vis;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));
  g_return_if_fail (allocation != NULL);

  clist = GTK_CLIST (widget);
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + GTK_CONTAINER (widget)->border_width,
			      allocation->y + GTK_CONTAINER (widget)->border_width,
			      allocation->width - GTK_CONTAINER (widget)->border_width * 2,
			      allocation->height - GTK_CONTAINER (widget)->border_width * 2);
    }

  /* use internal allocation structure for all the math
   * because it's easier than always subtracting the container
   * border width */
  clist->internal_allocation.x = 0;
  clist->internal_allocation.y = 0;
  clist->internal_allocation.width = MAX (1, allocation->width -
    GTK_CONTAINER (widget)->border_width * 2);
  clist->internal_allocation.height = MAX (1, allocation->height -
    GTK_CONTAINER (widget)->border_width * 2);
	
  /* allocate clist window assuming no scrollbars */
  clist_allocation.x = clist->internal_allocation.x + widget->style->klass->xthickness;
  clist_allocation.y = clist->internal_allocation.y + widget->style->klass->ythickness +
    clist->column_title_area.height;
  clist_allocation.width = MAX (1, clist->internal_allocation.width - 
    (2 * widget->style->klass->xthickness));
  clist_allocation.height = MAX (1, clist->internal_allocation.height -
    (2 * widget->style->klass->ythickness) -
    clist->column_title_area.height);
  
  /* 
   * here's where we decide to show/not show the scrollbars
   */
  vscrollbar_vis = 0;
  hscrollbar_vis = 0;
  
  for (i = 0; i <= 1; i++)
    {
      if (LIST_HEIGHT (clist) <= clist_allocation.height &&
	  clist->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  vscrollbar_vis = 0;
	}
      else
	{
	  if (!vscrollbar_vis)
	    {
	      vscrollbar_vis = 1;
	      clist_allocation.width = MAX (1, clist_allocation.width - 
                (clist->vscrollbar->requisition.width +
		 SCROLLBAR_SPACING (clist)));
	    }  
	}
      
      if (LIST_WIDTH (clist) <= clist_allocation.width &&
	  clist->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
	{
	  hscrollbar_vis = 0;
	}
      else
	{
	  if (!hscrollbar_vis)
	    {
	      hscrollbar_vis = 1;
	      clist_allocation.height = MAX (1,	clist_allocation.height - 
		(clist->hscrollbar->requisition.height +
		 SCROLLBAR_SPACING (clist)));
	    }  
	}
    }
  
  clist->clist_window_width = clist_allocation.width;
  clist->clist_window_height = clist_allocation.height;
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (clist->clist_window,
			      clist_allocation.x,
			      clist_allocation.y,
			      clist_allocation.width,
			      clist_allocation.height);
    }
  
  /* position the window which holds the column title buttons */
  clist->column_title_area.x = widget->style->klass->xthickness;
  clist->column_title_area.y = widget->style->klass->ythickness;
  clist->column_title_area.width = clist_allocation.width;
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (clist->title_window,
			      clist->column_title_area.x,
			      clist->column_title_area.y,
			      clist->column_title_area.width,
			      clist->column_title_area.height);
    }
  
  /* column button allocation */
  size_allocate_columns (clist);
  size_allocate_title_buttons (clist);

  adjust_scrollbars (clist);
  
  /* allocate the vscrollbar */
  if (vscrollbar_vis)
    {
      if (!GTK_WIDGET_VISIBLE (clist->vscrollbar))
	gtk_widget_show (clist->vscrollbar);
      
      child_allocation.x = clist->internal_allocation.x + 
	clist->internal_allocation.width -
	clist->vscrollbar->requisition.width;
      child_allocation.y = clist->internal_allocation.y;
      child_allocation.width = clist->vscrollbar->requisition.width;
      child_allocation.height = MAX (1, clist->internal_allocation.height -
	(hscrollbar_vis ? (clist->hscrollbar->requisition.height + SCROLLBAR_SPACING (clist)) : 0));
      
      gtk_widget_size_allocate (clist->vscrollbar, &child_allocation);
    }
  else
    {
      if (GTK_WIDGET_VISIBLE (clist->vscrollbar))
	gtk_widget_hide (clist->vscrollbar);
    }
  
  if (hscrollbar_vis)
    {
      if (!GTK_WIDGET_VISIBLE (clist->hscrollbar))
	gtk_widget_show (clist->hscrollbar);
      
      child_allocation.x = clist->internal_allocation.x;
      child_allocation.y = clist->internal_allocation.y +
	clist->internal_allocation.height -
	clist->hscrollbar->requisition.height;
      child_allocation.width = MAX (1, clist->internal_allocation.width -
	(vscrollbar_vis ? (clist->vscrollbar->requisition.width + SCROLLBAR_SPACING (clist)) : 0));
      child_allocation.height = clist->hscrollbar->requisition.height;
      
      gtk_widget_size_allocate (clist->hscrollbar, &child_allocation);
    }
  else
    {
      if (GTK_WIDGET_VISIBLE (clist->hscrollbar))
	gtk_widget_hide (clist->hscrollbar);
    }

  /* set the vscrollbar adjustments */
  adjust_scrollbars (clist);
}

/* 
 * GTKCONTAINER
 *   gtk_clist_foreach
 */
static void
gtk_clist_foreach (GtkContainer * container,
		   GtkCallback callback,
		   gpointer callback_data)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CLIST (container));
  g_return_if_fail (callback != NULL);

  clist = GTK_CLIST (container);

  /* callback for the column buttons */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button)
      (*callback) (clist->column[i].button, callback_data);

  /* callbacks for the scrollbars */
  if (clist->vscrollbar)
    (*callback) (clist->vscrollbar, callback_data);
  if (clist->hscrollbar)
    (*callback) (clist->hscrollbar, callback_data);
}

/*
 * DRAWING
 *   draw_row
 *   draw_rows
 */
static void
draw_row (GtkCList * clist,
	  GdkRectangle * area,
	  gint row,
	  GtkCListRow * clist_row)
{
  GtkWidget *widget;
  GdkGC *fg_gc, *bg_gc;
  GdkRectangle row_rectangle, cell_rectangle, clip_rectangle, intersect_rectangle,
   *rect;
  gint i, offset = 0, width, height, pixmap_width = 0;
  gint xsrc, ysrc, xdest, ydest;

  g_return_if_fail (clist != NULL);

  /* bail now if we arn't drawable yet */
  if (!GTK_WIDGET_DRAWABLE (clist))
    return;

  if (row < 0 || row >= clist->rows)
    return;

  widget = GTK_WIDGET (clist);

  /* if the function is passed the pointer to the row instead of null,
   * it avoids this expensive lookup */
  if (!clist_row)
    clist_row = (g_list_nth (clist->row_list, row))->data;

  /* rectangle of the entire row */
  row_rectangle.x = 0;
  row_rectangle.y = ROW_TOP_YPIXEL (clist, row);
  row_rectangle.width = clist->clist_window_width;
  row_rectangle.height = clist->row_height;

  /* rectangle of the cell spacing above the row */
  cell_rectangle.x = 0;
  cell_rectangle.y = row_rectangle.y - CELL_SPACING;
  cell_rectangle.width = row_rectangle.width;
  cell_rectangle.height = CELL_SPACING;

  /* rectangle used to clip drawing operations, it's y and height
   * positions only need to be set once, so we set them once here. 
   * the x and width are set withing the drawing loop below once per
   * column */
  clip_rectangle.y = row_rectangle.y;
  clip_rectangle.height = row_rectangle.height;

  /* select GC for background rectangle */
  if (clist_row->state == GTK_STATE_SELECTED)
    {
      fg_gc = widget->style->fg_gc[GTK_STATE_SELECTED];
      bg_gc = widget->style->bg_gc[GTK_STATE_SELECTED];
    }
  else
    {
      if (clist_row->fg_set)
	{
	  gdk_gc_set_foreground (clist->fg_gc, &clist_row->foreground);
	  fg_gc = clist->fg_gc;
	}
      else
	fg_gc = widget->style->fg_gc[GTK_STATE_NORMAL];
	
      if (clist_row->bg_set)
	{
	  gdk_gc_set_foreground (clist->bg_gc, &clist_row->background);
	  bg_gc = clist->bg_gc;
	}
      else
	bg_gc = widget->style->bg_gc[GTK_STATE_PRELIGHT];
    }

  /* draw the cell borders and background */
  if (area)
    {
      if (gdk_rectangle_intersect (area, &cell_rectangle, &intersect_rectangle))
	gdk_draw_rectangle (clist->clist_window,
			    widget->style->base_gc[GTK_STATE_NORMAL],
			    TRUE,
			    intersect_rectangle.x,
			    intersect_rectangle.y,
			    intersect_rectangle.width,
			    intersect_rectangle.height);

      /* the last row has to clear it's bottom cell spacing too */
      if (clist_row == clist->row_list_end->data)
	{
	  cell_rectangle.y += clist->row_height + CELL_SPACING;

	  if (gdk_rectangle_intersect (area, &cell_rectangle, &intersect_rectangle))
	    gdk_draw_rectangle (clist->clist_window,
				widget->style->base_gc[GTK_STATE_NORMAL],
				TRUE,
				intersect_rectangle.x,
				intersect_rectangle.y,
				intersect_rectangle.width,
				intersect_rectangle.height);
	}

      if (!gdk_rectangle_intersect (area, &row_rectangle, &intersect_rectangle))
	return;

      if (clist_row->state == GTK_STATE_SELECTED || clist_row->bg_set)
	gdk_draw_rectangle (clist->clist_window,
			    bg_gc,
			    TRUE,
			    intersect_rectangle.x,
			    intersect_rectangle.y,
			    intersect_rectangle.width,
			    intersect_rectangle.height);
      else
	gdk_window_clear_area (clist->clist_window,
			       intersect_rectangle.x,
			       intersect_rectangle.y,
			       intersect_rectangle.width,
			       intersect_rectangle.height);
    }
  else
    {
      gdk_draw_rectangle (clist->clist_window,
			  widget->style->base_gc[GTK_STATE_NORMAL],
			  TRUE,
			  cell_rectangle.x,
			  cell_rectangle.y,
			  cell_rectangle.width,
			  cell_rectangle.height);

      /* the last row has to clear it's bottom cell spacing too */
      if (clist_row == clist->row_list_end->data)
	{
	  cell_rectangle.y += clist->row_height + CELL_SPACING;

	  gdk_draw_rectangle (clist->clist_window,
			      widget->style->base_gc[GTK_STATE_NORMAL],
			      TRUE,
			      cell_rectangle.x,
			      cell_rectangle.y,
			      cell_rectangle.width,
			      cell_rectangle.height);     
	}	  

      if (clist_row->state == GTK_STATE_SELECTED || clist_row->bg_set)
	gdk_draw_rectangle (clist->clist_window,
			    bg_gc,
			    TRUE,
			    row_rectangle.x,
			    row_rectangle.y,
			    row_rectangle.width,
			    row_rectangle.height);
      else
	gdk_window_clear_area (clist->clist_window,
			       row_rectangle.x,
			       row_rectangle.y,
			       row_rectangle.width,
			       row_rectangle.height);
    }

  /* iterate and draw all the columns (row cells) and draw their contents */
  for (i = 0; i < clist->columns; i++)
    {
      clip_rectangle.x = clist->column[i].area.x + clist->hoffset;
      clip_rectangle.width = clist->column[i].area.width;

      /* calculate clipping region clipping region */
      if (!area)
	{
	  rect = &clip_rectangle;
	}
      else
	{
	  if (!gdk_rectangle_intersect (area, &clip_rectangle, 
					&intersect_rectangle))
	    continue;
	  rect = &intersect_rectangle;
	}

      /* calculate real width for column justification */
      switch (clist_row->cell[i].type)
	{
	case GTK_CELL_EMPTY:
	  continue;
	  break;

	case GTK_CELL_TEXT:
	  width = gdk_string_width (GTK_WIDGET (clist)->style->font,
				    GTK_CELL_TEXT (clist_row->cell[i])->text);
	  break;

	case GTK_CELL_PIXMAP:
	  gdk_window_get_size (GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap, &width, &height);
	  pixmap_width = width;
	  break;

	case GTK_CELL_PIXTEXT:
	  gdk_window_get_size (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap, &width, &height);
	  pixmap_width = width;
	  width += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	  width = gdk_string_width (GTK_WIDGET (clist)->style->font,
				    GTK_CELL_PIXTEXT (clist_row->cell[i])->text);
	  break;

	case GTK_CELL_WIDGET:
	  /* unimplimented */
	  continue;
	  break;

	default:
	  continue;
	  break;
	}

      switch (clist->column[i].justification)
	{
	case GTK_JUSTIFY_LEFT:
	  offset = clip_rectangle.x;
	  break;

	case GTK_JUSTIFY_RIGHT:
	  offset = (clip_rectangle.x + clip_rectangle.width) - width;
	  break;

	case GTK_JUSTIFY_CENTER:
	  offset = (clip_rectangle.x + (clip_rectangle.width / 2)) - (width / 2);
	  break;

	case GTK_JUSTIFY_FILL:
	  offset = (clip_rectangle.x + (clip_rectangle.width / 2)) - (width / 2);
	  break;

	default:
	  offset = 0;
	  break;
	};

      /* Draw Text or Pixmap */
      switch (clist_row->cell[i].type)
	{
	case GTK_CELL_EMPTY:
	  continue;
	  break;

	case GTK_CELL_TEXT:
	  gdk_gc_set_clip_rectangle (fg_gc, rect);

	  gdk_draw_string (clist->clist_window, 
			   widget->style->font,
			   fg_gc,
			   offset + clist_row->cell[i].horizontal,
			   row_rectangle.y + clist->row_center_offset + 
			   clist_row->cell[i].vertical,
			   GTK_CELL_TEXT (clist_row->cell[i])->text);

	  gdk_gc_set_clip_rectangle (fg_gc, NULL);
	  break;

	case GTK_CELL_PIXMAP:
	  xsrc = 0;
	  ysrc = 0;
	  xdest = offset + clist_row->cell[i].horizontal;
	  ydest = (clip_rectangle.y + (clip_rectangle.height / 2)) - height / 2 +
	    clist_row->cell[i].vertical;

	  if (xdest < clip_rectangle.x)
	    {
	      xsrc = clip_rectangle.x - xdest;
	      pixmap_width -= xsrc;
	      xdest = clip_rectangle.x;
	    }

	  if (xdest + pixmap_width > clip_rectangle.x + clip_rectangle.width)
	    pixmap_width = (clip_rectangle.x + clip_rectangle.width) - xdest;

	  if (ydest < clip_rectangle.y)
	    {
	      ysrc = clip_rectangle.y - ydest;
	      height -= ysrc;
	      ydest = clip_rectangle.y;
	    }

	  if (ydest + height > clip_rectangle.y + clip_rectangle.height)
	    height = (clip_rectangle.y + clip_rectangle.height) - ydest;

          if (GTK_CELL_PIXMAP (clist_row->cell[i])->mask)
          {
              gdk_gc_set_clip_mask (fg_gc, GTK_CELL_PIXMAP (clist_row->cell[i])->mask);
              gdk_gc_set_clip_origin (fg_gc, xdest, ydest);
          }
	  gdk_draw_pixmap (clist->clist_window,
			   fg_gc,
			   GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
			   xsrc, ysrc,
			   xdest, ydest,
			   pixmap_width, height);

          if (GTK_CELL_PIXMAP (clist_row->cell[i])->mask)
          {
              gdk_gc_set_clip_origin (fg_gc, 0, 0);
              gdk_gc_set_clip_mask (fg_gc, NULL);
          }
	  break;

	case GTK_CELL_PIXTEXT:
	  /* draw the pixmap */
	  xsrc = 0;
	  ysrc = 0;
	  xdest = offset + clist_row->cell[i].horizontal;
	  ydest = (clip_rectangle.y + (clip_rectangle.height / 2)) - height / 2 +
	    clist_row->cell[i].vertical;

	  if (xdest < clip_rectangle.x)
	    {
	      xsrc = clip_rectangle.x - xdest;
	      pixmap_width -= xsrc;
	      xdest = clip_rectangle.x;
	    }

	  if (xdest + pixmap_width > clip_rectangle.x + clip_rectangle.width)
	    pixmap_width = (clip_rectangle.x + clip_rectangle.width) - xdest;

	  if (ydest < clip_rectangle.y)
	    {
	      ysrc = clip_rectangle.y - ydest;
	      height -= ysrc;
	      ydest = clip_rectangle.y;
	    }

	  if (ydest + height > clip_rectangle.y + clip_rectangle.height)
	    height = (clip_rectangle.y + clip_rectangle.height) - ydest;

          if (GTK_CELL_PIXTEXT (clist_row->cell[i])->mask)
          {
              gdk_gc_set_clip_mask (fg_gc, GTK_CELL_PIXTEXT (clist_row->cell[i])->mask);
              gdk_gc_set_clip_origin (fg_gc, xdest, ydest);
          }
              
	  gdk_draw_pixmap (clist->clist_window,
			   fg_gc,
			   GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
			   xsrc, ysrc,
			   xdest,
			   ydest,
			   pixmap_width, height);

	  gdk_gc_set_clip_origin (fg_gc, 0, 0);

	  offset += pixmap_width + GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	  
	  /* draw the string */
	  gdk_gc_set_clip_rectangle (fg_gc, rect);

	  gdk_draw_string (clist->clist_window, 
			   widget->style->font,
			   fg_gc,
			   offset + clist_row->cell[i].horizontal,
			   row_rectangle.y + clist->row_center_offset + 
			   clist_row->cell[i].vertical,
			   GTK_CELL_PIXTEXT (clist_row->cell[i])->text);

	  gdk_gc_set_clip_rectangle (fg_gc, NULL);
	  break;

	case GTK_CELL_WIDGET:
	  /* unimplimented */
	  continue;
	  break;

	default:
	  continue;
	  break;
	}
    }
  if (clist->focus_row == row && GTK_WIDGET_HAS_FOCUS (widget))
    {
      if (area)
	{
	  if (gdk_rectangle_intersect (area, &row_rectangle,
				       &intersect_rectangle))
	    {
	      gdk_gc_set_clip_rectangle (clist->xor_gc, &intersect_rectangle);
	      gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
				  row_rectangle.x, row_rectangle.y,
				  row_rectangle.width - 1,
				  row_rectangle.height - 1);
	      gdk_gc_set_clip_rectangle (clist->xor_gc, NULL);
	    }
	}
      else
	gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			    row_rectangle.x, row_rectangle.y,
			    row_rectangle.width - 1, row_rectangle.height - 1);
    }
}

static void
draw_rows (GtkCList * clist,
	   GdkRectangle * area)
{
  GList *list;
  GtkCListRow *clist_row;
  int i, first_row, last_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (clist->row_height == 0 ||
      !GTK_WIDGET_DRAWABLE (clist))
    return;

  if (area)
    {
      first_row = ROW_FROM_YPIXEL (clist, area->y);
      last_row = ROW_FROM_YPIXEL (clist, area->y + area->height);
    }
  else
    {
      first_row = ROW_FROM_YPIXEL (clist, 0);
      last_row = ROW_FROM_YPIXEL (clist, clist->clist_window_height);
    }

  /* this is a small special case which exposes the bottom cell line
   * on the last row -- it might go away if I change the wall the cell spacings
   * are drawn */
  if (clist->rows == first_row)
    first_row--;

  list = g_list_nth (clist->row_list, first_row);
  i = first_row;
  while (list)
    {
      clist_row = list->data;
      list = list->next;

      if (i > last_row)
	return;

      GTK_CLIST_CLASS_FW (clist)->draw_row (clist, area, i, clist_row);
      i++;
    }

  if (!area)
    gdk_window_clear_area (clist->clist_window, 0, ROW_TOP_YPIXEL (clist, i), -1, -1);
}

/*
 * SIZE ALLOCATION
 *   size_allocate_title_buttons
 *   size_allocate_columns
 */
static void
size_allocate_title_buttons (GtkCList * clist)
{
  gint i, last_button = 0;
  GtkAllocation button_allocation;

  if (!GTK_WIDGET_REALIZED (clist))
    return;

  button_allocation.x = clist->hoffset;
  button_allocation.y = 0;
  button_allocation.width = 0;
  button_allocation.height = clist->column_title_area.height;

  for (i = 0; i < clist->columns; i++)
    {
      button_allocation.width += clist->column[i].area.width;

      if (i == clist->columns - 1)
	button_allocation.width += 2 * (CELL_SPACING + COLUMN_INSET);
      else
	button_allocation.width += CELL_SPACING + (2 * COLUMN_INSET);

      if (i == (clist->columns - 1) || clist->column[i + 1].button)
	{
	  gtk_widget_size_allocate (clist->column[last_button].button, &button_allocation);
	  button_allocation.x += button_allocation.width;
	  button_allocation.width = 0;

	  gdk_window_show (clist->column[last_button].window);
	  gdk_window_move_resize (clist->column[last_button].window,
				  button_allocation.x - (DRAG_WIDTH / 2), 
				  0, DRAG_WIDTH, clist->column_title_area.height);
	  
	  last_button = i + 1;
	}
      else
	{
	  gdk_window_hide (clist->column[i].window);
	}
    }
}

static void
size_allocate_columns (GtkCList * clist)
{
  gint i, xoffset = 0;

  for (i = 0; i < clist->columns; i++)
    {
      clist->column[i].area.x = xoffset + CELL_SPACING + COLUMN_INSET;

      if (i == clist->columns - 1)
	{
	  gint width;

	  if (clist->column[i].width_set)
	    {
	      width = clist->column[i].width;
	    }
	  else
	    {
	      if (clist->column[i].title)
		width = gdk_string_width (GTK_WIDGET (clist)->style->font, 
					  clist->column[i].title);
	      else
		width = 0;
	    }

	  clist->column[i].area.width = MAX (width,
					     clist->clist_window_width -
					     xoffset - (2 * (CELL_SPACING + COLUMN_INSET)));
					    
	}
      else
	{
	  clist->column[i].area.width = clist->column[i].width;
	}

      xoffset += clist->column[i].area.width + CELL_SPACING + (2 * COLUMN_INSET);
    }
}

/*
 * SELECTION
 *   select_row
 *   real_select_row
 *   real_unselect_row
 *   get_selection_info
 */
static void
toggle_row (GtkCList * clist,
	    gint row,
	    gint column,
	    GdkEvent * event)
{
  GtkCListRow *clist_row;

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_EXTENDED:
    case GTK_SELECTION_MULTIPLE:
    case GTK_SELECTION_SINGLE:

      clist_row = g_list_nth (clist->row_list, row)->data;
      if (clist_row->state == GTK_STATE_SELECTED)
	{
	  unselect_row (clist, row, column, event);
	  return;
	}

    case GTK_SELECTION_BROWSE:
      select_row (clist, row, column, event);
      break;
    }
}

static void
select_row (GtkCList * clist,
	    gint row,
	    gint column,
	    GdkEvent * event)
{
  gtk_signal_emit (GTK_OBJECT (clist), clist_signals[SELECT_ROW],
		   row, column, event);
}

static void
unselect_row (GtkCList * clist,
	      gint row,
	      gint column,
	      GdkEvent * event)
{
  gtk_signal_emit (GTK_OBJECT (clist), clist_signals[UNSELECT_ROW],
		   row, column, event);
}

static void
real_select_row (GtkCList * clist,
		 gint row,
		 gint column,
		 GdkEvent * event)
{
  GtkCListRow *clist_row;
  GList *list;
  gint sel_row;
  gboolean row_selected;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row > (clist->rows - 1))
    return;

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:

      row_selected = FALSE;
      list = clist->selection;

      while (list)
	{
	  sel_row = GPOINTER_TO_INT (list->data);
	  list = list->next;

	  if (row == sel_row)
	    row_selected = TRUE;
	  else
	    gtk_signal_emit (GTK_OBJECT (clist), clist_signals[UNSELECT_ROW], 
			     sel_row, column, event);
	}

      if (row_selected)
	return;
      
    default:
      break;
    }

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (clist_row->state != GTK_STATE_NORMAL)
    return;

  clist_row->state = GTK_STATE_SELECTED;
  if (!clist->selection)
    {
      clist->selection = g_list_append (clist->selection,
					GINT_TO_POINTER (row));
      clist->selection_end = clist->selection;
    }
  else
    clist->selection_end = 
      g_list_append (clist->selection_end, GINT_TO_POINTER (row))->next;
  
  if (!GTK_CLIST_FROZEN (clist)
      && (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE))
    GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row, clist_row);
}

static void
real_unselect_row (GtkCList * clist,
		   gint row,
		   gint column,
		   GdkEvent * event)
{
  GtkCListRow *clist_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (row < 0 || row > (clist->rows - 1))
    return;

  clist_row = (g_list_nth (clist->row_list, row))->data;

  if (clist_row->state == GTK_STATE_SELECTED)
    {
      clist_row->state = GTK_STATE_NORMAL;

      if (clist->selection_end && 
	  clist->selection_end->data == GINT_TO_POINTER (row))
	clist->selection_end = clist->selection_end->prev;

      clist->selection = g_list_remove (clist->selection,
					GINT_TO_POINTER (row));
      
      if (!GTK_CLIST_FROZEN (clist)
	  && (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE))
	GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row, clist_row);
    }
}

static gint
get_selection_info (GtkCList * clist,
		    gint x,
		    gint y,
		    gint * row,
		    gint * column)
{
  gint trow, tcol;

  g_return_val_if_fail (clist != NULL, 0);
  g_return_val_if_fail (GTK_IS_CLIST (clist), 0);

  /* bounds checking, return false if the user clicked 
   * on a blank area */
  trow = ROW_FROM_YPIXEL (clist, y);
  if (trow >= clist->rows)
    return 0;

  if (row)
    *row = trow;

  tcol = COLUMN_FROM_XPIXEL (clist, x);
  if (tcol >= clist->columns)
    return 0;

  if (column)
    *column = tcol;

  return 1;
}

gint
gtk_clist_get_selection_info (GtkCList *clist, 
			      gint      x, 
			      gint      y, 
			      gint *    row, 
			      gint *    column)
{
  g_return_val_if_fail (clist != NULL, 0);
  g_return_val_if_fail (GTK_IS_CLIST (clist), 0);
  return get_selection_info (clist, x, y, row, column);
}

/* 
 * RESIZE COLUMNS
 *   draw_xor_line
 *   new_column_width
 *   resize_column
 */
static void                          
draw_xor_line (GtkCList * clist)
{
  GtkWidget *widget;
  
  g_return_if_fail (clist != NULL);
  
  widget = GTK_WIDGET (clist);

  gdk_draw_line (widget->window, clist->xor_gc,  
                 clist->x_drag,                                       
                 widget->style->klass->ythickness,                               
                 clist->x_drag,                                             
                 clist->column_title_area.height + clist->clist_window_height + 1);
}

/* this function returns the new width of the column being resized given
 * the column and x position of the cursor; the x cursor position is passed
 * in as a pointer and automagicly corrected if it's beyond min/max limits */
static gint
new_column_width (GtkCList * clist,
		  gint column,
		  gint * x,
		  gint * visible)
{
  gint cx, rx, width;

  cx = *x;

  /* first translate the x position from widget->window
   * to clist->clist_window */
  cx -= GTK_WIDGET (clist)->style->klass->xthickness;

  /* rx is x from the list beginning */
  rx = cx - clist->hoffset;

  /* you can't shrink a column to less than its minimum width */
  if (cx < (COLUMN_LEFT_XPIXEL (clist, column) + CELL_SPACING + COLUMN_INSET + COLUMN_MIN_WIDTH))
    {
      *x = cx = COLUMN_LEFT_XPIXEL (clist, column) + CELL_SPACING + COLUMN_INSET + COLUMN_MIN_WIDTH +
	GTK_WIDGET (clist)->style->klass->xthickness;
      cx -= GTK_WIDGET (clist)->style->klass->xthickness;
      rx = cx - clist->hoffset;
    }

  if (cx < 0 || cx > clist->clist_window_width)
    *visible = 0;
  else
    *visible = 1;

  /* calculate new column width making sure it doesn't end up
   * less than the minimum width */
  width = (rx - COLUMN_LEFT (clist, column)) - COLUMN_INSET -
    ((clist->columns == (column - 1)) ? CELL_SPACING : 0);
  if (width < COLUMN_MIN_WIDTH)
    width = COLUMN_MIN_WIDTH;

  return width;
}

/* this will do more later */
static void
resize_column (GtkCList * clist,
	       gint column,
	       gint width)
{
  gtk_clist_set_column_width (clist, column, width);
}

/* BUTTONS */
static void
column_button_create (GtkCList * clist,
		      gint column)
{
  GtkWidget *button;

  button = clist->column[column].button = gtk_button_new ();
  gtk_widget_set_parent (button, GTK_WIDGET (clist));
  if (GTK_WIDGET_REALIZED (clist) && clist->title_window)
    gtk_widget_set_parent_window (clist->column[column].button, clist->title_window);
  
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) column_button_clicked,
		      (gpointer) clist);

  gtk_widget_show (button);
}

static void
column_button_clicked (GtkWidget * widget,
		       gpointer data)
{
  gint i;
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (data));

  clist = GTK_CLIST (data);

  /* find the column who's button was pressed */
  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].button == widget)
      break;

  gtk_signal_emit (GTK_OBJECT (clist), clist_signals[CLICK_COLUMN], i);
}

/* 
 * SCROLLBARS
 *
 * functions:
 *   create_scrollbars
 *   adjust_scrollbars
 *   vadjustment_changed
 *   hadjustment_changed
 *   vadjustment_value_changed
 *   hadjustment_value_changed 
 */
static void
create_scrollbars (GtkCList * clist)
{
  GtkAdjustment *adjustment;

  clist->vscrollbar = gtk_vscrollbar_new (NULL);

  adjustment = gtk_range_get_adjustment (GTK_RANGE (clist->vscrollbar));

  gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
		      (GtkSignalFunc) vadjustment_changed,
		      (gpointer) clist);

  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      (GtkSignalFunc) vadjustment_value_changed,
		      (gpointer) clist);

  gtk_widget_set_parent (clist->vscrollbar, GTK_WIDGET (clist));
  gtk_widget_show (clist->vscrollbar);

  clist->hscrollbar = gtk_hscrollbar_new (NULL);

  adjustment = gtk_range_get_adjustment (GTK_RANGE (clist->hscrollbar));

  gtk_signal_connect (GTK_OBJECT (adjustment), "changed",
		      (GtkSignalFunc) hadjustment_changed,
		      (gpointer) clist);

  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      (GtkSignalFunc) hadjustment_value_changed,
		      (gpointer) clist);

  gtk_widget_set_parent (clist->hscrollbar, GTK_WIDGET (clist));
  gtk_widget_show (clist->hscrollbar);
}

static void
adjust_scrollbars (GtkCList * clist)
{
  GTK_RANGE (clist->vscrollbar)->adjustment->page_size = clist->clist_window_height;
  GTK_RANGE (clist->vscrollbar)->adjustment->page_increment = clist->clist_window_height / 2;
  GTK_RANGE (clist->vscrollbar)->adjustment->step_increment = 10;
  GTK_RANGE (clist->vscrollbar)->adjustment->lower = 0;
  GTK_RANGE (clist->vscrollbar)->adjustment->upper = LIST_HEIGHT (clist);

  if (clist->clist_window_height - clist->voffset > LIST_HEIGHT (clist))
    {
      GTK_RANGE (clist->vscrollbar)->adjustment->value = MAX (0, LIST_HEIGHT (clist) - 
	clist->clist_window_height);
      gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->vscrollbar)->adjustment), 
			       "value_changed");
    }

  GTK_RANGE (clist->hscrollbar)->adjustment->page_size = clist->clist_window_width;
  GTK_RANGE (clist->hscrollbar)->adjustment->page_increment = clist->clist_window_width / 2;
  GTK_RANGE (clist->hscrollbar)->adjustment->step_increment = 10;
  GTK_RANGE (clist->hscrollbar)->adjustment->lower = 0;
  GTK_RANGE (clist->hscrollbar)->adjustment->upper = LIST_WIDTH (clist);

  if (clist->clist_window_width - clist->hoffset > LIST_WIDTH (clist))
    {
      GTK_RANGE (clist->hscrollbar)->adjustment->value = MAX (0, LIST_WIDTH (clist) - 
	clist->clist_window_width);
      gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->hscrollbar)->adjustment), 
			       "value_changed");
    }

  if (LIST_HEIGHT (clist) <= clist->clist_window_height &&
      clist->vscrollbar_policy == GTK_POLICY_AUTOMATIC)
    {
      if (GTK_WIDGET_VISIBLE (clist->vscrollbar))
	{
	  gtk_widget_hide (clist->vscrollbar);
	  gtk_widget_size_allocate (GTK_WIDGET (clist),
				    &GTK_WIDGET (clist)->allocation);
	}
    }
  else
    {
      if (!GTK_WIDGET_VISIBLE (clist->vscrollbar))
	{
	  gtk_widget_show (clist->vscrollbar);
	  gtk_widget_size_allocate (GTK_WIDGET (clist),
				    &GTK_WIDGET (clist)->allocation);
	}
    }

  if (LIST_WIDTH (clist) <= clist->clist_window_width &&
      clist->hscrollbar_policy == GTK_POLICY_AUTOMATIC)
    {
      if (GTK_WIDGET_VISIBLE (clist->hscrollbar))
	{
	  gtk_widget_hide (clist->hscrollbar);
	  gtk_widget_size_allocate (GTK_WIDGET (clist),
				    &GTK_WIDGET (clist)->allocation);
	}
    }
  else
    {
      if (!GTK_WIDGET_VISIBLE (clist->hscrollbar))
	{
	  gtk_widget_show (clist->hscrollbar);
	  gtk_widget_size_allocate (GTK_WIDGET (clist),
				    &GTK_WIDGET (clist)->allocation);
	}
    }

  gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->vscrollbar)->adjustment), "changed");
  gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (clist->hscrollbar)->adjustment), "changed");
}

static void
vadjustment_changed (GtkAdjustment * adjustment,
			       gpointer data)
{
  GtkCList *clist;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  clist = GTK_CLIST (data);
}

static void
hadjustment_changed (GtkAdjustment * adjustment,
			       gpointer data)
{
  GtkCList *clist;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  clist = GTK_CLIST (data);
}

static void
check_exposures (GtkCList *clist)
{
  GdkEvent *event;

  if (!GTK_WIDGET_REALIZED (clist))
    return;

  /* Make sure graphics expose events are processed before scrolling
   * again */
  while ((event = gdk_event_get_graphics_expose (clist->clist_window)) != NULL)
    {
      gtk_widget_event (GTK_WIDGET (clist), event);
      if (event->expose.count == 0)
	{
	  gdk_event_free (event);
	  break;
	}
      gdk_event_free (event);
    }
}

static void
vadjustment_value_changed (GtkAdjustment * adjustment,
				     gpointer data)
{
  GtkCList *clist;
  GdkRectangle area;
  gint diff, value;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (GTK_IS_CLIST (data));

  clist = GTK_CLIST (data);

  if (!GTK_WIDGET_DRAWABLE (clist))
    return;

  value = adjustment->value;

  if (adjustment == gtk_range_get_adjustment (GTK_RANGE (clist->vscrollbar)))
    {
      if (value > -clist->voffset)
	{
	  /* scroll down */
	  diff = value + clist->voffset;

	  /* we have to re-draw the whole screen here... */
	  if (diff >= clist->clist_window_height)
	    {
	      clist->voffset = -value;
	      draw_rows (clist, NULL);
	      return;
	    }

	  if ((diff != 0) && (diff != clist->clist_window_height))
	    gdk_window_copy_area (clist->clist_window,
				  clist->fg_gc,
				  0, 0,
				  clist->clist_window,
				  0,
				  diff,
				  clist->clist_window_width,
				  clist->clist_window_height - diff);

	  area.x = 0;
	  area.y = clist->clist_window_height - diff;
	  area.width = clist->clist_window_width;
	  area.height = diff;
	}
      else
	{
	  /* scroll up */
	  diff = -clist->voffset - value;

	  /* we have to re-draw the whole screen here... */
	  if (diff >= clist->clist_window_height)
	    {
	      clist->voffset = -value;
	      draw_rows (clist, NULL);
	      return;
	    }

	  if ((diff != 0) && (diff != clist->clist_window_height))
	    gdk_window_copy_area (clist->clist_window,
				  clist->fg_gc,
				  0, diff,
				  clist->clist_window,
				  0,
				  0,
				  clist->clist_window_width,
				  clist->clist_window_height - diff);

	  area.x = 0;
	  area.y = 0;
	  area.width = clist->clist_window_width;
	  area.height = diff;

	}

      clist->voffset = -value;
      if ((diff != 0) && (diff != clist->clist_window_height))
	check_exposures (clist);
    }

  draw_rows (clist, &area);
}

static void
hadjustment_value_changed (GtkAdjustment * adjustment,
				     gpointer data)
{
  GtkCList *clist;
  GdkRectangle area;
  gint i;
  gint y = 0;
  gint diff = 0;
  gint value;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (GTK_IS_CLIST (data));

  clist = GTK_CLIST (data);

  if (!GTK_WIDGET_DRAWABLE (clist) ||
      adjustment != gtk_range_get_adjustment (GTK_RANGE (clist->hscrollbar)))
    return;

  value = adjustment->value;
  
  /* move the column buttons and resize windows */
  for (i = 0; i < clist->columns; i++)
    {
      if (clist->column[i].button)
	{
	  clist->column[i].button->allocation.x -= value + clist->hoffset;
	  
	  if (clist->column[i].button->window)
	    {
	      gdk_window_move (clist->column[i].button->window,
			       clist->column[i].button->allocation.x,
			       clist->column[i].button->allocation.y);
	      
	      if (clist->column[i].window)
		gdk_window_move (clist->column[i].window,
				 clist->column[i].button->allocation.x +
				 clist->column[i].button->allocation.width - 
				 (DRAG_WIDTH / 2), 0); 
	    }
	}
    }

  if (value > -clist->hoffset)
    {
      /* scroll right */
      diff = value + clist->hoffset;
      
      clist->hoffset = -value;
      
      /* we have to re-draw the whole screen here... */
      if (diff >= clist->clist_window_width)
	{
	  draw_rows (clist, NULL);
	  return;
	}

      if (GTK_WIDGET_HAS_FOCUS (clist) && !GTK_CLIST_CHILD_HAS_FOCUS (clist) &&
	  GTK_CLIST_ADD_MODE (clist))
	{
	  y = ROW_TOP_YPIXEL (clist, clist->focus_row);
	      
	  gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE, 0, y,
			      clist->clist_window_width - 1,
			      clist->row_height - 1);
	}
      gdk_window_copy_area (clist->clist_window,
			    clist->fg_gc,
			    0, 0,
			    clist->clist_window,
			    diff,
			    0,
			    clist->clist_window_width - diff,
			    clist->clist_window_height);

      area.x = clist->clist_window_width - diff;
    }
  else
    {
      /* scroll left */
      if (!(diff = -clist->hoffset - value))
	return;

      clist->hoffset = -value;
      
      /* we have to re-draw the whole screen here... */
      if (diff >= clist->clist_window_width)
	{
	  draw_rows (clist, NULL);
	  return;
	}
      
      if (GTK_WIDGET_HAS_FOCUS (clist) && !GTK_CLIST_CHILD_HAS_FOCUS (clist) &&
	  GTK_CLIST_ADD_MODE (clist))
	{
	  y = ROW_TOP_YPIXEL (clist, clist->focus_row);
	  
	  gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE, 0, y,
			      clist->clist_window_width - 1,
			      clist->row_height - 1);
	}

      gdk_window_copy_area (clist->clist_window,
			    clist->fg_gc,
			    diff, 0,
			    clist->clist_window,
			    0,
			    0,
			    clist->clist_window_width - diff,
			    clist->clist_window_height);
	  
      area.x = 0;
    }

  area.y = 0;
  area.width = diff;
  area.height = clist->clist_window_height;

  check_exposures (clist);

  if (GTK_WIDGET_HAS_FOCUS (clist) && !GTK_CLIST_CHILD_HAS_FOCUS (clist))
    {
      if (GTK_CLIST_ADD_MODE (clist))
	{
	  gint focus_row;
	  
	  focus_row = clist->focus_row;
	  clist->focus_row = -1;
	  draw_rows (clist, &area);
	  clist->focus_row = focus_row;
	  
	  gdk_draw_rectangle (clist->clist_window, clist->xor_gc,
			      FALSE, 0, y, clist->clist_window_width - 1,
			      clist->row_height - 1);
	  return;
	}
      else
	{
	  gint x0;
	  gint x1;
	  
	  if (area.x == 0)
	    {
	      x0 = clist->clist_window_width - 1;
	      x1 = diff;
	    }
	  else
	    {
	      x0 = 0;
	      x1 = area.x - 1;
	    }
	  
	  y = ROW_TOP_YPIXEL (clist, clist->focus_row);
	  gdk_draw_line (clist->clist_window, clist->xor_gc,
			 x0, y + 1, x0, y + clist->row_height - 2);
	  gdk_draw_line (clist->clist_window, clist->xor_gc,
			 x1, y + 1, x1, y + clist->row_height - 2);
	  
	}
    }
  draw_rows (clist, &area);
}

/* 
 * Memory Allocation/Distruction Routines for GtkCList stuctures
 *
 * functions:
 *   columns_new
 *   column_title_new
 *   columns_delete
 *   row_new
 *   row_delete
 *   set_cell_contents 
 */
static GtkCListColumn *
columns_new (GtkCList * clist)
{
  gint i;
  GtkCListColumn *column;

  column = g_new (GtkCListColumn, clist->columns);

  for (i = 0; i < clist->columns; i++)
    {
      column[i].area.x = 0;
      column[i].area.y = 0;
      column[i].area.width = 0;
      column[i].area.height = 0;
      column[i].title = NULL;
      column[i].button = NULL;
      column[i].window = NULL;
      column[i].width = 0;
      column[i].width_set = FALSE;
      column[i].justification = GTK_JUSTIFY_LEFT;
    }

  return column;
}

static void
column_title_new (GtkCList * clist,
		  gint column,
		  gchar * title)
{
  if (clist->column[column].title)
    g_free (clist->column[column].title);

  clist->column[column].title = g_strdup (title);
}

static void
columns_delete (GtkCList * clist)
{
  gint i;

  for (i = 0; i < clist->columns; i++)
    if (clist->column[i].title)
      g_free (clist->column[i].title);
      
  g_free (clist->column);
}

static GtkCListRow *
row_new (GtkCList * clist)
{
  int i;
  GtkCListRow *clist_row;

  clist_row = g_chunk_new (GtkCListRow, clist->row_mem_chunk);
  clist_row->cell = g_chunk_new (GtkCell, clist->cell_mem_chunk);

  for (i = 0; i < clist->columns; i++)
    {
      clist_row->cell[i].type = GTK_CELL_EMPTY;
      clist_row->cell[i].vertical = 0;
      clist_row->cell[i].horizontal = 0;
    }

  clist_row->fg_set = FALSE;
  clist_row->bg_set = FALSE;
  clist_row->state = GTK_STATE_NORMAL;
  clist_row->data = NULL;
  clist_row->destroy = NULL;

  return clist_row;
}

static void
row_delete (GtkCList * clist,
	    GtkCListRow * clist_row)
{
  gint i;

  for (i = 0; i < clist->columns; i++)
    GTK_CLIST_CLASS_FW (clist)->set_cell_contents
      (clist, clist_row, i, GTK_CELL_EMPTY, NULL, 0, NULL, NULL);

  if (clist_row->destroy)
    clist_row->destroy (clist_row->data);

  g_mem_chunk_free (clist->cell_mem_chunk, clist_row->cell);
  g_mem_chunk_free (clist->row_mem_chunk, clist_row);
}

static void
set_cell_contents (GtkCList * clist,
		   GtkCListRow * clist_row,
		   gint column,
		   GtkCellType type,
		   gchar * text,
		   guint8 spacing,
		   GdkPixmap * pixmap,
		   GdkBitmap * mask)
{
  g_return_if_fail (clist_row != NULL);

  switch (clist_row->cell[column].type)
    {
    case GTK_CELL_EMPTY:
      break;
      
    case GTK_CELL_TEXT:
      g_free (GTK_CELL_TEXT (clist_row->cell[column])->text);
      break;
      
    case GTK_CELL_PIXMAP:
      gdk_pixmap_unref (GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap);
      if (GTK_CELL_PIXMAP (clist_row->cell[column])->mask)
	gdk_bitmap_unref (GTK_CELL_PIXMAP (clist_row->cell[column])->mask);
      break;
      
    case GTK_CELL_PIXTEXT:
      g_free (GTK_CELL_PIXTEXT (clist_row->cell[column])->text);
      gdk_pixmap_unref (GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap);
      if (GTK_CELL_PIXTEXT (clist_row->cell[column])->mask)
	gdk_bitmap_unref (GTK_CELL_PIXTEXT (clist_row->cell[column])->mask);
      break;

    case GTK_CELL_WIDGET:
      /* unimplimented */
      break;
      
    default:
      break;
    }

  clist_row->cell[column].type = GTK_CELL_EMPTY;

  switch (type)
    {
    case GTK_CELL_TEXT:
      if (text)
	{
	  clist_row->cell[column].type = GTK_CELL_TEXT;
	  GTK_CELL_TEXT (clist_row->cell[column])->text = g_strdup (text);
	}
      break;

    case GTK_CELL_PIXMAP:
      if (pixmap)
	{
	  clist_row->cell[column].type = GTK_CELL_PIXMAP;
	  GTK_CELL_PIXMAP (clist_row->cell[column])->pixmap = pixmap;
	  /* We set the mask even if it is NULL */
	  GTK_CELL_PIXMAP (clist_row->cell[column])->mask = mask;
	}
      break;

    case GTK_CELL_PIXTEXT:
      if (text && pixmap)
	{
	  clist_row->cell[column].type = GTK_CELL_PIXTEXT;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->text = g_strdup (text);
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->spacing = spacing;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->pixmap = pixmap;
	  GTK_CELL_PIXTEXT (clist_row->cell[column])->mask = mask;
	}
      break;

    default:
      break;
    }
}

/* Fill in data after widget has correct style */

static void 
add_style_data (GtkCList * clist)
{
  GtkWidget *widget;

  widget = GTK_WIDGET(clist);

  /* text properties */
  if (!GTK_CLIST_ROW_HEIGHT_SET (clist))
    {
      clist->row_height = widget->style->font->ascent + widget->style->font->descent + 1;
      clist->row_center_offset = widget->style->font->ascent + 1.5;
    }
  else
    {
      gint text_height;
      text_height = clist->row_height - (GTK_WIDGET (clist)->style->font->ascent +
		          GTK_WIDGET (clist) ->style->font->descent + 1);
      clist->row_center_offset = (text_height / 2) + GTK_WIDGET (clist)->style->font->ascent + 1.5;
    }

  /* Column widths */
}


/* focus functions */

static void
gtk_clist_draw_focus (GtkWidget *widget)
{
  GtkCList *clist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_CLIST (widget));

  if (!GTK_WIDGET_DRAWABLE (widget))
    return;

  clist = GTK_CLIST (widget);
  if (clist->focus_row >= 0)
    gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			0, ROW_TOP_YPIXEL(clist, clist->focus_row),
			clist->clist_window_width - 1,
			clist->row_height - 1);
}

static void
gtk_clist_set_focus_child (GtkContainer *container,
			   GtkWidget    *child)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_CLIST (container));

  if (child)
    {
      g_return_if_fail (GTK_IS_WIDGET (child));
      GTK_CLIST_SET_FLAG (GTK_CLIST (container), CLIST_CHILD_HAS_FOCUS);
    }

  parent_class->set_focus_child (container, child);
}

static gint
gtk_clist_focus_in (GtkWidget     *widget,
		    GdkEventFocus *event)
{
  GtkCList *clist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  GTK_CLIST_UNSET_FLAG (widget, CLIST_CHILD_HAS_FOCUS);

  clist = GTK_CLIST (widget);

  if (clist->selection_mode == GTK_SELECTION_BROWSE &&
      clist->selection == NULL && clist->focus_row > -1)
    select_row (clist, clist->focus_row, -1, (GdkEvent *) event);
  else
    gtk_widget_draw_focus (widget);

  return FALSE;
}

static gint
gtk_clist_focus_out (GtkWidget     *widget,
		     GdkEventFocus *event)
{
  GtkCList *clist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);
  
  clist = GTK_CLIST (widget);

  if (clist->anchor != -1 && clist->selection_mode == GTK_SELECTION_EXTENDED)
    GTK_CLIST_CLASS_FW (widget)->resync_selection (clist, (GdkEvent *) event);

  return FALSE;
}

static void
toggle_add_mode (GtkCList *clist)
{
  g_return_if_fail (clist != 0);
  g_return_if_fail (GTK_IS_CLIST (clist));
  
  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist)) ||
      clist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  gtk_clist_draw_focus (GTK_WIDGET (clist));
  if (!GTK_CLIST_ADD_MODE (clist))
    {
      GTK_CLIST_SET_FLAG (clist, CLIST_ADD_MODE);
      gdk_gc_set_line_attributes (clist->xor_gc, 1,
				  GDK_LINE_ON_OFF_DASH, 0, 0);
      gdk_gc_set_dashes (clist->xor_gc, 0, "\4\4", 2);
    }
  else
    {
      GTK_CLIST_UNSET_FLAG (clist, CLIST_ADD_MODE);
      gdk_gc_set_line_attributes (clist->xor_gc, 1, GDK_LINE_SOLID, 0, 0);
      clist->anchor_state = GTK_STATE_SELECTED;
    }
  gtk_clist_draw_focus (GTK_WIDGET (clist));
}

static void
toggle_focus_row (GtkCList *clist)
{
  g_return_if_fail (clist != 0);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist)) ||
      clist->focus_row < 0 || clist->focus_row >= clist->rows)
    return;

  switch (clist->selection_mode)
    {
    case  GTK_SELECTION_SINGLE:
    case  GTK_SELECTION_MULTIPLE:
      
      toggle_row (clist, clist->focus_row, 0, NULL);
      break;
      
    case GTK_SELECTION_EXTENDED:
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;

      clist->anchor = clist->focus_row;
      clist->drag_pos = clist->focus_row;
      clist->undo_anchor = clist->focus_row;
      
      if (GTK_CLIST_ADD_MODE (clist))
	fake_toggle_row (clist, clist->focus_row);
      else
	GTK_CLIST_CLASS_FW (clist)->fake_unselect_all (clist,clist->focus_row);

      GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      break;
      
    default:
      break;
    }
}

static void
move_focus_row (GtkCList      *clist,
		GtkScrollType  scroll_type,
		gfloat         position)
{
  GtkWidget *widget;

  g_return_if_fail (clist != 0);
  g_return_if_fail (GTK_IS_CLIST (clist));

  widget = GTK_WIDGET (clist);

  switch (scroll_type)
    {
    case GTK_SCROLL_STEP_BACKWARD:
      if (clist->focus_row <= 0)
	return;
      gtk_clist_draw_focus (widget);
      clist->focus_row--;
      gtk_clist_draw_focus (widget);
      break;
    case GTK_SCROLL_STEP_FORWARD:
      if (clist->focus_row >= clist->rows - 1)
	return;
      gtk_clist_draw_focus (widget);
      clist->focus_row++;
      gtk_clist_draw_focus (widget);
      break;
    case GTK_SCROLL_PAGE_BACKWARD:
      if (clist->focus_row <= 0)
	return;
      gtk_clist_draw_focus (widget);
      clist->focus_row = MAX (0, clist->focus_row -
			      (2 * clist->clist_window_height -
			       clist->row_height - CELL_SPACING) / 
			      (2 * (clist->row_height + CELL_SPACING)));
      gtk_clist_draw_focus (widget);
      break;
    case GTK_SCROLL_PAGE_FORWARD:
      if (clist->focus_row >= clist->rows - 1)
	return;
      gtk_clist_draw_focus (widget);
      clist->focus_row = MIN (clist->rows - 1, clist->focus_row + 
			      (2 * clist->clist_window_height -
			       clist->row_height - CELL_SPACING) / 
			      (2 * (clist->row_height + CELL_SPACING)));
      gtk_clist_draw_focus (widget);
      break;
    case GTK_SCROLL_JUMP:
      if (position >= 0 && position <= 1)
	{
	  gtk_clist_draw_focus (widget);
	  clist->focus_row = position * (clist->rows - 1);
	  gtk_clist_draw_focus (widget);
	}
      break;
    default:
      break;
    }
}

static void
scroll_horizontal (GtkCList      *clist,
		   GtkScrollType  scroll_type,
		   gfloat         position)
{
  gint column = 0;

  g_return_if_fail (clist != 0);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist))
    return;

  switch (scroll_type)
    {
    case GTK_SCROLL_STEP_BACKWARD:
      column = COLUMN_FROM_XPIXEL (clist, 0);
      if (COLUMN_LEFT_XPIXEL (clist, column) - CELL_SPACING - COLUMN_INSET >= 0
	  && column > 0)
	column--;
      break;
    case GTK_SCROLL_STEP_FORWARD:
      column =  COLUMN_FROM_XPIXEL (clist, clist->clist_window_width);
      if (column < 0)
	return;
      if (COLUMN_LEFT_XPIXEL (clist, column) + clist->column[column].area.width
	  + CELL_SPACING + COLUMN_INSET - 1 <= clist->clist_window_width &&
	  column < clist->columns - 1)
	column++;
      break;
    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_FORWARD:
      return;
    case GTK_SCROLL_JUMP:
      if (position >= 0 && position <= 1)
	column = position * (clist->columns - 1);
      else
	return;
      break;
    default:
      break;
    }

  if (COLUMN_LEFT_XPIXEL (clist, column) < CELL_SPACING + COLUMN_INSET)
    gtk_clist_moveto (clist, -1, column, 0, 0);
  else if (COLUMN_LEFT_XPIXEL (clist, column) + CELL_SPACING + COLUMN_INSET - 1
	   + clist->column[column].area.width > clist->clist_window_width)
    {
      if (column == clist->columns - 1)
	gtk_clist_moveto (clist, -1, column, 0, 0);
      else
	gtk_clist_moveto (clist, -1, column, 0, 1);
    }
}

static void
scroll_vertical (GtkCList      *clist,
		 GtkScrollType  scroll_type,
		 gfloat         position)
{
  gint old_focus_row;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist))
    return;

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_EXTENDED:
      if (clist->anchor >= 0)
	return;

    case GTK_SELECTION_BROWSE:

      old_focus_row = clist->focus_row;
      move_focus_row (clist, scroll_type, position);

      if (old_focus_row != clist->focus_row)
	{
	  if (clist->selection_mode == GTK_SELECTION_BROWSE)
	    unselect_row (clist,old_focus_row, -1, NULL);
	  else if (!GTK_CLIST_ADD_MODE (clist))
	    {
	      gtk_clist_unselect_all (clist);
	      clist->undo_anchor = old_focus_row;
	    }
	}

      switch (gtk_clist_row_is_visible (clist, clist->focus_row))
	{
	case GTK_VISIBILITY_NONE:
	  if (old_focus_row != clist->focus_row &&
	      !(clist->selection_mode == GTK_SELECTION_EXTENDED &&
		GTK_CLIST_ADD_MODE (clist)))
	    select_row (clist, clist->focus_row, -1, NULL);
	  switch (scroll_type)
	    {
	    case GTK_SCROLL_STEP_BACKWARD:
	    case GTK_SCROLL_PAGE_BACKWARD:
	      gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);
	      break;
	    case GTK_SCROLL_STEP_FORWARD:
	    case GTK_SCROLL_PAGE_FORWARD:
	      gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);
	      break;
	    case GTK_SCROLL_JUMP:
	      gtk_clist_moveto (clist, clist->focus_row, -1, 0.5, 0);
	      break;
	    default:
	      break;
	    }
	  break;

	case GTK_VISIBILITY_PARTIAL:
	  switch (scroll_type)
	    {
	    case GTK_SCROLL_STEP_BACKWARD:
	    case GTK_SCROLL_PAGE_BACKWARD:
	      gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);
	      break;
	    case GTK_SCROLL_STEP_FORWARD:
	    case GTK_SCROLL_PAGE_FORWARD:
	      gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);
	      break;
	    case GTK_SCROLL_JUMP:
	      gtk_clist_moveto (clist, clist->focus_row, -1, 0.5, 0);
	      break;
	    default:
	      break;
	    }

	default:
	  if (old_focus_row != clist->focus_row &&
	      !(clist->selection_mode == GTK_SELECTION_EXTENDED &&
		GTK_CLIST_ADD_MODE (clist)))
	    select_row (clist, clist->focus_row, -1, NULL);
	  break;
	}
      break;

    default:
      move_focus_row (clist, scroll_type, position);

      if (ROW_TOP_YPIXEL (clist, clist->focus_row) + clist->row_height >
	  clist->clist_window_height)
	gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);
      else if (ROW_TOP_YPIXEL (clist, clist->focus_row) < 0)
	gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);
      break;
    }
}

static void
set_anchor (GtkCList *clist,
	    gboolean  add_mode,
	    gint      anchor,
	    gint      undo_anchor)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));
  
  if (clist->selection_mode != GTK_SELECTION_EXTENDED || clist->anchor >= 0)
    return;

  g_list_free (clist->undo_selection);
  g_list_free (clist->undo_unselection);
  clist->undo_selection = NULL;
  clist->undo_unselection = NULL;

  if (add_mode)
    fake_toggle_row (clist, anchor);
  else
    {
      GTK_CLIST_CLASS_FW (clist)->fake_unselect_all (clist, anchor);
      clist->anchor_state = GTK_STATE_SELECTED;
    }

  clist->anchor = anchor;
  clist->drag_pos = anchor;
  clist->undo_anchor = undo_anchor;
}

static void
resync_selection (GtkCList *clist,
		  GdkEvent *event)
{
  gint i;
  gint e;
  gint row;
  gboolean thaw = FALSE;
  GList *list;
  GtkCListRow *clist_row;

  if (clist->anchor < 0)
    return;

  if (!GTK_CLIST_FROZEN (clist))
    {
      GTK_CLIST_SET_FLAG (clist, CLIST_FROZEN);
      thaw = TRUE;
    }

  i = MIN (clist->anchor, clist->drag_pos);
  e = MAX (clist->anchor, clist->drag_pos);

  if (clist->undo_selection)
    {

      list = clist->selection;
      clist->selection = clist->undo_selection;
      clist->selection_end = g_list_last (clist->selection);
      clist->undo_selection = list;
      list = clist->selection;
      while (list)
	{
	  row = GPOINTER_TO_INT (list->data);
	  list = list->next;
	  if (row < i || row > e)
	    {
	      clist_row = g_list_nth (clist->row_list, row)->data;
	      clist_row->state = GTK_STATE_SELECTED;
	      unselect_row (clist, row, -1, event);
	      clist->undo_selection = g_list_prepend
		(clist->undo_selection, GINT_TO_POINTER (row));
	    }
	}
    }    

  for (list = g_list_nth (clist->row_list, i); i <= e; i++, list = list->next)
    if (g_list_find (clist->selection, GINT_TO_POINTER(i)))
      {
	if (GTK_CLIST_ROW (list)->state == GTK_STATE_NORMAL)
	  {
	    GTK_CLIST_ROW (list)->state = GTK_STATE_SELECTED;
	    unselect_row (clist, i, -1, event);
	    clist->undo_selection = g_list_prepend (clist->undo_selection,
						    GINT_TO_POINTER (i));
	  }
      }
    else if (GTK_CLIST_ROW (list)->state == GTK_STATE_SELECTED)
      {
	GTK_CLIST_ROW (list)->state = GTK_STATE_NORMAL;
	clist->undo_unselection = g_list_prepend (clist->undo_unselection,
						  GINT_TO_POINTER (i));
      }

  for (list = clist->undo_unselection; list; list = list->next)
    gtk_signal_emit (GTK_OBJECT (clist), clist_signals[SELECT_ROW],
		     GPOINTER_TO_INT (list->data), -1, event);

  clist->anchor = -1;
  clist->drag_pos = -1;

  if (thaw)
    GTK_CLIST_UNSET_FLAG (clist, CLIST_FROZEN);
}

static void
update_extended_selection (GtkCList *clist,
			   gint      row)
{
  gint i;
  GList *list;
  GdkRectangle area;
  gint s1 = -1;
  gint s2 = -1;
  gint e1 = -1;
  gint e2 = -1;
  gint y1 = clist->clist_window_height;
  gint y2 = clist->clist_window_height;
  gint h1 = 0;
  gint h2 = 0;
  gint top;

  if (clist->selection_mode != GTK_SELECTION_EXTENDED || clist->anchor == -1)
    return;

  if (row < 0)
    row = 0;
  if (row >= clist->rows)
    row = clist->rows - 1;

  /* extending downwards */
  if (row > clist->drag_pos && clist->anchor <= clist->drag_pos)
    {
      s2 = clist->drag_pos + 1;
      e2 = row;
    }
  /* extending upwards */
  else if (row < clist->drag_pos && clist->anchor >= clist->drag_pos)
    {
      s2 = row;
      e2 = clist->drag_pos - 1;
    }
  else if (row < clist->drag_pos && clist->anchor < clist->drag_pos)
    {
      e1 = clist->drag_pos;
      /* row and drag_pos on different sides of anchor :
	 take back the selection between anchor and drag_pos,
         select between anchor and row */
      if (row < clist->anchor)
	{
	  s1 = clist->anchor + 1;
	  s2 = row;
	  e2 = clist->anchor - 1;
	}
      /* take back the selection between anchor and drag_pos */
      else
	s1 = row + 1;
    }
  else if (row > clist->drag_pos && clist->anchor > clist->drag_pos)
    {
      s1 = clist->drag_pos;
      /* row and drag_pos on different sides of anchor :
	 take back the selection between anchor and drag_pos,
         select between anchor and row */
      if (row > clist->anchor)
	{
	  e1 = clist->anchor - 1;
	  s2 = clist->anchor + 1;
	  e2 = row;
	}
      /* take back the selection between anchor and drag_pos */
      else
	e1 = row - 1;
    }

  clist->drag_pos = row;

  area.x = 0;
  area.width = clist->clist_window_width;

  /* restore the elements between s1 and e1 */
  if (s1 >= 0)
    {
      for (i = s1, list = g_list_nth (clist->row_list, i); i <= e1;
	   i++, list = list->next)
	{
	  if (GTK_CLIST_CLASS_FW (clist)->selection_find (clist, i, list))
            GTK_CLIST_ROW (list)->state = GTK_STATE_SELECTED;
          else
            GTK_CLIST_ROW (list)->state = GTK_STATE_NORMAL;
	}

      top = ROW_TOP_YPIXEL (clist, clist->focus_row);

      if (top + clist->row_height <= 0)
	{
	  area.y = 0;
	  area.height = ROW_TOP_YPIXEL (clist, e1) + clist->row_height;
	  draw_rows (clist, &area);
	  gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);
	}
      else if (top >= clist->clist_window_height)
	{
	  area.y = ROW_TOP_YPIXEL (clist, s1);
	  area.height = clist->clist_window_height - area.y;
	  draw_rows (clist, &area);
	  gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);
	}
      else if (top < 0)
	gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);
      else if (top + clist->row_height > clist->clist_window_height)
	gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);

      y1 = ROW_TOP_YPIXEL (clist, s1);
      h1 = (e1 - s1 + 1) * (clist->row_height + CELL_SPACING);
    }

  /* extend the selection between s2 and e2 */
  if (s2 >= 0)
    {
      for (i = s2, list = g_list_nth (clist->row_list, i); i <= e2;
	   i++, list = list->next)
	if (GTK_CLIST_ROW (list)->state != clist->anchor_state)
	  GTK_CLIST_ROW (list)->state = clist->anchor_state;

      top = ROW_TOP_YPIXEL (clist, clist->focus_row);

      if (top + clist->row_height <= 0)
	{
	  area.y = 0;
	  area.height = ROW_TOP_YPIXEL (clist, e2) + clist->row_height;
	  draw_rows (clist, &area);
	  gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);
	}
      else if (top >= clist->clist_window_height)
	{
	  area.y = ROW_TOP_YPIXEL (clist, s2);
	  area.height = clist->clist_window_height - area.y;
	  draw_rows (clist, &area);
	  gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);
	}
      else if (top < 0)
	gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);
      else if (top + clist->row_height > clist->clist_window_height)
	gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);

      y2 = ROW_TOP_YPIXEL (clist, s2);
      h2 = (e2-s2+1) * (clist->row_height + CELL_SPACING);
    }

  area.y = MAX (0, MIN (y1, y2));
  if (area.y > clist->clist_window_height)
    area.y = 0;
  area.height = MIN (clist->clist_window_height, h1 + h2);
  if (s1 >= 0 && s2 >= 0)
    area.height += (clist->row_height + CELL_SPACING);
  draw_rows (clist, &area);
}

static void
start_selection (GtkCList *clist)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist))
    return;

  set_anchor (clist, GTK_CLIST_ADD_MODE (clist), clist->focus_row,
	      clist->focus_row);
}

static void
end_selection (GtkCList *clist)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_FOCUS (clist)) ||
      clist->anchor == -1)
    return;
  
  GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
}

static void
extend_selection (GtkCList      *clist,
		  GtkScrollType  scroll_type,
		  gfloat         position,
		  gboolean       auto_start_selection)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist)) ||
      clist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  if (auto_start_selection)
    set_anchor (clist, GTK_CLIST_ADD_MODE (clist), clist->focus_row,
		clist->focus_row);
  else if (clist->anchor == -1)
    return;

  move_focus_row (clist, scroll_type, position);

  if (ROW_TOP_YPIXEL (clist, clist->focus_row) + clist->row_height >
      clist->clist_window_height)
    gtk_clist_moveto (clist, clist->focus_row, -1, 1, 0);
  else if (ROW_TOP_YPIXEL (clist, clist->focus_row) < 0)
    gtk_clist_moveto (clist, clist->focus_row, -1, 0, 0);

  update_extended_selection (clist, clist->focus_row);
}

static void
abort_column_resize (GtkCList *clist)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (!GTK_CLIST_IN_DRAG (clist))
    return;

  GTK_CLIST_UNSET_FLAG (clist, CLIST_IN_DRAG);
  gtk_grab_remove (GTK_WIDGET (clist));
  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  clist->drag_pos = -1;

  if (clist->x_drag >= 0 && clist->x_drag <= clist->clist_window_width - 1)
    draw_xor_line (clist);

  if (GTK_CLIST_ADD_MODE (clist))
    {
      gdk_gc_set_line_attributes (clist->xor_gc, 1, GDK_LINE_ON_OFF_DASH, 0,0);
      gdk_gc_set_dashes (clist->xor_gc, 0, "\4\4", 2);
    }
}

static gint
gtk_clist_key_press (GtkWidget   * widget,
		     GdkEventKey * event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_CLASS (parent_class)->key_press_event &&
      GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event))
    return TRUE;

  switch (event->keyval)
    {
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
      if (event->state & GDK_SHIFT_MASK)
	return gtk_container_focus (GTK_CONTAINER (widget),
				    GTK_DIR_TAB_BACKWARD);
      else
	return gtk_container_focus (GTK_CONTAINER (widget),
				    GTK_DIR_TAB_FORWARD);

    default:
      break;
    }
  
  return FALSE;
}

static gboolean
title_focus (GtkCList * clist,
	     gint       dir)
{
  GtkWidget *focus_child;
  gboolean return_val = FALSE;
  gint d = 1;
  gint i = 0;
  gint j;

  if (!GTK_CLIST_SHOW_TITLES (clist))
    return FALSE;

  focus_child = GTK_CONTAINER (clist)->focus_child;
  
  switch (dir)
    {
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_UP:
      if (!focus_child || focus_child == clist->hscrollbar ||
	  focus_child == clist->hscrollbar ||
	  !GTK_CLIST_CHILD_HAS_FOCUS (clist))
	{
	  if (dir == GTK_DIR_UP)
	    i = COLUMN_FROM_XPIXEL (clist, 0);
	  else
	    i = clist->columns - 1;
	  focus_child = clist->column[i].button;
	  dir = GTK_DIR_TAB_FORWARD;
	}
      else
	d = -1;
      break;
    case GTK_DIR_LEFT:
      d = -1;
      if (!focus_child || focus_child == clist->hscrollbar ||
	  focus_child == clist->hscrollbar)
	{
	  i = clist->columns - 1;
	  focus_child = clist->column[i].button;
	}
      break;
    case GTK_DIR_RIGHT:
      if (!focus_child || focus_child == clist->hscrollbar ||
	  focus_child == clist->hscrollbar)
	{
	  i = 0;
	  focus_child = clist->column[i].button;
	}
      break;
    }

  if (focus_child)
    while (i < clist->columns)
      {
	if (clist->column[i].button == focus_child)
	  {
	    if (clist->column[i].button && 
		GTK_WIDGET_VISIBLE (clist->column[i].button) &&
		GTK_IS_CONTAINER (clist->column[i].button) &&
		!GTK_WIDGET_HAS_FOCUS (clist->column[i].button))
	      if (gtk_container_focus 
		  (GTK_CONTAINER (clist->column[i].button), dir))
		{
		  return_val = TRUE;
		  i -= d;
		}
	    if (!return_val && dir == GTK_DIR_UP)
	      return FALSE;
	    i += d;
	    break;
	  }
	i++;
      }

  j = i;

  if (!return_val)
    while (j >= 0 && j < clist->columns)
      {
	if (clist->column[j].button &&
	    GTK_WIDGET_VISIBLE (clist->column[j].button))
	  {
	    if (GTK_IS_CONTAINER (clist->column[j].button) &&
		gtk_container_focus 
		(GTK_CONTAINER (clist->column[j].button), dir))
	      {
		return_val = TRUE;
		break;
	      }
	    else if (GTK_WIDGET_CAN_FOCUS (clist->column[j].button))
	      {
		gtk_widget_grab_focus (clist->column[j].button);
		return_val = TRUE;
		break;
	      }
	  }
	j += d;
      }
  
  if (return_val)
    {
      if (COLUMN_LEFT_XPIXEL (clist, j) < CELL_SPACING + COLUMN_INSET)
	gtk_clist_moveto (clist, -1, j, 0, 0);
      else if (COLUMN_LEFT_XPIXEL(clist, j) + clist->column[j].area.width >
	       clist->clist_window_width)
	{
	  if (j == clist->columns-1)
	    gtk_clist_moveto (clist, -1, j, 0, 0);
	  else
	    gtk_clist_moveto (clist, -1, j, 0, 1);
	}
    }
  return return_val;
}

static gint
gtk_clist_focus (GtkContainer     * container,
		 GtkDirectionType   direction)
{
  GtkCList *clist;
  GtkWidget *focus_child;
  gint old_row;

  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_CLIST (container), FALSE);

  if (!GTK_WIDGET_SENSITIVE (container))
    return FALSE;
  
  clist = GTK_CLIST (container);
  focus_child = container->focus_child;
  old_row = clist->focus_row;

  switch (direction)
    {
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      if (GTK_CLIST_CHILD_HAS_FOCUS (clist) &&
	  (!focus_child || (focus_child && focus_child != clist->vscrollbar && 
			    focus_child != clist->hscrollbar)))
	{
	  if (title_focus (clist, direction))
	    return TRUE;
	  gtk_container_set_focus_child (container, NULL);
	  return FALSE;
	 }
      gtk_widget_grab_focus (GTK_WIDGET (container));
      return TRUE;
    case GTK_DIR_DOWN:
    case GTK_DIR_TAB_FORWARD:
      if (GTK_CLIST_CHILD_HAS_FOCUS (clist) && 
	  (!focus_child || (focus_child != clist->vscrollbar &&
			    focus_child != clist->hscrollbar))) 
	{
	  gboolean tf = FALSE;

	  if (((focus_child && direction == GTK_DIR_DOWN) ||
	       !(tf = title_focus (clist, GTK_DIR_TAB_FORWARD)))
	      && clist->rows)
	    {
	      if (clist->focus_row < 0)
		{
		  clist->focus_row = 0;

		  if ((clist->selection_mode == GTK_SELECTION_BROWSE ||
		       clist->selection_mode == GTK_SELECTION_EXTENDED) &&
		      !clist->selection)
		    select_row (clist, clist->focus_row, -1, NULL);
		}
	      gtk_widget_grab_focus (GTK_WIDGET (container));
	      return TRUE;
	    }

	  if (tf)
	    return TRUE;
	}
      
      GTK_CLIST_SET_FLAG (clist, CLIST_CHILD_HAS_FOCUS);

      if ((!GTK_CLIST_CHILD_HAS_FOCUS (clist) || !focus_child ||
	   (focus_child != clist->vscrollbar &&
	    focus_child != clist->hscrollbar)) &&
	  GTK_WIDGET_VISIBLE (clist->vscrollbar) &&
	  GTK_WIDGET_CAN_FOCUS (clist->vscrollbar))
	{
	  gtk_widget_grab_focus (clist->vscrollbar);
	  return TRUE;
	}

      if ((!GTK_CLIST_CHILD_HAS_FOCUS (clist) || !focus_child || 
	   focus_child != clist->hscrollbar) &&
	  GTK_WIDGET_VISIBLE (clist->hscrollbar) &&
	  GTK_WIDGET_CAN_FOCUS (clist->hscrollbar))
	{
	  gtk_widget_grab_focus (clist->hscrollbar);
	  return TRUE;
	}
      break;
    case GTK_DIR_UP:
    case GTK_DIR_TAB_BACKWARD:
      if (!focus_child && GTK_CLIST_CHILD_HAS_FOCUS (clist) &&
	  GTK_WIDGET_VISIBLE (clist->hscrollbar) &&
	  GTK_WIDGET_CAN_FOCUS (clist->hscrollbar))
	{
	  gtk_widget_grab_focus (clist->hscrollbar);
	  return TRUE;
	}
	
      if ((!focus_child || focus_child == clist->hscrollbar) &&
	  GTK_CLIST_CHILD_HAS_FOCUS (clist) &&
	  GTK_WIDGET_VISIBLE (clist->vscrollbar) &&
	  GTK_WIDGET_CAN_FOCUS (clist->vscrollbar))
	{
	  gtk_widget_grab_focus (clist->vscrollbar);
	  return TRUE;
	}

      if ((!focus_child || focus_child == clist->hscrollbar ||
	   focus_child == clist->vscrollbar) &&
	  GTK_CLIST_CHILD_HAS_FOCUS (clist) && clist->rows)
	{
	  if (clist->focus_row < 0)
	    {
	      clist->focus_row = 0;
	      if ((clist->selection_mode == GTK_SELECTION_BROWSE ||
		   clist->selection_mode == GTK_SELECTION_EXTENDED) &&
		  !clist->selection)
		select_row (clist, clist->focus_row, -1, NULL);
	    }
	  gtk_widget_grab_focus (GTK_WIDGET (container));
	  return TRUE;
	}

      GTK_CLIST_SET_FLAG (clist, CLIST_CHILD_HAS_FOCUS);

      if (title_focus (clist, direction))
	return TRUE;

      break;

    default:
      break;
    }

  gtk_container_set_focus_child (container, NULL);
  return FALSE;
}

void
gtk_clist_unselect_all (GtkCList * clist)
{
  GTK_CLIST_CLASS_FW (clist)->unselect_all (clist);
}

static void
real_unselect_all (GtkCList * clist)
{
  GList *list;
  gint i;
 
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist))
    return;

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_BROWSE:
      if (clist->focus_row >= 0)
	{
	  select_row (clist, clist->focus_row, -1, NULL);
	  return;
	}
      break;

    case GTK_SELECTION_EXTENDED:
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;

      clist->anchor = -1;
      clist->drag_pos = -1;
      clist->undo_anchor = clist->focus_row;
      break;

    default:
      break;
    }

  list = clist->selection;

  while (list)
    {
      i = GPOINTER_TO_INT (list->data);
      list = list->next;
      unselect_row (clist, i, -1, NULL);
    }
}

void
gtk_clist_select_all (GtkCList * clist)
{
  GTK_CLIST_CLASS_FW (clist)->select_all (clist);
}

static void
real_select_all (GtkCList * clist)
{
  GList *list;
  gint i;
 
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist))
    return;

  switch (clist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:
      return;

    case GTK_SELECTION_EXTENDED:
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
	  
      if (clist->rows &&
	  ((GtkCListRow *) (clist->row_list->data))->state !=
	  GTK_STATE_SELECTED)
	fake_toggle_row (clist, 0);

      clist->anchor_state =  GTK_STATE_SELECTED;
      clist->anchor = 0;
      clist->drag_pos = 0;
      clist->undo_anchor = clist->focus_row;
      update_extended_selection (clist, clist->rows);
      GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      return;

    case GTK_SELECTION_MULTIPLE:
      for (i = 0, list = clist->row_list; list; i++, list = list->next)
	{
	  if (((GtkCListRow *)(list->data))->state == GTK_STATE_NORMAL)
	    gtk_signal_emit (GTK_OBJECT (clist), clist_signals[SELECT_ROW],
			     i, -1, NULL);
	}
      return;
    }
}

static void
fake_unselect_all (GtkCList * clist,
		   gint       row)
{
  GList *list;
  GList *work;
  gint i;

  if (row >= 0 && (work = g_list_nth (clist->row_list, row)))
    {
      if (GTK_CLIST_ROW (work)->state == GTK_STATE_NORMAL)
	{
	  GTK_CLIST_ROW (work)->state = GTK_STATE_SELECTED;
	  
	  if (!GTK_CLIST_FROZEN (clist) &&
	      gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
	    GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row,
						  GTK_CLIST_ROW (work));
	}  
    }

  clist->undo_selection = clist->selection;
  clist->selection = NULL;
  clist->selection_end = NULL;
  
  for (list = clist->undo_selection; list; list = list->next)
    {
      if ((i = GPOINTER_TO_INT (list->data)) == row ||
	  !(work = g_list_nth (clist->row_list, i)))
	continue;

      GTK_CLIST_ROW (work)->state = GTK_STATE_NORMAL;
      if (!GTK_CLIST_FROZEN (clist) &&
	  gtk_clist_row_is_visible (clist, i) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, i,
					      GTK_CLIST_ROW (work));
    }
}

static void
fake_toggle_row (GtkCList *clist,
		 gint      row)
{
  GList *work;

  if (!(work = g_list_nth (clist->row_list, row)))
    return;
  
  if (GTK_CLIST_ROW (work)->state == GTK_STATE_NORMAL)
    clist->anchor_state = GTK_CLIST_ROW (work)->state = GTK_STATE_SELECTED;
  else
    clist->anchor_state = GTK_CLIST_ROW (work)->state = GTK_STATE_NORMAL;
  
  if (!GTK_CLIST_FROZEN (clist) &&
      gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
    GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row,
					  GTK_CLIST_ROW (work));
}

static GList *
selection_find (GtkCList *clist,
		gint      row_number,
		GList    *row_list_element)
{
  return g_list_find (clist->selection, GINT_TO_POINTER (row_number));
}

static gint
default_compare (GtkCList     *clist,
		 gconstpointer ptr1,
		 gconstpointer ptr2)
{
  char *text1 = NULL;
  char *text2 = NULL;

  GtkCListRow *row1 = (GtkCListRow *) ptr1;
  GtkCListRow *row2 = (GtkCListRow *) ptr2;

  switch (row1->cell[clist->sort_column].type)
    {
    case GTK_CELL_TEXT:
      text1 = GTK_CELL_TEXT (row1->cell[clist->sort_column])->text;
      break;
    case GTK_CELL_PIXTEXT:
      text1 = GTK_CELL_PIXTEXT (row1->cell[clist->sort_column])->text;
      break;
    default:
      break;
    }
 
  switch (row2->cell[clist->sort_column].type)
    {
    case GTK_CELL_TEXT:
      text2 = GTK_CELL_TEXT (row2->cell[clist->sort_column])->text;
      break;
    case GTK_CELL_PIXTEXT:
      text2 = GTK_CELL_PIXTEXT (row2->cell[clist->sort_column])->text;
      break;
    default:
      break;
    }

  if (!text2)
    return (text1 != NULL);

  if (!text1)
    return -1;

  return strcmp (text1, text2);
}

void
gtk_clist_set_compare_func (GtkCList            *clist,
			    GtkCListCompareFunc  cmp_func)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  clist->compare = (cmp_func) ? cmp_func : default_compare;
}

void       
gtk_clist_set_auto_sort (GtkCList *clist,
			 gboolean  auto_sort)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));
  
  if (GTK_CLIST_AUTO_SORT (clist) && !auto_sort)
    GTK_CLIST_UNSET_FLAG (clist, CLIST_AUTO_SORT);
  else if (!GTK_CLIST_AUTO_SORT (clist) && auto_sort)
    {
      GTK_CLIST_SET_FLAG (clist, CLIST_AUTO_SORT);
      gtk_clist_sort (clist);
    }
}

void       
gtk_clist_set_sort_type (GtkCList   *clist,
			 GtkSortType sort_type)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));
  
  clist->sort_type = sort_type;
}

void
gtk_clist_set_sort_column (GtkCList *clist,
			   gint      column)
{
  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (column < 0 || column >= clist->columns)
    return;

  clist->sort_column = column;
}

static GList *
gtk_clist_merge (GtkCList *clist,
		 GList    *a,         /* first list to merge */
		 GList    *b)         /* second list to merge */
{
  GList z = { 0 };                    /* auxiliary node */
  GList *c;
  gint cmp;

  c = &z;

  while (a || b)
    {
      if (a && !b)
	{
	  c->next = a;
	  a->prev = c;
	  c = a;
	  a = a->next;
	  break;
	}
      else if (!a && b)
	{
	  c->next = b;
	  b->prev = c;
	  c = b;
	  b = b->next;
	  break;
	}
      else /* a && b */
	{
	  cmp = clist->compare (clist, GTK_CLIST_ROW (a), GTK_CLIST_ROW (b));
	  if ((cmp >= 0 && clist->sort_type == GTK_SORT_DESCENDING) ||
	      (cmp <= 0 && clist->sort_type == GTK_SORT_ASCENDING) ||
	      (a && !b))
	    {
	      c->next = a;
	      a->prev = c;
	      c = a;
	      a = a->next;
	    }
	  else
	    {
	      c->next = b;
	      b->prev = c;
	      c = b;
	      b = b->next;
	    }
	}
    }

  return z.next;
}

static GList *
gtk_clist_mergesort (GtkCList *clist,
		     GList    *list,         /* the list to sort */
		     gint      num)          /* the list's length */
{
  GList *half;
  gint i;

  if (num == 1)
    {
      return list;
    }
  else
    {
      /* move "half" to the middle */
      half = list;
      for (i = 0; i < num / 2; i++)
	half = half->next;

      /* cut the list in two */
      half->prev->next = NULL;
      half->prev = NULL;

      /* recursively sort both lists */
      return gtk_clist_merge (clist,
		       gtk_clist_mergesort (clist, list, num / 2),
		       gtk_clist_mergesort (clist, half, num - num / 2));
    }
}

void
gtk_clist_sort (GtkCList *clist)
{
  GTK_CLIST_CLASS_FW (clist)->sort_list (clist);
}

static void
real_sort_list (GtkCList *clist)
{
  GList *list;
  GList *work;
  gint i;
  gboolean thaw = FALSE;

  g_return_if_fail (clist != NULL);
  g_return_if_fail (GTK_IS_CLIST (clist));

  if (clist->rows <= 1)
    return;

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (clist))
    return;

  if (clist->anchor != -1 && clist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GTK_CLIST_CLASS_FW (clist)->resync_selection (clist, NULL);
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }
   
  if (!GTK_CLIST_FROZEN (clist))
    {
      gtk_clist_freeze (clist);
      thaw = TRUE;
    }

  clist->row_list = gtk_clist_mergesort (clist, clist->row_list, clist->rows);

  work = clist->selection;

  for (i = 0, list = clist->row_list; i < clist->rows; i++, list = list->next)
    {
      if (GTK_CLIST_ROW (list)->state == GTK_STATE_SELECTED)
	{
	  work->data = GINT_TO_POINTER (i);
	  work = work->next;
	}
      
      if (i == clist->rows - 1)
	clist->row_list_end = list;
    }

  if (thaw)
    gtk_clist_thaw (clist);
}
