/* GdkPixbuf library - GIF image loader
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
#include <glib.h>
#include <gif_lib.h>
#include <string.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"



#define MAXCOLORMAPSIZE  256

#define CM_RED           0
#define CM_GREEN         1
#define CM_BLUE          2

#define MAX_LWZ_BITS     12

#define INTERLACE          0x40
#define LOCALCOLORMAP      0x80
#define BitSet(byte, bit)  (((byte) & (bit)) == (bit))
#define ReadOK(file,buffer,len) (fread(buffer, len, 1, file) != 0)
#define LM_to_uint(a,b)         (((b)<<8)|(a))

#define GRAYSCALE        1
#define COLOR            2

typedef unsigned char CMap[3][MAXCOLORMAPSIZE];

typedef struct _Gif89 Gif89;
struct _Gif89
{
	int transparent;
	int delay_time;
	int input_flag;
	int disposal;
};

typedef struct _GifContext GifContext;
struct _GifContext
{
	unsigned int width;
	unsigned int height;
	CMap color_map;
	unsigned int bit_pixel;
	unsigned int color_resolution;
	unsigned int background;
	unsigned int aspect_ratio;
	int gray_scale;
	GdkPixbuf *pixbuf;
	Gif89 gif89;
};


int verbose = FALSE;
int showComment = TRUE;
char *globalcomment = NULL;
gint globalusecomment = TRUE;
static guchar   highest_used_index;

static int ReadColorMap (FILE *, int, CMap, int *);
static int DoExtension (FILE *file, GifContext *context, int label);
static int GetDataBlock (FILE *, unsigned char *);
static int GetCode (FILE *, int, int);
static int LWZReadByte (FILE *, int, int);
static gint32 ReadImage (FILE *, char *, int, int, CMap, int, int, int, int,
			 guint, guint, guint, guint);


static int
ReadColorMap (FILE *file,
	      int   number,
	      CMap  buffer,
	      int  *format)
{
	int i;
	unsigned char rgb[3];
	int flag;

	flag = TRUE;

	for (i = 0; i < number; ++i) {
		if (!ReadOK (file, rgb, sizeof (rgb))) {
			/*g_message (_("GIF: bad colormap\n"));*/
			return TRUE;
		}

		buffer[CM_RED][i] = rgb[0];
		buffer[CM_GREEN][i] = rgb[1];
		buffer[CM_BLUE][i] = rgb[2];

		flag &= (rgb[0] == rgb[1] && rgb[1] == rgb[2]);
	}

	*format = (flag) ? GRAYSCALE : COLOR;

	return FALSE;
}

static int
DoExtension (FILE *file, GifContext *context, int label)
{
	static guchar buf[256];

	switch (label) {
	case 0xf9:			/* Graphic Control Extension */
		(void) GetDataBlock (file, (unsigned char *) buf);
		context->gif89.disposal = (buf[0] >> 2) & 0x7;
		context->gif89.input_flag = (buf[0] >> 1) & 0x1;
		context->gif89.delay_time = LM_to_uint (buf[1], buf[2]);
		if ((buf[0] & 0x1) != 0)
			context->gif89.transparent = buf[3];
		else
			context->gif89.transparent = -1;
		while (GetDataBlock (file, (unsigned char *) buf) != 0)
			;
		return FALSE;
		break;
	default:
		/* Unhandled extension */
		break;
	}

	while (GetDataBlock (file, (unsigned char *) buf) != 0)
		;

	return FALSE;
}

int ZeroDataBlock = FALSE;

static int
GetDataBlock (FILE          *file,
	      unsigned char *buf)
{
	unsigned char count;

	if (!ReadOK (file, &count, 1)) {
		/*g_message (_("GIF: error in getting DataBlock size\n"));*/
		return -1;
	}

	ZeroDataBlock = count == 0;

	if ((count != 0) && (!ReadOK (file, buf, count))) {
		/*g_message (_("GIF: error in reading DataBlock\n"));*/
		return -1;
	}

	return count;
}

