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
#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-canvas-util.h>
#include "gdk-pixbuf/gdk-pixbuf.h"
#include "gdk-pixbuf/gnome-canvas-pixbuf.h"
#include "libart_lgpl/art_rgb_pixbuf_affine.h"



/* Private part of the GnomeCanvasPixbuf structure */
typedef struct {
	/* Our gdk-pixbuf */
	GdkPixbuf *pixbuf;

	/* Width value */
	double width;

	/* Height value */
	double height;

	/* X translation */
	double x;

	/* Y translation */
	double y;

	/* Whether dimensions are set and whether they are in pixels or units */
	guint width_set : 1;
	guint width_pixels : 1;
	guint height_set : 1;
	guint height_pixels : 1;
	guint x_set : 1;
	guint x_pixels : 1;
	guint y_set : 1;
	guint y_pixels : 1;

	/* Whether the pixbuf has changed */
	guint need_pixbuf_update : 1;

	/* Whether the transformation or size have changed */
	guint need_xform_update : 1;
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
	ARG_HEIGHT_PIXELS,
	ARG_X,
	ARG_X_SET,
	ARG_X_PIXELS,
	ARG_Y,
	ARG_Y_SET,
	ARG_Y_PIXELS
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
static void gnome_canvas_pixbuf_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf);
static double gnome_canvas_pixbuf_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
					 GnomeCanvasItem **actual_item);
static void gnome_canvas_pixbuf_bounds (GnomeCanvasItem *item,
					double *x1, double *y1, double *x2, double *y2);

static GnomeCanvasItemClass *parent_class;



/**
 * gnome_canvas_pixbuf_get_type:
 * @void:
 *
 * Registers the #GnomeCanvasPixbuf class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GnomeCanvasPixbuf class.
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
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::x",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::x_set",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_X_SET);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::x_pixels",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_X_PIXELS);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::y",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::y_set",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_Y_SET);
	gtk_object_add_arg_type ("GnomeCanvasPixbuf::y_pixels",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_Y_PIXELS);


	object_class->destroy = gnome_canvas_pixbuf_destroy;
	object_class->set_arg = gnome_canvas_pixbuf_set_arg;
	object_class->get_arg = gnome_canvas_pixbuf_get_arg;

	item_class->update = gnome_canvas_pixbuf_update;
	item_class->draw = gnome_canvas_pixbuf_draw;
	item_class->render = gnome_canvas_pixbuf_render;
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
	priv->x = 0.0;
	priv->y = 0.0;
}

/* Destroy handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_destroy (GtkObject *object)
{
	GnomeCanvasItem *item;
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIXBUF (object));

	item = GNOME_CANVAS_ITEM (object);
	gcp = (GNOME_CANVAS_PIXBUF (object));
	priv = gcp->priv;

	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

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
				g_return_if_fail (pixbuf->art_pixbuf->format == ART_PIX_RGB);
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
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_WIDTH_SET:
		priv->width_set = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_WIDTH_PIXELS:
		priv->width_pixels = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_HEIGHT:
		val = GTK_VALUE_DOUBLE (*arg);
		g_return_if_fail (val >= 0.0);
		priv->height = val;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_HEIGHT_SET:
		priv->height_set = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_HEIGHT_PIXELS:
		priv->height_pixels = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_X:
		priv->x = GTK_VALUE_DOUBLE (*arg);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_X_SET:
		priv->x_set = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_X_PIXELS:
		priv->x_pixels = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_Y:
		priv->y = GTK_VALUE_DOUBLE (*arg);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_Y_SET:
		priv->y_set = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_Y_PIXELS:
		priv->y_pixels = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_xform_update = TRUE;
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

	case ARG_X:
		GTK_VALUE_DOUBLE (*arg) = priv->x;
		break;

	case ARG_X_SET:
		GTK_VALUE_BOOL (*arg) = priv->x_set;
		break;

	case ARG_X_PIXELS:
		GTK_VALUE_BOOL (*arg) = priv->x_pixels;
		break;

	case ARG_Y:
		GTK_VALUE_DOUBLE (*arg) = priv->y;
		break;

	case ARG_Y_SET:
		GTK_VALUE_BOOL (*arg) = priv->y_set;
		break;

	case ARG_Y_PIXELS:
		GTK_VALUE_BOOL (*arg) = priv->y_pixels;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}



/* Bounds and utilities */

/* Computes the amount by which the unit horizontal and vertical vectors will be
 * scaled by an affine transformation.
 */
static void
compute_xform_scaling (double *affine, ArtPoint *i_c, ArtPoint *j_c)
{
	ArtPoint orig, orig_c;
	ArtPoint i, j;

	/* Origin */

	orig.x = 0.0;
	orig.y = 0.0;
	art_affine_point (&orig_c, &orig, affine);

	/* Horizontal and vertical vectors */

	i.x = 1.0;
	i.y = 0.0;
	art_affine_point (i_c, &i, affine);
	i_c->x -= orig_c.x;
	i_c->y -= orig_c.y;

	j.x = 0.0;
	j.y = 1.0;
	art_affine_point (j_c, &j, affine);
	j_c->x -= orig_c.x;
	j_c->y -= orig_c.y;
}

