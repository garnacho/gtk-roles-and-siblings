/* gtkcellrenderertoggle.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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
#include <stdlib.h>
#include "gtkcellrenderertoggle.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtktreeprivate.h"

static void gtk_cell_renderer_toggle_get_property  (GObject                    *object,
						    guint                       param_id,
						    GValue                     *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_toggle_set_property  (GObject                    *object,
						    guint                       param_id,
						    const GValue               *value,
						    GParamSpec                 *pspec);
static void gtk_cell_renderer_toggle_init       (GtkCellRendererToggle      *celltext);
static void gtk_cell_renderer_toggle_class_init (GtkCellRendererToggleClass *class);
static void gtk_cell_renderer_toggle_get_size   (GtkCellRenderer            *cell,
						 GtkWidget                  *widget,
 						 GdkRectangle               *cell_area,
						 gint                       *x_offset,
						 gint                       *y_offset,
						 gint                       *width,
						 gint                       *height);
static void gtk_cell_renderer_toggle_render     (GtkCellRenderer            *cell,
						 GdkWindow                  *window,
						 GtkWidget                  *widget,
						 GdkRectangle               *background_area,
						 GdkRectangle               *cell_area,
						 GdkRectangle               *expose_area,
						 GtkCellRendererState        flags);
static gboolean gtk_cell_renderer_toggle_activate  (GtkCellRenderer            *cell,
						    GdkEvent                   *event,
						    GtkWidget                  *widget,
						    const gchar                *path,
						    GdkRectangle               *background_area,
						    GdkRectangle               *cell_area,
						    GtkCellRendererState        flags);


enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_ZERO,
  PROP_ACTIVATABLE,
  PROP_ACTIVE,
  PROP_RADIO,
  PROP_INCONSISTENT
};

#define TOGGLE_WIDTH 12

static guint toggle_cell_signals[LAST_SIGNAL] = { 0 };

#define GTK_CELL_RENDERER_TOGGLE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_CELL_RENDERER_TOGGLE, GtkCellRendererTogglePrivate))

typedef struct _GtkCellRendererTogglePrivate GtkCellRendererTogglePrivate;
struct _GtkCellRendererTogglePrivate
{
  guint inconsistent : 1;
};


GType
gtk_cell_renderer_toggle_get_type (void)
{
  static GType cell_toggle_type = 0;

  if (!cell_toggle_type)
    {
      static const GTypeInfo cell_toggle_info =
      {
	sizeof (GtkCellRendererToggleClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_cell_renderer_toggle_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkCellRendererToggle),
	0,              /* n_preallocs */
	(GInstanceInitFunc) gtk_cell_renderer_toggle_init,
      };

      cell_toggle_type =
	g_type_register_static (GTK_TYPE_CELL_RENDERER, "GtkCellRendererToggle",
				&cell_toggle_info, 0);
    }

  return cell_toggle_type;
}

static void
gtk_cell_renderer_toggle_init (GtkCellRendererToggle *celltoggle)
{
  celltoggle->activatable = TRUE;
  celltoggle->active = FALSE;
  celltoggle->radio = FALSE;
  GTK_CELL_RENDERER (celltoggle)->mode = GTK_CELL_RENDERER_MODE_ACTIVATABLE;
  GTK_CELL_RENDERER (celltoggle)->xpad = 2;
  GTK_CELL_RENDERER (celltoggle)->ypad = 2;
}

