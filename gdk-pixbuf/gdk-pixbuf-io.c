/* GdkPixbuf library - Main loading interface.
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
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
#include <string.h>
#include <gmodule.h>
#include "gdk-pixbuf.h"



static gboolean
pixbuf_check_png (guchar *buffer, int size)
{
	if (size < 28)
		return FALSE;

	if (buffer [0] != 0x89 ||
	    buffer [1] != 'P' ||
	    buffer [2] != 'N' ||
	    buffer [3] != 'G' ||
	    buffer [4] != 0x0d ||
	    buffer [5] != 0x0a ||
	    buffer [6] != 0x1a ||
	    buffer [7] != 0x0a)
		return FALSE;

	return TRUE;
}

static gboolean
pixbuf_check_jpeg (guchar *buffer, int size)
{
	if (size < 10)
		return FALSE;

	if (buffer [0] != 0xff || buffer [1] != 0xd8)
		return FALSE;

	return TRUE;
}

static gboolean
pixbuf_check_tiff (guchar *buffer, int size)
{
	if (size < 10)
		return FALSE;

	if (buffer [0] == 'M' &&
	    buffer [1] == 'M' &&
	    buffer [2] == 0   &&
	    buffer [3] == 0x2a)
		return TRUE;

	if (buffer [0] == 'I' &&
	    buffer [1] == 'I' &&
	    buffer [2] == 0x2a &&
	    buffer [3] == 0)
		return TRUE;

	return FALSE;
}

static gboolean
pixbuf_check_gif (guchar *buffer, int size)
{
	if (size < 20)
		return FALSE;

	if (strncmp (buffer, "GIF8", 4) == 0)
		return TRUE;

	return FALSE;
}

static gboolean
pixbuf_check_xpm (guchar *buffer, int size)
{
	if (size < 20)
		return FALSE;

	if (strncmp (buffer, "/* XPM */", 9) == 0)
		return TRUE;

	return FALSE;
}

static gboolean
pixbuf_check_bmp (guchar *buffer, int size)
{
	if (size < 20)
		return FALSE;

	if (buffer [0] != 'B' || buffer [1] != 'M')
		return FALSE;

	return TRUE;
}

static gboolean
pixbuf_check_ppm (guchar *buffer, int size)
{
	if (size < 20)
		return FALSE;

	if (buffer [0] == 'P') {
		if (buffer [1] == '1' ||
		    buffer [1] == '2' ||
		    buffer [1] == '3' ||
		    buffer [1] == '4' ||
		    buffer [1] == '5' ||
		    buffer [1] == '6')
			return TRUE;
	}
	return FALSE;
}

static struct {
	char *module_name;
	gboolean (* format_check) (guchar *buffer, int size);
	GModule *module;
	GdkPixbuf *(* load) (FILE *f);
} file_formats [] = {
	{ "png",  pixbuf_check_png,  NULL, NULL },
	{ "jpeg", pixbuf_check_jpeg, NULL, NULL },
	{ "tiff", pixbuf_check_tiff, NULL, NULL },
	{ "gif",  pixbuf_check_gif,  NULL, NULL },
	{ "xpm",  pixbuf_check_xpm,  NULL, NULL },
#if 0
	{ "bmp",  pixbuf_check_bmp,  NULL, NULL },
	{ "ppm",  pixbuf_check_ppm,  NULL, NULL },
#endif
	{ NULL, NULL, NULL, NULL }
};

static void
image_handler_load (int idx)
{
	char *module_name;
	char *path;
	GModule *module;
	void *load_sym;

	module_name = g_strconcat ("pixbuf-", file_formats [idx].module_name, NULL);
	path = g_module_build_path (PIXBUF_LIBDIR, module_name);
	g_free (module_name);

	module = g_module_open (path, G_MODULE_BIND_LAZY);
	if (!module) {
		g_warning ("Unable to load module: %s", path);
		return;
	}

	file_formats [idx].module = module;

	if (g_module_symbol (module, "image_load", &load_sym))
		file_formats [idx].load = load_sym;
}



GdkPixbuf *
gdk_pixbuf_new_from_file (const char *filename)
{
	GdkPixbuf *pixbuf;
	gint n, i;
	FILE *f;
	char buffer [128];

	f = fopen (filename, "r");
	if (!f)
		return NULL;

	n = fread (&buffer, 1, sizeof (buffer), f);

	if (n == 0) {
		fclose (f);
		return NULL;
	}

	for (i = 0; file_formats [i].module_name; i++) {
		if ((* file_formats [i].format_check) (buffer, n)) {
			if (!file_formats [i].load)
				image_handler_load (i);

			if (!file_formats [i].load) {
				fclose (f);
				return NULL;
			}

			fseek (f, 0, SEEK_SET);
			pixbuf = (* file_formats [i].load) (f);
			fclose (f);

			if (pixbuf)
				g_assert (pixbuf->ref_count != 0);

			return pixbuf;
		}
	}

	fclose (f);
	g_warning ("Unable to find handler for file: %s", filename);
	return NULL;
}
