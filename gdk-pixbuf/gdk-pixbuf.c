/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - Basic memory management
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
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

#include "config.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define GDK_PIXBUF_C_COMPILATION
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-private.h"
/* Include the marshallers */
#include <glib-object.h>
#include "gdk-pixbuf-marshal.c"
#include "gdk-pixbuf-alias.h"

static void gdk_pixbuf_finalize     (GObject        *object);
static void gdk_pixbuf_set_property (GObject        *object,
				     guint           prop_id,
				     const GValue   *value,
				     GParamSpec     *pspec);
static void gdk_pixbuf_get_property (GObject        *object,
				     guint           prop_id,
				     GValue         *value,
				     GParamSpec     *pspec);


enum 
{
  PROP_0,
  PROP_COLORSPACE,
  PROP_N_CHANNELS,
  PROP_HAS_ALPHA,
  PROP_BITS_PER_SAMPLE,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_ROWSTRIDE,
  PROP_PIXELS
};

G_DEFINE_TYPE (GdkPixbuf, gdk_pixbuf, G_TYPE_OBJECT)

static void 
gdk_pixbuf_init (GdkPixbuf *pixbuf)
{
}

static void
gdk_pixbuf_class_init (GdkPixbufClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        
        object_class->finalize = gdk_pixbuf_finalize;
        object_class->set_property = gdk_pixbuf_set_property;
        object_class->get_property = gdk_pixbuf_get_property;

#define PIXBUF_PARAM_FLAGS G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|\
                           G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
        /**
         * GdkPixbuf:n-channels:
         *
         * The number of samples per pixel. 
         * Currently, only 3 or 4 samples per pixel are supported.
         */
        g_object_class_install_property (object_class,
                                         PROP_N_CHANNELS,
                                         g_param_spec_int ("n-channels",
                                                           P_("Number of Channels"),
                                                           P_("The number of samples per pixel"),
                                                           0,
                                                           G_MAXINT,
                                                           3,
                                                           PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_COLORSPACE,
                                         g_param_spec_enum ("colorspace",
                                                            P_("Colorspace"),
                                                            P_("The colorspace in which the samples are interpreted"),
                                                            GDK_TYPE_COLORSPACE,
                                                            GDK_COLORSPACE_RGB,
                                                            PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_HAS_ALPHA,
                                         g_param_spec_boolean ("has-alpha",
                                                               P_("Has Alpha"),
                                                               P_("Whether the pixbuf has an alpha channel"),
                                                               FALSE,
                                                               PIXBUF_PARAM_FLAGS));

        /**
         * GdkPixbuf:bits-per-sample:
         *
         * The number of bits per sample. 
         * Currently only 8 bit per sample are supported.
         */
        g_object_class_install_property (object_class,
                                         PROP_BITS_PER_SAMPLE,
                                         g_param_spec_int ("bits-per-sample",
                                                           P_("Bits per Sample"),
                                                           P_("The number of bits per sample"),
                                                           1,
                                                           16,
                                                           8,
                                                           PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_WIDTH,
                                         g_param_spec_int ("width",
                                                           P_("Width"),
                                                           P_("The number of columns of the pixbuf"),
                                                           1,
                                                           G_MAXINT,
                                                           1,
                                                           PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_HEIGHT,
                                         g_param_spec_int ("height",
                                                           P_("Height"),
                                                           P_("The number of rows of the pixbuf"),
                                                           1,
                                                           G_MAXINT,
                                                           1,
                                                           PIXBUF_PARAM_FLAGS));

        /**
         * GdkPixbuf:rowstride:
         *
         * The number of bytes between the start of a row and 
         * the start of the next row. This number must (obviously)
         * be at least as large as the width of the pixbuf.
         */
        g_object_class_install_property (object_class,
                                         PROP_ROWSTRIDE,
                                         g_param_spec_int ("rowstride",
                                                           P_("Rowstride"),
                                                           P_("The number of bytes between the start of a row and the start of the next row"),
                                                           1,
                                                           G_MAXINT,
                                                           1,
                                                           PIXBUF_PARAM_FLAGS));

        g_object_class_install_property (object_class,
                                         PROP_PIXELS,
                                         g_param_spec_pointer ("pixels",
                                                               P_("Pixels"),
                                                               P_("A pointer to the pixel data of the pixbuf"),
                                                               PIXBUF_PARAM_FLAGS));
}

static void
gdk_pixbuf_finalize (GObject *object)
{
        GdkPixbuf *pixbuf = GDK_PIXBUF (object);
        
        if (pixbuf->destroy_fn)
                (* pixbuf->destroy_fn) (pixbuf->pixels, pixbuf->destroy_fn_data);
        
        G_OBJECT_CLASS (gdk_pixbuf_parent_class)->finalize (object);
}


/**
 * gdk_pixbuf_ref:
 * @pixbuf: A pixbuf.
 *
 * Adds a reference to a pixbuf. 
 *
 * Return value: The same as the @pixbuf argument.
 *
 * Deprecated: Use g_object_ref().
 **/
GdkPixbuf *
gdk_pixbuf_ref (GdkPixbuf *pixbuf)
{
        return (GdkPixbuf *) g_object_ref (pixbuf);
}

/**
 * gdk_pixbuf_unref:
 * @pixbuf: A pixbuf.
 *
 * Removes a reference from a pixbuf. 
 *
 * Deprecated: Use g_object_unref().
 **/
void
gdk_pixbuf_unref (GdkPixbuf *pixbuf)
{
        g_object_unref (pixbuf);
}



/* Used as the destroy notification function for gdk_pixbuf_new() */
static void
free_buffer (guchar *pixels, gpointer data)
{
	g_free (pixels);
}

/**
 * gdk_pixbuf_new:
 * @colorspace: Color space for image
 * @has_alpha: Whether the image should have transparency information
 * @bits_per_sample: Number of bits per color sample
 * @width: Width of image in pixels, must be > 0
 * @height: Height of image in pixels, must be > 0
 *
 * Creates a new #GdkPixbuf structure and allocates a buffer for it.  The 
 * buffer has an optimal rowstride.  Note that the buffer is not cleared;
 * you will have to fill it completely yourself.
 *
 * Return value: A newly-created #GdkPixbuf with a reference count of 1, or 
 * %NULL if not enough memory could be allocated for the image buffer.
 **/
GdkPixbuf *
gdk_pixbuf_new (GdkColorspace colorspace, 
                gboolean      has_alpha,
                int           bits_per_sample,
                int           width,
                int           height)
{
	guchar *buf;
	int channels;
	int rowstride;
        gsize bytes;

	g_return_val_if_fail (colorspace == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail (bits_per_sample == 8, NULL);
	g_return_val_if_fail (width > 0, NULL);
	g_return_val_if_fail (height > 0, NULL);

	channels = has_alpha ? 4 : 3;
        rowstride = width * channels;
        if (rowstride / channels != width || rowstride + 3 < 0) /* overflow */
                return NULL;
        
	/* Always align rows to 32-bit boundaries */
	rowstride = (rowstride + 3) & ~3;

        bytes = height * rowstride;
        if (bytes / rowstride !=  height) /* overflow */
                return NULL;
            
	buf = g_try_malloc (bytes);
	if (!buf)
		return NULL;

	return gdk_pixbuf_new_from_data (buf, colorspace, has_alpha, bits_per_sample,
					 width, height, rowstride,
					 free_buffer, NULL);
}

/**
 * gdk_pixbuf_copy:
 * @pixbuf: A pixbuf.
 * 
 * Creates a new #GdkPixbuf with a copy of the information in the specified
 * @pixbuf.
 * 
 * Return value: A newly-created pixbuf with a reference count of 1, or %NULL if
 * not enough memory could be allocated.
 **/
GdkPixbuf *
gdk_pixbuf_copy (const GdkPixbuf *pixbuf)
{
	guchar *buf;
	int size;

	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	/* Calculate a semi-exact size.  Here we copy with full rowstrides;
	 * maybe we should copy each row individually with the minimum
	 * rowstride?
	 */

	size = ((pixbuf->height - 1) * pixbuf->rowstride +
		pixbuf->width * ((pixbuf->n_channels * pixbuf->bits_per_sample + 7) / 8));

	buf = g_try_malloc (size * sizeof (guchar));
	if (!buf)
		return NULL;

	memcpy (buf, pixbuf->pixels, size);

	return gdk_pixbuf_new_from_data (buf,
					 pixbuf->colorspace, pixbuf->has_alpha,
					 pixbuf->bits_per_sample,
					 pixbuf->width, pixbuf->height,
					 pixbuf->rowstride,
					 free_buffer,
					 NULL);
}

/**
 * gdk_pixbuf_new_subpixbuf:
 * @src_pixbuf: a #GdkPixbuf
 * @src_x: X coord in @src_pixbuf
 * @src_y: Y coord in @src_pixbuf
 * @width: width of region in @src_pixbuf
 * @height: height of region in @src_pixbuf
 * 
 * Creates a new pixbuf which represents a sub-region of
 * @src_pixbuf. The new pixbuf shares its pixels with the
 * original pixbuf, so writing to one affects both.
 * The new pixbuf holds a reference to @src_pixbuf, so
 * @src_pixbuf will not be finalized until the new pixbuf
 * is finalized.
 * 
 * Return value: a new pixbuf 
 **/
GdkPixbuf*
gdk_pixbuf_new_subpixbuf (GdkPixbuf *src_pixbuf,
                          int        src_x,
                          int        src_y,
                          int        width,
                          int        height)
{
        guchar *pixels;
        GdkPixbuf *sub;

        g_return_val_if_fail (GDK_IS_PIXBUF (src_pixbuf), NULL);
        g_return_val_if_fail (src_x >= 0 && src_x + width <= src_pixbuf->width, NULL);
        g_return_val_if_fail (src_y >= 0 && src_y + height <= src_pixbuf->height, NULL);

        pixels = (gdk_pixbuf_get_pixels (src_pixbuf)
                  + src_y * src_pixbuf->rowstride
                  + src_x * src_pixbuf->n_channels);

        sub = gdk_pixbuf_new_from_data (pixels,
                                        src_pixbuf->colorspace,
                                        src_pixbuf->has_alpha,
                                        src_pixbuf->bits_per_sample,
                                        width, height,
                                        src_pixbuf->rowstride,
                                        NULL, NULL);

        /* Keep a reference to src_pixbuf */
        g_object_ref (src_pixbuf);
  
        g_object_set_qdata_full (G_OBJECT (sub),
                                 g_quark_from_static_string ("gdk-pixbuf-subpixbuf-src"),
                                 src_pixbuf,
                                 (GDestroyNotify) g_object_unref);

        return sub;
}



/* Accessors */

/**
 * gdk_pixbuf_get_colorspace:
 * @pixbuf: A pixbuf.
 *
 * Queries the color space of a pixbuf.
 *
 * Return value: Color space.
 **/
GdkColorspace
gdk_pixbuf_get_colorspace (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), GDK_COLORSPACE_RGB);

	return pixbuf->colorspace;
}

/**
 * gdk_pixbuf_get_n_channels:
 * @pixbuf: A pixbuf.
 *
 * Queries the number of channels of a pixbuf.
 *
 * Return value: Number of channels.
 **/
int
gdk_pixbuf_get_n_channels (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->n_channels;
}

/**
 * gdk_pixbuf_get_has_alpha:
 * @pixbuf: A pixbuf.
 *
 * Queries whether a pixbuf has an alpha channel (opacity information).
 *
 * Return value: %TRUE if it has an alpha channel, %FALSE otherwise.
 **/
gboolean
gdk_pixbuf_get_has_alpha (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), FALSE);

	return pixbuf->has_alpha ? TRUE : FALSE;
}

/**
 * gdk_pixbuf_get_bits_per_sample:
 * @pixbuf: A pixbuf.
 *
 * Queries the number of bits per color sample in a pixbuf.
 *
 * Return value: Number of bits per color sample.
 **/
int
gdk_pixbuf_get_bits_per_sample (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->bits_per_sample;
}

/**
 * gdk_pixbuf_get_pixels:
 * @pixbuf: A pixbuf.
 *
 * Queries a pointer to the pixel data of a pixbuf.
 *
 * Return value: A pointer to the pixbuf's pixel data.  Please see <xref linkend="image-data"/>
 * for information about how the pixel data is stored in
 * memory.
 **/
guchar *
gdk_pixbuf_get_pixels (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	return pixbuf->pixels;
}

/**
 * gdk_pixbuf_get_width:
 * @pixbuf: A pixbuf.
 *
 * Queries the width of a pixbuf.
 *
 * Return value: Width in pixels.
 **/
int
gdk_pixbuf_get_width (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->width;
}

/**
 * gdk_pixbuf_get_height:
 * @pixbuf: A pixbuf.
 *
 * Queries the height of a pixbuf.
 *
 * Return value: Height in pixels.
 **/
int
gdk_pixbuf_get_height (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->height;
}

/**
 * gdk_pixbuf_get_rowstride:
 * @pixbuf: A pixbuf.
 *
 * Queries the rowstride of a pixbuf, which is the number of bytes between the start of a row
 * and the start of the next row.
 *
 * Return value: Distance between row starts.
 **/
int
gdk_pixbuf_get_rowstride (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), -1);

	return pixbuf->rowstride;
}



