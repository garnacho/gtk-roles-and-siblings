/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - Progressive loader object
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
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

#include <string.h>

#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-animation.h"
#include "gdk-pixbuf-io.h"
#include "gdk-pixbuf-loader.h"
#include "gdk-pixbuf-marshal.h"

enum {
        SIZE_PREPARED,
        AREA_PREPARED,
        AREA_UPDATED,
        CLOSED,
        LAST_SIGNAL
};


static void gdk_pixbuf_loader_class_init    (GdkPixbufLoaderClass   *klass);
static void gdk_pixbuf_loader_init          (GdkPixbufLoader        *loader);
static void gdk_pixbuf_loader_finalize      (GObject                *loader);

static gpointer parent_class = NULL;
static guint    pixbuf_loader_signals[LAST_SIGNAL] = { 0 };


/* Internal data */

#define LOADER_HEADER_SIZE 128

typedef struct
{
        GdkPixbufAnimation *animation;
        gboolean closed;
        guchar header_buf[LOADER_HEADER_SIZE];
        gint header_buf_offset;
        GdkPixbufModule *image_module;
        gpointer context;
        gint width;
        gint height;
        gboolean size_fixed;
        gboolean needs_scale;
} GdkPixbufLoaderPrivate;


/**
 * gdk_pixbuf_loader_get_type:
 * @void:
 *
 * Registers the #GdkPixbufLoader class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GdkPixbufLoader class.
 **/
GType
gdk_pixbuf_loader_get_type (void)
{
        static GType loader_type = 0;
  
        if (!loader_type)
                {
                        static const GTypeInfo loader_info = {
                                sizeof (GdkPixbufLoaderClass),
                                (GBaseInitFunc) NULL,
                                (GBaseFinalizeFunc) NULL,
                                (GClassInitFunc) gdk_pixbuf_loader_class_init,
                                NULL,           /* class_finalize */
                                NULL,           /* class_data */
                                sizeof (GdkPixbufLoader),
                                0,              /* n_preallocs */
                                (GInstanceInitFunc) gdk_pixbuf_loader_init
                        };
      
                        loader_type = g_type_register_static (G_TYPE_OBJECT,
                                                              "GdkPixbufLoader",
                                                              &loader_info,
                                                              0);
                }
  
        return loader_type;
}

