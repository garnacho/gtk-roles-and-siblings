/* gtktreeviewcolumn.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#include "gtktreeviewcolumn.h"
#include "gtktreeview.h"
#include "gtktreeprivate.h"
#include "gtksignal.h"
#include "gtkbutton.h"
#include "gtkalignment.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkmarshalers.h"
#include "gtkarrow.h"
#include "gtkintl.h"
#include <string.h>

enum
{
  PROP_0,
  PROP_VISIBLE,
  PROP_WIDTH,
  PROP_SIZING,
  PROP_FIXED_WIDTH,
  PROP_MIN_WIDTH,
  PROP_MAX_WIDTH,
  PROP_TITLE,
  PROP_CLICKABLE,
  PROP_WIDGET,
  PROP_ALIGNMENT,
  PROP_REORDERABLE,
  PROP_SORT_INDICATOR,
  PROP_SORT_ORDER
};

enum
{
  CLICKED,
  LAST_SIGNAL
};

typedef struct _GtkTreeViewColumnCellInfo GtkTreeViewColumnCellInfo;
struct _GtkTreeViewColumnCellInfo
{
  GtkCellRenderer *cell;
  GSList *attributes;
  GtkTreeCellDataFunc func;
  gpointer func_data;
  GtkDestroyNotify destroy;
  gint requested_width;
  guint expand : 1;
  guint pack : 1;
  guint has_focus : 1;
};

/* Type methods */
static void gtk_tree_view_column_init                          (GtkTreeViewColumn       *tree_column);
static void gtk_tree_view_column_class_init                    (GtkTreeViewColumnClass  *klass);

/* GObject methods */
static void gtk_tree_view_column_set_property                  (GObject                 *object,
								guint                    prop_id,
								const GValue            *value,
								GParamSpec              *pspec);
static void gtk_tree_view_column_get_property                  (GObject                 *object,
								guint                    prop_id,
								GValue                  *value,
								GParamSpec              *pspec);
static void gtk_tree_view_column_finalize                      (GObject                 *object);

/* Button handling code */ 
static void gtk_tree_view_column_create_button                 (GtkTreeViewColumn       *tree_column);
static void gtk_tree_view_column_update_button                 (GtkTreeViewColumn       *tree_column);

/* Button signal handlers */
static gint gtk_tree_view_column_button_event                  (GtkWidget               *widget,
								GdkEvent                *event,
								gpointer                 data);
static void gtk_tree_view_column_button_realize                (GtkWidget               *widget,
								gpointer                 data);
static void gtk_tree_view_column_button_clicked                (GtkWidget               *widget,
								gpointer                 data);

/* Property handlers */
static void gtk_tree_view_model_sort_column_changed            (GtkTreeSortable         *sortable,
								GtkTreeViewColumn       *tree_column);

/* Internal functions */
static void gtk_tree_view_column_sort                          (GtkTreeViewColumn       *tree_column,
								gpointer                 data);
static void gtk_tree_view_column_setup_sort_column_id_callback (GtkTreeViewColumn       *tree_column);
static void gtk_tree_view_column_set_attributesv               (GtkTreeViewColumn       *tree_column,
								GtkCellRenderer         *cell_renderer,
								va_list                  args);
static GtkTreeViewColumnCellInfo *gtk_tree_view_column_get_cell_info (GtkTreeViewColumn *tree_column,
								      GtkCellRenderer   *cell_renderer);



static GtkObjectClass *parent_class = NULL;
static guint tree_column_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_tree_view_column_get_type (void)
{
  static GtkType tree_column_type = 0;

  if (!tree_column_type)
    {
      static const GTypeInfo tree_column_info =
      {
	sizeof (GtkTreeViewColumnClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_tree_view_column_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkTreeViewColumn),
	0,
	(GInstanceInitFunc) gtk_tree_view_column_init,
      };

      tree_column_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkTreeViewColumn", &tree_column_info, 0);
    }

  return tree_column_type;
}

