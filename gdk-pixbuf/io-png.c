/* GdkPixbuf library - JPEG image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Federico Mena-Quintero <federico@gimp.org>
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"



static void
setup_png_transformations(png_structp png_read_ptr, png_infop png_info_ptr,
                          gboolean *fatal_error_occurred,
                          png_uint_32* width_p, png_uint_32* height_p,
                          int* color_type_p)
{
        png_uint_32 width, height;
        int bit_depth, color_type, interlace_type, compression_type, filter_type;
        int channels;
        
        /* Get the image info */

        png_get_IHDR (png_read_ptr, png_info_ptr,
                      &width, &height,
                      &bit_depth,
                      &color_type,
                      &interlace_type,
                      &compression_type,
                      &filter_type);

        /* set_expand() basically needs to be called unless
           we are already in RGB/RGBA mode
        */
        if (color_type == PNG_COLOR_TYPE_PALETTE &&
            bit_depth <= 8) {

                /* Convert indexed images to RGB */
                png_set_expand (png_read_ptr);

        } else if (color_type == PNG_COLOR_TYPE_GRAY &&
                   bit_depth < 8) {

                /* Convert grayscale to RGB */
                png_set_expand (png_read_ptr);

        } else if (png_get_valid (png_read_ptr, 
                                  png_info_ptr, PNG_INFO_tRNS)) {

                /* If we have transparency header, convert it to alpha
                   channel */
                png_set_expand(png_read_ptr);
                
        } else if (bit_depth < 8) {

                /* If we have < 8 scale it up to 8 */
                png_set_expand(png_read_ptr);


                /* Conceivably, png_set_packing() is a better idea;
                 * God only knows how libpng works
                 */
        }

        /* If we are 16-bit, convert to 8-bit */
        if (bit_depth == 16) {
                png_set_strip_16(png_read_ptr);
        }

        /* If gray scale, convert to RGB */
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                png_set_gray_to_rgb(png_read_ptr);
        }
        
        /* If interlaced, handle that */
        if (interlace_type != PNG_INTERLACE_NONE) {
                png_set_interlace_handling(png_read_ptr);
        }
        
        /* Update the info the reflect our transformations */
        png_read_update_info(png_read_ptr, png_info_ptr);
        
        png_get_IHDR (png_read_ptr, png_info_ptr,
                      &width, &height,
                      &bit_depth,
                      &color_type,
                      &interlace_type,
                      &compression_type,
                      &filter_type);

        *width_p = width;
        *height_p = height;
        *color_type_p = color_type;
        
#ifndef G_DISABLE_CHECKS
        /* Check that the new info is what we want */
        
        if (bit_depth != 8) {
                g_warning("Bits per channel of transformed PNG is %d, not 8.", bit_depth);
                *fatal_error_occurred = TRUE;
                return;
        }

        if ( ! (color_type == PNG_COLOR_TYPE_RGB ||
                color_type == PNG_COLOR_TYPE_RGB_ALPHA) ) {
                g_warning("Transformed PNG not RGB or RGBA.");
                *fatal_error_occurred = TRUE;
                return;
        }

        channels = png_get_channels(png_read_ptr, png_info_ptr);
        if ( ! (channels == 3 || channels == 4) ) {
                g_warning("Transformed PNG has %d channels, must be 3 or 4.", channels);
                *fatal_error_occurred = TRUE;
                return;
        }
#endif
}

/* Destroy notification function for the pixbuf */
static void
free_buffer (guchar *pixels, gpointer data)
{
	free (pixels);
}

/* Shared library entry point */
GdkPixbuf *
gdk_pixbuf__png_image_load (FILE *f)
{
	png_structp png_ptr;
	png_infop info_ptr, end_info;
        gboolean failed = FALSE;
	gint i, ctype, bpp;
	png_uint_32 w, h;
	png_bytepp rows;
	guchar *pixels;

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		return NULL;

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct (&png_ptr, NULL, NULL);
		return NULL;
	}

	end_info = png_create_info_struct (png_ptr);
	if (!end_info) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	if (setjmp (png_ptr->jmpbuf)) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	png_init_io (png_ptr, f);
	png_read_info (png_ptr, info_ptr);

        setup_png_transformations(png_ptr, info_ptr, &failed, &w, &h, &ctype);

        if (failed) {
                png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
                return NULL;
        }
        
	if (ctype & PNG_COLOR_MASK_ALPHA)
		bpp = 4;
	else
		bpp = 3;

	pixels = malloc (w * h * bpp);
	if (!pixels) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	rows = g_new (png_bytep, h);

	for (i = 0; i < h; i++)
		rows[i] = pixels + i * w * bpp;

	png_read_image (png_ptr, rows);
	png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
	g_free (rows);

	if (ctype & PNG_COLOR_MASK_ALPHA)
		return gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB, TRUE, 8,
						 w, h, w * 4,
						 free_buffer, NULL);
	else
		return gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB, FALSE, 8,
						 w, h, w * 3,
						 free_buffer, NULL);
}

