/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <time.h>

#include "gdkcolor.h"
#include "gdkinternals.h"
#include "gdkprivate-x11.h"

typedef struct _GdkColormapPrivateX11  GdkColormapPrivateX11;

struct _GdkColormapPrivateX11
{
  Colormap xcolormap;
  Display *xdisplay;
  gint private_val;

  GHashTable *hash;
  GdkColorInfo *info;
  time_t last_sync_time;
};

#define GDK_COLORMAP_PRIVATE_DATA(cmap) ((GdkColormapPrivateX11 *) GDK_COLORMAP (cmap)->windowing_data)

static gint     gdk_colormap_match_color (GdkColormap *cmap,
					  GdkColor    *color,
					  const gchar *available);
static void     gdk_colormap_add         (GdkColormap *cmap);
static void     gdk_colormap_remove      (GdkColormap *cmap);
static guint    gdk_colormap_hash        (Colormap    *cmap);
static gboolean gdk_colormap_equal       (Colormap    *a,
					  Colormap    *b);
static void     gdk_colormap_sync        (GdkColormap *colormap,
                                          gboolean     force);

static void gdk_colormap_init       (GdkColormap      *colormap);
static void gdk_colormap_class_init (GdkColormapClass *klass);
static void gdk_colormap_finalize   (GObject              *object);

static gpointer parent_class = NULL;

static GHashTable *colormap_hash = NULL;

GType
gdk_colormap_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkColormapClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_colormap_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkColormap),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_colormap_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkColormap",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_colormap_init (GdkColormap *colormap)
{
  GdkColormapPrivateX11 *private;

  private = g_new (GdkColormapPrivateX11, 1);

  colormap->windowing_data = private;
  
  private->xdisplay = gdk_display;
  private->hash = NULL;
  private->last_sync_time = 0;
  private->info = NULL;

  colormap->size = 0;
  colormap->colors = NULL;
}

static void
gdk_colormap_class_init (GdkColormapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_colormap_finalize;
}