static void
gtk_tree_view_column_class_init (GtkTreeViewColumnClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;

  parent_class = g_type_class_peek_parent (class);

  class->clicked = NULL;

  object_class->finalize = gtk_tree_view_column_finalize;
  object_class->set_property = gtk_tree_view_column_set_property;
  object_class->get_property = gtk_tree_view_column_get_property;
  
  tree_column_signals[CLICKED] =
    g_signal_new ("clicked",
                  GTK_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTreeViewColumnClass, clicked),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  GTK_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_VISIBLE,
                                   g_param_spec_boolean ("visible",
                                                        _("Visible"),
                                                        _("Whether to display the column"),
                                                         TRUE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_int ("width",
						     _("Width"),
						     _("Current width of the column"),
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READABLE));
  g_object_class_install_property (object_class,
                                   PROP_SIZING,
                                   g_param_spec_enum ("sizing",
                                                      _("Sizing"),
                                                      _("Resize mode of the column"),
                                                      GTK_TYPE_TREE_VIEW_COLUMN_SIZING,
                                                      GTK_TREE_VIEW_COLUMN_AUTOSIZE,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_FIXED_WIDTH,
                                   g_param_spec_int ("fixed_width",
                                                     _("Fixed Width"),
                                                     _("Current fixed width of the column"),
                                                     1,
                                                     G_MAXINT,
                                                     1, /* not useful */
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_MIN_WIDTH,
                                   g_param_spec_int ("min_width",
                                                     _("Minimum Width"),
                                                     _("Minimum allowed width of the column"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_MAX_WIDTH,
                                   g_param_spec_int ("max_width",
                                                     _("Maximum Width"),
                                                     _("Maximum allowed width of the column"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        _("Title"),
                                                        _("Title to appear in column header"),
                                                        "",
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_CLICKABLE,
                                   g_param_spec_boolean ("clickable",
                                                        _("Clickable"),
                                                        _("Whether the header can be clicked"),
                                                         TRUE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
  

  g_object_class_install_property (object_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        _("Widget"),
                                                        _("Widget to put in column header button instead of column title"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ALIGNMENT,
                                   g_param_spec_float ("alignment",
                                                       _("Alignment"),
                                                       _("X Alignment of the column header text or widget"),
                                                       0.0,
                                                       1.0,
                                                       0.5,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_REORDERABLE,
                                   g_param_spec_boolean ("reorderable",
							 _("Reorderable"),
							 _("Whether the column can be reordered around the headers"),
							 FALSE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_SORT_INDICATOR,
                                   g_param_spec_boolean ("sort_indicator",
                                                        _("Sort indicator"),
                                                        _("Whether to show a sort indicator"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_SORT_ORDER,
                                   g_param_spec_enum ("sort_order",
                                                      _("Sort order"),
                                                      _("Sort direction the sort indicator should indicate"),
                                                      GTK_TYPE_SORT_TYPE,
                                                      GTK_SORT_ASCENDING,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  
}

static void
gtk_tree_view_column_init (GtkTreeViewColumn *tree_column)
{
  tree_column->button = NULL;
  tree_column->xalign = 0.0;
  tree_column->width = 0;
  tree_column->requested_width = -1;
  tree_column->min_width = -1;
  tree_column->max_width = -1;
  tree_column->column_type = GTK_TREE_VIEW_COLUMN_GROW_ONLY;
  tree_column->visible = TRUE;
  tree_column->clickable = FALSE;
  tree_column->dirty = TRUE;
  tree_column->sort_order = GTK_SORT_ASCENDING;
  tree_column->show_sort_indicator = FALSE;
  tree_column->property_changed_signal = 0;
  tree_column->sort_clicked_signal = 0;
  tree_column->sort_column_changed_signal = 0;
  tree_column->sort_column_id = -1;
  tree_column->reorderable = FALSE;
  tree_column->maybe_reordered = FALSE;
}

static void
gtk_tree_view_column_finalize (GObject *object)
{
  GtkTreeViewColumn *tree_column = (GtkTreeViewColumn *) object;
  GList *list;

  for (list = tree_column->cell_list; list; list = list->next)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *) list->data;
      if (info->func_data && info->destroy)
	(info->destroy) (info->func_data);
      gtk_tree_view_column_clear_attributes (tree_column, info->cell);
      g_object_unref (G_OBJECT (info->cell));
      g_free (info);
    }

  g_free (tree_column->title);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_tree_view_column_set_property (GObject         *object,
                                   guint            prop_id,
                                   const GValue    *value,
                                   GParamSpec      *pspec)
{
  GtkTreeViewColumn *tree_column;

  tree_column = GTK_TREE_VIEW_COLUMN (object);

  switch (prop_id)
    {
    case PROP_VISIBLE:
      gtk_tree_view_column_set_visible (tree_column,
                                        g_value_get_boolean (value));
      break;

    case PROP_SIZING:
      gtk_tree_view_column_set_sizing (tree_column,
                                       g_value_get_enum (value));
      break;

    case PROP_FIXED_WIDTH:
      gtk_tree_view_column_set_fixed_width (tree_column,
					    g_value_get_int (value));
      break;

    case PROP_MIN_WIDTH:
      gtk_tree_view_column_set_min_width (tree_column,
                                          g_value_get_int (value));
      break;

    case PROP_MAX_WIDTH:
      gtk_tree_view_column_set_max_width (tree_column,
                                          g_value_get_int (value));
      break;

    case PROP_TITLE:
      gtk_tree_view_column_set_title (tree_column,
                                      g_value_get_string (value));
      break;

    case PROP_CLICKABLE:
      gtk_tree_view_column_set_clickable (tree_column,
                                          g_value_get_boolean (value));
      break;

    case PROP_WIDGET:
      gtk_tree_view_column_set_widget (tree_column,
                                       (GtkWidget*) g_value_get_object (value));
      break;

    case PROP_ALIGNMENT:
      gtk_tree_view_column_set_alignment (tree_column,
                                          g_value_get_float (value));
      break;

    case PROP_REORDERABLE:
      gtk_tree_view_column_set_reorderable (tree_column,
					    g_value_get_boolean (value));
      break;

    case PROP_SORT_INDICATOR:
      gtk_tree_view_column_set_sort_indicator (tree_column,
                                               g_value_get_boolean (value));
      break;

    case PROP_SORT_ORDER:
      gtk_tree_view_column_set_sort_order (tree_column,
                                           g_value_get_enum (value));
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_view_column_get_property (GObject         *object,
                                   guint            prop_id,
                                   GValue          *value,
                                   GParamSpec      *pspec)
{
  GtkTreeViewColumn *tree_column;

  tree_column = GTK_TREE_VIEW_COLUMN (object);

  switch (prop_id)
    {
    case PROP_VISIBLE:
      g_value_set_boolean (value,
                           gtk_tree_view_column_get_visible (tree_column));
      break;

    case PROP_WIDTH:
      g_value_set_int (value,
                       gtk_tree_view_column_get_width (tree_column));
      break;

    case PROP_SIZING:
      g_value_set_enum (value,
                        gtk_tree_view_column_get_sizing (tree_column));
      break;

    case PROP_FIXED_WIDTH:
      g_value_set_int (value,
                       gtk_tree_view_column_get_fixed_width (tree_column));
      break;

    case PROP_MIN_WIDTH:
      g_value_set_int (value,
                       gtk_tree_view_column_get_min_width (tree_column));
      break;

    case PROP_MAX_WIDTH:
      g_value_set_int (value,
                       gtk_tree_view_column_get_max_width (tree_column));
      break;

    case PROP_TITLE:
      g_value_set_string (value,
                          gtk_tree_view_column_get_title (tree_column));
      break;

    case PROP_CLICKABLE:
      g_value_set_boolean (value,
                           gtk_tree_view_column_get_clickable (tree_column));
      break;

    case PROP_WIDGET:
      g_value_set_object (value,
                          (GObject*) gtk_tree_view_column_get_widget (tree_column));
      break;

    case PROP_ALIGNMENT:
      g_value_set_float (value,
                         gtk_tree_view_column_get_alignment (tree_column));
      break;

    case PROP_REORDERABLE:
      g_value_set_boolean (value,
			   gtk_tree_view_column_get_reorderable (tree_column));
      break;

    case PROP_SORT_INDICATOR:
      g_value_set_boolean (value,
                           gtk_tree_view_column_get_sort_indicator (tree_column));
      break;

    case PROP_SORT_ORDER:
      g_value_set_enum (value,
                        gtk_tree_view_column_get_sort_order (tree_column));
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Helper functions
 */

/* Button handling code
 */
static void
gtk_tree_view_column_create_button (GtkTreeViewColumn *tree_column)
{
  GtkTreeView *tree_view;
  GtkWidget *child;
  GtkWidget *hbox;

  tree_view = (GtkTreeView *) tree_column->tree_view;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (tree_column->button == NULL);

  gtk_widget_push_composite_child ();
  tree_column->button = gtk_button_new ();
  gtk_widget_pop_composite_child ();

  /* make sure we own a reference to it as well. */
  if (tree_view->priv->header_window)
    gtk_widget_set_parent_window (tree_column->button, tree_view->priv->header_window);
  gtk_widget_set_parent (tree_column->button, GTK_WIDGET (tree_view));
  
  gtk_signal_connect (GTK_OBJECT (tree_column->button), "realize",
		      (GtkSignalFunc) gtk_tree_view_column_button_realize,
		      NULL);

  gtk_signal_connect (GTK_OBJECT (tree_column->button), "event",
		      (GtkSignalFunc) gtk_tree_view_column_button_event,
		      (gpointer) tree_column);
  
  gtk_signal_connect (GTK_OBJECT (tree_column->button), "clicked",
		      (GtkSignalFunc) gtk_tree_view_column_button_clicked,
		      (gpointer) tree_column);

  tree_column->alignment = gtk_alignment_new (tree_column->xalign, 0.5, 0.0, 0.0);

  hbox = gtk_hbox_new (FALSE, 2);
  tree_column->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_IN);

  if (tree_column->child)
    child = tree_column->child;
  else
    {
      child = gtk_label_new (tree_column->title);
      gtk_widget_show (child);
    }

  if (tree_column->xalign <= 0.5)
    gtk_box_pack_end (GTK_BOX (hbox), tree_column->arrow, FALSE, FALSE, 0);
  else
    gtk_box_pack_start (GTK_BOX (hbox), tree_column->arrow, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), tree_column->alignment, TRUE, TRUE, 0);
        
  gtk_container_add (GTK_CONTAINER (tree_column->alignment), child);
  gtk_container_add (GTK_CONTAINER (tree_column->button), hbox);

  gtk_widget_show (hbox);
  gtk_widget_show (tree_column->alignment);
  gtk_tree_view_column_update_button (tree_column);
}

static void 
gtk_tree_view_column_update_button (GtkTreeViewColumn *tree_column)
{
  GtkWidget *hbox;
  GtkWidget *alignment;
  GtkWidget *arrow;
  GtkWidget *current_child;

  /* Create a button if necessary */
  if (tree_column->visible &&
      tree_column->button == NULL &&
      tree_column->tree_view &&
      GTK_WIDGET_REALIZED (tree_column->tree_view))
    gtk_tree_view_column_create_button (tree_column);
  
  if (! tree_column->button)
    return;

  hbox = GTK_BIN (tree_column->button)->child;
  alignment = tree_column->alignment;
  arrow = tree_column->arrow;
  current_child = GTK_BIN (alignment)->child;

  /* Set up the actual button */
  gtk_alignment_set (GTK_ALIGNMENT (alignment), tree_column->xalign,
		     0.5, 0.0, 0.0);
      
  if (tree_column->child)
    {
      if (current_child != tree_column->child)
	{
	  gtk_container_remove (GTK_CONTAINER (alignment),
				current_child);
	  gtk_container_add (GTK_CONTAINER (alignment),
			     tree_column->child);
	}
    }
  else 
    {
      if (current_child == NULL)
	{
	  current_child = gtk_label_new (NULL);
	  gtk_widget_show (current_child);
	  gtk_container_add (GTK_CONTAINER (alignment),
			     current_child);
	}

      g_return_if_fail (GTK_IS_LABEL (current_child));

      if (tree_column->title)
	gtk_label_set_text (GTK_LABEL (current_child),
			    tree_column->title);
      else
	gtk_label_set_text (GTK_LABEL (current_child),
			    "");
    }

  switch (tree_column->sort_order)
    {
    case GTK_SORT_ASCENDING:
      gtk_arrow_set (GTK_ARROW (arrow),
		     GTK_ARROW_DOWN,
		     GTK_SHADOW_IN);
      break;

    case GTK_SORT_DESCENDING:
      gtk_arrow_set (GTK_ARROW (arrow),
		     GTK_ARROW_UP,
		     GTK_SHADOW_IN);
      break;
          
    default:
      g_warning (G_STRLOC": bad sort order");
      break;
    }

  /* Put arrow on the right if the text is left-or-center justified,
       * and on the left otherwise; do this by packing boxes, so flipping
       * text direction will reverse things
       */
  gtk_widget_ref (arrow);
  gtk_container_remove (GTK_CONTAINER (hbox), arrow);

  if (tree_column->xalign <= 0.5)
    {
      gtk_box_pack_end (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
    }
  else
    {
      gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
      /* move it to the front */
      gtk_box_reorder_child (GTK_BOX (hbox), arrow, 0);
    }
  gtk_widget_unref (arrow);

  if (tree_column->show_sort_indicator)
    gtk_widget_show (arrow);
  else
    gtk_widget_hide (arrow);

  /* It's always safe to hide the button.  It isn't always safe to show it, as if you show it
   * before it's realized, it'll get the wrong window. */
  if (tree_column->button &&
      tree_column->tree_view != NULL &&
      GTK_WIDGET_REALIZED (tree_column->tree_view))
    {
      if (tree_column->visible)
	{
	  gtk_widget_show_now (tree_column->button);
	  if (tree_column->window)
	    {
	      if (tree_column->column_type == GTK_TREE_VIEW_COLUMN_RESIZABLE)
		{
		  gdk_window_show (tree_column->window);
		  gdk_window_raise (tree_column->window);
		}
	      else
		{
		  gdk_window_hide (tree_column->window);
		}
	    }
	}
      else
	{
	  gtk_widget_hide (tree_column->button);
	  if (tree_column->window)
	    gdk_window_hide (tree_column->window);
	}
    }
  
  if (tree_column->reorderable || tree_column->clickable)
    {
      GTK_WIDGET_SET_FLAGS (tree_column->button, GTK_CAN_FOCUS);
    }
  else
    {
      GTK_WIDGET_UNSET_FLAGS (tree_column->button, GTK_CAN_FOCUS);
      if (GTK_WIDGET_HAS_FOCUS (tree_column->button))
	{
	  GtkWidget *toplevel = gtk_widget_get_toplevel (tree_column->tree_view);
	  if (GTK_WIDGET_TOPLEVEL (toplevel))
	    {
	      gtk_window_set_focus (GTK_WINDOW (toplevel), NULL);
	    }
	}
    }

  gtk_tree_view_column_cell_set_dirty (tree_column);
}

/* Button signal handlers
 */

static gint
gtk_tree_view_column_button_event (GtkWidget *widget,
				   GdkEvent  *event,
				   gpointer   data)
{
  GtkTreeViewColumn *column = (GtkTreeViewColumn *) data;

  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type == GDK_BUTTON_PRESS &&
      column->reorderable)
    {
      column->maybe_reordered = TRUE;
      gdk_window_get_pointer (widget->window,
			      &column->drag_x,
			      &column->drag_y,
			      NULL);
      gtk_widget_grab_focus (widget);
    }

  if (event->type == GDK_BUTTON_RELEASE &&
      column->maybe_reordered)
    column->maybe_reordered = FALSE;

  if (event->type == GDK_MOTION_NOTIFY &&
      (column->maybe_reordered) &&
      (gtk_drag_check_threshold (widget,
				 column->drag_x,
				 column->drag_y,
				 (gint) ((GdkEventMotion *)event)->x,
				 (gint) ((GdkEventMotion *)event)->y)))
    {
      column->maybe_reordered = FALSE;
      _gtk_tree_view_column_start_drag (GTK_TREE_VIEW (column->tree_view), column);
      return TRUE;
    }
  if (column->clickable == FALSE)
    {
      switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_RELEASE:
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
	  return TRUE;
	default:
	  return FALSE;
	}
    }
  return FALSE;
}

static void
gtk_tree_view_column_button_realize (GtkWidget *widget, gpointer data)
{
  gdk_window_set_events (widget->window, gdk_window_get_events (widget->window) | GDK_POINTER_MOTION_MASK);
}

static void
gtk_tree_view_column_button_clicked (GtkWidget *widget, gpointer data)
{
  g_signal_emit_by_name (G_OBJECT (data), "clicked");
}

static void
gtk_tree_view_model_sort_column_changed (GtkTreeSortable   *sortable,
					  GtkTreeViewColumn *column)
{
  gint sort_column_id;
  GtkSortType order;

  if (gtk_tree_sortable_get_sort_column_id (sortable,
					    &sort_column_id,
					    &order))
    {
      if (sort_column_id == column->sort_column_id)
	{
	  gtk_tree_view_column_set_sort_indicator (column, TRUE);
	  gtk_tree_view_column_set_sort_order (column, order);
	}
      else
	{
	  gtk_tree_view_column_set_sort_indicator (column, FALSE);
	}
    }
  else
    {
      gtk_tree_view_column_set_sort_indicator (column, FALSE);
    }
}

static void
gtk_tree_view_column_sort (GtkTreeViewColumn *tree_column,
			   gpointer           data)
{
  gint sort_column_id;
  GtkSortType order;
  gboolean has_sort_column;
  gboolean has_default_sort_func;

  g_return_if_fail (tree_column->tree_view != NULL);

  has_sort_column =
    gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (GTK_TREE_VIEW (tree_column->tree_view)->priv->model),
					  &sort_column_id,
					  &order);
  has_default_sort_func =
    gtk_tree_sortable_has_default_sort_func (GTK_TREE_SORTABLE (GTK_TREE_VIEW (tree_column->tree_view)->priv->model));

  if (has_sort_column &&
      sort_column_id == tree_column->sort_column_id)
    {
      if (order == GTK_SORT_ASCENDING)
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (GTK_TREE_VIEW (tree_column->tree_view)->priv->model),
					      tree_column->sort_column_id,
					      GTK_SORT_DESCENDING);
      else if (order == GTK_SORT_DESCENDING && has_default_sort_func)
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (GTK_TREE_VIEW (tree_column->tree_view)->priv->model),
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);
      else
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (GTK_TREE_VIEW (tree_column->tree_view)->priv->model),
					      tree_column->sort_column_id,
					      GTK_SORT_ASCENDING);
    }
  else
    {
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (GTK_TREE_VIEW (tree_column->tree_view)->priv->model),
					    tree_column->sort_column_id,
					    GTK_SORT_ASCENDING);
    }
}


static void
gtk_tree_view_column_setup_sort_column_id_callback (GtkTreeViewColumn *tree_column)
{
  GtkTreeModel *model;

  if (tree_column->tree_view == NULL)
    return;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_column->tree_view));

  if (model == NULL)
    return;

  if (GTK_IS_TREE_SORTABLE (model) &&
      tree_column->sort_column_id != -1)
    {
      gint real_sort_column_id;
      GtkSortType real_order;

      if (tree_column->sort_column_changed_signal == 0)
	tree_column->sort_column_changed_signal =
	  g_signal_connect (G_OBJECT (model), "sort_column_changed",
                            GTK_SIGNAL_FUNC (gtk_tree_view_model_sort_column_changed),
                            tree_column);
      
      if (gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (model),
						&real_sort_column_id,
						&real_order) &&
	  (real_sort_column_id == tree_column->sort_column_id))
	{
	  gtk_tree_view_column_set_sort_indicator (tree_column, TRUE);
	  gtk_tree_view_column_set_sort_order (tree_column, real_order);

	  return;
	}
    }
}


/* Exported Private Functions.
 * These should only be called by gtktreeview.c or gtktreeviewcolumn.c
 */

void
_gtk_tree_view_column_realize_button (GtkTreeViewColumn *column)
{
  GtkTreeView *tree_view;
  GdkWindowAttr attr;
  guint attributes_mask;

  tree_view = (GtkTreeView *)column->tree_view;

  g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  g_return_if_fail (GTK_WIDGET_REALIZED (tree_view));
  g_return_if_fail (tree_view->priv->header_window != NULL);
  g_return_if_fail (column->button != NULL);

  gtk_widget_set_parent_window (column->button, tree_view->priv->header_window);

  if (column->visible)
    gtk_widget_show (column->button);

  attr.window_type = GDK_WINDOW_CHILD;
  attr.wclass = GDK_INPUT_ONLY;
  attr.visual = gtk_widget_get_visual (GTK_WIDGET (tree_view));
  attr.colormap = gtk_widget_get_colormap (GTK_WIDGET (tree_view));
  attr.event_mask = gtk_widget_get_events (GTK_WIDGET (tree_view));
  attr.event_mask = (GDK_BUTTON_PRESS_MASK |
		     GDK_BUTTON_RELEASE_MASK |
		     GDK_POINTER_MOTION_MASK |
		     GDK_POINTER_MOTION_HINT_MASK |
		     GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_CURSOR | GDK_WA_X | GDK_WA_Y;
  attr.cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
  tree_view->priv->cursor_drag = attr.cursor;

  attr.y = 0;
  attr.width = TREE_VIEW_DRAG_WIDTH;
  attr.height = tree_view->priv->header_height;

  attr.x = (column->button->allocation.x + column->button->allocation.width) - 3;
          
  column->window = gdk_window_new (tree_view->priv->header_window,
				   &attr, attributes_mask);
  gdk_window_set_user_data (column->window, tree_view);

  gtk_tree_view_column_update_button (column);
}

void
_gtk_tree_view_column_unrealize_button (GtkTreeViewColumn *column)
{
  g_return_if_fail (column != NULL);
  g_return_if_fail (column->window != NULL);

  gdk_window_set_user_data (column->window, NULL);
  gdk_window_destroy (column->window);
  column->window = NULL;
}

void
_gtk_tree_view_column_set_tree_view (GtkTreeViewColumn *column,
				     GtkTreeView       *tree_view)
{
  g_assert (column->tree_view == NULL);

  column->tree_view = GTK_WIDGET (tree_view);
  gtk_tree_view_column_create_button (column);

  column->property_changed_signal =
	  g_signal_connect_swapped (GTK_OBJECT (tree_view),
				    "notify::model",
				    GTK_SIGNAL_FUNC (gtk_tree_view_column_setup_sort_column_id_callback),
				    column);

  gtk_tree_view_column_setup_sort_column_id_callback (column);
}

void
_gtk_tree_view_column_unset_tree_view (GtkTreeViewColumn *column)
{
  if (column->tree_view && column->button)
    {
      gtk_container_remove (GTK_CONTAINER (column->tree_view), column->button);
    }
  if (column->property_changed_signal)
    {
      g_signal_handler_disconnect (G_OBJECT (column->tree_view), column->property_changed_signal);
      column->property_changed_signal = 0;
    }

  if (column->sort_column_changed_signal)
    {
      g_signal_handler_disconnect (G_OBJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (column->tree_view))),
				   column->sort_column_changed_signal);
      column->sort_column_changed_signal = 0;
    }

  column->tree_view = NULL;
  column->button = NULL;
}

void
_gtk_tree_view_column_set_width (GtkTreeViewColumn *tree_column,
				 gint               width)
{
  if (tree_column->min_width != -1 &&
      width <= tree_column->min_width)
    width = tree_column->min_width;
  else if (tree_column->max_width != -1 &&
           width > tree_column->max_width)
    width = tree_column->max_width;
  
  if (tree_column->width == width)
    return;
  
  tree_column->width = width;

  g_object_notify (G_OBJECT (tree_column), "width");

  if (tree_column->tree_view != NULL)
    gtk_widget_queue_resize (tree_column->tree_view);

}

/* Public Functions */


/**
 * gtk_tree_view_column_new:
 * 
 * Creates a new #GtkTreeViewColumn.
 * 
 * Return value: A newly created #GtkTreeViewColumn.
 **/
GtkTreeViewColumn *
gtk_tree_view_column_new (void)
{
  GtkTreeViewColumn *tree_column;

  tree_column = GTK_TREE_VIEW_COLUMN (gtk_type_new (GTK_TYPE_TREE_VIEW_COLUMN));

  return tree_column;
}

/**
 * gtk_tree_view_column_new_with_attributes:
 * @title: The title to set the header to.
 * @cell: The #GtkCellRenderer.
 * @Varargs: A NULL terminated list of attributes.
 * 
 * Creates a new #GtkTreeViewColumn with a number of default values.  This is
 * equivalent to calling @gtk_tree_view_column_set_title,
 * @gtk_tree_view_column_pack_start, and
 * @gtk_tree_view_column_set_attributes on the newly created #GtkTreeViewColumn.
 * 
 * Return value: A newly created #GtkTreeViewColumn.
 **/
GtkTreeViewColumn *
gtk_tree_view_column_new_with_attributes (const gchar     *title,
					  GtkCellRenderer *cell,
					  ...)
{
  GtkTreeViewColumn *retval;
  va_list args;

  retval = gtk_tree_view_column_new ();

  gtk_tree_view_column_set_title (retval, title);
  gtk_tree_view_column_pack_start (retval, cell, TRUE);

  va_start (args, cell);
  gtk_tree_view_column_set_attributesv (retval, cell, args);
  va_end (args);

  return retval;
}

static GtkTreeViewColumnCellInfo *
gtk_tree_view_column_get_cell_info (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer   *cell_renderer)
{
  GList *list;
  for (list = tree_column->cell_list; list; list = list->next)
    if (((GtkTreeViewColumnCellInfo *)list->data)->cell == cell_renderer)
      return (GtkTreeViewColumnCellInfo *) list->data;
  return NULL;
}


/**
 * gtk_tree_view_column_pack_start:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell: The #GtkCellRenderer, 
 * @expand: TRUE if @cell is to be given extra space allocated to box.
 * 
 **/
void
gtk_tree_view_column_pack_start (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 gboolean           expand)
{
  GtkTreeViewColumnCellInfo *cell_info;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (! gtk_tree_view_column_get_cell_info (tree_column, cell));

  g_object_ref (G_OBJECT (cell));
  gtk_object_sink (GTK_OBJECT (cell));

  cell_info = g_new0 (GtkTreeViewColumnCellInfo, 1);
  cell_info->cell = cell;
  cell_info->expand = expand ? TRUE : FALSE;
  cell_info->pack = GTK_PACK_START;
  cell_info->has_focus = 0;
  cell_info->attributes = NULL;

  tree_column->cell_list = g_list_append (tree_column->cell_list, cell_info);
}

void
gtk_tree_view_column_pack_end (GtkTreeViewColumn  *tree_column,
			       GtkCellRenderer    *cell,
			       gboolean            expand)
{
  GtkTreeViewColumnCellInfo *cell_info;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (! gtk_tree_view_column_get_cell_info (tree_column, cell));

  g_object_ref (G_OBJECT (cell));
  gtk_object_sink (GTK_OBJECT (cell));

  cell_info = g_new (GtkTreeViewColumnCellInfo, 1);
  cell_info->cell = cell;
  cell_info->expand = expand ? TRUE : FALSE;
  cell_info->pack = GTK_PACK_END;
  cell_info->has_focus = 0;
  cell_info->attributes = NULL;

  tree_column->cell_list = g_list_append (tree_column->cell_list, cell_info);
}


void
gtk_tree_view_column_clear (GtkTreeViewColumn *tree_column)
{
  GList *list;
  g_return_if_fail (tree_column != NULL);

  for (list = tree_column->cell_list; list; list = list->next)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *)list->data;

      g_object_unref (G_OBJECT (info->cell));
      gtk_tree_view_column_clear_attributes (tree_column, info->cell);
      g_free (info);
    }

  g_list_free (tree_column->cell_list);
  tree_column->cell_list = NULL;
}

GList *
gtk_tree_view_column_get_cell_renderers (GtkTreeViewColumn *tree_column)
{
  GList *retval = NULL, *list;

  g_return_val_if_fail (tree_column != NULL, NULL);

  for (list = tree_column->cell_list; list; list = list->next)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *)list->data;

      retval = g_list_append (retval, info->cell);
    }

  return retval;
}

/**
 * gtk_tree_view_column_add_attribute:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell_renderer: the #GtkCellRenderer to set attributes on
 * @attribute: An attribute on the renderer
 * @column: The column position on the model to get the attribute from.
 * 
 * Adds an attribute mapping to the list in @tree_column.  The @column is the
 * column of the model to get a value from, and the @attribute is the
 * parameter on @cell_renderer to be set from the value. So for example
 * if column 2 of the model contains strings, you could have the
 * "text" attribute of a #GtkCellRendererText get its values from
 * column 2.
 **/
void
gtk_tree_view_column_add_attribute (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer   *cell_renderer,
				    const gchar       *attribute,
				    gint               column)
{
  GtkTreeViewColumnCellInfo *info;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  info = gtk_tree_view_column_get_cell_info (tree_column, cell_renderer);
  g_return_if_fail (info != NULL);

  info->attributes = g_slist_prepend (info->attributes, GINT_TO_POINTER (column));
  info->attributes = g_slist_prepend (info->attributes, g_strdup (attribute));

  if (tree_column->tree_view)
    gtk_tree_view_column_cell_set_dirty (tree_column);

}

static void
gtk_tree_view_column_set_attributesv (GtkTreeViewColumn *tree_column,
				      GtkCellRenderer   *cell_renderer,
				      va_list            args)
{
  gchar *attribute;
  gint column;

  attribute = va_arg (args, gchar *);

  gtk_tree_view_column_clear_attributes (tree_column, cell_renderer);
  
  while (attribute != NULL)
    {
      column = va_arg (args, gint);
      gtk_tree_view_column_add_attribute (tree_column, cell_renderer, attribute, column);
      attribute = va_arg (args, gchar *);
    }
}

/**
 * gtk_tree_view_column_set_attributes:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell_renderer: the #GtkCellRenderer we're setting the attributes of
 * @Varargs: A NULL terminated listing of attributes.
 * 
 * Sets the attributes in the list as the attributes of @tree_column.
 * The attributes should be in attribute/column order, as in
 * @gtk_tree_view_column_add_attribute. All existing attributes
 * are removed, and replaced with the new attributes.
 **/
void
gtk_tree_view_column_set_attributes (GtkTreeViewColumn *tree_column,
				     GtkCellRenderer   *cell_renderer,
				     ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell_renderer));
  g_return_if_fail (gtk_tree_view_column_get_cell_info (tree_column, cell_renderer));

  va_start (args, cell_renderer);
  gtk_tree_view_column_set_attributesv (tree_column, cell_renderer, args);
  va_end (args);
}


