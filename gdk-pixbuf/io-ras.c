/* GdkPixbuf library - SUNRAS image loader
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Arjan van de Ven <arjan@fenrus.demon.nl>
 *          Federico Mena-Quintero <federico@gimp.org>
 *
 * Based on io-gif.c, io-tiff.c and io-png.c
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

/*

Known bugs:
	* Compressed rasterfiles don't work yet

*/

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"



/* 
   Header structure for sunras files.
   All values are in big-endian order on disk
   
   Note: Every scanline is padded to be a multiple of 16 bits
 */

struct rasterfile {
	guint magic;
	guint width;
	guint height;
	guint depth;
	guint length;
	guint type;
	guint maptype;
	guint maplength;
};

/* 
	This does a byte-order swap. Does glib have something like
	be32_to_cpu() ??
*/

/* Progressive loading */

struct ras_progressive_state {
	ModulePreparedNotifyFunc prepared_func;
	ModuleUpdatedNotifyFunc updated_func;
	gpointer user_data;

	gint HeaderSize;	/* The size of the header-part (incl colormap) */
	guchar *HeaderBuf;	/* The buffer for the header (incl colormap) */
	gint HeaderDone;	/* The nr of bytes actually in HeaderBuf */

	gint LineWidth;		/* The width of a line in bytes */
	guchar *LineBuf;	/* Buffer for 1 line */
	gint LineDone;		/* # of bytes in LineBuf */
	gint Lines;		/* # of finished lines */

	gint RasType;		/*  32 = BGRA
				   24 = BGR
				   8 = 8 bit colormapped
				   1  = 1 bit bitonal 
				 */


	struct rasterfile Header;	/* Decoded (BE->CPU) header */


	GdkPixbuf *pixbuf;	/* Our "target" */
};

gpointer
gdk_pixbuf__ras_image_begin_load(ModulePreparedNotifyFunc prepared_func,
				 ModuleUpdatedNotifyFunc updated_func,
				 ModuleFrameDoneNotifyFunc frame_done_func,
				 ModuleAnimationDoneNotifyFunc anim_done_func,
				 gpointer user_data);
void gdk_pixbuf__ras_image_stop_load(gpointer data);
gboolean gdk_pixbuf__ras_image_load_increment(gpointer data, guchar * buf, guint size);



/* Shared library entry point */
GdkPixbuf *gdk_pixbuf__ras_image_load(FILE * f)
{
	guchar *membuf;
	size_t length;
	struct ras_progressive_state *State;
	
	GdkPixbuf *pb;
	
	State = gdk_pixbuf__ras_image_begin_load(NULL, NULL, NULL, NULL, NULL);
	
	membuf = g_malloc(4096);
	
	g_assert(membuf != NULL);
	
	while (feof(f) == 0) {
		length = fread(membuf, 1, 4096, f);
		(void)gdk_pixbuf__ras_image_load_increment(State, membuf, length);
	} 
	g_free(membuf);
	if (State->pixbuf != NULL)
		gdk_pixbuf_ref(State->pixbuf);

	pb = State->pixbuf;

	gdk_pixbuf__ras_image_stop_load(State);
	return pb;
}

static void RAS2State(struct rasterfile *RAS,
		      struct ras_progressive_state *State)
{
	State->Header.width = GUINT32_FROM_BE(RAS->width);
	State->Header.height = GUINT32_FROM_BE(RAS->height);
	State->Header.depth = GUINT32_FROM_BE(RAS->depth);
	State->Header.type = GUINT32_FROM_BE(RAS->type);
	State->Header.maptype = GUINT32_FROM_BE(RAS->maptype);
	State->Header.maplength = GUINT32_FROM_BE(RAS->maplength);

	g_assert(State->Header.maplength <= 768);	/* Otherwise, we are in trouble */

	State->RasType = State->Header.depth;	/* This may be less trivial someday */
	State->HeaderSize = 32 + State->Header.maplength;

