/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 Red Hat, Inc.
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

#include "gtkimagemenuitem.h"
#include "gtkaccellabel.h"
#include "gtksignal.h"
#include "gtkintl.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkimage.h"

static void gtk_image_menu_item_class_init           (GtkImageMenuItemClass *klass);
static void gtk_image_menu_item_init                 (GtkImageMenuItem      *image_menu_item);
static void gtk_image_menu_item_size_request         (GtkWidget        *widget,
                                                      GtkRequisition   *requisition);
static void gtk_image_menu_item_size_allocate        (GtkWidget        *widget,
                                                      GtkAllocation    *allocation);
static void gtk_image_menu_item_remove               (GtkContainer          *container,
                                                      GtkWidget             *child);
static void gtk_image_menu_item_toggle_size_request  (GtkMenuItem           *menu_item,
						      gint                  *requisition);

static void gtk_image_menu_item_map        (GtkWidget      *widget);
static void gtk_image_menu_item_unmap      (GtkWidget      *widget);
static void gtk_image_menu_item_forall     (GtkContainer   *container,
                                            gboolean	    include_internals,
                                            GtkCallback     callback,
                                            gpointer        callback_data);

static void gtk_image_menu_item_set_property (GObject         *object,
                                              guint            prop_id,
                                              const GValue    *value,
                                              GParamSpec      *pspec);
static void gtk_image_menu_item_get_property (GObject         *object,
                                              guint            prop_id,
                                              GValue          *value,
                                              GParamSpec      *pspec);


enum {
  PROP_ZERO,
  PROP_IMAGE
};

static GtkMenuItemClass *parent_class = NULL;

GtkType
gtk_image_menu_item_get_type (void)
{
  static GtkType image_menu_item_type = 0;

  if (!image_menu_item_type)
    {
      static const GtkTypeInfo image_menu_item_info =
      {
        "GtkImageMenuItem",
        sizeof (GtkImageMenuItem),
        sizeof (GtkImageMenuItemClass),
        (GtkClassInitFunc) gtk_image_menu_item_class_init,
        (GtkObjectInitFunc) gtk_image_menu_item_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      image_menu_item_type = gtk_type_unique (GTK_TYPE_MENU_ITEM, &image_menu_item_info);
    }

  return image_menu_item_type;
}

static void
gtk_image_menu_item_class_init (GtkImageMenuItemClass *klass)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;
  GtkContainerClass *container_class;

  gobject_class = (GObjectClass*) klass;
  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  menu_item_class = (GtkMenuItemClass*) klass;
  container_class = (GtkContainerClass*) klass;
  
  parent_class = gtk_type_class (GTK_TYPE_MENU_ITEM);
  
  widget_class->size_request = gtk_image_menu_item_size_request;
  widget_class->size_allocate = gtk_image_menu_item_size_allocate;
  widget_class->map = gtk_image_menu_item_map;
  widget_class->unmap = gtk_image_menu_item_unmap;

  container_class->forall = gtk_image_menu_item_forall;
  container_class->remove = gtk_image_menu_item_remove;
  
  menu_item_class->toggle_size_request = gtk_image_menu_item_toggle_size_request;

  gobject_class->set_property = gtk_image_menu_item_set_property;
  gobject_class->get_property = gtk_image_menu_item_get_property;
  
  g_object_class_install_property (gobject_class,
                                   PROP_IMAGE,
                                   g_param_spec_object ("image",
                                                        _("Image widget"),
                                                        _("Child widget to appear next to the menu text"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gtk_image_menu_item_init (GtkImageMenuItem *image_menu_item)
{
  image_menu_item->image = NULL;
}

static void
gtk_image_menu_item_set_property (GObject         *object,
                                  guint            prop_id,
                                  const GValue    *value,
                                  GParamSpec      *pspec)
{
  GtkImageMenuItem *image_menu_item = GTK_IMAGE_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_IMAGE:
      {
        GtkWidget *image;

        image = (GtkWidget*) g_value_get_object (value);

	gtk_image_menu_item_set_image (image_menu_item, image);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}
static void
gtk_image_menu_item_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
  GtkImageMenuItem *image_menu_item = GTK_IMAGE_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_IMAGE:
      g_value_set_object (value,
                          (GObject*) image_menu_item->image);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_image_menu_item_toggle_size_request (GtkMenuItem *menu_item,
					 gint        *requisition)
{
  GtkImageMenuItem *image_menu_item;
  
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (menu_item));

  image_menu_item = GTK_IMAGE_MENU_ITEM (menu_item);

  if (image_menu_item->image)
    *requisition = image_menu_item->image->requisition.width;
  else
    *requisition = 0;
}


static void
gtk_image_menu_item_size_request (GtkWidget      *widget,
                                  GtkRequisition *requisition)
{
  GtkImageMenuItem *image_menu_item;
  gint child_height = 0;
  
  image_menu_item = GTK_IMAGE_MENU_ITEM (widget);
  
  if (image_menu_item->image && GTK_WIDGET_VISIBLE (image_menu_item->image))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (image_menu_item->image,
                               &child_requisition);

      child_height = child_requisition.height;
    }
  
  (* GTK_WIDGET_CLASS (parent_class)->size_request) (widget, requisition);

  /* not done with height since that happens via the
   * toggle_size_request
   */
  requisition->height = MAX (requisition->height, child_height);
  
  /* Note that GtkMenuShell always size requests before
   * toggle_size_request, so toggle_size_request will be able to use
   * image_menu_item->image->requisition
   */
}

static void
gtk_image_menu_item_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
  GtkImageMenuItem *image_menu_item;
  
  image_menu_item = GTK_IMAGE_MENU_ITEM (widget);  
  
  (* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

  if (image_menu_item->image)
    {
      gint width, height, x, y;
      GtkAllocation child_allocation;
      
      /* Man this is lame hardcoding action, but I can't
       * come up with a solution that's really better.
       */
      
      width = image_menu_item->image->requisition.width;
      height = image_menu_item->image->requisition.height;

      x = (GTK_CONTAINER (image_menu_item)->border_width +
	   widget->style->xthickness) +
        (GTK_MENU_ITEM (image_menu_item)->toggle_size - width) / 2;
      y = (widget->allocation.height - height) / 2;

      child_allocation.width = width;
      child_allocation.height = height;
      child_allocation.x = MAX (x, 0);
      child_allocation.y = MAX (y, 0);

      gtk_widget_size_allocate (image_menu_item->image, &child_allocation);
    }
}

static void
gtk_image_menu_item_map (GtkWidget *widget)
{
  GtkImageMenuItem *image_menu_item;

  g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (widget));

  image_menu_item = GTK_IMAGE_MENU_ITEM (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->map) (widget);

  if (image_menu_item->image &&
      GTK_WIDGET_VISIBLE (image_menu_item->image) &&
      !GTK_WIDGET_MAPPED (image_menu_item->image))
    gtk_widget_map (image_menu_item->image);

  if (!GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_show (widget->window);  
}

static void
gtk_image_menu_item_unmap (GtkWidget *widget)
{
  GtkImageMenuItem *image_menu_item;

  g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (widget));

  image_menu_item = GTK_IMAGE_MENU_ITEM (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
  
  if (!GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_hide (widget->window);
  
  if (image_menu_item->image && GTK_WIDGET_MAPPED (image_menu_item->image))
    gtk_widget_unmap (image_menu_item->image);  
}

static void
gtk_image_menu_item_forall (GtkContainer   *container,
                            gboolean	    include_internals,
                            GtkCallback     callback,
                            gpointer        callback_data)
{
  GtkImageMenuItem *image_menu_item;

  g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (container));

  image_menu_item = GTK_IMAGE_MENU_ITEM (container);
  
  (* GTK_CONTAINER_CLASS (parent_class)->forall) (container,
                                                  include_internals,
                                                  callback,
                                                  callback_data);

  if (image_menu_item->image)
    (* callback) (image_menu_item->image, callback_data);
}

GtkWidget*
gtk_image_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_IMAGE_MENU_ITEM, NULL);
}

GtkWidget*
gtk_image_menu_item_new_with_label (const gchar *label)
{
  GtkImageMenuItem *image_menu_item;
  GtkWidget *accel_label;
  
  image_menu_item = GTK_IMAGE_MENU_ITEM (g_object_new (GTK_TYPE_IMAGE_MENU_ITEM,
                                                       NULL));

  accel_label = gtk_accel_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (image_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label),
                                    GTK_WIDGET (image_menu_item));
  gtk_widget_show (accel_label);

  return GTK_WIDGET(image_menu_item);
}