/**
 * gtk_tree_view_column_set_cell_data_func:
 * @tree_column: A #GtkTreeViewColumn
 * @cell_renderer: A #GtkCellRenderer
 * @func: The #GtkTreeViewColumnFunc to use. 
 * @func_data: The user data for @func.
 * @destroy: The destroy notification for @func_data
 * 
 * Sets the #GtkTreeViewColumnFunc to use for the column.  This
 * function is used instead of the standard attributes mapping for
 * setting the column value, and should set the value of @tree_column's
 * cell renderer as appropriate.  @func may be NULL to remove an
 * older one.
 **/
void
gtk_tree_view_column_set_cell_data_func (GtkTreeViewColumn   *tree_column,
					 GtkCellRenderer     *cell_renderer,
					 GtkTreeCellDataFunc  func,
					 gpointer             func_data,
					 GtkDestroyNotify     destroy)
{
  GtkTreeViewColumnCellInfo *info;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell_renderer));
  info = gtk_tree_view_column_get_cell_info (tree_column, cell_renderer);

  g_return_if_fail (info != NULL);

  if (func == info->func &&
      func_data == info->func_data &&
      destroy == info->destroy)
    return;

  if (info->func_data && info->destroy)
    (info->destroy) (info->func_data);

  info->func = func;
  info->func_data = func_data;
  info->destroy = destroy;

  if (tree_column->tree_view)
    gtk_tree_view_column_cell_set_dirty (tree_column);
}