	if (State->RasType == 32)
		State->LineWidth = State->Header.width * 4;
	if (State->RasType == 24)
		State->LineWidth = State->Header.width * 3;
	if (State->RasType == 8)
		State->LineWidth = State->Header.width * 1;
	if (State->RasType == 1) {
		State->LineWidth = State->Header.width / 8;
		if ((State->Header.width & 7) != 0)
			State->LineWidth++;
	}

	/* Now padd the line to be a multiple of 16 bits */
	if ((State->LineWidth & 1) != 0)
		State->LineWidth++;

	if (State->LineBuf == NULL)
		State->LineBuf = g_malloc(State->LineWidth);

	g_assert(State->LineBuf != NULL);


	if (State->pixbuf == NULL) {
		if (State->RasType == 32)
			State->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE,
						       8,
						       (gint)
						       State->Header.width,
						       (gint)
						       State->Header.
						       height);
		else
			State->pixbuf =
			    gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
					   (gint) State->Header.width,
					   (gint) State->Header.height);
		if (State->prepared_func != NULL)
			/* Notify the client that we are ready to go */
			(*State->prepared_func) (State->pixbuf,
						 State->user_data);

	}
	
	if ((State->Header.maplength==0)&&(State->RasType==1)) {
		State->HeaderBuf[32] = 255;
		State->HeaderBuf[33] = 0;
		State->HeaderBuf[34] = 255;
		State->HeaderBuf[35] = 0;
		State->HeaderBuf[36] = 255;
		State->HeaderBuf[37] = 0;
	}

}

/* 
 * func - called when we have pixmap created (but no image data)
 * user_data - passed as arg 1 to func
 * return context (opaque to user)
 */

gpointer
gdk_pixbuf__ras_image_begin_load(ModulePreparedNotifyFunc prepared_func,
				 ModuleUpdatedNotifyFunc updated_func,
				 ModuleFrameDoneNotifyFunc frame_done_func,
				 ModuleAnimationDoneNotifyFunc anim_done_func,
				 gpointer user_data)
{
	struct ras_progressive_state *context;

	context = g_new0(struct ras_progressive_state, 1);
	context->prepared_func = prepared_func;
	context->updated_func = updated_func;
	context->user_data = user_data;

	context->HeaderSize = 32;
	context->HeaderBuf = g_malloc(32 + 768);	/* 32 for rasheader,
							   768 for the colormap */
	context->HeaderDone = 0;

	context->LineWidth = 0;
	context->LineBuf = NULL;
	context->LineDone = 0;
	context->Lines = 0;

	context->RasType = 0;

	memset(&context->Header, 0, sizeof(struct rasterfile));


	context->pixbuf = NULL;


	return (gpointer) context;
}

/*
 * context - returned from image_begin_load
 *
 * free context, unref gdk_pixbuf
 */
void
gdk_pixbuf__ras_image_stop_load(gpointer data)
{
	struct ras_progressive_state *context =
	    (struct ras_progressive_state *) data;


	g_return_if_fail(context != NULL);

	if (context->LineBuf != NULL)
		g_free(context->LineBuf);
	if (context->HeaderBuf != NULL)
		g_free(context->HeaderBuf);

	if (context->pixbuf)
		gdk_pixbuf_unref(context->pixbuf);

	g_free(context);
}

/* 
 OneLine is called when enough data is received to process 1 line 
 of pixels 
 */

static void OneLine32(struct ras_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	Pixels = context->pixbuf->pixels + context->pixbuf->rowstride * context->Lines;
	while (X < context->Header.width) {
		/* The joys of having a BGR byteorder */
		Pixels[X * 4 + 0] = context->LineBuf[X * 4 + 2];
		Pixels[X * 4 + 1] = context->LineBuf[X * 4 + 1];
		Pixels[X * 4 + 2] = context->LineBuf[X * 4 + 0];
		Pixels[X * 4 + 3] = context->LineBuf[X * 4 + 3];
		X++;
	}
}

