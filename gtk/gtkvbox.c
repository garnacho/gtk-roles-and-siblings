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
#include "gtkvbox.h"


static void gtk_vbox_class_init    (GtkVBoxClass   *klass);
static void gtk_vbox_init          (GtkVBox        *box);
static void gtk_vbox_size_request  (GtkWidget      *widget,
				    GtkRequisition *requisition);
static void gtk_vbox_size_allocate (GtkWidget      *widget,
				    GtkAllocation  *allocation);


guint
gtk_vbox_get_type ()
{
  static guint vbox_type = 0;

  if (!vbox_type)
    {
      GtkTypeInfo vbox_info =
      {
	"GtkVBox",
	sizeof (GtkVBox),
	sizeof (GtkVBoxClass),
	(GtkClassInitFunc) gtk_vbox_class_init,
	(GtkObjectInitFunc) gtk_vbox_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      vbox_type = gtk_type_unique (gtk_box_get_type (), &vbox_info);
    }

  return vbox_type;
}

static void
gtk_vbox_class_init (GtkVBoxClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->size_request = gtk_vbox_size_request;
  widget_class->size_allocate = gtk_vbox_size_allocate;
}

static void
gtk_vbox_init (GtkVBox *vbox)
{
}

GtkWidget*
gtk_vbox_new (gint homogeneous,
	      gint spacing)
{
  GtkVBox *vbox;

  vbox = gtk_type_new (gtk_vbox_get_type ());

  GTK_BOX (vbox)->spacing = spacing;
  GTK_BOX (vbox)->homogeneous = homogeneous ? TRUE : FALSE;

  return GTK_WIDGET (vbox);
}


static void
gtk_vbox_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;
  gint nvis_children;
  gint height;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VBOX (widget));
  g_return_if_fail (requisition != NULL);

  box = GTK_BOX (widget);
  requisition->width = 0;
  requisition->height = 0;
  nvis_children = 0;

  children = box->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  gtk_widget_size_request (child->widget, &child->widget->requisition);

	  if (box->homogeneous)
	    {
	      height = child->widget->requisition.height + child->padding * 2;
	      requisition->height = MAX (requisition->height, height);
	    }
	  else
	    {
	      requisition->height += child->widget->requisition.height + child->padding * 2;
	    }

	  requisition->width = MAX (requisition->width, child->widget->requisition.width);

	  nvis_children += 1;
	}
    }

  if (nvis_children > 0)
    {
      if (box->homogeneous)
	requisition->height *= nvis_children;
      requisition->height += (nvis_children - 1) * box->spacing;
    }

  requisition->width += GTK_CONTAINER (box)->border_width * 2;
  requisition->height += GTK_CONTAINER (box)->border_width * 2;
}

static void
gtk_vbox_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;
  GtkAllocation child_allocation;
  gint nvis_children;
  gint nexpand_children;
  gint child_height;
  gint height;
  gint extra;
  gint y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VBOX (widget));
  g_return_if_fail (allocation != NULL);

  box = GTK_BOX (widget);
  widget->allocation = *allocation;

  nvis_children = 0;
  nexpand_children = 0;
  children = box->children;

  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  nvis_children += 1;
	  if (child->expand)
	    nexpand_children += 1;
	}
    }

  if (nvis_children > 0)
    {
      if (box->homogeneous)
	{
	  height = (allocation->height -
		   GTK_CONTAINER (box)->border_width * 2 -
		   (nvis_children - 1) * box->spacing);
	  extra = height / nvis_children;
	}
      else if (nexpand_children > 0)
	{
	  height = (gint)allocation->height - (gint)widget->requisition.height;
	  extra = height / nexpand_children;
	}
      else
	{
	  height = 0;
	  extra = 0;
	}

      y = allocation->y + GTK_CONTAINER (box)->border_width;
      child_allocation.x = allocation->x + GTK_CONTAINER (box)->border_width;
      child_allocation.width = MAX (0, allocation->width - GTK_CONTAINER (box)->border_width * 2);

      children = box->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if ((child->pack == GTK_PACK_START) && GTK_WIDGET_VISIBLE (child->widget))
	    {
	      if (box->homogeneous)
		{
		  if (nvis_children == 1)
		    child_height = height;
		  else
		    child_height = extra;

		  nvis_children -= 1;
		  height -= extra;
		}
	      else
		{
		  child_height = child->widget->requisition.height + child->padding * 2;

		  if (child->expand)
		    {
		      if (nexpand_children == 1)
			child_height += height;
		      else
			child_height += extra;

		      nexpand_children -= 1;
		      height -= extra;
		    }
		}

	      if (child->fill)
		{
		  child_allocation.height = MAX (0, child_height - child->padding * 2);
		  child_allocation.y = y + child->padding;
		}
	      else
		{
		  child_allocation.height = child->widget->requisition.height;
		  child_allocation.y = y + (child_height - child_allocation.height) / 2;
		}

	      gtk_widget_size_allocate (child->widget, &child_allocation);

	      y += child_height + box->spacing;
	    }
	}

      y = allocation->y + allocation->height - GTK_CONTAINER (box)->border_width;

      children = box->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if ((child->pack == GTK_PACK_END) && GTK_WIDGET_VISIBLE (child->widget))
	    {
              if (box->homogeneous)
                {
                  if (nvis_children == 1)
                    child_height = height;
                  else
                    child_height = extra;

                  nvis_children -= 1;
                  height -= extra;
                }
              else
                {
		  child_height = child->widget->requisition.height + child->padding * 2;

                  if (child->expand)
                    {
                      if (nexpand_children == 1)
                        child_height += height;
                      else
                        child_height += extra;

                      nexpand_children -= 1;
                      height -= extra;
                    }
                }

              if (child->fill)
                {
                  child_allocation.height = MAX (0, child_height - child->padding * 2);
                  child_allocation.y = y + child->padding - child_height;
                }
              else
                {
                  child_allocation.height = child->widget->requisition.height;
                  child_allocation.y = y + (child_height - child_allocation.height) / 2 - child_height;
                }

              gtk_widget_size_allocate (child->widget, &child_allocation);

              y -= (child_height + box->spacing);
	    }
	}
    }
}