/**
 * gtk_tree_view_column_clear_attributes:
 * @tree_column: a #GtkTreeViewColumn
 *@cell_renderer: a #GtkCellRenderer to clear the attribute mapping on.
 * 
 * Clears all existing attributes previously set with
 * gtk_tree_view_column_set_attributes().
 **/
void
gtk_tree_view_column_clear_attributes (GtkTreeViewColumn *tree_column,
				       GtkCellRenderer   *cell_renderer)
{
  GtkTreeViewColumnCellInfo *info;
  GSList *list;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell_renderer));
  info = gtk_tree_view_column_get_cell_info (tree_column, cell_renderer);

  list = info->attributes;

  while (list && list->next)
    {
      g_free (list->data);
      list = list->next->next;
    }
  g_slist_free (info->attributes);
  info->attributes = NULL;

  if (tree_column->tree_view)
    gtk_tree_view_column_cell_set_dirty (tree_column);
}


/**
 * gtk_tree_view_column_set_spacing:
 * @tree_column: A #GtkTreeViewColumn.
 * @spacing: distance between cell renderers in pixels.
 * 
 * Sets the spacing field of @tree_column, which is the number of pixels to
 * place between cell renderers packed into it.
 **/
void
gtk_tree_view_column_set_spacing (GtkTreeViewColumn *tree_column,
				  gint               spacing)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (spacing >= 0);

  if (tree_column->spacing == spacing)
    return;

  tree_column->spacing = spacing;
  if (tree_column->tree_view)
    gtk_tree_view_column_cell_set_dirty (tree_column);
}