/* I wish these avoided the setjmp()/longjmp() crap in libpng instead
   just allow you to change the error reporting. */
static void png_error_callback  (png_structp png_read_ptr,
                                 png_const_charp error_msg);

static void png_warning_callback(png_structp png_read_ptr,
                                 png_const_charp warning_msg);

/* Called at the start of the progressive load */
static void png_info_callback   (png_structp png_read_ptr,
                                 png_infop   png_info_ptr);

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void png_row_callback   (png_structp png_read_ptr,
                                png_bytep   new_row,
                                png_uint_32 row_num,
                                int pass_num);

/* Called after reading the entire image */
static void png_end_callback   (png_structp png_read_ptr,
                                png_infop   png_info_ptr);

typedef struct _LoadContext LoadContext;

struct _LoadContext {
        png_structp png_read_ptr;
        png_infop   png_info_ptr;

        ModulePreparedNotifyFunc prepare_func;
        ModuleUpdatedNotifyFunc update_func;
        gpointer notify_user_data;

        GdkPixbuf* pixbuf;

        /* row number of first row seen, or -1 if none yet seen */

        gint first_row_seen_in_chunk;

        /* pass number for the first row seen */

        gint first_pass_seen_in_chunk;
        
        /* row number of last row seen */
        gint last_row_seen_in_chunk;

        gint last_pass_seen_in_chunk;

        /* highest row number seen */
        gint max_row_seen_in_chunk;
        
        guint fatal_error_occurred : 1;

};

gpointer
gdk_pixbuf__png_image_begin_load (ModulePreparedNotifyFunc prepare_func,
				  ModuleUpdatedNotifyFunc update_func,
				  ModuleFrameDoneNotifyFunc frame_done_func,
				  ModuleAnimationDoneNotifyFunc anim_done_func,
				  gpointer user_data)
{
        LoadContext* lc;
        
        lc = g_new0(LoadContext, 1);
        
        lc->fatal_error_occurred = FALSE;

        lc->prepare_func = prepare_func;
        lc->update_func = update_func;
        lc->notify_user_data = user_data;

        lc->first_row_seen_in_chunk = -1;
        lc->last_row_seen_in_chunk = -1;
        lc->first_pass_seen_in_chunk = -1;
        lc->last_pass_seen_in_chunk = -1;
        lc->max_row_seen_in_chunk = -1;
        
        /* Create the main PNG context struct */

		
        lc->png_read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                  lc, /* error/warning callback data */
                                                  png_error_callback,
                                                  png_warning_callback);

        if (lc->png_read_ptr == NULL) {
                g_free(lc);
                return NULL;
        }

	if (setjmp (lc->png_read_ptr->jmpbuf)) {
		if (lc->png_info_ptr)
			png_destroy_read_struct(&lc->png_read_ptr, NULL, NULL);
                g_free(lc);
                return NULL;
	}

        /* Create the auxiliary context struct */

        lc->png_info_ptr = png_create_info_struct(lc->png_read_ptr);

        if (lc->png_info_ptr == NULL) {
                png_destroy_read_struct(&lc->png_read_ptr, NULL, NULL);
                g_free(lc);
                return NULL;
        }

        png_set_progressive_read_fn(lc->png_read_ptr,
                                    lc, /* callback data */
                                    png_info_callback,
                                    png_row_callback,
                                    png_end_callback);
        

        return lc;
}

void
gdk_pixbuf__png_image_stop_load (gpointer context)
{
        LoadContext* lc = context;

        g_return_if_fail(lc != NULL);

        gdk_pixbuf_unref(lc->pixbuf);
        
        png_destroy_read_struct(&lc->png_read_ptr, NULL, NULL);
        g_free(lc);
}

