/* GTK - The GIMP Toolkit
 * gtktextview.c Copyright (C) 2000 Red Hat, Inc.
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

#include <string.h>

#include "gtkbindings.h"
#include "gtkdnd.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkseparatormenuitem.h"
#include "gtksignal.h"
#include "gtktextdisplay.h"
#include "gtktextview.h"
#include "gtkimmulticontext.h"
#include "gdk/gdkkeysyms.h"
#include <string.h>

/* How scrolling, validation, exposes, etc. work.
 *
 * The expose_event handler has the invariant that the onscreen lines
 * have been validated.
 *
 * There are two ways that onscreen lines can become invalid. The first
 * is to change which lines are onscreen. This happens when the value
 * of a scroll adjustment changes. So the code path begins in
 * gtk_text_view_value_changed() and goes like this:
 *   - gdk_window_scroll() to reflect the new adjustment value
 *   - validate the lines that were moved onscreen
 *   - gdk_window_process_updates() to handle the exposes immediately
 *
 * The second way is that you get the "invalidated" signal from the layout,
 * indicating that lines have become invalid. This code path begins in
 * invalidated_handler() and goes like this:
 *   - install high-priority idle which does the rest of the steps
 *   - if a scroll is pending from scroll_to_mark(), do the scroll,
 *     jumping to the gtk_text_view_value_changed() code path
 *   - otherwise, validate the onscreen lines
 *   - DO NOT process updates
 *
 * In both cases, validating the onscreen lines can trigger a scroll
 * due to maintaining the first_para on the top of the screen.
 * If validation triggers a scroll, we jump to the top of the code path
 * for value_changed, and bail out of the current code path.
 *
 * Also, in size_allocate, if we invalidate some lines from changing
 * the layout width, we need to go ahead and run the high-priority idle,
 * because GTK sends exposes right after doing the size allocates without
 * returning to the main loop. This is also why the high-priority idle
 * is at a higher priority than resizing.
 *
 */

#define FOCUS_EDGE_WIDTH 1

#define SCREEN_WIDTH(widget) text_window_get_width (GTK_TEXT_VIEW (widget)->text_window)
#define SCREEN_HEIGHT(widget) text_window_get_height (GTK_TEXT_VIEW (widget)->text_window)

/* #define DEBUG_VALIDATION_AND_SCROLLING */

#ifdef DEBUG_VALIDATION_AND_SCROLLING
#define DV(x) (x)
#else
#define DV(x)
#endif

struct _GtkTextPendingScroll
{
  GtkTextMark   *mark;
  gdouble        within_margin;
  gboolean       use_align;
  gdouble        xalign;
  gdouble        yalign;
};
  
enum
{
  MOVE_CURSOR,
  SET_ANCHOR,
  INSERT_AT_CURSOR,
  DELETE_FROM_CURSOR,
  CUT_CLIPBOARD,
  COPY_CLIPBOARD,
  PASTE_CLIPBOARD,
  TOGGLE_OVERWRITE,
  SET_SCROLL_ADJUSTMENTS,
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_HEIGHT_LINES,
  ARG_WIDTH_COLUMNS,
  ARG_PIXELS_ABOVE_LINES,
  ARG_PIXELS_BELOW_LINES,
  ARG_PIXELS_INSIDE_WRAP,
  ARG_EDITABLE,
  ARG_WRAP_MODE,
  ARG_JUSTIFY,
  ARG_LEFT_MARGIN,
  ARG_RIGHT_MARGIN,
  ARG_INDENT,
  ARG_TABS,
  LAST_ARG
};

static void gtk_text_view_init                 (GtkTextView      *text_view);
static void gtk_text_view_class_init           (GtkTextViewClass *klass);
static void gtk_text_view_destroy              (GtkObject        *object);
static void gtk_text_view_finalize             (GObject          *object);
static void gtk_text_view_set_arg              (GtkObject        *object,
                                                GtkArg           *arg,
                                                guint             arg_id);
static void gtk_text_view_get_arg              (GtkObject        *object,
                                                GtkArg           *arg,
                                                guint             arg_id);
static void gtk_text_view_size_request         (GtkWidget        *widget,
                                                GtkRequisition   *requisition);
static void gtk_text_view_size_allocate        (GtkWidget        *widget,
                                                GtkAllocation    *allocation);
static void gtk_text_view_realize              (GtkWidget        *widget);
static void gtk_text_view_unrealize            (GtkWidget        *widget);
static void gtk_text_view_style_set            (GtkWidget        *widget,
                                                GtkStyle         *previous_style);
static void gtk_text_view_direction_changed    (GtkWidget        *widget,
                                                GtkTextDirection  previous_direction);
static gint gtk_text_view_event                (GtkWidget        *widget,
                                                GdkEvent         *event);
static gint gtk_text_view_key_press_event      (GtkWidget        *widget,
                                                GdkEventKey      *event);
static gint gtk_text_view_key_release_event    (GtkWidget        *widget,
                                                GdkEventKey      *event);
static gint gtk_text_view_button_press_event   (GtkWidget        *widget,
                                                GdkEventButton   *event);
static gint gtk_text_view_button_release_event (GtkWidget        *widget,
                                                GdkEventButton   *event);
static gint gtk_text_view_focus_in_event       (GtkWidget        *widget,
                                                GdkEventFocus    *event);
static gint gtk_text_view_focus_out_event      (GtkWidget        *widget,
                                                GdkEventFocus    *event);
static gint gtk_text_view_motion_event         (GtkWidget        *widget,
                                                GdkEventMotion   *event);
static gint gtk_text_view_expose_event         (GtkWidget        *widget,
                                                GdkEventExpose   *expose);
static void gtk_text_view_draw_focus           (GtkWidget        *widget);

/* Source side drag signals */
static void gtk_text_view_drag_begin       (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_text_view_drag_end         (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_text_view_drag_data_get    (GtkWidget        *widget,
                                            GdkDragContext   *context,
                                            GtkSelectionData *selection_data,
                                            guint             info,
                                            guint             time);
static void gtk_text_view_drag_data_delete (GtkWidget        *widget,
                                            GdkDragContext   *context);

/* Target side drag signals */
static void     gtk_text_view_drag_leave         (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  guint             time);
static gboolean gtk_text_view_drag_motion        (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static gboolean gtk_text_view_drag_drop          (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static void     gtk_text_view_drag_data_received (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  GtkSelectionData *selection_data,
                                                  guint             info,
                                                  guint             time);

static void gtk_text_view_set_scroll_adjustments (GtkTextView   *text_view,
                                                  GtkAdjustment *hadj,
                                                  GtkAdjustment *vadj);

static void gtk_text_view_move_cursor      (GtkTextView           *text_view,
                                            GtkMovementStep        step,
                                            gint                   count,
                                            gboolean               extend_selection);
static void gtk_text_view_set_anchor       (GtkTextView           *text_view);
static void gtk_text_view_scroll_pages     (GtkTextView           *text_view,
                                            gint                   count);
static void gtk_text_view_insert_at_cursor (GtkTextView           *text_view,
                                            const gchar           *str);
static void gtk_text_view_delete_from_cursor (GtkTextView           *text_view,
                                              GtkDeleteType          type,
                                              gint                   count);
static void gtk_text_view_cut_clipboard    (GtkTextView           *text_view);
static void gtk_text_view_copy_clipboard   (GtkTextView           *text_view);
static void gtk_text_view_paste_clipboard  (GtkTextView           *text_view);
static void gtk_text_view_toggle_overwrite (GtkTextView           *text_view);
static void gtk_text_view_unselect         (GtkTextView           *text_view);

static void     gtk_text_view_validate_onscreen     (GtkTextView        *text_view);
static void     gtk_text_view_get_first_para_iter   (GtkTextView        *text_view,
                                                     GtkTextIter        *iter);
static void     gtk_text_view_update_layout_width       (GtkTextView        *text_view);
static void     gtk_text_view_set_attributes_from_style (GtkTextView        *text_view,
                                                         GtkTextAttributes *values,
                                                         GtkStyle           *style);
static void     gtk_text_view_ensure_layout         (GtkTextView        *text_view);
static void     gtk_text_view_destroy_layout        (GtkTextView        *text_view);
static void     gtk_text_view_reset_im_context      (GtkTextView        *text_view);
static void     gtk_text_view_start_selection_drag  (GtkTextView        *text_view,
                                                     const GtkTextIter  *iter,
                                                     GdkEventButton     *event);
static gboolean gtk_text_view_end_selection_drag    (GtkTextView        *text_view,
                                                     GdkEventButton     *event);
static void     gtk_text_view_start_selection_dnd   (GtkTextView        *text_view,
                                                     const GtkTextIter  *iter,
                                                     GdkEventMotion     *event);
static void     gtk_text_view_start_cursor_blink    (GtkTextView        *text_view,
                                                     gboolean            with_delay);
static void     gtk_text_view_stop_cursor_blink     (GtkTextView        *text_view);

static void gtk_text_view_value_changed           (GtkAdjustment *adj,
						   GtkTextView   *view);
static void gtk_text_view_commit_handler          (GtkIMContext  *context,
						   const gchar   *str,
						   GtkTextView   *text_view);
static void gtk_text_view_preedit_changed_handler (GtkIMContext  *context,
						   GtkTextView   *text_view);

static void gtk_text_view_mark_set_handler       (GtkTextBuffer     *buffer,
                                                  const GtkTextIter *location,
                                                  GtkTextMark       *mark,
                                                  gpointer           data);
static void gtk_text_view_get_virtual_cursor_pos (GtkTextView       *text_view,
                                                  gint              *x,
                                                  gint              *y);
static void gtk_text_view_set_virtual_cursor_pos (GtkTextView       *text_view,
                                                  gint               x,
                                                  gint               y);

static GtkAdjustment* get_hadjustment            (GtkTextView       *text_view);
static GtkAdjustment* get_vadjustment            (GtkTextView       *text_view);

static void gtk_text_view_popup_menu             (GtkTextView       *text_view,
						  GdkEventButton    *event);

static void gtk_text_view_queue_scroll           (GtkTextView   *text_view,
                                                  GtkTextMark   *mark,
                                                  gdouble        within_margin,
                                                  gboolean       use_align,
                                                  gdouble        xalign,
                                                  gdouble        yalign);

static gboolean gtk_text_view_flush_scroll       (GtkTextView *text_view);

/* Container methods */
static void gtk_text_view_add    (GtkContainer *container,
                                  GtkWidget    *child);
static void gtk_text_view_remove (GtkContainer *container,
                                  GtkWidget    *child);
static void gtk_text_view_forall (GtkContainer *container,
                                  gboolean      include_internals,
                                  GtkCallback   callback,
                                  gpointer      callback_data);

/* FIXME probably need the focus methods. */

/* Hack-around */
#define g_signal_handlers_disconnect_by_func(obj, func, data) g_signal_handlers_disconnect_matched (obj, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, NULL, func, data)

typedef struct _GtkTextViewChild GtkTextViewChild;

struct _GtkTextViewChild
{
  GtkWidget *widget;

  GtkTextChildAnchor *anchor;

  gint from_top_of_line;
  gint from_left_of_buffer;
  
  /* These are ignored if anchor != NULL */
  GtkTextWindowType type;
  gint x;
  gint y;
};

static GtkTextViewChild* text_view_child_new_anchored (GtkWidget          *child,
                                                       GtkTextChildAnchor *anchor,
                                                       GtkTextLayout      *layout);
static GtkTextViewChild* text_view_child_new_window   (GtkWidget          *child,
                                                       GtkTextWindowType   type,
                                                       gint                x,
                                                       gint                y);
static void              text_view_child_free         (GtkTextViewChild   *child);

static void              text_view_child_realize      (GtkTextView      *text_view,
                                                       GtkTextViewChild *child);

struct _GtkTextWindow
{
  GtkTextWindowType type;
  GtkWidget *widget;
  GdkWindow *window;
  GdkWindow *bin_window;
  GtkRequisition requisition;
  GdkRectangle allocation;
};

static GtkTextWindow *text_window_new             (GtkTextWindowType  type,
                                                   GtkWidget         *widget,
                                                   gint               width_request,
                                                   gint               height_request);
static void           text_window_free            (GtkTextWindow     *win);
static void           text_window_realize         (GtkTextWindow     *win,
                                                   GdkWindow         *parent);
static void           text_window_unrealize       (GtkTextWindow     *win);
static void           text_window_size_allocate   (GtkTextWindow     *win,
                                                   GdkRectangle      *rect);
static void           text_window_scroll          (GtkTextWindow     *win,
                                                   gint               dx,
                                                   gint               dy);
static void           text_window_invalidate_rect (GtkTextWindow     *win,
                                                   GdkRectangle      *rect);

static gint           text_window_get_width       (GtkTextWindow     *win);
static gint           text_window_get_height      (GtkTextWindow     *win);
static void           text_window_get_allocation  (GtkTextWindow     *win,
                                                   GdkRectangle      *rect);


enum
{
  TARGET_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT,
  TARGET_UTF8_STRING,
  TARGET_TEXT_BUFFER_CONTENTS
};

static GtkTargetEntry target_table[] = {
  { "GTK_TEXT_BUFFER_CONTENTS", GTK_TARGET_SAME_APP,
    TARGET_TEXT_BUFFER_CONTENTS },
  { "UTF8_STRING", 0, TARGET_UTF8_STRING },
  { "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
  { "TEXT", 0, TARGET_TEXT },
  { "text/plain", 0, TARGET_STRING },
  { "STRING",     0, TARGET_STRING }
};

static GtkContainerClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_text_view_get_type (void)
{
  static GtkType our_type = 0;

  if (our_type == 0)
    {
      static const GtkTypeInfo our_info =
      {
        "GtkTextView",
        sizeof (GtkTextView),
        sizeof (GtkTextViewClass),
        (GtkClassInitFunc) gtk_text_view_class_init,
        (GtkObjectInitFunc) gtk_text_view_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

      our_type = gtk_type_unique (GTK_TYPE_CONTAINER, &our_info);
    }

  return our_type;
}

static void
add_move_binding (GtkBindingSet  *binding_set,
                  guint           keyval,
                  guint           modmask,
                  GtkMovementStep step,
                  gint            count)
{
  g_return_if_fail ((modmask & GDK_SHIFT_MASK) == 0);

  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
                                "move_cursor", 3,
                                GTK_TYPE_ENUM, step,
                                GTK_TYPE_INT, count,
                                GTK_TYPE_BOOL, FALSE);

  /* Selection-extending version */
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | GDK_SHIFT_MASK,
                                "move_cursor", 3,
                                GTK_TYPE_ENUM, step,
                                GTK_TYPE_INT, count,
                                GTK_TYPE_BOOL, TRUE);
}

static void
gtk_text_view_class_init (GtkTextViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkBindingSet *binding_set;

  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  /* Default handlers and virtual methods
   */
  object_class->set_arg = gtk_text_view_set_arg;
  object_class->get_arg = gtk_text_view_get_arg;

  object_class->destroy = gtk_text_view_destroy;
  gobject_class->finalize = gtk_text_view_finalize;

  widget_class->realize = gtk_text_view_realize;
  widget_class->unrealize = gtk_text_view_unrealize;
  widget_class->style_set = gtk_text_view_style_set;
  widget_class->direction_changed = gtk_text_view_direction_changed;
  widget_class->size_request = gtk_text_view_size_request;
  widget_class->size_allocate = gtk_text_view_size_allocate;
  widget_class->event = gtk_text_view_event;
  widget_class->key_press_event = gtk_text_view_key_press_event;
  widget_class->key_release_event = gtk_text_view_key_release_event;
  widget_class->button_press_event = gtk_text_view_button_press_event;
  widget_class->button_release_event = gtk_text_view_button_release_event;
  widget_class->focus_in_event = gtk_text_view_focus_in_event;
  widget_class->focus_out_event = gtk_text_view_focus_out_event;
  widget_class->motion_notify_event = gtk_text_view_motion_event;
  widget_class->expose_event = gtk_text_view_expose_event;

  widget_class->drag_begin = gtk_text_view_drag_begin;
  widget_class->drag_end = gtk_text_view_drag_end;
  widget_class->drag_data_get = gtk_text_view_drag_data_get;
  widget_class->drag_data_delete = gtk_text_view_drag_data_delete;

  widget_class->drag_leave = gtk_text_view_drag_leave;
  widget_class->drag_motion = gtk_text_view_drag_motion;
  widget_class->drag_drop = gtk_text_view_drag_drop;
  widget_class->drag_data_received = gtk_text_view_drag_data_received;

  container_class->add = gtk_text_view_add;
  container_class->remove = gtk_text_view_remove;
  container_class->forall = gtk_text_view_forall;

  klass->move_cursor = gtk_text_view_move_cursor;
  klass->set_anchor = gtk_text_view_set_anchor;
  klass->insert_at_cursor = gtk_text_view_insert_at_cursor;
  klass->delete_from_cursor = gtk_text_view_delete_from_cursor;
  klass->cut_clipboard = gtk_text_view_cut_clipboard;
  klass->copy_clipboard = gtk_text_view_copy_clipboard;
  klass->paste_clipboard = gtk_text_view_paste_clipboard;
  klass->toggle_overwrite = gtk_text_view_toggle_overwrite;
  klass->set_scroll_adjustments = gtk_text_view_set_scroll_adjustments;

  /*
   * Arguments
   */
  gtk_object_add_arg_type ("GtkTextView::height_lines", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_HEIGHT_LINES);
  gtk_object_add_arg_type ("GtkTextView::width_columns", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_WIDTH_COLUMNS);
  gtk_object_add_arg_type ("GtkTextView::pixels_above_lines", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_PIXELS_ABOVE_LINES);
  gtk_object_add_arg_type ("GtkTextView::pixels_below_lines", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_PIXELS_BELOW_LINES);
  gtk_object_add_arg_type ("GtkTextView::pixels_inside_wrap", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_PIXELS_INSIDE_WRAP);
  gtk_object_add_arg_type ("GtkTextView::editable", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_EDITABLE);
  gtk_object_add_arg_type ("GtkTextView::wrap_mode", GTK_TYPE_ENUM,
                           GTK_ARG_READWRITE, ARG_WRAP_MODE);
  gtk_object_add_arg_type ("GtkTextView::justify", GTK_TYPE_ENUM,
                           GTK_ARG_READWRITE, ARG_JUSTIFY);
  gtk_object_add_arg_type ("GtkTextView::left_margin", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_LEFT_MARGIN);
  gtk_object_add_arg_type ("GtkTextView::right_margin", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_RIGHT_MARGIN);
  gtk_object_add_arg_type ("GtkTextView::indent", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_INDENT);
  gtk_object_add_arg_type ("GtkTextView::tabs", GTK_TYPE_POINTER, /* FIXME */
                           GTK_ARG_READWRITE, ARG_TABS);

  /*
   * Signals
   */

  signals[MOVE_CURSOR] =
    gtk_signal_new ("move_cursor",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, move_cursor),
                    gtk_marshal_VOID__ENUM_INT_BOOLEAN,
                    GTK_TYPE_NONE, 3, GTK_TYPE_MOVEMENT_STEP, GTK_TYPE_INT, GTK_TYPE_BOOL);

  signals[SET_ANCHOR] =
    gtk_signal_new ("set_anchor",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, set_anchor),
                    gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);

  signals[INSERT_AT_CURSOR] =
    gtk_signal_new ("insert_at_cursor",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, insert_at_cursor),
                    gtk_marshal_VOID__POINTER,
                    GTK_TYPE_NONE, 1, GTK_TYPE_STRING);

  signals[DELETE_FROM_CURSOR] =
    gtk_signal_new ("delete_from_cursor",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, delete_from_cursor),
                    gtk_marshal_VOID__ENUM_INT,
                    GTK_TYPE_NONE, 2, GTK_TYPE_DELETE_TYPE, GTK_TYPE_INT);

  signals[CUT_CLIPBOARD] =
    gtk_signal_new ("cut_clipboard",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, cut_clipboard),
                    gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);

  signals[COPY_CLIPBOARD] =
    gtk_signal_new ("copy_clipboard",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, copy_clipboard),
                    gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);

  signals[PASTE_CLIPBOARD] =
    gtk_signal_new ("paste_clipboard",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, paste_clipboard),
                    gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);

  signals[TOGGLE_OVERWRITE] =
    gtk_signal_new ("toggle_overwrite",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, toggle_overwrite),
                    gtk_marshal_VOID__VOID,
                    GTK_TYPE_NONE, 0);

  signals[SET_SCROLL_ADJUSTMENTS] =
    gtk_signal_new ("set_scroll_adjustments",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextViewClass, set_scroll_adjustments),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
  widget_class->set_scroll_adjustments_signal = signals[SET_SCROLL_ADJUSTMENTS];

  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (klass);

  /* Moving the insertion point */
  add_move_binding (binding_set, GDK_Right, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, 1);

  add_move_binding (binding_set, GDK_Left, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (binding_set, GDK_f, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_LOGICAL_POSITIONS, 1);

  add_move_binding (binding_set, GDK_b, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_LOGICAL_POSITIONS, -1);

  add_move_binding (binding_set, GDK_Right, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_Left, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, -1);

  /* Eventually we want to move by display lines, not paragraphs */
  add_move_binding (binding_set, GDK_Up, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, -1);

  add_move_binding (binding_set, GDK_Down, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, 1);

  add_move_binding (binding_set, GDK_p, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_DISPLAY_LINES, -1);

  add_move_binding (binding_set, GDK_n, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_DISPLAY_LINES, 1);

  add_move_binding (binding_set, GDK_a, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_PARAGRAPH_ENDS, -1);

  add_move_binding (binding_set, GDK_e, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_PARAGRAPH_ENDS, 1);

  add_move_binding (binding_set, GDK_f, GDK_MOD1_MASK,
                    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_b, GDK_MOD1_MASK,
                    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_Home, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_End, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (binding_set, GDK_Home, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_End, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (binding_set, GDK_Page_Up, 0,
                    GTK_MOVEMENT_PAGES, -1);

  add_move_binding (binding_set, GDK_Page_Down, 0,
                    GTK_MOVEMENT_PAGES, 1);


  /* Setting the cut/paste/copy anchor */
  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_CONTROL_MASK,
                                "set_anchor", 0);

  /* Deleting text */
  gtk_binding_entry_add_signal (binding_set, GDK_Delete, 0,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_CHARS,
                                GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_d, GDK_CONTROL_MASK,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_CHARS,
                                GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, 0,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_CHARS,
                                GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_Delete, GDK_CONTROL_MASK,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
                                GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_d, GDK_MOD1_MASK,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
                                GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, GDK_CONTROL_MASK,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
                                GTK_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_k, GDK_CONTROL_MASK,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_PARAGRAPH_ENDS,
                                GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_u, GDK_CONTROL_MASK,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_PARAGRAPHS,
                                GTK_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_MOD1_MASK,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_WHITESPACE,
                                GTK_TYPE_INT, 1);
  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_MOD1_MASK,
                                "insert_at_cursor", 1,
                                GTK_TYPE_STRING, " ");

  gtk_binding_entry_add_signal (binding_set, GDK_backslash, GDK_MOD1_MASK,
                                "delete_from_cursor", 2,
                                GTK_TYPE_ENUM, GTK_DELETE_WHITESPACE,
                                GTK_TYPE_INT, 1);

  /* Cut/copy/paste */

  gtk_binding_entry_add_signal (binding_set, GDK_x, GDK_CONTROL_MASK,
                                "cut_clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_w, GDK_CONTROL_MASK,
                                "cut_clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_c, GDK_CONTROL_MASK,
                                "copy_clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_v, GDK_CONTROL_MASK,
                                "paste_clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_y, GDK_CONTROL_MASK,
                                "paste_clipboard", 0);

  /* Overwrite */
  gtk_binding_entry_add_signal (binding_set, GDK_Insert, 0,
                                "toggle_overwrite", 0);
}