/* General initialization hooks */
const guint gdk_pixbuf_major_version = GDK_PIXBUF_MAJOR;
const guint gdk_pixbuf_minor_version = GDK_PIXBUF_MINOR;
const guint gdk_pixbuf_micro_version = GDK_PIXBUF_MICRO;

const char *gdk_pixbuf_version = GDK_PIXBUF_VERSION;

/* Error quark */
GQuark
gdk_pixbuf_error_quark (void)
{
  return g_quark_from_static_string ("gdk-pixbuf-error-quark");
}

/**
 * gdk_pixbuf_fill:
 * @pixbuf: a #GdkPixbuf
 * @pixel: RGBA pixel to clear to
 *         (0xffffffff is opaque white, 0x00000000 transparent black)
 *
 * Clears a pixbuf to the given RGBA value, converting the RGBA value into
 * the pixbuf's pixel format. The alpha will be ignored if the pixbuf
 * doesn't have an alpha channel.
 * 
 **/
void
gdk_pixbuf_fill (GdkPixbuf *pixbuf,
                 guint32    pixel)
{
        guchar *pixels;
        guint r, g, b, a;
        guchar *p;
        guint w, h;

        g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

        if (pixbuf->width == 0 || pixbuf->height == 0)
                return;

        pixels = pixbuf->pixels;

        r = (pixel & 0xff000000) >> 24;
        g = (pixel & 0x00ff0000) >> 16;
        b = (pixel & 0x0000ff00) >> 8;
        a = (pixel & 0x000000ff);

        h = pixbuf->height;
        
        while (h--) {
                w = pixbuf->width;
                p = pixels;

                switch (pixbuf->n_channels) {
                case 3:
                        while (w--) {
                                p[0] = r;
                                p[1] = g;
                                p[2] = b;
                                p += 3;
                        }
                        break;
                case 4:
                        while (w--) {
                                p[0] = r;
                                p[1] = g;
                                p[2] = b;
                                p[3] = a;
                                p += 4;
                        }
                        break;
                default:
                        break;
                }
                
                pixels += pixbuf->rowstride;
        }
}



