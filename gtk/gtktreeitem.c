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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "gtklabel.h"
#include "gtktree.h"
#include "gtktreeitem.h"
#include "gtkeventbox.h"
#include "gtkpixmap.h"
#include "gtkmain.h"
#include "gtksignal.h"

/* remove comment if you want replace loading pixmap by static bitmap 
   for icons 
   experimental code and it is buggy */
/* #define WITH_BITMAP */

#define DEFAULT_DELTA 9

#ifdef WITH_BITMAP
#include "tree_plus.xbm"
#include "tree_minus.xbm"
#endif

enum {
  COLLAPSE_TREE,
  EXPAND_TREE,
  LAST_SIGNAL
};

typedef void (*GtkTreeItemSignal) (GtkObject *object,
				   gpointer   arg1,
				   gpointer   data);

static void gtk_tree_item_class_init (GtkTreeItemClass *klass);
static void gtk_tree_item_init       (GtkTreeItem      *tree_item);
static void gtk_tree_item_realize       (GtkWidget        *widget);
static void gtk_tree_item_size_request  (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void gtk_tree_item_size_allocate (GtkWidget        *widget,
					 GtkAllocation    *allocation);
static void gtk_tree_item_draw          (GtkWidget        *widget,
					 GdkRectangle     *area);
static void gtk_tree_item_draw_focus    (GtkWidget        *widget);
static gint gtk_tree_item_button_press  (GtkWidget        *widget,
					 GdkEventButton   *event);
static gint gtk_tree_item_expose        (GtkWidget        *widget,
					 GdkEventExpose   *event);
static gint gtk_tree_item_focus_in      (GtkWidget        *widget,
					 GdkEventFocus    *event);
static gint gtk_tree_item_focus_out     (GtkWidget        *widget,
					 GdkEventFocus    *event);
static void gtk_real_tree_item_select   (GtkItem          *item);
static void gtk_real_tree_item_deselect (GtkItem          *item);
static void gtk_real_tree_item_toggle   (GtkItem          *item);
static void gtk_real_tree_item_expand   (GtkTreeItem      *item);
static void gtk_real_tree_item_collapse (GtkTreeItem      *item);
static void gtk_real_tree_item_expand   (GtkTreeItem      *item);
static void gtk_real_tree_item_collapse (GtkTreeItem      *item);
static void gtk_tree_item_marshal_signal (GtkObject      *object,
					  GtkSignalFunc   func,
					  gpointer        func_data,
					  GtkArg         *args);
static void gtk_tree_item_destroy        (GtkObject *object);
static void gtk_tree_item_subtree_button_click (GtkWidget *widget);
static void gtk_tree_item_subtree_button_changed_state (GtkWidget *widget);

static void gtk_tree_item_map(GtkWidget*);
static void gtk_tree_item_unmap(GtkWidget*);

static GtkItemClass *parent_class = NULL;
static GtkContainerClass *container_class = NULL;
static gint tree_item_signals[LAST_SIGNAL] = { 0 };

guint
gtk_tree_item_get_type ()
{
  static guint tree_item_type = 0;

  if (!tree_item_type)
    {
      GtkTypeInfo tree_item_info =
      {
	"GtkTreeItem",
	sizeof (GtkTreeItem),
	sizeof (GtkTreeItemClass),
	(GtkClassInitFunc) gtk_tree_item_class_init,
	(GtkObjectInitFunc) gtk_tree_item_init,
	(GtkArgFunc) NULL,
      };

      tree_item_type = gtk_type_unique (gtk_item_get_type (), &tree_item_info);
    }

  return tree_item_type;
}

static void
gtk_tree_item_class_init (GtkTreeItemClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkItemClass *item_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  item_class = (GtkItemClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_item_get_type ());
  
  tree_item_signals[EXPAND_TREE] =
    gtk_signal_new ("expand",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkTreeItemClass, expand),
		    gtk_tree_item_marshal_signal,
		    GTK_TYPE_NONE, 0);
  tree_item_signals[COLLAPSE_TREE] =
    gtk_signal_new ("collapse",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkTreeItemClass, collapse),
		    gtk_tree_item_marshal_signal,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, tree_item_signals, LAST_SIGNAL);

  object_class->destroy = gtk_tree_item_destroy;

  widget_class->realize = gtk_tree_item_realize;
  widget_class->size_request = gtk_tree_item_size_request;
  widget_class->size_allocate = gtk_tree_item_size_allocate;
  widget_class->draw = gtk_tree_item_draw;
  widget_class->draw_focus = gtk_tree_item_draw_focus;
  widget_class->button_press_event = gtk_tree_item_button_press;
  widget_class->expose_event = gtk_tree_item_expose;
  widget_class->focus_in_event = gtk_tree_item_focus_in;
  widget_class->focus_out_event = gtk_tree_item_focus_out;
  widget_class->map = gtk_tree_item_map;
  widget_class->unmap = gtk_tree_item_unmap;

  item_class->select = gtk_real_tree_item_select;
  item_class->deselect = gtk_real_tree_item_deselect;
  item_class->toggle = gtk_real_tree_item_toggle;

  class->expand = gtk_real_tree_item_expand;
  class->collapse = gtk_real_tree_item_collapse;

  container_class = (GtkContainerClass*) parent_class;
}

