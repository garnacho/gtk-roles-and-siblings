/* GTK+ Pixbuf Engine
 * Copyright (C) 1998-2000 Red Hat, Inc.
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
 *
 * Written by Owen Taylor <otaylor@redhat.com>, based on code by
 * Carsten Haitzler <raster@rasterman.com>
 */

#include <math.h>
#include <string.h>

#include "pixbuf.h"
#include "pixbuf-rc-style.h"
#include "pixbuf-style.h"

static void pixbuf_style_init       (PixbufStyle      *style);
static void pixbuf_style_class_init (PixbufStyleClass *klass);

GtkStyleClass *parent_class;

static ThemeImage *
match_theme_image (GtkStyle       *style,
		   ThemeMatchData *match_data)
{
  GList *tmp_list;

  tmp_list = PIXBUF_RC_STYLE (style->rc_style)->img_list;
  
  while (tmp_list)
    {
      guint flags;
      ThemeImage *image = tmp_list->data;
      tmp_list = tmp_list->next;

      if (match_data->function != image->match_data.function)
	continue;

      flags = match_data->flags & image->match_data.flags;
      
      if (flags != image->match_data.flags) /* Required components not present */
	continue;

      if ((flags & THEME_MATCH_STATE) &&
	  match_data->state != image->match_data.state)
	continue;

      if ((flags & THEME_MATCH_SHADOW) &&
	  match_data->shadow != image->match_data.shadow)
	continue;
      
      if ((flags & THEME_MATCH_ARROW_DIRECTION) &&
	  match_data->arrow_direction != image->match_data.arrow_direction)
	continue;

      if ((flags & THEME_MATCH_ORIENTATION) &&
	  match_data->orientation != image->match_data.orientation)
	continue;

      if ((flags & THEME_MATCH_GAP_SIDE) &&
	  match_data->gap_side != image->match_data.gap_side)
	continue;

      if (image->match_data.detail &&
	  (!image->match_data.detail ||
	   strcmp (match_data->detail, image->match_data.detail) != 0))
      continue;

      return image;
    }
  
  return NULL;
}