/**
 * gtk_tree_view_column_get_spacing:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the spacing of @tree_column.
 **/
gint
gtk_tree_view_column_get_spacing (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->spacing;
}

/* Options for manipulating the columns */

/**
 * gtk_tree_view_column_set_visible:
 * @tree_column: A #GtkTreeViewColumn.
 * @visible: TRUE if the @tree_column is visible.
 * 
 * Sets the visibility of @tree_column.
 **/
void
gtk_tree_view_column_set_visible (GtkTreeViewColumn *tree_column,
				  gboolean           visible)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  visible = !! visible;
  
  if (tree_column->visible == visible)
    return;

  tree_column->visible = visible;

  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "visible");
}

/**
 * gtk_tree_view_column_get_visible:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns TRUE if @tree_column is visible.
 * 
 * Return value: whether the column is visible or not.  If it is visible, then
 * the tree will show the column.
 **/
gboolean
gtk_tree_view_column_get_visible (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->visible;
}

/**
 * gtk_tree_view_column_set_sizing:
 * @tree_column: A #GtkTreeViewColumn.
 * @type: The #GtkTreeViewColumnSizing.
 * 
 * Sets the growth behavior of @tree_column to @type.
 **/
void
gtk_tree_view_column_set_sizing (GtkTreeViewColumn       *tree_column,
                                 GtkTreeViewColumnSizing  type)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (type == tree_column->column_type)
    return;

  if (tree_column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE &&
      tree_column->requested_width != -1)
    {
      gtk_tree_view_column_set_sizing (tree_column, tree_column->requested_width);
    }
  tree_column->column_type = type;

  gtk_tree_view_column_update_button (tree_column);

  if (type != GTK_TREE_VIEW_COLUMN_AUTOSIZE)
  g_object_notify (G_OBJECT (tree_column), "sizing");
}

/**
 * gtk_tree_view_column_get_sizing:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current type of @tree_column.
 * 
 * Return value: The type of @tree_column.
 **/
GtkTreeViewColumnSizing
gtk_tree_view_column_get_sizing (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->column_type;
}

/**
 * gtk_tree_view_column_get_width:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current size of the @tree_column in pixels.
 * 
 * Return value: The current width of the @tree_column.
 **/
gint
gtk_tree_view_column_get_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->width;
}

/**
 * gtk_tree_view_column_set_fixed_width:
 * @tree_column: A #GtkTreeViewColumn.
 * @fixed_width: The size to set the @tree_column to. Must be greater than 0.
 * 
 * Sets the size of the column in pixels.  This is meaningful only if the sizing
 * type is #GTK_TREE_VIEW_COLUMN_FIXED.  In this case, the value is discarded
 * as the size of the column is based on the calculated width of the column. The
 * width is clamped to the min/max width for the column.
 **/
void
gtk_tree_view_column_set_fixed_width (GtkTreeViewColumn *tree_column,
				      gint               fixed_width)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (fixed_width > 0);

  if (tree_column->column_type == GTK_TREE_VIEW_COLUMN_AUTOSIZE)
    return;

  tree_column->fixed_width = fixed_width;
  tree_column->requested_width = fixed_width;
  _gtk_tree_view_column_set_width (tree_column, fixed_width);
}

/**
 * gtk_tree_view_column_get_fixed_width:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Gets the fixed width of the column.  This value is only meaning may not be the
 * actual width of the column on the screen, just what is requested.
 * 
 * Return value: the fixed width of the column
 **/
gint
gtk_tree_view_column_get_fixed_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->fixed_width;
}

/**
 * gtk_tree_view_column_set_min_width:
 * @tree_column: A #GtkTreeViewColumn.
 * @min_width: The minimum width of the column in pixels, or -1.
 * 
 * Sets the minimum width of the @tree_column.  If @min_width is -1, then the
 * minimum width is unset.
 **/
