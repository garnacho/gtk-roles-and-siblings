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
#include "gtkaccelgroup.h"
#include "gtkaccelmap.h"
#include "gdk/gdkkeysyms.h"
#include "gtksignal.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>


/* --- prototypes --- */
static void gtk_accel_group_class_init	(GtkAccelGroupClass	*class);
static void gtk_accel_group_init	(GtkAccelGroup		*accel_group);
static void gtk_accel_group_finalize	(GObject		*object);


/* --- variables --- */
static GObjectClass     *parent_class = NULL;
static guint		 signal_accel_activate = 0;
static guint		 signal_accel_changed = 0;
static guint		 quark_acceleratable_groups = 0;
static guint		 default_accel_mod_mask = (GDK_SHIFT_MASK |
						   GDK_CONTROL_MASK |
						   GDK_MOD1_MASK);


/* --- functions --- */
/**
 * gtk_accel_map_change_entry
 * @returns: the type ID for accelerator groups
 */
GType
gtk_accel_group_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info = {
	sizeof (GtkAccelGroupClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gtk_accel_group_class_init,
	NULL,   /* clas_finalize */
	NULL,   /* class_data */
	sizeof (GtkAccelGroup),
	0,      /* n_preallocs */
	(GInstanceInitFunc) gtk_accel_group_init,
      };

      object_type = g_type_register_static (G_TYPE_OBJECT,
					    "GtkAccelGroup",
					    &object_info, 0);
    }

  return object_type;
}

static gboolean
accel_activate_accumulator (GSignalInvocationHint *ihint,
			    GValue                *return_accu,
			    const GValue          *handler_return,
			    gpointer               data)
{
  gboolean continue_emission;
  gboolean handler_val;

  /* handler returns whether the accelerator was handled */
  handler_val = g_value_get_boolean (handler_return);

  /* record that as result for this emission */
  g_value_set_boolean (return_accu, handler_val);

  /* don't continue if accelerator was handled */
  continue_emission = !handler_val;

  return continue_emission;
}

static void
gtk_accel_group_class_init (GtkAccelGroupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  quark_acceleratable_groups = g_quark_from_static_string ("gtk-acceleratable-accel-groups");

  object_class->finalize = gtk_accel_group_finalize;

  class->accel_changed = NULL;
  signal_accel_activate = g_signal_new ("accel_activate",
					G_OBJECT_CLASS_TYPE (class),
					G_SIGNAL_DETAILED,
					0,
					accel_activate_accumulator, NULL,
					gtk_marshal_BOOLEAN__OBJECT_UINT_UINT,
					G_TYPE_BOOLEAN, 3, G_TYPE_OBJECT, G_TYPE_UINT, G_TYPE_UINT);
  signal_accel_changed = g_signal_new ("accel_changed",
				       G_OBJECT_CLASS_TYPE (class),
				       G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
				       G_STRUCT_OFFSET (GtkAccelGroupClass, accel_changed),
				       NULL, NULL,
				       gtk_marshal_VOID__UINT_UINT_BOXED,
				       G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_CLOSURE);
}