static void
gdk_colormap_finalize (GObject *object)
{
  GdkColormap *colormap = GDK_COLORMAP (object);
  GdkColormapPrivateX11 *private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  gdk_colormap_remove (colormap);

  XFreeColormap (private->xdisplay, private->xcolormap);

  if (private->hash)
    g_hash_table_destroy (private->hash);
  
  g_free (private->info);
  g_free (colormap->colors);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GdkColormap*
gdk_colormap_new (GdkVisual *visual,
		  gboolean   private_cmap)
{
  GdkColormap *colormap;
  GdkColormapPrivateX11 *private;
  Visual *xvisual;
  int size;
  int i;

  /* FIXME when object properties settle down, there needs to be some
   * kind of default construction (and construct-only arguments)
   */
  
  g_return_val_if_fail (visual != NULL, NULL);

  colormap = g_object_new (gdk_colormap_get_type (), NULL);
  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  colormap->visual = visual;
  
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;

  colormap->size = visual->colormap_size;

  switch (visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      private->info = g_new0 (GdkColorInfo, colormap->size);
      colormap->colors = g_new (GdkColor, colormap->size);
      
      private->hash = g_hash_table_new ((GHashFunc) gdk_color_hash,
					(GEqualFunc) gdk_color_equal);
      
      private->private_val = private_cmap;
      private->xcolormap = XCreateColormap (private->xdisplay, _gdk_root_window,
					    xvisual, (private_cmap) ? (AllocAll) : (AllocNone));

      if (private_cmap)
	{
	  XColor *default_colors;

	  default_colors = g_new (XColor, colormap->size);

	  for (i = 0; i < colormap->size; i++)
	    default_colors[i].pixel = i;

	  XQueryColors (private->xdisplay,
			DefaultColormap (private->xdisplay, _gdk_screen),
			default_colors, colormap->size);

	  for (i = 0; i < colormap->size; i++)
	    {
	      colormap->colors[i].pixel = default_colors[i].pixel;
	      colormap->colors[i].red = default_colors[i].red;
	      colormap->colors[i].green = default_colors[i].green;
	      colormap->colors[i].blue = default_colors[i].blue;
	    }

	  gdk_colormap_change (colormap, colormap->size);
	  
	  g_free (default_colors);
	}
      break;

    case GDK_VISUAL_DIRECT_COLOR:
      private->private_val = TRUE;
      private->xcolormap = XCreateColormap (private->xdisplay, _gdk_root_window,
					    xvisual, AllocAll);
      colormap->colors = g_new (GdkColor, colormap->size);

      size = 1 << visual->red_prec;
      for (i = 0; i < size; i++)
	colormap->colors[i].red = i * 65535 / (size - 1);

      size = 1 << visual->green_prec;
      for (i = 0; i < size; i++)
	colormap->colors[i].green = i * 65535 / (size - 1);

      size = 1 << visual->blue_prec;
      for (i = 0; i < size; i++)
	colormap->colors[i].blue = i * 65535 / (size - 1);

      gdk_colormap_change (colormap, colormap->size);
      break;

    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
      private->private_val = FALSE;
      private->xcolormap = XCreateColormap (private->xdisplay, _gdk_root_window,
					    xvisual, AllocNone);
      
      colormap->colors = g_new (GdkColor, colormap->size);
      gdk_colormap_sync (colormap, TRUE);
      break;
      
    case GDK_VISUAL_TRUE_COLOR:
      private->private_val = FALSE;
      private->xcolormap = XCreateColormap (private->xdisplay, _gdk_root_window,
					    xvisual, AllocNone);
      break;
    }

  gdk_colormap_add (colormap);

  return colormap;
}

#define MIN_SYNC_TIME 2

static void
gdk_colormap_sync (GdkColormap *colormap,
		   gboolean     force)
{
  time_t current_time;
  GdkColormapPrivateX11 *private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  XColor *xpalette;
  gint nlookup;
  gint i;
  
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  current_time = time (NULL);
  if (!force && ((current_time - private->last_sync_time) < MIN_SYNC_TIME))
    return;

  private->last_sync_time = current_time;

  nlookup = 0;
  xpalette = g_new (XColor, colormap->size);
  
  for (i = 0; i < colormap->size; i++)
    {
      if (!private->info || private->info[i].ref_count == 0)
	{
	  xpalette[nlookup].pixel = i;
	  xpalette[nlookup].red = 0;
	  xpalette[nlookup].green = 0;
	  xpalette[nlookup].blue = 0;
	  nlookup++;
	}
    }
  
  XQueryColors (gdk_display, private->xcolormap, xpalette, nlookup);
  
  for (i = 0; i < nlookup; i++)
    {
      gulong pixel = xpalette[i].pixel;
      colormap->colors[pixel].pixel = pixel;
      colormap->colors[pixel].red = xpalette[i].red;
      colormap->colors[pixel].green = xpalette[i].green;
      colormap->colors[pixel].blue = xpalette[i].blue;
    }
  
  g_free (xpalette);
}
		   

GdkColormap*
gdk_colormap_get_system (void)
{
  static GdkColormap *colormap = NULL;
  GdkColormapPrivateX11 *private;

  if (!colormap)
    {
      colormap = g_object_new (gdk_colormap_get_type (), NULL);
      private = GDK_COLORMAP_PRIVATE_DATA (colormap);

      private->xdisplay = gdk_display;
      private->xcolormap = DefaultColormap (gdk_display, _gdk_screen);
      colormap->visual = gdk_visual_get_system ();
      private->private_val = FALSE;

      private->hash = NULL;
      private->last_sync_time = 0;
      private->info = NULL;

      colormap->colors = NULL;
      colormap->size = colormap->visual->colormap_size;

      switch (colormap->visual->type)
	{
	case GDK_VISUAL_GRAYSCALE:
	case GDK_VISUAL_PSEUDO_COLOR:
	  private->info = g_new0 (GdkColorInfo, colormap->size);
	  private->hash = g_hash_table_new ((GHashFunc) gdk_color_hash,
					    (GEqualFunc) gdk_color_equal);
	  /* Fall through */
	case GDK_VISUAL_STATIC_GRAY:
	case GDK_VISUAL_STATIC_COLOR:
	  colormap->colors = g_new (GdkColor, colormap->size);
	  gdk_colormap_sync (colormap, TRUE);
	  
	case GDK_VISUAL_DIRECT_COLOR:
	case GDK_VISUAL_TRUE_COLOR:
	  break;
	}

      gdk_colormap_add (colormap);
    }

  return colormap;
}

gint
gdk_colormap_get_system_size (void)
{
  return DisplayCells (gdk_display, _gdk_screen);
}

void
gdk_colormap_change (GdkColormap *colormap,
		     gint         ncolors)
{
  GdkColormapPrivateX11 *private;
  GdkVisual *visual;
  XColor *palette;
  gint shift;
  int max_colors;
  int size;
  int i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  palette = g_new (XColor, ncolors);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  switch (colormap->visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      for (i = 0; i < ncolors; i++)
	{
	  palette[i].pixel = colormap->colors[i].pixel;
	  palette[i].red = colormap->colors[i].red;
	  palette[i].green = colormap->colors[i].green;
	  palette[i].blue = colormap->colors[i].blue;
	  palette[i].flags = DoRed | DoGreen | DoBlue;
	}

      XStoreColors (private->xdisplay, private->xcolormap, palette, ncolors);
      break;

    case GDK_VISUAL_DIRECT_COLOR:
      visual = colormap->visual;

      shift = visual->red_shift;
      max_colors = 1 << visual->red_prec;
      size = (ncolors < max_colors) ? (ncolors) : (max_colors);

      for (i = 0; i < size; i++)
	{
	  palette[i].pixel = i << shift;
	  palette[i].red = colormap->colors[i].red;
	  palette[i].flags = DoRed;
	}

      XStoreColors (private->xdisplay, private->xcolormap, palette, size);

      shift = visual->green_shift;
      max_colors = 1 << visual->green_prec;
      size = (ncolors < max_colors) ? (ncolors) : (max_colors);

      for (i = 0; i < size; i++)
	{
	  palette[i].pixel = i << shift;
	  palette[i].green = colormap->colors[i].green;
	  palette[i].flags = DoGreen;
	}

      XStoreColors (private->xdisplay, private->xcolormap, palette, size);

      shift = visual->blue_shift;
      max_colors = 1 << visual->blue_prec;
      size = (ncolors < max_colors) ? (ncolors) : (max_colors);

      for (i = 0; i < size; i++)
	{
	  palette[i].pixel = i << shift;
	  palette[i].blue = colormap->colors[i].blue;
	  palette[i].flags = DoBlue;
	}

      XStoreColors (private->xdisplay, private->xcolormap, palette, size);
      break;

    default:
      break;
    }

  g_free (palette);
}

gboolean
gdk_colors_alloc (GdkColormap   *colormap,
		  gboolean       contiguous,
		  gulong        *planes,
		  gint           nplanes,
		  gulong        *pixels,
		  gint           npixels)
{
  GdkColormapPrivateX11 *private;
  gint return_val;
  gint i;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), 0);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  return_val = XAllocColorCells (private->xdisplay, private->xcolormap,
				 contiguous, planes, nplanes, pixels, npixels);

  if (return_val)
    {
      for (i=0; i<npixels; i++)
	{
	  private->info[pixels[i]].ref_count++;
	  private->info[pixels[i]].flags |= GDK_COLOR_WRITEABLE;
	}
    }

  return return_val != 0;
}

