/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
#include "gtksettings.h"
#include "gtkrc.h"
#include "gtkintl.h"
#include "gtkwidget.h"

typedef struct _GtkSettingsValuePrivate GtkSettingsValuePrivate;

typedef enum
{
  GTK_SETTINGS_SOURCE_DEFAULT,
  GTK_SETTINGS_SOURCE_RC_FILE,
  GTK_SETTINGS_SOURCE_APPLICATION
} GtkSettingsSource;

struct _GtkSettingsValuePrivate
{
  GtkSettingsValue public;
  GtkSettingsSource source;
};

struct _GtkSettingsPropertyValue
{
  GValue value;
  GtkSettingsSource source;
};

#ifdef GDK_WINDOWING_X11
#include <pango/pangoxft.h>
#include <gdk/x11/gdkx.h>
#endif

enum {
  PROP_0,
  PROP_DOUBLE_CLICK_TIME,
  PROP_DOUBLE_CLICK_DISTANCE,
  PROP_CURSOR_BLINK,
  PROP_CURSOR_BLINK_TIME,
  PROP_SPLIT_CURSOR,
  PROP_THEME_NAME,
  PROP_ICON_THEME_NAME,
  PROP_KEY_THEME_NAME,
  PROP_MENU_BAR_ACCEL,
  PROP_DND_DRAG_THRESHOLD,
  PROP_FONT_NAME,
  PROP_ICON_SIZES,
  PROP_XFT_ANTIALIAS,
  PROP_XFT_HINTING,
  PROP_XFT_HINTSTYLE,
  PROP_XFT_RGBA,
  PROP_XFT_DPI
};


/* --- prototypes --- */
static void	gtk_settings_init		 (GtkSettings		*settings);
static void	gtk_settings_class_init		 (GtkSettingsClass	*class);
static void	gtk_settings_finalize		 (GObject		*object);
static void	gtk_settings_get_property	 (GObject		*object,
						  guint			 property_id,
						  GValue		*value,
						  GParamSpec		*pspec);
static void	gtk_settings_set_property	 (GObject		*object,
						  guint			 property_id,
						  const GValue		*value,
						  GParamSpec		*pspec);
static void	gtk_settings_notify		 (GObject		*object,
						  GParamSpec		*pspec);
static guint	settings_install_property_parser (GtkSettingsClass      *class,
						  GParamSpec            *pspec,
						  GtkRcPropertyParser    parser);
static void    settings_update_double_click      (GtkSettings           *settings);


/* --- variables --- */
static gpointer		 parent_class = NULL;
static GQuark		 quark_property_parser = 0;
static GSList           *object_list = NULL;
static guint		 class_n_properties = 0;


/* --- functions --- */
GType
gtk_settings_get_type (void)
{
  static GType settings_type = 0;
  
  if (!settings_type)
    {
      static const GTypeInfo settings_info =
      {
	sizeof (GtkSettingsClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_settings_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkSettings),
	0,              /* n_preallocs */
	(GInstanceInitFunc) gtk_settings_init,
      };
      
      settings_type = g_type_register_static (G_TYPE_OBJECT, "GtkSettings",
					      &settings_info, 0);
    }
  
  return settings_type;
}

#ifdef GDK_WINDOWING_X11
static void
gtk_default_substitute (FcPattern *pattern,
			gpointer   data)
{
  GtkSettings *settings = data;
  gint antialias;
  gint hinting;
  char *rgba;
  char *hintstyle;
  gint dpi;
  FcValue v;
  
  g_object_get (G_OBJECT (settings),
		"gtk-xft-antialias", &antialias,
		"gtk-xft-hinting", &hinting,
		"gtk-xft-hintstyle", &hintstyle,
		"gtk-xft-rgba", &rgba,
		"gtk-xft-dpi", &dpi,
		NULL);
  
  if (antialias >= 0 &&
      FcPatternGet (pattern, FC_ANTIALIAS, 0, &v) == FcResultNoMatch)
    FcPatternAddBool (pattern, FC_ANTIALIAS, antialias != 0);
  
  if (hinting >= 0 &&
      FcPatternGet (pattern, FC_HINTING, 0, &v) == FcResultNoMatch)
    FcPatternAddBool (pattern, FC_HINTING, hinting != 0);
 
#ifdef FC_HINT_STYLE 
  if (hintstyle && FcPatternGet (pattern, FC_HINT_STYLE, 0, &v) == FcResultNoMatch)
    {
      int val = FC_HINT_FULL;	/* Quiet GCC */
      gboolean found = TRUE;

      if (strcmp (hintstyle, "hintnone") == 0)
	val = FC_HINT_NONE;
      else if (strcmp (hintstyle, "hintslight") == 0)
	val = FC_HINT_SLIGHT;
      else if (strcmp (hintstyle, "hintmedium") == 0)
	val = FC_HINT_MEDIUM;
      else if (strcmp (hintstyle, "hintfull") == 0)
	val = FC_HINT_FULL;
      else
	found = FALSE;

      if (found)
	FcPatternAddInteger (pattern, FC_HINT_STYLE, val);
    }
#endif /* FC_HINT_STYLE */

  if (rgba && FcPatternGet (pattern, FC_RGBA, 0, &v) == FcResultNoMatch)
    {
      int val = FC_RGBA_NONE;	/* Quiet GCC */
      gboolean found = TRUE;

      if (strcmp (rgba, "none") == 0)
	val = FC_RGBA_NONE;
      else if (strcmp (rgba, "rgb") == 0)
	val = FC_RGBA_RGB;
      else if (strcmp (rgba, "bgr") == 0)
	val = FC_RGBA_BGR;
      else if (strcmp (rgba, "vrgb") == 0)
	val = FC_RGBA_VRGB;
      else if (strcmp (rgba, "vbgr") == 0)
	val = FC_RGBA_VBGR;
      else
	found = FALSE;

      if (found)
	FcPatternAddInteger (pattern, FC_RGBA, val);
    }

  if (dpi >= 0 && FcPatternGet (pattern, FC_DPI, 0, &v) == FcResultNoMatch)
    FcPatternAddDouble (pattern, FC_DPI, dpi / 1024.);

  g_free (hintstyle);
  g_free (rgba);
}
#endif /* GDK_WINDOWING_X11 */

