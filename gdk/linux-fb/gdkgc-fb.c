#include <config.h>
#include <string.h>
#include <stdlib.h>

#include "gdkprivate-fb.h"
#include "gdkgc.h"
#include "gdkfb.h"
#include "gdkregion-generic.h"

typedef enum {
  GDK_GC_DIRTY_CLIP = 1 << 0,
  GDK_GC_DIRTY_TS = 1 << 1
} GdkGCDirtyValues;

static void gdk_fb_gc_finalize   (GObject         *obj);
static void gdk_fb_gc_get_values (GdkGC           *gc,
				  GdkGCValues     *values);
static void gdk_fb_gc_set_values (GdkGC           *gc,
				  GdkGCValues     *values,
				  GdkGCValuesMask  values_mask);
static void gdk_fb_gc_set_dashes (GdkGC           *gc,
				  gint             dash_offset,
				  gint8            dash_list[],
				  gint             n);

static gpointer parent_class = NULL;

static void
gdk_gc_fb_class_init (GdkGCFBClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkGCClass *gc_class = GDK_GC_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_fb_gc_finalize;

  gc_class->get_values = gdk_fb_gc_get_values;
  gc_class->set_values = gdk_fb_gc_set_values;
  gc_class->set_dashes = gdk_fb_gc_set_dashes;
}

GType
gdk_gc_fb_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkGCFBClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_gc_fb_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkGCFBData),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (GDK_TYPE_GC,
                                            "GdkGCFB",
                                            &object_info,
					    0);
    }

  return object_type;
}

GdkGC *
_gdk_fb_gc_new (GdkDrawable      *drawable,
		GdkGCValues      *values,
		GdkGCValuesMask   values_mask)
{
  GdkGC *gc;
  GdkGCFBData *private;

  gc = GDK_GC (g_object_new (gdk_gc_fb_get_type (), NULL));

  private = (GdkGCFBData *)gc;
  
  private->depth = GDK_DRAWABLE_FBDATA (drawable)->depth;
  private->values.foreground.pixel = 255;
  private->values.foreground.red =
    private->values.foreground.green =
    private->values.foreground.blue = 65535;

  private->values.cap_style = GDK_CAP_BUTT;

  _gdk_fb_gc_calc_state (gc, _GDK_FB_GC_DEPTH);

  gdk_fb_gc_set_values (gc, values, values_mask);
  return gc;
}