/* callback for event box mouse event */
static void 
gtk_tree_item_subtree_button_click (GtkWidget *widget)
{
  GtkTreeItem* item;

  item = (GtkTreeItem*) gtk_object_get_user_data(GTK_OBJECT(widget));

  if(item->expanded)
    gtk_tree_item_collapse(item);
  else
    gtk_tree_item_expand(item);
}

/* callback for event box state changed */
static void
gtk_tree_item_subtree_button_changed_state(GtkWidget *w)
{
  if(GTK_WIDGET_VISIBLE (w)) {

    if (w->state == GTK_STATE_NORMAL)
      gdk_window_set_background (w->window, &w->style->white);
    else
      gdk_window_set_background (w->window, &w->style->bg[w->state]);

    if (GTK_WIDGET_DRAWABLE(w))
      gdk_window_clear_area (w->window, 0, 0, 
			     w->allocation.width, w->allocation.height);
  }
}

static void
gtk_tree_item_init (GtkTreeItem *tree_item)
{
  GtkWidget *eventbox, *pixmapwid;
  static GdkPixmap *pixmap_plus = NULL;
  static GdkPixmap *pixmap_minus = NULL;
#ifndef WITH_BITMAP
  static GdkBitmap *mask = NULL; 
#endif
  static GtkStyle *style = NULL;

  tree_item->expanded = FALSE;
  tree_item->subtree = NULL;
  GTK_WIDGET_SET_FLAGS (tree_item, GTK_CAN_FOCUS);

  if(style == NULL) 
    {

      style=gtk_widget_get_style(GTK_WIDGET(tree_item));

#ifndef WITH_BITMAP
      /* create pixmaps for one time, based on xpm file */
      pixmap_plus = gdk_pixmap_create_from_xpm (GTK_WIDGET(tree_item)->window, &mask, 
						&style->bg[GTK_STATE_NORMAL],
						"tree_plus.xpm");
      if(!pixmap_plus)
	g_warning("gtk_tree_item_init: can't find tree_plus.xpm file !\n");

      pixmap_minus = gdk_pixmap_create_from_xpm (GTK_WIDGET(tree_item)->window, &mask, 
						 &style->bg[GTK_STATE_NORMAL],
						 "tree_minus.xpm");      
      if(!pixmap_minus)
	g_warning("gtk_tree_item_init: can't find tree_minus.xpm file !\n");
#else

      pixmap_plus = gdk_pixmap_create_from_data(GTK_WIDGET(tree_item)->window,
						(gchar*) tree_plus_bits,
						tree_plus_width, tree_plus_height, -1,
						&style->black,
						&style->white);

      pixmap_minus = gdk_pixmap_create_from_data(GTK_WIDGET(tree_item)->window,
						 (gchar*) tree_minus_bits,
						 tree_minus_width, tree_minus_height, -1,
						 &style->black,
						 &style->white);
#endif /* WITH_BITMAP */
    }
  
  if(pixmap_plus && pixmap_minus) 
    {
      /* create an event box containing one pixmaps */
      eventbox = gtk_event_box_new();
      gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK);
      gtk_signal_connect(GTK_OBJECT(eventbox), "state_changed",
			 (GtkSignalFunc)gtk_tree_item_subtree_button_changed_state, 
			 (gpointer)NULL);
      gtk_signal_connect(GTK_OBJECT(eventbox), "realize",
			 (GtkSignalFunc)gtk_tree_item_subtree_button_changed_state, 
			 (gpointer)NULL);
      gtk_signal_connect(GTK_OBJECT(eventbox), "button_press_event",
			 (GtkSignalFunc)gtk_tree_item_subtree_button_click,
			 (gpointer)NULL);
      gtk_object_set_user_data(GTK_OBJECT(eventbox), tree_item);
      tree_item->pixmaps_box = eventbox;

      /* create pixmap for button '+' */
#ifndef WITH_BITMAP
      pixmapwid = gtk_pixmap_new (pixmap_plus, mask);
#else
      pixmapwid = gtk_pixmap_new (pixmap_plus, NULL);
#endif
      if(!tree_item->expanded) 
	gtk_container_add(GTK_CONTAINER(eventbox), pixmapwid);
      gtk_widget_show(pixmapwid);
      tree_item->plus_pix_widget = pixmapwid;

      /* create pixmap for button '-' */
#ifndef WITH_BITMAP
      pixmapwid = gtk_pixmap_new (pixmap_minus, mask);
#else
      pixmapwid = gtk_pixmap_new (pixmap_minus, NULL);
#endif
      if(tree_item->expanded) 
	gtk_container_add(GTK_CONTAINER(eventbox), pixmapwid);
      gtk_widget_show(pixmapwid);
      tree_item->minus_pix_widget = pixmapwid;

      gtk_widget_set_parent(eventbox, GTK_WIDGET(tree_item));
    } else
      tree_item->pixmaps_box = NULL;
}