static void
gtk_settings_init (GtkSettings *settings)
{
  GParamSpec **pspecs, **p;
  guint i = 0;
  
  g_datalist_init (&settings->queued_settings);
  object_list = g_slist_prepend (object_list, settings);

  /* build up property array for all yet existing properties and queue
   * notification for them (at least notification for internal properties
   * will instantly be caught)
   */
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (settings), NULL);
  for (p = pspecs; *p; p++)
    if ((*p)->owner_type == G_OBJECT_TYPE (settings))
      i++;
  settings->property_values = g_new0 (GtkSettingsPropertyValue, i);
  i = 0;
  g_object_freeze_notify (G_OBJECT (settings));
  for (p = pspecs; *p; p++)
    {
      GParamSpec *pspec = *p;

      if (pspec->owner_type != G_OBJECT_TYPE (settings))
	continue;
      g_value_init (&settings->property_values[i].value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_param_value_set_default (pspec, &settings->property_values[i].value);
      g_object_notify (G_OBJECT (settings), pspec->name);
      settings->property_values[i].source = GTK_SETTINGS_SOURCE_DEFAULT;
      i++;
    }
  g_object_thaw_notify (G_OBJECT (settings));
  g_free (pspecs);
}

static void
gtk_settings_class_init (GtkSettingsClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  guint result;
  
  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_settings_finalize;
  gobject_class->get_property = gtk_settings_get_property;
  gobject_class->set_property = gtk_settings_set_property;
  gobject_class->notify = gtk_settings_notify;

  quark_property_parser = g_quark_from_static_string ("gtk-rc-property-parser");

  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-double-click-time",
                                                               P_("Double Click Time"),
                                                               P_("Maximum time allowed between two clicks for them to be considered a double click (in milliseconds)"),
                                                               0, G_MAXINT, 250,
                                                               G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_DOUBLE_CLICK_TIME);
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-double-click-distance",
                                                               P_("Double Click Distance"),
                                                               P_("Maximum distance allowed between two clicks for them to be considered a double click (in pixels)"),
                                                               0, G_MAXINT, 5,
                                                               G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_DOUBLE_CLICK_DISTANCE);
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-cursor-blink",
								   P_("Cursor Blink"),
								   P_("Whether the cursor should blink"),
								   TRUE,
								   G_PARAM_READWRITE),
					     NULL);
  g_assert (result == PROP_CURSOR_BLINK);
  result = settings_install_property_parser (class,
                                             g_param_spec_int ("gtk-cursor-blink-time",
                                                               P_("Cursor Blink Time"),
                                                               P_("Length of the cursor blink cycle, in milleseconds"),
                                                               100, G_MAXINT, 1200,
                                                               G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_CURSOR_BLINK_TIME);
  result = settings_install_property_parser (class,
                                             g_param_spec_boolean ("gtk-split-cursor",
								   P_("Split Cursor"),
								   P_("Whether two cursors should be displayed for mixed left-to-right and right-to-left text"),
								   TRUE,
								   G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_SPLIT_CURSOR);
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-theme-name",
								   P_("Theme Name"),
								   P_("Name of theme RC file to load"),
								  "Default",
								  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_THEME_NAME);
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-icon-theme-name",
								  P_("Icon Theme Name"),
								  P_("Name of icon theme to use"),
								  "hicolor",
								  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ICON_THEME_NAME);
  
  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-key-theme-name",
								  P_("Key Theme Name"),
								  P_("Name of key theme RC file to load"),
								  NULL,
								  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_KEY_THEME_NAME);    

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-menu-bar-accel",
                                                                  P_("Menu bar accelerator"),
                                                                  P_("Keybinding to activate the menu bar"),
                                                                  "F10",
                                                                  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_MENU_BAR_ACCEL);

  result = settings_install_property_parser (class,
					     g_param_spec_int ("gtk-dnd-drag-threshold",
							       P_("Drag threshold"),
							       P_("Number of pixels the cursor can move before dragging"),
							       1, G_MAXINT, 8,
                                                               G_PARAM_READWRITE),
					     NULL);
  g_assert (result == PROP_DND_DRAG_THRESHOLD);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-font-name",
								   P_("Font Name"),
								   P_("Name of default font to use"),
								  "Sans 10",
								  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_FONT_NAME);

  result = settings_install_property_parser (class,
                                             g_param_spec_string ("gtk-icon-sizes",
								   P_("Icon Sizes"),
								   P_("List of icon sizes (gtk-menu=16,16;gtk-button=20,20..."),
								  NULL,
								  G_PARAM_READWRITE),
                                             NULL);
  g_assert (result == PROP_ICON_SIZES);