gboolean
gdk_pixbuf__png_image_load_increment(gpointer context, guchar *buf, guint size)
{
        LoadContext* lc = context;

        g_return_val_if_fail(lc != NULL, FALSE);

        /* reset */
        lc->first_row_seen_in_chunk = -1;
        lc->last_row_seen_in_chunk = -1;
        lc->first_pass_seen_in_chunk = -1;
        lc->last_pass_seen_in_chunk = -1;
        lc->max_row_seen_in_chunk = -1;
        
        /* Invokes our callbacks as needed */
	if (setjmp (lc->png_read_ptr->jmpbuf)) {
		return FALSE;
	} else {
		png_process_data(lc->png_read_ptr, lc->png_info_ptr, buf, size);
	}

        if (lc->fatal_error_occurred)
                return FALSE;
        else {
                if (lc->first_row_seen_in_chunk >= 0) {
                        /* We saw at least one row */
                        gint pass_diff = lc->last_pass_seen_in_chunk - lc->first_pass_seen_in_chunk;
                        
                        g_assert(pass_diff >= 0);
                        
                        if (pass_diff == 0) {
                                /* start and end row were in the same pass */
                                (lc->update_func)(lc->pixbuf, 0,
                                                  lc->first_row_seen_in_chunk,
                                                  lc->pixbuf->width,
                                                  (lc->last_row_seen_in_chunk -
                                                   lc->first_row_seen_in_chunk) + 1,
						  lc->notify_user_data);
                        } else if (pass_diff == 1) {
                                /* We have from the first row seen to
                                   the end of the image (max row
                                   seen), then from the top of the
                                   image to the last row seen */
                                /* first row to end */
                                (lc->update_func)(lc->pixbuf, 0,
                                                  lc->first_row_seen_in_chunk,
                                                  lc->pixbuf->width,
                                                  (lc->max_row_seen_in_chunk -
                                                   lc->first_row_seen_in_chunk) + 1,
						  lc->notify_user_data);
                                /* top to last row */
                                (lc->update_func)(lc->pixbuf,
                                                  0, 0, 
                                                  lc->pixbuf->width,
                                                  lc->last_row_seen_in_chunk + 1,
						  lc->notify_user_data);
                        } else {
                                /* We made at least one entire pass, so update the
                                   whole image */
                                (lc->update_func)(lc->pixbuf,
                                                  0, 0, 
                                                  lc->pixbuf->width,
                                                  lc->max_row_seen_in_chunk + 1,
						  lc->notify_user_data);
                        }
                }
                
                return TRUE;
        }
}

/* Called at the start of the progressive load, once we have image info */
static void
png_info_callback   (png_structp png_read_ptr,
                     png_infop   png_info_ptr)
{
        LoadContext* lc;
        png_uint_32 width, height;
        int color_type;
        gboolean have_alpha = FALSE;
        gboolean failed = FALSE;
        
        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;
        

        setup_png_transformations(lc->png_read_ptr,
                                  lc->png_info_ptr,
                                  &failed,
                                  &width, &height, &color_type);

        if (failed) {
                lc->fatal_error_occurred = TRUE;
                return;
        }

        /* If we have alpha, set a flag */
        if (color_type & PNG_COLOR_MASK_ALPHA)
                have_alpha = TRUE;
        
        lc->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, have_alpha, 8, width, height);

        if (lc->pixbuf == NULL) {
                /* Failed to allocate memory */
                lc->fatal_error_occurred = TRUE;
                return;
        }
        
        /* Notify the client that we are ready to go */

        if (lc->prepare_func)
                (* lc->prepare_func) (lc->pixbuf, lc->notify_user_data);
        
        return;
}

/* Called for each row; note that you will get duplicate row numbers
   for interlaced PNGs */
static void
png_row_callback   (png_structp png_read_ptr,
                    png_bytep   new_row,
                    png_uint_32 row_num,
                    int pass_num)
{
        LoadContext* lc;
        guchar* old_row = NULL;

        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;

        if (lc->first_row_seen_in_chunk < 0) {
                lc->first_row_seen_in_chunk = row_num;
                lc->first_pass_seen_in_chunk = pass_num;
        }

        lc->max_row_seen_in_chunk = MAX(lc->max_row_seen_in_chunk, ((gint)row_num));
        lc->last_row_seen_in_chunk = row_num;
        lc->last_pass_seen_in_chunk = pass_num;
        
        old_row = lc->pixbuf->pixels + (row_num * lc->pixbuf->rowstride);

        png_progressive_combine_row(lc->png_read_ptr, old_row, new_row);
}

/* Called after reading the entire image */
static void
png_end_callback   (png_structp png_read_ptr,
                    png_infop   png_info_ptr)
{
        LoadContext* lc;

        lc = png_get_progressive_ptr(png_read_ptr);

        if (lc->fatal_error_occurred)
                return;
}

static void
png_error_callback(png_structp png_read_ptr,
                   png_const_charp error_msg)
{
        LoadContext* lc;
        
        lc = png_get_error_ptr(png_read_ptr);
        
        lc->fatal_error_occurred = TRUE;
        
        fprintf(stderr, "Fatal error loading PNG: %s\n", error_msg);
}

static void
png_warning_callback(png_structp png_read_ptr,
                     png_const_charp warning_msg)
{
        LoadContext* lc;
        
        lc = png_get_error_ptr(png_read_ptr);
        
        fprintf(stderr, "Warning loading PNG: %s\n", warning_msg);
}




