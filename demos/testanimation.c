
/* testpixbuf -- test program for gdk-pixbuf code
 * Copyright (C) 1999 Mark Crichton, Larry Ewing
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gdk-pixbuf-loader.h>

typedef struct {
	FILE             *imagefile;
	GdkPixbufLoader  *loader;
	GtkWidget        **rgbwin;
	guchar           *buf;
	guint            timeout;
	guint            readlen;

} ProgressFileStatus;


#define DEFAULT_WIDTH  24
#define DEFAULT_HEIGHT 24

static const unsigned char default_image[] = {
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xae, 0xb3, 0xb3, 0xc6, 0xc9, 0xcd, 0xd7, 0xd4, 0xdf,
	0xec, 0xde, 0xf3, 0xe7, 0xcb, 0xe9, 0xd9, 0xb5, 0xd3, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0xb1, 0xb7, 0xa5,
	0xb0, 0xb8, 0xad, 0xb3, 0xb9, 0xb6, 0xc1, 0xc6, 0xc8, 0xd5, 0xd3, 0xdc,
	0xec, 0xde, 0xf3, 0xe5, 0xca, 0xe6, 0xe0, 0xbb, 0xd7, 0xe1, 0xad, 0xc2,
	0xe3, 0xac, 0xa3, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0xca, 0xc1, 0xa4, 0xc5, 0xc7, 0xac,
	0xb7, 0xbe, 0xaf, 0xad, 0xb4, 0xaf, 0xbd, 0xc2, 0xc3, 0xd1, 0xd0, 0xd8,
	0xec, 0xde, 0xf3, 0xe5, 0xc7, 0xe4, 0xe0, 0xb6, 0xd1, 0xe7, 0xa9, 0xb4,
	0xed, 0xcd, 0xb6, 0xd6, 0xcf, 0xae, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0x00, 0x00, 0x00, 0xdf, 0xa7, 0x9f, 0xdd, 0xbf, 0xaa, 0xcf, 0xc5, 0xa9,
	0xc1, 0xc4, 0xac, 0xb2, 0xba, 0xaf, 0xb6, 0xbb, 0xbb, 0xcd, 0xce, 0xd4,
	0xec, 0xde, 0xf3, 0xe4, 0xc4, 0xe1, 0xe0, 0xaf, 0xc7, 0xea, 0xbc, 0xae,
	0xe1, 0xd6, 0xb6, 0xc7, 0xcc, 0xae, 0xa2, 0xab, 0x9a, 0x00, 0x00, 0x00,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0x00, 0x00, 0x00, 0xe3, 0xab, 0xc0, 0xe6, 0xa3, 0xa7, 0xdf, 0xba, 0xa8,
	0xcf, 0xc5, 0xa9, 0xbd, 0xc2, 0xae, 0xad, 0xb4, 0xaf, 0xc6, 0xc9, 0xcd,
	0xec, 0xde, 0xf3, 0xe2, 0xbf, 0xdc, 0xe7, 0xa9, 0xb4, 0xe7, 0xd6, 0xb8,
	0xc7, 0xcc, 0xae, 0xac, 0xb6, 0xa6, 0x9d, 0xa8, 0x9f, 0x00, 0x00, 0x00,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
	0xd9, 0xaf, 0xcf, 0xe1, 0xb4, 0xd2, 0xe2, 0xb0, 0xcb, 0xe4, 0xa9, 0xbb,
	0xe2, 0xb2, 0xa6, 0xcf, 0xc5, 0xa9, 0x6a, 0x6a, 0x6a, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x6a, 0x6a, 0x6a, 0xed, 0xcd, 0xb6, 0xc7, 0xcc, 0xae,
	0xa6, 0xb1, 0xa3, 0x98, 0xa2, 0x9c, 0x8f, 0x97, 0x96, 0x7e, 0x84, 0x85,
	0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
	0xe8, 0xc6, 0xe7, 0xe5, 0xc2, 0xe3, 0xe3, 0xbd, 0xdd, 0xe1, 0xb6, 0xd5,
	0xe2, 0xb0, 0xcb, 0x6a, 0x6a, 0x6a, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x6a, 0x6a, 0x6a, 0x9d, 0xa8, 0x9f,
	0x8f, 0x97, 0x96, 0x8b, 0x90, 0x92, 0x97, 0x9e, 0xa2, 0xa0, 0xa7, 0xae,
	0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
	0xe7, 0xd3, 0xed, 0xe8, 0xd1, 0xed, 0xe8, 0xce, 0xec, 0xe9, 0xcc, 0xeb,
	0xe8, 0xc6, 0xe7, 0x0d, 0x0d, 0x0d, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x0d, 0x0d, 0x0d, 0x97, 0x9e, 0xa2,
	0xa7, 0xae, 0xb7, 0xb2, 0xb6, 0xc5, 0xba, 0xbc, 0xce, 0xbf, 0xbe, 0xd3,
	0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
	0xe9, 0xdf, 0xf0, 0xe9, 0xdf, 0xf0, 0xe9, 0xdf, 0xf0, 0xe9, 0xdf, 0xf0,
	0xe9, 0xdf, 0xf0, 0x0d, 0x0d, 0x0d, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x0d, 0x0d, 0x0d, 0xe1, 0xd2, 0xf7,
	0xe1, 0xd2, 0xf7, 0xe1, 0xd2, 0xf7, 0xe1, 0xd2, 0xf7, 0xe1, 0xd2, 0xf7,
	0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
	0xca, 0xc7, 0xd2, 0xc5, 0xc4, 0xcd, 0xbf, 0xbf, 0xc7, 0xb8, 0xb9, 0xc0,
	0xae, 0xaf, 0xb6, 0x6a, 0x6a, 0x6a, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x6a, 0x6a, 0x6a, 0xd5, 0xa8, 0xe1,
	0xd8, 0xb2, 0xe9, 0xd9, 0xb8, 0xed, 0xdb, 0xbd, 0xf0, 0xdc, 0xbf, 0xf1,
	0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
	0xa4, 0xa6, 0xac, 0xa8, 0xaa, 0xaf, 0xa0, 0xa6, 0xa8, 0x98, 0x9e, 0x9c,
	0xa1, 0xa8, 0x9e, 0xb1, 0xb6, 0xa1, 0x6a, 0x6a, 0x6a, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x0d, 0x6a, 0x6a, 0x6a, 0xc0, 0x8c, 0xad, 0xcc, 0x90, 0xb5,
	0xd3, 0x94, 0xca, 0xd6, 0xa2, 0xdb, 0xd5, 0xa8, 0xe1, 0xcf, 0xa7, 0xdf,
	0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0x00, 0x00, 0x00, 0x98, 0x9f, 0x9b, 0xa1, 0xa8, 0x9e, 0xac, 0xb3, 0xa0,
	0xb9, 0xb9, 0xa4, 0xd0, 0xb8, 0xa8, 0xc5, 0xb5, 0xb8, 0xb6, 0xbb, 0xad,
	0xe3, 0xd7, 0xb5, 0xdd, 0xb4, 0xa9, 0xcb, 0x89, 0xac, 0xc0, 0x8c, 0xad,
	0xc8, 0x91, 0xb5, 0xd1, 0x8d, 0xb7, 0xd3, 0x94, 0xca, 0x00, 0x00, 0x00,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0x00, 0x00, 0x00, 0xa1, 0xa7, 0x98, 0xb1, 0xb6, 0xa1, 0xbd, 0xb9, 0xa5,
	0xd0, 0xb8, 0xa8, 0xca, 0xb5, 0xb7, 0xb8, 0xb1, 0xb1, 0xc2, 0xc8, 0xb2,
	0xe3, 0xd7, 0xb5, 0xe1, 0xbf, 0xaf, 0xdb, 0x92, 0x9a, 0xbe, 0x82, 0xa6,
	0xc0, 0x8c, 0xad, 0xc8, 0x91, 0xb4, 0xc7, 0x8b, 0xb0, 0x00, 0x00, 0x00,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0xbc, 0xb6, 0xa1, 0xd0, 0xb8, 0xa8,
	0xcd, 0xb6, 0xb7, 0xc0, 0xb4, 0xb5, 0xb1, 0xb1, 0xaa, 0xca, 0xd1, 0xb4,
	0xe3, 0xd7, 0xb5, 0xe2, 0xc1, 0xb0, 0xdb, 0xa8, 0xa3, 0xd2, 0x8a, 0xa9,
	0xb7, 0x7e, 0xa2, 0xbd, 0x89, 0xa9, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0xc9, 0xaf, 0xaf,
	0xc5, 0xb5, 0xb8, 0xb8, 0xb1, 0xb1, 0xb6, 0xbb, 0xad, 0xd0, 0xd6, 0xb5,
	0xe3, 0xd7, 0xb5, 0xe2, 0xbf, 0xaf, 0xdd, 0xb4, 0xa9, 0xdb, 0x92, 0x9a,
	0xc6, 0x84, 0xa7, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xac, 0xaa, 0xa6, 0xbd, 0xc3, 0xb0, 0xd2, 0xd7, 0xb5,
	0xe3, 0xd7, 0xb5, 0xe2, 0xbf, 0xae, 0xdb, 0xb6, 0xa8, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff
};

static void
quit_func (GtkWidget *widget, gpointer dummy)
{
	gtk_main_quit ();
}

static void
expose_func (GtkWidget *drawing_area, GdkEventExpose *event, gpointer data)
{
	GdkPixbuf *pixbuf;

	pixbuf = (GdkPixbuf *)gtk_object_get_data(GTK_OBJECT(drawing_area), "pixbuf");

	if (gdk_pixbuf_get_has_alpha (pixbuf)) {
		gdk_draw_rgb_32_image (drawing_area->window,
				       drawing_area->style->black_gc,
				       event->area.x, event->area.y, 
				       event->area.width, 
				       event->area.height,
				       GDK_RGB_DITHER_MAX, 
				       gdk_pixbuf_get_pixels (pixbuf) 
				       + (event->area.y * gdk_pixbuf_get_rowstride (pixbuf)) 
				       + (event->area.x * gdk_pixbuf_get_n_channels (pixbuf)),
				       gdk_pixbuf_get_rowstride (pixbuf));
	} else {
		gdk_draw_rgb_image (drawing_area->window,
				    drawing_area->style->white_gc,
				    event->area.x, event->area.y, 
				    event->area.width, 
				    event->area.height,
				    GDK_RGB_DITHER_NORMAL,
				    gdk_pixbuf_get_pixels (pixbuf)
				    + (event->area.y * gdk_pixbuf_get_rowstride (pixbuf))
				    + (event->area.x * gdk_pixbuf_get_n_channels (pixbuf)),
				    gdk_pixbuf_get_rowstride (pixbuf));
	}
}

static void
config_func (GtkWidget *drawing_area, GdkEventConfigure *event, gpointer data)
{
	GdkPixbuf *pixbuf;
    
	pixbuf = (GdkPixbuf *)gtk_object_get_data(GTK_OBJECT(drawing_area), "pixbuf");

#if 0
	if (((event->width) != gdk_pixbuf_get_width (pixbuf)) ||
	    ((event->height) != gdk_pixbuf_get_height (pixbuf)))
		gdk_pixbuf_scale(pixbuf, event->width, event->height);
#endif
}

static GtkWidget*
new_testrgb_window (GdkPixbuf *pixbuf, gchar *title)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *temp_box;
	GtkWidget *button;
	GtkWidget *drawing_area;
	gint w, h;
 
	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);

	window = gtk_widget_new (gtk_window_get_type (),
				 "GtkObject::user_data", NULL,
				 "GtkWindow::type", GTK_WINDOW_TOPLEVEL,
				 "GtkWindow::title", "testrgb",
				 "GtkWindow::allow_shrink", TRUE,
				 NULL);
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			    (GtkSignalFunc) quit_func, NULL);

	vbox = gtk_vbox_new (FALSE, 0);

	if (title)
		gtk_box_pack_start (GTK_BOX (vbox), gtk_label_new (title),
				    TRUE, TRUE, 0);

	drawing_area = gtk_drawing_area_new ();

	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_drawing_area_size (GTK_DRAWING_AREA(drawing_area), w, h);
	gtk_box_pack_start (GTK_BOX (temp_box), drawing_area, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), temp_box, FALSE, FALSE, 0);
	

	gtk_signal_connect (GTK_OBJECT(drawing_area), "expose_event",
			    GTK_SIGNAL_FUNC(expose_func), NULL);
	gtk_signal_connect (GTK_OBJECT(drawing_area), "configure_event",
			    GTK_SIGNAL_FUNC (config_func), NULL);

	gtk_object_set_data (GTK_OBJECT(drawing_area), "pixbuf", pixbuf);

	gtk_widget_show (drawing_area);

	button = gtk_button_new_with_label ("Quit");
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				   (GtkSignalFunc) gtk_widget_destroy,
				   GTK_OBJECT (window));

	gtk_widget_show (button);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show_all (vbox);

	gtk_widget_show (window);

        return window;
}

#if 0

static gint
update_timeout(gpointer data)
{
        ProgressFileStatus *status = data;
	gboolean done;

	done = TRUE;
	if (!feof(status->imagefile)) {
		gint nbytes;

		nbytes = fread(status->buf, 1, status->readlen, 
			       status->imagefile);

		done = !gdk_pixbuf_loader_write (GDK_PIXBUF_LOADER (status->loader), status->buf, nbytes);
			
	}

	if (done) {
                gtk_widget_queue_draw(*status->rgbwin);
		gdk_pixbuf_loader_close (GDK_PIXBUF_LOADER (status->loader));
		gtk_object_destroy (GTK_OBJECT(status->loader));
		fclose (status->imagefile);
		g_free (status->buf);
	}

	return !done;
}


static void
progressive_prepared_callback(GdkPixbufLoader* loader, gpointer data)
{
        GtkWidget** retloc = data;
        GdkPixbuf* pixbuf;

        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        g_assert(pixbuf != NULL);

        gdk_pixbuf_ref(pixbuf); /* for the RGB window */

        *retloc = new_testrgb_window(pixbuf, "Progressive");

        return;
}