#ifdef GDK_WINDOWING_X11
  result = settings_install_property_parser (class,
					     g_param_spec_int ("gtk-xft-antialias",
 							       P_("Xft Antialias"),
 							       P_("Whether to antialias Xft fonts; 0=no, 1=yes, -1=default"),
 							       -1, 1, -1,
 							       G_PARAM_READWRITE),
					     NULL);
 
  g_assert (result == PROP_XFT_ANTIALIAS);
  
  result = settings_install_property_parser (class,
					     g_param_spec_int ("gtk-xft-hinting",
 							       P_("Xft Hinting"),
 							       P_("Whether to hint Xft fonts; 0=no, 1=yes, -1=default"),
 							       -1, 1, -1,
 							       G_PARAM_READWRITE),
					     NULL);
  
  g_assert (result == PROP_XFT_HINTING);
  
  result = settings_install_property_parser (class,
					     g_param_spec_string ("gtk-xft-hintstyle",
 								  P_("Xft Hint Style"),
 								  P_("What degree of hinting to use; none, slight, medium, or full"),
 								  NULL,
 								  G_PARAM_READWRITE),
                                              NULL);
  
  g_assert (result == PROP_XFT_HINTSTYLE);
  
  result = settings_install_property_parser (class,
					     g_param_spec_string ("gtk-xft-rgba",
 								  P_("Xft RGBA"),
 								  P_("Type of subpixel antialiasing; none, rgb, bgr, vrgb, vbgr"),
 								  NULL,
 								  G_PARAM_READWRITE),
					     NULL);
  
  g_assert (result == PROP_XFT_RGBA);
  
  result = settings_install_property_parser (class,
					     g_param_spec_int ("gtk-xft-dpi",
 							       P_("Xft DPI"),
 							       P_("Resolution for Xft, in 1024 * dots/inch. -1 to use default value"),
 							       -1, 1024*1024, -1,
 							       G_PARAM_READWRITE),
					     NULL);
  
  g_assert (result == PROP_XFT_DPI);
#endif  /* GDK_WINDOWING_X11 */
}

static void
gtk_settings_finalize (GObject *object)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  guint i;

  object_list = g_slist_remove (object_list, settings);

  for (i = 0; i < class_n_properties; i++)
    g_value_unset (&settings->property_values[i].value);
  g_free (settings->property_values);

  g_datalist_clear (&settings->queued_settings);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtk_settings_get_for_screen:
 * @screen : a #GdkScreen.
 * 
 * Gets the #GtkSettings object for @screen, creating it if necessary.
 *
 * Return value: a #GtkSettings object.
 *
 * Since: 2.2
 */
GtkSettings*
gtk_settings_get_for_screen (GdkScreen *screen)
{
  GtkSettings *settings;
  
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  
  settings = g_object_get_data (G_OBJECT (screen), "gtk-settings");
  if (!settings)
    {
      settings = g_object_new (GTK_TYPE_SETTINGS, NULL);
      settings->screen = screen;
      g_object_set_data (G_OBJECT (screen), "gtk-settings", settings);

#ifdef GDK_WINDOWING_X11  
      /* Set the default substitution function for the Pango fontmap.
       */
      pango_xft_set_default_substitute (GDK_SCREEN_XDISPLAY (screen),
					GDK_SCREEN_XNUMBER (screen),
					gtk_default_substitute,
					settings, NULL);
#endif /* GDK_WINDOWING_X11 */
      
      gtk_rc_reparse_all_for_settings (settings, TRUE);
      settings_update_double_click (settings);
    }
  
  return settings;
}

/**
 * gtk_settings_get_default:
 * 
 * Gets the #GtkSettings object for the default GDK screen, creating
 * it if necessary. See gtk_settings_get_for_screen().
 * 
 * Return value: a #GtkSettings object. If there is no default
 *  screen, then returns %NULL.
 **/
GtkSettings*
gtk_settings_get_default (void)
{
  GdkScreen *screen = gdk_screen_get_default ();

  if (screen)
    return gtk_settings_get_for_screen (screen);
  else
    return NULL;
}

static void
gtk_settings_set_property (GObject      *object,
			   guint	 property_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);

  g_value_copy (value, &settings->property_values[property_id - 1].value);
  settings->property_values[property_id - 1].source = GTK_SETTINGS_SOURCE_APPLICATION;
}