GtkWidget*
gtk_tree_item_new ()
{
  GtkWidget *tree_item;

  tree_item = GTK_WIDGET (gtk_type_new (gtk_tree_item_get_type ()));

  return tree_item;
}

GtkWidget*
gtk_tree_item_new_with_label (gchar *label)
{
  GtkWidget *tree_item;
  GtkWidget *label_widget;


  tree_item = gtk_tree_item_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (tree_item), label_widget);
  gtk_widget_show (label_widget);


  return tree_item;
}

void
gtk_tree_item_set_subtree (GtkTreeItem *tree_item,
			   GtkWidget   *subtree)
{
  g_return_if_fail (tree_item != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));

  if(tree_item->subtree) {
    g_warning("there is already a subtree for this tree item\n");
    return;
  }

  tree_item->subtree = subtree; 
  GTK_TREE(subtree)->tree_owner = GTK_WIDGET(tree_item);

  /* set root tree for selection list */
  GTK_TREE(subtree)->root_tree = GTK_TREE(GTK_WIDGET(tree_item)->parent)->root_tree;

  /* show subtree button */
  if(tree_item->pixmaps_box)
    gtk_widget_show(tree_item->pixmaps_box);

  /* set parent widget */
  gtk_widget_set_parent(subtree, GTK_WIDGET(tree_item)->parent);

  if(GTK_WIDGET_VISIBLE(GTK_WIDGET(tree_item))) 
    {
      if(GTK_WIDGET_REALIZED (GTK_WIDGET(tree_item)) &&
	 !GTK_WIDGET_REALIZED (GTK_WIDGET(subtree)))
	gtk_widget_realize (GTK_WIDGET(subtree));

      if(GTK_WIDGET_MAPPED (GTK_WIDGET(tree_item)) &&
	 !GTK_WIDGET_MAPPED (GTK_WIDGET(subtree)))
	gtk_widget_map (GTK_WIDGET(subtree));
    }

  if(tree_item->expanded)
    gtk_widget_show(subtree);
  else
    gtk_widget_hide(subtree);
  
  if (GTK_WIDGET_VISIBLE (tree_item) && GTK_WIDGET_VISIBLE (tree_item))
    gtk_widget_queue_resize (GTK_WIDGET(tree_item));

}

void
gtk_tree_item_select (GtkTreeItem *tree_item)
{

  gtk_item_select (GTK_ITEM (tree_item));

}