/* computes the addtional resolution dependent affine needed to
 * fit the image within its viewport defined by x,y,width and height
 * args
 */
static void
compute_viewport_affine (GnomeCanvasPixbuf *gcp, double *viewport_affine, double *i2c)
{
	PixbufPrivate *priv;
	ArtPoint i_c, j_c;
	double i_len, j_len;
	double si_len, sj_len;
	double ti_len, tj_len;
	double scale[6], translate[6];
	double w, h;
	double x, y;

	priv = gcp->priv;

	/* Compute scaling vectors and required width/height */

	compute_xform_scaling (i2c, &i_c, &j_c);

	i_len = sqrt (i_c.x * i_c.x + i_c.y * i_c.y);
	j_len = sqrt (j_c.x * j_c.x + j_c.y * j_c.y);

	if (priv->width_set)
		w = priv->width;
	else
		w = priv->pixbuf->art_pixbuf->width;

	if (priv->height_set)
		h = priv->height;
	else
		h = priv->pixbuf->art_pixbuf->height;

	if (priv->x_set)
		x = priv->x;
	else
		x = 0.0;

	if (priv->y_set)
		y = priv->y;
	else
		y = 0.0;

	/* Convert i_len and j_len into scaling factors */

	if (priv->width_pixels) {
		if (i_len > GNOME_CANVAS_EPSILON)
			si_len = 1.0 / i_len;
		else
			si_len = 0.0;
	} else
		si_len = 1.0;

	si_len *= w / priv->pixbuf->art_pixbuf->width;

	if (priv->height_pixels) {
		if (j_len > GNOME_CANVAS_EPSILON)
			sj_len = 1.0 / j_len;
		else
			sj_len = 0.0;
	} else
		sj_len = 1.0;

	sj_len *= h / priv->pixbuf->art_pixbuf->height;

	/* Calculate translation offsets */

	if (priv->x_pixels) {
		if (i_len > GNOME_CANVAS_EPSILON)
			ti_len = 1.0 / i_len;
		else
			ti_len = 0.0;
	} else
		ti_len = 1.0;

	ti_len *= x;

	if (priv->y_pixels) {
		if (j_len > GNOME_CANVAS_EPSILON)
			tj_len = 1.0 / j_len;
		else
			tj_len = 0.0;
	} else
		tj_len = 1.0;

	tj_len *= y;

	/* Compute the final affine */

	art_affine_scale (scale, si_len, sj_len);
	art_affine_translate (translate, ti_len, tj_len);
	art_affine_multiply (viewport_affine, scale, translate);
}

/* Computes the affine transformation with which the pixbuf needs to be
 * transformed to render it on the canvas.  This is not the same as the
 * item_to_canvas transformation because we may need to scale the pixbuf
 * by some other amount.
 */
static void
compute_render_affine (GnomeCanvasPixbuf *gcp, double *render_affine, double *i2c)
{
	double viewport_affine[6];

	compute_viewport_affine (gcp, viewport_affine, i2c);
	art_affine_multiply (render_affine, viewport_affine, i2c);
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
	double i2c[6], render_affine[6];
	ArtDRect rect;

	item = GNOME_CANVAS_ITEM (gcp);
	priv = gcp->priv;

	if (!priv->pixbuf) {
		item->x1 = item->y1 = item->x2 = item->y2 = 0.0;
		return;
	}

	rect.x0 = 0.0;
	rect.x1 = priv->pixbuf->art_pixbuf->width;

	rect.y0 = 0.0;
	rect.y1 = priv->pixbuf->art_pixbuf->height;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (gcp, render_affine, i2c);
	art_drect_affine_transform (&rect, &rect, render_affine);

	item->x1 = floor (rect.x0);
	item->y1 = floor (rect.y0);
	item->x2 = ceil (rect.x1);
	item->y2 = ceil (rect.y1);
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
	    || priv->need_xform_update)
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	/* If we need a pixbuf update, or if the item changed visibility to
	 * shown, recompute the bounding box.
	 */
	if (priv->need_pixbuf_update
	    || priv->need_xform_update
	    || ((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
		&& (GTK_OBJECT_FLAGS (gcp) & GNOME_CANVAS_ITEM_VISIBLE))
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)) {
		recompute_bounding_box (gcp);
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
		priv->need_pixbuf_update = FALSE;
		priv->need_xform_update = FALSE;
	}
}



/* Rendering */

/* This is private to libart, but we need it.  Sigh. */
extern void art_rgb_affine_run (int *p_x0, int *p_x1, int y, int src_width, int src_height,
				const double affine[6]);