static void
gtk_settings_get_property (GObject     *object,
			   guint	property_id,
			   GValue      *value,
			   GParamSpec  *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  GType value_type = G_VALUE_TYPE (value);
  GType fundamental_type = G_TYPE_FUNDAMENTAL (value_type);

  /* For enums and strings, we need to get the value as a string,
   * not as an int, since we support using names/nicks as the setting
   * value.
   */
  if ((g_value_type_transformable (G_TYPE_INT, value_type) &&
       !(fundamental_type == G_TYPE_ENUM || fundamental_type == G_TYPE_FLAGS)) ||
      g_value_type_transformable (G_TYPE_STRING, G_VALUE_TYPE (value)) ||
      g_value_type_transformable (GDK_TYPE_COLOR, G_VALUE_TYPE (value)))
    {
      if (settings->property_values[property_id - 1].source == GTK_SETTINGS_SOURCE_APPLICATION ||
	  !gdk_screen_get_setting (settings->screen, pspec->name, value))
        g_value_copy (&settings->property_values[property_id - 1].value, value);
      else
        g_param_value_validate (pspec, value);
    }
  else
    {
      GValue val = { 0, };

      /* Try to get xsetting as a string and parse it. */
      
      g_value_init (&val, G_TYPE_STRING);

      if (settings->property_values[property_id - 1].source == GTK_SETTINGS_SOURCE_APPLICATION ||
	  !gdk_screen_get_setting (settings->screen, pspec->name, &val))
        {
          g_value_copy (&settings->property_values[property_id - 1].value, value);
        }
      else
        {
          GValue tmp_value = { 0, };
          GValue gstring_value = { 0, };
          GtkRcPropertyParser parser = (GtkRcPropertyParser) g_param_spec_get_qdata (pspec, quark_property_parser);
          
          g_value_init (&gstring_value, G_TYPE_GSTRING);

          g_value_set_boxed (&gstring_value,
                             g_string_new (g_value_get_string (&val)));

          g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));

          if (parser && _gtk_settings_parse_convert (parser, &gstring_value,
                                                     pspec, &tmp_value))
            {
              g_value_copy (&tmp_value, value);
              g_param_value_validate (pspec, value);
            }
          else
            {
              g_value_copy (&settings->property_values[property_id - 1].value, value);
            }

          g_value_unset (&gstring_value);
          g_value_unset (&tmp_value);
        }

      g_value_unset (&val);
    }
}

static void
gtk_settings_notify (GObject    *object,
		     GParamSpec *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  guint property_id = pspec->param_id;

  if (settings->screen == NULL) /* initialization */
    return;

  switch (property_id)
    {
    case PROP_DOUBLE_CLICK_TIME:
    case PROP_DOUBLE_CLICK_DISTANCE:
      settings_update_double_click (settings);
      break;
#ifdef GDK_WINDOWING_X11      
    case PROP_XFT_ANTIALIAS:
    case PROP_XFT_HINTING:
    case PROP_XFT_HINTSTYLE:
    case PROP_XFT_RGBA:
    case PROP_XFT_DPI:
      pango_xft_substitute_changed (GDK_SCREEN_XDISPLAY (settings->screen),
 				    GDK_SCREEN_XNUMBER (settings->screen));
      /* This is a hack because with gtk_rc_reset_styles() doesn't get
       * widgets with gtk_widget_style_set(), and also causes more
       * recomputation than necessary.
       */
      gtk_rc_reset_styles (GTK_SETTINGS (object));
      break;
#endif /* GDK_WINDOWING_X11 */
    }
}

gboolean
_gtk_settings_parse_convert (GtkRcPropertyParser parser,
			     const GValue       *src_value,
			     GParamSpec         *pspec,
			     GValue	        *dest_value)
{
  gboolean success = FALSE;

  g_return_val_if_fail (G_VALUE_HOLDS (dest_value, G_PARAM_SPEC_VALUE_TYPE (pspec)), FALSE);

  if (parser)
    {
      GString *gstring;
      gboolean free_gstring = TRUE;
      
      if (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING))
	{
	  gstring = g_value_get_boxed (src_value);
	  free_gstring = FALSE;
	}
      else if (G_VALUE_HOLDS_LONG (src_value))
	{
	  gstring = g_string_new (NULL);
	  g_string_append_printf (gstring, "%ld", g_value_get_long (src_value));
	}
      else if (G_VALUE_HOLDS_DOUBLE (src_value))
	{
	  gstring = g_string_new (NULL);
	  g_string_append_printf (gstring, "%f", g_value_get_double (src_value));
	}
      else if (G_VALUE_HOLDS_STRING (src_value))
	{
	  gchar *tstr = g_strescape (g_value_get_string (src_value), NULL);
	  
	  gstring = g_string_new ("\"");
	  g_string_append (gstring, tstr);
	  g_string_append_c (gstring, '\"');
	  g_free (tstr);
	}
      else
	{
	  g_return_val_if_fail (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING), FALSE);
	  gstring = NULL; /* silence compiler */
	}

      success = (parser (pspec, gstring, dest_value) &&
		 !g_param_value_validate (pspec, dest_value));

      if (free_gstring)
	g_string_free (gstring, TRUE);
    }
  else if (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING))
    {
      if (G_VALUE_HOLDS (dest_value, G_TYPE_STRING))
	{
	  GString *gstring = g_value_get_boxed (src_value);

	  g_value_set_string (dest_value, gstring ? gstring->str : NULL);
	  success = !g_param_value_validate (pspec, dest_value);
	}
    }
  else if (g_value_type_transformable (G_VALUE_TYPE (src_value), G_VALUE_TYPE (dest_value)))
    success = g_param_value_convert (pspec, src_value, dest_value, TRUE);

  return success;
}