static gboolean
draw_simple_image(GtkStyle       *style,
		  GdkWindow      *window,
		  GdkRectangle   *area,
		  GtkWidget      *widget,
		  ThemeMatchData *match_data,
		  gboolean        draw_center,
		  gboolean        allow_setbg,
		  gint            x,
		  gint            y,
		  gint            width,
		  gint            height)
{
  ThemeImage *image;
  gboolean setbg = FALSE;
  
  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      if (allow_setbg)
      	setbg = TRUE;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  if (!(match_data->flags & THEME_MATCH_ORIENTATION))
    {
      match_data->flags |= THEME_MATCH_ORIENTATION;
      
      if (height > width)
	match_data->orientation = GTK_ORIENTATION_VERTICAL;
      else
	match_data->orientation = GTK_ORIENTATION_HORIZONTAL;
    }
    
  image = match_theme_image (style, match_data);
  if (image)
    {
      if (image->background)
	{
	  GdkBitmap *mask = NULL;

	  if (image->background->stretch && setbg &&
	      !GDK_IS_PIXMAP (window))
	    {
	      GdkPixbuf *pixbuf = theme_pixbuf_get_pixbuf (image->background);
	      if (pixbuf && gdk_pixbuf_get_has_alpha (pixbuf))
		mask = gdk_pixmap_new (window, width, height, 1);
	    }
	  
	  theme_pixbuf_render (image->background,
			       window, mask, area,
			       draw_center ? COMPONENT_ALL : COMPONENT_ALL | COMPONENT_CENTER,
			       FALSE,
			       x, y, width, height);
	  
	  if (mask)
	    {
	      gdk_window_shape_combine_mask (window, mask, 0, 0);
	      gdk_pixmap_unref (mask);
	    }
	}
      
      if (image->overlay && draw_center)
	theme_pixbuf_render (image->overlay,
			     window, NULL, area, COMPONENT_ALL,
			     TRUE, 
			     x, y, width, height);

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
draw_gap_image(GtkStyle       *style,
	       GdkWindow      *window,
	       GdkRectangle   *area,
	       GtkWidget      *widget,
	       ThemeMatchData *match_data,
	       gboolean        draw_center,
	       gint            x,
	       gint            y,
	       gint            width,
	       gint            height,
	       GtkPositionType gap_side,
	       gint            gap_x,
	       gint            gap_width)
{
  ThemeImage *image;
  gboolean setbg = FALSE;
  
  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = TRUE;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  if (!(match_data->flags & THEME_MATCH_ORIENTATION))
    {
      match_data->flags |= THEME_MATCH_ORIENTATION;
      
      if (height > width)
	match_data->orientation = GTK_ORIENTATION_VERTICAL;
      else
	match_data->orientation = GTK_ORIENTATION_HORIZONTAL;
    }

  match_data->flags |= THEME_MATCH_GAP_SIDE;
  match_data->gap_side = gap_side;
    
  image = match_theme_image (style, match_data);
  if (image)
    {
      gint thickness;
      GdkRectangle r1, r2, r3;
      GdkPixbuf *pixbuf = NULL;
      guint components = COMPONENT_ALL;

      if (!draw_center)
	components |= COMPONENT_CENTER;

      if (image->gap_start)
	pixbuf = theme_pixbuf_get_pixbuf (image->gap_start);

      switch (gap_side)
	{
	case GTK_POS_TOP:
	  if (pixbuf)
	    thickness = gdk_pixbuf_get_height (pixbuf);
	  else
	    thickness = style->ythickness;
	  
	  if (!draw_center)
	    components |= COMPONENT_NORTH_WEST | COMPONENT_NORTH | COMPONENT_NORTH_EAST;

	  r1.x      = x;
	  r1.y      = y;
	  r1.width  = gap_x;
	  r1.height = thickness;
	  r2.x      = x + gap_x;
	  r2.y      = y;
	  r2.width  = gap_width;
	  r2.height = thickness;
	  r3.x      = x + gap_x + gap_width;
	  r3.y      = y;
	  r3.width  = width - (gap_x + gap_width);
	  r3.height = thickness;
	  break;
	  
	case GTK_POS_BOTTOM:
	  if (pixbuf)
	    thickness = gdk_pixbuf_get_height (pixbuf);
	  else
	    thickness = style->ythickness;

	  if (!draw_center)
	    components |= COMPONENT_SOUTH_WEST | COMPONENT_SOUTH | COMPONENT_SOUTH_EAST;

	  r1.x      = x;
	  r1.y      = y + height - thickness;
	  r1.width  = gap_x;
	  r1.height = thickness;
	  r2.x      = x + gap_x;
	  r2.y      = y + height - thickness;
	  r2.width  = gap_width;
	  r2.height = thickness;
	  r3.x      = x + gap_x + gap_width;
	  r3.y      = y + height - thickness;
	  r3.width  = width - (gap_x + gap_width);
	  r3.height = thickness;
	  break;
	  
	case GTK_POS_LEFT:
	  if (pixbuf)
	    thickness = gdk_pixbuf_get_width (pixbuf);
	  else
	    thickness = style->xthickness;

	  if (!draw_center)
	    components |= COMPONENT_NORTH_WEST | COMPONENT_WEST | COMPONENT_SOUTH_WEST;

	  r1.x      = x;
	  r1.y      = y;
	  r1.width  = thickness;
	  r1.height = gap_x;
	  r2.x      = x;
	  r2.y      = y + gap_x;
	  r2.width  = thickness;
	  r2.height = gap_width;
	  r3.x      = x;
	  r3.y      = y + gap_x + gap_width;
	  r3.width  = thickness;
	  r3.height = height - (gap_x + gap_width);
	  break;
	  
	case GTK_POS_RIGHT:
	  if (pixbuf)
	    thickness = gdk_pixbuf_get_width (pixbuf);
	  else
	    thickness = style->xthickness;

	  if (!draw_center)
	    components |= COMPONENT_NORTH_EAST | COMPONENT_EAST | COMPONENT_SOUTH_EAST;

	  r1.x      = x + width - thickness;
	  r1.y      = y;
	  r1.width  = thickness;
	  r1.height = gap_x;
	  r2.x      = x + width - thickness;
	  r2.y      = y + gap_x;
	  r2.width  = thickness;
	  r2.height = gap_width;
	  r3.x      = x + width - thickness;
	  r3.y      = y + gap_x + gap_width;
	  r3.width  = thickness;
	  r3.height = height - (gap_x + gap_width);
	  break;
	}

      if (image->background)
	theme_pixbuf_render (image->background,
			     window, NULL, area, components, FALSE,
			     x, y, width, height);
      if (image->gap_start)
	theme_pixbuf_render (image->gap_start,
			     window, NULL, area, COMPONENT_ALL, FALSE,
			     r1.x, r1.y, r1.width, r1.height);
      if (image->gap)
	theme_pixbuf_render (image->gap,
			     window, NULL, area, COMPONENT_ALL, FALSE,
			     r2.x, r2.y, r2.width, r2.height);
      if (image->gap_end)
	theme_pixbuf_render (image->gap_end,
			     window, NULL, area, COMPONENT_ALL, FALSE,
			     r3.x, r3.y, r3.width, r3.height);

      return TRUE;
    }
  else
    return FALSE;
}

static void
draw_hline (GtkStyle     *style,
	    GdkWindow    *window,
	    GtkStateType  state,
	    GdkRectangle *area,
	    GtkWidget    *widget,
	    const gchar  *detail,
	    gint          x1,
	    gint          x2,
	    gint          y)
{
  ThemeImage *image;
  ThemeMatchData   match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_HLINE;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_ORIENTATION | THEME_MATCH_STATE;
  match_data.state = state;
  match_data.orientation = GTK_ORIENTATION_HORIZONTAL;
  
  image = match_theme_image (style, &match_data);
  if (image)
    {
      if (image->background)
	theme_pixbuf_render (image->background,
			     window, NULL, area, COMPONENT_ALL, FALSE,
			     x1, y, (x2 - x1) + 1, 2);
    }
  else
    parent_class->draw_hline (style, window, state, area, widget, detail,
			      x1, x2, y);
}

static void
draw_vline (GtkStyle     *style,
	    GdkWindow    *window,
	    GtkStateType  state,
	    GdkRectangle *area,
	    GtkWidget    *widget,
	    const gchar  *detail,
	    gint          y1,
	    gint          y2,
	    gint          x)
{
  ThemeImage    *image;
  ThemeMatchData match_data;
  
  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  match_data.function = TOKEN_D_VLINE;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_ORIENTATION | THEME_MATCH_STATE;
  match_data.state = state;
  match_data.orientation = GTK_ORIENTATION_VERTICAL;
  
  image = match_theme_image (style, &match_data);
  if (image)
    {
      if (image->background)
	theme_pixbuf_render (image->background,
			     window, NULL, area, COMPONENT_ALL, FALSE,
			     x, y1, 2, (y2 - y1) + 1);
    }
  else
    parent_class->draw_vline (style, window, state, area, widget, detail,
			      y1, y2, x);
}

static void
draw_shadow(GtkStyle     *style,
	    GdkWindow    *window,
	    GtkStateType  state,
	    GtkShadowType shadow,
	    GdkRectangle *area,
	    GtkWidget    *widget,
	    const gchar  *detail,
	    gint          x,
	    gint          y,
	    gint          width,
	    gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_SHADOW;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;

  if (!draw_simple_image (style, window, area, widget, &match_data, FALSE, FALSE,
			  x, y, width, height))
    parent_class->draw_shadow (style, window, state, shadow, area, widget, detail,
			       x, y, width, height);
}

static void
draw_polygon(GtkStyle * style,
	     GdkWindow * window,
	     GtkStateType state,
	     GtkShadowType shadow,
	     GdkRectangle * area,
	     GtkWidget * widget,
	     const gchar *detail,
	     GdkPoint * points,
	     gint npoints,
	     gint fill)
{
#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif /* M_PI */
#ifndef M_PI_4
#define M_PI_4  0.78539816339744830962
#endif /* M_PI_4 */

  static const gdouble pi_over_4 = M_PI_4;
  static const gdouble pi_3_over_4 = M_PI_4 * 3;

  GdkGC              *gc3;
  GdkGC              *gc4;
  gdouble             angle;
  gint                i;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);
  g_return_if_fail(points != NULL);

  switch (shadow)
    {
    case GTK_SHADOW_IN:
      gc3 = style->light_gc[state];
      gc4 = style->black_gc;
      break;
    case GTK_SHADOW_OUT:
      gc3 = style->black_gc;
      gc4 = style->light_gc[state];
      break;
    default:
      return;
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle(gc3, area);
      gdk_gc_set_clip_rectangle(gc4, area);
    }
  if (fill)
    gdk_draw_polygon(window, style->bg_gc[state], TRUE, points, npoints);

  npoints--;

  for (i = 0; i < npoints; i++)
    {
      if ((points[i].x == points[i + 1].x) &&
	  (points[i].y == points[i + 1].y))
	angle = 0;
      else
	angle = atan2(points[i + 1].y - points[i].y,
		      points[i + 1].x - points[i].x);

      if ((angle > -pi_3_over_4) && (angle < pi_over_4))
	gdk_draw_line(window, gc3,
		      points[i].x, points[i].y,
		      points[i + 1].x, points[i + 1].y);
      else
	gdk_draw_line(window, gc4,
		      points[i].x, points[i].y,
		      points[i + 1].x, points[i + 1].y);
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(gc3, NULL);
      gdk_gc_set_clip_rectangle(gc4, NULL);
    }
}

static void
draw_arrow (GtkStyle     *style,
	    GdkWindow    *window,
	    GtkStateType  state,
	    GtkShadowType shadow,
	    GdkRectangle *area,
	    GtkWidget    *widget,
	    const gchar  *detail,
	    GtkArrowType  arrow_direction,
	    gint          fill,
	    gint          x,
	    gint          y,
	    gint          width,
	    gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_ARROW;
  match_data.detail = (gchar *)detail;
  match_data.flags = (THEME_MATCH_SHADOW | 
		      THEME_MATCH_STATE | 
		      THEME_MATCH_ARROW_DIRECTION);
  match_data.shadow = shadow;
  match_data.state = state;
  match_data.arrow_direction = arrow_direction;
  
  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_arrow (style, window, state, shadow, area, widget, detail,
			      arrow_direction, fill, x, y, width, height);
}

static void
draw_diamond (GtkStyle     *style,
	      GdkWindow    *window,
	      GtkStateType  state,
	      GtkShadowType shadow,
	      GdkRectangle *area,
	      GtkWidget    *widget,
	      const gchar  *detail,
	      gint          x,
	      gint          y,
	      gint          width,
	      gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_DIAMOND;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;
  
  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_diamond (style, window, state, shadow, area, widget, detail,
				x, y, width, height);
}

static void
draw_oval (GtkStyle     *style,
	   GdkWindow    *window,
	   GtkStateType  state,
	   GtkShadowType shadow,
	   GdkRectangle *area,
	   GtkWidget    *widget,
	   const gchar  *detail,
	   gint          x,
	   gint          y,
	   gint          width,
	   gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_OVAL;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;

  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_oval (style, window, state, shadow, area, widget, detail,
			     x, y, width, height);
}

static void
draw_string (GtkStyle * style,
	     GdkWindow * window,
	     GtkStateType state,
	     GdkRectangle * area,
	     GtkWidget * widget,
	     const gchar *detail,
	     gint x,
	     gint y,
	     const gchar * string)
{
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if (state == GTK_STATE_INSENSITIVE)
    {
      if (area)
	{
	  gdk_gc_set_clip_rectangle(style->white_gc, area);
	  gdk_gc_set_clip_rectangle(style->fg_gc[state], area);
	}

      gdk_draw_string(window, style->font, style->fg_gc[state], x, y, string);
      
      if (area)
	{
	  gdk_gc_set_clip_rectangle(style->white_gc, NULL);
	  gdk_gc_set_clip_rectangle(style->fg_gc[state], NULL);
	}
    }
  else
    {
      gdk_gc_set_clip_rectangle(style->fg_gc[state], area);
      gdk_draw_string(window, style->font, style->fg_gc[state], x, y, string);
      gdk_gc_set_clip_rectangle(style->fg_gc[state], NULL);
    }
}

static void
draw_box (GtkStyle     *style,
	  GdkWindow    *window,
 	  GtkStateType  state,
 	  GtkShadowType shadow,
 	  GdkRectangle *area,
 	  GtkWidget    *widget,
	  const gchar  *detail,
	  gint          x,
	  gint          y,
	  gint          width,
	  gint          height)
{
  ThemeMatchData match_data;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_BOX;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;

  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_box (style, window, state, shadow, area, widget, detail,
			    x, y, width, height);
}

static void
draw_flat_box (GtkStyle     *style,
	       GdkWindow    *window,
	       GtkStateType  state,
	       GtkShadowType shadow,
	       GdkRectangle *area,
	       GtkWidget    *widget,
	       const gchar  *detail,
	       gint          x,
	       gint          y,
	       gint          width,
	       gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_FLAT_BOX;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;

  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_flat_box (style, window, state, shadow, area, widget, detail,
				 x, y, width, height);
}

static void
draw_check (GtkStyle     *style,
	    GdkWindow    *window,
	    GtkStateType  state,
	    GtkShadowType shadow,
	    GdkRectangle *area,
	    GtkWidget    *widget,
	    const gchar  *detail,
	    gint          x,
	    gint          y,
	    gint          width,
	    gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_CHECK;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;
  
  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_check (style, window, state, shadow, area, widget, detail,
			      x, y, width, height);
}

static void
draw_option (GtkStyle      *style,
	     GdkWindow     *window,
	     GtkStateType  state,
	     GtkShadowType shadow,
	     GdkRectangle *area,
	     GtkWidget    *widget,
	     const gchar  *detail,
	     gint          x,
	     gint          y,
	     gint          width,
	     gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_OPTION;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;
  
  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_option (style, window, state, shadow, area, widget, detail,
			       x, y, width, height);
}

static void
draw_cross (GtkStyle     *style,
	    GdkWindow    *window,
	    GtkStateType  state,
	    GtkShadowType shadow,
	    GdkRectangle *area,
	    GtkWidget    *widget,
	    const gchar  *detail,
	    gint          x,
	    gint          y,
	    gint          width,
	    gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_CROSS;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;
  
  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_cross (style, window, state, shadow, area, widget, detail,
			      x, y, width, height);
}

static void
draw_ramp (GtkStyle     *style,
	   GdkWindow    *window,
	   GtkStateType  state,
	   GtkShadowType shadow,
	   GdkRectangle *area,
	   GtkWidget    *widget,
	   const gchar  *detail,
	   GtkArrowType  arrow_direction,
	   gint          x,
	   gint          y,
	   gint          width,
	   gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_RAMP;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;
  
  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_ramp (style, window, state, shadow, area, widget, detail,
			     arrow_direction, x, y, width, height);
}

static void
draw_tab (GtkStyle     *style,
	  GdkWindow    *window,
	  GtkStateType  state,
	  GtkShadowType shadow,
	  GdkRectangle *area,
	  GtkWidget    *widget,
	  const gchar  *detail,
	  gint          x,
	  gint          y,
	  gint          width,
	  gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_TAB;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.shadow = shadow;
  match_data.state = state;
  
  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_tab (style, window, state, shadow, area, widget, detail,
			    x, y, width, height);
}

static void
draw_shadow_gap (GtkStyle       *style,
		 GdkWindow      *window,
		 GtkStateType    state,
		 GtkShadowType   shadow,
		 GdkRectangle   *area,
		 GtkWidget      *widget,
		 const gchar    *detail,
		 gint            x,
		 gint            y,
		 gint            width,
		 gint            height,
		 GtkPositionType gap_side,
		 gint            gap_x,
		 gint            gap_width)
{
  ThemeMatchData match_data;
  
  match_data.function = TOKEN_D_SHADOW_GAP;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.flags = (THEME_MATCH_SHADOW | 
		      THEME_MATCH_STATE | 
		      THEME_MATCH_ORIENTATION);
  match_data.shadow = shadow;
  match_data.state = state;
  
  if (!draw_gap_image (style, window, area, widget, &match_data, FALSE,
		       x, y, width, height, gap_side, gap_x, gap_width))
    parent_class->draw_shadow_gap (style, window, state, shadow, area, widget, detail,
				   x, y, width, height, gap_side, gap_x, gap_width);
}

static void
draw_box_gap (GtkStyle       *style,
	      GdkWindow      *window,
	      GtkStateType    state,
	      GtkShadowType   shadow,
	      GdkRectangle   *area,
	      GtkWidget      *widget,
	      const gchar    *detail,
	      gint            x,
	      gint            y,
	      gint            width,
	      gint            height,
	      GtkPositionType gap_side,
	      gint            gap_x,
	      gint            gap_width)
{
  ThemeMatchData match_data;
  
  match_data.function = TOKEN_D_BOX_GAP;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE;
  match_data.flags = (THEME_MATCH_SHADOW | 
		      THEME_MATCH_STATE | 
		      THEME_MATCH_ORIENTATION);
  match_data.shadow = shadow;
  match_data.state = state;
  
  if (!draw_gap_image (style, window, area, widget, &match_data, TRUE,
		       x, y, width, height, gap_side, gap_x, gap_width))
    parent_class->draw_box_gap (style, window, state, shadow, area, widget, detail,
				x, y, width, height, gap_side, gap_x, gap_width);
}

static void
draw_extension (GtkStyle       *style,
		GdkWindow      *window,
		GtkStateType    state,
		GtkShadowType   shadow,
		GdkRectangle   *area,
		GtkWidget      *widget,
		const gchar    *detail,
		gint            x,
		gint            y,
		gint            width,
		gint            height,
		GtkPositionType gap_side)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  /* Why? */
  if (width >=0)
    width++;
  if (height >=0)
    height++;
  
  match_data.function = TOKEN_D_EXTENSION;
  match_data.detail = (gchar *)detail;
  match_data.flags = THEME_MATCH_SHADOW | THEME_MATCH_STATE | THEME_MATCH_GAP_SIDE;
  match_data.shadow = shadow;
  match_data.state = state;
  match_data.gap_side = gap_side;

  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_extension (style, window, state, shadow, area, widget, detail,
				  x, y, width, height, gap_side);
}

