/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmimeint.h: Internal defines and functions.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 * 
 * Copyright (C) 2003  Red Hat, Inc.
 * Copyright (C) 2003  Jonathan Blandford <jrb@alum.mit.edu>
 *
 * Licensed under the Academic Free License version 1.2
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __XDG_MIME_INT_H__
#define __XDG_MIME_INT_H__

/* FIXME: Should be configure check */
typedef unsigned int   xdg_unichar_t;
typedef unsigned char  xdg_uchar8_t;
typedef unsigned short xdg_uint16_t;
typedef unsigned int   xdg_uint32_t;


#define SWAP_BE16_TO_LE16(val) (xdg_uint16_t)(((xdg_uint16_t)(val) << 8)|((xdg_uint16_t)(val) >> 8))

#define SWAP_BE32_TO_LE32(val) (xdg_uint32_t)((((xdg_uint32_t)(val) & 0xFF000000U) >> 24) |	\
					      (((xdg_uint32_t)(val) & 0x00FF0000U) >> 8) |	\
					      (((xdg_uint32_t)(val) & 0x0000FF00U) << 8) |	\
					      (((xdg_uint32_t)(val) & 0x000000FFU) << 24))
/* UTF-8 utils
 */
const char *const utf8_skip;
#define _xdg_utf8_next_char(p) (char *)((p) + utf8_skip[*(unsigned char *)(p)])
#define _xdg_utf8_char_size(p) (int) (utf8_skip[*(unsigned char *)(p)])

xdg_unichar_t  _xdg_utf8_to_ucs4  (const char    *source);
xdg_unichar_t  _xdg_ucs4_to_upper (xdg_unichar_t  source);
int            _xdg_utf8_validate (const char    *source);
const char    *_xdg_get_base_name (const char    *file_name);

#endif /* __XDG_MIME_INT_H__ */