static void
gtk_accel_group_finalize (GObject *object)
{
  GtkAccelGroup *accel_group = GTK_ACCEL_GROUP (object);

  g_free (accel_group->priv_accels);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_accel_group_init (GtkAccelGroup *accel_group)
{
  accel_group->lock_count = 0;
  accel_group->modifier_mask = gtk_accelerator_get_default_mod_mask ();
  accel_group->acceleratables = NULL;
  accel_group->n_accels = 0;
  accel_group->priv_accels = NULL;
}

/**
 * gtk_accel_group_new
 * @returns: a new #GtkAccelGroup object
 * 
 * Creates a new #GtkAccelGroup. 
 */
GtkAccelGroup*
gtk_accel_group_new (void)
{
  return g_object_new (GTK_TYPE_ACCEL_GROUP, NULL);
}

static void
accel_group_weak_ref_detach (GSList  *free_list,
			     GObject *stale_object)
{
  GSList *slist;
  
  for (slist = free_list; slist; slist = slist->next)
    {
      GtkAccelGroup *accel_group;
      
      accel_group = slist->data;
      accel_group->acceleratables = g_slist_remove (accel_group->acceleratables, stale_object);
      g_object_unref (accel_group);
    }
  g_slist_free (free_list);
}

void
_gtk_accel_group_attach (GtkAccelGroup *accel_group,
			 GObject       *object)
{
  GSList *slist;
  
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (g_slist_find (accel_group->acceleratables, object) == NULL);
  
  g_object_ref (accel_group);
  accel_group->acceleratables = g_slist_prepend (accel_group->acceleratables, object);
  slist = g_object_get_qdata (object, quark_acceleratable_groups);
  if (slist)
    g_object_weak_unref (object,
			 (GWeakNotify) accel_group_weak_ref_detach,
			 slist);
  slist = g_slist_prepend (slist, accel_group);
  g_object_set_qdata (object, quark_acceleratable_groups, slist);
  g_object_weak_ref (object,
		     (GWeakNotify) accel_group_weak_ref_detach,
		     slist);
}

void
_gtk_accel_group_detach (GtkAccelGroup *accel_group,
			 GObject       *object)
{
  GSList *slist;
  
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (g_slist_find (accel_group->acceleratables, object) != NULL);
  
  accel_group->acceleratables = g_slist_remove (accel_group->acceleratables, object);
  slist = g_object_get_qdata (object, quark_acceleratable_groups);
  g_object_weak_unref (object,
		       (GWeakNotify) accel_group_weak_ref_detach,
		       slist);
  slist = g_slist_remove (slist, accel_group);
  g_object_set_qdata (object, quark_acceleratable_groups, slist);
  if (slist)
    g_object_weak_ref (object,
		       (GWeakNotify) accel_group_weak_ref_detach,
		       slist);
  g_object_unref (accel_group);
}

GSList*
gtk_accel_groups_from_acceleratable (GObject *object)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  
  return g_object_get_qdata (object, quark_acceleratable_groups);
}

GtkAccelKey*
gtk_accel_group_find (GtkAccelGroup  *accel_group,
		      gboolean (*find_func) (GtkAccelKey *key,
					     GClosure    *closure,
					     gpointer     data),
		      gpointer        data)
{
  GtkAccelKey *key = NULL;
  guint i;

  g_return_val_if_fail (GTK_IS_ACCEL_GROUP (accel_group), NULL);
  g_return_val_if_fail (find_func != NULL, NULL);

  g_object_ref (accel_group);
  for (i = 0; i < accel_group->n_accels; i++)
    if (find_func (&accel_group->priv_accels[i].key,
		   accel_group->priv_accels[i].closure,
		   data))
      {
	key = &accel_group->priv_accels[i].key;
	break;
      }
  g_object_unref (accel_group);

  return key;
}

/**
 * gtk_accel_group_lock
 * @accel_group: a #GtkAccelGroup
 * 
 * Locking an acelerator group prevents the accelerators contained
 * within it to be changed during runtime. Refer to
 * gtk_accel_map_change_entry() about runtime accelerator changes.
 *
 * If called more than once, @accel_group remains locked until
 * gtk_accel_group_unlock() has been called an equivalent number
 * of times.
 */
void
gtk_accel_group_lock (GtkAccelGroup *accel_group)
{
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));
  
  accel_group->lock_count += 1;
}

/**
 * gtk_accel_group_unlock
 * @accel_group: a #GtkAccelGroup
 * 
 * This function undoes the last call to gtk_accel_group_lock()
 * on this @accel_group.
 */
void
gtk_accel_group_unlock (GtkAccelGroup *accel_group)
{
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));
  g_return_if_fail (accel_group->lock_count > 0);

  accel_group->lock_count -= 1;
}

static void
accel_tag_func (gpointer  data,
		GClosure *closure)
{
  /* GtkAccelGroup *accel_group = data; */
}

static int
bsearch_compare_accels (const void *d1,
			const void *d2)
{
  const GtkAccelGroupEntry *entry1 = d1;
  const GtkAccelGroupEntry *entry2 = d2;

  if (entry1->key.accel_key == entry2->key.accel_key)
    return entry1->key.accel_mods < entry2->key.accel_mods ? -1 : entry1->key.accel_mods > entry2->key.accel_mods;
  else
    return entry1->key.accel_key < entry2->key.accel_key ? -1 : 1;
}