void
gtk_text_view_init (GtkTextView *text_view)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (text_view);

  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);

  /* Set up default style */
  text_view->wrap_mode = GTK_WRAP_NONE;
  text_view->pixels_above_lines = 0;
  text_view->pixels_below_lines = 0;
  text_view->pixels_inside_wrap = 0;
  text_view->justify = GTK_JUSTIFY_LEFT;
  text_view->left_margin = 0;
  text_view->right_margin = 0;
  text_view->indent = 0;
  text_view->tabs = NULL;
  text_view->editable = TRUE;

  gtk_drag_dest_set (widget,
                     GTK_DEST_DEFAULT_DROP,
                     target_table, G_N_ELEMENTS (target_table),
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  text_view->virtual_cursor_x = -1;
  text_view->virtual_cursor_y = -1;

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize ().
   */
  text_view->im_context = gtk_im_multicontext_new ();

  gtk_signal_connect (GTK_OBJECT (text_view->im_context), "commit",
                      GTK_SIGNAL_FUNC (gtk_text_view_commit_handler), text_view);

  gtk_signal_connect (GTK_OBJECT (text_view->im_context), "preedit_changed",
 		      GTK_SIGNAL_FUNC (gtk_text_view_preedit_changed_handler), text_view);

  text_view->cursor_visible = TRUE;

  text_view->text_window = text_window_new (GTK_TEXT_WINDOW_TEXT,
                                            widget, 200, 200);

  text_view->drag_start_x = -1;
  text_view->drag_start_y = -1;
}

/**
 * gtk_text_view_new:
 *
 * Creates a new #GtkTextView. If you don't call gtk_text_view_set_buffer()
 * before using the text view, an empty default buffer will be created
 * for you. Get the buffer with gtk_text_view_get_buffer(). If you want
 * to specify your own buffer, consider gtk_text_view_new_with_buffer().
 *
 * Return value: a new #GtkTextView
 **/
GtkWidget*
gtk_text_view_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_text_view_get_type ()));
}

/**
 * gtk_text_view_new_with_buffer:
 * @buffer: a #GtkTextBuffer
 *
 * Creates a new #GtkTextView widget displaying the buffer
 * @buffer. One buffer can be shared among many widgets.
 * @buffer may be NULL to create a default buffer, in which case
 * this function is equivalent to gtk_text_view_new(). The
 * text view adds its own reference count to the buffer; it does not
 * take over an existing reference.
 *
 * Return value: a new #GtkTextView.
 **/
GtkWidget*
gtk_text_view_new_with_buffer (GtkTextBuffer *buffer)
{
  GtkTextView *text_view;

  text_view = (GtkTextView*)gtk_text_view_new ();

  gtk_text_view_set_buffer (text_view, buffer);

  return GTK_WIDGET (text_view);
}

/**
 * gtk_text_view_set_buffer:
 * @text_view: a #GtkTextView
 * @buffer: a #GtkTextBuffer
 *
 * Sets @buffer as the buffer being displayed by @text_view. The previous
 * buffer displayed by the text view is unreferenced, and a reference is
 * added to @buffer. If you owned a reference to @buffer before passing it
 * to this function, you must remove that reference yourself; #GtkTextView
 * will not "adopt" it.
 *
 **/
void
gtk_text_view_set_buffer (GtkTextView   *text_view,
                          GtkTextBuffer *buffer)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (buffer == NULL || GTK_IS_TEXT_BUFFER (buffer));

  if (text_view->buffer == buffer)
    return;

  if (text_view->buffer != NULL)
    {
      /* Destroy all anchored children */
      GSList *tmp_list;
      GSList *copy;

      copy = g_slist_copy (text_view->children);
      tmp_list = copy;
      while (tmp_list != NULL)
        {
          GtkTextViewChild *vc = tmp_list->data;

          if (vc->anchor)
            {
              gtk_widget_destroy (vc->widget);
              /* vc may now be invalid! */
            }

          tmp_list = g_slist_next (tmp_list);
        }

      g_slist_free (copy);

      g_signal_handlers_disconnect_by_func (G_OBJECT (text_view->buffer),
                                            gtk_text_view_mark_set_handler, text_view);
      g_object_unref (G_OBJECT (text_view->buffer));
      text_view->dnd_mark = NULL;
    }

  text_view->buffer = buffer;

  if (buffer != NULL)
    {
      GtkTextIter start;

      g_object_ref (G_OBJECT (buffer));

      if (text_view->layout)
        gtk_text_layout_set_buffer (text_view->layout, buffer);

      gtk_text_buffer_get_iter_at_offset (text_view->buffer, &start, 0);

      text_view->dnd_mark = gtk_text_buffer_create_mark (text_view->buffer,
                                                         "gtk_drag_target",
                                                         &start, FALSE);

      text_view->first_para_mark = gtk_text_buffer_create_mark (text_view->buffer,
                                                                NULL,
                                                                &start, TRUE);

      text_view->first_para_pixels = 0;

      g_signal_connect_data (G_OBJECT (text_view->buffer), "mark_set",
                             gtk_text_view_mark_set_handler, text_view,
                             NULL, FALSE, FALSE);
    }

  if (GTK_WIDGET_VISIBLE (text_view))
    gtk_widget_queue_draw (GTK_WIDGET (text_view));
}

static GtkTextBuffer*
get_buffer (GtkTextView *text_view)
{
  if (text_view->buffer == NULL)
    {
      GtkTextBuffer *b;
      b = gtk_text_buffer_new (NULL);
      gtk_text_view_set_buffer (text_view, b);
      g_object_unref (G_OBJECT (b));
    }

  return text_view->buffer;
}

/**
 * gtk_text_view_get_buffer:
 * @text_view: a #GtkTextView
 *
 * Returns the #GtkTextBuffer being displayed by this text view.
 * The reference count on the buffer is not incremented; the caller
 * of this function won't own a new reference.
 *
 * Return value: a #GtkTextBuffer
 **/
GtkTextBuffer*
gtk_text_view_get_buffer (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  return get_buffer (text_view);
}

/**
 * gtk_text_view_get_iter_at_location:
 * @text_view: a #GtkTextView
 * @iter: a #GtkTextIter
 * @x: x position, in buffer coordinates
 * @y: y position, in buffer coordinates
 *
 * Retrieves the iterator at buffer coordinates @x and @y. Buffer
 * coordinates are coordinates for the entire buffer, not just the
 * currently-displayed portion.  If you have coordinates from an
 * event, you have to convert those to buffer coordinates with
 * gtk_text_view_window_to_buffer_coords().
 *
 **/
void
gtk_text_view_get_iter_at_location (GtkTextView *text_view,
                                    GtkTextIter *iter,
                                    gint         x,
                                    gint         y)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text_view->layout != NULL);

  gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                     iter,
                                     x,
                                     y);
}

/**
 * gtk_text_view_get_iter_location:
 * @text_view: a #GtkTextView
 * @iter: a #GtkTextIter
 * @location: bounds of the character at @iter
 *
 * Gets a rectangle which roughly contains the character at @iter.
 * The rectangle position is in buffer coordinates; use
 * gtk_text_view_buffer_to_window_coords() to convert these
 * coordinates to coordinates for one of the windows in the text view.
 *
 **/
void
gtk_text_view_get_iter_location (GtkTextView       *text_view,
                                 const GtkTextIter *iter,
                                 GdkRectangle      *location)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == get_buffer (text_view));

  gtk_text_layout_get_iter_location (text_view->layout, iter, location);
}

/**
 * gtk_text_view_get_line_yrange:
 * @text_view: a #GtkTextView
 * @iter: a #GtkTextIter
 * @y: return location for a y coordinate
 * @height: return location for a height
 *
 * Gets the y coordinate of the top of the line containing @iter,
 * and the height of the line. The coordinate is a buffer coordinate;
 * convert to window coordinates with gtk_text_view_buffer_to_window_coords().
 *
 **/
void
gtk_text_view_get_line_yrange (GtkTextView       *text_view,
                               const GtkTextIter *iter,
                               gint              *y,
                               gint              *height)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == get_buffer (text_view));

  gtk_text_layout_get_line_yrange (text_view->layout,
                                   iter,
                                   y,
                                   height);
}

/**
 * gtk_text_view_get_line_at_y:
 * @text_view: a #GtkTextView
 * @target_iter: a #GtkTextIter
 * @y: a y coordinate
 * @line_top: return location for top coordinate of the line
 *
 * Gets the #GtkTextIter at the start of the line containing
 * the coordinate @y. @y is in buffer coordinates, convert from
 * window coordinates with gtk_text_view_window_to_buffer_coords().
 * If non-%NULL, @line_top will be filled with the coordinate of the top
 * edge of the line.
 **/
void
gtk_text_view_get_line_at_y (GtkTextView *text_view,
                             GtkTextIter *target_iter,
                             gint         y,
                             gint        *line_top)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  gtk_text_layout_get_line_at_y (text_view->layout,
                                 target_iter,
                                 y,
                                 line_top);
}

static gboolean
set_adjustment_clamped (GtkAdjustment *adj, gfloat val)
{
  /* We don't really want to clamp to upper; we want to clamp to
     upper - page_size which is the highest value the scrollbar
     will let us reach. */
  if (val > (adj->upper - adj->page_size))
    val = adj->upper - adj->page_size;

  if (val < adj->lower)
    val = adj->lower;

  if (val != adj->value)
    {
      gtk_adjustment_set_value (adj, val);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_view_scroll_to_iter:
 * @text_view: a #GtkTextView
 * @iter: a #GtkTextIter
 * @within_margin: margin as a [0.0,0.5) fraction of screen size
 * @use_align: whether to use alignment arguments (if %FALSE, just get the mark onscreen)
 * @xalign: horizontal alignment of mark within visible area.
 * @yalign: vertical alignment of mark within visible area
 *
 * Scrolls @text_view so that @iter is on the screen in the position
 * indicated by @xalign and @yalign. An alignment of 0.0 indicates
 * left or top, 1.0 indicates right or bottom, 0.5 means center. If @use_align
 * is %FALSE, the text scrolls the minimal distance to get the mark onscreen,
 * possibly not scrolling at all. The effective screen for purposes
 * of this function is reduced by a margin of size @within_margin.
 * NOTE: This function uses the currently-computed height of the
 * lines in the text buffer. Note that line heights are computed
 * in an idle handler; so this function may not have the desired effect
 * if it's called before the height computations. To avoid oddness,
 * consider using gtk_text_view_scroll_to_mark() which saves a point
 * to be scrolled to after line validation.
 *
 * Return value: %TRUE if scrolling occurred
 **/
gboolean
gtk_text_view_scroll_to_iter (GtkTextView   *text_view,
                              GtkTextIter   *iter,
                              gdouble        within_margin,
                              gboolean       use_align,
                              gdouble        xalign,
                              gdouble        yalign)
{
  GdkRectangle rect;
  GdkRectangle screen;
  gint screen_bottom;
  gint screen_right;
  gint scroll_dest;
  GtkWidget *widget;
  gboolean retval = FALSE;
  gint scroll_inc;
  gint screen_xoffset, screen_yoffset;
  gint current_x_scroll, current_y_scroll;
  
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (within_margin >= 0.0 && within_margin < 0.5, FALSE);
  g_return_val_if_fail (xalign >= 0.0 && xalign <= 1.0, FALSE);
  g_return_val_if_fail (yalign >= 0.0 && yalign <= 1.0, FALSE);
  
  widget = GTK_WIDGET (text_view);

  DV(g_print(G_STRLOC"\n"));
  
  if (!GTK_WIDGET_MAPPED (widget))
    {
      g_warning ("%s: calling this function before mapping the GtkTextView doesn't make sense, maybe try gtk_text_view_scroll_to_mark() instead", G_STRLOC);
      return FALSE;
    }
  
  gtk_text_layout_get_iter_location (text_view->layout,
                                     iter,
                                     &rect);

  current_x_scroll = text_view->xoffset;
  current_y_scroll = text_view->yoffset;

  screen.x = current_x_scroll;
  screen.y = current_y_scroll;
  screen.width = SCREEN_WIDTH (widget);
  screen.height = SCREEN_HEIGHT (widget);
  
  screen_xoffset = screen.width * within_margin;
  screen_yoffset = screen.height * within_margin;
  
  screen.x += screen_xoffset;
  screen.y += screen_yoffset;
  screen.width -= screen_xoffset * 2;
  screen.height -= screen_yoffset * 2;

  /* paranoia check */
  if (screen.width < 1)
    screen.width = 1;
  if (screen.height < 1)
    screen.height = 1;
  
  screen_right = screen.x + screen.width;
  screen_bottom = screen.y + screen.height;
  
  /* The alignment affects the point in the target character that we
   * choose to align. If we're doing right/bottom alignment, we align
   * the right/bottom edge of the character the mark is at; if we're
   * doing left/top we align the left/top edge of the character; if
   * we're doing center alignment we align the center of the
   * character.
   */
  
  /* Vertical scroll */

  scroll_inc = 0;
  scroll_dest = current_y_scroll;
  
  if (use_align)
    {      
      scroll_dest = rect.y + (rect.height * yalign) - (screen.height * yalign);
      
      /* if scroll_dest < screen.y, we move a negative increment (up),
       * else a positive increment (down)
       */
      scroll_inc = scroll_dest - screen.y + screen_yoffset;
    }
  else
    {
      /* move minimum to get onscreen */
      if (rect.y < screen.y)
        {
          scroll_dest = rect.y;
          scroll_inc = scroll_dest - screen.y - screen_yoffset;
        }
      else if ((rect.y + rect.height) > screen_bottom)
        {
          scroll_dest = rect.y + rect.height;
          scroll_inc = scroll_dest - screen_bottom + screen_yoffset;
        }
    }  
  
  if (scroll_inc != 0)
    {
      retval = set_adjustment_clamped (get_vadjustment (text_view),
                                       current_y_scroll + scroll_inc);
    }

  /* Horizontal scroll */
  
  scroll_inc = 0;
  scroll_dest = current_x_scroll;
  
  if (use_align)
    {      
      scroll_dest = rect.x + (rect.width * xalign) - (screen.width * xalign);

      /* if scroll_dest < screen.y, we move a negative increment (left),
       * else a positive increment (right)
       */
      scroll_inc = scroll_dest - screen.x + screen_xoffset;
    }
  else
    {
      /* move minimum to get onscreen */
      if (rect.x < screen.x)
        {
          scroll_dest = rect.x;
          scroll_inc = scroll_dest - screen.x - screen_xoffset;
        }
      else if ((rect.x + rect.width) > screen_right)
        {
          scroll_dest = rect.x + rect.width;
          scroll_inc = scroll_dest - screen_right + screen_xoffset;
        }
    }
  
  if (scroll_inc != 0)
    {
      retval = set_adjustment_clamped (get_hadjustment (text_view),
                                       current_x_scroll + scroll_inc);
    }

  if (retval)
    DV(g_print (">Actually scrolled ("G_STRLOC")\n"));
  else
    DV(g_print (">Didn't end up scrolling ("G_STRLOC")\n"));
  
  return retval;
}

static void
free_pending_scroll (GtkTextPendingScroll *scroll)
{
  if (!gtk_text_mark_get_deleted (scroll->mark))
    gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (scroll->mark),
                                 scroll->mark);
  g_object_unref (G_OBJECT (scroll->mark));
  g_free (scroll);
}

static void
gtk_text_view_queue_scroll (GtkTextView   *text_view,
                            GtkTextMark   *mark,
                            gdouble        within_margin,
                            gboolean       use_align,
                            gdouble        xalign,
                            gdouble        yalign)
{
  GtkTextIter iter;
  GtkTextPendingScroll *scroll;

  DV(g_print(G_STRLOC"\n"));
  
  scroll = g_new (GtkTextPendingScroll, 1);

  scroll->within_margin = within_margin;
  scroll->use_align = use_align;
  scroll->xalign = xalign;
  scroll->yalign = yalign;
  
  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &iter, mark);

  scroll->mark = gtk_text_buffer_create_mark (get_buffer (text_view),
                                              NULL,
                                              &iter,
                                              gtk_text_mark_get_left_gravity (mark));

  g_object_ref (G_OBJECT (scroll->mark));
  
  if (text_view->pending_scroll)
    free_pending_scroll (text_view->pending_scroll);

  text_view->pending_scroll = scroll;
}

