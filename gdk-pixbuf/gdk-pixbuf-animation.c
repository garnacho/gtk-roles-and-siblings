/* GdkPixbuf library - Simple animation support
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Jonathan Blandford <jrb@redhat.com>
 *          Havoc Pennington <hp@redhat.com>
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
#include "gdk-pixbuf-io.h"
#include "gdk-pixbuf-private.h"



/**
 * gdk_pixbuf_animation_new_from_file:
 * @filename: Name of file to load.
 *
 * Creates a new animation by loading it from a file.  The file format is
 * detected automatically.  If the file's format does not support multi-frame
 * images, then an animation with a single frame will be created.
 *
 * Return value: A newly created animation with a reference count of 1, or NULL
 * if any of several error conditions ocurred:  the file could not be opened,
 * there was no loader for the file's format, there was not enough memory to
 * allocate the image buffer, or the image file contained invalid data.
 **/
GdkPixbufAnimation *
gdk_pixbuf_animation_new_from_file (const char *filename)
{
	GdkPixbufAnimation *animation;
	int size;
	FILE *f;
	guchar buffer [128];
	GdkPixbufModule *image_module;

	g_return_val_if_fail (filename != NULL, NULL);

	f = fopen (filename, "r");
	if (!f)
		return NULL;

	size = fread (&buffer, 1, sizeof (buffer), f);

	if (size == 0) {
		fclose (f);
		return NULL;
	}

	image_module = gdk_pixbuf_get_module (buffer, size);
	if (!image_module) {
		g_warning ("Unable to find handler for file: %s", filename);
		fclose (f);
		return NULL;
	}

	if (image_module->module == NULL)
		gdk_pixbuf_load_module (image_module);

	if (image_module->load_animation == NULL) {
		GdkPixbuf *pixbuf;
		GdkPixbufFrame *frame;

		/* Keep this logic in sync with gdk_pixbuf_new_from_file() */

		if (image_module->load == NULL) {
			fclose (f);
			return NULL;
		}

		fseek (f, 0, SEEK_SET);
		pixbuf = (* image_module->load) (f);
		fclose (f);

		if (pixbuf)
			g_assert (pixbuf->ref_count > 0);
		else
			return NULL;

		frame = g_new (GdkPixbufFrame, 1);
		frame->pixbuf = pixbuf;
		frame->x_offset = 0;
		frame->y_offset = 0;
		frame->delay_time = -1;
		frame->action = GDK_PIXBUF_FRAME_RETAIN;

		animation = g_new0 (GdkPixbufAnimation, 1);
		animation->ref_count = 1;
		animation->n_frames = 1;
		animation->frames = g_list_prepend (NULL, frame);
		animation->width = gdk_pixbuf_get_width (pixbuf);
		animation->height = gdk_pixbuf_get_height (pixbuf);
	} else {
		fseek (f, 0, SEEK_SET);
		animation = (* image_module->load_animation) (f);
		fclose (f);
	}

	return animation;
}

/**
 * gdk_pixbuf_animation_ref:
 * @animation: An animation.
 *
 * Adds a reference to an animation.  It must be released afterwards using
 * gdk_pixbuf_animation_unref().
 *
 * Return value: The same as the @animation argument.
 **/
GdkPixbufAnimation *
gdk_pixbuf_animation_ref (GdkPixbufAnimation *animation)
{
	g_return_val_if_fail (animation != NULL, NULL);
	g_return_val_if_fail (animation->ref_count > 0, NULL);

	animation->ref_count++;
	return animation;
}

/**
 * gdk_pixbuf_animation_unref:
 * @animation: An animation.
 * 
 * Removes a reference from an animation.  It will be destroyed when the
 * reference count drops to zero.  At that point, all the frames in the
 * animation will be freed and their corresponding pixbufs will be unreferenced.
 **/
void
gdk_pixbuf_animation_unref (GdkPixbufAnimation *animation)
{
	g_return_if_fail (animation != NULL);
	g_return_if_fail (animation->ref_count > 0);

	animation->ref_count--;

	if (animation->ref_count == 0) {
		GList *l;
		GdkPixbufFrame *frame;

		for (l = animation->frames; l; l = l->next) {
			frame = l->data;
			gdk_pixbuf_unref (frame->pixbuf);
			g_free (frame);
		}

		g_list_free (animation->frames);
		g_free (animation);
	}
}