/**
 * gdk_pixbuf_get_option:
 * @pixbuf: a #GdkPixbuf
 * @key: a nul-terminated string.
 * 
 * Looks up @key in the list of options that may have been attached to the
 * @pixbuf when it was loaded, or that may have been attached by another
 * function using gdk_pixbuf_set_option().
 *
 * For instance, the ANI loader provides "Title" and "Artist" options. 
 * The ICO, XBM, and XPM loaders provide "x_hot" and "y_hot" hot-spot 
 * options for cursor definitions. The PNG loader provides the tEXt ancillary
 * chunk key/value pairs as options. Since 2.12, the TIFF and JPEG loaders
 * return an "orientation" option string that corresponds to the embedded 
 * TIFF/Exif orientation tag (if present).
 * 
 * Return value: the value associated with @key. This is a nul-terminated 
 * string that should not be freed or %NULL if @key was not found.
 **/
G_CONST_RETURN gchar *
gdk_pixbuf_get_option (GdkPixbuf   *pixbuf,
                       const gchar *key)
{
        gchar **options;
        gint i;

        g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
        g_return_val_if_fail (key != NULL, NULL);
  
        options = g_object_get_qdata (G_OBJECT (pixbuf), 
                                      g_quark_from_static_string ("gdk_pixbuf_options"));
        if (options) {
                for (i = 0; options[2*i]; i++) {
                        if (strcmp (options[2*i], key) == 0)
                                return options[2*i+1];
                }
        }
        
        return NULL;
}