static gboolean
gtk_text_view_flush_scroll (GtkTextView *text_view)
{
  GtkTextIter iter, start, end;
  GtkTextPendingScroll *scroll;
  gint y0, y1, height;
  gboolean retval;
  
  DV(g_print(G_STRLOC"\n"));
  
  if (text_view->pending_scroll == NULL)
    return FALSE;

  scroll = text_view->pending_scroll;

  /* avoid recursion */
  text_view->pending_scroll = NULL;
  
  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &iter, scroll->mark);

  start = iter;
  end = iter;

  /* Force-validate the region around the iterator, at least the lines
   * on either side, so that we can meaningfully get the iter location
   */
  gtk_text_iter_backward_line (&start);
  gtk_text_iter_forward_line (&end);
  
  gtk_text_layout_get_line_yrange (text_view->layout, &start, &y0, NULL);
  gtk_text_layout_get_line_yrange (text_view->layout, &end, &y1, &height);

  DV(g_print (">Validating scroll destination ("G_STRLOC")\n"));
  gtk_text_layout_validate_yrange (text_view->layout, &start, y0, y1 + height);

  DV(g_print (">Done validating scroll destination ("G_STRLOC")\n"));
  
  retval = gtk_text_view_scroll_to_iter (text_view,
                                         &iter,
                                         scroll->within_margin,
                                         scroll->use_align,
                                         scroll->xalign,
                                         scroll->yalign);
  
  free_pending_scroll (scroll);

  return retval;
}

static void
gtk_text_view_set_adjustment_upper (GtkAdjustment *adj, gfloat upper)
{  
  if (upper != adj->upper)
    {
      gfloat min = MAX (0., upper - adj->page_size);
      gboolean value_changed = FALSE;

      adj->upper = upper;

      if (adj->value > min)
        {
          adj->value = min;
          value_changed = TRUE;
        }

      gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
      DV(g_print(">Changed adj upper to %d ("G_STRLOC")\n", upper));
      
      if (value_changed)
        {
          DV(g_print(">Changed adj value because upper descreased ("G_STRLOC")\n"));
          gtk_signal_emit_by_name (GTK_OBJECT (adj), "value_changed");
        }
    }
}

static void
gtk_text_view_update_adjustments (GtkTextView *text_view)
{
  gint width = 0, height = 0;

  DV(g_print(">Updating adjustments ("G_STRLOC")\n"));
  
  gtk_text_layout_get_size (text_view->layout, &width, &height);

  if (text_view->width != width || text_view->height != height)
    {
      text_view->width = width;
      text_view->height = height;

      gtk_text_view_set_adjustment_upper (get_hadjustment (text_view),
                                          MAX (SCREEN_WIDTH (text_view), width));
      gtk_text_view_set_adjustment_upper (get_vadjustment (text_view),
                                          MAX (SCREEN_HEIGHT (text_view), height));
      
      /* hadj/vadj exist since we called get_hadjustment/get_vadjustment above */

      /* Set up the step sizes; we'll say that a page is
         our allocation minus one step, and a step is
         1/10 of our allocation. */
      text_view->hadjustment->step_increment =
        SCREEN_WIDTH (text_view) / 10.0;
      text_view->hadjustment->page_increment =
        SCREEN_WIDTH (text_view) * 0.9;
      
      text_view->vadjustment->step_increment =
        SCREEN_HEIGHT (text_view) / 10.0;
      text_view->vadjustment->page_increment =
        SCREEN_HEIGHT (text_view) * 0.9;

      gtk_signal_emit_by_name (GTK_OBJECT (get_hadjustment (text_view)), "changed");
      gtk_signal_emit_by_name (GTK_OBJECT (get_hadjustment (text_view)), "changed");
    }
}

static void
gtk_text_view_update_layout_width (GtkTextView *text_view)
{
  DV(g_print(">Updating layout width ("G_STRLOC")\n"));
  
  gtk_text_view_ensure_layout (text_view);

  gtk_text_layout_set_screen_width (text_view->layout,
                                    SCREEN_WIDTH (text_view));
}

/**
 * gtk_text_view_scroll_to_mark:
 * @text_view: a #GtkTextView
 * @mark: a #GtkTextMark
 * @within_margin: margin as a [0.0,0.5) fraction of screen size
 * @use_align: whether to use alignment arguments (if %FALSE, just get the mark onscreen)
 * @xalign: horizontal alignment of mark within visible area.
 * @yalign: vertical alignment of mark within visible area
 *
 * Scrolls @text_view so that @mark is on the screen in the position
 * indicated by @xalign and @yalign. An alignment of 0.0 indicates
 * left or top, 1.0 indicates right or bottom, 0.5 means center. If @use_align
 * is %FALSE, the text scrolls the minimal distance to get the mark onscreen,
 * possibly not scrolling at all. The effective screen for purposes
 * of this function is reduced by a margin of size @within_margin.
 *
 **/
void
gtk_text_view_scroll_to_mark (GtkTextView *text_view,
                              GtkTextMark *mark,
                              gdouble      within_margin,
                              gboolean     use_align,
                              gdouble      xalign,
                              gdouble      yalign)
{  
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (within_margin >= 0.0 && within_margin < 0.5);
  g_return_if_fail (xalign >= 0.0 && xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0 && yalign <= 1.0);

  gtk_text_view_queue_scroll (text_view, mark,
                              within_margin,
                              use_align,
                              xalign,
                              yalign);

  /* If no validation is pending, we need to go ahead and force an
   * immediate scroll.
   */
  if (text_view->layout &&
      gtk_text_layout_is_valid (text_view->layout))
    gtk_text_view_flush_scroll (text_view);
}

void
gtk_text_view_scroll_mark_onscreen (GtkTextView *text_view,
                                    GtkTextMark *mark)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));

  gtk_text_view_scroll_to_mark (text_view, mark, 0.0, FALSE, 0.0, 0.0);
}

static gboolean
clamp_iter_onscreen (GtkTextView *text_view, GtkTextIter *iter)
{
  GdkRectangle visible_rect;
  gtk_text_view_get_visible_rect (text_view, &visible_rect);

  return gtk_text_layout_clamp_iter_to_vrange (text_view->layout, iter,
                                               visible_rect.y,
                                               visible_rect.y + visible_rect.height);
}

/**
 * gtk_text_view_move_mark_onscreen:
 * @text_view: a #GtkTextView
 * @mark: a #GtkTextMark
 *
 * Moves a mark within the buffer so that it's
 * located within the currently-visible text area.
 *
 * Return value: %TRUE if the mark moved (wasn't already onscreen)
 **/
gboolean
gtk_text_view_move_mark_onscreen (GtkTextView *text_view,
                                  GtkTextMark *mark)
{
  GtkTextIter iter;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (mark != NULL, FALSE);

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &iter, mark);

  if (clamp_iter_onscreen (text_view, &iter))
    {
      gtk_text_buffer_move_mark (get_buffer (text_view), mark, &iter);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_view_get_visible_rect:
 * @text_view: a #GtkTextView
 * @visible_rect: rectangle to fill
 *
 * Fills @visible_rect with the currently-visible
 * region of the buffer, in buffer coordinates. Convert to window coordinates
 * with gtk_text_view_buffer_to_window_coords().
 **/
void
gtk_text_view_get_visible_rect (GtkTextView  *text_view,
                                GdkRectangle *visible_rect)
{
  GtkWidget *widget;

  g_return_if_fail (text_view != NULL);
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  widget = GTK_WIDGET (text_view);

  if (visible_rect)
    {
      visible_rect->x = text_view->xoffset;
      visible_rect->y = text_view->yoffset;
      visible_rect->width = SCREEN_WIDTH (widget);
      visible_rect->height = SCREEN_HEIGHT (widget);

      DV(g_print(" visible rect: %d,%d %d x %d\n",
                 visible_rect->x,
                 visible_rect->y,
                 visible_rect->width,
                 visible_rect->height));
    }
}

/**
 * gtk_text_view_set_wrap_mode:
 * @text_view: a #GtkTextView
 * @wrap_mode: a #GtkWrapMode
 *
 * Sets the line wrapping for the view.
 **/
void
gtk_text_view_set_wrap_mode (GtkTextView *text_view,
                             GtkWrapMode  wrap_mode)
{
  g_return_if_fail (text_view != NULL);
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->wrap_mode != wrap_mode)
    {
      text_view->wrap_mode = wrap_mode;

      if (text_view->layout)
        {
          text_view->layout->default_style->wrap_mode = wrap_mode;
          gtk_text_layout_default_style_changed (text_view->layout);
        }
    }
}

/**
 * gtk_text_view_get_wrap_mode:
 * @text_view: a #GtkTextView
 *
 * Gets the line wrapping for the view.
 *
 * Return value: the line wrap setting
 **/
GtkWrapMode
gtk_text_view_get_wrap_mode (GtkTextView *text_view)
{
  g_return_val_if_fail (text_view != NULL, GTK_WRAP_NONE);
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), GTK_WRAP_NONE);

  return text_view->wrap_mode;
}

/**
 * gtk_text_view_set_editable:
 * @text_view: a #GtkTextView
 * @setting: whether it's editable
 *
 * Sets the default editability of the #GtkTextView. You can override
 * this default setting with tags in the buffer, using the "editable"
 * attribute of tags.
 **/
void
gtk_text_view_set_editable (GtkTextView *text_view,
                            gboolean     setting)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->editable != setting)
    {
      text_view->editable = setting;

      if (text_view->layout)
        {
          text_view->layout->default_style->editable = text_view->editable;
          gtk_text_layout_default_style_changed (text_view->layout);
        }
    }
}

/**
 * gtk_text_view_get_editable:
 * @text_view: a #GtkTextView
 *
 * Returns the default editability of the #GtkTextView. Tags in the
 * buffer may override this setting for some ranges of text.
 *
 * Return value: whether text is editable by default
 **/
gboolean
gtk_text_view_get_editable (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  return text_view->editable;
}

void
gtk_text_view_set_pixels_above_lines (GtkTextView *text_view,
                                      gint         pixels_above_lines)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->pixels_above_lines != pixels_above_lines)
    {
      text_view->pixels_above_lines = pixels_above_lines;

      if (text_view->layout)
        {
          text_view->layout->default_style->pixels_above_lines = pixels_above_lines;
          gtk_text_layout_default_style_changed (text_view->layout);
        }
    }
}

gint
gtk_text_view_get_pixels_above_lines (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->pixels_above_lines;
}

void
gtk_text_view_set_pixels_below_lines (GtkTextView *text_view,
                                      gint         pixels_below_lines)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->pixels_below_lines != pixels_below_lines)
    {
      text_view->pixels_below_lines = pixels_below_lines;

      if (text_view->layout)
        {
          text_view->layout->default_style->pixels_below_lines = pixels_below_lines;
          gtk_text_layout_default_style_changed (text_view->layout);
        }
    }
}

gint
gtk_text_view_get_pixels_below_lines (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->pixels_below_lines;
}

void
gtk_text_view_set_pixels_inside_wrap (GtkTextView *text_view,
                                      gint         pixels_inside_wrap)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->pixels_inside_wrap != pixels_inside_wrap)
    {
      text_view->pixels_inside_wrap = pixels_inside_wrap;

      if (text_view->layout)
        {
          text_view->layout->default_style->pixels_inside_wrap = pixels_inside_wrap;
          gtk_text_layout_default_style_changed (text_view->layout);
        }
    }
}

gint
gtk_text_view_get_pixels_inside_wrap (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->pixels_inside_wrap;
}

void
gtk_text_view_set_justification (GtkTextView     *text_view,
                                 GtkJustification justify)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->justify != justify)
    {
      text_view->justify = justify;

      if (text_view->layout)
        {
          text_view->layout->default_style->justification = justify;
          gtk_text_layout_default_style_changed (text_view->layout);
        }
    }
}

GtkJustification
gtk_text_view_get_justification (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), GTK_JUSTIFY_LEFT);

  return text_view->justify;
}

void
gtk_text_view_set_left_margin (GtkTextView *text_view,
                               gint         left_margin)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->left_margin != left_margin)
    {
      text_view->left_margin = left_margin;

      if (text_view->layout)
        {
          text_view->layout->default_style->left_margin = left_margin;
          gtk_text_layout_default_style_changed (text_view->layout);
        }
    }
}

gint
gtk_text_view_get_left_margin (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->left_margin;
}

void
gtk_text_view_set_right_margin (GtkTextView *text_view,
                                gint         right_margin)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->right_margin != right_margin)
    {
      text_view->right_margin = right_margin;

      if (text_view->layout)
        {
          text_view->layout->default_style->right_margin = right_margin;
          gtk_text_layout_default_style_changed (text_view->layout);
        }
    }
}

gint
gtk_text_view_get_right_margin (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->right_margin;
}

void
gtk_text_view_set_indent (GtkTextView *text_view,
                          gint         indent)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->indent != indent)
    {
      text_view->indent = indent;

      if (text_view->layout)
        {
          text_view->layout->default_style->indent = indent;
          gtk_text_layout_default_style_changed (text_view->layout);
        }
    }
}

gint
gtk_text_view_get_indent (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->indent;
}

void
gtk_text_view_set_tabs (GtkTextView   *text_view,
                        PangoTabArray *tabs)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->tabs)
    pango_tab_array_free (text_view->tabs);

  text_view->tabs = tabs ? pango_tab_array_copy (tabs) : NULL;

  if (text_view->layout)
    {
      /* some unkosher futzing in internal struct details... */
      if (text_view->layout->default_style->tabs)
        pango_tab_array_free (text_view->layout->default_style->tabs);

      text_view->layout->default_style->tabs =
        text_view->tabs ? pango_tab_array_copy (text_view->tabs) : NULL;

      gtk_text_layout_default_style_changed (text_view->layout);
    }
}

PangoTabArray*
gtk_text_view_get_tabs (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  return text_view->tabs ? pango_tab_array_copy (text_view->tabs) : NULL;
}

/**
 * gtk_text_view_set_cursor_visible:
 * @text_view: a #GtkTextView
 * @setting: whether to show the insertion cursor
 *
 * Toggles whether the insertion point is displayed. A buffer with no editable
 * text probably shouldn't have a visible cursor, so you may want to turn
 * the cursor off.
 **/
void
gtk_text_view_set_cursor_visible    (GtkTextView   *text_view,
                                     gboolean       setting)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  setting = (setting != FALSE);

  if (text_view->cursor_visible != setting)
    {
      text_view->cursor_visible = setting;

      if (GTK_WIDGET_HAS_FOCUS (text_view))
        {
          if (text_view->layout)
            {
              gtk_text_layout_set_cursor_visible (text_view->layout, setting);

              if (setting)
                gtk_text_view_start_cursor_blink (text_view, FALSE);
              else
                gtk_text_view_stop_cursor_blink (text_view);
            }
        }
    }
}

/**
 * gtk_text_view_get_cursor_visible:
 * @text_view: a #GtkTextView
 *
 * Find out whether the cursor is being displayed.
 *
 * Return value: whether the insertion mark is visible
 **/
gboolean
gtk_text_view_get_cursor_visible    (GtkTextView   *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  return text_view->cursor_visible;
}


/**
 * gtk_text_view_place_cursor_onscreen:
 * @text_view: a #GtkTextView
 *
 * Moves the cursor to the currently visible region of the
 * buffer, it it isn't there already.
 *
 * Return value: TRUE if the cursor had to be moved.
 **/