void
gtk_tree_view_column_set_min_width (GtkTreeViewColumn *tree_column,
				    gint               min_width)
{
  gint real_min_width;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (min_width >= -1);

  if (min_width == tree_column->min_width)
    return;

  if (tree_column->tree_view == NULL)
    {
      tree_column->min_width = min_width;
      return;
    }

  real_min_width = (tree_column->min_width == -1) ?
    tree_column->button->requisition.width : tree_column->min_width;

  /* We want to queue a resize if the either the old min_size or the
   * new min_size determined the size of the column */
  if (GTK_WIDGET_REALIZED (tree_column->tree_view))
    {
      if ((tree_column->min_width > tree_column->width) ||
	  (tree_column->min_width == -1 &&
	   tree_column->button->requisition.width > tree_column->width) ||
	  (min_width > tree_column->width) ||
	  (min_width == -1 &&
	   tree_column->button->requisition.width > tree_column->width))
	gtk_widget_queue_resize (tree_column->tree_view);
    }

  if (tree_column->max_width != -1 && tree_column->max_width < real_min_width)
    tree_column->max_width = real_min_width;

  tree_column->min_width = min_width;

  g_object_notify (G_OBJECT (tree_column), "min_width");
}

/**
 * gtk_tree_view_column_get_min_width:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the minimum width in pixels of the @tree_column, or -1 if no minimum
 * width is set.
 * 
 * Return value: The minimum width of the @tree_column.
 **/
gint
gtk_tree_view_column_get_min_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), -1);

  return tree_column->min_width;
}

/**
 * gtk_tree_view_column_set_max_width:
 * @tree_column: A #GtkTreeViewColumn.
 * @max_width: The maximum width of the column in pixels, or -1.
 * 
 * Sets the maximum width of the @tree_column.  If @max_width is -1, then the
 * maximum width is unset.  Note, the column can actually be wider than max
 * width if it's the last column in a view.  In this case, the column expands to
 * fill the view.
 **/
void
gtk_tree_view_column_set_max_width (GtkTreeViewColumn *tree_column,
				    gint               max_width)
{
  gint real_max_width;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (max_width >= -1);

  if (max_width == tree_column->max_width)
    return;

  if (tree_column->tree_view == NULL)
    {
      tree_column->max_width = max_width;
      return;
    }

  real_max_width = tree_column->max_width == -1 ?
    tree_column->button->requisition.width : tree_column->max_width;

  if (tree_column->tree_view &&
      GTK_WIDGET_REALIZED (tree_column->tree_view) &&
      ((tree_column->max_width < tree_column->width) ||
       (max_width != -1 && max_width < tree_column->width)))
    gtk_widget_queue_resize (tree_column->tree_view);

  tree_column->max_width = max_width;

  if (real_max_width > max_width)
    tree_column->max_width = max_width;

  g_object_notify (G_OBJECT (tree_column), "max_width");
}

/**
 * gtk_tree_view_column_get_max_width:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the maximum width in pixels of the @tree_column, or -1 if no maximum
 * width is set.
 * 
 * Return value: The maximum width of the @tree_column.
 **/
gint
gtk_tree_view_column_get_max_width (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), -1);

  return tree_column->max_width;
}

/**
 * gtk_tree_view_column_clicked:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Emits the "clicked" signal on the column.  This function will only work if
 * the user could have conceivably clicked on the button.
 **/
void
gtk_tree_view_column_clicked (GtkTreeViewColumn *tree_column)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (tree_column->visible &&
      tree_column->button &&
      tree_column->clickable)
    gtk_button_clicked (GTK_BUTTON (tree_column->button));
}

/**
 * gtk_tree_view_column_set_title:
 * @tree_column: A #GtkTreeViewColumn.
 * @title: The title of the @tree_column.
 * 
 * Sets the title of the @tree_column.  If a custom widget has been set, then
 * this value is ignored.
 **/
void
gtk_tree_view_column_set_title (GtkTreeViewColumn *tree_column,
				const gchar       *title)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  g_free (tree_column->title);
  if (title)
    tree_column->title = g_strdup (title);
  else
    tree_column->title = NULL;

  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "title");
}

/**
 * gtk_tree_view_column_get_title:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the title of the widget.  This value should not be modified.
 * 
 * Return value: the title of the column.
 **/
G_CONST_RETURN gchar *
gtk_tree_view_column_get_title (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);

  return tree_column->title;
}

/**
 * gtk_tree_view_column_set_clickable:
 * @tree_column: A #GtkTreeViewColumn.
 * @clickable: TRUE if the header is active.
 * 
 * Sets the header to be active if @active is TRUE.  When the header is active,
 * then it can take keyboard focus, and can be clicked.
 **/
void
gtk_tree_view_column_set_clickable (GtkTreeViewColumn *tree_column,
                                    gboolean           clickable)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (tree_column->clickable == (clickable?TRUE:FALSE))
    return;

  tree_column->clickable = (clickable?TRUE:FALSE);
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "clickable");
}

/**
 * gtk_tree_view_column_get_clickable:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Returns %TRUE if the user can click on the header for the column.
 * 
 * Return value: whether the user can click the column header
 **/
gboolean
gtk_tree_view_column_get_clickable (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->clickable;
}

/**
 * gtk_tree_view_column_set_widget:
 * @tree_column: A #GtkTreeViewColumn.
 * @widget: A child #GtkWidget, or NULL.
 * 
 * Sets the widget in the header to be @widget.  If widget is NULL, then the
 * header button is set with a #GtkLabel set to the title of @tree_column.
 **/
void
gtk_tree_view_column_set_widget (GtkTreeViewColumn *tree_column,
				 GtkWidget         *widget)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (widget)
    {
      gtk_object_ref (GTK_OBJECT (widget));
      gtk_object_sink (GTK_OBJECT (widget));
    }

  if (tree_column->child)      
    gtk_object_unref (GTK_OBJECT (tree_column->child));

  tree_column->child = widget;
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "widget");
}

/**
 * gtk_tree_view_column_get_widget:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the #GtkWidget in the button in the column header.  If a custom
 * widget has not been set, then this will be a #GtkAlignment with a #GtkLabel
 * in it.
 * 
 * Return value: The #GtkWidget in the column header.
 **/
GtkWidget *
gtk_tree_view_column_get_widget (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), NULL);

  return tree_column->child;
}

/**
 * gtk_tree_view_column_set_alignment:
 * @tree_column: A #GtkTreeViewColumn.
 * @xalign: The alignment, which is between [0.0 and 1.0] inclusive.
 * 
 * Sets the alignment of the title or custom widget inside the column header.
 * The alignment determines its location inside the button -- 0.0 for left, 0.5
 * for center, 1.0 for right.
 **/
void
gtk_tree_view_column_set_alignment (GtkTreeViewColumn *tree_column,
                                    gfloat             xalign)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  xalign = CLAMP (xalign, 0.0, 1.0);

  if (tree_column->xalign == xalign)
    return;

  tree_column->xalign = xalign;
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "alignment");
}

/**
 * gtk_tree_view_column_get_alignment:
 * @tree_column: A #GtkTreeViewColumn.
 * 
 * Returns the current x alignment of @tree_column.  This value can range
 * between 0.0 and 1.0.
 * 
 * Return value: The current alignent of @tree_column.
 **/
gfloat
gtk_tree_view_column_get_alignment (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0.5);

  return tree_column->xalign;
}

void
gtk_tree_view_column_set_reorderable (GtkTreeViewColumn *tree_column,
				      gboolean           reorderable)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  /*  if (reorderable)
      gtk_tree_view_column_set_clickable (tree_column, TRUE);*/

  if (tree_column->reorderable == (reorderable?TRUE:FALSE))
    return;

  tree_column->reorderable = (reorderable?TRUE:FALSE);
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "reorderable");
}

gboolean
gtk_tree_view_column_get_reorderable (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->reorderable;
}


/**
 * gtk_tree_view_column_set_sort_column_id:
 * @tree_column: a #GtkTreeViewColumn
 * @sort_column_id: The sort_column_id of the model to sort on.
 * 
 * Sets the logical sort_column_id that this column sorts on when this column is
 * selected for sorting.  Doing so makes the column header clickable.
 **/