static void
draw_focus (GtkStyle     *style,
	    GdkWindow    *window,
	    GdkRectangle *area,
	    GtkWidget    *widget,
	    const gchar  *detail,
	    gint          x,
	    gint          y,
	    gint          width,
	    gint          height)
{
  ThemeMatchData match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  /* Why? */
  if (width >=0)
    width++;
  if (height >=0)
    height++;

  match_data.function = TOKEN_D_FOCUS;
  match_data.detail = (gchar *)detail;
  match_data.flags = 0;
  
  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, FALSE,
			  x, y, width, height))
    parent_class->draw_focus (style, window, area, widget, detail,
			      x, y, width, height);
}

static void
draw_slider (GtkStyle      *style,
	     GdkWindow     *window,
	     GtkStateType   state,
	     GtkShadowType  shadow,
	     GdkRectangle  *area,
	     GtkWidget     *widget,
	     const gchar   *detail,
	     gint           x,
	     gint           y,
	     gint           width,
	     gint           height,
	     GtkOrientation orientation)
{
  ThemeMatchData           match_data;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  match_data.function = TOKEN_D_SLIDER;
  match_data.detail = (gchar *)detail;
  match_data.flags = (THEME_MATCH_SHADOW | 
		      THEME_MATCH_STATE | 
		      THEME_MATCH_ORIENTATION);
  match_data.shadow = shadow;
  match_data.state = state;
  match_data.orientation = orientation;

  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_slider (style, window, state, shadow, area, widget, detail,
			       x, y, width, height, orientation);
}