static void
quick_accel_add (GtkAccelGroup  *accel_group,
		 guint           accel_key,
		 GdkModifierType accel_mods,
		 GtkAccelFlags   accel_flags,
		 GClosure       *closure,
		 GQuark          path_quark)
{
  guint pos, i = accel_group->n_accels++;
  GtkAccelGroupEntry key;

  key.key.accel_key = accel_key;
  key.key.accel_mods = accel_mods;
  for (pos = 0; pos < i; pos++)
    if (bsearch_compare_accels (&key, accel_group->priv_accels + pos) < 0)
      break;
  accel_group->priv_accels = g_renew (GtkAccelGroupEntry, accel_group->priv_accels, accel_group->n_accels);
  g_memmove (accel_group->priv_accels + pos + 1, accel_group->priv_accels + pos,
	     (i - pos) * sizeof (accel_group->priv_accels[0]));
  accel_group->priv_accels[pos].key.accel_key = accel_key;
  accel_group->priv_accels[pos].key.accel_mods = accel_mods;
  accel_group->priv_accels[pos].key.accel_flags = accel_flags;
  accel_group->priv_accels[pos].closure = g_closure_ref (closure);
  accel_group->priv_accels[pos].accel_path_quark = path_quark;
  g_closure_sink (closure);

  /* tag closure for backwards lookup */
  g_closure_add_invalidate_notifier (closure, accel_group, accel_tag_func);
}

static GtkAccelGroupEntry*
quick_accel_find (GtkAccelGroup  *accel_group,
		  guint           accel_key,
		  GdkModifierType accel_mods,
		  guint		 *count_p)
{
  GtkAccelGroupEntry *entry;
  GtkAccelGroupEntry key;

  if (!accel_group->n_accels)
    return NULL;

  key.key.accel_key = accel_key;
  key.key.accel_mods = accel_mods;
  entry = bsearch (&key, accel_group->priv_accels, accel_group->n_accels,
		   sizeof (accel_group->priv_accels[0]), bsearch_compare_accels);
  
  if (!entry)
    return NULL;

  /* step back to the first member */
  for (; entry > accel_group->priv_accels; entry--)
    if (entry[-1].key.accel_key != accel_key ||
	entry[-1].key.accel_mods != accel_mods)
      break;
  /* count equal members */
  for (*count_p = 0; entry + *count_p < accel_group->priv_accels + accel_group->n_accels; (*count_p)++)
    if (entry[*count_p].key.accel_key != accel_key ||
	entry[*count_p].key.accel_mods != accel_mods)
      break;
  return entry;
}

static GSList*
quick_accel_remove (GtkAccelGroup  *accel_group,
		    guint           accel_key,
		    GdkModifierType accel_mods)
{
  guint i, n;
  GtkAccelGroupEntry *entry = quick_accel_find (accel_group, accel_key, accel_mods, &n);
  guint pos = entry - accel_group->priv_accels;
  GSList *clist = NULL;

  if (!entry)
    return NULL;
  for (i = 0; i < n; i++)
    {
      g_closure_remove_invalidate_notifier (entry[i].closure, accel_group, accel_tag_func);
      clist = g_slist_prepend (clist, entry[i].closure);
    }

  accel_group->n_accels -= n;
  g_memmove (entry, entry + n,
	     (accel_group->n_accels - pos) * sizeof (accel_group->priv_accels[0]));

  return clist;
}

/**
 * gtk_accel_group_connect
 * @accel_group:      the ccelerator group to install an accelerator in
 * @accel_key:        key value of the accelerator
 * @accel_mods:       modifier combination of the accelerator
 * @accel_flags:      a flag mask to configure this accelerator
 * @closure:          closure to be executed upon accelerator activation
 * @accel_path_quark: accelerator path quark from GtkAccelMapNotify
 *
 * Install an accelerator in this group. When @accel_group is being activated
 * in response to a call to gtk_accel_groups_activate(), @closure will be
 * invoked if the @accel_key and @accel_mods from gtk_accel_groups_activate()
 * match those of this connection.
 * The signature used for the @closure is that of #GtkAccelGroupActivate.
 * If this connection is made in response to an accelerator path change (see
 * gtk_accel_map_change_entry()) from a #GtkAccelMapNotify notifier,
 * @accel_path_quark must be passed on from the notifier into this function,
 * it should be 0 otherwise.
 */
