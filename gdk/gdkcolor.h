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

typedef struct _GdkColormapClass GdkColormapClass;

#define GDK_TYPE_COLORMAP              (gdk_colormap_get_type ())
#define GDK_COLORMAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_COLORMAP, GdkColormap))
#define GDK_COLORMAP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_COLORMAP, GdkColormapClass))
#define GDK_IS_COLORMAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_COLORMAP))
#define GDK_IS_COLORMAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_COLORMAP))
#define GDK_COLORMAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_COLORMAP, GdkColormapClass))


struct _GdkColormap
{
  GObject parent_instance;

  /*< public >*/
  gint      size;
  GdkColor *colors;

  /*< private >*/
  GdkVisual *visual;
  
  gpointer windowing_data;
};

struct _GdkColormapClass
{
  GObjectClass parent_class;

};

GType        gdk_colormap_get_type (void) G_GNUC_CONST;

GdkColormap* gdk_colormap_new	  (GdkVisual   *visual,
				   gboolean	allocate);
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
     
GdkColor *gdk_color_copy  (const GdkColor *color);
void      gdk_color_free  (GdkColor       *color);
gint      gdk_color_parse (const gchar    *spec,
			   GdkColor       *color);
guint     gdk_color_hash  (const GdkColor *colora);
gboolean  gdk_color_equal (const GdkColor *colora,
			   const GdkColor *colorb);

/* The following functions are deprecated */
void gdk_colors_store	 (GdkColormap	*colormap,
			  GdkColor	*colors,
			  gint		 ncolors);
gint gdk_colors_alloc	 (GdkColormap	*colormap,
			  gboolean	 contiguous,
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