static void
gdk_pixbuf_loader_class_init (GdkPixbufLoaderClass *class)
{
        GObjectClass *object_class;
  
        object_class = (GObjectClass *) class;
  
        parent_class = g_type_class_peek_parent (class);
  
        object_class->finalize = gdk_pixbuf_loader_finalize;

        pixbuf_loader_signals[SIZE_PREPARED] =
                g_signal_new ("size_prepared",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdkPixbufLoaderClass, size_prepared),
                              NULL, NULL,
                              gdk_pixbuf_marshal_VOID__INT_INT,
                              G_TYPE_NONE, 2, 
                              G_TYPE_INT,
                              G_TYPE_INT);
  
        pixbuf_loader_signals[AREA_PREPARED] =
                g_signal_new ("area_prepared",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdkPixbufLoaderClass, area_prepared),
                              NULL, NULL,
                              gdk_pixbuf_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
  
        pixbuf_loader_signals[AREA_UPDATED] =
                g_signal_new ("area_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdkPixbufLoaderClass, area_updated),
                              NULL, NULL,
                              gdk_pixbuf_marshal_VOID__INT_INT_INT_INT,
                              G_TYPE_NONE, 4,
                              G_TYPE_INT,
                              G_TYPE_INT,
                              G_TYPE_INT,
                              G_TYPE_INT);
  
        pixbuf_loader_signals[CLOSED] =
                g_signal_new ("closed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdkPixbufLoaderClass, closed),
                              NULL, NULL,
                              gdk_pixbuf_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

static void
gdk_pixbuf_loader_init (GdkPixbufLoader *loader)
{
        GdkPixbufLoaderPrivate *priv;
  
        priv = g_new0 (GdkPixbufLoaderPrivate, 1);
        loader->priv = priv;
}

static void
gdk_pixbuf_loader_finalize (GObject *object)
{
        GdkPixbufLoader *loader;
        GdkPixbufLoaderPrivate *priv = NULL;
  
        loader = GDK_PIXBUF_LOADER (object);
        priv = loader->priv;

        if (!priv->closed)
                g_warning ("GdkPixbufLoader finalized without calling gdk_pixbuf_loader_close() - this is not allowed. You must explicitly end the data stream to the loader before dropping the last reference.");
  
        if (priv->animation)
                g_object_unref (priv->animation);
  
        g_free (priv);
  
        G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gdk_pixbuf_loader_set_size:
 * @loader: A pixbuf loader.
 * @width: The desired width of the image being loaded.
 * @height: The desired height of the image being loaded.
 *
 * Causes the image to be scaled while it is loaded. The desired
 * image size can be determined relative to the original size of
 * the image by calling gdk_pixbuf_loader_set_size() from a
 * signal handler for the ::size_prepared signal.
 *
 * Attempts to set the desired image size  are ignored after the 
 * emission of the ::size_prepared signal.
 *
 * Since: 2.2
 */
void 
gdk_pixbuf_loader_set_size (GdkPixbufLoader *loader,
			    gint             width,
			    gint             height)
{
        GdkPixbufLoaderPrivate *priv = GDK_PIXBUF_LOADER (loader)->priv;
        g_return_if_fail (width > 0 && height > 0);

        if (!priv->size_fixed) 
                {
                        priv->width = width;
                        priv->height = height;
                }
}

static void
gdk_pixbuf_loader_size_func (gint *width, gint *height, gpointer loader)
{
        GdkPixbufLoaderPrivate *priv = GDK_PIXBUF_LOADER (loader)->priv;

        /* allow calling gdk_pixbuf_loader_set_size() before the signal */
        if (priv->width == 0 && priv->height == 0) 
                {
                        priv->width = *width;
                        priv->height = *height;
                }

        g_signal_emit (loader, pixbuf_loader_signals[SIZE_PREPARED], 0, *width, *height);
        priv->size_fixed = TRUE;

        *width = priv->width;
        *height = priv->height;
}

static void
gdk_pixbuf_loader_prepare (GdkPixbuf          *pixbuf,
                           GdkPixbufAnimation *anim,
			   gpointer            loader)
{
        GdkPixbufLoaderPrivate *priv = GDK_PIXBUF_LOADER (loader)->priv;
        g_return_if_fail (pixbuf != NULL);

        if (!priv->size_fixed) 
                {
                        /* Defend against lazy loaders which don't call size_func */
                        gint width = gdk_pixbuf_get_width (pixbuf);
                        gint height = gdk_pixbuf_get_height (pixbuf);
                        
                        gdk_pixbuf_loader_size_func (&width, &height, loader);
                }

        priv->needs_scale = FALSE;
        if (priv->width > 0 && priv->height > 0 &&
            (priv->width != gdk_pixbuf_get_width (pixbuf) ||
             priv->height != gdk_pixbuf_get_height (pixbuf)))
                priv->needs_scale = TRUE;

        if (anim)
                g_object_ref (anim);
        else
                anim = gdk_pixbuf_non_anim_new (pixbuf);
  
        priv->animation = anim;
  
        if (!priv->needs_scale)
                g_signal_emit (loader, pixbuf_loader_signals[AREA_PREPARED], 0);
}

static void
gdk_pixbuf_loader_update (GdkPixbuf *pixbuf,
			  gint       x,
			  gint       y,
			  gint       width,
			  gint       height,
			  gpointer   loader)
{
        GdkPixbufLoaderPrivate *priv = GDK_PIXBUF_LOADER (loader)->priv;
  
        if (!priv->needs_scale)
                g_signal_emit (loader,
                               pixbuf_loader_signals[AREA_UPDATED],
                               0,
                               x, y,
                               /* sanity check in here.  Defend against an errant loader */
                               MIN (width, gdk_pixbuf_animation_get_width (priv->animation)),
                               MIN (height, gdk_pixbuf_animation_get_height (priv->animation)));
}

static gint
gdk_pixbuf_loader_load_module (GdkPixbufLoader *loader,
                               const char      *image_type,
                               GError         **error)
{
        GdkPixbufLoaderPrivate *priv = loader->priv;

        if (image_type)
                {
                        priv->image_module = _gdk_pixbuf_get_named_module (image_type,
                                                                           error);
                }
        else
                {
                        priv->image_module = _gdk_pixbuf_get_module (priv->header_buf,
                                                                     priv->header_buf_offset,
                                                                     NULL,
                                                                     error);
                }
  
        if (priv->image_module == NULL)
                return 0;
  
        if (priv->image_module->module == NULL)
                if (!_gdk_pixbuf_load_module (priv->image_module, error))
                        return 0;
  
        if (priv->image_module->module == NULL)
                return 0;
  
        if ((priv->image_module->begin_load == NULL) ||
            (priv->image_module->stop_load == NULL) ||
            (priv->image_module->load_increment == NULL))
                {
                        g_set_error (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_UNSUPPORTED_OPERATION,
                                     _("Incremental loading of image type '%s' is not supported"),
                                     priv->image_module->module_name);

                        return 0;
                }

        priv->context = priv->image_module->begin_load (gdk_pixbuf_loader_size_func,
                                                        gdk_pixbuf_loader_prepare,
                                                        gdk_pixbuf_loader_update,
                                                        loader,
                                                        error);
  
        if (priv->context == NULL)
                {
                        /* Defense against broken loaders; DO NOT take this as a GError
                         * example
                         */
                        if (error && *error == NULL)
                                {
                                        g_warning ("Bug! loader '%s' didn't set an error on failure",
                                                   priv->image_module->module_name);
                                        g_set_error (error,
                                                     GDK_PIXBUF_ERROR,
                                                     GDK_PIXBUF_ERROR_FAILED,
                                                     _("Internal error: Image loader module '%s'"
                                                       " failed to begin loading an image, but didn't"
                                                       " give a reason for the failure"),
                                                     priv->image_module->module_name);

                                }
      
                        return 0;
                }
  
        if (priv->header_buf_offset
            && priv->image_module->load_increment (priv->context, priv->header_buf, priv->header_buf_offset, error))
                return priv->header_buf_offset;
  
        return 0;
}

static int
gdk_pixbuf_loader_eat_header_write (GdkPixbufLoader *loader,
				    const guchar    *buf,
				    gsize            count,
                                    GError         **error)
{
        gint n_bytes;
        GdkPixbufLoaderPrivate *priv = loader->priv;
  
        n_bytes = MIN(LOADER_HEADER_SIZE - priv->header_buf_offset, count);
        memcpy (priv->header_buf + priv->header_buf_offset, buf, n_bytes);
  
        priv->header_buf_offset += n_bytes;
  
        if (priv->header_buf_offset >= LOADER_HEADER_SIZE)
                {
                        if (gdk_pixbuf_loader_load_module (loader, NULL, error) == 0)
                                return 0;
                }
  
        return n_bytes;
}

/**
 * gdk_pixbuf_loader_write:
 * @loader: A pixbuf loader.
 * @buf: Pointer to image data.
 * @count: Length of the @buf buffer in bytes.
 * @error: return location for errors
 *
 * This will cause a pixbuf loader to parse the next @count bytes of
 * an image.  It will return %TRUE if the data was loaded successfully,
 * and %FALSE if an error occurred.  In the latter case, the loader
 * will be closed, and will not accept further writes. If %FALSE is
 * returned, @error will be set to an error from the #GDK_PIXBUF_ERROR
 * or #G_FILE_ERROR domains.
 *
 * Return value: %TRUE if the write was successful, or %FALSE if the loader
 * cannot parse the buffer.
 **/
gboolean
gdk_pixbuf_loader_write (GdkPixbufLoader *loader,
			 const guchar    *buf,
			 gsize            count,
                         GError         **error)
{
        GdkPixbufLoaderPrivate *priv;
  
        g_return_val_if_fail (loader != NULL, FALSE);
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), FALSE);
  
        g_return_val_if_fail (buf != NULL, FALSE);
        g_return_val_if_fail (count >= 0, FALSE);
  
        priv = loader->priv;

        /* we expect it's not to be closed */
        g_return_val_if_fail (priv->closed == FALSE, FALSE);
  
        if (priv->image_module == NULL)
                {
                        gint eaten;
      
                        eaten = gdk_pixbuf_loader_eat_header_write (loader, buf, count, error);
                        if (eaten <= 0)
                                return FALSE;
      
                        count -= eaten;
                        buf += eaten;
                }
  
        if (count > 0 && priv->image_module->load_increment)
                {
                        gboolean retval;
                        retval = priv->image_module->load_increment (priv->context, buf, count,
                                                                     error);
                        if (!retval && error && *error == NULL)
                                {
                                        /* Fix up busted image loader */
                                        g_warning ("Bug! loader '%s' didn't set an error on failure",
                                                   priv->image_module->module_name);
                                        g_set_error (error,
                                                     GDK_PIXBUF_ERROR,
                                                     GDK_PIXBUF_ERROR_FAILED,
                                                     _("Internal error: Image loader module '%s'"
                                                       " failed to begin loading an image, but didn't"
                                                       " give a reason for the failure"),
                                                     priv->image_module->module_name);
                                }

                        return retval;
                }
      
        return TRUE;
}