void
gtk_accel_group_connect (GtkAccelGroup	*accel_group,
			 guint		 accel_key,
			 GdkModifierType accel_mods,
			 GtkAccelFlags	 accel_flags,
			 GClosure	*closure,
			 GQuark          accel_path_quark)
{
  gchar *accel_name;
  GQuark accel_quark;

  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));
  g_return_if_fail (closure != NULL);
  g_return_if_fail (accel_key > 0);

  accel_name = gtk_accelerator_name (accel_key, accel_mods);
  accel_quark = g_quark_from_string (accel_name);
  g_free (accel_name);

  quick_accel_add (accel_group, accel_key, accel_mods, accel_flags, closure, accel_path_quark);

  /* setup handler */
  g_signal_connect_closure_by_id (accel_group, signal_accel_activate, accel_quark, closure, FALSE);

  /* and notify */
  g_signal_emit (accel_group, signal_accel_changed, accel_quark, accel_key, accel_mods, closure);
}

static gboolean
accel_group_disconnect_closure (GtkAccelGroup  *accel_group,
				guint	        accel_key,
				GdkModifierType accel_mods,
				GClosure       *closure)
{
  gchar *accel_name;
  GQuark accel_quark;
  GSList *clist , *slist;
  gboolean removed_some = FALSE;

  accel_name = gtk_accelerator_name (accel_key, accel_mods);
  accel_quark = g_quark_from_string (accel_name);
  g_free (accel_name);

  clist = quick_accel_remove (accel_group, accel_key, accel_mods);
  if (!clist)
    return FALSE;

  g_object_ref (accel_group);

  for (slist = clist; slist; slist = slist->next)
    if (!closure || slist->data == (gpointer) closure)
      {
	g_signal_handlers_disconnect_matched (accel_group, G_SIGNAL_MATCH_CLOSURE | G_SIGNAL_MATCH_ID,
					      signal_accel_activate, 0,
					      slist->data, NULL, NULL);
	/* and notify */
	g_signal_emit (accel_group, signal_accel_changed, accel_quark, accel_key, accel_mods, slist->data);
	
	/* remove quick_accel_add() ref_count */
	g_closure_unref (slist->data);

	removed_some = TRUE;
      }
  g_slist_free (clist);

  g_object_unref (accel_group);
  
  return removed_some;
}

/**
 * gtk_accel_group_disconnect
 * @accel_group:      the ccelerator group to install an accelerator in
 * @accel_key:        key value of the accelerator
 * @accel_mods:       modifier combination of the accelerator
 * @returns:          %TRUE if there was an accelerator which could be removed, %FALSE otherwise
 *
 * Remove an accelerator previously installed through
 * gtk_accel_group_connect().
 */
gboolean
gtk_accel_group_disconnect (GtkAccelGroup  *accel_group,
			    guint	    accel_key,
			    GdkModifierType accel_mods)
{
  g_return_val_if_fail (GTK_IS_ACCEL_GROUP (accel_group), FALSE);

  return accel_group_disconnect_closure (accel_group, accel_key, accel_mods, NULL);
}

GtkAccelGroupEntry*
gtk_accel_group_query (GtkAccelGroup  *accel_group,
		       guint           accel_key,
		       GdkModifierType accel_mods,
		       guint          *n_entries)
{
  GtkAccelGroupEntry *entries;
  guint n;

  g_return_val_if_fail (GTK_IS_ACCEL_GROUP (accel_group), NULL);

  entries = quick_accel_find (accel_group, accel_key, accel_mods, &n);

  if (n_entries)
    *n_entries = entries ? n : 0;

  return entries;
}

static gboolean
find_accel_closure (GtkAccelKey *key,
		    GClosure    *closure,
		    gpointer     data)
{
  return data == (gpointer) closure;
}