gboolean
gtk_text_view_place_cursor_onscreen (GtkTextView *text_view)
{
  GtkTextIter insert;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_mark (get_buffer (text_view),
                                                              "insert"));

  if (clamp_iter_onscreen (text_view, &insert))
    {
      gtk_text_buffer_place_cursor (get_buffer (text_view), &insert);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_text_view_destroy (GtkObject *object)
{
  GtkTextView *text_view;
  GtkTextLayout *layout;
  
  text_view = GTK_TEXT_VIEW (object);

  layout = text_view->layout;
  
  gtk_text_view_destroy_layout (text_view);
  gtk_text_view_set_buffer (text_view, NULL);

  (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_text_view_finalize (GObject *object)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (object);

  g_return_if_fail (text_view->buffer == NULL);

  gtk_text_view_destroy_layout (text_view);
  gtk_text_view_set_buffer (text_view, NULL);
  
  if (text_view->pending_scroll)
    {
      free_pending_scroll (text_view->pending_scroll);
      text_view->pending_scroll = NULL;
    }
  
  if (text_view->hadjustment)
    g_object_unref (G_OBJECT (text_view->hadjustment));
  if (text_view->vadjustment)
    g_object_unref (G_OBJECT (text_view->vadjustment));

  text_window_free (text_view->text_window);

  if (text_view->left_window)
    text_window_free (text_view->left_window);

  if (text_view->top_window)
    text_window_free (text_view->top_window);

  if (text_view->right_window)
    text_window_free (text_view->right_window);

  if (text_view->bottom_window)
    text_window_free (text_view->bottom_window);

  g_object_unref (G_OBJECT (text_view->im_context));

  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_text_view_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (object);

  switch (arg_id)
    {
    case ARG_HEIGHT_LINES:
      g_warning ("FIXME");
      break;

    case ARG_WIDTH_COLUMNS:
      g_warning ("FIXME");
      break;

    case ARG_PIXELS_ABOVE_LINES:
      gtk_text_view_set_pixels_above_lines (text_view, GTK_VALUE_INT (*arg));
      break;

    case ARG_PIXELS_BELOW_LINES:
      gtk_text_view_set_pixels_below_lines (text_view, GTK_VALUE_INT (*arg));
      break;

    case ARG_PIXELS_INSIDE_WRAP:
      gtk_text_view_set_pixels_inside_wrap (text_view, GTK_VALUE_INT (*arg));
      break;

    case ARG_EDITABLE:
      gtk_text_view_set_editable (text_view, GTK_VALUE_BOOL (*arg));
      break;

    case ARG_WRAP_MODE:
      gtk_text_view_set_wrap_mode (text_view, GTK_VALUE_ENUM (*arg));
      break;

    case ARG_JUSTIFY:
      gtk_text_view_set_justification (text_view, GTK_VALUE_ENUM (*arg));
      break;

    case ARG_LEFT_MARGIN:
      gtk_text_view_set_left_margin (text_view, GTK_VALUE_INT (*arg));
      break;

    case ARG_RIGHT_MARGIN:
      gtk_text_view_set_right_margin (text_view, GTK_VALUE_INT (*arg));
      break;

    case ARG_INDENT:
      gtk_text_view_set_indent (text_view, GTK_VALUE_INT (*arg));
      break;

    case ARG_TABS:
      gtk_text_view_set_tabs (text_view, GTK_VALUE_POINTER (*arg));
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_text_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (object);

  switch (arg_id)
    {
    case ARG_HEIGHT_LINES:
      g_warning ("FIXME");
      break;

    case ARG_WIDTH_COLUMNS:
      g_warning ("FIXME");
      break;

    case ARG_PIXELS_ABOVE_LINES:
      GTK_VALUE_INT (*arg) = text_view->pixels_above_lines;
      break;

    case ARG_PIXELS_BELOW_LINES:
      GTK_VALUE_INT (*arg) = text_view->pixels_below_lines;
      break;

    case ARG_PIXELS_INSIDE_WRAP:
      GTK_VALUE_INT (*arg) = text_view->pixels_inside_wrap;
      break;

    case ARG_EDITABLE:
      GTK_VALUE_BOOL (*arg) = text_view->editable;
      break;

    case ARG_WRAP_MODE:
      GTK_VALUE_ENUM (*arg) = text_view->wrap_mode;
      break;

    case ARG_JUSTIFY:
      GTK_VALUE_ENUM (*arg) = text_view->justify;
      break;

    case ARG_LEFT_MARGIN:
      GTK_VALUE_INT (*arg) = text_view->left_margin;
      break;

    case ARG_RIGHT_MARGIN:
      GTK_VALUE_INT (*arg) = text_view->right_margin;
      break;

    case ARG_INDENT:
      GTK_VALUE_INT (*arg) = text_view->indent;
      break;

    case ARG_TABS:
      GTK_VALUE_POINTER (*arg) = gtk_text_view_get_tabs (text_view);
      break;

    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_text_view_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  GtkTextView *text_view;
  GSList *tmp_list;

  text_view = GTK_TEXT_VIEW (widget);

  requisition->width = text_view->text_window->requisition.width + FOCUS_EDGE_WIDTH * 2;
  requisition->height = text_view->text_window->requisition.height + FOCUS_EDGE_WIDTH * 2;

  if (text_view->left_window)
    requisition->width += text_view->left_window->requisition.width;

  if (text_view->right_window)
    requisition->width += text_view->right_window->requisition.width;

  if (text_view->top_window)
    requisition->height += text_view->top_window->requisition.height;

  if (text_view->bottom_window)
    requisition->height += text_view->bottom_window->requisition.height;

  tmp_list = text_view->children;
  while (tmp_list != NULL)
    {
      GtkTextViewChild *child = tmp_list->data;

      if (child->anchor)
        {
          GtkRequisition child_req;
          GtkRequisition old_req;

          old_req = child->widget->requisition;

          gtk_widget_size_request (child->widget, &child_req);

          if (text_view->layout &&
              (old_req.width != child_req.width ||
               old_req.height != child_req.height))
            gtk_text_child_anchor_queue_resize (child->anchor,
                                                text_view->layout);
        }
      else
        {

        }

      tmp_list = g_slist_next (tmp_list);
    }
}

static void
gtk_text_view_update_child_allocation (GtkTextView      *text_view,
                                       GtkTextViewChild *vc)
{
  gint buffer_y;
  GtkTextIter iter;
  GtkAllocation allocation;
  
  gtk_text_buffer_get_iter_at_child_anchor (get_buffer (text_view),
                                            &iter,
                                            vc->anchor);

  gtk_text_layout_get_line_yrange (text_view->layout, &iter,
                                   &buffer_y, NULL);

  buffer_y += vc->from_top_of_line;

  allocation.x = vc->from_left_of_buffer;
  allocation.y = buffer_y;
  allocation.width = vc->widget->requisition.width;
  allocation.height = vc->widget->requisition.height;
  
  gtk_widget_size_allocate (vc->widget, &allocation);
}

static void
gtk_text_view_child_allocated (GtkTextLayout *layout,
                               GtkWidget     *child,
                               gint           x,
                               gint           y,
                               gpointer       data)
{
  GtkTextViewChild *vc = NULL;
  GtkTextView *text_view = data;
  
  /* x,y is the position of the child from the top of the line, and
   * from the left of the buffer. We have to translate that into text
   * window coordinates, then size_allocate the child.
   */

  vc = g_object_get_data (G_OBJECT (child),
                            "gtk-text-view-child");

  g_assert (vc != NULL);

  g_print ("child allocated at %d,%d\n", x, y);
  
  vc->from_left_of_buffer = x;
  vc->from_top_of_line = y;

  gtk_text_view_update_child_allocation (text_view, vc);
}

static void
gtk_text_view_validate_children (GtkTextView *text_view)
{
  GSList *tmp_list;

  DV(g_print(G_STRLOC"\n"));
  
  tmp_list = text_view->children;
  while (tmp_list != NULL)
    {
      GtkTextViewChild *child = tmp_list->data;

      if (child->anchor)
        {
          /* We need to force-validate the regions containing
           * children.
           */
          GtkTextIter child_loc;
          gtk_text_buffer_get_iter_at_child_anchor (get_buffer (text_view),
                                                    &child_loc,
                                                    child->anchor);

          gtk_text_layout_validate_yrange (text_view->layout,
                                           &child_loc,
                                           0, 1);
        }
      else
        {

        }

      tmp_list = g_slist_next (tmp_list);
    }
}

static void
gtk_text_view_size_allocate (GtkWidget *widget,
                             GtkAllocation *allocation)
{
  GtkTextView *text_view;
  GtkTextIter first_para;
  gint y;
  GtkAdjustment *vadj;
  gboolean yoffset_changed = FALSE;
  gint width, height;
  GdkRectangle text_rect;
  GdkRectangle left_rect;
  GdkRectangle right_rect;
  GdkRectangle top_rect;
  GdkRectangle bottom_rect;

  text_view = GTK_TEXT_VIEW (widget);

  DV(g_print(G_STRLOC"\n"));
  
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);
    }

  /* distribute width/height among child windows. Ensure all
   * windows get at least a 1x1 allocation.
   */

  width = allocation->width - FOCUS_EDGE_WIDTH * 2;

  if (text_view->left_window)
    left_rect.width = text_view->left_window->requisition.width;
  else
    left_rect.width = 1;

  width -= left_rect.width;

  if (text_view->right_window)
    right_rect.width = text_view->right_window->requisition.width;
  else
    right_rect.width = 1;

  width -= right_rect.width;

  text_rect.width = MAX (1, width);

  top_rect.width = text_rect.width;
  bottom_rect.width = text_rect.width;


  height = allocation->height - FOCUS_EDGE_WIDTH * 2;

  if (text_view->top_window)
    top_rect.height = text_view->top_window->requisition.height;
  else
    top_rect.height = 1;

  height -= top_rect.height;

  if (text_view->bottom_window)
    bottom_rect.height = text_view->bottom_window->requisition.height;
  else
    bottom_rect.height = 1;

  height -= bottom_rect.height;

  text_rect.height = MAX (1, height);

  left_rect.height = text_rect.height;
  right_rect.height = text_rect.height;

  /* Origins */
  left_rect.x = FOCUS_EDGE_WIDTH;
  top_rect.y = FOCUS_EDGE_WIDTH;

  text_rect.x = left_rect.x + left_rect.width;
  text_rect.y = top_rect.y + top_rect.height;

  left_rect.y = text_rect.y;
  right_rect.y = text_rect.y;

  top_rect.x = text_rect.x;
  bottom_rect.x = text_rect.x;

  right_rect.x = text_rect.x + text_rect.width;
  bottom_rect.y = text_rect.y + text_rect.height;

  text_window_size_allocate (text_view->text_window,
                             &text_rect);

  if (text_view->left_window)
    text_window_size_allocate (text_view->left_window,
                               &left_rect);

  if (text_view->right_window)
    text_window_size_allocate (text_view->right_window,
                               &right_rect);

  if (text_view->top_window)
    text_window_size_allocate (text_view->top_window,
                               &top_rect);

  if (text_view->bottom_window)
    text_window_size_allocate (text_view->bottom_window,
                               &bottom_rect);

  gtk_text_view_update_layout_width (text_view);
  
  /* This is because validating children ends up size allocating them. */
  gtk_text_view_validate_children (text_view);

  /* Now adjust the value of the adjustment to keep the cursor at the
   * same place in the buffer
   */
  gtk_text_view_get_first_para_iter (text_view, &first_para);
  gtk_text_layout_get_line_yrange (text_view->layout, &first_para, &y, NULL);

  y += text_view->first_para_pixels;

  /* Ensure h/v adj exist */
  get_hadjustment (text_view);
  get_vadjustment (text_view);

  vadj = text_view->vadjustment;
  if (y > vadj->upper - vadj->page_size)
    y = MAX (0, vadj->upper - vadj->page_size);

  if (y != text_view->yoffset)
    {
      vadj->value = text_view->yoffset = y;
      yoffset_changed = TRUE;
    }

  text_view->hadjustment->page_size = SCREEN_WIDTH (text_view);
  text_view->hadjustment->page_increment = SCREEN_WIDTH (text_view) / 2;
  text_view->hadjustment->lower = 0;
  text_view->hadjustment->upper = MAX (SCREEN_WIDTH (text_view),
                                       text_view->width);
  gtk_signal_emit_by_name (GTK_OBJECT (text_view->hadjustment), "changed");

  text_view->vadjustment->page_size = SCREEN_HEIGHT (text_view);
  text_view->vadjustment->page_increment = SCREEN_HEIGHT (text_view) / 2;
  text_view->vadjustment->lower = 0;
  text_view->vadjustment->upper = MAX (SCREEN_HEIGHT (text_view),
                                       text_view->height);
  gtk_signal_emit_by_name (GTK_OBJECT (text_view->vadjustment), "changed");

  if (yoffset_changed)
    gtk_adjustment_value_changed (vadj);

  if (text_view->first_validate_idle != 0)
    {
      /* The GTK resize loop processes all the pending exposes right
       * after doing the resize stuff, so the idle sizer won't have a
       * chance to run. So we do the work here. 
       */

      g_source_remove (text_view->first_validate_idle);
      text_view->first_validate_idle = 0;
      
      if (!gtk_text_view_flush_scroll (text_view))
        gtk_text_view_validate_onscreen (text_view);
    }
}

static void
gtk_text_view_get_first_para_iter (GtkTextView *text_view,
                                   GtkTextIter *iter)
{
  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), iter,
                                    text_view->first_para_mark);
}

static void
gtk_text_view_validate_onscreen (GtkTextView *text_view)
{
  GtkWidget *widget = GTK_WIDGET (text_view);
  
  DV(g_print(">Validating onscreen ("G_STRLOC")\n"));
  
  if (SCREEN_HEIGHT (widget) > 0)
    {
      GtkTextIter first_para;

      /* Be sure we've validated the stuff onscreen; if we
       * scrolled, these calls won't have any effect, because
       * they were called in the recursive validate_onscreen
       */
      gtk_text_view_get_first_para_iter (text_view, &first_para);

      gtk_text_layout_validate_yrange (text_view->layout,
                                       &first_para,
                                       0,
                                       text_view->first_para_pixels +
                                       SCREEN_HEIGHT (widget));
    }

  text_view->onscreen_validated = TRUE;

  DV(g_print(">Done validating onscreen, onscreen_validated = TRUE ("G_STRLOC")\n"));
  
  /* This can have the odd side effect of triggering a scroll, which should
   * flip "onscreen_validated" back to FALSE, but should also get us
   * back into this function to turn it on again.
   */
  gtk_text_view_update_adjustments (text_view);

  g_assert (text_view->onscreen_validated);
}

static gboolean
first_validate_callback (gpointer data)
{
  GtkTextView *text_view = data;

  /* Note that some of this code is duplicated at the end of size_allocate,
   * keep in sync with that.
   */
  
  DV(g_print(G_STRLOC"\n"));

  /* Do this first, which means that if an "invalidate"
   * occurs during any of this process, a new first_validate_callback
   * will be installed, and we'll start again.
   */
  text_view->first_validate_idle = 0;
  
  /* be sure we have up-to-date screen size set on the
   * layout.
   */
  gtk_text_view_update_layout_width (text_view);

  /* Bail out if we invalidated stuff; scrolling right away will just
   * confuse the issue.
   */
  if (text_view->first_validate_idle != 0)
    {
      DV(g_print(">Width change forced requeue ("G_STRLOC")\n"));
      return FALSE;
    }
  
  /* scroll to any marks, if that's pending. This can
   * jump us to the validation codepath used for scrolling
   * onscreen, if so we bail out.
   */
  if (!gtk_text_view_flush_scroll (text_view))
    gtk_text_view_validate_onscreen (text_view);

  DV(g_print(">Leaving first validate idle ("G_STRLOC")\n"));

  g_assert (text_view->onscreen_validated);
  
  return FALSE;
}

static gboolean
incremental_validate_callback (gpointer data)
{
  GtkTextView *text_view = data;

  DV(g_print(G_STRLOC"\n"));
  
  gtk_text_layout_validate (text_view->layout, 2000);

  gtk_text_view_update_adjustments (text_view);
  
  if (gtk_text_layout_is_valid (text_view->layout))
    {
      text_view->incremental_validate_idle = 0;
      return FALSE;
    }
  else
    return TRUE;
}

static void
invalidated_handler (GtkTextLayout *layout,
                     gpointer       data)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (data);

  text_view->onscreen_validated = FALSE;
  
  DV(g_print(">Invalidate, onscreen_validated = FALSE ("G_STRLOC")\n"));
  
  if (!text_view->first_validate_idle)
    text_view->first_validate_idle = g_idle_add_full (GTK_PRIORITY_RESIZE - 1, first_validate_callback, text_view, NULL);

  if (!text_view->incremental_validate_idle)
    text_view->incremental_validate_idle = g_idle_add_full (GDK_PRIORITY_REDRAW + 1, incremental_validate_callback, text_view, NULL);
}

static void
changed_handler (GtkTextLayout *layout,
                 gint           start_y,
                 gint           old_height,
                 gint           new_height,
                 gpointer       data)
{
  GtkTextView *text_view;
  GtkWidget *widget;
  GdkRectangle visible_rect;
  GdkRectangle redraw_rect;

  text_view = GTK_TEXT_VIEW (data);
  widget = GTK_WIDGET (data);
  
  DV(g_print(">Lines Validated ("G_STRLOC")\n"));

  if (GTK_WIDGET_REALIZED (text_view))
    {      
      gtk_text_view_get_visible_rect (text_view, &visible_rect);

      redraw_rect.x = visible_rect.x;
      redraw_rect.width = visible_rect.width;
      redraw_rect.y = start_y;

      if (old_height == new_height)
        redraw_rect.height = old_height;
      else
        redraw_rect.height = MAX (0, visible_rect.y + visible_rect.height - start_y);

      if (gdk_rectangle_intersect (&redraw_rect, &visible_rect, &redraw_rect))
        {
          /* text_window_invalidate_rect() takes buffer coordinates */
          text_window_invalidate_rect (text_view->text_window,
                                       &redraw_rect);

          DV(g_print(" invalidated rect: %d,%d %d x %d\n",
                     redraw_rect.x,
                     redraw_rect.y,
                     redraw_rect.width,
                     redraw_rect.height));
          
          if (text_view->left_window)
            text_window_invalidate_rect (text_view->left_window,
                                         &redraw_rect);
          if (text_view->right_window)
            text_window_invalidate_rect (text_view->right_window,
                                         &redraw_rect);
          if (text_view->top_window)
            text_window_invalidate_rect (text_view->top_window,
                                         &redraw_rect);
          if (text_view->bottom_window)
            text_window_invalidate_rect (text_view->bottom_window,
                                         &redraw_rect);
        }
    }
  
  if (old_height != new_height)
    {
      gboolean yoffset_changed = FALSE;
      GSList *tmp_list;
      
      if (start_y + old_height <= text_view->yoffset - text_view->first_para_pixels)
        {
          text_view->yoffset += new_height - old_height;
          get_vadjustment (text_view)->value = text_view->yoffset;
          yoffset_changed = TRUE;
        }

      if (yoffset_changed)
        gtk_adjustment_value_changed (get_vadjustment (text_view));

      /* FIXME be smarter about which anchored widgets we update */

      tmp_list = text_view->children;
      while (tmp_list != NULL)
        {
          GtkTextViewChild *child = tmp_list->data;

          if (child->anchor)
            gtk_text_view_update_child_allocation (text_view, child);

          tmp_list = g_slist_next (tmp_list);
        }
    }
}

static void
gtk_text_view_realize (GtkWidget *widget)
{
  GtkTextView *text_view;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GSList *tmp_list;
  
  text_view = GTK_TEXT_VIEW (widget);
  GTK_WIDGET_SET_FLAGS (text_view, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  /* must come before text_window_realize calls */
  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_background (widget->window,
                             &widget->style->bg[GTK_WIDGET_STATE (widget)]);

  text_window_realize (text_view->text_window, widget->window);

  if (text_view->left_window)
    text_window_realize (text_view->left_window,
                         widget->window);

  if (text_view->top_window)
    text_window_realize (text_view->top_window,
                         widget->window);

  if (text_view->right_window)
    text_window_realize (text_view->right_window,
                         widget->window);

  if (text_view->bottom_window)
    text_window_realize (text_view->bottom_window,
                         widget->window);

  gtk_text_view_ensure_layout (text_view);
}

static void
gtk_text_view_unrealize (GtkWidget *widget)
{
  GtkTextView *text_view;
  GSList *tmp_list;
  
  text_view = GTK_TEXT_VIEW (widget);

  if (text_view->first_validate_idle)
    {
      g_source_remove (text_view->first_validate_idle);
      text_view->first_validate_idle = 0;
    }

  if (text_view->incremental_validate_idle)
    {
      g_source_remove (text_view->incremental_validate_idle);
      text_view->incremental_validate_idle = 0;
    }

  if (text_view->popup_menu)
    {
      gtk_widget_destroy (text_view->popup_menu);
      text_view->popup_menu = NULL;
    }

  text_window_unrealize (text_view->text_window);

  if (text_view->left_window)
    text_window_unrealize (text_view->left_window);

  if (text_view->top_window)
    text_window_unrealize (text_view->top_window);

  if (text_view->right_window)
    text_window_unrealize (text_view->right_window);

  if (text_view->bottom_window)
    text_window_unrealize (text_view->bottom_window);

  gtk_text_view_destroy_layout (text_view);
  
  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_text_view_style_set (GtkWidget *widget,
                         GtkStyle  *previous_style)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_set_background (widget->window,
                                 &widget->style->bg[GTK_WIDGET_STATE (widget)]);

      gdk_window_set_background (text_view->text_window->bin_window,
                                 &widget->style->base[GTK_WIDGET_STATE (widget)]);

      if (text_view->left_window)
        gdk_window_set_background (text_view->left_window->bin_window,
                                   &widget->style->bg[GTK_WIDGET_STATE (widget)]);
      if (text_view->right_window)
        gdk_window_set_background (text_view->right_window->bin_window,
                                   &widget->style->bg[GTK_WIDGET_STATE (widget)]);

      if (text_view->top_window)
        gdk_window_set_background (text_view->top_window->bin_window,
                                   &widget->style->bg[GTK_WIDGET_STATE (widget)]);

      if (text_view->bottom_window)
        gdk_window_set_background (text_view->bottom_window->bin_window,
                                   &widget->style->bg[GTK_WIDGET_STATE (widget)]);

      gtk_text_view_set_attributes_from_style (text_view,
                                               text_view->layout->default_style,
                                               widget->style);
      gtk_text_layout_default_style_changed (text_view->layout);
    }
}

static void
gtk_text_view_direction_changed (GtkWidget        *widget,
                                 GtkTextDirection  previous_direction)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  if (text_view->layout)
    {
      text_view->layout->default_style->direction = gtk_widget_get_direction (widget);
      gtk_text_layout_default_style_changed (text_view->layout);
    }
}

/*
 * Events
 */

static gboolean
get_event_coordinates (GdkEvent *event, gint *x, gint *y)
{
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
        *x = event->motion.x;
        *y = event->motion.y;
        return TRUE;
        break;

      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
        *x = event->button.x;
        *y = event->button.y;
        return TRUE;
        break;

      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
      case GDK_PROPERTY_NOTIFY:
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
      case GDK_DRAG_ENTER:
      case GDK_DRAG_LEAVE:
      case GDK_DRAG_MOTION:
      case GDK_DRAG_STATUS:
      case GDK_DROP_START:
      case GDK_DROP_FINISHED:
      default:
        return FALSE;
        break;
      }

  return FALSE;
}

static gint
emit_event_on_tags (GtkWidget   *widget,
                    GdkEvent    *event,
                    GtkTextIter *iter)
{
  GSList *tags;
  GSList *tmp;
  gint retval = FALSE;
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  tags = gtk_text_iter_get_tags (iter);

  tmp = tags;
  while (tmp != NULL)
    {
      GtkTextTag *tag = tmp->data;

      if (gtk_text_tag_event (tag, G_OBJECT (widget), event, iter))
        {
          retval = TRUE;
          break;
        }

      tmp = g_slist_next (tmp);
    }

  g_slist_free (tags);

  return retval;
}

