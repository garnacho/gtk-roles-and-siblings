#ifndef __GDK_IMAGE_H__
#define __GDK_IMAGE_H__

#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Types of images.
 *   Normal: Normal X image type. These are slow as they involve passing
 *	     the entire image through the X connection each time a draw
 *	     request is required. On Win32, a bitmap.
 *   Shared: Shared memory X image type. These are fast as the X server
 *	     and the program actually use the same piece of memory. They
 *	     should be used with care though as there is the possibility
 *	     for both the X server and the program to be reading/writing
 *	     the image simultaneously and producing undesired results.
 *	     On Win32, also a bitmap.
 *   Shared Pixmap: Also a shared memory image, which also has a
 *	     pixmap using the same memory. Used by gdk_imlib with the
 *	     Win32 backend.
 */
typedef enum
{
  GDK_IMAGE_NORMAL,
  GDK_IMAGE_SHARED,
  GDK_IMAGE_FASTEST,
  GDK_IMAGE_SHARED_PIXMAP
} GdkImageType;

typedef struct _GdkImageClass GdkImageClass;

#define GDK_TYPE_IMAGE              (gdk_image_get_type ())
#define GDK_IMAGE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_IMAGE, GdkImage))
#define GDK_IMAGE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_IMAGE, GdkImageClass))
#define GDK_IS_IMAGE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_IMAGE))
#define GDK_IS_IMAGE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_IMAGE))
#define GDK_IMAGE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_IMAGE, GdkImageClass))

struct _GdkImage
{
  GObject parent_instance;
  
  GdkImageType	type;
  GdkVisual    *visual;	    /* visual used to create the image */
  GdkByteOrder	byte_order;
  gint		width;
  gint		height;
  guint16	depth;
  guint16	bpp;	    /* bytes per pixel */
  guint16	bpl;	    /* bytes per line */
  gpointer	mem;

  gpointer windowing_data;
};

struct _GdkImageClass
{
  GObjectClass parent_class;
};

GType     gdk_image_get_type   (void);

GdkImage* gdk_image_new_bitmap (GdkVisual     *visual,
				gpointer      data,
				gint          width,
				gint          height);
GdkImage*  gdk_image_new       (GdkImageType  type,
				GdkVisual    *visual,
				gint	      width,
				gint	      height);
#ifdef GDK_WINDOWING_WIN32
GdkImage*  gdk_image_bitmap_new(GdkImageType  type,
				GdkVisual    *visual,
				gint	      width,
				gint	      height);

#endif
GdkImage*  gdk_image_get       (GdkDrawable  *drawable,
				gint	      x,
				gint	      y,
				gint	      width,
				gint	      height);

GdkImage * gdk_image_ref       (GdkImage     *image);
void       gdk_image_unref     (GdkImage     *image);

void	   gdk_image_put_pixel (GdkImage     *image,
				gint	      x,
				gint	      y,
				guint32	      pixel);
guint32	   gdk_image_get_pixel (GdkImage     *image,
				gint	      x,
				gint	      y);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_IMAGE_H__ */