gboolean
gtk_accel_groups_disconnect_closure (GClosure *closure)
{
  GtkAccelGroup *group;

  g_return_val_if_fail (closure != NULL, FALSE);

  group = gtk_accel_group_from_accel_closure (closure);
  if (group)
    {
      GtkAccelKey *key = gtk_accel_group_find (group, find_accel_closure, closure);

      /* sigh, not finding the key can unexpectedly happen if someone disposes
       * accel groups. that's highly recommended to _not_ do though.
       */
      if (key)
	{
	  accel_group_disconnect_closure (group, key->accel_key, key->accel_mods, closure);
	  return TRUE;
	}
    }
  return FALSE;
}

GtkAccelGroup*
gtk_accel_group_from_accel_closure (GClosure *closure)
{
  guint i;

  g_return_val_if_fail (closure != NULL, NULL);

  /* a few remarks on wat we do here. in general, we need a way to back-lookup
   * accel_groups from closures that are being used in accel groups. this could
   * be done e.g via a hashtable. it is however cheaper (memory wise) to just
   * store a NOP notifier on the closure itself that contains the accel group
   * as data which, besides needing to peek a bit at closure internals, works
   * just as good.
   */
  for (i = 0; i < G_CLOSURE_N_NOTIFIERS (closure); i++)
    if (closure->notifiers[i].notify == accel_tag_func)
      return closure->notifiers[i].data;

  return NULL;
}

gboolean
_gtk_accel_group_activate (GtkAccelGroup  *accel_group,
			   GQuark	   accel_quark,
			   GObject	  *acceleratable,
			   guint	   accel_key,
			   GdkModifierType accel_mods)
{
  gboolean was_handled;

  g_return_val_if_fail (GTK_IS_ACCEL_GROUP (accel_group), FALSE);

  was_handled = FALSE;
  g_signal_emit (accel_group, signal_accel_activate, accel_quark,
		 acceleratable, accel_key, accel_mods, &was_handled);

  return was_handled;
}

/**
 * gtk_accel_groups_activate:
 * @acceleratable: usually a #GtkWindow
 * @accel_key:     accelerator keyval from a key event
 * @accel_mods:    keyboard state mask from a key event
 * @returns:       %TRUE if the accelerator was handled, %FALSE otherwise
 * 
 * Finds the first accelerator in any #GtkAccelGroup attached
 * to @acceleratable that matches @accel_key and @accel_mods, and
 * activates that accelerator.
 * If an accelerator was activated and handled this keypress, %TRUE
 * is returned.
 */
gboolean
gtk_accel_groups_activate (GObject	  *acceleratable,
			   guint	   accel_key,
			   GdkModifierType accel_mods)
{
  g_return_val_if_fail (G_IS_OBJECT (acceleratable), FALSE);
  
  if (gtk_accelerator_valid (accel_key, accel_mods))
    {
      gchar *accel_name;
      GQuark accel_quark;
      GSList *slist;

      accel_name = gtk_accelerator_name (accel_key, accel_mods);
      accel_quark = g_quark_from_string (accel_name);
      g_free (accel_name);
      
      for (slist = gtk_accel_groups_from_acceleratable (acceleratable); slist; slist = slist->next)
	if (_gtk_accel_group_activate (slist->data, accel_quark, acceleratable, accel_key, accel_mods))
	  return TRUE;
    }
  
  return FALSE;
}

/**
 * gtk_accelerator_valid
 * @keyval:    a GDK keyval
 * @modifiers: modifier mask
 * @returns:   %TRUE if the accelerator is valid
 * 
 * Determines whether a given keyval and modifier mask constitute
 * a valid keyboard accelerator. For example, the GDK_a keyval
 * plus GDK_CONTROL_MASK is valid - this is a "Ctrl+a" accelerator.
 * But by default (see gtk_accelerator_set_default_mod_mask()) you
 * cannot use the NumLock key as an accelerator modifier.
 */
