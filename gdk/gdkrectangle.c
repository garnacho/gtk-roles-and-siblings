/* GDK - The GIMP Drawing Kit
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
#include "gdk.h"


gint
gdk_rectangle_intersect (GdkRectangle *src1,
			 GdkRectangle *src2,
			 GdkRectangle *dest)
{
  GdkRectangle *temp;
  gint src1_x2, src1_y2;
  gint src2_x2, src2_y2;
  gint return_val;

  g_return_val_if_fail (src1 != NULL, FALSE);
  g_return_val_if_fail (src2 != NULL, FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);

  return_val = FALSE;

  if (src2->x < src1->x)
    {
      temp = src1;
      src1 = src2;
      src2 = temp;
    }
  dest->x = src2->x;

  src1_x2 = src1->x + src1->width;
  src2_x2 = src2->x + src2->width;

  if (src2->x < src1_x2)
    {
      if (src1_x2 < src2_x2)
	dest->width = src1_x2 - dest->x;
      else
	dest->width = src2_x2 - dest->x;

      if (src2->y < src1->y)
	{
	  temp = src1;
	  src1 = src2;
	  src2 = temp;
	}
      dest->y = src2->y;

      src1_y2 = src1->y + src1->height;
      src2_y2 = src2->y + src2->height;

      if (src2->y < src1_y2)
	{
	  return_val = TRUE;

	  if (src1_y2 < src2_y2)
	    dest->height = src1_y2 - dest->y;
	  else
	    dest->height = src2_y2 - dest->y;

	  if (dest->height == 0)
	    return_val = FALSE;
	  if (dest->width == 0)
	    return_val = FALSE;
	}
    }

  return return_val;
}