/**
 * gdk_pixbuf_set_option:
 * @pixbuf: a #GdkPixbuf
 * @key: a nul-terminated string.
 * @value: a nul-terminated string.
 * 
 * Attaches a key/value pair as an option to a #GdkPixbuf. If %key already
 * exists in the list of options attached to @pixbuf, the new value is 
 * ignored and %FALSE is returned.
 *
 * Return value: %TRUE on success.
 *
 * Since: 2.2
 **/
gboolean
gdk_pixbuf_set_option (GdkPixbuf   *pixbuf,
                       const gchar *key,
                       const gchar *value)
{
        GQuark  quark;
        gchar **options;
        gint n = 0;
 
        g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), FALSE);
        g_return_val_if_fail (key != NULL, FALSE);
        g_return_val_if_fail (value != NULL, FALSE);

        quark = g_quark_from_static_string ("gdk_pixbuf_options");

        options = g_object_get_qdata (G_OBJECT (pixbuf), quark);

        if (options) {
                for (n = 0; options[2*n]; n++) {
                        if (strcmp (options[2*n], key) == 0)
                                return FALSE;
                }

                g_object_steal_qdata (G_OBJECT (pixbuf), quark);
                options = g_renew (gchar *, options, 2*(n+1) + 1);
        } else {
                options = g_new (gchar *, 3);
        }
        
        options[2*n]   = g_strdup (key);
        options[2*n+1] = g_strdup (value);
        options[2*n+2] = NULL;

        g_object_set_qdata_full (G_OBJECT (pixbuf), quark,
                                 options, (GDestroyNotify) g_strfreev);
        
        return TRUE;
}