static int
GetCode (FILE *file,
	 int   code_size,
	 int   flag)
{
	static unsigned char buf[280];
	static int curbit, lastbit, done, last_byte;
	int i, j, ret;
	unsigned char count;

	if (flag) {
		curbit = 0;
		lastbit = 0;
		done = FALSE;
		return 0;
	}

	if ((curbit + code_size) >= lastbit){
		if (done) {
			if (curbit >= lastbit) {
				/*g_message (_("GIF: ran off the end of by bits\n"));*/
				return -1;
			}
			return -1;
		}
		buf[0] = buf[last_byte - 2];
		buf[1] = buf[last_byte - 1];

		if ((count = GetDataBlock (file, &buf[2])) == 0)
			done = TRUE;

		last_byte = 2 + count;
		curbit = (curbit - lastbit) + 16;
		lastbit = (2 + count) * 8;
	}

	ret = 0;
	for (i = curbit, j = 0; j < code_size; ++i, ++j)
		ret |= ((buf[i / 8] & (1 << (i % 8))) != 0) << j;

	curbit += code_size;

	return ret;
}

static int
LWZReadByte (FILE *file,
	     int   flag,
	     int   input_code_size)
{
	static int fresh = FALSE;
	int code, incode;
	static int code_size, set_code_size;
	static int max_code, max_code_size;
	static int firstcode, oldcode;
	static int clear_code, end_code;
	static int table[2][(1 << MAX_LWZ_BITS)];
	static int stack[(1 << (MAX_LWZ_BITS)) * 2], *sp;
	register int i;

	if (flag) {
		set_code_size = input_code_size;
		code_size = set_code_size + 1;
		clear_code = 1 << set_code_size;
		end_code = clear_code + 1;
		max_code_size = 2 * clear_code;
		max_code = clear_code + 2;

		GetCode (file, 0, TRUE);

		fresh = TRUE;

		for (i = 0; i < clear_code; ++i) {
			table[0][i] = 0;
			table[1][i] = i;
		}
		for (; i < (1 << MAX_LWZ_BITS); ++i)
			table[0][i] = table[1][0] = 0;

		sp = stack;

		return 0;
	} else if (fresh) {
		fresh = FALSE;
		do {
			firstcode = oldcode = GetCode (file, code_size, FALSE);
		} while (firstcode == clear_code);
		return firstcode;
	}

	if (sp > stack)
		return *--sp;

	while ((code = GetCode (file, code_size, FALSE)) >= 0) {
		if (code == clear_code) {
			for (i = 0; i < clear_code; ++i) {
				table[0][i] = 0;
				table[1][i] = i;
			}
			for (; i < (1 << MAX_LWZ_BITS); ++i)
				table[0][i] = table[1][i] = 0;
			code_size = set_code_size + 1;
			max_code_size = 2 * clear_code;
			max_code = clear_code + 2;
			sp = stack;
			firstcode = oldcode =
				GetCode (file, code_size, FALSE);
			return firstcode;
		} else if (code == end_code) {
			int count;
			unsigned char buf[260];

			if (ZeroDataBlock)
				return -2;

			while ((count = GetDataBlock (file, buf)) > 0)
				;

			if (count != 0)
				/*g_print (_("GIF: missing EOD in data stream (common occurence)"));*/
			return -2;
		}

		incode = code;

		if (code >= max_code) {
			*sp++ = firstcode;
			code = oldcode;
		}

		while (code >= clear_code) {
			*sp++ = table[1][code];
			if (code == table[0][code]) {
				/*g_message (_("GIF: circular table entry BIG ERROR\n"));*/
				/*gimp_quit ();*/
				return -1;
			}
			code = table[0][code];
		}

		*sp++ = firstcode = table[1][code];

		if ((code = max_code) < (1 << MAX_LWZ_BITS)) {
			table[0][code] = oldcode;
			table[1][code] = firstcode;
			++max_code;
			if ((max_code >= max_code_size) &&
			    (max_code_size < (1 << MAX_LWZ_BITS))) {
				max_code_size *= 2;
				++code_size;
			}
		}

		oldcode = incode;

		if (sp > stack)
			return *--sp;
	}
	return code;
}