static void
apply_queued_setting (GtkSettings             *data,
		      GParamSpec              *pspec,
		      GtkSettingsValuePrivate *qvalue)
{
  GValue tmp_value = { 0, };
  GtkRcPropertyParser parser = (GtkRcPropertyParser) g_param_spec_get_qdata (pspec, quark_property_parser);

  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (_gtk_settings_parse_convert (parser, &qvalue->public.value,
				   pspec, &tmp_value))
    {
      if (data->property_values[pspec->param_id - 1].source <= qvalue->source)
	{
	  g_object_set_property (G_OBJECT (data), pspec->name, &tmp_value);
	  data->property_values[pspec->param_id - 1].source = qvalue->source;
	}
    }
  else
    {
      gchar *debug = g_strdup_value_contents (&qvalue->public.value);
      
      g_message ("%s: failed to retrieve property `%s' of type `%s' from rc file value \"%s\" of type `%s'",
		 qvalue->public.origin,
		 pspec->name,
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		 debug,
		 G_VALUE_TYPE_NAME (&tmp_value));
      g_free (debug);
    }
  g_value_unset (&tmp_value);
}

static guint
settings_install_property_parser (GtkSettingsClass   *class,
				  GParamSpec         *pspec,
				  GtkRcPropertyParser parser)
{
  GSList *node, *next;

  switch (G_TYPE_FUNDAMENTAL (G_PARAM_SPEC_VALUE_TYPE (pspec)))
    {
    case G_TYPE_BOOLEAN:
    case G_TYPE_UCHAR:
    case G_TYPE_CHAR:
    case G_TYPE_UINT:
    case G_TYPE_INT:
    case G_TYPE_ULONG:
    case G_TYPE_LONG:
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
    case G_TYPE_STRING:
      break;
    default:
      if (!parser)
        {
          g_warning (G_STRLOC ": parser needs to be specified for property \"%s\" of type `%s'",
                     pspec->name, g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
          return 0;
        }
    }
  if (g_object_class_find_property (G_OBJECT_CLASS (class), pspec->name))
    {
      g_warning (G_STRLOC ": an rc-data property \"%s\" already exists",
		 pspec->name);
      return 0;
    }
  
  for (node = object_list; node; node = node->next)
    g_object_freeze_notify (node->data);

  g_object_class_install_property (G_OBJECT_CLASS (class), ++class_n_properties, pspec);
  g_param_spec_set_qdata (pspec, quark_property_parser, (gpointer) parser);

  for (node = object_list; node; node = node->next)
    {
      GtkSettings *settings = node->data;
      GtkSettingsValuePrivate *qvalue;
      
      settings->property_values = g_renew (GtkSettingsPropertyValue, settings->property_values, class_n_properties);
      settings->property_values[class_n_properties - 1].value.g_type = 0;
      g_value_init (&settings->property_values[class_n_properties - 1].value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_param_value_set_default (pspec, &settings->property_values[class_n_properties - 1].value);
      settings->property_values[class_n_properties - 1].source = GTK_SETTINGS_SOURCE_DEFAULT;
      g_object_notify (G_OBJECT (settings), pspec->name);
      
      qvalue = g_datalist_get_data (&settings->queued_settings, pspec->name);
      if (qvalue)
	apply_queued_setting (settings, pspec, qvalue);
    }

  for (node = object_list; node; node = next)
    {
      next = node->next;
      g_object_thaw_notify (node->data);
    }

  return class_n_properties;
}

GtkRcPropertyParser
_gtk_rc_property_parser_from_type (GType type)
{
  if (type == GDK_TYPE_COLOR)
    return gtk_rc_property_parse_color;
  else if (type == GTK_TYPE_REQUISITION)
    return gtk_rc_property_parse_requisition;
  else if (type == GTK_TYPE_BORDER)
    return gtk_rc_property_parse_border;
  else if (G_TYPE_FUNDAMENTAL (type) == G_TYPE_ENUM && G_TYPE_IS_DERIVED (type))
    return gtk_rc_property_parse_enum;
  else if (G_TYPE_FUNDAMENTAL (type) == G_TYPE_FLAGS && G_TYPE_IS_DERIVED (type))
    return gtk_rc_property_parse_flags;
  else
    return NULL;
}

void
gtk_settings_install_property (GParamSpec *pspec)
{
  GtkRcPropertyParser parser;

  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  parser = _gtk_rc_property_parser_from_type (G_PARAM_SPEC_VALUE_TYPE (pspec));

  settings_install_property_parser (gtk_type_class (GTK_TYPE_SETTINGS), pspec, parser);
}

void
gtk_settings_install_property_parser (GParamSpec          *pspec,
				      GtkRcPropertyParser  parser)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (parser != NULL);
  
  settings_install_property_parser (gtk_type_class (GTK_TYPE_SETTINGS), pspec, parser);
}

static void
free_value (gpointer data)
{
  GtkSettingsValuePrivate *qvalue = data;
  
  g_value_unset (&qvalue->public.value);
  g_free (qvalue->public.origin);
  g_free (qvalue);
}

static void
gtk_settings_set_property_value_internal (GtkSettings            *settings,
					  const gchar            *prop_name,
					  const GtkSettingsValue *new_value,
					  GtkSettingsSource       source)
{
  GtkSettingsValuePrivate *qvalue;
  GParamSpec *pspec;
  gchar *name;
  GQuark name_quark;

  if (!G_VALUE_HOLDS_LONG (&new_value->value) &&
      !G_VALUE_HOLDS_DOUBLE (&new_value->value) &&
      !G_VALUE_HOLDS_STRING (&new_value->value) &&
      !G_VALUE_HOLDS (&new_value->value, G_TYPE_GSTRING))
    {
      g_warning (G_STRLOC ": value type invalid");
      return;
    }
  
  name = g_strdup (prop_name);
  g_strcanon (name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-", '-');
  name_quark = g_quark_from_string (name);
  g_free (name);

  qvalue = g_datalist_id_get_data (&settings->queued_settings, name_quark);
  if (!qvalue)
    {
      qvalue = g_new0 (GtkSettingsValuePrivate, 1);
      g_datalist_id_set_data_full (&settings->queued_settings, name_quark, qvalue, free_value);
    }
  else
    {
      g_free (qvalue->public.origin);
      g_value_unset (&qvalue->public.value);
    }
  qvalue->public.origin = g_strdup (new_value->origin);
  g_value_init (&qvalue->public.value, G_VALUE_TYPE (&new_value->value));
  g_value_copy (&new_value->value, &qvalue->public.value);
  qvalue->source = source;
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), g_quark_to_string (name_quark));
  if (pspec)
    apply_queued_setting (settings, pspec, qvalue);
}