/* Fills the specified buffer with the transformed version of a pixbuf */
static void
transform_pixbuf (guchar *dest, int x, int y, int width, int height, int rowstride,
		  GdkPixbuf *pixbuf, double *affine)
{
	ArtPixBuf *apb;
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
		run_x2 = x + width;
		art_rgb_affine_run (&run_x1, &run_x2, yy + y,
				    apb->width, apb->height,
				    inv);

		d = dest + yy * rowstride + (run_x1 - x) * 4;

		for (xx = run_x1; xx < run_x2; xx++) {
			dest_p.x = xx + 0.5;
			art_affine_point (&src_p, &dest_p, inv);
			src_x = floor (src_p.x);
			src_y = floor (src_p.y);

			src = apb->pixels + src_y * apb->rowstride + src_x * apb->n_channels;

			for (i = 0; i < apb->n_channels; i++)
				*d++ = *src++;

			if (!apb->has_alpha)
				*d++ = 255; /* opaque */
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
	double i2c[6], render_affine[6];
	guchar *buf;
	GdkPixbuf *pixbuf;
	ArtIRect p_rect, a_rect, d_rect;
	int w, h;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (!priv->pixbuf)
		return;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (gcp, render_affine, i2c);

	/* Compute the area we need to repaint */

	p_rect.x0 = item->x1;
	p_rect.y0 = item->y1;
	p_rect.x1 = item->x2;
	p_rect.y1 = item->y2;

	a_rect.x0 = x;
	a_rect.y0 = y;
	a_rect.x1 = x + width;
	a_rect.y1 = y + height;

	art_irect_intersect (&d_rect, &p_rect, &a_rect);
	if (art_irect_empty (&d_rect))
		return;

	/* Create a temporary buffer and transform the pixbuf there */

	w = d_rect.x1 - d_rect.x0;
	h = d_rect.y1 - d_rect.y0;

	buf = g_new0 (guchar, w * h * 4);
	transform_pixbuf (buf,
			  d_rect.x0, d_rect.y0,
			  w, h,
			  w * 4,
			  priv->pixbuf, render_affine);

	pixbuf = gdk_pixbuf_new_from_data (buf, ART_PIX_RGB, TRUE, w, h, w * 4, NULL, NULL);

	gdk_pixbuf_render_to_drawable_alpha (pixbuf, drawable,
					     0, 0,
					     d_rect.x0 - x, d_rect.y0 - y,
					     w, h,
					     GDK_PIXBUF_ALPHA_BILEVEL,
					     128,
					     GDK_RGB_DITHER_MAX,
					     d_rect.x0, d_rect.y0);

	gdk_pixbuf_unref (pixbuf);
	g_free (buf);
}

/* Render handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	double i2c[6], render_affine[6];

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (!priv->pixbuf)
		return;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (gcp, render_affine, i2c);
        gnome_canvas_buf_ensure_buf (buf);

	art_rgb_pixbuf_affine (buf->buf,
			       buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1,
			       buf->buf_rowstride,
			       priv->pixbuf->art_pixbuf,
			       render_affine,
			       ART_FILTER_NEAREST, NULL);

	buf->is_bg = 0;
}



/* Point handler for the pixbuf canvas item */
static double
gnome_canvas_pixbuf_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
			   GnomeCanvasItem **actual_item)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	double i2c[6], render_affine[6], inv[6];
	ArtPoint c, p;
	int px, py;
	double no_hit;
	ArtPixBuf *apb;
	guchar *src;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	*actual_item = item;

	no_hit = item->canvas->pixels_per_unit * 2 + 10;

	if (!priv->pixbuf)
		return no_hit;

	apb = priv->pixbuf->art_pixbuf;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (gcp, render_affine, i2c);
	art_affine_invert (inv, render_affine);

	c.x = cx;
	c.y = cy;
	art_affine_point (&p, &c, inv);
	px = p.x;
	py = p.y;

	if (px < 0 || px >= apb->width || py < 0 || py >= apb->height)
		return no_hit;

	if (!apb->has_alpha)
		return 0.0;

	src = apb->pixels + py * apb->rowstride + px * apb->n_channels;

	if (src[3] < 128)
		return no_hit;
	else
		return 0.0;
}



/* Bounds handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	double i2c[6], viewport_affine[6];
	ArtDRect rect;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (!priv->pixbuf) {
		*x1 = *y1 = *x2 = *y2 = 0.0;
		return;
	}

	rect.x0 = 0.0;
	rect.x1 = priv->pixbuf->art_pixbuf->width;

	rect.y0 = 0.0;
	rect.y1 = priv->pixbuf->art_pixbuf->height;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_viewport_affine (gcp, viewport_affine, i2c);
	art_drect_affine_transform (&rect, &rect, viewport_affine);

	*x1 = rect.x0;
	*y1 = rect.y0;
	*x2 = rect.x1;
	*y2 = rect.y1;
}
