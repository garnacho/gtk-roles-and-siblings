/* gtkcellrenderer.c
 * Copyright (C) 2000  Red Hat, Inc. Jonathan Blandford
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
#include "gtkcellrenderer.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtktreeprivate.h"

static void gtk_cell_renderer_init       (GtkCellRenderer      *cell);
static void gtk_cell_renderer_class_init (GtkCellRendererClass *class);
static void gtk_cell_renderer_get_property  (GObject              *object,
					     guint                 param_id,
					     GValue               *value,
					     GParamSpec           *pspec);
static void gtk_cell_renderer_set_property  (GObject              *object,
					     guint                 param_id,
					     const GValue         *value,
					     GParamSpec           *pspec);
static void set_cell_bg_color               (GtkCellRenderer      *cell,
					     GdkColor             *color);


#define GTK_CELL_RENDERER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_CELL_RENDERER, GtkCellRendererPrivate))

typedef struct _GtkCellRendererPrivate GtkCellRendererPrivate;
struct _GtkCellRendererPrivate
{
  GdkColor cell_background;
};


enum {
  PROP_ZERO,
  PROP_MODE,
  PROP_VISIBLE,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_XPAD,
  PROP_YPAD,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_IS_EXPANDER,
  PROP_IS_EXPANDED,
  PROP_CELL_BACKGROUND,
  PROP_CELL_BACKGROUND_GDK,
  PROP_CELL_BACKGROUND_SET
};

/* Signal IDs */
enum {
  EDITING_CANCELED,
  LAST_SIGNAL
};

static guint cell_renderer_signals[LAST_SIGNAL] = { 0 };


GType
gtk_cell_renderer_get_type (void)
{
  static GType cell_type = 0;

  if (!cell_type)
    {
      static const GTypeInfo cell_info =
      {
        sizeof (GtkCellRendererClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_cell_renderer_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkCellRenderer),
	0,		/* n_preallocs */
        (GInstanceInitFunc) gtk_cell_renderer_init,
	NULL,		/* value_table */
      };

      cell_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkCellRenderer", 
					  &cell_info, G_TYPE_FLAG_ABSTRACT);
    }

  return cell_type;
}

static void
gtk_cell_renderer_init (GtkCellRenderer *cell)
{
  cell->mode = GTK_CELL_RENDERER_MODE_INERT;
  cell->visible = TRUE;
  cell->width = -1;
  cell->height = -1;
  cell->xalign = 0.5;
  cell->yalign = 0.5;
  cell->xpad = 0;
  cell->ypad = 0;
}