void
gtk_tree_item_deselect (GtkTreeItem *tree_item)
{

  gtk_item_deselect (GTK_ITEM (tree_item));

}

void
gtk_tree_item_expand (GtkTreeItem *tree_item)
{

  gtk_signal_emit (GTK_OBJECT (tree_item), tree_item_signals[EXPAND_TREE], NULL);

}

void
gtk_tree_item_collapse (GtkTreeItem *tree_item)
{

  gtk_signal_emit (GTK_OBJECT (tree_item), tree_item_signals[COLLAPSE_TREE], NULL);

}

static void
gtk_tree_item_realize (GtkWidget *widget)
{    
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (widget));

  if (GTK_WIDGET_CLASS (parent_class)->realize)
    (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
  
  gdk_window_set_background (widget->window, &widget->style->white);
}

static void
gtk_tree_item_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkBin *bin;
  GtkTreeItem* item;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);
  item = GTK_TREE_ITEM(widget);

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->klass->xthickness) * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &bin->child->requisition);

      requisition->width += bin->child->requisition.width;

      gtk_widget_size_request (item->pixmaps_box, 
			       &item->pixmaps_box->requisition);
      requisition->width += item->pixmaps_box->requisition.width + DEFAULT_DELTA + 
	GTK_TREE(widget->parent)->current_indent;

      requisition->height += MAX(bin->child->requisition.height,
				 item->pixmaps_box->requisition.height);
    }
}

static void
gtk_tree_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkTreeItem* item;
  GtkAllocation child_allocation;
  guint border_width;
  int temp;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  bin = GTK_BIN (widget);
  item = GTK_TREE_ITEM(widget);

  if (bin->child)
    {
      border_width = (GTK_CONTAINER (widget)->border_width +
		      widget->style->klass->xthickness);

      child_allocation.x = border_width + GTK_TREE(widget->parent)->current_indent;
      child_allocation.y = GTK_CONTAINER (widget)->border_width;

#if 0
      child_allocation.height = allocation->height - child_allocation.y * 2;
      child_allocation.width = item->pixmaps_box->requisition.width;

      child_allocation.y += 1;
      child_allocation.height -= 2;
      gtk_widget_size_allocate (item->pixmaps_box, &child_allocation);

      child_allocation.height += 2;
#else
      child_allocation.width = item->pixmaps_box->requisition.width;
      child_allocation.height = item->pixmaps_box->requisition.height;
      
      temp = allocation->height - child_allocation.height;
      child_allocation.y += ( temp / 2 ) + ( temp % 2 );

      gtk_widget_size_allocate (item->pixmaps_box, &child_allocation);

      child_allocation.y = GTK_CONTAINER (widget)->border_width;
      child_allocation.height = allocation->height - child_allocation.y * 2;
#endif
      child_allocation.x += item->pixmaps_box->requisition.width+DEFAULT_DELTA;

      child_allocation.width = 
	allocation->width - (child_allocation.x + border_width);

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }

}

static void 
gtk_tree_item_draw_lines(GtkWidget *widget) 
{
  GtkTreeItem* item;
  GtkTree* tree;
  guint lx1, ly1, lx2, ly2;

  item = GTK_TREE_ITEM(widget);
  tree = GTK_TREE(widget->parent);

  /* draw vertical line */
  lx1 = item->pixmaps_box->allocation.width;
  lx1 = lx2 = ( lx1 / 2 ) + ( lx1 % 2 ) + 
    GTK_CONTAINER(widget)->border_width + 1 + tree->current_indent;
  ly1 = 0;
  ly2 = widget->allocation.height;

  if(g_list_last(tree->children)->data == (gpointer)widget)
    ly2 = (ly2 / 2) + (ly2 % 2);

  if(tree != tree->root_tree)
    gdk_draw_line(widget->window, widget->style->black_gc,
		  lx1, ly1, lx2, ly2);

  /* draw vertical line for subtree connecting */
  if(g_list_last(tree->children)->data != (gpointer)widget)
    ly2 = (ly2 / 2) + (ly2 % 2);
  
  lx2 += DEFAULT_DELTA;

  if(item->subtree && item->expanded)
    gdk_draw_line(widget->window, widget->style->black_gc,
		  lx2, ly2, lx2, widget->allocation.height);

  /* draw horizontal line */
  ly1 = ly2;
  lx2 += 2;

  gdk_draw_line(widget->window, widget->style->black_gc,
		lx1, ly1, lx2, ly2);

  lx2 -= DEFAULT_DELTA+2;
  ly1 = 0;
  ly2 = widget->allocation.height;

  if(tree != tree->root_tree)
    {
      item = GTK_TREE_ITEM(tree->tree_owner);
      tree = GTK_TREE(GTK_WIDGET(tree)->parent);
      while(tree != tree->root_tree) {
	lx1 = lx2 -= tree->indent_value;
      
	if(g_list_last(tree->children)->data != (gpointer)item)
	  gdk_draw_line(widget->window, widget->style->black_gc,
			lx1, ly1, lx2, ly2);
	item = GTK_TREE_ITEM(tree->tree_owner);
	tree = GTK_TREE(GTK_WIDGET(tree)->parent);
      } 
    }
}