gboolean
gtk_accelerator_valid (guint		  keyval,
		       GdkModifierType	  modifiers)
{
  static const guint invalid_accelerator_vals[] = {
    GDK_BackSpace, GDK_Delete, GDK_KP_Delete,
    GDK_Shift_L, GDK_Shift_R, GDK_Shift_Lock, GDK_Caps_Lock, GDK_ISO_Lock,
    GDK_Control_L, GDK_Control_R, GDK_Meta_L, GDK_Meta_R,
    GDK_Alt_L, GDK_Alt_R, GDK_Super_L, GDK_Super_R, GDK_Hyper_L, GDK_Hyper_R,
    GDK_Mode_switch, GDK_Num_Lock, GDK_Multi_key,
    GDK_Scroll_Lock, GDK_Sys_Req, 
    GDK_Up, GDK_Down, GDK_Left, GDK_Right, GDK_Tab, GDK_ISO_Left_Tab,
    GDK_KP_Up, GDK_KP_Down, GDK_KP_Left, GDK_KP_Right, GDK_KP_Tab,
    GDK_First_Virtual_Screen, GDK_Prev_Virtual_Screen,
    GDK_Next_Virtual_Screen, GDK_Last_Virtual_Screen,
    GDK_Terminate_Server, GDK_AudibleBell_Enable,
    0
  };
  const guint *ac_val;

  modifiers &= GDK_MODIFIER_MASK;
    
  if (keyval <= 0xFF)
    return keyval >= 0x20;

  ac_val = invalid_accelerator_vals;
  while (*ac_val)
    {
      if (keyval == *ac_val++)
	return FALSE;
    }

  return TRUE;
}

static inline gboolean
is_alt (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'a' || string[1] == 'A') &&
	  (string[2] == 'l' || string[2] == 'L') &&
	  (string[3] == 't' || string[3] == 'T') &&
	  (string[4] == '>'));
}

static inline gboolean
is_ctl (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'c' || string[1] == 'C') &&
	  (string[2] == 't' || string[2] == 'T') &&
	  (string[3] == 'l' || string[3] == 'L') &&
	  (string[4] == '>'));
}

static inline gboolean
is_modx (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'm' || string[1] == 'M') &&
	  (string[2] == 'o' || string[2] == 'O') &&
	  (string[3] == 'd' || string[3] == 'D') &&
	  (string[4] >= '1' && string[4] <= '5') &&
	  (string[5] == '>'));
}

static inline gboolean
is_ctrl (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'c' || string[1] == 'C') &&
	  (string[2] == 't' || string[2] == 'T') &&
	  (string[3] == 'r' || string[3] == 'R') &&
	  (string[4] == 'l' || string[4] == 'L') &&
	  (string[5] == '>'));
}

static inline gboolean
is_shft (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 's' || string[1] == 'S') &&
	  (string[2] == 'h' || string[2] == 'H') &&
	  (string[3] == 'f' || string[3] == 'F') &&
	  (string[4] == 't' || string[4] == 'T') &&
	  (string[5] == '>'));
}

static inline gboolean
is_shift (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 's' || string[1] == 'S') &&
	  (string[2] == 'h' || string[2] == 'H') &&
	  (string[3] == 'i' || string[3] == 'I') &&
	  (string[4] == 'f' || string[4] == 'F') &&
	  (string[5] == 't' || string[5] == 'T') &&
	  (string[6] == '>'));
}

static inline gboolean
is_control (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'c' || string[1] == 'C') &&
	  (string[2] == 'o' || string[2] == 'O') &&
	  (string[3] == 'n' || string[3] == 'N') &&
	  (string[4] == 't' || string[4] == 'T') &&
	  (string[5] == 'r' || string[5] == 'R') &&
	  (string[6] == 'o' || string[6] == 'O') &&
	  (string[7] == 'l' || string[7] == 'L') &&
	  (string[8] == '>'));
}

static inline gboolean
is_release (const gchar *string)
{
  return ((string[0] == '<') &&
	  (string[1] == 'r' || string[1] == 'R') &&
	  (string[2] == 'e' || string[2] == 'E') &&
	  (string[3] == 'l' || string[3] == 'L') &&
	  (string[4] == 'e' || string[4] == 'E') &&
	  (string[5] == 'a' || string[5] == 'A') &&
	  (string[6] == 's' || string[6] == 'S') &&
	  (string[7] == 'e' || string[7] == 'E') &&
	  (string[8] == '>'));
}