gboolean
gdk_color_parse (const gchar *spec,
		 GdkColor *color)
{
  Colormap xcolormap;
  XColor xcolor;
  gboolean return_val;

  g_return_val_if_fail (spec != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  xcolormap = DefaultColormap (gdk_display, _gdk_screen);

  if (XParseColor (gdk_display, xcolormap, spec, &xcolor))
    {
      return_val = TRUE;
      color->red = xcolor.red;
      color->green = xcolor.green;
      color->blue = xcolor.blue;
    }
  else
    return_val = FALSE;

  return return_val;
}

/* This is almost identical to gdk_colormap_free_colors.
 * Keep them in sync!
 */
void
gdk_colors_free (GdkColormap *colormap,
		 gulong      *in_pixels,
		 gint         in_npixels,
		 gulong       planes)
{
  GdkColormapPrivateX11 *private;
  gulong *pixels;
  gint npixels = 0;
  gint i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  g_return_if_fail (in_pixels != NULL);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if ((colormap->visual->type != GDK_VISUAL_PSEUDO_COLOR) &&
      (colormap->visual->type != GDK_VISUAL_GRAYSCALE))
    return;
  
  pixels = g_new (gulong, in_npixels);

  for (i=0; i<in_npixels; i++)
    {
      gulong pixel = in_pixels[i];
      
      if (private->info[pixel].ref_count)
	{
	  private->info[pixel].ref_count--;

	  if (private->info[pixel].ref_count == 0)
	    {
	      pixels[npixels++] = pixel;
	      if (!(private->info[pixel].flags & GDK_COLOR_WRITEABLE))
		g_hash_table_remove (private->hash, &colormap->colors[pixel]);
	      private->info[pixel].flags = 0;
	    }
	}
    }

  if (npixels)
    XFreeColors (private->xdisplay, private->xcolormap,
		 pixels, npixels, planes);
  g_free (pixels);
}

/* This is almost identical to gdk_colors_free.
 * Keep them in sync!
 */
void
gdk_colormap_free_colors (GdkColormap *colormap,
			  GdkColor    *colors,
			  gint         ncolors)
{
  GdkColormapPrivateX11 *private;
  gulong *pixels;
  gint npixels = 0;
  gint i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  g_return_if_fail (colors != NULL);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if ((colormap->visual->type != GDK_VISUAL_PSEUDO_COLOR) &&
      (colormap->visual->type != GDK_VISUAL_GRAYSCALE))
    return;

  pixels = g_new (gulong, ncolors);

  for (i=0; i<ncolors; i++)
    {
      gulong pixel = colors[i].pixel;
      
      if (private->info[pixel].ref_count)
	{
	  private->info[pixel].ref_count--;

	  if (private->info[pixel].ref_count == 0)
	    {
	      pixels[npixels++] = pixel;
	      if (!(private->info[pixel].flags & GDK_COLOR_WRITEABLE))
		g_hash_table_remove (private->hash, &colormap->colors[pixel]);
	      private->info[pixel].flags = 0;
	    }
	}
    }

  if (npixels)
    XFreeColors (private->xdisplay, private->xcolormap,
		 pixels, npixels, 0);

  g_free (pixels);
}

/********************
 * Color allocation *
 ********************/

/* Try to allocate a single color using XAllocColor. If it succeeds,
 * cache the result in our colormap, and store in ret.
 */
static gboolean 
gdk_colormap_alloc1 (GdkColormap *colormap,
		     GdkColor    *color,
		     GdkColor    *ret)
{
  GdkColormapPrivateX11 *private;
  XColor xcolor;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  xcolor.red = color->red;
  xcolor.green = color->green;
  xcolor.blue = color->blue;
  xcolor.pixel = color->pixel;
  xcolor.flags = DoRed | DoGreen | DoBlue;

  if (XAllocColor (private->xdisplay, private->xcolormap, &xcolor))
    {
      ret->pixel = xcolor.pixel;
      ret->red = xcolor.red;
      ret->green = xcolor.green;
      ret->blue = xcolor.blue;
      
      if (ret->pixel < colormap->size)
	{
	  if (private->info[ret->pixel].ref_count) /* got a duplicate */
	    {
	      XFreeColors (private->xdisplay, private->xcolormap,
			   &xcolor.pixel, 1, 0);
	    }
	  else
	    {
	      colormap->colors[ret->pixel] = *color;
	      colormap->colors[ret->pixel].pixel = ret->pixel;
	      private->info[ret->pixel].ref_count = 1;

	      g_hash_table_insert (private->hash,
				   &colormap->colors[ret->pixel],
				   &colormap->colors[ret->pixel]);
	    }
	}
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gint
gdk_colormap_alloc_colors_writeable (GdkColormap *colormap,
				     GdkColor    *colors,
				     gint         ncolors,
				     gboolean     writeable,
				     gboolean     best_match,
				     gboolean    *success)
{
  GdkColormapPrivateX11 *private;
  gulong *pixels;
  Status status;
  gint i, index;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if (private->private_val)
    {
      index = 0;
      for (i=0; i<ncolors; i++)
	{
	  while ((index < colormap->size) && (private->info[index].ref_count != 0))
	    index++;
	  
	  if (index < colormap->size)
	    {
	      colors[i].pixel = index;
	      success[i] = TRUE;
	      private->info[index].ref_count++;
	      private->info[i].flags |= GDK_COLOR_WRITEABLE;
	    }
	  else
	    break;
	}
      return i;
    }
  else
    {
      pixels = g_new (gulong, ncolors);
      /* Allocation of a writeable color cells */
      
      status =  XAllocColorCells (private->xdisplay, private->xcolormap,
				  FALSE, NULL, 0, pixels, ncolors);
      if (status)
	{
	  for (i=0; i<ncolors; i++)
	    {
	      colors[i].pixel = pixels[i];
	      private->info[pixels[i]].ref_count++;
	      private->info[pixels[i]].flags |= GDK_COLOR_WRITEABLE;
	    }
	}
      
      g_free (pixels);

      return status ? ncolors : 0; 
    }
}

static gint
gdk_colormap_alloc_colors_private (GdkColormap *colormap,
				   GdkColor    *colors,
				   gint         ncolors,
				   gboolean     writeable,
				   gboolean     best_match,
				   gboolean    *success)
{
  GdkColormapPrivateX11 *private;
  gint i, index;
  XColor *store = g_new (XColor, ncolors);
  gint nstore = 0;
  gint nremaining = 0;
  
  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  index = -1;

  /* First, store the colors we have room for */

  index = 0;
  for (i=0; i<ncolors; i++)
    {
      if (!success[i])
	{
	  while ((index < colormap->size) && (private->info[index].ref_count != 0))
	    index++;

	  if (index < colormap->size)
	    {
	      store[nstore].red = colors[i].red;
	      store[nstore].blue = colors[i].blue;
	      store[nstore].green = colors[i].green;
	      store[nstore].pixel = index;
	      nstore++;

	      success[i] = TRUE;

	      colors[i].pixel = index;
	      private->info[index].ref_count++;
	    }
	  else
	    nremaining++;
	}
    }
  
  XStoreColors (private->xdisplay, private->xcolormap, store, nstore);
  g_free (store);

  if (nremaining > 0 && best_match)
    {
      /* Get best matches for remaining colors */

      gchar *available = g_new (gchar, colormap->size);
      for (i = 0; i < colormap->size; i++)
	available[i] = TRUE;

      for (i=0; i<ncolors; i++)
	{
	  if (!success[i])
	    {
	      index = gdk_colormap_match_color (colormap, 
						&colors[i], 
						available);
	      if (index != -1)
		{
		  colors[i] = colormap->colors[index];
		  private->info[index].ref_count++;

		  success[i] = TRUE;
		  nremaining--;
		}
	    }
	}
      g_free (available);
    }

  return (ncolors - nremaining);
}

static gint
gdk_colormap_alloc_colors_shared (GdkColormap *colormap,
				  GdkColor    *colors,
				  gint         ncolors,
				  gboolean     writeable,
				  gboolean     best_match,
				  gboolean    *success)
{
  GdkColormapPrivateX11 *private;
  gint i, index;
  gint nremaining = 0;
  gint nfailed = 0;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  index = -1;

  for (i=0; i<ncolors; i++)
    {
      if (!success[i])
	{
	  if (gdk_colormap_alloc1 (colormap, &colors[i], &colors[i]))
	    success[i] = TRUE;
	  else
	    nremaining++;
	}
    }


  if (nremaining > 0 && best_match)
    {
      gchar *available = g_new (gchar, colormap->size);
      for (i = 0; i < colormap->size; i++)
	available[i] = ((private->info[i].ref_count == 0) ||
			!(private->info[i].flags && GDK_COLOR_WRITEABLE));
      gdk_colormap_sync (colormap, FALSE);
      
      while (nremaining > 0)
	{
	  for (i=0; i<ncolors; i++)
	    {
	      if (!success[i])
		{
		  index = gdk_colormap_match_color (colormap, &colors[i], available);
		  if (index != -1)
		    {
		      if (private->info[index].ref_count)
			{
			  private->info[index].ref_count++;
			  colors[i] = colormap->colors[index];
			  success[i] = TRUE;
			  nremaining--;
			}
		      else
			{
			  if (gdk_colormap_alloc1 (colormap, 
						   &colormap->colors[index],
						   &colors[i]))
			    {
			      success[i] = TRUE;
			      nremaining--;
			      break;
			    }
			  else
			    {
			      available[index] = FALSE;
			    }
			}
		    }
		  else
		    {
		      nfailed++;
		      nremaining--;
		      success[i] = 2; /* flag as permanent failure */
		    }
		}
	    }
	}
      g_free (available);
    }

  /* Change back the values we flagged as permanent failures */
  if (nfailed > 0)
    {
      for (i=0; i<ncolors; i++)
	if (success[i] == 2)
	  success[i] = FALSE;
      nremaining = nfailed;
    }
  
  return (ncolors - nremaining);
}

static gint
gdk_colormap_alloc_colors_pseudocolor (GdkColormap *colormap,
				       GdkColor    *colors,
				       gint         ncolors,
				       gboolean     writeable,
				       gboolean     best_match,
				       gboolean    *success)
{
  GdkColormapPrivateX11 *private;
  GdkColor *lookup_color;
  gint i;
  gint nremaining = 0;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  /* Check for an exact match among previously allocated colors */

  for (i=0; i<ncolors; i++)
    {
      if (!success[i])
	{
	  lookup_color = g_hash_table_lookup (private->hash, &colors[i]);
	  if (lookup_color)
	    {
	      private->info[lookup_color->pixel].ref_count++;
	      colors[i].pixel = lookup_color->pixel;
	      success[i] = TRUE;
	    }
	  else
	    nremaining++;
	}
    }

  /* If that failed, we try to allocate a new color, or approxmiate
   * with what we can get if best_match is TRUE.
   */
  if (nremaining > 0)
    {
      if (private->private_val)
	return gdk_colormap_alloc_colors_private (colormap, colors, ncolors, writeable, best_match, success);
      else
	return gdk_colormap_alloc_colors_shared (colormap, colors, ncolors, writeable, best_match, success);
    }
  else
    return 0;
}

gint
gdk_colormap_alloc_colors (GdkColormap *colormap,
			   GdkColor    *colors,
			   gint         ncolors,
			   gboolean     writeable,
			   gboolean     best_match,
			   gboolean    *success)
{
  GdkColormapPrivateX11 *private;
  GdkVisual *visual;
  gint i;
  gint nremaining = 0;
  XColor xcolor;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), FALSE);
  g_return_val_if_fail (colors != NULL, FALSE);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  for (i=0; i<ncolors; i++)
    {
      success[i] = FALSE;
    }

  switch (colormap->visual->type)
    {
    case GDK_VISUAL_PSEUDO_COLOR:
    case GDK_VISUAL_GRAYSCALE:
      if (writeable)
	return gdk_colormap_alloc_colors_writeable (colormap, colors, ncolors,
						    writeable, best_match, success);
      else
	return gdk_colormap_alloc_colors_pseudocolor (colormap, colors, ncolors,
						    writeable, best_match, success);
      break;

    case GDK_VISUAL_DIRECT_COLOR:
    case GDK_VISUAL_TRUE_COLOR:
      visual = colormap->visual;

      for (i=0; i<ncolors; i++)
	{
	  colors[i].pixel = (((colors[i].red >> (16 - visual->red_prec)) << visual->red_shift) +
			     ((colors[i].green >> (16 - visual->green_prec)) << visual->green_shift) +
			     ((colors[i].blue >> (16 - visual->blue_prec)) << visual->blue_shift));
	  success[i] = TRUE;
	}
      break;

    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
      for (i=0; i<ncolors; i++)
	{
	  xcolor.red = colors[i].red;
	  xcolor.green = colors[i].green;
	  xcolor.blue = colors[i].blue;
	  xcolor.pixel = colors[i].pixel;
	  xcolor.flags = DoRed | DoGreen | DoBlue;

	  if (XAllocColor (private->xdisplay, private->xcolormap, &xcolor))
	    {
	      colors[i].pixel = xcolor.pixel;
	      success[i] = TRUE;
	    }
	  else
	    nremaining++;
	}
      break;
    }
  return nremaining;
}

void
gdk_colormap_query_color (GdkColormap *colormap,
			  gulong       pixel,
			  GdkColor    *result)
{
  XColor xcolor;
  GdkVisual *visual;
  GdkColormapPrivateX11 *private;
  
  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  
  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  visual = gdk_colormap_get_visual (colormap);

  switch (visual->type) {
  case GDK_VISUAL_DIRECT_COLOR:
  case GDK_VISUAL_TRUE_COLOR:
    result->red = 65535. * (double)((pixel & visual->red_mask) >> visual->red_shift) / ((1 << visual->red_prec) - 1);
    result->green = 65535. * (double)((pixel & visual->green_mask) >> visual->green_shift) / ((1 << visual->green_prec) - 1);
    result->blue = 65535. * (double)((pixel & visual->blue_mask) >> visual->blue_shift) / ((1 << visual->blue_prec) - 1);
    break;
  case GDK_VISUAL_STATIC_GRAY:
  case GDK_VISUAL_GRAYSCALE:
    result->red = result->green = result->blue = 65535. * (double)pixel/((1<<visual->depth) - 1);
    break;
  case GDK_VISUAL_STATIC_COLOR:
    xcolor.pixel = pixel;
    XQueryColor (private->xdisplay, private->xcolormap, &xcolor);
    result->red = xcolor.red;
    result->green = xcolor.green;
    result->blue =  xcolor.blue;
    break;
  case GDK_VISUAL_PSEUDO_COLOR:
    result->red = colormap->colors[pixel].red;
    result->green = colormap->colors[pixel].green;
    result->blue = colormap->colors[pixel].blue;
    break;
  default:
    g_assert_not_reached ();
    break;
  }
}

gboolean
gdk_color_change (GdkColormap *colormap,
		  GdkColor    *color)
{
  GdkColormapPrivateX11 *private;
  XColor xcolor;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  xcolor.pixel = color->pixel;
  xcolor.red = color->red;
  xcolor.green = color->green;
  xcolor.blue = color->blue;
  xcolor.flags = DoRed | DoGreen | DoBlue;

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);
  XStoreColor (private->xdisplay, private->xcolormap, &xcolor);

  return TRUE;
}