static void
gdk_pixbuf_set_property (GObject         *object,
			 guint            prop_id,
			 const GValue    *value,
			 GParamSpec      *pspec)
{
  GdkPixbuf *pixbuf = GDK_PIXBUF (object);

  switch (prop_id)
          {
          case PROP_COLORSPACE:
                  pixbuf->colorspace = g_value_get_enum (value);
                  break;
          case PROP_N_CHANNELS:
                  pixbuf->n_channels = g_value_get_int (value);
                  break;
          case PROP_HAS_ALPHA:
                  pixbuf->has_alpha = g_value_get_boolean (value);
                  break;
          case PROP_BITS_PER_SAMPLE:
                  pixbuf->bits_per_sample = g_value_get_int (value);
                  break;
          case PROP_WIDTH:
                  pixbuf->width = g_value_get_int (value);
                  break;
          case PROP_HEIGHT:
                  pixbuf->height = g_value_get_int (value);
                  break;
          case PROP_ROWSTRIDE:
                  pixbuf->rowstride = g_value_get_int (value);
                  break;
          case PROP_PIXELS:
                  pixbuf->pixels = (guchar *) g_value_get_pointer (value);
                  break;
          default:
                  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                  break;
          }
}

static void
gdk_pixbuf_get_property (GObject         *object,
			 guint            prop_id,
			 GValue          *value,
			 GParamSpec      *pspec)
{
  GdkPixbuf *pixbuf = GDK_PIXBUF (object);
  
  switch (prop_id)
          {
          case PROP_COLORSPACE:
                  g_value_set_enum (value, gdk_pixbuf_get_colorspace (pixbuf));
                  break;
          case PROP_N_CHANNELS:
                  g_value_set_int (value, gdk_pixbuf_get_n_channels (pixbuf));
                  break;
          case PROP_HAS_ALPHA:
                  g_value_set_boolean (value, gdk_pixbuf_get_has_alpha (pixbuf));
                  break;
          case PROP_BITS_PER_SAMPLE:
                  g_value_set_int (value, gdk_pixbuf_get_bits_per_sample (pixbuf));
                  break;
          case PROP_WIDTH:
                  g_value_set_int (value, gdk_pixbuf_get_width (pixbuf));
                  break;
          case PROP_HEIGHT:
                  g_value_set_int (value, gdk_pixbuf_get_height (pixbuf));
                  break;
          case PROP_ROWSTRIDE:
                  g_value_set_int (value, gdk_pixbuf_get_rowstride (pixbuf));
                  break;
          case PROP_PIXELS:
                  g_value_set_pointer (value, gdk_pixbuf_get_pixels (pixbuf));
                  break;
          default:
                  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                  break;
          }
}

#define __GDK_PIXBUF_C__
#include "gdk-pixbuf-aliasdef.c"