#if 0
static gint32
ReadImage (FILE *file,
	   char *filename,
	   int   len,
	   int   height,
	   CMap  cmap,
	   int   ncols,
	   int   format,
	   int   interlace,
	   int   number,
	   guint   leftpos,
	   guint   toppos,
	   guint screenwidth,
	   guint screenheight)
{
	static gint32 image_ID;

	gint32 layer_ID;
	GPixelRgn pixel_rgn;
	GDrawable *drawable;
	guchar *dest, *temp;
	guchar c;
	gint xpos = 0, ypos = 0, pass = 0;
	gint cur_progress, max_progress;
	gint v;
	gint i, j;
	gboolean alpha_frame = FALSE;
	int nreturn_vals;
	static int previous_disposal;

	/*
	**  Initialize the Compression routines
	*/
	if (!ReadOK (file, &c, 1)) {
		/*g_message (_("GIF: EOF / read error on image data\n"));*/
		return -1;
	}

	if (LWZReadByte (file, TRUE, c) < 0) {
		/*g_message (_("GIF: error while reading\n"));*/
		return -1;
	}

	if (frame_number == 1 ) {
		image_ID = gimp_image_new (screenwidth, screenheight, INDEXED);
		gimp_image_set_filename (image_ID, filename);

		for (i = 0, j = 0; i < ncols; i++) {
			used_cmap[0][i] = gimp_cmap[j++] = cmap[0][i];
			used_cmap[1][i] = gimp_cmap[j++] = cmap[1][i];
			used_cmap[2][i] = gimp_cmap[j++] = cmap[2][i];
		}

		gimp_image_set_cmap (image_ID, gimp_cmap, ncols);

		if (Gif89.delayTime < 0)
			strcpy(framename, _("Background"));
		else
			sprintf(framename, _("Background (%dms)"), 10*Gif89.delayTime);

		previous_disposal = Gif89.disposal;

		if (Gif89.transparent == -1) {
			layer_ID = gimp_layer_new (image_ID, framename,
						   len, height,
						   INDEXED_IMAGE, 100, NORMAL_MODE);
		} else {
			layer_ID = gimp_layer_new (image_ID, framename,
						   len, height,
						   INDEXEDA_IMAGE, 100, NORMAL_MODE);
			alpha_frame=TRUE;
		}
	}
#if 0
	else {
		/* NOT FIRST FRAME */
		/* If the colourmap is now different, we have to promote to
		   RGB! */
		if (!promote_to_rgb) {
			for (i=0;i<ncols;i++) {
				if ((used_cmap[0][i] != cmap[0][i]) ||
				    (used_cmap[1][i] != cmap[1][i]) ||
				    (used_cmap[2][i] != cmap[2][i])) {
					/* Everything is RGB(A) from now on... sigh. */
					promote_to_rgb = TRUE;

					/* Promote everything we have so far into RGB(A) */
					gimp_run_procedure("gimp_convert_rgb", &nreturn_vals,
							   PARAM_IMAGE, image_ID,
							   PARAM_END);
					break;
				}
			}
		}

		if (Gif89.delayTime < 0)
			sprintf(framename, _("Frame %d"), frame_number);
		else
			sprintf(framename, _("Frame %d (%dms)"),
				frame_number, 10*Gif89.delayTime);

		switch (previous_disposal) {
		case 0x00: break; /* 'don't care' */
		case 0x01: strcat(framename,_(" (combine)")); break;
		case 0x02: strcat(framename,_(" (replace)")); break;
		case 0x03: strcat(framename,_(" (combine)")); break;
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			strcat(framename,_(" (unknown disposal)"));
			g_message (_("GIF: Hmm... please forward this GIF to the "
				     "GIF plugin author!\n  (adam@foxbox.org)\n"));
			break;
		default: g_message (_("GIF: Something got corrupted.\n")); break;
		}

		previous_disposal = Gif89.disposal;

		layer_ID = gimp_layer_new (image_ID, framename,
					   len, height,
					   promote_to_rgb ? RGBA_IMAGE : INDEXEDA_IMAGE,
					   100, NORMAL_MODE);
		alpha_frame = TRUE;
	}

	frame_number++;
#endif
	gimp_image_add_layer (image_ID, layer_ID, 0);
	gimp_layer_translate (layer_ID, (gint)leftpos, (gint)toppos);

	drawable = gimp_drawable_get (layer_ID);

	cur_progress = 0;
	max_progress = height;

	if (alpha_frame)
		dest = (guchar *) g_malloc (len * height *
					    (promote_to_rgb ? 4 : 2));
	else
		dest = (guchar *) g_malloc (len * height);

	if (verbose)
		g_print (_("GIF: reading %d by %d%s GIF image, ncols=%d\n"),
	     len, height, interlace ? _(" interlaced") : "", ncols);

	if (!alpha_frame && promote_to_rgb) {
		g_message (_("GIF: Ouchie!  Can't handle non-alpha RGB frames.\n     Please mail the plugin author.  (adam@gimp.org)\n"));
		exit(-1);
	}

	while ((v = LWZReadByte (file, FALSE, c)) >= 0) {
		if (alpha_frame) {
			if (((guchar)v > highest_used_index) && !(v == Gif89.transparent))
				highest_used_index = (guchar)v;
			if (promote_to_rgb) {
				temp = dest + ( (ypos * len) + xpos ) * 4;
				*(temp  ) = (guchar) cmap[0][v];
				*(temp+1) = (guchar) cmap[1][v];
				*(temp+2) = (guchar) cmap[2][v];
				*(temp+3) = (guchar) ((v == Gif89.transparent) ? 0 : 255);
			} else {
				temp = dest + ( (ypos * len) + xpos ) * 2;
				*temp = (guchar) v;
				*(temp+1) = (guchar) ((v == Gif89.transparent) ? 0 : 255);
			}
		} else {
			if ((guchar)v > highest_used_index)
				highest_used_index = (guchar)v;

			temp = dest + (ypos * len) + xpos;
			*temp = (guchar) v;
		}

		xpos++;
		if (xpos == len) {
			xpos = 0;
			if (interlace) {
				switch (pass) {
				case 0:
				case 1:
					ypos += 8;
					break;
				case 2:
					ypos += 4;
					break;
				case 3:
					ypos += 2;
					break;
				}

				if (ypos >= height) {
					pass++;
					switch (pass) {
					case 1:
						ypos = 4;
						break;
					case 2:
						ypos = 2;
						break;
					case 3:
						ypos = 1;
						break;
					default:
						goto fini;
					}
				}
			} else {
				ypos++;
			}

			if (run_mode != RUN_NONINTERACTIVE) {
				cur_progress++;
				if ((cur_progress % 16) == 0)
					gimp_progress_update ((double) cur_progress / (double) max_progress);
			}
		}
		if (ypos >= height)
			break;
	}

 fini:
	if (LWZReadByte (file, FALSE, c) >= 0)
		g_print (_("GIF: too much input data, ignoring extra...\n"));

	gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width, drawable->height, TRUE, FALSE);
	gimp_pixel_rgn_set_rect (&pixel_rgn, dest, 0, 0, drawable->width, drawable->height);

	g_free (dest);

	gimp_drawable_flush (drawable);
	gimp_drawable_detach (drawable);

	return image_ID;
}
#endif