/**
 * gdk_pixbuf_loader_new:
 *
 * Creates a new pixbuf loader object.
 *
 * Return value: A newly-created pixbuf loader.
 **/
GdkPixbufLoader *
gdk_pixbuf_loader_new (void)
{
        return g_object_new (GDK_TYPE_PIXBUF_LOADER, NULL);
}

/**
 * gdk_pixbuf_loader_new_with_type:
 * @image_type: name of the image format to be loaded with the image
 * @error: return location for an allocated #GError, or %NULL to ignore errors
 *
 * Creates a new pixbuf loader object that always attempts to parse
 * image data as if it were an image of type @image_type, instead of
 * identifying the type automatically. Useful if you want an error if
 * the image isn't the expected type, for loading image formats
 * that can't be reliably identified by looking at the data, or if
 * the user manually forces a specific type.
 *
 * Return value: A newly-created pixbuf loader.
 **/
GdkPixbufLoader *
gdk_pixbuf_loader_new_with_type (const char *image_type,
                                 GError    **error)
{
        GdkPixbufLoader *retval;
        GError *tmp;
  
        retval = g_object_new (GDK_TYPE_PIXBUF_LOADER, NULL);

        tmp = NULL;
        gdk_pixbuf_loader_load_module(retval, image_type, &tmp);
        if (tmp != NULL)
                {
                        g_propagate_error (error, tmp);
                        g_object_unref (retval);
                        return NULL;
                }

        return retval;
}