void
gtk_settings_set_property_value (GtkSettings            *settings,
				 const gchar            *prop_name,
				 const GtkSettingsValue *new_value)
{
  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (prop_name != NULL);
  g_return_if_fail (new_value != NULL);
  g_return_if_fail (new_value->origin != NULL);

  gtk_settings_set_property_value_internal (settings, prop_name, new_value,
					    GTK_SETTINGS_SOURCE_APPLICATION);
}

void
_gtk_settings_set_property_value_from_rc (GtkSettings            *settings,
					  const gchar            *prop_name,
					  const GtkSettingsValue *new_value)
{
  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (prop_name != NULL);
  g_return_if_fail (new_value != NULL);
  g_return_if_fail (new_value->origin != NULL);

  gtk_settings_set_property_value_internal (settings, prop_name, new_value,
					    GTK_SETTINGS_SOURCE_RC_FILE);
}

void
gtk_settings_set_string_property (GtkSettings *settings,
				  const gchar *name,
				  const gchar *v_string,
				  const gchar *origin)
{
  GtkSettingsValue svalue = { NULL, { 0, }, };

  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);
  g_return_if_fail (v_string != NULL);
  g_return_if_fail (origin != NULL);

  svalue.origin = (gchar*) origin;
  g_value_init (&svalue.value, G_TYPE_STRING);
  g_value_set_static_string (&svalue.value, v_string);
  gtk_settings_set_property_value (settings, name, &svalue);
  g_value_unset (&svalue.value);
}

void
gtk_settings_set_long_property (GtkSettings *settings,
				const gchar *name,
				glong	     v_long,
				const gchar *origin)
{
  GtkSettingsValue svalue = { NULL, { 0, }, };
  
  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);
  g_return_if_fail (origin != NULL);

  svalue.origin = (gchar*) origin;
  g_value_init (&svalue.value, G_TYPE_LONG);
  g_value_set_long (&svalue.value, v_long);
  gtk_settings_set_property_value (settings, name, &svalue);
  g_value_unset (&svalue.value);
}

void
gtk_settings_set_double_property (GtkSettings *settings,
				  const gchar *name,
				  gdouble      v_double,
				  const gchar *origin)
{
  GtkSettingsValue svalue = { NULL, { 0, }, };

  g_return_if_fail (GTK_SETTINGS (settings));
  g_return_if_fail (name != NULL);
  g_return_if_fail (origin != NULL);

  svalue.origin = (gchar*) origin;
  g_value_init (&svalue.value, G_TYPE_DOUBLE);
  g_value_set_double (&svalue.value, v_double);
  gtk_settings_set_property_value (settings, name, &svalue);
  g_value_unset (&svalue.value);
}

/**
 * gtk_rc_property_parse_color:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold #GdkColor values.
 * 
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses a
 * color given either by its name or in the form 
 * <literal>{ red, green, blue }</literal> where %red, %green and
 * %blue are integers between 0 and 65535 or floating-point numbers
 * between 0 and 1.
 * 
 * Return value: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting #GdkColor.
 **/
