/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_DEBUG_H__
#define __GTK_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  GTK_DEBUG_OBJECTS = 1<<0,
  GTK_DEBUG_MISC = 1<<1
} GtkDebugFlag;

#ifdef G_ENABLE_DEBUG

#define GTK_NOTE(type,action)                G_STMT_START { \
    if (gtk_debug_flags & GTK_DEBUG_##type)                 \
       { action; };                          } G_STMT_END

#else /* !G_ENABLE_DEBUG */

#define GTK_NOTE(type, action)
      
#endif /* G_ENABLE_DEBUG */

extern guint gtk_debug_flags;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_DEBUG_H__ */
