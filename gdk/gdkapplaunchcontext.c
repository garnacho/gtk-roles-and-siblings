/* gdkapplaunchcontext.c - Gtk+ implementation for GAppLaunchContext

   Copyright (C) 2007 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>

#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gio/gfileicon.h>
#include <gio/gthemedicon.h>

#include "gdkapplaunchcontext.h"
#include "gdkinternals.h"
#include "gdkscreen.h"
#include "gdkintl.h"
#include "gdkalias.h"


G_DEFINE_TYPE (GdkAppLaunchContext, gdk_app_launch_context,
	       G_TYPE_APP_LAUNCH_CONTEXT);

static void
gdk_app_launch_context_finalize (GObject *object)
{
  GdkAppLaunchContext *context;
  GdkAppLaunchContextPrivate *priv;

  context = GDK_APP_LAUNCH_CONTEXT (object);

  priv = context->priv;

  if (priv->display)
    g_object_unref (priv->display);

  if (priv->screen)
    g_object_unref (priv->screen);

  if (priv->icon)
    g_object_unref (priv->icon);

  g_free (priv->icon_name);

  (*G_OBJECT_CLASS (gdk_app_launch_context_parent_class)->finalize) (object);
}

static char *
get_display (GAppLaunchContext *context, 
             GAppInfo          *info, 
             GList             *files)
{
  GdkDisplay *display;
  GdkAppLaunchContextPrivate *priv;

  priv = GDK_APP_LAUNCH_CONTEXT (context)->priv;

  if (priv->screen)
    return gdk_screen_make_display_name (priv->screen);

  if (priv->display)
    display = priv->display;
  else
    display = gdk_display_get_default ();

  return g_strdup (gdk_display_get_name (display));
}

static void
gdk_app_launch_context_class_init (GdkAppLaunchContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GAppLaunchContextClass *context_class = G_APP_LAUNCH_CONTEXT_CLASS (klass);

  gobject_class->finalize = gdk_app_launch_context_finalize;

  context_class->get_display = get_display;
  context_class->get_startup_notify_id = _gdk_windowing_get_startup_notify_id;
  context_class->launch_failed = _gdk_windowing_launch_failed;

  g_type_class_add_private (klass, sizeof (GdkAppLaunchContextPrivate));
}

static void
gdk_app_launch_context_init (GdkAppLaunchContext *context)
{
  context->priv = G_TYPE_INSTANCE_GET_PRIVATE (context,
					       GDK_TYPE_APP_LAUNCH_CONTEXT,
					       GdkAppLaunchContextPrivate);
  context->priv->workspace = -1;
}

/**
 * gdk_app_launch_context_set_display:
 * @context: a #GdkAppLaunchContext
 * @display: a #GdkDisplay
 *
 * Sets the display on which applications will be launched when
 * using this context. See also gdk_app_launch_context_set_screen().
 *
 * Since: 2.16
 */
void
gdk_app_launch_context_set_display (GdkAppLaunchContext *context,
				    GdkDisplay          *display)
{
  if (context->priv->display)
    {
      g_object_unref (context->priv->display);
      context->priv->display = NULL;
    }

  if (display)
    context->priv->display = g_object_ref (display);
}

/**
 * gdk_app_launch_context_set_screen:
 * @context: a #GdkAppLaunchContext
 * @screen: a #GdkScreen
 *
 * Sets the screen on which applications will be launched when
 * using this context. See also gdk_app_launch_context_set_display().
 * If both @screen and @display are set, the @screen takes priority.
 *
 * Since: 2.16
 */
void
gdk_app_launch_context_set_screen (GdkAppLaunchContext *context,
				   GdkScreen           *screen)
{
  if (context->priv->screen)
    {
      g_object_unref (context->priv->screen);
      context->priv->screen = NULL;
    }

  if (screen)
    context->priv->screen = g_object_ref (screen);
}

/**
 * gdk_app_launch_context_set_desktop:
 * @context: a #GdkAppLaunchContext
 * @desktop: the number of a workspace, or -1
 *
 * Sets the workspace on which applications will be launched when
 * using this context when running under a window manager that 
 * supports multiple workspaces, as described in the 
 * <ulink url="http://www.freedesktop.org/Standards/wm-spec">Extended 
 * Window Manager Hints</ulink>. 
 *
 * Since: 2.16
 */
void
gdk_app_launch_context_set_desktop (GdkAppLaunchContext *context,
				    gint                 desktop)
{
  context->priv->workspace = desktop;
}

/**
 * gdk_app_launch_context_set_timestamp:
 * @context: a #GdkAppLaunchContext
 * @timestamp: a timestamp
 *
 * Sets the timestamp of @context. The timestamp should ideally
 * be taken from the event that triggered the launch.
 *
 * Since: 2.16
 */
void
gdk_app_launch_context_set_timestamp (GdkAppLaunchContext *context,
				      guint32              timestamp)
{
  context->priv->timestamp = timestamp;
}

/**
 * gdk_app_launch_context_set_icon:
 * @context: a #GdkAppLaunchContext
 * @icon: a #GIcon, or %NULL
 *
 * Sets the icon for applications that are launched with this
 * context. See also gdk_app_launch_context_set_icon_name().
 *
 * Since: 2.16
 */
void
gdk_app_launch_context_set_icon (GdkAppLaunchContext *context, 
                                 GIcon               *icon)
{
  if (context->priv->icon)
    {
      g_object_unref (context->priv->icon);
      context->priv->icon = NULL;
    }

  if (icon)
    context->priv->icon = g_object_ref (icon);
}

/**
 * gdk_app_launch_context_set_icon_name:
 * @context: a #GdkAppLaunchContext
 * @icon_name: an icon name, or %NULL
 *
 * Sets the icon for applications that are launched with this
 * context. The @icon_name will be interpreted in the same way 
 * as the Icon field in desktop files. 
 * See also gdk_app_launch_context_set_icon(). If both @icon 
 * and @icon_name are set, the @icon_name takes priority.
 * 
 * Since: 2.16
 */
void
gdk_app_launch_context_set_icon_name (GdkAppLaunchContext *context,
				      const char          *icon_name)
{
  g_free (context->priv->icon_name);
  context->priv->icon_name = g_strdup (icon_name);
}

/**
 * gdk_app_launch_context_new:
 *
 * Creates a new #GdkAppLaunchContext.
 *
 * Returns: a new #GdkAppLaunchContext
 *
 * Since: 2.16
 */
GdkAppLaunchContext *
gdk_app_launch_context_new (void)
{
  GdkAppLaunchContext *context;

  context = g_object_new (gdk_app_launch_context_get_type (), NULL);

  return context;
}


#define __GDK_APP_LAUNCH_CONTEXT_C__
#include "gdkaliasdef.c"