static void
draw_handle (GtkStyle      *style,
	     GdkWindow     *window,
	     GtkStateType   state,
	     GtkShadowType  shadow,
	     GdkRectangle  *area,
	     GtkWidget     *widget,
	     const gchar   *detail,
	     gint           x,
	     gint           y,
	     gint           width,
	     gint           height,
	     GtkOrientation orientation)
{
  ThemeMatchData match_data;
  
  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  match_data.function = TOKEN_D_HANDLE;
  match_data.detail = (gchar *)detail;
  match_data.flags = (THEME_MATCH_SHADOW | 
		      THEME_MATCH_STATE | 
		      THEME_MATCH_ORIENTATION);
  match_data.shadow = shadow;
  match_data.state = state;
  match_data.orientation = orientation;

  if (!draw_simple_image (style, window, area, widget, &match_data, TRUE, TRUE,
			  x, y, width, height))
    parent_class->draw_handle (style, window, state, shadow, area, widget, detail,
			       x, y, width, height, orientation);
}

GType pixbuf_type_style = 0;

void
pixbuf_style_register_type (GTypeModule *module)
{
  static const GTypeInfo object_info =
  {
    sizeof (PixbufStyleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) pixbuf_style_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (PixbufStyle),
    0,              /* n_preallocs */
    (GInstanceInitFunc) pixbuf_style_init,
  };
  
  pixbuf_type_style = g_type_module_register_type (module,
						   GTK_TYPE_STYLE,
						   "PixbufStyle",
						   &object_info, 0);
}

static void
pixbuf_style_init (PixbufStyle *style)
{
}

static void
pixbuf_style_class_init (PixbufStyleClass *klass)
{
  GtkStyleClass *style_class = GTK_STYLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  style_class->draw_hline = draw_hline;
  style_class->draw_vline = draw_vline;
  style_class->draw_shadow = draw_shadow;
  style_class->draw_polygon = draw_polygon;
  style_class->draw_arrow = draw_arrow;
  style_class->draw_diamond = draw_diamond;
  style_class->draw_oval = draw_oval;
  style_class->draw_string = draw_string;
  style_class->draw_box = draw_box;
  style_class->draw_flat_box = draw_flat_box;
  style_class->draw_check = draw_check;
  style_class->draw_option = draw_option;
  style_class->draw_cross = draw_cross;
  style_class->draw_ramp = draw_ramp;
  style_class->draw_tab = draw_tab;
  style_class->draw_shadow_gap = draw_shadow_gap;
  style_class->draw_box_gap = draw_box_gap;
  style_class->draw_extension = draw_extension;
  style_class->draw_focus = draw_focus;
  style_class->draw_slider = draw_slider;
  style_class->draw_handle = draw_handle;
}