static gint
gtk_text_view_event (GtkWidget *widget, GdkEvent *event)
{
  GtkTextView *text_view;
  gint x = 0, y = 0;

  text_view = GTK_TEXT_VIEW (widget);

  if (text_view->layout == NULL ||
      get_buffer (text_view) == NULL)
    return FALSE;

  if (event->any.window != text_view->text_window->bin_window)
    return FALSE;

  if (get_event_coordinates (event, &x, &y))
    {
      GtkTextIter iter;

      x += text_view->xoffset;
      y += text_view->yoffset;

      /* FIXME this is slow and we do it twice per event.
       * My favorite solution is to have GtkTextLayout cache
       * the last couple lookups.
       */
      gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                         &iter,
                                         x, y);

      return emit_event_on_tags (widget, event, &iter);
    }
  else if (event->type == GDK_KEY_PRESS ||
           event->type == GDK_KEY_RELEASE)
    {
      GtkTextMark *insert;
      GtkTextIter iter;

      insert = gtk_text_buffer_get_mark (get_buffer (text_view),
                                         "insert");

      gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &iter, insert);

      return emit_event_on_tags (widget, event, &iter);
    }
  else
    return FALSE;
}

static gint
gtk_text_view_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  gint retval = FALSE;
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  if (text_view->layout == NULL ||
      get_buffer (text_view) == NULL)
    return FALSE;

  if (gtk_im_context_filter_keypress (text_view->im_context, event))
    {
      text_view->need_im_reset = TRUE;
      retval = TRUE;
    }
  else if (GTK_WIDGET_CLASS (parent_class)->key_press_event &&
 	   GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event))
    retval = TRUE;
  else if (event->keyval == GDK_Return)
    {
      gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view), "\n", 1,
                                                    text_view->editable);
      gtk_text_view_scroll_to_mark (text_view,
                                    gtk_text_buffer_get_mark (get_buffer (text_view),
                                                              "insert"),
                                    0.0, FALSE, 0.0, 0.0);
      retval = TRUE;
    }
  /* Pass through Tab as literal tab, unless Control is held down */
  else if (event->keyval == GDK_Tab && !(event->state & GDK_CONTROL_MASK))
    {
      gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view), "\t", 1,
                                                    text_view->editable);
      gtk_text_view_scroll_mark_onscreen (text_view,
                                          gtk_text_buffer_get_mark (get_buffer (text_view),
                                                                    "insert"));
      retval = TRUE;
    }
  else
    retval = FALSE;

  gtk_text_view_start_cursor_blink (text_view, TRUE);

  return retval;
}

static gint
gtk_text_view_key_release_event (GtkWidget *widget, GdkEventKey *event)
{
  return FALSE;
}

static gint
gtk_text_view_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  gtk_widget_grab_focus (widget);

  if (event->window != text_view->text_window->bin_window)
    {
      /* Remove selection if any. */
      gtk_text_view_unselect (text_view);
      return FALSE;
    }

#if 0
  /* debug hack */
  if (event->button == 3 && (event->state & GDK_CONTROL_MASK) != 0)
    _gtk_text_buffer_spew (GTK_TEXT_VIEW (widget)->buffer);
  else if (event->button == 3)
    gtk_text_layout_spew (GTK_TEXT_VIEW (widget)->layout);
#endif

  if (event->type == GDK_BUTTON_PRESS)
    {
      gtk_text_view_reset_im_context (text_view);

      if (event->button == 1)
        {
          /* If we're in the selection, start a drag copy/move of the
           * selection; otherwise, start creating a new selection.
           */
          GtkTextIter iter;
          GtkTextIter start, end;

          gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                             &iter,
                                             event->x + text_view->xoffset,
                                             event->y + text_view->yoffset);

          if (gtk_text_buffer_get_selection_bounds (get_buffer (text_view),
                                                    &start, &end) &&
              gtk_text_iter_in_range (&iter, &start, &end))
            {
              text_view->drag_start_x = event->x;
              text_view->drag_start_y = event->y;
            }
          else
            {
              gtk_text_view_start_selection_drag (text_view, &iter, event);
            }

          return TRUE;
        }
      else if (event->button == 2)
        {
          GtkTextIter iter;

          gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                             &iter,
                                             event->x + text_view->xoffset,
                                             event->y + text_view->yoffset);

          gtk_text_buffer_paste_primary (get_buffer (text_view),
                                         &iter,
                                         text_view->editable);
          return TRUE;
        }
      else if (event->button == 3)
        {
	  gtk_text_view_popup_menu (text_view, event);
        }
    }
  else if ((event->type == GDK_2BUTTON_PRESS ||
            event->type == GDK_3BUTTON_PRESS) &&
           event->button == 1)
    {
      GtkTextIter start, end;

      /* End the selection drag, otherwise we'd clear the new
       * word/line selection on button release
       */
      gtk_text_view_end_selection_drag (text_view, event);

      gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                         &start,
                                         event->x + text_view->xoffset,
                                         event->y + text_view->yoffset); 

      end = start;
      
      if (event->type == GDK_2BUTTON_PRESS)
        {
          if (gtk_text_iter_inside_word (&start))
            {
              if (!gtk_text_iter_starts_word (&start))
                gtk_text_iter_backward_word_start (&start);
              
              if (!gtk_text_iter_ends_word (&end))
                gtk_text_iter_forward_word_end (&end);
            }
        }
      else if (event->type == GDK_3BUTTON_PRESS)
        {
          if (gtk_text_view_starts_display_line (text_view, &start))
            {
              /* If on a display line boundary, we assume the user
               * clicked off the end of a line and we therefore select
               * the line before the boundary.
               */
              gtk_text_view_backward_display_line_start (text_view, &start);
            }
          else
            {
              /* start isn't on the start of a line, so we move it to the
               * start, and move end to the end unless it's already there.
               */
              gtk_text_view_backward_display_line_start (text_view, &start);

              if (!gtk_text_view_starts_display_line (text_view, &end))
                gtk_text_view_forward_display_line_end (text_view, &end);
            }
        }

      gtk_text_buffer_move_mark (get_buffer (text_view),
                                 gtk_text_buffer_get_selection_bound (get_buffer (text_view)),
                                 &start);
      gtk_text_buffer_move_mark (get_buffer (text_view),
                                 gtk_text_buffer_get_insert (get_buffer (text_view)),
                                 &end);

      text_view->just_selected_element = TRUE;
      
      return TRUE;
    }
  
  return FALSE;
}

static gint
gtk_text_view_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  if (event->window != text_view->text_window->bin_window)
    return FALSE;

  if (event->button == 1)
    {
      if (text_view->drag_start_x >= 0)
        {
          text_view->drag_start_x = -1;
          text_view->drag_start_y = -1;
        }

      if (gtk_text_view_end_selection_drag (GTK_TEXT_VIEW (widget), event))
        return TRUE;
      else if (text_view->just_selected_element)
        {
          text_view->just_selected_element = FALSE;
          return FALSE;
        }
      else
        {
          /* Unselect everything; probably we were dragging, or clicked
           * outside the text.
           */
          gtk_text_view_unselect (text_view);
          return FALSE;
        }
    }

  return FALSE;
}

static gint
gtk_text_view_focus_in_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_queue_draw (widget);

  if (text_view->cursor_visible && text_view->layout)
    {
      gtk_text_layout_set_cursor_visible (text_view->layout, TRUE);
      gtk_text_view_start_cursor_blink (text_view, FALSE);
    }

  text_view->need_im_reset = TRUE;
  gtk_im_context_focus_in (GTK_TEXT_VIEW (widget)->im_context);

  return FALSE;
}

static gint
gtk_text_view_focus_out_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_queue_draw (widget);

  if (text_view->cursor_visible && text_view->layout)
    {
      gtk_text_layout_set_cursor_visible (text_view->layout, FALSE);
      gtk_text_view_stop_cursor_blink (text_view);
    }

  text_view->need_im_reset = TRUE;
  gtk_im_context_focus_out (GTK_TEXT_VIEW (widget)->im_context);

  return FALSE;
}

static gint
gtk_text_view_motion_event (GtkWidget *widget, GdkEventMotion *event)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  if (event->window == text_view->text_window->bin_window &&
      text_view->drag_start_x >= 0)
    {
      gint x, y;

      gdk_window_get_pointer (text_view->text_window->bin_window,
                              &x, &y, NULL);

      if (gtk_drag_check_threshold (widget,
				    text_view->drag_start_x, 
				    text_view->drag_start_y,
				    x, y))
        {
          GtkTextIter iter;
          gint buffer_x, buffer_y;

          gtk_text_view_window_to_buffer_coords (text_view,
                                                 GTK_TEXT_WINDOW_TEXT,
                                                 text_view->drag_start_x,
                                                 text_view->drag_start_y,
                                                 &buffer_x,
                                                 &buffer_y);

          gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                             &iter,
                                             buffer_x, buffer_y);

          gtk_text_view_start_selection_dnd (text_view, &iter, event);
          return TRUE;
        }
    }

  return FALSE;
}

static void
gtk_text_view_paint (GtkWidget *widget, GdkRectangle *area)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  g_return_if_fail (text_view->layout != NULL);
  g_return_if_fail (text_view->xoffset >= 0);
  g_return_if_fail (text_view->yoffset >= 0);
  
  if (!text_view->onscreen_validated)
    {
      G_BREAKPOINT ();
      g_warning (G_STRLOC ": somehow some text lines were modified or scrolling occurred since the last validation of lines on the screen");
    }
  
#if 0
  printf ("painting %d,%d  %d x %d\n",
          area->x, area->y,
          area->width, area->height);
#endif
  
  gtk_text_layout_draw (text_view->layout,
                        widget,
                        text_view->text_window->bin_window,
                        text_view->xoffset,
                        text_view->yoffset,
                        area->x, area->y,
                        area->width, area->height);
}

static gint
gtk_text_view_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
#if 0
  {
    GdkWindow *win = event->window;
    GdkColor color = { 0, 0, 0, 65535 };
    GdkGC *gc = gdk_gc_new (win);
    gdk_gc_set_rgb_fg_color (gc, &color);
    gdk_draw_rectangle (win,
                        gc, TRUE,
                        event->area.x, event->area.y,
                        event->area.width, event->area.height);
    gdk_gc_unref (gc);
  }
#endif
  
  if (event->window == gtk_text_view_get_window (GTK_TEXT_VIEW (widget),
                                                 GTK_TEXT_WINDOW_TEXT))
    {
      DV(g_print (">Exposed ("G_STRLOC")\n"));
      gtk_text_view_paint (widget, &event->area);
    }

  if (event->window == widget->window)
    gtk_text_view_draw_focus (widget);

  return TRUE;
}

static void
gtk_text_view_draw_focus (GtkWidget *widget)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
        {
          gtk_paint_focus (widget->style, widget->window,
                           NULL, widget, "textview",
                           0, 0,
                           widget->allocation.width - 1,
                           widget->allocation.height - 1);
        }
      else
        {
          gdk_window_clear (widget->window);
        }
    }
}

/*
 * Container
 */

static void
gtk_text_view_add (GtkContainer *container,
                   GtkWidget    *child)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (container));
  g_return_if_fail (GTK_IS_WIDGET (child));

  /* This is pretty random. */
  gtk_text_view_add_child_in_window (GTK_TEXT_VIEW (container),
                                     child,
                                     GTK_TEXT_WINDOW_WIDGET,
                                     0, 0);
}

static void
gtk_text_view_remove (GtkContainer *container,
                      GtkWidget    *child)
{
  GSList *iter;
  GtkTextView *text_view;
  GtkTextViewChild *vc;

  g_return_if_fail (GTK_IS_TEXT_VIEW (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == (GtkWidget*) container);

  text_view = GTK_TEXT_VIEW (container);

  vc = NULL;
  iter = text_view->children;

  while (iter != NULL)
    {
      vc = iter->data;

      if (vc->widget == child)
        break;

      iter = g_slist_next (iter);
    }

  g_assert (iter != NULL); /* be sure we had the child in the list */

  text_view->children = g_slist_remove (text_view->children, vc);

  gtk_widget_unparent (vc->widget);

  text_view_child_free (vc);
}

static void
gtk_text_view_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  GSList *iter;
  GtkTextView *text_view;

  g_return_if_fail (GTK_IS_TEXT_VIEW (container));
  g_return_if_fail (callback != NULL);

  text_view = GTK_TEXT_VIEW (container);

  iter = text_view->children;

  while (iter != NULL)
    {
      GtkTextViewChild *vc = iter->data;

      (* callback) (vc->widget, callback_data);

      iter = g_slist_next (iter);
    }
}

/* Note that CURSOR_ON_TIME is effectively added to PREBLINK_TIME
 * because blinking starts with the cursor turned on.
 */
#define PREBLINK_TIME 300
#define CURSOR_ON_TIME 800
#define CURSOR_OFF_TIME 400

/*
 * preblink!
 */

static gint
preblink_cb (gpointer data)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (data);

  text_view->preblink_timeout = 0;
  gtk_text_view_start_cursor_blink (text_view, FALSE);

  /* Remove ourselves */
  return FALSE;
}

/*
 * Blink!
 */

static gint
blink_cb (gpointer data)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (data);
  gboolean visible;
  
  g_assert (text_view->layout);
  g_assert (GTK_WIDGET_HAS_FOCUS (text_view));
  g_assert (text_view->cursor_visible);

  visible = gtk_text_layout_get_cursor_visible (text_view->layout);

  if (visible)
    text_view->blink_timeout = gtk_timeout_add (CURSOR_OFF_TIME,
                                                blink_cb,
                                                text_view);
  else
    text_view->blink_timeout = gtk_timeout_add (CURSOR_ON_TIME,
                                                blink_cb,
                                                text_view);
  
  gtk_text_layout_set_cursor_visible (text_view->layout,
                                      !visible);

  /* Remove ourselves */
  return FALSE;
}

static void
gtk_text_view_start_cursor_blink(GtkTextView *text_view,
                                 gboolean     with_delay)
{
#ifdef DEBUG_VALIDATION_AND_SCROLLING
  return;
#endif

  if (text_view->layout == NULL)
    return;
  
  if (!text_view->cursor_visible)
    return;

  if (!GTK_WIDGET_HAS_FOCUS (text_view))
    return;
  
  if (text_view->preblink_timeout != 0)
    {
      gtk_timeout_remove (text_view->preblink_timeout);
      text_view->preblink_timeout = 0;
    }
  
  if (with_delay)
    {
      if (text_view->blink_timeout != 0)
        {
          gtk_timeout_remove (text_view->blink_timeout);
          text_view->blink_timeout = 0;
        }
      
      gtk_text_layout_set_cursor_visible (text_view->layout, TRUE);
      
      text_view->preblink_timeout = gtk_timeout_add (PREBLINK_TIME,
                                                     preblink_cb,
                                                     text_view);
    }
  else
    {
      if (text_view->blink_timeout == 0)
        {
          gtk_text_layout_set_cursor_visible (text_view->layout, TRUE);
          
          text_view->blink_timeout = gtk_timeout_add (CURSOR_ON_TIME,
                                                      blink_cb,
                                                      text_view);
        }
    }
}

static void
gtk_text_view_stop_cursor_blink (GtkTextView *text_view)
{
  if (text_view->preblink_timeout)
    {
      gtk_timeout_remove (text_view->preblink_timeout);
      text_view->preblink_timeout = 0;
    }

  if (text_view->blink_timeout)  
    { 
      gtk_timeout_remove (text_view->blink_timeout);
      text_view->blink_timeout = 0;
    }
}

/*
 * Key binding handlers
 */

static void
gtk_text_view_move_iter_by_lines (GtkTextView *text_view,
                                  GtkTextIter *newplace,
                                  gint         count)
{
  while (count < 0)
    {
      gtk_text_layout_move_iter_to_previous_line (text_view->layout, newplace);
      count++;
    }

  while (count > 0)
    {
      gtk_text_layout_move_iter_to_next_line (text_view->layout, newplace);
      count--;
    }
}

static void
gtk_text_view_move_cursor (GtkTextView     *text_view,
                           GtkMovementStep  step,
                           gint             count,
                           gboolean         extend_selection)
{
  GtkTextIter insert;
  GtkTextIter newplace;

  gint cursor_x_pos = 0;

  gtk_text_view_reset_im_context (text_view);

  if (step == GTK_MOVEMENT_PAGES)
    {
      gtk_text_view_scroll_pages (text_view, count);
      gtk_text_view_start_cursor_blink (text_view, TRUE);
      return;
    }

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_mark (get_buffer (text_view),
                                                              "insert"));
  newplace = insert;

  if (step == GTK_MOVEMENT_DISPLAY_LINES)
    gtk_text_view_get_virtual_cursor_pos (text_view, &cursor_x_pos, NULL);

  switch (step)
    {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
      gtk_text_iter_forward_cursor_positions (&newplace, count);
      break;

    case GTK_MOVEMENT_VISUAL_POSITIONS:
      gtk_text_layout_move_iter_visually (text_view->layout,
                                          &newplace, count);
      break;

    case GTK_MOVEMENT_WORDS:
      if (count < 0)
        gtk_text_iter_backward_word_starts (&newplace, -count);
      else if (count > 0)
        gtk_text_iter_forward_word_ends (&newplace, count);
      break;

    case GTK_MOVEMENT_DISPLAY_LINES:
      gtk_text_view_move_iter_by_lines (text_view, &newplace, count);
      gtk_text_layout_move_iter_to_x (text_view->layout, &newplace, cursor_x_pos);
      break;

    case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
      if (count > 1)
        gtk_text_view_move_iter_by_lines (text_view, &newplace, --count);
      else if (count < -1)
        gtk_text_view_move_iter_by_lines (text_view, &newplace, ++count);

      if (count != 0)
        gtk_text_layout_move_iter_to_line_end (text_view->layout, &newplace, count);
      break;

    case GTK_MOVEMENT_PARAGRAPHS:
      /* This should almost certainly instead be doing the parallel thing to WORD */
      /*       gtk_text_iter_down_lines (&newplace, count); */
      /* FIXME */
      break;

    case GTK_MOVEMENT_PARAGRAPH_ENDS:
      if (count > 0)
        gtk_text_iter_forward_to_line_end (&newplace);
      else if (count < 0)
        gtk_text_iter_set_line_offset (&newplace, 0);
      break;

    case GTK_MOVEMENT_BUFFER_ENDS:
      if (count > 0)
        gtk_text_buffer_get_end_iter (get_buffer (text_view), &newplace);
      else if (count < 0)
        gtk_text_buffer_get_iter_at_offset (get_buffer (text_view), &newplace, 0);
      break;

    default:
      break;
    }

  if (!gtk_text_iter_equal (&insert, &newplace))
    {
      if (extend_selection)
        gtk_text_buffer_move_mark (get_buffer (text_view),
                                   gtk_text_buffer_get_mark (get_buffer (text_view),
                                                             "insert"),
                                   &newplace);
      else
        gtk_text_buffer_place_cursor (get_buffer (text_view), &newplace);

      gtk_text_view_scroll_mark_onscreen (text_view,
                                          gtk_text_buffer_get_mark (get_buffer (text_view),
                                                                    "insert"));

      if (step == GTK_MOVEMENT_DISPLAY_LINES)
        {
          gtk_text_view_set_virtual_cursor_pos (text_view, cursor_x_pos, -1);
        }
    }

  gtk_text_view_start_cursor_blink (text_view, TRUE);
}

static void
gtk_text_view_set_anchor (GtkTextView *text_view)
{
  GtkTextIter insert;

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_mark (get_buffer (text_view),
                                                              "insert"));

  gtk_text_buffer_create_mark (get_buffer (text_view), "anchor", &insert, TRUE);
}

