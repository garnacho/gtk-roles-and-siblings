/* gtktoolshell.c
 * Copyright (C) 2007  Openismus GmbH
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
 * Author:
 *   Mathias Hasselmann
 */

#include <config.h>
#include "gtktoolshell.h"
#include "gtkwidget.h"
#include "gtkintl.h"
#include "gtkalias.h"

/**
 * SECTION:gtktoolshell
 * @short_description: Interface for containers containing #GtkToolItem widgets.
 *
 * The #GtkToolShell interface allows container widgets to provide additional
 * information when embedding #GtkToolItem widgets.
 *
 * @see_also: #GtkToolbar, #GtkToolItem
 */

/**
 * GtkToolShell:
 *
 * Dummy structure for accessing instances of #GtkToolShellIface.
 */

GType
gtk_tool_shell_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      type = g_type_register_static_simple (G_TYPE_INTERFACE, I_("GtkToolShell"),
                                            sizeof (GtkToolShellIface),
                                            NULL, 0, NULL, 0);
      g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
    }

  return type;
}

/**
 * gtk_tool_shell_get_icon_size:
 * @shell: a #GtkToolShell
 *
 * Retrieves the icon size for the tool shell. Tool items must not call this
 * function directly, but rely on gtk_tool_item_get_icon_size() instead.
 *
 * Return value: the current size for icons of @shell
 *
 * Since: 2.16
 **/
GtkIconSize
gtk_tool_shell_get_icon_size (GtkToolShell *shell)
{
  return GTK_TOOL_SHELL_GET_IFACE (shell)->get_icon_size (shell);
}

/**
 * gtk_tool_shell_get_orientation:
 * @shell: a #GtkToolShell
 *
 * Retrieves the current orientation for the tool shell. Tool items must not
 * call this function directly, but rely on gtk_tool_item_get_orientation()
 * instead.
 *
 * Return value: the current orientation of @shell
 *
 * Since: 2.16
 **/
GtkOrientation
gtk_tool_shell_get_orientation (GtkToolShell *shell)
{
  return GTK_TOOL_SHELL_GET_IFACE (shell)->get_orientation (shell);
}

/**
 * gtk_tool_shell_get_style:
 * @shell: a #GtkToolShell
 *
 * Retrieves whether the tool shell has text, icons, or both. Tool items must
 * not call this function directly, but rely on gtk_tool_item_get_style()
 * instead.
 *
 * Return value: the current style of @shell
 *
 * Since: 2.16
 **/
GtkToolbarStyle
gtk_tool_shell_get_style (GtkToolShell *shell)
{
  return GTK_TOOL_SHELL_GET_IFACE (shell)->get_style (shell);
}

/**
 * gtk_tool_shell_get_relief_style:
 * @shell: a #GtkToolShell
 *
 * Returns the relief style of buttons on @shell. Tool items must not call this
 * function directly, but rely on gtk_tool_item_get_relief_style() instead.
 *
 * Return value: The relief style of buttons on @shell.
 *
 * Since: 2.16
 **/
GtkReliefStyle
gtk_tool_shell_get_relief_style (GtkToolShell *shell)
{
  GtkToolShellIface *iface = GTK_TOOL_SHELL_GET_IFACE (shell);

  if (iface->get_relief_style)
    iface->get_relief_style (shell);

  return GTK_RELIEF_NONE;
}

/**
 * gtk_tool_shell_rebuild_menu:
 * @shell: a #GtkToolShell
 *
 * Calling this function signals the tool shell that the overflow menu item for
 * tool items have changed. If there is an overflow menu and if it is visible
 * when this function it called, the menu will be rebuilt.
 *
 * Tool items must not call this function directly, but rely on
 * gtk_tool_item_rebuild_menu() instead.
 *
 * Since: 2.16
 **/
void
gtk_tool_shell_rebuild_menu (GtkToolShell *shell)
{
  GtkToolShellIface *iface = GTK_TOOL_SHELL_GET_IFACE (shell);

  if (iface->rebuild_menu)
    iface->rebuild_menu (shell);
}

#define __GTK_TOOL_SHELL_C__
#include "gtkaliasdef.c"