static void
progressive_updated_callback(GdkPixbufLoader* loader, guint x, guint y, guint width, guint height, gpointer data)
{
        GtkWidget** window_loc = data;

/*  	g_print ("progressive_updated_callback:\n\t%d\t%d\t%d\t%d\n", x, y, width, height); */

        if (*window_loc != NULL)
                gtk_widget_queue_draw_area(*window_loc,
					   x, y, width, height);

        return;
}

#endif

static int readlen = 4096;

extern void pixbuf_init();

int
main (int argc, char **argv)
{
	int i;
	int found_valid = FALSE;

	GdkPixbufAnimation *animation;

	pixbuf_init ();

	gtk_init (&argc, &argv);

	gdk_rgb_set_verbose (TRUE);

	gdk_rgb_init ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());

	{
		char *tbf_readlen = getenv("TBF_READLEN");
		if(tbf_readlen) readlen = atoi(tbf_readlen);
	}

	{
		char *tbf_bps = getenv("TBF_KBPS");
		guint bps;

		if (tbf_bps) {
			bps = atoi(tbf_bps);
			g_print ("Simulating %d kBytes/sec\n", bps);
			readlen = (bps*1024)/10;
		}
	}

	i = 1;
	if (argc == 1) {
		g_print ("USAGE:  testanimation FILE1 ...\n");
		return 0;
	} else {
		for (i = 1; i < argc; i++) {
                        GError *error;

                        error = NULL;
			animation = gdk_pixbuf_animation_new_from_file (argv[i],
                                                                        &error);

                        if (animation == NULL) {
                                g_warning ("Failed to load animation: %s",
                                           error->message);
                                g_error_free (error);
                        }
                        
			if (animation) {
				gint i = 0;
				GList *listptr;
				for (listptr = gdk_pixbuf_animation_get_frames (animation);
				     listptr;
				     listptr = listptr->next) {
					GdkPixbufFrame *frame;
					GdkPixbuf *pixbuf;
					gchar *title;

					frame = listptr->data;
					pixbuf = gdk_pixbuf_frame_get_pixbuf (frame);

					title = g_strdup_printf ("Frame %d", i);
					g_print ("Frame %d  x:%d y:%d width:%d height:%d\n",
						 i,
						 gdk_pixbuf_frame_get_x_offset (frame),
						 gdk_pixbuf_frame_get_y_offset (frame),
						 gdk_pixbuf_get_width (pixbuf),
						 gdk_pixbuf_get_height (pixbuf));
					new_testrgb_window (pixbuf, title);
					g_free (title);
					i++;
				}
				found_valid = TRUE;
			}
		}
#if 0
                {
                        GtkWidget* rgb_window = NULL;
			ProgressFileStatus   status;
			GdkPixbufLoader *pixbuf_loader;

                        pixbuf_loader = gdk_pixbuf_loader_new ();
			status.loader = pixbuf_loader;

			status.rgbwin = &rgb_window;

			status.buf = g_malloc (readlen);
                        gtk_signal_connect(GTK_OBJECT(pixbuf_loader),
                                           "area_prepared",
                                           GTK_SIGNAL_FUNC(progressive_prepared_callback),
                                           &rgb_window);

                        gtk_signal_connect(GTK_OBJECT(pixbuf_loader),
                                           "area_updated",
                                           GTK_SIGNAL_FUNC(progressive_updated_callback),
                                           &rgb_window);

			
                        status.imagefile = fopen (argv[1], "r");
                        g_assert (status.imagefile != NULL);

			status.readlen = readlen;

                        status.timeout = gtk_timeout_add(100, update_timeout, &status);
                }
#endif
	}

	if (found_valid)
		gtk_main ();

	return 0;
}