/* XXX: Do not use this function until it is fixed. An X Colormap
 *      is useless unless we also have the visual.
 */
GdkColormap*
gdkx_colormap_get (Colormap xcolormap)
{
  GdkColormap *colormap;
  GdkColormapPrivateX11 *private;

  colormap = gdk_colormap_lookup (xcolormap);
  if (colormap)
    return colormap;

  if (xcolormap == DefaultColormap (gdk_display, _gdk_screen))
    return gdk_colormap_get_system ();

  colormap = g_object_new (gdk_colormap_get_type (), NULL);
  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  private->xdisplay = gdk_display;
  private->xcolormap = xcolormap;
  colormap->visual = NULL;
  private->private_val = TRUE;

  /* To do the following safely, we would have to have some way of finding
   * out what the size or visual of the given colormap is. It seems
   * X doesn't allow this
   */

#if 0
  for (i = 0; i < 256; i++)
    {
      xpalette[i].pixel = i;
      xpalette[i].red = 0;
      xpalette[i].green = 0;
      xpalette[i].blue = 0;
    }

  XQueryColors (gdk_display, private->xcolormap, xpalette, 256);

  for (i = 0; i < 256; i++)
    {
      colormap->colors[i].pixel = xpalette[i].pixel;
      colormap->colors[i].red = xpalette[i].red;
      colormap->colors[i].green = xpalette[i].green;
      colormap->colors[i].blue = xpalette[i].blue;
    }
#endif

  colormap->colors = NULL;
  colormap->size = 0;

  gdk_colormap_add (colormap);

  return colormap;
}