gboolean
gtk_rc_property_parse_color (const GParamSpec *pspec,
			     const GString    *gstring,
			     GValue           *property_value)
{
  GdkColor color = { 0, 0, 0, 0, };
  GScanner *scanner;
  gboolean success;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS (property_value, GDK_TYPE_COLOR), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);
  if (gtk_rc_parse_color (scanner, &color) == G_TOKEN_NONE &&
      g_scanner_get_next_token (scanner) == G_TOKEN_EOF)
    {
      g_value_set_boxed (property_value, &color);
      success = TRUE;
    }
  else
    success = FALSE;
  g_scanner_destroy (scanner);

  return success;
}

/**
 * gtk_rc_property_parse_enum:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold enum values.
 * 
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses a single
 * enumeration value.
 *
 * The enumeration value can be specified by its name, its nickname or
 * its numeric value. For consistency with flags parsing, the value
 * may be surrounded by parentheses.
 * 
 * Return value: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting #GEnumValue.
 **/
gboolean
gtk_rc_property_parse_enum (const GParamSpec *pspec,
			    const GString    *gstring,
			    GValue           *property_value)
{
  gboolean need_closing_brace = FALSE, success = FALSE;
  GScanner *scanner;
  GEnumValue *enum_value = NULL;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_ENUM (property_value), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  /* we just want to parse _one_ value, but for consistency with flags parsing
   * we support optional parenthesis
   */
  g_scanner_get_next_token (scanner);
  if (scanner->token == '(')
    {
      need_closing_brace = TRUE;
      g_scanner_get_next_token (scanner);
    }
  if (scanner->token == G_TOKEN_IDENTIFIER)
    {
      GEnumClass *class = G_PARAM_SPEC_ENUM (pspec)->enum_class;
      
      enum_value = g_enum_get_value_by_name (class, scanner->value.v_identifier);
      if (!enum_value)
	enum_value = g_enum_get_value_by_nick (class, scanner->value.v_identifier);
      if (enum_value)
	{
	  g_value_set_enum (property_value, enum_value->value);
	  success = TRUE;
	}
    }
  else if (scanner->token == G_TOKEN_INT)
    {
      g_value_set_enum (property_value, scanner->value.v_int);
      success = TRUE;
    }
  if (need_closing_brace && g_scanner_get_next_token (scanner) != ')')
    success = FALSE;
  if (g_scanner_get_next_token (scanner) != G_TOKEN_EOF)
    success = FALSE;

  g_scanner_destroy (scanner);

  return success;
}

static guint
parse_flags_value (GScanner    *scanner,
		   GFlagsClass *class,
		   guint       *number)
{
  g_scanner_get_next_token (scanner);
  if (scanner->token == G_TOKEN_IDENTIFIER)
    {
      GFlagsValue *flags_value;

      flags_value = g_flags_get_value_by_name (class, scanner->value.v_identifier);
      if (!flags_value)
	flags_value = g_flags_get_value_by_nick (class, scanner->value.v_identifier);
      if (flags_value)
	{
	  *number |= flags_value->value;
	  return G_TOKEN_NONE;
	}
    }
  else if (scanner->token == G_TOKEN_INT)
    {
      *number |= scanner->value.v_int;
      return G_TOKEN_NONE;
    }
  return G_TOKEN_IDENTIFIER;
}

/**
 * gtk_rc_property_parse_flags:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold flags values.
 * 
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses flags. 
 * 
 * Flags can be specified by their name, their nickname or
 * numerically. Multiple flags can be specified in the form 
 * <literal>"( flag1 | flag2 | ... )"</literal>.
 * 
 * Return value: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting flags value.
 **/
gboolean
gtk_rc_property_parse_flags (const GParamSpec *pspec,
			     const GString    *gstring,
			     GValue           *property_value)
{
  GFlagsClass *class;
   gboolean success = FALSE;
  GScanner *scanner;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_FLAGS (property_value), FALSE);

  class = G_PARAM_SPEC_FLAGS (pspec)->flags_class;
  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  /* parse either a single flags value or a "\( ... [ \| ... ] \)" compound */
  if (g_scanner_peek_next_token (scanner) == G_TOKEN_IDENTIFIER ||
      scanner->next_token == G_TOKEN_INT)
    {
      guint token, flags_value = 0;
      
      token = parse_flags_value (scanner, class, &flags_value);

      if (token == G_TOKEN_NONE && g_scanner_peek_next_token (scanner) == G_TOKEN_EOF)
	{
	  success = TRUE;
	  g_value_set_flags (property_value, flags_value);
	}
      
    }
  else if (g_scanner_get_next_token (scanner) == '(')
    {
      guint token, flags_value = 0;

      /* parse first value */
      token = parse_flags_value (scanner, class, &flags_value);

      /* parse nth values, preceeded by '|' */
      while (token == G_TOKEN_NONE && g_scanner_get_next_token (scanner) == '|')
	token = parse_flags_value (scanner, class, &flags_value);

      /* done, last token must have closed expression */
      if (token == G_TOKEN_NONE && scanner->token == ')' &&
	  g_scanner_peek_next_token (scanner) == G_TOKEN_EOF)
	{
	  g_value_set_flags (property_value, flags_value);
	  success = TRUE;
	}
    }
  g_scanner_destroy (scanner);

  return success;
}