/**
 * gtk_accelerator_parse
 * @accelerator:      string representing an accelerator
 * @accelerator_key:  return location for accelerator keyval
 * @accelerator_mods: return location for accelerator modifier mask
 *
 * Parses a string representing an accelerator. The
 * format looks like "<Control>a" or "<Shift><Alt>F1" or
 * "<Release>z" (the last one is for key release).
 * The parser is fairly liberal and allows lower or upper case,
 * and also abbreviations such as "<Ctl>" and "<Ctrl>".
 *
 * If the parse fails, @accelerator_key and @accelerator_mods will
 * be set to 0 (zero).
 */
void
gtk_accelerator_parse (const gchar     *accelerator,
		       guint           *accelerator_key,
		       GdkModifierType *accelerator_mods)
{
  guint keyval;
  GdkModifierType mods;
  gint len;
  
  if (accelerator_key)
    *accelerator_key = 0;
  if (accelerator_mods)
    *accelerator_mods = 0;
  g_return_if_fail (accelerator != NULL);
  
  keyval = 0;
  mods = 0;
  len = strlen (accelerator);
  while (len)
    {
      if (*accelerator == '<')
	{
	  if (len >= 9 && is_release (accelerator))
	    {
	      accelerator += 9;
	      len -= 9;
	      mods |= GDK_RELEASE_MASK;
	    }
	  else if (len >= 9 && is_control (accelerator))
	    {
	      accelerator += 9;
	      len -= 9;
	      mods |= GDK_CONTROL_MASK;
	    }
	  else if (len >= 7 && is_shift (accelerator))
	    {
	      accelerator += 7;
	      len -= 7;
	      mods |= GDK_SHIFT_MASK;
	    }
	  else if (len >= 6 && is_shft (accelerator))
	    {
	      accelerator += 6;
	      len -= 6;
	      mods |= GDK_SHIFT_MASK;
	    }
	  else if (len >= 6 && is_ctrl (accelerator))
	    {
	      accelerator += 6;
	      len -= 6;
	      mods |= GDK_CONTROL_MASK;
	    }
	  else if (len >= 6 && is_modx (accelerator))
	    {
	      static const guint mod_vals[] = {
		GDK_MOD1_MASK, GDK_MOD2_MASK, GDK_MOD3_MASK,
		GDK_MOD4_MASK, GDK_MOD5_MASK
	      };

	      len -= 6;
	      accelerator += 4;
	      mods |= mod_vals[*accelerator - '1'];
	      accelerator += 2;
	    }
	  else if (len >= 5 && is_ctl (accelerator))
	    {
	      accelerator += 5;
	      len -= 5;
	      mods |= GDK_CONTROL_MASK;
	    }
	  else if (len >= 5 && is_alt (accelerator))
	    {
	      accelerator += 5;
	      len -= 5;
	      mods |= GDK_MOD1_MASK;
	    }
	  else
	    {
	      gchar last_ch;
	      
	      last_ch = *accelerator;
	      while (last_ch && last_ch != '>')
		{
		  last_ch = *accelerator;
		  accelerator += 1;
		  len -= 1;
		}
	    }
	}
      else
	{
	  keyval = gdk_keyval_from_name (accelerator);
	  accelerator += len;
	  len -= len;
	}
    }
  
  if (accelerator_key)
    *accelerator_key = gdk_keyval_to_lower (keyval);
  if (accelerator_mods)
    *accelerator_mods = mods;
}

/**
 * gtk_accelerator_name
 * @accelerator_key:  accelerator keyval
 * @accelerator_mods: accelerator modifier mask
 * @returns:          a newly allocated accelerator name
 * 
 * Converts an accelerator keyval and modifier mask
 * into a string parseable by gtk_accelerator_parse().
 * For example, if you pass in GDK_q and GDK_CONTROL_MASK,
 * this function returns "<Control>q". 
 *
 * The caller of this function must free the returned string.
 */