void
gtk_tree_view_column_set_sort_column_id (GtkTreeViewColumn *tree_column,
					 gint               sort_column_id)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (sort_column_id >= 0);

  if (tree_column->sort_column_id == sort_column_id)
    return;

  tree_column->sort_column_id = sort_column_id;

  /* Handle unsetting the id */
  if (sort_column_id == -1)
    {
      if (tree_column->sort_clicked_signal)
	{
	  g_signal_handler_disconnect (G_OBJECT (tree_column), tree_column->sort_clicked_signal);
	  tree_column->sort_clicked_signal = 0;
	}

      if (tree_column->sort_column_changed_signal)
	{
	  g_signal_handler_disconnect (G_OBJECT (tree_column), tree_column->sort_column_changed_signal);
	  tree_column->sort_column_changed_signal = 0;
	}

      gtk_tree_view_column_set_sort_order (tree_column, GTK_SORT_ASCENDING);
      gtk_tree_view_column_set_sort_indicator (tree_column, FALSE);
      return;
    }

  gtk_tree_view_column_set_clickable (tree_column, TRUE);

  if (! tree_column->sort_clicked_signal)
    tree_column->sort_clicked_signal = g_signal_connect (G_OBJECT (tree_column),
                                                         "clicked",
                                                         G_CALLBACK (gtk_tree_view_column_sort),
                                                         NULL);

  gtk_tree_view_column_setup_sort_column_id_callback (tree_column);
}

/**
 * gtk_tree_view_column_get_sort_column_id:
 * @tree_column: a #GtkTreeViewColumn
 *
 * Gets the logical sort_column_id that the model sorts on when this
 * coumn is selected for sorting. See gtk_tree_view_column_set_sort_column_id().
 *
 * Return value: the current sort_column_id for this column, or -1 if
 *               this column can't be used for sorting.
 **/
gint
gtk_tree_view_column_get_sort_column_id (GtkTreeViewColumn *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->sort_column_id;
}

/**
 * gtk_tree_view_column_set_sort_indicator:
 * @tree_column: a #GtkTreeViewColumn
 * @setting: %TRUE to display an indicator that the column is sorted
 *
 * Call this function with a @setting of %TRUE to display an arrow in
 * the header button indicating the column is sorted. Call
 * gtk_tree_view_column_set_sort_order() to change the direction of
 * the arrow.
 * 
 **/
void
gtk_tree_view_column_set_sort_indicator (GtkTreeViewColumn     *tree_column,
                                         gboolean               setting)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  setting = setting != FALSE;

  if (setting == tree_column->show_sort_indicator)
    return;

  tree_column->show_sort_indicator = setting;
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "sort_indicator");
}

/**
 * gtk_tree_view_column_get_sort_indicator:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Gets the value set by gtk_tree_view_column_set_sort_indicator().
 * 
 * Return value: whether the sort indicator arrow is displayed
 **/
gboolean
gtk_tree_view_column_get_sort_indicator  (GtkTreeViewColumn     *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  return tree_column->show_sort_indicator;
}

/**
 * gtk_tree_view_column_set_sort_order:
 * @tree_column: a #GtkTreeViewColumn
 * @order: sort order that the sort indicator should indicate
 *
 * Changes the appearance of the sort indicator. 
 * 
 * This <emphasis>does not</emphasis> actually sort the model.  Use
 * gtk_tree_view_column_set_sort_column_id() if you want automatic sorting
 * support.  This function is primarily for custom sorting behavior, and should
 * be used in conjunction with gtk_tree_sortable_set_sort_column() to do
 * that. For custom models, the mechanism will vary. 
 * 
 * The sort indicator changes direction to indicate normal sort or reverse sort.
 * Note that you must have the sort indicator enabled to see anything when 
 * calling this function; see gtk_tree_view_column_set_sort_indicator().
 **/
void
gtk_tree_view_column_set_sort_order      (GtkTreeViewColumn     *tree_column,
                                          GtkSortType            order)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (order == tree_column->sort_order)
    return;

  tree_column->sort_order = order;
  gtk_tree_view_column_update_button (tree_column);
  g_object_notify (G_OBJECT (tree_column), "sort_order");
}

/**
 * gtk_tree_view_column_get_sort_order:
 * @tree_column: a #GtkTreeViewColumn
 * 
 * Gets the value set by gtk_tree_view_column_set_sort_order().
 * 
 * Return value: the sort order the sort indicator is indicating
 **/
GtkSortType
gtk_tree_view_column_get_sort_order      (GtkTreeViewColumn     *tree_column)
{
  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), 0);

  return tree_column->sort_order;
}

/**
 * gtk_tree_view_column_cell_set_cell_data:
 * @tree_column: A #GtkTreeViewColumn.
 * @tree_model: The #GtkTreeModel to to get the cell renderers attributes from.
 * @iter: The #GtkTreeIter to to get the cell renderer's attributes from.
 * @is_expander: TRUE, if the row has children
 * @is_expanded: TRUE, if the row has visible children
 * 
 * Sets the cell renderer based on the @tree_model and @tree_node.  That is, for
 * every attribute mapping in @tree_column, it will get a value from the set
 * column on the @tree_node, and use that value to set the attribute on the cell
 * renderer.  This is used primarily by the GtkTreeView.
 **/
void
gtk_tree_view_column_cell_set_cell_data (GtkTreeViewColumn *tree_column,
					 GtkTreeModel      *tree_model,
					 GtkTreeIter       *iter,
					 gboolean           is_expander,
					 gboolean           is_expanded)
{
  GSList *list;
  GValue value = { 0, };
  GList *cell_list;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (tree_column->cell_list != NULL);

  if (tree_model == NULL)
    return;

  for (cell_list = tree_column->cell_list; cell_list; cell_list = cell_list->next)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *) cell_list->data;
      GObject *cell = (GObject *) info->cell;

      list = info->attributes;

      g_object_freeze_notify (cell);
      g_object_set (cell, "is_expander", is_expander, "is_expanded", is_expanded, NULL);

      while (list && list->next)
	{
	  gtk_tree_model_get_value (tree_model, iter,
				    GPOINTER_TO_INT (list->next->data),
				    &value);
	  g_object_set_property (cell, (gchar *) list->data, &value);
	  g_value_unset (&value);
	  list = list->next->next;
	}

      if (info->func)
	(* info->func) (tree_column, info->cell, tree_model, iter, info->func_data);
      g_object_thaw_notify (G_OBJECT (info->cell));
    }

}

/**
 * gtk_tree_view_column_cell_get_size:
 * @tree_column: A #GtkTreeViewColumn.
 * @cell_area: The area a the column will be allocated, or %NULL
 * @x_offset: location to return x offset of cell relative to @cell_area, or %NULL
 * @y_offset: location to return y offset of cell relative to @cell_area, or %NULL
 * @width: location to return width needed to render a cell, or %NULL
 * @height: location to return height needed to render a cell, or %NULL
 * 
 * Obtains the width and height needed to render the column.  This is used
 * primarily by the GtkTreeView.
 **/
void
gtk_tree_view_column_cell_get_size (GtkTreeViewColumn *tree_column,
				    GdkRectangle      *cell_area,
				    gint              *x_offset,
				    gint              *y_offset,
				    gint              *width,
				    gint              *height)
{
  GList *list;
  gboolean first_cell = TRUE;

  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));

  if (height)
    * height = 0;
  if (width)
    * width = 0;

  for (list = tree_column->cell_list; list; list = list->next)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *) list->data;
      gboolean visible;
      gint new_height = 0;
      gint new_width = 0;
      g_object_get (info->cell, "visible", &visible, NULL);

      if (visible == FALSE)
	continue;

      if (first_cell == FALSE && *width)
	*width += tree_column->spacing;

      gtk_cell_renderer_get_size (info->cell,
				  tree_column->tree_view,
				  cell_area,
				  x_offset,
				  y_offset,
				  &new_width,
				  &new_height);

      if (height)
	* height = MAX (*height, new_height);
      info->requested_width = MAX (info->requested_width, new_width);
      if (width)
	* width += info->requested_width;
      first_cell = TRUE;
    }
}

/* both rendering and rendering focus are somewhat complicated, and a bit of
 * code.  Rather than duplicate them, we put them together to keep the code in
 * one place
 */