/**
 * gdk_pixbuf_animation_get_width:
 * @animation: An animation.
 *
 * Queries the width of the bounding box of a pixbuf animation.
 * 
 * Return value: Width of the bounding box of the animation.
 **/
int
gdk_pixbuf_animation_get_width (GdkPixbufAnimation *animation)
{
	g_return_val_if_fail (animation != NULL, -1);

	return animation->width;
}

/**
 * gdk_pixbuf_animation_get_height:
 * @animation: An animation.
 *
 * Queries the height of the bounding box of a pixbuf animation.
 * 
 * Return value: Height of the bounding box of the animation.
 **/
int
gdk_pixbuf_animation_get_height (GdkPixbufAnimation *animation)
{
	g_return_val_if_fail (animation != NULL, -1);

	return animation->height;
}

/**
 * gdk_pixbuf_animation_get_num_frames:
 * @animation: An animation.
 *
 * Queries the number of frames in a pixbuf animation.
 * 
 * Return value: Number of frames in the animation.
 **/
int
gdk_pixbuf_animation_get_num_frames (GdkPixbufAnimation *animation)
{
	g_return_val_if_fail (animation != NULL, -1);

	return animation->n_frames;
}

/**
 * gdk_pixbuf_animation_get_frames:
 * @animation: An animation.
 *
 * Queries the list of frames of an animation.
 * 
 * Return value: List of frames in the animation; this is a #GList of
 * #GdkPixbufFrame structures.
 **/
GList *
gdk_pixbuf_animation_get_frames (GdkPixbufAnimation *animation)
{
	g_return_val_if_fail (animation != NULL, NULL);

	return animation->frames;
}



/**
 * gdk_pixbuf_frame_get_pixbuf:
 * @frame: A pixbuf animation frame.
 * 
 * Queries the pixbuf of an animation frame.
 * 
 * Return value: A pixbuf.
 **/
GdkPixbuf *
gdk_pixbuf_frame_get_pixbuf (GdkPixbufFrame *frame)
{
	g_return_val_if_fail (frame != NULL, NULL);

	return frame->pixbuf;
}

/**
 * gdk_pixbuf_frame_get_x_offset:
 * @frame: A pixbuf animation frame.
 * 
 * Queries the X offset of an animation frame.
 * 
 * Return value: X offset from the top left corner of the animation.
 **/
int
gdk_pixbuf_frame_get_x_offset (GdkPixbufFrame *frame)
{
	g_return_val_if_fail (frame != NULL, -1);

	return frame->x_offset;
}

/**
 * gdk_pixbuf_frame_get_y_offset:
 * @frame: A pixbuf animation frame.
 * 
 * Queries the Y offset of an animation frame.
 * 
 * Return value: Y offset from the top left corner of the animation.
 **/
int
gdk_pixbuf_frame_get_y_offset (GdkPixbufFrame *frame)
{
	g_return_val_if_fail (frame != NULL, -1);

	return frame->y_offset;
}

/**
 * gdk_pixbuf_frame_get_delay_time:
 * @frame: A pixbuf animation frame.
 * 
 * Queries the delay time in milliseconds of an animation frame.
 * 
 * Return value: Delay time in milliseconds.
 **/
int
gdk_pixbuf_frame_get_delay_time (GdkPixbufFrame *frame)
{
	g_return_val_if_fail (frame != NULL, -1);

	return frame->delay_time;
}

/**
 * gdk_pixbuf_frame_get_action:
 * @frame: A pixbuf animation frame.
 * 
 * Queries the overlay action of an animation frame.
 * 
 * Return value: Overlay action for this frame.
 **/
GdkPixbufFrameAction
gdk_pixbuf_frame_get_action (GdkPixbufFrame *frame)
{
	g_return_val_if_fail (frame != NULL, GDK_PIXBUF_FRAME_RETAIN);

	return frame->action;
}