static void
gtk_cell_renderer_toggle_class_init (GtkCellRendererToggleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->get_property = gtk_cell_renderer_toggle_get_property;
  object_class->set_property = gtk_cell_renderer_toggle_set_property;

  cell_class->get_size = gtk_cell_renderer_toggle_get_size;
  cell_class->render = gtk_cell_renderer_toggle_render;
  cell_class->activate = gtk_cell_renderer_toggle_activate;
  
  g_object_class_install_property (object_class,
				   PROP_ACTIVE,
				   g_param_spec_boolean ("active",
							 P_("Toggle state"),
							 P_("The toggle state of the button"),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
		                   PROP_INCONSISTENT,
				   g_param_spec_boolean ("inconsistent",
					                 P_("Inconsistent state"),
							 P_("The inconsistent state of the button"),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
				   PROP_ACTIVATABLE,
				   g_param_spec_boolean ("activatable",
							 P_("Activatable"),
							 P_("The toggle button can be activated"),
							 TRUE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_RADIO,
				   g_param_spec_boolean ("radio",
							 P_("Radio state"),
							 P_("Draw the toggle button as a radio button"),
							 FALSE,
							 G_PARAM_READABLE |
							 G_PARAM_WRITABLE));


  toggle_cell_signals[TOGGLED] =
    g_signal_new ("toggled",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkCellRendererToggleClass, toggled),
		  NULL, NULL,
		  _gtk_marshal_VOID__STRING,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);

  g_type_class_add_private (object_class, sizeof (GtkCellRendererTogglePrivate));
}

static void
gtk_cell_renderer_toggle_get_property (GObject     *object,
				       guint        param_id,
				       GValue      *value,
				       GParamSpec  *pspec)
{
  GtkCellRendererToggle *celltoggle = GTK_CELL_RENDERER_TOGGLE (object);
  GtkCellRendererTogglePrivate *priv;

  priv = GTK_CELL_RENDERER_TOGGLE_GET_PRIVATE (object);
  
  switch (param_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, celltoggle->active);
      break;
    case PROP_INCONSISTENT:
      g_value_set_boolean (value, priv->inconsistent);
      break;
    case PROP_ACTIVATABLE:
      g_value_set_boolean (value, celltoggle->activatable);
      break;
    case PROP_RADIO:
      g_value_set_boolean (value, celltoggle->radio);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
gtk_cell_renderer_toggle_set_property (GObject      *object,
				       guint         param_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
  GtkCellRendererToggle *celltoggle = GTK_CELL_RENDERER_TOGGLE (object);
  GtkCellRendererTogglePrivate *priv;

  priv = GTK_CELL_RENDERER_TOGGLE_GET_PRIVATE (object);
  
  switch (param_id)
    {
    case PROP_ACTIVE:
      celltoggle->active = g_value_get_boolean (value);
      g_object_notify (G_OBJECT(object), "active");
      break;
    case PROP_INCONSISTENT:
      priv->inconsistent = g_value_get_boolean (value);
      g_object_notify (G_OBJECT (object), "inconsistent");
      break;
    case PROP_ACTIVATABLE:
      celltoggle->activatable = g_value_get_boolean (value);
      g_object_notify (G_OBJECT(object), "activatable");
      break;
    case PROP_RADIO:
      celltoggle->radio = g_value_get_boolean (value);
      g_object_notify (G_OBJECT(object), "radio");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * gtk_cell_renderer_toggle_new:
 * 
 * Creates a new #GtkCellRendererToggle. Adjust rendering
 * parameters using object properties. Object properties can be set
 * globally (with g_object_set()). Also, with #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel. For example, you
 * can bind the "active" property on the cell renderer to a boolean value
 * in the model, thus causing the check button to reflect the state of
 * the model.
 * 
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
gtk_cell_renderer_toggle_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_TOGGLE, NULL);
}

static void
gtk_cell_renderer_toggle_get_size (GtkCellRenderer *cell,
				   GtkWidget       *widget,
				   GdkRectangle    *cell_area,
				   gint            *x_offset,
				   gint            *y_offset,
				   gint            *width,
				   gint            *height)
{
  gint calc_width;
  gint calc_height;

  calc_width = (gint) cell->xpad * 2 + TOGGLE_WIDTH;
  calc_height = (gint) cell->ypad * 2 + TOGGLE_WIDTH;

  if (width)
    *width = calc_width;

  if (height)
    *height = calc_height;

  if (cell_area)
    {
      if (x_offset)
	{
	  *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
		       (1.0 - cell->xalign) : cell->xalign) * (cell_area->width - calc_width);
	  *x_offset = MAX (*x_offset, 0);
	}
      if (y_offset)
	{
	  *y_offset = cell->yalign * (cell_area->height - calc_height);
	  *y_offset = MAX (*y_offset, 0);
	}
    }
}

static void
gtk_cell_renderer_toggle_render (GtkCellRenderer      *cell,
				 GdkDrawable          *window,
				 GtkWidget            *widget,
				 GdkRectangle         *background_area,
				 GdkRectangle         *cell_area,
				 GdkRectangle         *expose_area,
				 GtkCellRendererState  flags)
{
  GtkCellRendererToggle *celltoggle = (GtkCellRendererToggle *) cell;
  GtkCellRendererTogglePrivate *priv;
  gint width, height;
  gint x_offset, y_offset;
  GtkShadowType shadow;
  GtkStateType state = 0;

  priv = GTK_CELL_RENDERER_TOGGLE_GET_PRIVATE (cell);

  gtk_cell_renderer_toggle_get_size (cell, widget, cell_area,
				     &x_offset, &y_offset,
				     &width, &height);
  width -= cell->xpad*2;
  height -= cell->ypad*2;

  if (width <= 0 || height <= 0)
    return;

  if (priv->inconsistent)
    shadow = GTK_SHADOW_ETCHED_IN;
  else
    shadow = celltoggle->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
	state = GTK_STATE_SELECTED;
      else
	state = GTK_STATE_ACTIVE;
    }
  else
    {
      if (celltoggle->activatable)
        state = GTK_STATE_NORMAL;
      else
        state = GTK_STATE_INSENSITIVE;
    }

  if (celltoggle->radio)
    {
      gtk_paint_option (widget->style,
                        window,
                        state, shadow,
                        expose_area, widget, "cellradio",
                        cell_area->x + x_offset + cell->xpad,
                        cell_area->y + y_offset + cell->ypad,
                        width - 1, height - 1);
    }
  else
    {
      gtk_paint_check (widget->style,
                       window,
                       state, shadow,
                       expose_area, widget, "cellcheck",
                       cell_area->x + x_offset + cell->xpad,
                       cell_area->y + y_offset + cell->ypad,
                       width - 1, height - 1);
    }
}

static gint
gtk_cell_renderer_toggle_activate (GtkCellRenderer      *cell,
				   GdkEvent             *event,
				   GtkWidget            *widget,
				   const gchar          *path,
				   GdkRectangle         *background_area,
				   GdkRectangle         *cell_area,
				   GtkCellRendererState  flags)
{
  GtkCellRendererToggle *celltoggle;
  
  celltoggle = GTK_CELL_RENDERER_TOGGLE (cell);
  if (celltoggle->activatable)
    {
      g_signal_emit (cell, toggle_cell_signals[TOGGLED], 0, path);
      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_cell_renderer_toggle_set_radio:
 * @toggle: a #GtkCellRendererToggle
 * @radio: %TRUE to make the toggle look like a radio button
 * 
 * If @radio is %TRUE, the cell renderer renders a radio toggle
 * (i.e. a toggle in a group of mutually-exclusive toggles).
 * If %FALSE, it renders a check toggle (a standalone boolean option).
 * This can be set globally for the cell renderer, or changed just
 * before rendering each cell in the model (for #GtkTreeView, you set
 * up a per-row setting using #GtkTreeViewColumn to associate model
 * columns with cell renderer properties).
 **/
void
gtk_cell_renderer_toggle_set_radio (GtkCellRendererToggle *toggle,
				    gboolean               radio)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle));

  toggle->radio = radio;
}

