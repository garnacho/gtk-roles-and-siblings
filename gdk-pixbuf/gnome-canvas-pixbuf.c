/* GNOME libraries - GdkPixbuf item for the GNOME canvas
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
#include <math.h>
#include <libart_lgpl/art_rgb_affine_private.h>
#include "gdk-pixbuf.h"
#include "gnome-canvas-pixbuf.h"



/* Private part of the GnomeCanvasPixbuf structure */
typedef struct {
	/* Our gdk-pixbuf */
	GdkPixbuf *pixbuf;

	/* Width value */
	double width;

	/* Height value */
	double height;

	/* Whether dimensions are set and whether they are in pixels or units */
	guint width_set : 1;
	guint width_pixels : 1;
	guint height_set : 1;
	guint height_pixels : 1;

	/* Whether the pixbuf has changed */
	guint need_pixbuf_update : 1;

	/* Whether the size has changed */
	guint need_size_update : 1;
} PixbufPrivate;



/* Object argument IDs */
enum {
	ARG_0,
	ARG_PIXBUF,
	ARG_WIDTH,
	ARG_WIDTH_SET,
	ARG_WIDTH_PIXELS,
	ARG_HEIGHT,
	ARG_HEIGHT_SET,
	ARG_HEIGHT_PIXELS
};

static void gnome_canvas_pixbuf_class_init (GnomeCanvasPixbufClass *class);
static void gnome_canvas_pixbuf_init (GnomeCanvasPixbuf *cpb);
static void gnome_canvas_pixbuf_destroy (GtkObject *object);
static void gnome_canvas_pixbuf_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gnome_canvas_pixbuf_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void gnome_canvas_pixbuf_update (GnomeCanvasItem *item, double *affine,
					ArtSVP *clip_path, int flags);
static void gnome_canvas_pixbuf_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
				      int x, int y, int width, int height);
static double gnome_canvas_pixbuf_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
					 GnomeCanvasItem **actual_item);
static void gnome_canvas_pixbuf_bounds (GnomeCanvasItem *item,
					double *x1, double *y1, double *x2, double *y2);

static GnomeCanvasItemClass *parent_class;



/**
 * gnome_canvas_pixbuf_get_type:
 * @void:
 *
 * Registers the &GnomeCanvasPixbuf class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the &GnomeCanvasPixbuf class.
 **/
GtkType
gnome_canvas_pixbuf_get_type (void)
{
	static GtkType canvas_pixbuf_type = 0;

	if (!canvas_pixbuf_type) {
		static const GtkTypeInfo canvas_pixbuf_info = {
			"GnomeCanvasPixbuf",
			sizeof (GnomeCanvasPixbuf),
			sizeof (GnomeCanvasPixbufClass),
			(GtkClassInitFunc) gnome_canvas_pixbuf_class_init,
			(GtkObjectInitFunc) gnome_canvas_pixbuf_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		canvas_pixbuf_type = gtk_type_unique (gnome_canvas_item_get_type (),
						      &canvas_pixbuf_info);
	}

	return canvas_pixbuf_type;
}

/* Class initialization function for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_class_init (GnomeCanvasPixbufClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("GnomeCanvasPixbuf::pixbuf",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_PIXBUF);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::width",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::width_set",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_WIDTH_SET);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::width_pixels",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_WIDTH_PIXELS);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::height",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_HEIGHT);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::height_set",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HEIGHT_SET);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::height_pixels",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HEIGHT_PIXELS);

	object_class->destroy = gnome_canvas_pixbuf_destroy;
	object_class->set_arg = gnome_canvas_pixbuf_set_arg;
	object_class->get_arg = gnome_canvas_pixbuf_get_arg;

	item_class->update = gnome_canvas_pixbuf_update;
	item_class->draw = gnome_canvas_pixbuf_draw;
	item_class->point = gnome_canvas_pixbuf_point;
	item_class->bounds = gnome_canvas_pixbuf_bounds;
}

/* Object initialization function for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_init (GnomeCanvasPixbuf *gcp)
{
	PixbufPrivate *priv;

	priv = g_new0 (PixbufPrivate, 1);
	gcp->priv = priv;

	priv->width = 0.0;
	priv->height = 0.0;
}

/* Destroy handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_destroy (GtkObject *object)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIXBUF (object));

	gcp = (GNOME_CANVAS_PIXBUF (object));
	priv = gcp->priv;

	/* FIXME: redraw area */

	if (priv->pixbuf)
		gdk_pixbuf_unref (priv->pixbuf);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Set_arg handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	GdkPixbuf *pixbuf;
	double val;

	item = GNOME_CANVAS_ITEM (object);
	gcp = GNOME_CANVAS_PIXBUF (object);
	priv = gcp->priv;

	switch (arg_id) {
	case ARG_PIXBUF:
		pixbuf = GTK_VALUE_POINTER (*arg);
		if (pixbuf != priv->pixbuf) {
			if (pixbuf) {
				g_return_if_fail (pixbuf->art_pixbuf->format != ART_PIX_RGB);
				g_return_if_fail (pixbuf->art_pixbuf->n_channels == 3
						  || pixbuf->art_pixbuf->n_channels == 4);
				g_return_if_fail (pixbuf->art_pixbuf->bits_per_sample == 8);

				gdk_pixbuf_ref (pixbuf);
			}

			if (priv->pixbuf)
				gdk_pixbuf_unref (priv->pixbuf);

			priv->pixbuf = pixbuf;
		}

		priv->need_pixbuf_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_WIDTH:
		val = GTK_VALUE_DOUBLE (*arg);
		g_return_if_fail (val >= 0.0);
		priv->width = val;
		priv->need_size_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_WIDTH_SET:
		priv->width_set = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_size_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_WIDTH_PIXELS:
		priv->width_pixels = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_size_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_HEIGHT:
		val = GTK_VALUE_DOUBLE (*arg);
		g_return_if_fail (val >= 0.0);
		priv->height = val;
		priv->need_size_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_HEIGHT_SET:
		priv->height_set = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_size_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_HEIGHT_PIXELS:
		priv->height_pixels = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_size_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	default:
		break;
	}
}