static void
gtk_text_view_scroll_pages (GtkTextView *text_view,
                            gint         count)
{
  gfloat newval;
  GtkAdjustment *adj;
  gint cursor_x_pos, cursor_y_pos;
  GtkTextIter new_insert;
  GtkTextIter anchor;
  gint y0, y1;

  g_return_if_fail (text_view->vadjustment != NULL);

  adj = text_view->vadjustment;

  /* Validate the region that will be brought into view by the cursor motion
   */
  if (count < 0)
    {
      gtk_text_view_get_first_para_iter (text_view, &anchor);
      y0 = adj->page_size;
      y1 = adj->page_size + count * adj->page_increment;
    }
  else
    {
      gtk_text_view_get_first_para_iter (text_view, &anchor);
      y0 = count * adj->page_increment + adj->page_size;
      y1 = 0;
    }

  gtk_text_layout_validate_yrange (text_view->layout, &anchor, y0, y1);

  gtk_text_view_get_virtual_cursor_pos (text_view, &cursor_x_pos, &cursor_y_pos);

  newval = adj->value;

  newval += count * adj->page_increment;

  cursor_y_pos += newval - adj->value;
  set_adjustment_clamped (adj, newval);

  gtk_text_layout_get_iter_at_pixel (text_view->layout, &new_insert, cursor_x_pos, cursor_y_pos);
  clamp_iter_onscreen (text_view, &new_insert);
  gtk_text_buffer_place_cursor (get_buffer (text_view), &new_insert);

  gtk_text_view_set_virtual_cursor_pos (text_view, cursor_x_pos, cursor_y_pos);

  /* Adjust to have the cursor _entirely_ onscreen, move_mark_onscreen
   * only guarantees 1 pixel onscreen.
   */
  gtk_text_view_scroll_mark_onscreen (text_view,
                                      gtk_text_buffer_get_mark (get_buffer (text_view),
                                                                "insert"));
}

static gboolean
whitespace (gunichar ch, gpointer user_data)
{
  return (ch == ' ' || ch == '\t');
}

static gboolean
not_whitespace (gunichar ch, gpointer user_data)
{
  return !whitespace (ch, user_data);
}

static gboolean
find_whitepace_region (const GtkTextIter *center,
                       GtkTextIter *start, GtkTextIter *end)
{
  *start = *center;
  *end = *center;

  if (gtk_text_iter_backward_find_char (start, not_whitespace, NULL, NULL))
    gtk_text_iter_forward_char (start); /* we want the first whitespace... */
  if (whitespace (gtk_text_iter_get_char (end), NULL))
    gtk_text_iter_forward_find_char (end, not_whitespace, NULL, NULL);

  return !gtk_text_iter_equal (start, end);
}

static void
gtk_text_view_insert_at_cursor (GtkTextView *text_view,
                                const gchar *str)
{
  gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view), str, -1,
                                                text_view->editable);
}

static void
gtk_text_view_delete_from_cursor (GtkTextView   *text_view,
                                  GtkDeleteType  type,
                                  gint           count)
{
  GtkTextIter insert;
  GtkTextIter start;
  GtkTextIter end;
  gboolean leave_one = FALSE;

  gtk_text_view_reset_im_context (text_view);

  if (type == GTK_DELETE_CHARS)
    {
      /* Char delete deletes the selection, if one exists */
      if (gtk_text_buffer_delete_selection (get_buffer (text_view), TRUE,
                                            text_view->editable))
        return;
    }

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view),
                                    &insert,
                                    gtk_text_buffer_get_mark (get_buffer (text_view),
                                                              "insert"));

  start = insert;
  end = insert;

  switch (type)
    {
    case GTK_DELETE_CHARS:
      gtk_text_iter_forward_cursor_positions (&end, count);
      break;

    case GTK_DELETE_WORD_ENDS:
      if (count > 0)
        gtk_text_iter_forward_word_ends (&end, count);
      else if (count < 0)
        gtk_text_iter_backward_word_starts (&start, 0 - count);
      break;

    case GTK_DELETE_WORDS:
      break;

    case GTK_DELETE_DISPLAY_LINE_ENDS:
      break;

    case GTK_DELETE_DISPLAY_LINES:
      break;

    case GTK_DELETE_PARAGRAPH_ENDS:
      /* If we're already at a newline, we need to
       * simply delete that newline, instead of
       * moving to the next one.
       */
      if (gtk_text_iter_ends_line (&end))
        {
          gtk_text_iter_forward_line (&end);
          --count;
        }

      while (count > 0)
        {
          if (!gtk_text_iter_forward_to_line_end (&end))
            break;

          --count;
        }

      /* FIXME figure out what a negative count means
         and support that */
      break;

    case GTK_DELETE_PARAGRAPHS:
      if (count > 0)
        {
          gtk_text_iter_set_line_offset (&start, 0);
          gtk_text_iter_forward_to_line_end (&end);

          /* Do the lines beyond the first. */
          while (count > 1)
            {
              gtk_text_iter_forward_to_line_end (&end);

              --count;
            }
        }

      /* FIXME negative count? */

      break;

    case GTK_DELETE_WHITESPACE:
      {
        find_whitepace_region (&insert, &start, &end);
      }
      break;

    default:
      break;
    }

  if (!gtk_text_iter_equal (&start, &end))
    {
      gtk_text_buffer_begin_user_action (get_buffer (text_view));

      if (gtk_text_buffer_delete_interactive (get_buffer (text_view), &start, &end,
                                              text_view->editable))
        {
          if (leave_one)
            gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view),
                                                          " ", 1,
                                                          text_view->editable);
        }

      gtk_text_buffer_end_user_action (get_buffer (text_view));
      
      gtk_text_view_scroll_mark_onscreen (text_view,
                                          gtk_text_buffer_get_mark (get_buffer (text_view), "insert"));
    }
}

static void
gtk_text_view_cut_clipboard (GtkTextView *text_view)
{
  gtk_text_buffer_cut_clipboard (get_buffer (text_view), text_view->editable);
  gtk_text_view_scroll_mark_onscreen (text_view,
                                      gtk_text_buffer_get_mark (get_buffer (text_view),
                                                                "insert"));
}

static void
gtk_text_view_copy_clipboard (GtkTextView *text_view)
{
  gtk_text_buffer_copy_clipboard (get_buffer (text_view));
  gtk_text_view_scroll_mark_onscreen (text_view,
                                      gtk_text_buffer_get_mark (get_buffer (text_view),
                                                                "insert"));
}

static void
gtk_text_view_paste_clipboard (GtkTextView *text_view)
{
  gtk_text_buffer_paste_clipboard (get_buffer (text_view), text_view->editable);
  gtk_text_view_scroll_mark_onscreen (text_view,
                                      gtk_text_buffer_get_mark (get_buffer (text_view),
                                                                "insert"));
}

static void
gtk_text_view_toggle_overwrite (GtkTextView *text_view)
{
  text_view->overwrite_mode = !text_view->overwrite_mode;
}

/*
 * Selections
 */

static void
gtk_text_view_unselect (GtkTextView *text_view)
{
  GtkTextIter insert;

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view),
                                    &insert,
                                    gtk_text_buffer_get_mark (get_buffer (text_view),
                                                              "insert"));

  gtk_text_buffer_move_mark (get_buffer (text_view),
                             gtk_text_buffer_get_mark (get_buffer (text_view),
                                                       "selection_bound"),
                             &insert);
}

static void
move_mark_to_pointer_and_scroll (GtkTextView *text_view,
                                 const gchar *mark_name)
{
  gint x, y;
  GdkModifierType state;
  GtkTextIter newplace;

  gdk_window_get_pointer (text_view->text_window->bin_window,
                          &x, &y, &state);
  
  gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                     &newplace,
                                     x + text_view->xoffset,
                                     y + text_view->yoffset);

  {
    GtkTextMark *mark =
      gtk_text_buffer_get_mark (get_buffer (text_view), mark_name);

    gtk_text_buffer_move_mark (get_buffer (text_view),
                               mark,
                               &newplace);

    gtk_text_view_scroll_mark_onscreen (text_view, mark);
  }
}

static gint
selection_scan_timeout (gpointer data)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (data);

  move_mark_to_pointer_and_scroll (text_view, "insert");

  return TRUE; /* remain installed. */
}

#define DND_SCROLL_MARGIN 0.20

static gint
drag_scan_timeout (gpointer data)
{
  GtkTextView *text_view;
  gint x, y;
  GdkModifierType state;
  GtkTextIter newplace;
  
  text_view = GTK_TEXT_VIEW (data);

  gdk_window_get_pointer (text_view->text_window->bin_window,
                          &x, &y, &state);
  
  gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                     &newplace,
                                     x + text_view->xoffset,
                                     y + text_view->yoffset);
  
  gtk_text_buffer_move_mark (get_buffer (text_view),
                             text_view->dnd_mark,
                             &newplace);
  
  gtk_text_view_scroll_to_mark (text_view,
                                text_view->dnd_mark,
                                DND_SCROLL_MARGIN, FALSE, 0.0, 0.0);

  return TRUE;
}

static gint
selection_motion_event_handler (GtkTextView *text_view, GdkEventMotion *event, gpointer data)
{
  move_mark_to_pointer_and_scroll (text_view, "insert");

  /* If we had to scroll offscreen, insert a timeout to do so
   * again. Note that in the timeout, even if the mouse doesn't
   * move, due to this scroll xoffset/yoffset will have changed
   * and we'll need to scroll again.
   */
  if (text_view->scroll_timeout != 0) /* reset on every motion event */
    gtk_timeout_remove (text_view->scroll_timeout);
  
  text_view->scroll_timeout =
    gtk_timeout_add (50, selection_scan_timeout, text_view);

  return TRUE;
}

static void
gtk_text_view_start_selection_drag (GtkTextView       *text_view,
                                    const GtkTextIter *iter,
                                    GdkEventButton    *button)
{
  GtkTextIter newplace;

  g_return_if_fail (text_view->selection_drag_handler == 0);

  gtk_grab_add (GTK_WIDGET (text_view));

  newplace = *iter;

  gtk_text_buffer_place_cursor (get_buffer (text_view), &newplace);

  text_view->selection_drag_handler = gtk_signal_connect (GTK_OBJECT (text_view),
                                                          "motion_notify_event",
                                                          GTK_SIGNAL_FUNC (selection_motion_event_handler),
                                                          NULL);
}

/* returns whether we were really dragging */
static gboolean
gtk_text_view_end_selection_drag (GtkTextView *text_view, GdkEventButton *event)
{
  if (text_view->selection_drag_handler == 0)
    return FALSE;

  gtk_signal_disconnect (GTK_OBJECT (text_view), text_view->selection_drag_handler);
  text_view->selection_drag_handler = 0;

  if (text_view->scroll_timeout != 0)
    {
      gtk_timeout_remove (text_view->scroll_timeout);
      text_view->scroll_timeout = 0;
    }

  /* one last update to current position */
  move_mark_to_pointer_and_scroll (text_view, "insert");

  gtk_grab_remove (GTK_WIDGET (text_view));

  return TRUE;
}

/*
 * Layout utils
 */

static void
gtk_text_view_set_attributes_from_style (GtkTextView        *text_view,
                                         GtkTextAttributes *values,
                                         GtkStyle           *style)
{
  values->appearance.bg_color = style->base[GTK_STATE_NORMAL];
  values->appearance.fg_color = style->fg[GTK_STATE_NORMAL];

  if (values->font.family_name)
    g_free (values->font.family_name);

  values->font = *style->font_desc;
  values->font.family_name = g_strdup (style->font_desc->family_name);
}

static void
gtk_text_view_ensure_layout (GtkTextView *text_view)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (text_view);

  if (text_view->layout == NULL)
    {
      GtkTextAttributes *style;
      PangoContext *ltr_context, *rtl_context;
      GSList *tmp_list;

      DV(g_print(G_STRLOC"\n"));
      
      text_view->layout = gtk_text_layout_new ();

      g_signal_connect_data (G_OBJECT (text_view->layout),
                             "invalidated",
                             invalidated_handler,
                             text_view,
                             NULL, FALSE, FALSE);

      g_signal_connect_data (G_OBJECT (text_view->layout),
                             "changed",
                             changed_handler,
                             text_view,
                             NULL, FALSE, FALSE);

      g_signal_connect_data (G_OBJECT (text_view->layout),
                             "allocate_child",
                             gtk_text_view_child_allocated,
                             text_view,
                             NULL, FALSE, FALSE);
      
      if (get_buffer (text_view))
        gtk_text_layout_set_buffer (text_view->layout, get_buffer (text_view));

      if ((GTK_WIDGET_HAS_FOCUS (text_view) && text_view->cursor_visible))
        gtk_text_view_start_cursor_blink (text_view, FALSE);
      else
        gtk_text_layout_set_cursor_visible (text_view->layout, FALSE);

      ltr_context = gtk_widget_create_pango_context (GTK_WIDGET (text_view));
      pango_context_set_base_dir (ltr_context, PANGO_DIRECTION_LTR);
      rtl_context = gtk_widget_create_pango_context (GTK_WIDGET (text_view));
      pango_context_set_base_dir (rtl_context, PANGO_DIRECTION_RTL);

      gtk_text_layout_set_contexts (text_view->layout, ltr_context, rtl_context);

      g_object_unref (G_OBJECT (ltr_context));
      g_object_unref (G_OBJECT (rtl_context));

      style = gtk_text_attributes_new ();

      gtk_widget_ensure_style (widget);
      gtk_text_view_set_attributes_from_style (text_view,
                                               style, widget->style);

      style->pixels_above_lines = text_view->pixels_above_lines;
      style->pixels_below_lines = text_view->pixels_below_lines;
      style->pixels_inside_wrap = text_view->pixels_inside_wrap;
      style->left_margin = text_view->left_margin;
      style->right_margin = text_view->right_margin;
      style->indent = text_view->indent;
      style->tabs = text_view->tabs ? pango_tab_array_copy (text_view->tabs) : NULL;

      style->wrap_mode = text_view->wrap_mode;
      style->justification = text_view->justify;
      style->direction = gtk_widget_get_direction (GTK_WIDGET (text_view));

      gtk_text_layout_set_default_style (text_view->layout, style);

      gtk_text_attributes_unref (style);

      /* Set layout for all anchored children */

      tmp_list = text_view->children;
      while (tmp_list != NULL)
        {
          GtkTextViewChild *vc = tmp_list->data;

          if (vc->anchor)
            {
              gtk_text_anchored_child_set_layout (vc->widget,
                                                  text_view->layout);
              /* vc may now be invalid! */
            }

          tmp_list = g_slist_next (tmp_list);
        }
    }
}

static void
gtk_text_view_destroy_layout (GtkTextView *text_view)
{
  if (text_view->layout)
    {
      GSList *tmp_list;

      if (text_view->incremental_validate_idle)
        {
          g_source_remove (text_view->incremental_validate_idle);
          text_view->incremental_validate_idle = 0;
        }

      /* Remove layout from all anchored children */
      
      tmp_list = text_view->children;
      while (tmp_list != NULL)
        {
          GtkTextViewChild *vc = tmp_list->data;

          if (vc->anchor)
            {
              gtk_text_anchored_child_set_layout (vc->widget, NULL);
              /* vc may now be invalid! */
            }

          tmp_list = g_slist_next (tmp_list);
        }
      
      gtk_text_view_stop_cursor_blink (text_view);
      gtk_text_view_end_selection_drag (text_view, NULL);

      g_signal_handlers_disconnect_by_func (G_OBJECT (text_view->layout),
                                            invalidated_handler, text_view);
      g_signal_handlers_disconnect_by_func (G_OBJECT (text_view->layout),
                                            changed_handler, text_view);
      g_object_unref (G_OBJECT (text_view->layout));
      text_view->layout = NULL;
    }
}

static void
gtk_text_view_reset_im_context (GtkTextView *text_view)
{
  if (text_view->need_im_reset)
    {
      text_view->need_im_reset = FALSE;
      gtk_im_context_reset (text_view->im_context);
    }
}

/*
 * DND feature
 */

static void
gtk_text_view_start_selection_dnd (GtkTextView       *text_view,
                                   const GtkTextIter *iter,
                                   GdkEventMotion    *event)
{
  GdkDragContext *context;
  GtkTargetList *target_list;

  text_view->drag_start_x = -1;
  text_view->drag_start_y = -1;

  target_list = gtk_target_list_new (target_table,
                                     G_N_ELEMENTS (target_table));

  context = gtk_drag_begin (GTK_WIDGET (text_view), target_list,
                            GDK_ACTION_COPY | GDK_ACTION_MOVE,
                            1, (GdkEvent*)event);

  gtk_target_list_unref (target_list);

  gtk_drag_set_icon_default (context);
}

static void
gtk_text_view_drag_begin (GtkWidget        *widget,
                          GdkDragContext   *context)
{
  /* do nothing */
}

static void
gtk_text_view_drag_end (GtkWidget        *widget,
                        GdkDragContext   *context)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);
}

static void
gtk_text_view_drag_data_get (GtkWidget        *widget,
                             GdkDragContext   *context,
                             GtkSelectionData *selection_data,
                             guint             info,
                             guint             time)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  if (selection_data->target == gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE))
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (text_view);

      gtk_selection_data_set (selection_data,
                              gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE),
                              8, /* bytes */
                              (void*)&buffer,
                              sizeof (buffer));
    }
  else
    {
      gchar *str;
      GtkTextIter start;
      GtkTextIter end;

      str = NULL;

      if (gtk_text_buffer_get_selection_bounds (get_buffer (text_view),
                                                &start, &end))
        {
          /* Extract the selected text */
          str = gtk_text_iter_get_visible_text (&start, &end);
        }

      if (str)
        {
          gtk_selection_data_set_text (selection_data, str);
          g_free (str);
        }
    }
}

static void
gtk_text_view_drag_data_delete (GtkWidget        *widget,
                                GdkDragContext   *context)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  gtk_text_buffer_delete_selection (GTK_TEXT_VIEW (widget)->buffer,
                                    TRUE, GTK_TEXT_VIEW (widget)->editable);
}

static void
gtk_text_view_drag_leave (GtkWidget        *widget,
                          GdkDragContext   *context,
                          guint             time)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (widget);

  gtk_text_mark_set_visible (text_view->dnd_mark, FALSE);
  
  if (text_view->scroll_timeout != 0)
    gtk_timeout_remove (text_view->scroll_timeout);

  text_view->scroll_timeout = 0;
}

static gboolean
gtk_text_view_drag_motion (GtkWidget        *widget,
                           GdkDragContext   *context,
                           gint              x,
                           gint              y,
                           guint             time)
{
  GtkTextIter newplace;
  GtkTextView *text_view;
  GtkTextIter start;
  GtkTextIter end;
  GdkRectangle target_rect;
  gint bx, by;
  GdkDragAction suggested_action = 0;
  
  text_view = GTK_TEXT_VIEW (widget);

  target_rect = text_view->text_window->allocation;
  
  if (x < target_rect.x ||
      y < target_rect.y ||
      x > (target_rect.x + target_rect.width) ||
      y > (target_rect.y + target_rect.height))
    return FALSE; /* outside the text window, allow parent widgets to handle event */

  gtk_text_view_window_to_buffer_coords (text_view,
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y,
                                         &bx, &by);

  gtk_text_layout_get_iter_at_pixel (text_view->layout,
                                     &newplace,
                                     bx, by);  

  if (gtk_drag_dest_find_target (widget, context,
                                 gtk_drag_dest_get_target_list (widget)) == GDK_NONE)
    {
      /* can't accept any of the offered targets */
    }                                 
  else if (gtk_text_buffer_get_selection_bounds (get_buffer (text_view),
                                            &start, &end) &&
           gtk_text_iter_in_range (&newplace, &start, &end))
    {
      /* We're inside the selection. */
    }
  else
    {      
      if (gtk_text_iter_editable (&newplace, text_view->editable))
        {
          GtkWidget *source_widget;
          
          suggested_action = context->suggested_action;
          
          source_widget = gtk_drag_get_source_widget (context);
          
          if (source_widget == widget)
            {
              /* Default to MOVE, unless the user has
               * pressed ctrl or alt to affect available actions
               */
              if ((context->actions & GDK_ACTION_MOVE) != 0)
                suggested_action = GDK_ACTION_MOVE;
            }
        }
      else
        {
          /* Can't drop here. */
        }
    }

  if (suggested_action != 0)
    {
      gtk_text_mark_set_visible (text_view->dnd_mark,
                                 text_view->cursor_visible);
      
      gdk_drag_status (context, suggested_action, time);
    }
  else
    {
      gdk_drag_status (context, 0, time);
      gtk_text_mark_set_visible (text_view->dnd_mark, FALSE);
    }
      
  gtk_text_buffer_move_mark (get_buffer (text_view),
                             text_view->dnd_mark,
                             &newplace);

  gtk_text_view_scroll_to_mark (text_view,
                                text_view->dnd_mark,
                                DND_SCROLL_MARGIN, FALSE, 0.0, 0.0);
  
  if (text_view->scroll_timeout != 0) /* reset on every motion event */
    gtk_timeout_remove (text_view->scroll_timeout);
      
  text_view->scroll_timeout =
    gtk_timeout_add (50, drag_scan_timeout, text_view);

  /* TRUE return means don't propagate the drag motion to parent
   * widgets that may also be drop sites.
   */
  return TRUE;
}

