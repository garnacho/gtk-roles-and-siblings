/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#ifndef __GTK_IMAGE_H__
#define __GTK_IMAGE_H__


#include <gdk/gdk.h>
#include <gtk/gtkmisc.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_IMAGE                  (gtk_image_get_type ())
#define GTK_IMAGE(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_IMAGE, GtkImage))
#define GTK_IMAGE_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IMAGE, GtkImageClass))
#define GTK_IS_IMAGE(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_IMAGE))
#define GTK_IS_IMAGE_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IMAGE))
#define GTK_IMAGE_GET_CLASS(obj)        (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_IMAGE, GtkImageClass))


typedef struct _GtkImage       GtkImage;
typedef struct _GtkImageClass  GtkImageClass;

typedef struct _GtkImagePixmapData  GtkImagePixmapData;
typedef struct _GtkImageImageData   GtkImageImageData;
typedef struct _GtkImagePixbufData  GtkImagePixbufData;
typedef struct _GtkImageStockData   GtkImageStockData;
typedef struct _GtkImageIconSetData GtkImageIconSetData;
typedef struct _GtkImageAnimationData GtkImageAnimationData;

struct _GtkImagePixmapData
{
  GdkPixmap *pixmap;
  GdkBitmap *mask;
};

struct _GtkImageImageData
{
  GdkImage *image;
  GdkBitmap *mask;
};

struct _GtkImagePixbufData
{
  GdkPixbuf *pixbuf;
};

struct _GtkImageStockData
{
  gchar *stock_id;
  GtkIconSize size;
};

struct _GtkImageIconSetData
{
  GtkIconSet *icon_set;
  GtkIconSize size;
};

struct _GtkImageAnimationData
{
  GdkPixbufAnimation *anim;
  GdkPixbufAnimationIter *iter;
  guint frame_timeout;
};

typedef enum
{
  GTK_IMAGE_EMPTY,
  GTK_IMAGE_PIXMAP,
  GTK_IMAGE_IMAGE,
  GTK_IMAGE_PIXBUF,
  GTK_IMAGE_STOCK,
  GTK_IMAGE_ICON_SET,
  GTK_IMAGE_ANIMATION
} GtkImageType;

struct _GtkImage
{
  GtkMisc misc;

  GtkImageType storage_type;
  
  union
  {
    GtkImagePixmapData pixmap;
    GtkImageImageData image;
    GtkImagePixbufData pixbuf;
    GtkImageStockData stock;
    GtkImageIconSetData icon_set;
    GtkImageAnimationData anim;
  } data;
};

struct _GtkImageClass
{
  GtkMiscClass parent_class;
};

GtkType    gtk_image_get_type (void) G_GNUC_CONST;

GtkWidget* gtk_image_new                (void);
GtkWidget* gtk_image_new_from_pixmap    (GdkPixmap       *pixmap,
                                         GdkBitmap       *mask);
GtkWidget* gtk_image_new_from_image     (GdkImage        *image,
                                         GdkBitmap       *mask);
GtkWidget* gtk_image_new_from_file      (const gchar     *filename);
GtkWidget* gtk_image_new_from_pixbuf    (GdkPixbuf       *pixbuf);
GtkWidget* gtk_image_new_from_stock     (const gchar     *stock_id,
                                         GtkIconSize      size);
GtkWidget* gtk_image_new_from_icon_set  (GtkIconSet      *icon_set,
                                         GtkIconSize      size);
GtkWidget* gtk_image_new_from_animation (GdkPixbufAnimation *animation);

void gtk_image_set_from_pixmap    (GtkImage        *image,
                                   GdkPixmap       *pixmap,
                                   GdkBitmap       *mask);
void gtk_image_set_from_image     (GtkImage        *image,
                                   GdkImage        *gdk_image,
                                   GdkBitmap       *mask);
void gtk_image_set_from_file      (GtkImage        *image,
                                   const gchar     *filename);
void gtk_image_set_from_pixbuf    (GtkImage        *image,
                                   GdkPixbuf       *pixbuf);
void gtk_image_set_from_stock     (GtkImage        *image,
                                   const gchar     *stock_id,
                                   GtkIconSize      size);
void gtk_image_set_from_icon_set  (GtkImage        *image,
                                   GtkIconSet      *icon_set,
                                   GtkIconSize      size);
void gtk_image_set_from_animation (GtkImage           *image,
                                   GdkPixbufAnimation *animation);

GtkImageType gtk_image_get_storage_type (GtkImage   *image);

void       gtk_image_get_pixmap   (GtkImage         *image,
                                   GdkPixmap       **pixmap,
                                   GdkBitmap       **mask);
void       gtk_image_get_image    (GtkImage         *image,
                                   GdkImage        **gdk_image,
                                   GdkBitmap       **mask);
GdkPixbuf* gtk_image_get_pixbuf   (GtkImage         *image);
void       gtk_image_get_stock    (GtkImage         *image,
                                   gchar           **stock_id,
                                   GtkIconSize      *size);
void       gtk_image_get_icon_set (GtkImage         *image,
                                   GtkIconSet      **icon_set,
                                   GtkIconSize      *size);
GdkPixbufAnimation* gtk_image_get_animation (GtkImage *image);


#ifndef GTK_DISABLE_DEPRECATED
/* These three are deprecated */

void       gtk_image_set      (GtkImage   *image,
			       GdkImage   *val,
			       GdkBitmap  *mask);
void       gtk_image_get      (GtkImage   *image,
			       GdkImage  **val,
			       GdkBitmap **mask);
#endif /* GTK_DISABLE_DEPRECATED */

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_IMAGE_H__ */