static void
gdk_fb_gc_finalize (GObject *obj)
{
  GdkGC *gc = GDK_GC_P (obj);
  GdkGCFBData *private;

  private = GDK_GC_FBDATA (gc);

  if (private->clip_region)
    gdk_region_destroy (private->clip_region);
  if (private->values.clip_mask)
    gdk_pixmap_unref (private->values.clip_mask);
  if (private->values.stipple)
    gdk_pixmap_unref (private->values.stipple);
  if (private->values.tile)
    gdk_pixmap_unref (private->values.tile);

  g_free (private->dash_list);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gdk_fb_gc_get_values (GdkGC       *gc,
		      GdkGCValues *values)
{
  *values = GDK_GC_FBDATA (gc)->values;
}


static void
gdk_fb_gc_set_values (GdkGC           *gc,
		      GdkGCValues     *values,
		      GdkGCValuesMask  values_mask)
{
  GdkPixmap *oldpm;
  GdkFont *oldf;
  GdkGCFBData *private;

  private = GDK_GC_FBDATA (gc);
  
  if (values_mask & GDK_GC_FOREGROUND)
    {
      private->values.foreground = values->foreground;
      private->values_mask |= GDK_GC_FOREGROUND;
    }

  if (values_mask & GDK_GC_BACKGROUND)
    {
      private->values.background = values->background;
      private->values_mask |= GDK_GC_BACKGROUND;
    }

  if (values_mask & GDK_GC_FONT)
    {
      oldf = private->values.font;
      private->values.font = gdk_font_ref (values->font);
      private->values_mask |= GDK_GC_FONT;
      if (oldf)
	gdk_font_unref(oldf);
    }

  if (values_mask & GDK_GC_FUNCTION)
    {
      private->values.function = values->function;
      private->values_mask |= GDK_GC_FUNCTION;
    }

  if (values_mask & GDK_GC_FILL)
    {
      private->values.fill = values->fill;
      private->values_mask |= GDK_GC_FILL;
    }

  if (values_mask & GDK_GC_TILE)
    {
      oldpm = private->values.tile;
      if (values->tile)
	g_assert (GDK_DRAWABLE_IMPL_FBDATA (values->tile)->depth >= 8);

      private->values.tile = values->tile ? gdk_pixmap_ref (values->tile) : NULL;
      private->values_mask |= GDK_GC_TILE;
      if (oldpm)
	gdk_pixmap_unref (oldpm);
    }

  if (values_mask & GDK_GC_STIPPLE)
    {
      oldpm = private->values.stipple;
      if (values->stipple)
	g_assert (GDK_DRAWABLE_IMPL_FBDATA (values->stipple)->depth == 1);
      private->values.stipple = values->stipple ? gdk_pixmap_ref (values->stipple) : NULL;
      private->values_mask |= GDK_GC_STIPPLE;
      if (oldpm)
	gdk_pixmap_unref (oldpm);
    }

  if (values_mask & GDK_GC_CLIP_MASK)
    {
      oldpm = private->values.clip_mask;

      private->values.clip_mask = values->clip_mask ? gdk_pixmap_ref (values->clip_mask) : NULL;
      private->values_mask |= GDK_GC_CLIP_MASK;
      if (oldpm)
	gdk_pixmap_unref (oldpm);

      if (private->clip_region)
	{
	  gdk_region_destroy (private->clip_region);
	  private->clip_region = NULL;
	}
    }

  if (values_mask & GDK_GC_SUBWINDOW)
    {
      private->values.subwindow_mode = values->subwindow_mode;
      private->values_mask |= GDK_GC_SUBWINDOW;
    }

  if (values_mask & GDK_GC_TS_X_ORIGIN)
    {
      private->values.ts_x_origin = values->ts_x_origin;
      private->values_mask |= GDK_GC_TS_X_ORIGIN;
    }

  if (values_mask & GDK_GC_TS_Y_ORIGIN)
    {
      private->values.ts_y_origin = values->ts_y_origin;
      private->values_mask |= GDK_GC_TS_Y_ORIGIN;
    }

  if (values_mask & GDK_GC_CLIP_X_ORIGIN)
    {
      private->values.clip_x_origin = GDK_GC_P (gc)->clip_x_origin = values->clip_x_origin;
      private->values_mask |= GDK_GC_CLIP_X_ORIGIN;
    }

  if (values_mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      private->values.clip_y_origin = GDK_GC_P(gc)->clip_y_origin = values->clip_y_origin;
      private->values_mask |= GDK_GC_CLIP_Y_ORIGIN;
    }

  if (values_mask & GDK_GC_EXPOSURES)
    {
      private->values.graphics_exposures = values->graphics_exposures;
      private->values_mask |= GDK_GC_EXPOSURES;
    }

  if (values_mask & GDK_GC_LINE_WIDTH)
    {
      private->values.line_width = values->line_width;
      private->values_mask |= GDK_GC_LINE_WIDTH;
    }

  if (values_mask & GDK_GC_LINE_STYLE)
    {
      private->values.line_style = values->line_style;
      private->values_mask |= GDK_GC_LINE_STYLE;
    }

  if (values_mask & GDK_GC_CAP_STYLE)
    {
      private->values.cap_style = values->cap_style;
      private->values_mask |= GDK_GC_CAP_STYLE;
    }

  if (values_mask & GDK_GC_JOIN_STYLE)
    {
      private->values.join_style = values->join_style;
      private->values_mask |= GDK_GC_JOIN_STYLE;
    }

  _gdk_fb_gc_calc_state (gc, values_mask);
}

static void
gdk_fb_gc_set_dashes (GdkGC *gc,
		      gint dash_offset,
		      gint8 dash_list[],
		      gint n)
{
  GdkGCFBData *private;

  private = GDK_GC_FBDATA (gc);

  private->dash_offset = dash_offset;
  private->dash_list_len = n;
  
  if (n)
    {
      private->dash_list = g_realloc (private->dash_list, n);
      memcpy (private->dash_list, dash_list, n);
    }
  else
    {
      g_free (private->dash_list);
      private->dash_list = NULL;
    }
}

static void
gc_unset_cmask(GdkGC *gc)
{
  GdkGCFBData *data;
  data = GDK_GC_FBDATA (gc);
  
  if (data->values.clip_mask)
    {
      gdk_pixmap_unref (data->values.clip_mask);
      data->values.clip_mask = NULL;
      data->values_mask &= ~ GDK_GC_CLIP_MASK;
    }

  _gdk_fb_gc_calc_state (gc, GDK_GC_CLIP_MASK);
}

void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGC *private = (GdkGC *)gc;
  GdkGCFBData *data;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_FBDATA (gc);

  if (data->clip_region)
    {
      gdk_region_destroy (data->clip_region);
      data->clip_region = NULL;
    }

  if (rectangle)
    data->clip_region = gdk_region_rectangle (rectangle);

  private->clip_x_origin = 0;
  private->clip_y_origin = 0;
  data->values.clip_x_origin = 0;
  data->values.clip_y_origin = 0;

  gc_unset_cmask (gc);

  _gdk_fb_gc_calc_state (gc, GDK_GC_CLIP_X_ORIGIN|GDK_GC_CLIP_Y_ORIGIN);
} 