/* Get_arg handler for the pixbuf canvasi item */
static void
gnome_canvas_pixbuf_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;

	item = GNOME_CANVAS_ITEM (object);
	gcp = GNOME_CANVAS_PIXBUF (object);
	priv = gcp->priv;

	switch (arg_id) {
	case ARG_PIXBUF:
		GTK_VALUE_POINTER (*arg) = priv->pixbuf;
		break;

	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = priv->width;
		break;

	case ARG_WIDTH_SET:
		GTK_VALUE_BOOL (*arg) = priv->width_set;
		break;

	case ARG_WIDTH_PIXELS:
		GTK_VALUE_BOOL (*arg) = priv->width_pixels;
		break;

	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = priv->height;
		break;

	case ARG_HEIGHT_SET:
		GTK_VALUE_BOOL (*arg) = priv->height_set;
		break;

	case ARG_HEIGHT_PIXELS:
		GTK_VALUE_BOOL (*arg) = priv->height_pixels;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}



/* Bounds and utilities */

/* Computes horizontal and vertical scaling vectors for the final transformation
 * to canvas pixel coordinates of a pixbuf canvas item.  The horizontal and
 * vertical dimensions may be specified in units or pixels, separately, so we
 * have to compute the components individually for each dimension.
 */
static void
compute_xform_vectors (GnomeCanvasPixbuf *gcp, double *i2c, ArtPoint *i_c, ArtPoint *j_c)
{
	PixbufPrivate *priv;
	ArtPoint orig, orig_c;
	ArtPoint i, j;
	double length;

	priv = gcp->priv;

	/* Our origin */

	orig.x = 0.0;
	orig.y = 0.0;
	art_affine_point (&orig_c, &orig, i2c);

	/* Horizontal and vertical vectors */

	i.x = 1.0;
	i.y = 0.0;
	art_affine_point (&i_c, &i, i2c);
	i_c.x -= orig_c.x;
	i_c.y -= orig_c.y;

	j.x = 0.0;
	j.y = 1.0;
	art_affine_point (&j_c, &j, i2c);
	j_c.x -= orig_c.x;
	j_c.y -= orig_c.y;

	/* Compute the scaling vectors.  If a dimension is specified in pixels,
	 * we normalize the base vector first so that it will represent the size
	 * in pixels when scaled.
	 */

	if (priv->width_pixels) {
		length = sqrt (i_c.x * i_c.x + i_c.y * i_c.y);

		if (length > GNOME_CANVAS_EPSILON) {
			i_c->x /= length;
			i_c->y /= length;
		} else
			i_c->x = i_c->y = 0.0;
	}

	if (priv->height_pixels) {
		length = sqrt (j_c.x * j_c.x + j_c.y * j_c.y);

		if (length > GNOME_CANVAS_EPSILON) {
			j_c->x /= length;
			j_c->y /= length;
		} else
			j_c->x = j_c->y = 0.0;
	}
}

/* Recomputes the bounding box of a pixbuf canvas item.  The horizontal and
 * vertical dimensions may be specified in units or pixels, separately, so we
 * have to compute the components individually for each dimension.
 */