static void
gtk_tree_view_column_cell_render_or_focus (GtkTreeViewColumn *tree_column,
					   GdkWindow         *window,
					   GdkRectangle      *background_area,
					   GdkRectangle      *cell_area,
					   GdkRectangle      *expose_area,
					   guint              flags,
					   gboolean           render,
					   GdkRectangle      *focus_rectangle)
{
  GList *list;
  GdkRectangle real_cell_area;
  gint expand_cell_count = 0;
  gint full_requested_width = 0;
  gint extra_space;
  gint min_x, min_y, max_x, max_y;

  min_x = G_MAXINT;
  min_y = G_MAXINT;
  max_x = 0;
  max_y = 0;

  real_cell_area = *cell_area;

  /* Find out how my extra space we have to allocate */
  for (list = tree_column->cell_list; list; list = list->next)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *) list->data;
      gboolean visible;

      g_object_get (info->cell, "visible", &visible, NULL);
      if (visible == FALSE)
	continue;

      if (info->expand == TRUE)
	expand_cell_count ++;
      full_requested_width += info->requested_width;
    }

  extra_space = cell_area->width - full_requested_width;
  if (extra_space < 0)
    extra_space = 0;

  for (list = tree_column->cell_list; list; list = list->next)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *) list->data;
      gboolean visible;

      if (info->pack == GTK_PACK_END)
	continue;

      g_object_get (info->cell, "visible", &visible, NULL);
      if (visible == FALSE)
	continue;

      real_cell_area.width = info->requested_width +
	(info->expand?extra_space:0);
      if (render)
	{
	  gtk_cell_renderer_render (info->cell,
				    window,
				    tree_column->tree_view,
				    background_area,
				    &real_cell_area,
				    expose_area,
				    flags);
	}
      else
	{
	  gint x_offset, y_offset, width, height;

	  gtk_cell_renderer_get_size (info->cell,
				      tree_column->tree_view,
				      &real_cell_area,
				      &x_offset, &y_offset,
				      &width, &height);

	  if (min_x > (real_cell_area.x + x_offset))
	    min_x = real_cell_area.x + x_offset;
	  if (max_x < real_cell_area.x + x_offset + width)
	    max_x = real_cell_area.x + x_offset + width;
	  if (min_y > (real_cell_area.y + y_offset))
	    min_y = real_cell_area.y + y_offset;
	  if (max_y < real_cell_area.y + y_offset + height)
	    max_y = real_cell_area.y + y_offset + height;
	}
      real_cell_area.x += (info->requested_width + tree_column->spacing);
    }
  for (list = g_list_last (tree_column->cell_list); list; list = list->prev)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *) list->data;
      gboolean visible;

      if (info->pack == GTK_PACK_START)
	continue;

      g_object_get (info->cell, "visible", &visible, NULL);
      if (visible == FALSE)
	continue;

      real_cell_area.width = info->requested_width +
	(info->expand?extra_space:0);
      gtk_cell_renderer_render (info->cell,
				window,
				tree_column->tree_view,
				background_area,
				&real_cell_area,
				expose_area,
				flags);
      real_cell_area.x += (info->requested_width + tree_column->spacing);
    }
  if (! render)
    {
      if (min_x >= max_x || min_y >= max_y)
	{
	  *focus_rectangle = *cell_area;
	  focus_rectangle->x -= 1;
	  focus_rectangle->y -= 1;
	  focus_rectangle->width += 2;
	  focus_rectangle->height += 2;
	}
      else
	{
	  focus_rectangle->x = min_x - 1;
	  focus_rectangle->y = min_y - 1;
	  focus_rectangle->width = (max_x - min_x) + 2;
	  focus_rectangle->height = (max_y - min_y) + 2;
	}
    }
}

/**
 * gtk_tree_view_column_cell_render:
 * @tree_column: A #GtkTreeViewColumn.
 * @window: a #GdkDrawable to draw to
 * @background_area: entire cell area (including tree expanders and maybe padding on the sides)
 * @cell_area: area normally rendered by a cell renderer
 * @expose_area: area that actually needs updating
 * @flags: flags that affect rendering
 * 
 * Renders the cell contained by #tree_column. This is used primarily by the
 * GtkTreeView.
 **/
void
gtk_tree_view_column_cell_render (GtkTreeViewColumn *tree_column,
				  GdkWindow         *window,
				  GdkRectangle      *background_area,
				  GdkRectangle      *cell_area,
				  GdkRectangle      *expose_area,
				  guint              flags)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  g_return_if_fail (background_area != NULL);
  g_return_if_fail (cell_area != NULL);
  g_return_if_fail (expose_area != NULL);

  gtk_tree_view_column_cell_render_or_focus (tree_column,
					     window,
					     background_area,
					     cell_area,
					     expose_area,
					     flags,
					     TRUE,
					     NULL);
}

gboolean
_gtk_tree_view_column_cell_event (GtkTreeViewColumn  *tree_column,
				  GtkCellEditable   **editable_widget,
				  GdkEvent           *event,
				  gchar              *path_string,
				  GdkRectangle       *background_area,
				  GdkRectangle       *cell_area,
				  guint               flags)
{
  gboolean visible, mode;

  g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column), FALSE);

  g_object_get (G_OBJECT (((GtkTreeViewColumnCellInfo *) tree_column->cell_list->data)->cell),
		"visible", &visible,
		"mode", &mode,
		NULL);
  if (visible && mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE)
    {
      if (gtk_cell_renderer_activate (((GtkTreeViewColumnCellInfo *) tree_column->cell_list->data)->cell,
				      event,
				      tree_column->tree_view,
				      path_string,
				      background_area,
				      cell_area,
				      flags))
	return TRUE;
    }
  else if (visible && mode == GTK_CELL_RENDERER_MODE_EDITABLE)
    {
      *editable_widget = gtk_cell_renderer_start_editing (((GtkTreeViewColumnCellInfo *) tree_column->cell_list->data)->cell,
							  event,
							  tree_column->tree_view,
							  path_string,
							  background_area,
							  cell_area,
							  flags);

      if (*editable_widget != NULL)
	{
	  g_return_val_if_fail (GTK_IS_CELL_EDITABLE (*editable_widget), FALSE);

	  return TRUE;
	}
    }
  return FALSE;
}


gboolean
gtk_tree_view_column_cell_focus (GtkTreeViewColumn *tree_column,
				 gint               direction)
{
  if (GTK_TREE_VIEW (tree_column->tree_view)->priv->focus_column == tree_column)
    return FALSE;
  return TRUE;
}

void
gtk_tree_view_column_cell_draw_focus (GtkTreeViewColumn       *tree_column,
				      GdkWindow               *window,
				      GdkRectangle            *background_area,
				      GdkRectangle            *cell_area,
				      GdkRectangle            *expose_area,
				      guint                    flags)
{
  g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
  if (tree_column->editable_widget)
    {
      /* This function is only called on the editable row when editing.
       */

      gtk_paint_focus (tree_column->tree_view->style,
		       window,
		       NULL,
		       tree_column->tree_view,
		       "treeview",
		       cell_area->x - 1,
		       cell_area->y - 1,
		       cell_area->width + 2 - 1,
		       cell_area->height + 2 - 1);
      
    }
  else
    {
      GdkRectangle focus_rectangle;
      gtk_tree_view_column_cell_render_or_focus (tree_column,
						 window,
						 background_area,
						 cell_area,
						 expose_area,
						 flags,
						 FALSE,
						 &focus_rectangle);
      
      gtk_paint_focus (tree_column->tree_view->style,
		       window,
		       NULL,
		       tree_column->tree_view,
		       "treeview",
		       focus_rectangle.x,
		       focus_rectangle.y,
		       focus_rectangle.width - 1,
		       focus_rectangle.height - 1);
    }
}

gboolean
gtk_tree_view_column_cell_is_visible (GtkTreeViewColumn *tree_column)
{
  GList *list;

  for (list = tree_column->cell_list; list; list = list->next)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *) list->data;
      gboolean visible;

      g_object_get (G_OBJECT (info->cell), "visible", &visible, NULL);

      if (visible)
	return TRUE;
    }

  return FALSE;
}

void
gtk_tree_view_column_cell_set_dirty (GtkTreeViewColumn *tree_column)
{
  GList *list;

  for (list = tree_column->cell_list; list; list = list->next)
    {
      GtkTreeViewColumnCellInfo *info = (GtkTreeViewColumnCellInfo *) list->data;

      info->requested_width = 0;
    }
  tree_column->dirty = TRUE;

  if (tree_column->tree_view)
    gtk_widget_queue_resize (tree_column->tree_view);
}

void
_gtk_tree_view_column_start_editing (GtkTreeViewColumn *tree_column,
				     GtkCellEditable   *cell_editable)
{
  g_return_if_fail (tree_column->editable_widget == NULL);

  tree_column->editable_widget = cell_editable;
}

void
_gtk_tree_view_column_stop_editing (GtkTreeViewColumn *tree_column)
{
  g_return_if_fail (tree_column->editable_widget != NULL);

  tree_column->editable_widget = NULL;
}