static void
gtk_tree_item_draw (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle child_area, item_area;
  GtkTreeItem* tree_item;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);
      tree_item = GTK_TREE_ITEM(widget);

      /* draw left size of tree item */
      item_area.x = 0; item_area.y = 0;
      item_area.width = tree_item->pixmaps_box->allocation.width+DEFAULT_DELTA +
	(GTK_TREE(widget->parent)->current_indent + 2);
      item_area.height = widget->allocation.height;

      if(gdk_rectangle_intersect(&item_area, area, &child_area)) {
	
	if (!GTK_WIDGET_IS_SENSITIVE (widget)) 
	  gtk_style_set_background (widget->style, widget->window, 
				    GTK_STATE_INSENSITIVE);
	else if(GTK_TREE(widget->parent)->view_mode == GTK_TREE_VIEW_LINE &&
		widget->state == GTK_STATE_SELECTED)
	  gtk_style_set_background (widget->style, widget->window, widget->state);
	else
	  gdk_window_set_background (widget->window, &widget->style->white);

	gdk_window_clear_area (widget->window, 
			       child_area.x, child_area.y,
			       child_area.width, child_area.height);

/* 	gtk_tree_item_draw_lines(widget); */

	if (tree_item->pixmaps_box && 
	    GTK_WIDGET_VISIBLE(tree_item->pixmaps_box) &&
	    gtk_widget_intersect (tree_item->pixmaps_box, area, &child_area))
	  gtk_widget_draw (tree_item->pixmaps_box, &child_area);
      }

      /* draw right side */
      if(gtk_widget_intersect (bin->child, area, &child_area)) {

	if (!GTK_WIDGET_IS_SENSITIVE (widget)) 
	  gtk_style_set_background (widget->style, widget->window, 
				    GTK_STATE_INSENSITIVE);
	else if (widget->state == GTK_STATE_NORMAL)
	  gdk_window_set_background(widget->window, &widget->style->white);
	else
	  gtk_style_set_background (widget->style, widget->window, widget->state);

	gdk_window_clear_area (widget->window, child_area.x, child_area.y,
			       child_area.width+1, child_area.height);

	if (bin->child && 
	    GTK_WIDGET_VISIBLE(bin->child) &&
	    gtk_widget_intersect (bin->child, area, &child_area))
	  gtk_widget_draw (bin->child, &child_area);
      }

      gtk_widget_draw_focus (widget);
    }
}

static void
gtk_tree_item_draw_focus (GtkWidget *widget)
{
  GdkGC *gc;
  int dx;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
	gc = widget->style->black_gc;
      else if (!GTK_WIDGET_IS_SENSITIVE (widget))
	gc = widget->style->bg_gc[GTK_STATE_INSENSITIVE];
      else if (widget->state == GTK_STATE_NORMAL)
	gc = widget->style->white_gc;
      else
	gc = widget->style->bg_gc[widget->state];

      dx = 0;

      if(GTK_TREE(widget->parent)->view_mode == GTK_TREE_VIEW_ITEM) 
	dx = GTK_TREE_ITEM(widget)->pixmaps_box->allocation.width + DEFAULT_DELTA +
	  GTK_TREE(widget->parent)->current_indent+1;

      gdk_draw_rectangle (widget->window, gc, FALSE, dx, 0,
			  widget->allocation.width - 1 - dx,
			  widget->allocation.height - 1);

      if(GTK_TREE(widget->parent)->view_line) 
	gtk_tree_item_draw_lines(widget);
    }
}