/* Shared library entry point */
GdkPixbuf *
image_load (FILE *file)
{
	unsigned char buf[16];
	unsigned char c;
	CMap localColorMap;
	int useGlobalColormap;
	int bitPixel;
	int imageCount = 0;
	char version[4];
	GifContext *context;

	g_return_val_if_fail (file != NULL, NULL);

	context = g_new (GifContext, 1);

	if (!ReadOK (file, buf, 6)) {
		/* Unable to read magic number */
		return NULL;
	}

	if (strncmp ((char *) buf, "GIF", 3) != 0) {
		/* Not a GIF file */
		return NULL;
	}

	strncpy (version, (char *) buf + 3, 3);
	version[3] = '\0';

	if ((strcmp (version, "87a") != 0) && (strcmp (version, "89a") != 0)) {
		/* bad version number, not '87a' or '89a' */
		return NULL;
	}

	/* read the screen descriptor */
	if (!ReadOK (file, buf, 7)) {
		/* Failed to read screen descriptor */
		return NULL;
	}

	context->width = LM_to_uint (buf[0], buf[1]);
	context->height = LM_to_uint (buf[2], buf[3]);
	context->bit_pixel = 2 << (buf[4] & 0x07);
	context->color_resolution = (((buf[4] & 0x70) >> 3) + 1);
	context->background = buf[5];
	context->aspect_ratio = buf[6];
	context->pixbuf = NULL;

	if (BitSet (buf[4], LOCALCOLORMAP)) {
		/* Global Colormap */
		if (ReadColorMap (file, context->bit_pixel,
				  context->color_map,
				  &context->gray_scale)) {
			g_free (context);
			return NULL;
		}
	}

	if (context->aspect_ratio != 0 && context->aspect_ratio != 49) {
		/*g_message (_("GIF: warning - non-square pixels\n"));*/
	}

	highest_used_index = 0;

	for (;;) {
		if (!ReadOK (file, &c, 1)) {
			/*g_message (_("GIF: EOF / read error on image data\n"));*/
			return context->pixbuf;
		}

		if (c == ';') {
			/* GIF terminator */
			return context->pixbuf;
		}

		if (c == '!') {
			/* Extension */
			if (!ReadOK (file, &c, 1)) {
				/*g_message (_("GIF: EOF / read error on extension function code\n"));*/
				return context->pixbuf; /* will be -1 if failed on first image! */
			}
			DoExtension (file, context, c);
			continue;
		}

		if (c != ',') {
			/* Not a valid start character */
			/*g_message (_("GIF: bogus character 0x%02x, ignoring\n"), (int) c);*/
			continue;
		}

		++imageCount;

		if (!ReadOK (file, buf, 9)) {
			/*g_message (_("GIF: couldn't read left/top/width/height\n"));*/
			return context->pixbuf; /* will be -1 if failed on first image! */
		}

		useGlobalColormap = !BitSet (buf[8], LOCALCOLORMAP);

		bitPixel = 1 << ((buf[8] & 0x07) + 1);

#if 0
		if (!useGlobalColormap) {
			if (ReadColorMap (file, bitPixel, localColorMap, &grayScale)) {
				/*g_message (_("GIF: error reading local colormap\n"));*/
				return image_ID; /* will be -1 if failed on first image! */
			}
			image_ID = ReadImage (file, filename, LM_to_uint (buf[4], buf[5]),
					      LM_to_uint (buf[6], buf[7]),
					      localColorMap, bitPixel,
					      grayScale,
					      BitSet (buf[8], INTERLACE), imageCount,
					      (guint) LM_to_uint (buf[0], buf[1]),
					      (guint) LM_to_uint (buf[2], buf[3]),
					      GifScreen.Width,
					      GifScreen.Height);
		} else {
			image_ID = ReadImage (file, filename, LM_to_uint (buf[4], buf[5]),
					      LM_to_uint (buf[6], buf[7]),
					      GifScreen.ColorMap, GifScreen.BitPixel,
					      GifScreen.GrayScale,
					      BitSet (buf[8], INTERLACE), imageCount,
					      (guint) LM_to_uint (buf[0], buf[1]),
					      (guint) LM_to_uint (buf[2], buf[3]),
					      GifScreen.Width,
					      GifScreen.Height);
		}
#endif
	}

	return context->pixbuf;
}