/**
 * gtk_cell_renderer_toggle_get_radio:
 * @toggle: a #GtkCellRendererToggle
 *
 * Returns wether we're rendering radio toggles rather than checkboxes. 
 * 
 * Return value: %TRUE if we're rendering radio toggles rather than checkboxes
 **/
gboolean
gtk_cell_renderer_toggle_get_radio (GtkCellRendererToggle *toggle)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle), FALSE);

  return toggle->radio;
}

/**
 * gtk_cell_renderer_toggle_get_active:
 * @toggle: a #GtkCellRendererToggle
 *
 * Returns whether the cell renderer is active. See
 * gtk_cell_renderer_toggle_set_active().
 *
 * Return value: %TRUE if the cell renderer is active.
 **/
gboolean
gtk_cell_renderer_toggle_get_active (GtkCellRendererToggle *toggle)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle), FALSE);

  return toggle->active;
}

/**
 * gtk_cell_renderer_toggle_set_active:
 * @toggle: a #GtkCellRendererToggle.
 * @setting: the value to set.
 *
 * Activates or deactivates a cell renderer.
 **/
void
gtk_cell_renderer_toggle_set_active (GtkCellRendererToggle *toggle,
				     gboolean               setting)
{
  g_return_if_fail (GTK_IS_CELL_RENDERER_TOGGLE (toggle));

  g_object_set (toggle, "active", setting ? TRUE : FALSE, NULL);
}