static gint
gdk_colormap_match_color (GdkColormap *cmap,
			  GdkColor    *color,
			  const gchar *available)
{
  GdkColor *colors;
  guint sum, max;
  gint rdiff, gdiff, bdiff;
  gint i, index;

  g_return_val_if_fail (cmap != NULL, 0);
  g_return_val_if_fail (color != NULL, 0);

  colors = cmap->colors;
  max = 3 * (65536);
  index = -1;

  for (i = 0; i < cmap->size; i++)
    {
      if ((!available) || (available && available[i]))
	{
	  rdiff = (color->red - colors[i].red);
	  gdiff = (color->green - colors[i].green);
	  bdiff = (color->blue - colors[i].blue);

	  sum = ABS (rdiff) + ABS (gdiff) + ABS (bdiff);

	  if (sum < max)
	    {
	      index = i;
	      max = sum;
	    }
	}
    }

  return index;
}


GdkColormap*
gdk_colormap_lookup (Colormap xcolormap)
{
  GdkColormap *cmap;

  if (!colormap_hash)
    return NULL;

  cmap = g_hash_table_lookup (colormap_hash, &xcolormap);
  return cmap;
}

static void
gdk_colormap_add (GdkColormap *cmap)
{
  GdkColormapPrivateX11 *private;

  if (!colormap_hash)
    colormap_hash = g_hash_table_new ((GHashFunc) gdk_colormap_hash,
				      (GEqualFunc) gdk_colormap_equal);

  private = GDK_COLORMAP_PRIVATE_DATA (cmap);

  g_hash_table_insert (colormap_hash, &private->xcolormap, cmap);
}

static void
gdk_colormap_remove (GdkColormap *cmap)
{
  GdkColormapPrivateX11 *private;

  if (!colormap_hash)
    colormap_hash = g_hash_table_new ((GHashFunc) gdk_colormap_hash,
				      (GEqualFunc) gdk_colormap_equal);

  private = GDK_COLORMAP_PRIVATE_DATA (cmap);

  g_hash_table_remove (colormap_hash, &private->xcolormap);
}

static guint
gdk_colormap_hash (Colormap *cmap)
{
  return *cmap;
}

static gboolean
gdk_colormap_equal (Colormap *a,
		    Colormap *b)
{
  return (*a == *b);
}

Display *
gdk_x11_colormap_get_xdisplay (GdkColormap *colormap)
{
  GdkColormapPrivateX11 *private;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), NULL);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  return private->xdisplay;
}

Colormap
gdk_x11_colormap_get_xcolormap (GdkColormap *colormap)
{
  GdkColormapPrivateX11 *private;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), None);

  private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  return private->xcolormap;
}