static gboolean
gtk_text_view_drag_drop (GtkWidget        *widget,
                         GdkDragContext   *context,
                         gint              x,
                         gint              y,
                         guint             time)
{
  GtkTextView *text_view;
  
  text_view = GTK_TEXT_VIEW (widget);
  
  if (text_view->scroll_timeout != 0)
    gtk_timeout_remove (text_view->scroll_timeout);

  text_view->scroll_timeout = 0;

  gtk_text_mark_set_visible (text_view->dnd_mark, FALSE);
  
  return TRUE;
}

static void
insert_text_data (GtkTextView      *text_view,
                  GtkTextIter      *drop_point,
                  GtkSelectionData *selection_data)
{
  gchar *str;

  str = gtk_selection_data_get_text (selection_data);

  if (str)
    {
      gtk_text_buffer_insert_interactive (get_buffer (text_view),
                                          drop_point, str, -1,
                                          text_view->editable);
      g_free (str);
    }
}

static void
gtk_text_view_drag_data_received (GtkWidget        *widget,
                                  GdkDragContext   *context,
                                  gint              x,
                                  gint              y,
                                  GtkSelectionData *selection_data,
                                  guint             info,
                                  guint             time)
{
  GtkTextIter drop_point;
  GtkTextView *text_view;
  GtkTextMark *drag_target_mark;

  text_view = GTK_TEXT_VIEW (widget);

  drag_target_mark = gtk_text_buffer_get_mark (get_buffer (text_view),
                                               "gtk_drag_target");

  if (drag_target_mark == NULL)
    return;

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view),
                                    &drop_point,
                                    drag_target_mark);

  if (selection_data->target == gdk_atom_intern ("GTK_TEXT_BUFFER_CONTENTS", FALSE))
    {
      GtkTextBuffer *src_buffer = NULL;
      GtkTextIter start, end;
      gboolean copy_tags = TRUE;

      if (selection_data->length != sizeof (src_buffer))
        return;

      memcpy (&src_buffer, selection_data->data, sizeof (src_buffer));

      if (src_buffer == NULL)
        return;

      g_return_if_fail (GTK_IS_TEXT_BUFFER (src_buffer));

      if (gtk_text_buffer_get_tag_table (src_buffer) !=
          gtk_text_buffer_get_tag_table (get_buffer (text_view)))
        copy_tags = FALSE;

      if (gtk_text_buffer_get_selection_bounds (src_buffer,
                                                &start,
                                                &end))
        {
          if (copy_tags)
            gtk_text_buffer_insert_range_interactive (get_buffer (text_view),
                                                      &drop_point,
                                                      &start,
                                                      &end,
                                                      text_view->editable);
          else
            {
              gchar *str;

              str = gtk_text_iter_get_visible_text (&start, &end);
              gtk_text_buffer_insert_interactive (get_buffer (text_view),
                                                  &drop_point, str, -1,
                                                  text_view->editable);
              g_free (str);
            }
        }
    }
  else
    insert_text_data (text_view, &drop_point, selection_data);
}

static GtkAdjustment*
get_hadjustment (GtkTextView *text_view)
{
  if (text_view->hadjustment == NULL)
    gtk_text_view_set_scroll_adjustments (text_view,
                                          NULL, /* forces creation */
                                          text_view->vadjustment);

  return text_view->hadjustment;
}

static GtkAdjustment*
get_vadjustment (GtkTextView *text_view)
{
  if (text_view->vadjustment == NULL)
    gtk_text_view_set_scroll_adjustments (text_view,
                                          text_view->hadjustment,
                                          NULL); /* forces creation */
  return text_view->vadjustment;
}


static void
gtk_text_view_set_scroll_adjustments (GtkTextView   *text_view,
                                      GtkAdjustment *hadj,
                                      GtkAdjustment *vadj)
{
  gboolean need_adjust = FALSE;

  g_return_if_fail (text_view != NULL);
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (hadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
  else
    hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
  if (vadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

  if (text_view->hadjustment && (text_view->hadjustment != hadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text_view->hadjustment), text_view);
      g_object_unref (G_OBJECT (text_view->hadjustment));
    }

  if (text_view->vadjustment && (text_view->vadjustment != vadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (text_view->vadjustment), text_view);
      g_object_unref (G_OBJECT (text_view->vadjustment));
    }

  if (text_view->hadjustment != hadj)
    {
      text_view->hadjustment = hadj;
      g_object_ref (G_OBJECT (text_view->hadjustment));
      gtk_object_sink (GTK_OBJECT (text_view->hadjustment));

      gtk_signal_connect (GTK_OBJECT (text_view->hadjustment), "value_changed",
                          (GtkSignalFunc) gtk_text_view_value_changed,
                          text_view);
      need_adjust = TRUE;
    }

  if (text_view->vadjustment != vadj)
    {
      text_view->vadjustment = vadj;
      g_object_ref (G_OBJECT (text_view->vadjustment));
      gtk_object_sink (GTK_OBJECT (text_view->vadjustment));

      gtk_signal_connect (GTK_OBJECT (text_view->vadjustment), "value_changed",
                          (GtkSignalFunc) gtk_text_view_value_changed,
                          text_view);
      need_adjust = TRUE;
    }

  if (need_adjust)
    gtk_text_view_value_changed (NULL, text_view);
}

static void
gtk_text_view_value_changed (GtkAdjustment *adj,
                             GtkTextView   *text_view)
{
  GtkTextIter iter;
  gint line_top;
  gint dx = 0;
  gint dy = 0;

  text_view->onscreen_validated = FALSE;

  DV(g_print(">Scroll offset changed, onscreen_validated = FALSE ("G_STRLOC")\n"));
  
  if (adj == text_view->hadjustment)
    {
      dx = text_view->xoffset - (gint)adj->value;
      text_view->xoffset = adj->value;
    }
  else if (adj == text_view->vadjustment)
    {
      dy = text_view->yoffset - (gint)adj->value;
      text_view->yoffset = adj->value;

      if (text_view->layout)
        {
          gtk_text_layout_get_line_at_y (text_view->layout, &iter, adj->value, &line_top);

          gtk_text_buffer_move_mark (get_buffer (text_view), text_view->first_para_mark, &iter);

          text_view->first_para_pixels = adj->value - line_top;
        }
    }
  
  if (GTK_WIDGET_REALIZED (text_view) && (dx != 0 || dy != 0))
    {
      if (dy != 0)
        {
          if (text_view->left_window)
            text_window_scroll (text_view->left_window, 0, dy);
          if (text_view->right_window)
            text_window_scroll (text_view->right_window, 0, dy);
        }

      if (dx != 0)
        {
          if (text_view->top_window)
            text_window_scroll (text_view->top_window, dx, 0);
          if (text_view->bottom_window)
            text_window_scroll (text_view->bottom_window, dx, 0);
        }

      /* It looks nicer to scroll the main area last, because
       * it takes a while, and making the side areas update
       * afterward emphasizes the slowness of scrolling the
       * main area.
       */
      text_window_scroll (text_view->text_window, dx, dy);
    }

  /* This could result in invalidation, which would install the
   * first_validate_idle, which would validate onscreen;
   * but we're going to go ahead and validate here, so
   * first_validate_idle shouldn't have anything to do.
   */
  gtk_text_view_update_layout_width (text_view);
  
  /* note that validation of onscreen could invoke this function
   * recursively, by scrolling to maintain first_para, or in response
   * to updating the layout width, however there is no problem with
   * that, or shouldn't be.
   */
  gtk_text_view_validate_onscreen (text_view);

  /* process exposes */
  if (GTK_WIDGET_REALIZED (text_view))
    {
      if (text_view->left_window)
        gdk_window_process_updates (text_view->left_window->bin_window, TRUE);

      if (text_view->right_window)
        gdk_window_process_updates (text_view->right_window->bin_window, TRUE);

      if (text_view->top_window)
        gdk_window_process_updates (text_view->top_window->bin_window, TRUE);
      
      if (text_view->bottom_window)
        gdk_window_process_updates (text_view->bottom_window->bin_window, TRUE);
  
      gdk_window_process_updates (text_view->text_window->bin_window, TRUE);
    }

  /* If this got installed, get rid of it, it's just a waste of time. */
  if (text_view->first_validate_idle != 0)
    {
      g_source_remove (text_view->first_validate_idle);
      text_view->first_validate_idle = 0;
    }
  
  DV(g_print(">End scroll offset changed handler ("G_STRLOC")\n"));
}

static void
gtk_text_view_commit_handler (GtkIMContext  *context,
                              const gchar   *str,
                              GtkTextView   *text_view)
{
  gtk_text_buffer_begin_user_action (get_buffer (text_view));

  gtk_text_buffer_delete_selection (get_buffer (text_view), TRUE,
                                    text_view->editable);

  if (!strcmp (str, "\n"))
    {
      gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view), "\n", 1,
                                                    text_view->editable);
    }
  else
    {
      if (text_view->overwrite_mode)
        gtk_text_view_delete_from_cursor (text_view, GTK_DELETE_CHARS, 1);
      gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view), str, -1,
                                                    text_view->editable);
    }

  gtk_text_buffer_end_user_action (get_buffer (text_view));
  
  gtk_text_view_scroll_mark_onscreen (text_view,
                                      gtk_text_buffer_get_mark (get_buffer (text_view),
                                                                "insert"));
}

static void
gtk_text_view_preedit_changed_handler (GtkIMContext *context,
				       GtkTextView  *text_view)
{
  gchar *str;
  PangoAttrList *attrs;
  gint cursor_pos;

  gtk_im_context_get_preedit_string (context, &str, &attrs, &cursor_pos);
  gtk_text_layout_set_preedit_string (text_view->layout, str, attrs, cursor_pos);

  pango_attr_list_unref (attrs);
  g_free (str);
}

static void
gtk_text_view_mark_set_handler (GtkTextBuffer     *buffer,
                                const GtkTextIter *location,
                                GtkTextMark       *mark,
                                gpointer           data)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (data);
  gboolean need_reset = FALSE;

  if (mark == gtk_text_buffer_get_insert (buffer))
    {
      text_view->virtual_cursor_x = -1;
      text_view->virtual_cursor_y = -1;
      need_reset = TRUE;
    }
  else if (mark == gtk_text_buffer_get_selection_bound (buffer))
    {
      need_reset = TRUE;
    }

  if (need_reset)
    gtk_text_view_reset_im_context (text_view);
}

static void
gtk_text_view_get_virtual_cursor_pos (GtkTextView *text_view,
                                      gint        *x,
                                      gint        *y)
{
  GdkRectangle strong_pos;
  GtkTextIter insert;

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_mark (get_buffer (text_view),
                                                              "insert"));

  if ((x && text_view->virtual_cursor_x == -1) ||
      (y && text_view->virtual_cursor_y == -1))
    gtk_text_layout_get_cursor_locations (text_view->layout, &insert, &strong_pos, NULL);

  if (x)
    {
      if (text_view->virtual_cursor_x != -1)
        *x = text_view->virtual_cursor_x;
      else
        *x = strong_pos.x;
    }

  if (y)
    {
      if (text_view->virtual_cursor_x != -1)
        *y = text_view->virtual_cursor_y;
      else
        *y = strong_pos.y + strong_pos.height / 2;
    }
}

static void
gtk_text_view_set_virtual_cursor_pos (GtkTextView *text_view,
                                      gint         x,
                                      gint         y)
{
  GdkRectangle strong_pos;
  GtkTextIter insert;

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_mark (get_buffer (text_view),
                                                              "insert"));

  if (x == -1 || y == -1)
    gtk_text_layout_get_cursor_locations (text_view->layout, &insert, &strong_pos, NULL);

  text_view->virtual_cursor_x = (x == -1) ? strong_pos.x : x;
  text_view->virtual_cursor_y = (y == -1) ? strong_pos.y + strong_pos.height / 2 : y;
}

/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget   *menuitem,
	     GtkTextView *text_view)
{
  const gchar *signal = g_object_get_data (G_OBJECT (menuitem), "gtk-signal");
  gtk_signal_emit_by_name (GTK_OBJECT (text_view), signal);
}

static void
append_action_signal (GtkTextView  *text_view,
		      GtkWidget    *menu,
		      const gchar  *label,
		      const gchar  *signal)
{
  GtkWidget *menuitem = gtk_menu_item_new_with_label (label);

  g_object_set_data (G_OBJECT (menuitem), "gtk-signal", (char *)signal);
  gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
		      activate_cb, text_view);

  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}

static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  GTK_TEXT_VIEW (attach_widget)->popup_menu = NULL;
}

static void
gtk_text_view_popup_menu (GtkTextView    *text_view,
			  GdkEventButton *event)
{
  if (!text_view->popup_menu)
    {
      GtkWidget *menuitem;

      text_view->popup_menu = gtk_menu_new ();

      gtk_menu_attach_to_widget (GTK_MENU (text_view->popup_menu),
				 GTK_WIDGET (text_view),
				 popup_menu_detach);

      append_action_signal (text_view, text_view->popup_menu, _("Cut"), "cut_clipboard");
      append_action_signal (text_view, text_view->popup_menu, _("Copy"), "copy_clipboard");
      append_action_signal (text_view, text_view->popup_menu, _("Paste"), "paste_clipboard");

      menuitem = gtk_separator_menu_item_new ();
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (text_view->popup_menu), menuitem);

      gtk_im_multicontext_append_menuitems (GTK_IM_MULTICONTEXT (text_view->im_context),
					    GTK_MENU_SHELL (text_view->popup_menu));
    }

  gtk_menu_popup (GTK_MENU (text_view->popup_menu), NULL, NULL,
		  NULL, NULL,
		  event->button, event->time);
}



/* Child GdkWindows */


static GtkTextWindow*
text_window_new (GtkTextWindowType  type,
                 GtkWidget         *widget,
                 gint               width_request,
                 gint               height_request)
{
  GtkTextWindow *win;

  win = g_new (GtkTextWindow, 1);

  win->type = type;
  win->widget = widget;
  win->window = NULL;
  win->bin_window = NULL;
  win->requisition.width = width_request;
  win->requisition.height = height_request;
  win->allocation.width = width_request;
  win->allocation.height = height_request;
  win->allocation.x = 0;
  win->allocation.y = 0;

  return win;
}

static void
text_window_free (GtkTextWindow *win)
{
  if (win->window)
    text_window_unrealize (win);

  g_free (win);
}

static void
text_window_realize (GtkTextWindow *win,
                     GdkWindow     *parent)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkCursor *cursor;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = win->allocation.x;
  attributes.y = win->allocation.y;
  attributes.width = win->allocation.width;
  attributes.height = win->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (win->widget);
  attributes.colormap = gtk_widget_get_colormap (win->widget);
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  win->window = gdk_window_new (parent,
                                &attributes,
                                attributes_mask);

  gdk_window_show (win->window);
  gdk_window_set_user_data (win->window, win->widget);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = win->allocation.width;
  attributes.height = win->allocation.height;
  attributes.event_mask = (GDK_EXPOSURE_MASK            |
                           GDK_SCROLL_MASK              |
                           GDK_KEY_PRESS_MASK           |
                           GDK_BUTTON_PRESS_MASK        |
                           GDK_BUTTON_RELEASE_MASK      |
                           GDK_POINTER_MOTION_MASK      |
                           GDK_POINTER_MOTION_HINT_MASK |
                           gtk_widget_get_events (win->widget));

  win->bin_window = gdk_window_new (win->window,
                                    &attributes,
                                    attributes_mask);

  gdk_window_show (win->bin_window);
  gdk_window_set_user_data (win->bin_window, win->widget);

  if (win->type == GTK_TEXT_WINDOW_TEXT)
    {
      /* I-beam cursor */
      cursor = gdk_cursor_new (GDK_XTERM);
      gdk_window_set_cursor (win->bin_window, cursor);
      gdk_cursor_destroy (cursor);

      gtk_im_context_set_client_window (GTK_TEXT_VIEW (win->widget)->im_context,
                                        win->window);


      gdk_window_set_background (win->bin_window,
                                 &win->widget->style->base[GTK_WIDGET_STATE (win->widget)]);
    }
  else
    {
      gdk_window_set_background (win->bin_window,
                                 &win->widget->style->bg[GTK_WIDGET_STATE (win->widget)]);
    }

  g_object_set_qdata (G_OBJECT (win->window),
                      g_quark_from_static_string ("gtk-text-view-text-window"),
                      win);

  g_object_set_qdata (G_OBJECT (win->bin_window),
                      g_quark_from_static_string ("gtk-text-view-text-window"),
                      win);
}

static void
text_window_unrealize (GtkTextWindow *win)
{
  if (win->type == GTK_TEXT_WINDOW_TEXT)
    {
      gtk_im_context_set_client_window (GTK_TEXT_VIEW (win->widget)->im_context,
                                        NULL);
    }

  gdk_window_set_user_data (win->window, NULL);
  gdk_window_set_user_data (win->bin_window, NULL);
  gdk_window_destroy (win->bin_window);
  gdk_window_destroy (win->window);
  win->window = NULL;
  win->bin_window = NULL;
}

static void
text_window_size_allocate (GtkTextWindow *win,
                           GdkRectangle  *rect)
{
  win->allocation = *rect;

  if (win->window)
    {
      gdk_window_move_resize (win->window,
                              rect->x, rect->y,
                              rect->width, rect->height);

      gdk_window_resize (win->bin_window,
                         rect->width, rect->height);
    }
}

static void
text_window_scroll        (GtkTextWindow *win,
                           gint           dx,
                           gint           dy)
{
  if (dx != 0 || dy != 0)
    {
      gdk_window_scroll (win->bin_window, dx, dy);
    }
}

void
text_window_invalidate_rect (GtkTextWindow *win,
                             GdkRectangle  *rect)
{
  GdkRectangle window_rect;

  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (win->widget),
                                         win->type,
                                         rect->x,
                                         rect->y,
                                         &window_rect.x,
                                         &window_rect.y);

  window_rect.width = rect->width;
  window_rect.height = rect->height;
  
  /* Adjust the rect as appropriate */
  
  switch (win->type)
    {
    case GTK_TEXT_WINDOW_TEXT:
      break;

    case GTK_TEXT_WINDOW_LEFT:
    case GTK_TEXT_WINDOW_RIGHT:
      window_rect.x = 0;
      window_rect.width = win->allocation.width;
      break;

    case GTK_TEXT_WINDOW_TOP:
    case GTK_TEXT_WINDOW_BOTTOM:
      window_rect.y = 0;
      window_rect.height = win->allocation.height;
      break;

    default:
      g_warning ("%s: bug!", G_STRLOC);
      return;
      break;
    }
          
  gdk_window_invalidate_rect (win->bin_window, &window_rect, FALSE);

#if 0
  {
    GdkColor color = { 0, 65535, 0, 0 };
    GdkGC *gc = gdk_gc_new (win->bin_window);
    gdk_gc_set_rgb_fg_color (gc, &color);
    gdk_draw_rectangle (win->bin_window,
                        gc, TRUE, window_rect.x, window_rect.y,
                        window_rect.width, window_rect.height);
    gdk_gc_unref (gc);
  }