/**
 * gdk_pixbuf_loader_get_pixbuf:
 * @loader: A pixbuf loader.
 *
 * Queries the #GdkPixbuf that a pixbuf loader is currently creating.
 * In general it only makes sense to call this function after the
 * "area_prepared" signal has been emitted by the loader; this means
 * that enough data has been read to know the size of the image that
 * will be allocated.  If the loader has not received enough data via
 * gdk_pixbuf_loader_write(), then this function returns %NULL.  The
 * returned pixbuf will be the same in all future calls to the loader,
 * so simply calling g_object_ref() should be sufficient to continue
 * using it.  Additionally, if the loader is an animation, it will
 * return the "static image" of the animation
 * (see gdk_pixbuf_animation_get_static_image()).
 * 
 * Return value: The #GdkPixbuf that the loader is creating, or %NULL if not
 * enough data has been read to determine how to create the image buffer.
 **/
GdkPixbuf *
gdk_pixbuf_loader_get_pixbuf (GdkPixbufLoader *loader)
{
        GdkPixbufLoaderPrivate *priv;
  
        g_return_val_if_fail (loader != NULL, NULL);
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), NULL);
  
        priv = loader->priv;

        if (priv->animation)
                return gdk_pixbuf_animation_get_static_image (priv->animation);
        else
                return NULL;
}

