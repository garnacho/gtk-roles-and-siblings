/* GTK - The GIMP Toolkit
 * Copyright (C) 1998, 2001 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_ACCEL_GROUP_H__
#define __GTK_ACCEL_GROUP_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS


/* --- type macros --- */
#define GTK_TYPE_ACCEL_GROUP              (gtk_accel_group_get_type ())
#define GTK_ACCEL_GROUP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_ACCEL_GROUP, GtkAccelGroup))
#define GTK_ACCEL_GROUP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ACCEL_GROUP, GtkAccelGroupClass))
#define GTK_IS_ACCEL_GROUP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_ACCEL_GROUP))
#define GTK_IS_ACCEL_GROUP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ACCEL_GROUP))
#define GTK_ACCEL_GROUP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ACCEL_GROUP, GtkAccelGroupClass))


/* --- accel flags --- */
typedef enum
{
  GTK_ACCEL_VISIBLE        = 1 << 0,	/* display in GtkAccelLabel? */
  GTK_ACCEL_LOCKED         = 1 << 1,	/* is it removable? */
  GTK_ACCEL_MASK           = 0x07
} GtkAccelFlags;


/* --- typedefs & structures --- */
typedef struct _GtkAccelGroup	   GtkAccelGroup;
typedef struct _GtkAccelGroupClass GtkAccelGroupClass;
typedef struct _GtkAccelKey        GtkAccelKey;
typedef struct _GtkAccelGroupEntry GtkAccelGroupEntry;
typedef gboolean (*GtkAccelGroupActivate) (GtkAccelGroup  *accel_group,
					   GObject        *acceleratable,
					   guint           keyval,
					   GdkModifierType modifier);
struct _GtkAccelGroup
{
  GObject             parent;
  guint	              lock_count;
  GdkModifierType     modifier_mask;
  GSList             *acceleratables;
  guint		      n_accels;
  GtkAccelGroupEntry *priv_accels;
};
struct _GtkAccelGroupClass
{
  GObjectClass parent_class;

  void	(*accel_changed)	(GtkAccelGroup	*accel_group,
				 guint           keyval,
				 GdkModifierType modifier,
				 GClosure       *accel_closure);
};
struct _GtkAccelKey
{
  guint           accel_key;
  GdkModifierType accel_mods;
  guint           accel_flags : 16;
};


/* -- Accelerator Groups --- */
GType           gtk_accel_group_get_type        (void);
GtkAccelGroup*  gtk_accel_group_new	      	(void);
void		gtk_accel_group_lock		(GtkAccelGroup	*accel_group);
void		gtk_accel_group_unlock		(GtkAccelGroup	*accel_group);
void		gtk_accel_group_connect		(GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods,
						 GtkAccelFlags	 accel_flags,
						 GClosure	*closure,
						 GQuark		 accel_path_quark);
gboolean	gtk_accel_group_disconnect	(GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods);
gboolean    gtk_accel_groups_disconnect_closure	(GClosure	*closure);


/* --- GtkActivatable glue --- */
void		_gtk_accel_group_attach		(GtkAccelGroup	*accel_group,
						 GObject	*object);
void		_gtk_accel_group_detach		(GtkAccelGroup	*accel_group,
						 GObject	*object);
gboolean        gtk_accel_groups_activate      	(GObject	*acceleratable,
						 guint		 accel_key,
						 GdkModifierType accel_mods);
GSList*	    gtk_accel_groups_from_acceleratable (GObject	*object);
GtkAccelKey*	gtk_accel_group_find		(GtkAccelGroup	*accel_group,
						 gboolean (*find_func) (GtkAccelKey *key,
									GClosure    *closure,
									gpointer     data),
						 gpointer        data);
GtkAccelGroup*	gtk_accel_group_from_accel_closure (GClosure    *closure);


/* --- Accelerators--- */
gboolean gtk_accelerator_valid		      (guint	        keyval,
					       GdkModifierType  modifiers) G_GNUC_CONST;
void	 gtk_accelerator_parse		      (const gchar     *accelerator,
					       guint	       *accelerator_key,
					       GdkModifierType *accelerator_mods);
gchar*	 gtk_accelerator_name		      (guint	        accelerator_key,
					       GdkModifierType  accelerator_mods);
void	 gtk_accelerator_set_default_mod_mask (GdkModifierType  default_mod_mask);
guint	 gtk_accelerator_get_default_mod_mask (void);


/* --- internal --- */
GtkAccelGroupEntry*	gtk_accel_group_query	(GtkAccelGroup	*accel_group,
						 guint		 accel_key,
						 GdkModifierType accel_mods,
						 guint          *n_entries);
struct _GtkAccelGroupEntry
{
  GtkAccelKey  key;
  GClosure    *closure;
  GQuark       accel_path_quark;
};


#ifndef GTK_DISABLE_DEPRECATED
#define	gtk_accel_group_ref	g_object_ref
#define	gtk_accel_group_unref	g_object_unref
#endif /* GTK_DISABLE_DEPRECATED */

G_END_DECLS


#endif /* __GTK_ACCEL_GROUP_H__ */