GtkWidget*
gtk_image_menu_item_new_from_stock (const gchar      *stock_id,
				    GtkAccelGroup    *accel_group)
{
  GtkWidget *image;
  GtkStockItem stock_item;
  GtkWidget *item;

  g_return_val_if_fail (stock_id != NULL, NULL);

  image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);

  if (gtk_stock_lookup (stock_id, &stock_item))
    {
      item = gtk_image_menu_item_new_with_label (stock_item.label);

      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      
      if (stock_item.keyval && accel_group)
	gtk_widget_add_accelerator (item,
				    "activate",
				    accel_group,
				    stock_item.keyval,
				    stock_item.modifier,
				    GTK_ACCEL_VISIBLE);
    }
  else
    {
      item = gtk_image_menu_item_new_with_label (stock_id);

      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    }

  gtk_widget_show (image);
  return item;
}

void
gtk_image_menu_item_set_image (GtkImageMenuItem *image_menu_item,
                               GtkWidget        *image)
{
  g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (image_menu_item));

  if (image == image_menu_item->image)
    return;

  if (image_menu_item->image)
    gtk_container_remove (GTK_CONTAINER (image_menu_item),
			  image_menu_item->image);

  image_menu_item->image = image;

  if (image == NULL)
    return;

  gtk_widget_set_parent (image, GTK_WIDGET (image_menu_item));

  g_object_notify (G_OBJECT (image_menu_item), "image");
  
  if (GTK_WIDGET_REALIZED (image->parent))
    gtk_widget_realize (image);
  
  if (GTK_WIDGET_VISIBLE (image->parent) && GTK_WIDGET_VISIBLE (image))
    {
      if (GTK_WIDGET_MAPPED (image->parent))
	gtk_widget_map (image);

      gtk_widget_queue_resize (image);
    }
}

GtkWidget*
gtk_image_menu_item_get_image (GtkImageMenuItem *image_menu_item)
{
  g_return_val_if_fail (GTK_IS_IMAGE_MENU_ITEM (image_menu_item), NULL);

  return image_menu_item->image;
}

static void
gtk_image_menu_item_remove (GtkContainer *container,
                            GtkWidget    *child)
{
  GtkImageMenuItem *image_menu_item;

  image_menu_item = GTK_IMAGE_MENU_ITEM (container);

  if (child == image_menu_item->image)
    {
      gboolean widget_was_visible;
      
      widget_was_visible = GTK_WIDGET_VISIBLE (child);
      
      gtk_widget_unparent (child);
      image_menu_item->image = NULL;
      
      if (GTK_WIDGET_VISIBLE (container) && widget_was_visible)
        gtk_widget_queue_resize (GTK_WIDGET (container));

      g_object_notify (G_OBJECT (image_menu_item), "image");
    }
  else
    {
      (* GTK_CONTAINER_CLASS (parent_class)->remove) (container, child);
    }
}