static gint
gtk_tree_item_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type == GDK_BUTTON_PRESS)
    if (!GTK_WIDGET_HAS_FOCUS (widget))
      gtk_widget_grab_focus (widget);

  return FALSE;
}

static gint
gtk_tree_item_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    gtk_tree_item_draw(widget, &event->area);

  return FALSE;
}

static gint
gtk_tree_item_focus_in (GtkWidget     *widget,
			GdkEventFocus *event)
{

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);


  return FALSE;
}

static gint
gtk_tree_item_focus_out (GtkWidget     *widget,
			 GdkEventFocus *event)
{

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);


  return FALSE;
}

static void
gtk_real_tree_item_select (GtkItem *item)
{
    
  g_return_if_fail (item != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (item));

  if (GTK_WIDGET (item)->state == GTK_STATE_SELECTED)
    return;

  if(GTK_TREE(GTK_WIDGET(item)->parent)->view_mode == GTK_TREE_VIEW_LINE)
    gtk_widget_set_state (GTK_TREE_ITEM (item)->pixmaps_box, GTK_STATE_SELECTED);

  gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_SELECTED);

  gtk_widget_queue_draw (GTK_WIDGET (item));
}

static void
gtk_real_tree_item_deselect (GtkItem *item)
{

  g_return_if_fail (item != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (item));

  if (GTK_WIDGET (item)->state == GTK_STATE_NORMAL)
    return;

  if(GTK_WIDGET_MAPPED(GTK_WIDGET (item))) 
    {
      gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_NORMAL);
      
      if(GTK_TREE(GTK_WIDGET(item)->parent)->view_mode == GTK_TREE_VIEW_LINE)
	gtk_widget_set_state (GTK_TREE_ITEM (item)->pixmaps_box, GTK_STATE_NORMAL);

      gtk_widget_queue_draw (GTK_WIDGET (item));
    }
}

static void
gtk_real_tree_item_toggle (GtkItem *item)
{

  g_return_if_fail (item != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (item));

  if (GTK_WIDGET (item)->parent && GTK_IS_TREE (GTK_WIDGET (item)->parent))
    gtk_tree_select_child (GTK_TREE (GTK_WIDGET (item)->parent),
			   GTK_WIDGET (item));
  else
    {
      /* Should we really bother with this bit? A listitem not in a list?
       * -Johannes Keukelaar
       * yes, always be on the save side!
       * -timj
       */
      if (GTK_WIDGET (item)->state == GTK_STATE_SELECTED)
	gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_NORMAL);
      else
	gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_SELECTED);
      gtk_widget_queue_draw (GTK_WIDGET (item));
    }
}

static void
gtk_tree_item_marshal_signal (GtkObject      *object,
			      GtkSignalFunc   func,
			      gpointer        func_data,
			      GtkArg         *args)
{
  GtkTreeItemSignal rfunc;

  rfunc = (GtkTreeItemSignal) func;

  (* rfunc) (object, GTK_VALUE_OBJECT (args[0]), func_data);
}

