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
#ifndef __GTK_ACTION_GROUP_H__
#define __GTK_ACTION_GROUP_H__

#include <gtk/gtkaction.h>

#define GTK_TYPE_ACTION_GROUP              (gtk_action_group_get_type ())
#define GTK_ACTION_GROUP(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ACTION_GROUP, GtkActionGroup))
#define GTK_ACTION_GROUP_CLASS(vtable)     (G_TYPE_CHECK_CLASS_CAST ((vtable), GTK_TYPE_ACTION_GROUP, GtkActionGroupClass))
#define GTK_IS_ACTION_GROUP(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ACTION_GROUP))
#define GTK_IS_ACTION_GROUP_CLASS(vtable)  (G_TYPE_CHECK_CLASS_TYPE ((vtable), GTK_TYPE_ACTION_GROUP))
#define GTK_ACTION_GROUP_GET_CLASS(inst)   (G_TYPE_INSTANCE_GET_CLASS ((inst), GTK_TYPE_ACTION_GROUP, GtkActionGroupClass))

typedef struct _GtkActionGroup        GtkActionGroup;
typedef struct _GtkActionGroupPrivate GtkActionGroupPrivate;
typedef struct _GtkActionGroupClass   GtkActionGroupClass;
typedef struct _GtkActionGroupEntry   GtkActionGroupEntry;

struct _GtkActionGroup
{
  GObject parent;

  /*< private >*/

  GtkActionGroupPrivate *private_data;
};

struct _GtkActionGroupClass
{
  GObjectClass parent_class;

  GtkAction *(* get_action) (GtkActionGroup *action_group,
			     const gchar    *action_name);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

typedef enum 
{
  GTK_ACTION_NORMAL,
  GTK_ACTION_TOGGLE,
  GTK_ACTION_RADIO
} GtkActionGroupEntryType;

struct _GtkActionGroupEntry 
{
  gchar *name;
  gchar *label;
  gchar *stock_id;
  gchar *accelerator;
  gchar *tooltip;

  GCallback callback;
  gpointer user_data;

  GtkActionGroupEntryType entry_type;
  gchar *extra_data;
};

GType           gtk_action_group_get_type      (void);

GtkActionGroup *gtk_action_group_new           (const gchar         *name);

const gchar    *gtk_action_group_get_name      (GtkActionGroup      *action_group);
GtkAction      *gtk_action_group_get_action    (GtkActionGroup      *action_group,
						const gchar         *action_name);
GList          *gtk_action_group_list_actions  (GtkActionGroup      *action_group);
void            gtk_action_group_add_action    (GtkActionGroup      *action_group,
						GtkAction           *action);
void            gtk_action_group_remove_action (GtkActionGroup      *action_group,
						GtkAction           *action);

void            gtk_action_group_add_actions   (GtkActionGroup      *action_group,
						GtkActionGroupEntry *entries,
						guint                n_entries);

#endif  /* __GTK_ACTION_GROUP_H__ */