static gboolean
get_braced_int (GScanner *scanner,
		gboolean  first,
		gboolean  last,
		gint     *value)
{
  if (first)
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token != '{')
	return FALSE;
    }

  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_INT)
    return FALSE;

  *value = scanner->value.v_int;

  if (last)
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token != '}')
	return FALSE;
    }
  else
    {
      g_scanner_get_next_token (scanner);
      if (scanner->token != ',')
	return FALSE;
    }

  return TRUE;
}

/**
 * gtk_rc_property_parse_requisition:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold boxed values.
 * 
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses a
 * requisition in the form 
 * <literal>"{ width, height }"</literal> for integers %width and %height.
 * 
 * Return value: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting #GtkRequisition.
 **/
gboolean
gtk_rc_property_parse_requisition  (const GParamSpec *pspec,
				    const GString    *gstring,
				    GValue           *property_value)
{
  GtkRequisition requisition;
  GScanner *scanner;
  gboolean success = FALSE;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (property_value), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  if (get_braced_int (scanner, TRUE, FALSE, &requisition.width) &&
      get_braced_int (scanner, FALSE, TRUE, &requisition.height))
    {
      g_value_set_boxed (property_value, &requisition);
      success = TRUE;
    }

  g_scanner_destroy (scanner);

  return success;
}

/**
 * gtk_rc_property_parse_border:
 * @pspec: a #GParamSpec
 * @gstring: the #GString to be parsed
 * @property_value: a #GValue which must hold boxed values.
 * 
 * A #GtkRcPropertyParser for use with gtk_settings_install_property_parser()
 * or gtk_widget_class_install_style_property_parser() which parses
 * borders in the form 
 * <literal>"{ left, right, top, bottom }"</literal> for integers 
 * %left, %right, %top and %bottom.
 * 
 * Return value: %TRUE if @gstring could be parsed and @property_value
 * has been set to the resulting #GtkBorder.
 **/
gboolean
gtk_rc_property_parse_border (const GParamSpec *pspec,
			      const GString    *gstring,
			      GValue           *property_value)
{
  GtkBorder border;
  GScanner *scanner;
  gboolean success = FALSE;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_BOXED (property_value), FALSE);

  scanner = gtk_rc_scanner_new ();
  g_scanner_input_text (scanner, gstring->str, gstring->len);

  if (get_braced_int (scanner, TRUE, FALSE, &border.left) &&
      get_braced_int (scanner, FALSE, FALSE, &border.right) &&
      get_braced_int (scanner, FALSE, FALSE, &border.top) &&
      get_braced_int (scanner, FALSE, TRUE, &border.bottom))
    {
      g_value_set_boxed (property_value, &border);
      success = TRUE;
    }

  g_scanner_destroy (scanner);

  return success;
}

void
_gtk_settings_handle_event (GdkEventSetting *event)
{
  GtkSettings *settings = gtk_settings_get_for_screen (gdk_drawable_get_screen (event->window));
  
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (settings), event->name))
    g_object_notify (G_OBJECT (settings), event->name);
}

static void
reset_rc_values_foreach (GQuark    key_id,
			 gpointer  data,
			 gpointer  user_data)
{
  GtkSettingsValuePrivate *qvalue = data;
  GSList **to_reset = user_data;

  if (qvalue->source == GTK_SETTINGS_SOURCE_RC_FILE)
    *to_reset = g_slist_prepend (*to_reset, GUINT_TO_POINTER (key_id));
}

void
_gtk_settings_reset_rc_values (GtkSettings *settings)
{
  GSList *to_reset = NULL;
  GSList *tmp_list;
  GParamSpec **pspecs, **p;
  gint i;

  /* Remove any queued settings
   */
  g_datalist_foreach (&settings->queued_settings,
		      reset_rc_values_foreach,
		      &to_reset);

  for (tmp_list = to_reset; tmp_list; tmp_list = tmp_list->next)
    {
      GQuark key_id = GPOINTER_TO_UINT (tmp_list->data);
      g_datalist_id_remove_data (&settings->queued_settings, key_id);
    }

  /* Now reset the active settings
   */
  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (settings), NULL);
  i = 0;

  g_object_freeze_notify (G_OBJECT (settings));
  for (p = pspecs; *p; p++)
    {
      if (settings->property_values[i].source == GTK_SETTINGS_SOURCE_RC_FILE)
	{
	  GParamSpec *pspec = *p;

	  g_param_value_set_default (pspec, &settings->property_values[i].value);
	  g_object_notify (G_OBJECT (settings), pspec->name);
	}
      i++;
    }
  g_object_thaw_notify (G_OBJECT (settings));
  g_free (pspecs);
}

static void
settings_update_double_click (GtkSettings *settings)
{
  if (gdk_screen_get_number (settings->screen) == 0)
    {
      GdkDisplay *display = gdk_screen_get_display (settings->screen);
      gint double_click_time;
      gint double_click_distance;
  
      g_object_get (settings, 
		    "gtk-double-click-time", &double_click_time, 
		    "gtk-double-click-distance", &double_click_distance,
		    NULL);
      
      gdk_display_set_double_click_time (display, double_click_time);
      gdk_display_set_double_click_distance (display, double_click_distance);
    }
}