static void
gtk_real_tree_item_expand (GtkTreeItem *tree_item)
{
  GtkTree* tree;
  
  g_return_if_fail (tree_item != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  g_return_if_fail (tree_item->subtree != NULL);


  if(!tree_item->expanded) 
    {
      tree = GTK_TREE(GTK_WIDGET(tree_item)->parent); 

      /* hide subtree widget */
      gtk_widget_show(tree_item->subtree);

      /* hide button '+' and show button '-' */
      if(tree_item->pixmaps_box) {
	gtk_container_remove(GTK_CONTAINER(tree_item->pixmaps_box), 
			     tree_item->plus_pix_widget);
	gtk_container_add(GTK_CONTAINER(tree_item->pixmaps_box), 
			  tree_item->minus_pix_widget);
      }
      if(tree->root_tree) gtk_widget_queue_resize(GTK_WIDGET(tree->root_tree));
      tree_item->expanded = TRUE;
    }
}

static void
gtk_real_tree_item_collapse (GtkTreeItem *tree_item)
{
  GtkTree* tree;

  g_return_if_fail (tree_item != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (tree_item));
  g_return_if_fail (tree_item->subtree != NULL);

  if(tree_item->expanded) 
    {
      tree = GTK_TREE(GTK_WIDGET(tree_item)->parent);

      /* hide subtree widget */
      gtk_widget_hide(tree_item->subtree);

      /* hide button '-' and show button '+' */
      if(tree_item->pixmaps_box) {
	gtk_container_remove(GTK_CONTAINER(tree_item->pixmaps_box), 
			     tree_item->minus_pix_widget);
	gtk_container_add(GTK_CONTAINER(tree_item->pixmaps_box), 
			  tree_item->plus_pix_widget);
      }
      if(tree->root_tree) gtk_widget_queue_resize(GTK_WIDGET(tree->root_tree));
      tree_item->expanded = FALSE;
    }

}

static void
gtk_tree_item_destroy (GtkObject *object)
{
  GtkTreeItem* item;
  GtkWidget* child;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (object));

  item = GTK_TREE_ITEM(object);

  /* free sub tree if it exist */
  if((child = item->subtree)) 
    {
      child->parent = NULL; 
      gtk_object_unref (GTK_OBJECT (child));
      gtk_widget_destroy (child);
    }

  /* free pixmaps box */
  if((child = item->pixmaps_box))
    {
      child->parent = NULL; 
      gtk_object_unref (GTK_OBJECT (child));
      gtk_widget_destroy (child);
    }

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);

}

void
gtk_tree_item_remove_subtree (GtkTreeItem* item) 
{
  g_return_if_fail(item != NULL);
  g_return_if_fail(GTK_IS_TREE_ITEM(item));
  g_return_if_fail(item->subtree);

  if(GTK_TREE(item->subtree)->children)
    gtk_tree_remove_items(GTK_TREE(item->subtree), 
			  GTK_TREE(item->subtree)->children);

  if (GTK_WIDGET_MAPPED (item->subtree))
    gtk_widget_unmap (item->subtree);

  gtk_widget_unparent (item->subtree);

  if(item->pixmaps_box)
    gtk_widget_hide(item->pixmaps_box);

  item->subtree = NULL;
  item->expanded = FALSE;
  if(item->pixmaps_box) {
    gtk_container_remove(GTK_CONTAINER(item->pixmaps_box), 
			 item->minus_pix_widget);
    gtk_container_add(GTK_CONTAINER(item->pixmaps_box), 
		      item->plus_pix_widget);
  }
}

static void
gtk_tree_item_map (GtkWidget *widget)
{
  GtkBin *bin;
  GtkTreeItem* item;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  bin = GTK_BIN (widget);
  item = GTK_TREE_ITEM(widget);

  if (!GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_show (widget->window);
  else
    gtk_widget_queue_draw (widget);

  if(item->pixmaps_box &&
     GTK_WIDGET_VISIBLE (item->pixmaps_box) &&
     !GTK_WIDGET_MAPPED (item->pixmaps_box))
    gtk_widget_map (item->pixmaps_box);

  if (bin->child &&
      GTK_WIDGET_VISIBLE (bin->child) &&
      !GTK_WIDGET_MAPPED (bin->child))
    gtk_widget_map (bin->child);
}

static void
gtk_tree_item_unmap (GtkWidget *widget)
{
  GtkBin *bin;
  GtkTreeItem* item;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TREE_ITEM (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  bin = GTK_BIN (widget);
  item = GTK_TREE_ITEM(widget);

  if (GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);
  else
    gdk_window_hide (widget->window);

  if(item->pixmaps_box &&
     GTK_WIDGET_VISIBLE (item->pixmaps_box) &&
     GTK_WIDGET_MAPPED (item->pixmaps_box))
    gtk_widget_unmap (bin->child);

  if (bin->child &&
      GTK_WIDGET_VISIBLE (bin->child) &&
      GTK_WIDGET_MAPPED (bin->child))
    gtk_widget_unmap (bin->child);
}
