/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Author: James Henstridge <james@daa.com.au>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */
#ifndef __GTK_ACTION_H__
#define __GTK_ACTION_H__

#include <gtk/gtkwidget.h>
#include <glib-object.h>

#define GTK_TYPE_ACTION            (gtk_action_get_type ())
#define GTK_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ACTION, GtkAction))
#define GTK_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ACTION, GtkActionClass))
#define GTK_IS_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ACTION))
#define GTK_IS_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), GTK_TYPE_ACTION))
#define GTK_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_ACTION, GtkActionClass))

typedef struct _GtkAction      GtkAction;
typedef struct _GtkActionClass GtkActionClass;
typedef struct _GtkActionPrivate GtkActionPrivate;

struct _GtkAction
{
  GObject object;

  /*< private >*/

  GtkActionPrivate *private_data;
};

struct _GtkActionClass
{
  GObjectClass parent_class;

  /* activation signal */
  void       (* activate)           (GtkAction    *action);

  GType      menu_item_type;
  GType      toolbar_item_type;

  /* widget creation routines (not signals) */
  GtkWidget *(* create_menu_item)   (GtkAction *action);
  GtkWidget *(* create_tool_item)   (GtkAction *action);
  void       (* connect_proxy)      (GtkAction *action,
				     GtkWidget *proxy);
  void       (* disconnect_proxy)   (GtkAction *action,
				     GtkWidget *proxy);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType      gtk_action_get_type              (void);

const gchar* gtk_action_get_name            (GtkAction   *action);
void       gtk_action_activate              (GtkAction   *action);

GtkWidget *gtk_action_create_icon           (GtkAction   *action,
					     GtkIconSize  icon_size);
GtkWidget *gtk_action_create_menu_item      (GtkAction   *action);
GtkWidget *gtk_action_create_tool_item      (GtkAction   *action);
void       gtk_action_connect_proxy         (GtkAction   *action,
					     GtkWidget   *proxy);
void       gtk_action_disconnect_proxy      (GtkAction   *action,
					     GtkWidget   *proxy);
GSList    *gtk_action_get_proxies           (GtkAction   *action);

/* protected ... for use by child actions */
void       gtk_action_block_activate_from   (GtkAction   *action,
		   		             GtkWidget   *proxy);
void       gtk_action_unblock_activate_from (GtkAction   *action,
				             GtkWidget   *proxy);

/* protected ... for use by action groups */
void       gtk_action_set_accel_path        (GtkAction   *action,
				             const gchar *accel_path);


#endif  /* __GTK_ACTION_H__ */