/**
 * gdk_pixbuf_loader_get_animation:
 * @loader: A pixbuf loader
 *
 * Queries the #GdkPixbufAnimation that a pixbuf loader is currently creating.
 * In general it only makes sense to call this function after the "area_prepared"
 * signal has been emitted by the loader. If the loader doesn't have enough
 * bytes yet (hasn't emitted the "area_prepared" signal) this function will 
 * return %NULL.
 *
 * Return value: The #GdkPixbufAnimation that the loader is loading, or %NULL if
 not enough data has been read to determine the information.
**/
GdkPixbufAnimation *
gdk_pixbuf_loader_get_animation (GdkPixbufLoader *loader)
{
        GdkPixbufLoaderPrivate *priv;
  
        g_return_val_if_fail (loader != NULL, NULL);
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), NULL);
  
        priv = loader->priv;
  
        return priv->animation;
}

/**
 * gdk_pixbuf_loader_close:
 * @loader: A pixbuf loader.
 * @error: return location for a #GError, or %NULL to ignore errors
 *
 * Informs a pixbuf loader that no further writes with
 * gdk_pixbuf_loader_write() will occur, so that it can free its
 * internal loading structures. Also, tries to parse any data that
 * hasn't yet been parsed; if the remaining data is partial or
 * corrupt, an error will be returned.  If %FALSE is returned, @error
 * will be set to an error from the #GDK_PIXBUF_ERROR or #G_FILE_ERROR
 * domains. If you're just cancelling a load rather than expecting it
 * to be finished, passing %NULL for @error to ignore it is
 * reasonable.
 *
 * Returns: %TRUE if all image data written so far was successfully
            passed out via the update_area signal
 **/
gboolean
gdk_pixbuf_loader_close (GdkPixbufLoader *loader,
                         GError         **error)
{
        GdkPixbufLoaderPrivate *priv;
        gboolean retval = TRUE;
  
        g_return_val_if_fail (loader != NULL, TRUE);
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), TRUE);
  
        priv = loader->priv;
  
        /* we expect it's not closed */
        g_return_val_if_fail (priv->closed == FALSE, TRUE);
  
        /* We have less the 128 bytes in the image.  Flush it, and keep going. */
        if (priv->image_module == NULL)
                {
                        GError *tmp = NULL;
                        gdk_pixbuf_loader_load_module (loader, NULL, &tmp);
                        if (tmp != NULL)
                                {
                                        g_propagate_error (error, tmp);
                                        retval = FALSE;
                                }
                }  

        if (priv->image_module && priv->image_module->stop_load && priv->context) 
                {
                        if (!priv->image_module->stop_load (priv->context, error))
                                retval = FALSE;
                }
  
        priv->closed = TRUE;

        if (priv->needs_scale) 
                {
                        GdkPixbuf *tmp, *pixbuf;
                        
                        tmp = gdk_pixbuf_animation_get_static_image (priv->animation);
                        g_object_ref (tmp);
                        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, tmp->has_alpha, 8, priv->width, priv->height);
                        g_object_unref (priv->animation);
                        priv->animation = gdk_pixbuf_non_anim_new (pixbuf);
                        g_object_unref (pixbuf);
                        g_signal_emit (loader, pixbuf_loader_signals[AREA_PREPARED], 0);
                        gdk_pixbuf_scale (tmp, pixbuf, 0, 0, priv->width, priv->height, 0, 0,
                                          (double) priv->width / tmp->width,
                                          (double) priv->height / tmp->height,
                                          GDK_INTERP_BILINEAR); 
                        g_object_unref (tmp);
                        
                        g_signal_emit (loader, pixbuf_loader_signals[AREA_UPDATED], 0, 
                                       0, 0, priv->width, priv->height);
                }

        
        g_signal_emit (loader, pixbuf_loader_signals[CLOSED], 0);

        return retval;
}

/**
 * gdk_pixbuf_loader_get_format:
 * @loader: A pixbuf loader.
 *
 * Obtains the available information about the format of the 
 * currently loading image file.
 *
 * Returns: A #GdkPixbufFormat or %NULL. The return value is owned 
 * by GdkPixbuf and should not be freed.
 * 
 * Since: 2.2
 */
GdkPixbufFormat *
gdk_pixbuf_loader_get_format (GdkPixbufLoader *loader)
{
        GdkPixbufLoaderPrivate *priv;
  
        g_return_val_if_fail (loader != NULL, NULL);
        g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), NULL);
  
        priv = loader->priv;

        if (priv->image_module)
                return _gdk_pixbuf_get_format (priv->image_module);
        else
                return NULL;
}