static void
recompute_bounding_box (GnomeCanvasPixbuf *gcp)
{
	GnomeCanvasItem *item;
	PixbufPrivate *priv;
	double i2c[6];
	ArtPoint orig, orig_c;
	ArtPoint i_c, j_c;
	double w, h;
	double length;
	double x1, y1, x2, y2, x3, y3, x4, y4;
	double mx1, my1, mx2, my2;

	item = GNOME_CANVAS_ITEM (gcp);
	priv = gcp->priv;

	if (!priv->pixbuf) {
		item->x1 = item->y1 = item->x2 = item->y2 = 0.0;
		return;
	}

	gnome_canvas_item_i2c_affine (item, i2c);

	/* Our "origin" */

	orig.x = 0.0;
	orig.y = 0.0;
	art_affine_point (&orig_c, &orig, i2c);

	/* Horizontal and vertical vectors */

	compute_xform_vectors (gcp, i2c, &i_c, &j_c);

	/* Width vector */

	if (priv->width_set)
		w = priv->width;
	else if (priv->pixbuf)
		w = priv->pixbuf->art_pixbuf->width;
	else
		w = 0.0;

	i_c.x *= w;
	i_c.y *= w;

	/* Height */

	if (priv->height_set)
		h = priv->height;
	else if (priv->pixbuf)
		h = priv->pixbuf->art_pixbuf->height;
	else
		h = 0.0;

	j_c.x *= h;
	j_c.y *= h;

	/* Compute vertices */

	x1 = orig_c.x;
	y1 = orig_c.y;

	x2 = orig_c.x + i_c.x;
	y2 = orig_c.y + i_c.y;

	x3 = orig_c.x + j_c.x;
	y3 = orig_c.y + j_c.y;

	x4 = orig_c.x + i_c.x + j_c.x;
	y4 = orig_c.y + i_c.y + j_c.y;

	/* Compute bounds */

	mx1 = MIN (x1, x2);
	my1 = MIN (y1, y2);
	mx2 = MAX (x3, x4);
	my2 = MAX (y3, y4);

	if (mx1 < mx2) {
		item->x1 = mx1;
		item->x2 = mx2;
	} else {
		item->x1 = mx2;
		item->x2 = mx1;
	}

	if (my1 < my2) {
		item->y1 = my1;
		item->y2 = my2;
	} else {
		item->y1 = my2;
		item->y2 = my1;
	}
}



/* Update sequence */

/* Update handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	if (((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
	     && !(GTK_OBJECT_FLAGS (item) & GNOME_CANVAS_ITEM_VISIBLE))
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)
	    || priv->need_pixbuf_update
	    || priv->need_size_update)
		gnome_canvas_request_redraw (item, item->x1, item->y1, item->x2, item->y2);

	/* If we need a pixbuf update, or if the item changed visibility to
	 * shown, recompute the bounding box.
	 */
	if (priv->need_pixbuf_update
	    || priv->need_size_update
	    || ((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
		&& (GTK_OBJECT_FLAGS (ii) & GNOME_CANVAS_ITEM_VISIBLE))
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)) {
		recompute_bounding_box (gcp);
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
		priv->need_pixbuf_update = FALSE;
		priv->need_size_update = FALSE;
	}
	
}



/* Rendering */

/* Fills the specified buffer with the transformed version of a pixbuf */
static void
transform_pixbuf (guchar *dest, int x, int y, int width, int height, int rowstride,
		  GdkPixbuf *pixbuf, double *affine)
{
	ArtPixbuf *apb;
	int xx, yy;
	double inv[6];
	guchar *src, *d;
	ArtPoint src_p, dest_p;
	int run_x1, run_x2;
	int src_x, src_y;
	int i;

	apb = pixbuf->art_pixbuf;

	art_affine_invert (inv, affine);

	for (yy = 0; yy < height; yy++) {
		dest_p.y = y + yy + 0.5;

		run_x1 = x;
		run_x1 = x + width;
		art_affine_run (&run_x1, &run_x2, yy + y,
				apb->width, apb->height,
				inv);

		d = dest + yy * rowstride + (run_x1 - x) * 4;

		for (xx = run_x0; xx < run-x1; xx++) {
			dest_p.x = x + 0.5;
			art_affine_point (&src_p, &dest_p, inv);
			src_x = floor (src_p.x + 0.5);
			src_y = floor (src_p.y + 0.5);

			src = apb->pixels + src_y * apb->rowstride + src_x * apb->num_channels;

			for (i = 0; i < apb->num_channels; i++)
				*d++ = *src++;

			if (!apb->has_alpha)
				d++; /* It is already zeroed */
		}
	}
}

/* Draw handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
			  int x, int y, int width, int height)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	double i2c[6];
	ArtPoint i_c, j_c;
	double i_len, j_len;
	double scale[6], final[6];
	guchar *buf;
	GdkPixbuf *pixbuf;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (!priv->pixbuf)
		return;

	/* Compute scaling factors and build the final affine */

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_xform_vectors (gcp, i2c, &i_c, &j_c);

	i_len = sqrt (i_c.x * i_c.x + i_c.y * i_c.y);
	j_len = sqrt (j_c.x * j_c.x + j_c.y * j_c.y);

	art_affine_scale (scale, i_len, j_len);
	art_affine_multiply (final, i2c, scale);

	buf = g_new0 (guchar, width * height * 4);
	transform_pixbuf (buf, x, y, width, height, width * 4, priv->pixbuf, final);

	pixbuf = gdk_pixbuf_new_from_data (buf, ART_PIX_RGB, TRUE,
					   width, height, width * 4,
					   NULL, NULL);

	gdk_pixbuf_render_to_drawable (pixbuf, drawable, 0, 0, 0, 0, width, height,
				       GDK_RGB_DITHER_MAX, x, y);

	gdk_pixbuf_unref (pixbuf);
	g_free (buf);
}