static void OneLine24(struct ras_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	Pixels = context->pixbuf->pixels + context->pixbuf->rowstride * context->Lines;
	while (X < context->Header.width) {
		/* The joys of having a BGR byteorder */
		Pixels[X * 3 + 0] = context->LineBuf[X * 3 + 2];
		Pixels[X * 3 + 1] = context->LineBuf[X * 3 + 1];
		Pixels[X * 3 + 2] = context->LineBuf[X * 3 + 0];
		X++;
	}

}

static void OneLine8(struct ras_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	Pixels = context->pixbuf->pixels + context->pixbuf->rowstride * context->Lines;
	while (X < context->Header.width) {
		/* The joys of having a BGR byteorder */
		Pixels[X * 3 + 0] =
		    context->HeaderBuf[context->LineBuf[X] + 32];
		Pixels[X * 3 + 1] =
		    context->HeaderBuf[context->LineBuf[X] + 256 + 32];
		Pixels[X * 3 + 2] =
		    context->HeaderBuf[context->LineBuf[X] + 512 + 32];
		X++;
	}
}

static void OneLine1(struct ras_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	Pixels = context->pixbuf->pixels + context->pixbuf->rowstride * context->Lines;
	while (X < context->Header.width) {
		int Bit;
		
		Bit = (context->LineBuf[X/8])>>(7-(X&7));
		Bit = Bit & 1;
		/* The joys of having a BGR byteorder */
		Pixels[X * 3 + 0] =
		    context->HeaderBuf[Bit + 32];
		Pixels[X * 3 + 1] =
		    context->HeaderBuf[Bit + 2 + 32];
		Pixels[X * 3 + 2] =
		    context->HeaderBuf[Bit + 4 + 32];
		X++;
	}
}


static void OneLine(struct ras_progressive_state *context)
{
	if (context->RasType == 32)
		OneLine32(context);
	if (context->RasType == 24)
		OneLine24(context);
	if (context->RasType == 8)
		OneLine8(context);
	if (context->RasType == 1)
		OneLine1(context);

	context->LineDone = 0;
	if (context->Lines > context->Header.height)
		return;
	context->Lines++;

	if (context->updated_func != NULL) {
		(*context->updated_func) (context->pixbuf,
					  0,
					  context->Lines,
					  context->Header.width,
					  context->Header.height,
					  context->user_data);

	}
}

/*
 * context - from image_begin_load
 * buf - new image data
 * size - length of new image data
 *
 * append image data onto inrecrementally built output image
 */
gboolean
gdk_pixbuf__ras_image_load_increment(gpointer data, guchar * buf, guint size)
{
	struct ras_progressive_state *context =
	    (struct ras_progressive_state *) data;

	gint BytesToCopy;

	while (size > 0) {
		if (context->HeaderDone < context->HeaderSize) {	/* We still 
									   have headerbytes to do */
			BytesToCopy =
			    context->HeaderSize - context->HeaderDone;
			if (BytesToCopy > size)
				BytesToCopy = size;

			memmove(context->HeaderBuf + context->HeaderDone,
			       buf, BytesToCopy);

			size -= BytesToCopy;
			buf += BytesToCopy;
			context->HeaderDone += BytesToCopy;

		} else {
			/* Pixeldata only */
			BytesToCopy =
			    context->LineWidth - context->LineDone;
			if (BytesToCopy > size)
				BytesToCopy = size;

			if (BytesToCopy > 0) {
				memmove(context->LineBuf +
				       context->LineDone, buf,
				       BytesToCopy);

				size -= BytesToCopy;
				buf += BytesToCopy;
				context->LineDone += BytesToCopy;
			}
			if ((context->LineDone >= context->LineWidth) &&
			    (context->LineWidth > 0))
				OneLine(context);


		}

		if (context->HeaderDone >= 32)
			RAS2State((struct rasterfile *) context->HeaderBuf,
				  context);


	}

	return TRUE;
}