void
gdk_gc_set_clip_region (GdkGC	  *gc,
			GdkRegion *region)
{
  GdkGC *private = (GdkGC *)gc;
  GdkGCFBData *data;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_FBDATA (gc);

  if (region == data->clip_region)
    return;

  if (data->clip_region)
    {
      gdk_region_destroy (data->clip_region);
      data->clip_region = NULL;
    }

  if (region)
    data->clip_region = gdk_region_copy (region);
  
  private->clip_x_origin = 0;
  private->clip_y_origin = 0;
  data->values.clip_x_origin = 0;
  data->values.clip_y_origin = 0;

  gc_unset_cmask (gc);
  
  _gdk_fb_gc_calc_state (gc, GDK_GC_CLIP_X_ORIGIN|GDK_GC_CLIP_Y_ORIGIN);
}


void
gdk_gc_copy (GdkGC *dst_gc, GdkGC *src_gc)
{
  GdkGCFBData *dst_private;
  GdkGCFBData *src_private;

  src_private = GDK_GC_FBDATA (dst_gc);
  dst_private = GDK_GC_FBDATA (dst_gc);

  g_return_if_fail (dst_gc != NULL);
  g_return_if_fail (src_gc != NULL);

  if (dst_private->clip_region)
    gdk_region_destroy (dst_private->clip_region);

  if (dst_private->values_mask & GDK_GC_FONT)
    gdk_font_unref (dst_private->values.font);
  if (dst_private->values_mask & GDK_GC_TILE)
    gdk_pixmap_unref (dst_private->values.tile);
  if (dst_private->values_mask & GDK_GC_STIPPLE)
    gdk_pixmap_unref (dst_private->values.stipple);
  if (dst_private->values_mask & GDK_GC_CLIP_MASK)
    gdk_pixmap_unref (dst_private->values.clip_mask);

  g_free (dst_private->dash_list);

  *dst_private = *src_private;
  if (dst_private->values_mask & GDK_GC_FONT)
    gdk_font_ref (dst_private->values.font);
  if (dst_private->values_mask & GDK_GC_TILE)
    gdk_pixmap_ref (dst_private->values.tile);
  if (dst_private->values_mask & GDK_GC_STIPPLE)
    gdk_pixmap_ref (dst_private->values.stipple);
  if (dst_private->values_mask & GDK_GC_CLIP_MASK)
    gdk_pixmap_ref (dst_private->values.clip_mask);
  
  if (dst_private->clip_region)
    dst_private->clip_region = gdk_region_copy (dst_private->clip_region);
  
  if (dst_private->dash_list)
    {
      dst_private->dash_list = g_malloc (dst_private->dash_list_len);
      memcpy (dst_private->dash_list,
	      src_private->dash_list,
	      dst_private->dash_list_len);
    }
}