#endif
}

static gint
text_window_get_width (GtkTextWindow *win)
{
  return win->allocation.width;
}

static gint
text_window_get_height (GtkTextWindow *win)
{
  return win->allocation.height;
}

static void
text_window_get_allocation (GtkTextWindow *win,
                            GdkRectangle  *rect)
{
  *rect = win->allocation;
}

/* Windows */


/**
 * gtk_text_view_get_window:
 * @text_view: a #GtkTextView
 * @win: window to get
 *
 * Retrieves the #GdkWindow corresponding to an area of the text view;
 * possible windows include the overall widget window, child windows
 * on the left, right, top, bottom, and the window that displays the
 * text buffer. Windows are %NULL and nonexistent if their width or
 * height is 0, and are nonexistent before the widget has been
 * realized.
 *
 * Return value: a #GdkWindow, or %NULL
 **/
GdkWindow*
gtk_text_view_get_window (GtkTextView *text_view,
                          GtkTextWindowType win)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  switch (win)
    {
    case GTK_TEXT_WINDOW_WIDGET:
      return GTK_WIDGET (text_view)->window;
      break;

    case GTK_TEXT_WINDOW_TEXT:
      return text_view->text_window->bin_window;
      break;

    case GTK_TEXT_WINDOW_LEFT:
      if (text_view->left_window)
        return text_view->left_window->bin_window;
      else
        return NULL;
      break;

    case GTK_TEXT_WINDOW_RIGHT:
      if (text_view->right_window)
        return text_view->right_window->bin_window;
      else
        return NULL;
      break;

    case GTK_TEXT_WINDOW_TOP:
      if (text_view->top_window)
        return text_view->top_window->bin_window;
      else
        return NULL;
      break;

    case GTK_TEXT_WINDOW_BOTTOM:
      if (text_view->bottom_window)
        return text_view->bottom_window->bin_window;
      else
        return NULL;
      break;

    default:
      g_warning ("%s: Unknown GtkTextWindowType", G_STRLOC);
      return NULL;
      break;
    }
}

/**
 * gtk_text_view_get_window_type:
 * @text_view: a #GtkTextView
 * @window: a window type
 *
 * Usually used to find out which window an event corresponds to.
 * If you connect to an event signal on @text_view, this function
 * should be called on <literal>event-&gt;window</literal> to
 * see which window it was.
 *
 * Return value: the window type.
 **/
GtkTextWindowType
gtk_text_view_get_window_type (GtkTextView *text_view,
                               GdkWindow   *window)
{
  GtkTextWindow *win;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);
  g_return_val_if_fail (GDK_IS_WINDOW (text_view), 0);

  if (window == GTK_WIDGET (text_view)->window)
    return GTK_TEXT_WINDOW_WIDGET;

  win = g_object_get_qdata (G_OBJECT (window),
                            g_quark_try_string ("gtk-text-view-text-window"));

  if (win)
    return win->type;
  else
    {
      return GTK_TEXT_WINDOW_PRIVATE;
    }
}

static void
buffer_to_widget (GtkTextView      *text_view,
                  gint              buffer_x,
                  gint              buffer_y,
                  gint             *window_x,
                  gint             *window_y)
{
  if (window_x)
    {
      *window_x = buffer_x - text_view->xoffset + FOCUS_EDGE_WIDTH;
      if (text_view->left_window)
        *window_x += text_view->left_window->allocation.width;
    }

  if (window_y)
    {
      *window_y = buffer_y - text_view->yoffset + FOCUS_EDGE_WIDTH;
      if (text_view->top_window)
        *window_y += text_view->top_window->allocation.height;
    }
}

static void
widget_to_text_window (GtkTextWindow *win,
                       gint           widget_x,
                       gint           widget_y,
                       gint          *window_x,
                       gint          *window_y)
{
  if (window_x)
    *window_x = widget_x - win->allocation.x;

  if (window_y)
    *window_y = widget_y - win->allocation.y;
}

static void
buffer_to_text_window (GtkTextView   *text_view,
                       GtkTextWindow *win,
                       gint           buffer_x,
                       gint           buffer_y,
                       gint          *window_x,
                       gint          *window_y)
{
  if (win == NULL)
    {
      g_warning ("Attempt to convert text buffer coordinates to coordinates "
                 "for a nonexistent or private child window of GtkTextView");
      return;
    }

  buffer_to_widget (text_view,
                    buffer_x, buffer_y,
                    window_x, window_y);

  widget_to_text_window (win,
                         window_x ? *window_x : 0,
                         window_y ? *window_y : 0,
                         window_x,
                         window_y);
}

/**
 * gtk_text_view_buffer_to_window_coords:
 * @text_view: a #GtkTextView
 * @win: a #GtkTextWindowType
 * @buffer_x: buffer x coordinate
 * @buffer_y: buffer y coordinate
 * @window_x: window x coordinate return location
 * @window_y: window y coordinate return location
 *
 * Converts coordinate (@buffer_x, @buffer_y) to coordinates for the window
 * @win, and stores the result in (@window_x, @window_y).
 **/
void
gtk_text_view_buffer_to_window_coords (GtkTextView      *text_view,
                                       GtkTextWindowType win,
                                       gint              buffer_x,
                                       gint              buffer_y,
                                       gint             *window_x,
                                       gint             *window_y)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  switch (win)
    {
    case GTK_TEXT_WINDOW_WIDGET:
      buffer_to_widget (text_view,
                        buffer_x, buffer_y,
                        window_x, window_y);
      break;

    case GTK_TEXT_WINDOW_TEXT:
      if (window_x)
        *window_x = buffer_x - text_view->xoffset;
      if (window_y)
        *window_y = buffer_y - text_view->yoffset;
      break;

    case GTK_TEXT_WINDOW_LEFT:
      buffer_to_text_window (text_view,
                             text_view->left_window,
                             buffer_x, buffer_y,
                             window_x, window_y);
      break;

    case GTK_TEXT_WINDOW_RIGHT:
      buffer_to_text_window (text_view,
                             text_view->right_window,
                             buffer_x, buffer_y,
                             window_x, window_y);
      break;

    case GTK_TEXT_WINDOW_TOP:
      buffer_to_text_window (text_view,
                             text_view->top_window,
                             buffer_x, buffer_y,
                             window_x, window_y);
      break;

    case GTK_TEXT_WINDOW_BOTTOM:
      buffer_to_text_window (text_view,
                             text_view->bottom_window,
                             buffer_x, buffer_y,
                             window_x, window_y);
      break;

    case GTK_TEXT_WINDOW_PRIVATE:
      g_warning ("%s: can't get coords for private windows", G_STRLOC);
      break;

    default:
      g_warning ("%s: Unknown GtkTextWindowType", G_STRLOC);
      break;
    }
}

static void
widget_to_buffer (GtkTextView *text_view,
                  gint         widget_x,
                  gint         widget_y,
                  gint        *buffer_x,
                  gint        *buffer_y)
{
  if (buffer_x)
    {
      *buffer_x = widget_x - FOCUS_EDGE_WIDTH + text_view->xoffset;
      if (text_view->left_window)
        *buffer_x -= text_view->left_window->allocation.width;
    }

  if (buffer_y)
    {
      *buffer_y = widget_y - FOCUS_EDGE_WIDTH + text_view->yoffset;
      if (text_view->top_window)
        *buffer_y -= text_view->top_window->allocation.height;
    }
}

static void
text_window_to_widget (GtkTextWindow *win,
                       gint           window_x,
                       gint           window_y,
                       gint          *widget_x,
                       gint          *widget_y)
{
  if (widget_x)
    *widget_x = window_x + win->allocation.x;

  if (widget_y)
    *widget_y = window_y + win->allocation.y;
}

static void
text_window_to_buffer (GtkTextView   *text_view,
                       GtkTextWindow *win,
                       gint           window_x,
                       gint           window_y,
                       gint          *buffer_x,
                       gint          *buffer_y)
{
  if (win == NULL)
    {
      g_warning ("Attempt to convert GtkTextView buffer coordinates into "
                 "coordinates for a nonexistent child window.");
      return;
    }

  text_window_to_widget (win,
                         window_x,
                         window_y,
                         buffer_x,
                         buffer_y);

  widget_to_buffer (text_view,
                    buffer_x ? *buffer_x : 0,
                    buffer_y ? *buffer_y : 0,
                    buffer_x,
                    buffer_y);
}

/**
 * gtk_text_view_window_to_buffer_coords:
 * @text_view: a #GtkTextView
 * @win: a #GtkTextWindowType
 * @window_x: window x coordinate
 * @window_y: window y coordinate
 * @buffer_x: buffer x coordinate return location
 * @buffer_y: buffer y coordinate return location
 *
 * Converts coordinates on the window identified by @win to buffer
 * coordinates, storing the result in (@buffer_x,@buffer_y).
 **/
void
gtk_text_view_window_to_buffer_coords (GtkTextView      *text_view,
                                       GtkTextWindowType win,
                                       gint              window_x,
                                       gint              window_y,
                                       gint             *buffer_x,
                                       gint             *buffer_y)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  switch (win)
    {
    case GTK_TEXT_WINDOW_WIDGET:
      widget_to_buffer (text_view,
                        window_x, window_y,
                        buffer_x, buffer_y);
      break;

    case GTK_TEXT_WINDOW_TEXT:
      if (buffer_x)
        *buffer_x = window_x + text_view->xoffset;
      if (buffer_y)
        *buffer_y = window_y + text_view->yoffset;
      break;

    case GTK_TEXT_WINDOW_LEFT:
      text_window_to_buffer (text_view,
                             text_view->left_window,
                             window_x, window_y,
                             buffer_x, buffer_y);
      break;

    case GTK_TEXT_WINDOW_RIGHT:
      text_window_to_buffer (text_view,
                             text_view->right_window,
                             window_x, window_y,
                             buffer_x, buffer_y);
      break;

    case GTK_TEXT_WINDOW_TOP:
      text_window_to_buffer (text_view,
                             text_view->top_window,
                             window_x, window_y,
                             buffer_x, buffer_y);
      break;

    case GTK_TEXT_WINDOW_BOTTOM:
      text_window_to_buffer (text_view,
                             text_view->bottom_window,
                             window_x, window_y,
                             buffer_x, buffer_y);
      break;

    case GTK_TEXT_WINDOW_PRIVATE:
      g_warning ("%s: can't get coords for private windows", G_STRLOC);
      break;

    default:
      g_warning ("%s: Unknown GtkTextWindowType", G_STRLOC);
      break;
    }
}

static void
set_window_width (GtkTextView      *text_view,
                  gint              width,
                  GtkTextWindowType type,
                  GtkTextWindow   **winp)
{
  if (width == 0)
    {
      if (*winp)
        {
          text_window_free (*winp);
          *winp = NULL;
          gtk_widget_queue_resize (GTK_WIDGET (text_view));
        }
    }
  else
    {
      if (*winp == NULL)
        {
          *winp = text_window_new (type,
                                   GTK_WIDGET (text_view),
                                   width, 0);
          /* if the widget is already realized we need to realize the child manually */
          if (GTK_WIDGET_REALIZED (text_view))
            text_window_realize (*winp,
                         GTK_WIDGET(text_view)->window);
        }
      else
        {
          if ((*winp)->requisition.width == width)
            return;
        }

      gtk_widget_queue_resize (GTK_WIDGET (text_view));
    }
}


static void
set_window_height (GtkTextView      *text_view,
                   gint              height,
                   GtkTextWindowType type,
                   GtkTextWindow   **winp)
{
  if (height == 0)
    {
      if (*winp)
        {
          text_window_free (*winp);
          *winp = NULL;
          gtk_widget_queue_resize (GTK_WIDGET (text_view));
        }
    }
  else
    {
      if (*winp == NULL)
        {
          *winp = text_window_new (type,
                                   GTK_WIDGET (text_view),
                                   0, height);

          /* if the widget is already realized we need to realize the child manually */
          if (GTK_WIDGET_REALIZED (text_view))
            text_window_realize (*winp,
                         GTK_WIDGET(text_view)->window);
        }
      else
        {
          if ((*winp)->requisition.height == height)
            return;
        }

      gtk_widget_queue_resize (GTK_WIDGET (text_view));
    }
}

/**
 * gtk_text_view_set_border_window_size:
 * @text_view: a #GtkTextView
 * @type: window to affect
 * @size: width or height of the window
 *
 * Sets the width of %GTK_TEXT_WINDOW_LEFT or %GTK_TEXT_WINDOW_RIGHT,
 * or the height of %GTK_TEXT_WINDOW_TOP or %GTK_TEXT_WINDOW_BOTTOM.
 * Automatically destroys the corresponding window if the size is set to 0,
 * and creates the window if the size is set to non-zero.
 **/
void
gtk_text_view_set_border_window_size (GtkTextView      *text_view,
                                      GtkTextWindowType type,
                                      gint              size)

{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (size >= 0);
  g_return_if_fail (type != GTK_TEXT_WINDOW_WIDGET);
  g_return_if_fail (type != GTK_TEXT_WINDOW_TEXT);

  switch (type)
    {
    case GTK_TEXT_WINDOW_LEFT:
      set_window_width (text_view, size, GTK_TEXT_WINDOW_LEFT,
                        &text_view->left_window);
      break;

    case GTK_TEXT_WINDOW_RIGHT:
      set_window_width (text_view, size, GTK_TEXT_WINDOW_RIGHT,
                        &text_view->right_window);
      break;

    case GTK_TEXT_WINDOW_TOP:
      set_window_height (text_view, size, GTK_TEXT_WINDOW_TOP,
                         &text_view->top_window);
      break;

    case GTK_TEXT_WINDOW_BOTTOM:
      set_window_height (text_view, size, GTK_TEXT_WINDOW_BOTTOM,
                         &text_view->bottom_window);
      break;

    default:
      g_warning ("Can't set size of center or widget or private GtkTextWindowType in %s", G_STRLOC);
      break;
    }
}

/**
 * gtk_text_view_set_text_window_size:
 * @text_view: a #GtkTextView
 * @width: a width in pixels
 * @height: a height in pixels
 *
 * Sets the size request for the main text window (%GTK_TEXT_WINDOW_TEXT).
 * If the widget gets more space than it requested, the main text window
 * will be larger than this.
 *
 **/
void
gtk_text_view_set_text_window_size (GtkTextView *text_view,
                                    gint         width,
                                    gint         height)
{
  GtkTextWindow *win;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  win = text_view->text_window;

  if (win->requisition.width == width &&
      win->requisition.height == height)
    return;

  win->requisition.width = width;
  win->requisition.height = height;

  gtk_widget_queue_resize (GTK_WIDGET (text_view));
}

/*
 * Child widgets
 */

static GtkTextViewChild*
text_view_child_new_anchored (GtkWidget          *child,
                              GtkTextChildAnchor *anchor,
                              GtkTextLayout      *layout)
{
  GtkTextViewChild *vc;

  vc = g_new (GtkTextViewChild, 1);

  vc->widget = child;
  vc->anchor = anchor;

  vc->from_top_of_line = 0;
  vc->from_left_of_buffer = 0;
  
  g_object_ref (G_OBJECT (vc->widget));
  g_object_ref (G_OBJECT (vc->anchor));

  g_object_set_data (G_OBJECT (child),
                     "gtk-text-view-child",
                     vc);

  gtk_text_child_anchor_register_child (anchor, child, layout);
  
  return vc;
}

static GtkTextViewChild*
text_view_child_new_window (GtkWidget          *child,
                            GtkTextWindowType   type,
                            gint                x,
                            gint                y)
{
  GtkTextViewChild *vc;

  vc = g_new (GtkTextViewChild, 1);

  vc->widget = child;
  vc->anchor = NULL;

  vc->from_top_of_line = 0;
  vc->from_left_of_buffer = 0;
  
  g_object_ref (G_OBJECT (vc->widget));

  vc->type = type;
  vc->x = x;
  vc->y = y;

  return vc;
}

static void
text_view_child_free (GtkTextViewChild *child)
{
  g_object_set_data (G_OBJECT (child->widget),
                     "gtk-text-view-child", NULL);

  if (child->anchor)
    {
      gtk_text_child_anchor_unregister_child (child->anchor,
                                              child->widget);
      g_object_unref (G_OBJECT (child->anchor));
    }

  g_object_unref (G_OBJECT (child->widget));

  g_free (child);
}

static void
text_view_child_realize (GtkTextView      *text_view,
                         GtkTextViewChild *vc)
{
  if (vc->anchor)
    gtk_widget_set_parent_window (vc->widget,
                                  text_view->text_window->bin_window);
  else
    {
      GdkWindow *window;
      window = gtk_text_view_get_window (text_view,
                                         vc->type);
      gtk_widget_set_parent_window (vc->widget, window);
    }

  gtk_widget_realize (vc->widget);
}

static void
add_child (GtkTextView      *text_view,
           GtkTextViewChild *vc)
{
  text_view->children = g_slist_prepend (text_view->children,
                                         vc);

  gtk_widget_set_parent (vc->widget, GTK_WIDGET (text_view));

  if (GTK_WIDGET_REALIZED (text_view))
    text_view_child_realize (text_view, vc);

  if (GTK_WIDGET_VISIBLE (text_view) && GTK_WIDGET_VISIBLE (vc->widget))
    {
      if (GTK_WIDGET_MAPPED (text_view))
        gtk_widget_map (vc->widget);

      gtk_widget_queue_resize (vc->widget);
    }
}

void
gtk_text_view_add_child_at_anchor (GtkTextView          *text_view,
                                   GtkWidget            *child,
                                   GtkTextChildAnchor   *anchor)
{
  GtkTextViewChild *vc;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (child->parent == NULL);

  gtk_text_view_ensure_layout (text_view);

  vc = text_view_child_new_anchored (child, anchor,
                                     text_view->layout);

  add_child (text_view, vc);
}

void
gtk_text_view_add_child_in_window (GtkTextView          *text_view,
                                   GtkWidget            *child,
                                   GtkTextWindowType     which_window,
                                   gint                  xpos,
                                   gint                  ypos)
{
  GtkTextViewChild *vc;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (xpos >= 0);
  g_return_if_fail (ypos >= 0);
  g_return_if_fail (child->parent == NULL);

  vc = text_view_child_new_window (child, which_window,
                                   xpos, ypos);

  add_child (text_view, vc);
}

void
gtk_text_view_move_child          (GtkTextView          *text_view,
                                   GtkWidget            *child,
                                   gint                  xpos,
                                   gint                  ypos)
{
  GtkTextViewChild *vc;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (xpos >= 0);
  g_return_if_fail (ypos >= 0);
  g_return_if_fail (child->parent == (GtkWidget*) text_view);

  vc = g_object_get_data (G_OBJECT (child),
                          "gtk-text-view-child");

  g_assert (vc != NULL);

  vc->x = xpos;
  vc->y = ypos;

  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (text_view))
    gtk_widget_queue_resize (child);
}



/* Iterator operations */

gboolean
gtk_text_view_forward_display_line (GtkTextView *text_view,
                                    GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_to_next_line (text_view->layout, iter);
}

gboolean
gtk_text_view_backward_display_line (GtkTextView *text_view,
                                     GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_to_previous_line (text_view->layout, iter);
}

gboolean
gtk_text_view_forward_display_line_end (GtkTextView *text_view,
                                        GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_to_line_end (text_view->layout, iter, 1);
}

gboolean
gtk_text_view_backward_display_line_start (GtkTextView *text_view,
                                           GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_to_line_end (text_view->layout, iter, -1);
}

gboolean
gtk_text_view_starts_display_line (GtkTextView       *text_view,
                                   const GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_iter_starts_line (text_view->layout, iter);
}

gboolean
gtk_text_view_move_visually (GtkTextView *text_view,
                             GtkTextIter *iter,
                             gint         count)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_visually (text_view->layout, iter, count);
}