static void
gtk_cell_renderer_class_init (GtkCellRendererClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_cell_renderer_get_property;
  object_class->set_property = gtk_cell_renderer_set_property;

  class->render = NULL;
  class->get_size = NULL;

  /**
   * GtkCellRenderer::editing-canceled:
   * @renderer: the object which received the signal
   *
   * This signal gets emitted when the user cancels the process of editing a
   * cell.  For example, an editable cell renderer could be written to cancel
   * editing when the user presses Escape.
   *
   * See also: gtk_cell_renderer_editing_canceled()
   *
   * Since: 2.4
   */

  cell_renderer_signals[EDITING_CANCELED] =
    g_signal_new ("editing-canceled",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCellRendererClass, editing_canceled),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
				   PROP_MODE,
				   g_param_spec_enum ("mode",
						      P_("mode"),
						      P_("Editable mode of the CellRenderer"),
						      GTK_TYPE_CELL_RENDERER_MODE,
						      GTK_CELL_RENDERER_MODE_INERT,
						      G_PARAM_READABLE |
						      G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible",
							 P_("visible"),
							 P_("Display the cell"),
							 TRUE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_XALIGN,
				   g_param_spec_float ("xalign",
						       P_("xalign"),
						       P_("The x-align"),
						       0.0,
						       1.0,
						       0.0,
						       G_PARAM_READABLE |
						       G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_YALIGN,
				   g_param_spec_float ("yalign",
						       P_("yalign"),
						       P_("The y-align"),
						       0.0,
						       1.0,
						       0.5,
						       G_PARAM_READABLE |
						       G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_XPAD,
				   g_param_spec_uint ("xpad",
						      P_("xpad"),
						      P_("The xpad"),
						      0,
						      G_MAXUINT,
						      2,
						      G_PARAM_READABLE |
						      G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_YPAD,
				   g_param_spec_uint ("ypad",
						      P_("ypad"),
						      P_("The ypad"),
						      0,
						      G_MAXUINT,
						      2,
						      G_PARAM_READABLE |
						      G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_WIDTH,
				   g_param_spec_int ("width",
						     P_("width"),
						     P_("The fixed width"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE |
						     G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_HEIGHT,
				   g_param_spec_int ("height",
						     P_("height"),
						     P_("The fixed height"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE |
						     G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_IS_EXPANDER,
				   g_param_spec_boolean ("is_expander",
							 P_("Is Expander"),
							 P_("Row has children"),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));


  g_object_class_install_property (object_class,
				   PROP_IS_EXPANDED,
				   g_param_spec_boolean ("is_expanded",
							 P_("Is Expanded"),
							 P_("Row is an expander row, and is expanded"),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_CELL_BACKGROUND,
				   g_param_spec_string ("cell_background",
							P_("Cell background color name"),
							P_("Cell background color as a string"),
							NULL,
							G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_CELL_BACKGROUND_GDK,
				   g_param_spec_boxed ("cell_background_gdk",
						       P_("Cell background color"),
						       P_("Cell background color as a GdkColor"),
						       GDK_TYPE_COLOR,
						       G_PARAM_READABLE | G_PARAM_WRITABLE));


#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (object_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, G_PARAM_READABLE | G_PARAM_WRITABLE))

  ADD_SET_PROP ("cell_background_set", PROP_CELL_BACKGROUND_SET,
                P_("Cell background set"),
                P_("Whether this tag affects the cell background color"));

  g_type_class_add_private (object_class, sizeof (GtkCellRendererPrivate));
}

static void
gtk_cell_renderer_get_property (GObject     *object,
				guint        param_id,
				GValue      *value,
				GParamSpec  *pspec)
{
  GtkCellRenderer *cell = GTK_CELL_RENDERER (object);
  GtkCellRendererPrivate *priv = GTK_CELL_RENDERER_GET_PRIVATE (object);

  switch (param_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, cell->mode);
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, cell->visible);
      break;
    case PROP_XALIGN:
      g_value_set_float (value, cell->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float (value, cell->yalign);
      break;
    case PROP_XPAD:
      g_value_set_uint (value, cell->xpad);
      break;
    case PROP_YPAD:
      g_value_set_uint (value, cell->ypad);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, cell->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, cell->height);
      break;
    case PROP_IS_EXPANDER:
      g_value_set_boolean (value, cell->is_expander);
      break;
    case PROP_IS_EXPANDED:
      g_value_set_boolean (value, cell->is_expanded);
      break;
    case PROP_CELL_BACKGROUND_GDK:
      {
	GdkColor color;

	color.red = priv->cell_background.red;
	color.green = priv->cell_background.green;
	color.blue = priv->cell_background.blue;

	g_value_set_boxed (value, &color);
      }
      break;
    case PROP_CELL_BACKGROUND_SET:
      g_value_set_boolean (value, cell->cell_background_set);
      break;
    case PROP_CELL_BACKGROUND:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }

}

static void
gtk_cell_renderer_set_property (GObject      *object,
				guint         param_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  GtkCellRenderer *cell = GTK_CELL_RENDERER (object);

  switch (param_id)
    {
    case PROP_MODE:
      cell->mode = g_value_get_enum (value);
      break;
    case PROP_VISIBLE:
      cell->visible = g_value_get_boolean (value);
      break;
    case PROP_XALIGN:
      cell->xalign = g_value_get_float (value);
      break;
    case PROP_YALIGN:
      cell->yalign = g_value_get_float (value);
      break;
    case PROP_XPAD:
      cell->xpad = g_value_get_uint (value);
      break;
    case PROP_YPAD:
      cell->ypad = g_value_get_uint (value);
      break;
    case PROP_WIDTH:
      cell->width = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      cell->height = g_value_get_int (value);
      break;
    case PROP_IS_EXPANDER:
      cell->is_expander = g_value_get_boolean (value);
      break;
    case PROP_IS_EXPANDED:
      cell->is_expanded = g_value_get_boolean (value);
      break;
    case PROP_CELL_BACKGROUND:
      {
	GdkColor color;

	if (!g_value_get_string (value))
	  set_cell_bg_color (cell, NULL);
	else if (gdk_color_parse (g_value_get_string (value), &color))
	  set_cell_bg_color (cell, &color);
	else
	  g_warning ("Don't know color `%s'", g_value_get_string (value));

	g_object_notify (object, "cell_background_gdk");
      }
      break;
    case PROP_CELL_BACKGROUND_GDK:
      set_cell_bg_color (cell, g_value_get_boxed (value));
      break;
    case PROP_CELL_BACKGROUND_SET:
      cell->cell_background_set = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
set_cell_bg_color (GtkCellRenderer *cell,
		   GdkColor        *color)
{
  GtkCellRendererPrivate *priv = GTK_CELL_RENDERER_GET_PRIVATE (cell);

  if (color)
    {
      if (!cell->cell_background_set)
        {
	  cell->cell_background_set = TRUE;
	  g_object_notify (G_OBJECT (cell), "cell_background_set");
	}

      priv->cell_background.red = color->red;
      priv->cell_background.green = color->green;
      priv->cell_background.blue = color->blue;
    }
  else
    {
      if (cell->cell_background_set)
        {
	  cell->cell_background_set = FALSE;
	  g_object_notify (G_OBJECT (cell), "cell_background_set");
	}
    }
}

/**
 * gtk_cell_renderer_get_size:
 * @cell: a #GtkCellRenderer
 * @widget: the widget the renderer is rendering to
 * @cell_area: The area a cell will be allocated, or %NULL
 * @x_offset: location to return x offset of cell relative to @cell_area, or %NULL
 * @y_offset: location to return y offset of cell relative to @cell_area, or %NULL
 * @width: location to return width needed to render a cell, or %NULL
 * @height: location to return height needed to render a cell, or %NULL
 *
 * Obtains the width and height needed to render the cell. Used by view widgets
 * to determine the appropriate size for the cell_area passed to
 * gtk_cell_renderer_render().  If @cell_area is not %NULL, fills in the x and y
 * offsets (if set) of the cell relative to this location.  Please note that the
 * values set in @width and @height, as well as those in @x_offset and @y_offset
 * are inclusive of the xpad and ypad properties.
 **/
void
gtk_cell_renderer_get_size (GtkCellRenderer *cell,
			    GtkWidget       *widget,
			    GdkRectangle    *cell_area,
			    gint            *x_offset,
			    gint            *y_offset,
			    gint            *width,
			    gint            *height)
{
  gint *real_width = width;
  gint *real_height = height;

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_CELL_RENDERER_GET_CLASS (cell)->get_size != NULL);

  if (width && cell->width != -1)
    {
      real_width = NULL;
      *width = cell->width;
    }
  if (height && cell->height != -1)
    {
      real_height = NULL;
      *height = cell->height;
    }

  GTK_CELL_RENDERER_GET_CLASS (cell)->get_size (cell,
						widget,
						cell_area,
						x_offset,
						y_offset,
						real_width,
						real_height);
}

/**
 * gtk_cell_renderer_render:
 * @cell: a #GtkCellRenderer
 * @window: a #GdkDrawable to draw to
 * @widget: the widget owning @window
 * @background_area: entire cell area (including tree expanders and maybe padding on the sides)
 * @cell_area: area normally rendered by a cell renderer
 * @expose_area: area that actually needs updating
 * @flags: flags that affect rendering
 *
 * Invokes the virtual render function of the #GtkCellRenderer. The three
 * passed-in rectangles are areas of @window. Most renderers will draw within
 * @cell_area; the xalign, yalign, xpad, and ypad fields of the #GtkCellRenderer
 * should be honored with respect to @cell_area. @background_area includes the
 * blank space around the cell, and also the area containing the tree expander;
 * so the @background_area rectangles for all cells tile to cover the entire
 * @window.  @expose_area is a clip rectangle.
 *
 **/
void
gtk_cell_renderer_render (GtkCellRenderer     *cell,
			  GdkWindow           *window,
			  GtkWidget           *widget,
			  GdkRectangle        *background_area,
			  GdkRectangle        *cell_area,
			  GdkRectangle        *expose_area,
			  GtkCellRendererState flags)
{
  gboolean selected = FALSE;
  GtkCellRendererPrivate *priv = GTK_CELL_RENDERER_GET_PRIVATE (cell);

  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (GTK_CELL_RENDERER_GET_CLASS (cell)->render != NULL);

  selected = (flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED;

  if (cell->cell_background_set && !selected)
    {
      GdkColor color;
      GdkGC *gc;

      color.red = priv->cell_background.red;
      color.green = priv->cell_background.green;
      color.blue = priv->cell_background.blue;

      gc = gdk_gc_new (window);
      gdk_gc_set_rgb_fg_color (gc, &color);
      gdk_draw_rectangle (window, gc, TRUE,
                          background_area->x, background_area->y,
                          background_area->width, background_area->height);
      g_object_unref (gc);
    }

  GTK_CELL_RENDERER_GET_CLASS (cell)->render (cell,
					      window,
					      widget,
					      background_area,
					      cell_area,
					      expose_area,
					      flags);
}

/**
 * gtk_cell_renderer_activate:
 * @cell: a #GtkCellRenderer
 * @event: a #GdkEvent
 * @widget: widget that received the event
 * @path: widget-dependent string representation of the event location; e.g. for #GtkTreeView, a string representation of #GtkTreePath
 * @background_area: background area as passed to @gtk_cell_renderer_render
 * @cell_area: cell area as passed to @gtk_cell_renderer_render
 * @flags: render flags
 *
 * Passes an activate event to the cell renderer for possible processing.  Some
 * cell renderers may use events; for example, #GtkCellRendererToggle toggles
 * when it gets a mouse click.
 *
 * Return value: %TRUE if the event was consumed/handled
 **/
gboolean
gtk_cell_renderer_activate (GtkCellRenderer      *cell,
			    GdkEvent             *event,
			    GtkWidget            *widget,
			    const gchar          *path,
			    GdkRectangle         *background_area,
			    GdkRectangle         *cell_area,
			    GtkCellRendererState  flags)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), FALSE);

  if (cell->mode != GTK_CELL_RENDERER_MODE_ACTIVATABLE)
    return FALSE;

  if (GTK_CELL_RENDERER_GET_CLASS (cell)->activate == NULL)
    return FALSE;

  return GTK_CELL_RENDERER_GET_CLASS (cell)->activate (cell,
						       event,
						       widget,
						       path,
						       background_area,
						       cell_area,
						       flags);
}

/**
 * gtk_cell_renderer_start_editing:
 * @cell: a #GtkCellRenderer
 * @event: a #GdkEvent
 * @widget: widget that received the event
 * @path: widget-dependent string representation of the event location; e.g. for #GtkTreeView, a string representation of #GtkTreePath
 * @background_area: background area as passed to @gtk_cell_renderer_render
 * @cell_area: cell area as passed to @gtk_cell_renderer_render
 * @flags: render flags
 * 
 * Passes an activate event to the cell renderer for possible processing.
 * 
 * Return value: A new #GtkCellEditable, or %NULL
 **/
GtkCellEditable *
gtk_cell_renderer_start_editing (GtkCellRenderer      *cell,
				 GdkEvent             *event,
				 GtkWidget            *widget,
				 const gchar          *path,
				 GdkRectangle         *background_area,
				 GdkRectangle         *cell_area,
				 GtkCellRendererState  flags)

{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER (cell), NULL);

  if (cell->mode != GTK_CELL_RENDERER_MODE_EDITABLE)
    return NULL;

  if (GTK_CELL_RENDERER_GET_CLASS (cell)->start_editing == NULL)
    return NULL;

  
  return GTK_CELL_RENDERER_GET_CLASS (cell)->start_editing (cell,
							    event,
							    widget,
							    path,
							    background_area,
							    cell_area,
							    flags);
}

/**
 * gtk_cell_renderer_set_fixed_size:
 * @cell: A #GtkCellRenderer
 * @width: the width of the cell renderer, or -1
 * @height: the height of the cell renderer, or -1
 * 
 * Sets the renderer size to be explicit, independent of the properties set.
 **/
void
gtk_cell_renderer_set_fixed_size (GtkCellRenderer *cell,
				  gint             width,
				  gint             height)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (width >= -1 && height >= -1);

  if ((width != cell->width) || (height != cell->height))
    {
      g_object_freeze_notify (G_OBJECT (cell));

      if (width != cell->width)
        {
          cell->width = width;
          g_object_notify (G_OBJECT (cell), "width");
        }

      if (height != cell->height)
        {
          cell->height = height;
          g_object_notify (G_OBJECT (cell), "height");
        }

      g_object_thaw_notify (G_OBJECT (cell));
    }
}

/**
 * gtk_cell_renderer_get_fixed_size:
 * @cell: A #GtkCellRenderer
 * @width: location to fill in with the fixed width of the widget, or %NULL
 * @height: location to fill in with the fixed height of the widget, or %NULL
 * 
 * Fills in @width and @height with the appropriate size of @cell.
 **/
void
gtk_cell_renderer_get_fixed_size (GtkCellRenderer *cell,
				  gint            *width,
				  gint            *height)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  if (width)
    (* width) = cell->width;
  if (height)
    (* height) = cell->height;
}

/**
 * gtk_cell_renderer_editing_canceled:
 * @cell: A #GtkCellRenderer
 * 
 * Causes the cell renderer to emit the "editing-canceled" signal.  This
 * function is for use only by implementations of cell renderers that need to
 * notify the client program that an editing process was canceled and the
 * changes were not committed.
 *
 * Since: 2.4
 **/
void
gtk_cell_renderer_editing_canceled (GtkCellRenderer *cell)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  g_signal_emit (cell, cell_renderer_signals[EDITING_CANCELED], 0);
}