gchar*
gtk_accelerator_name (guint           accelerator_key,
		      GdkModifierType accelerator_mods)
{
  static const gchar text_release[] = "<Release>";
  static const gchar text_shift[] = "<Shift>";
  static const gchar text_control[] = "<Control>";
  static const gchar text_mod1[] = "<Alt>";
  static const gchar text_mod2[] = "<Mod2>";
  static const gchar text_mod3[] = "<Mod3>";
  static const gchar text_mod4[] = "<Mod4>";
  static const gchar text_mod5[] = "<Mod5>";
  guint l;
  gchar *keyval_name;
  gchar *accelerator;

  accelerator_mods &= GDK_MODIFIER_MASK;

  keyval_name = gdk_keyval_name (gdk_keyval_to_lower (accelerator_key));
  if (!keyval_name)
    keyval_name = "";

  l = 0;
  if (accelerator_mods & GDK_RELEASE_MASK)
    l += sizeof (text_release) - 1;
  if (accelerator_mods & GDK_SHIFT_MASK)
    l += sizeof (text_shift) - 1;
  if (accelerator_mods & GDK_CONTROL_MASK)
    l += sizeof (text_control) - 1;
  if (accelerator_mods & GDK_MOD1_MASK)
    l += sizeof (text_mod1) - 1;
  if (accelerator_mods & GDK_MOD2_MASK)
    l += sizeof (text_mod2) - 1;
  if (accelerator_mods & GDK_MOD3_MASK)
    l += sizeof (text_mod3) - 1;
  if (accelerator_mods & GDK_MOD4_MASK)
    l += sizeof (text_mod4) - 1;
  if (accelerator_mods & GDK_MOD5_MASK)
    l += sizeof (text_mod5) - 1;
  l += strlen (keyval_name);

  accelerator = g_new (gchar, l + 1);

  l = 0;
  accelerator[l] = 0;
  if (accelerator_mods & GDK_RELEASE_MASK)
    {
      strcpy (accelerator + l, text_release);
      l += sizeof (text_release) - 1;
    }
  if (accelerator_mods & GDK_SHIFT_MASK)
    {
      strcpy (accelerator + l, text_shift);
      l += sizeof (text_shift) - 1;
    }
  if (accelerator_mods & GDK_CONTROL_MASK)
    {
      strcpy (accelerator + l, text_control);
      l += sizeof (text_control) - 1;
    }
  if (accelerator_mods & GDK_MOD1_MASK)
    {
      strcpy (accelerator + l, text_mod1);
      l += sizeof (text_mod1) - 1;
    }
  if (accelerator_mods & GDK_MOD2_MASK)
    {
      strcpy (accelerator + l, text_mod2);
      l += sizeof (text_mod2) - 1;
    }
  if (accelerator_mods & GDK_MOD3_MASK)
    {
      strcpy (accelerator + l, text_mod3);
      l += sizeof (text_mod3) - 1;
    }
  if (accelerator_mods & GDK_MOD4_MASK)
    {
      strcpy (accelerator + l, text_mod4);
      l += sizeof (text_mod4) - 1;
    }
  if (accelerator_mods & GDK_MOD5_MASK)
    {
      strcpy (accelerator + l, text_mod5);
      l += sizeof (text_mod5) - 1;
    }
  strcpy (accelerator + l, keyval_name);

  return accelerator;
}

/**
 * gtk_accelerator_set_default_mod_mask
 * @default_mod_mask: accelerator modifier mask
 *
 * Sets the modifiers that will be considered significant for keyboard
 * accelerators. The default mod mask is #GDK_CONTROL_MASK |
 * #GDK_SHIFT_MASK | #GDK_MOD1_MASK, that is, Control, Shift, and Alt.
 * Other modifiers will by default be ignored by #GtkAccelGroup.
 *
 * The default mod mask should be changed on application startup,
 * before using any accelerator groups.
 */
void
gtk_accelerator_set_default_mod_mask (GdkModifierType default_mod_mask)
{
  default_accel_mod_mask = default_mod_mask & GDK_MODIFIER_MASK;
}

/**
 * gtk_accelerator_get_default_mod_mask
 * @returns: the default accelerator modifier mask
 *
 * Gets the value set by gtk_accelerator_set_default_mod_mask().
 */
guint
gtk_accelerator_get_default_mod_mask (void)
{
  return default_accel_mod_mask;
}
