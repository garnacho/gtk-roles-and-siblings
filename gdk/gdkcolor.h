#ifndef __GDK_COLOR_H__
#define __GDK_COLOR_H__

#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* The color type.
 *   A color consists of red, green and blue values in the
 *    range 0-65535 and a pixel value. The pixel value is highly
 *    dependent on the depth and colormap which this color will
 *    be used to draw into. Therefore, sharing colors between
 *    colormaps is a bad idea.
 */
struct _GdkColor
{
  gulong  pixel;
  gushort red;
  gushort green;
  gushort blue;
};

/* The colormap type.
 */
struct _GdkColormap
{
  gint      size;
  GdkColor *colors;
};


GdkColormap* gdk_colormap_new	  (GdkVisual   *visual,
				   gint		allocate);
GdkColormap* gdk_colormap_ref	  (GdkColormap *cmap);
void	     gdk_colormap_unref	  (GdkColormap *cmap);

GdkColormap* gdk_colormap_get_system	   (void);
gint	     gdk_colormap_get_system_size  (void);

void gdk_colormap_change (GdkColormap	*colormap,
			  gint		 ncolors);


gint  gdk_colormap_alloc_colors   (GdkColormap *colormap,
				   GdkColor    *colors,
				   gint         ncolors,
				   gboolean     writeable,
				   gboolean     best_match,
				   gboolean    *success);
gboolean gdk_colormap_alloc_color (GdkColormap *colormap,
				   GdkColor    *color,
				   gboolean     writeable,
				   gboolean     best_match);
void     gdk_colormap_free_colors (GdkColormap *colormap,
				   GdkColor    *colors,
				   gint         ncolors);

GdkVisual *gdk_colormap_get_visual (GdkColormap *colormap);
     
GdkColor *gdk_color_copy (GdkColor *color);
void      gdk_color_free (GdkColor *color);

gint gdk_color_parse	 (const gchar	*spec,
			  GdkColor	*color);
guint gdk_color_hash     (const GdkColor *colora);
gint gdk_color_equal	 (const GdkColor *colora,
			  const GdkColor *colorb);


/* The following functions are deprecated */
void gdk_colors_store	 (GdkColormap	*colormap,
			  GdkColor	*colors,
			  gint		 ncolors);
gint gdk_colors_alloc	 (GdkColormap	*colormap,
			  gint		 contiguous,
			  gulong	*planes,
			  gint		 nplanes,
			  gulong	*pixels,
			  gint		 npixels);
void gdk_colors_free	 (GdkColormap	*colormap,
			  gulong	*pixels,
			  gint		 npixels,
			  gulong	 planes);
gint gdk_color_white	 (GdkColormap	*colormap,
			  GdkColor	*color);
gint gdk_color_black	 (GdkColormap	*colormap,
			  GdkColor	*color);
gint gdk_color_alloc	 (GdkColormap	*colormap,
			  GdkColor	*color);
gint gdk_color_change	 (GdkColormap	*colormap,
			  GdkColor	*color);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_COLOR_H__ */
