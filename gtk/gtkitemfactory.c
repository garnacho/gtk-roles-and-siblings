/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkItemFactory: Flexible item factory with automatic rc handling
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include	"gtkitemfactory.h"
#include	"gtk/gtksignal.h"
#include	"gtk/gtkoptionmenu.h"
#include	"gtk/gtkmenubar.h"
#include	"gtk/gtkmenu.h"
#include	"gtk/gtkmenuitem.h"
#include	"gtk/gtkradiomenuitem.h"
#include	"gtk/gtkcheckmenuitem.h"
#include	"gtk/gtkaccellabel.h"
#include	<string.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<unistd.h>



/* --- defines --- */
#define		PARENT_DELIMITER	('/')
#define		ITEM_FACTORY_STRING	((gchar*) item_factory_string)
#define		ITEM_BLOCK_SIZE		(128)


/* --- structures --- */
typedef struct	_GtkIFCBData		GtkIFCBData;
typedef struct	_GtkIFActionLink	GtkIFActionLink;
struct _GtkIFCBData
{
  GtkItemFactoryCallback  func;
  guint			  callback_type;
  gpointer		  func_data;
  guint			  callback_action;
};
struct _GtkIFActionLink
{
  GtkWidget *widget;
  guint callback_action;
};


/* --- prototypes --- */
static void	gtk_item_factory_class_init		(GtkItemFactoryClass  *klass);
static void	gtk_item_factory_init			(GtkItemFactory	      *ifactory);
static void	gtk_item_factory_destroy		(GtkObject	      *object);
static void	gtk_item_factory_finalize		(GtkObject	      *object);


/* --- static variables --- */
static GtkItemFactoryClass *gtk_item_factory_class = NULL;
static GtkObjectClass	*parent_class = NULL;
static const gchar	*item_factory_string = "Gtk-<ItemFactory>";
static GMemChunk	*ifactory_item_chunks = NULL;
static GMemChunk	*ifactory_cb_data_chunks = NULL;
static const gchar	*key_popup_data = "GtkItemFactory-popup-data";
static guint		 key_id_popup_data = 0;
static const gchar	*key_if_menu_pos = "GtkItemFactory-menu-position";
static guint		 key_id_if_menu_pos = 0;
static const gchar	*key_item_factory = "GtkItemFactory";
static guint		 key_id_item_factory = 0;
static const gchar	*key_item_factory_path = "GtkItemFactory-path";
static guint		 key_id_item_factory_path = 0;
static const gchar	*key_type_item = "<Item>";
static guint		 key_id_type_item = 0;
static const gchar	*key_type_title = "<Title>";
static guint		 key_id_type_title = 0;
static const gchar	*key_type_radio_item = "<RadioItem>";
static guint		 key_id_type_radio_item = 0;
static const gchar	*key_type_check_item = "<CheckItem>";
static guint		 key_id_type_check_item = 0;
static const gchar	*key_type_toggle_item = "<ToggleItem>";
static guint		 key_id_type_toggle_item = 0;
static const gchar	*key_type_separator_item = "<Separator>";
static guint		 key_id_type_separator_item = 0;
static const gchar	*key_type_branch = "<Branch>";
static guint		 key_id_type_branch = 0;
static const gchar	*key_type_last_branch = "<LastBranch>";
static guint		 key_id_type_last_branch = 0;
static	GScannerConfig	ifactory_scanner_config =
{
  (
   " \t\n"
   )			/* cset_skip_characters */,
  (
   G_CSET_a_2_z
   "_"
   G_CSET_A_2_Z
   )			/* cset_identifier_first */,
  (
   G_CSET_a_2_z
   "-+_0123456789"
   G_CSET_A_2_Z
   G_CSET_LATINS
   G_CSET_LATINC
   )			/* cset_identifier_nth */,
  ( ";\n" )		/* cpair_comment_single */,
  
  FALSE			/* case_sensitive */,
  
  TRUE			/* skip_comment_multi */,
  TRUE			/* skip_comment_single */,
  FALSE			/* scan_comment_multi */,
  TRUE			/* scan_identifier */,
  FALSE			/* scan_identifier_1char */,
  FALSE			/* scan_identifier_NULL */,
  TRUE			/* scan_symbols */,
  TRUE			/* scan_binary */,
  TRUE			/* scan_octal */,
  TRUE			/* scan_float */,
  TRUE			/* scan_hex */,
  FALSE			/* scan_hex_dollar */,
  TRUE			/* scan_string_sq */,
  TRUE			/* scan_string_dq */,
  TRUE			/* numbers_2_int */,
  FALSE			/* int_2_float */,
  FALSE			/* identifier_2_string */,
  TRUE			/* char_2_token */,
  FALSE			/* symbol_2_token */,
};


/* --- functions --- */
GtkType
gtk_item_factory_get_type (void)
{
  static GtkType item_factory_type = 0;
  
  if (!item_factory_type)
    {
      GtkTypeInfo item_factory_info =
      {
	"GtkItemFactory",
	sizeof (GtkItemFactory),
	sizeof (GtkItemFactoryClass),
	(GtkClassInitFunc) gtk_item_factory_class_init,
	(GtkObjectInitFunc) gtk_item_factory_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL,
      };
      
      item_factory_type = gtk_type_unique (gtk_object_get_type (), &item_factory_info);
    }
  
  return item_factory_type;
}

static void
gtk_item_factory_class_init (GtkItemFactoryClass  *class)
{
  GtkObjectClass *object_class;

  gtk_item_factory_class = class;

  parent_class = gtk_type_class (gtk_object_get_type ());

  object_class = (GtkObjectClass*) class;

  object_class->destroy = gtk_item_factory_destroy;
  object_class->finalize = gtk_item_factory_finalize;

  class->cpair_comment_single = g_strdup (";\n");

  class->item_ht = g_hash_table_new (g_str_hash, g_str_equal);
  ifactory_item_chunks =
    g_mem_chunk_new ("GtkItemFactoryItem",
		     sizeof (GtkItemFactoryItem),
		     sizeof (GtkItemFactoryItem) * ITEM_BLOCK_SIZE,
		     G_ALLOC_ONLY);
  ifactory_cb_data_chunks =
    g_mem_chunk_new ("GtkIFCBData",
		     sizeof (GtkIFCBData),
		     sizeof (GtkIFCBData) * ITEM_BLOCK_SIZE,
		     G_ALLOC_AND_FREE);

  key_id_popup_data = gtk_object_data_force_id (key_popup_data);
  key_id_if_menu_pos = gtk_object_data_force_id (key_if_menu_pos);
  key_id_item_factory = gtk_object_data_force_id (key_item_factory);
  key_id_item_factory_path = gtk_object_data_force_id (key_item_factory_path);
  key_id_type_item = gtk_object_data_force_id (key_type_item);
  key_id_type_title = gtk_object_data_force_id (key_type_title);
  key_id_type_radio_item = gtk_object_data_force_id (key_type_radio_item);
  key_id_type_check_item = gtk_object_data_force_id (key_type_check_item);
  key_id_type_toggle_item = gtk_object_data_force_id (key_type_toggle_item);
  key_id_type_separator_item = gtk_object_data_force_id (key_type_separator_item);
  key_id_type_branch = gtk_object_data_force_id (key_type_branch);
  key_id_type_last_branch = gtk_object_data_force_id (key_type_last_branch);
}

static void
gtk_item_factory_init (GtkItemFactory	    *ifactory)
{
  GtkObject *object;

  object = GTK_OBJECT (ifactory);

  ifactory->path = NULL;
  ifactory->accel_group = NULL;
  ifactory->widget = NULL;
  ifactory->widgets_by_action = NULL;
}

GtkItemFactory*
gtk_item_factory_new (GtkType	     container_type,
		      const gchar   *path,
		      GtkAccelGroup *accel_group)
{
  GtkItemFactory *ifactory;

  g_return_val_if_fail (path != NULL, NULL);

  ifactory = gtk_type_new (gtk_item_factory_get_type ());
  gtk_item_factory_construct (ifactory, container_type, path, accel_group);

  return ifactory;
}

static void
gtk_item_factory_callback_marshal (GtkWidget *widget,
				   gpointer   func_data)
{
  GtkIFCBData *data;

  data = func_data;

  if (data->callback_type == 1)
    {
      GtkItemFactoryCallback1 func1 = data->func;
      func1 (data->func_data, data->callback_action, widget);
    }
  else if (data->callback_type == 2)
    {
      GtkItemFactoryCallback2 func2 = data->func;
      func2 (widget, data->func_data, data->callback_action);
    }
}

static void
gtk_item_factory_propagate_accelerator (GtkItemFactoryItem *item,
					GtkWidget          *exclude)
{
  GSList *widget_list;
  GSList *slist;
  
  if (item->in_propagation)
    return;
  
  item->in_propagation = TRUE;
  
  widget_list = NULL;
  for (slist = item->widgets; slist; slist = slist->next)
    {
      GtkWidget *widget;
      
      widget = slist->data;
      
      if (widget != exclude)
	{
	  gtk_widget_ref (widget);
	  widget_list = g_slist_prepend (widget_list, widget);
	}
    }
  
  for (slist = widget_list; slist; slist = slist->next)
    {
      GtkWidget *widget;
      GtkItemFactory *ifactory;
      
      widget = slist->data;
      
      ifactory = gtk_item_factory_from_widget (widget);
      
      if (ifactory)
	{
	  guint signal_id;
	  
	  signal_id = gtk_signal_lookup ("activate", GTK_OBJECT_TYPE (widget));
	  if (signal_id)
	    {
	      if (item->accelerator_key)
		gtk_widget_add_accelerator (widget,
					    "activate",
					    ifactory->accel_group,
					    item->accelerator_key,
					    item->accelerator_mods,
					    GTK_ACCEL_VISIBLE);
	      else
		{
		  GSList *slist;
		  
		  slist = gtk_accel_group_entries_from_object (GTK_OBJECT (widget));
		  while (slist)
		    {
		      GtkAccelEntry *ac_entry;
		      
		      ac_entry = slist->data;
		      slist = slist->next;
		      if (ac_entry->accel_flags & GTK_ACCEL_VISIBLE &&
			  ac_entry->accel_group == ifactory->accel_group &&
			  ac_entry->signal_id == signal_id)
			gtk_widget_remove_accelerator (GTK_WIDGET (widget),
						       ac_entry->accel_group,
						       ac_entry->accelerator_key,
						       ac_entry->accelerator_mods);
		    }
		}
	    }
	}
      gtk_widget_unref (widget);
    }
  g_slist_free (widget_list);
  
  item->in_propagation = FALSE;
}

static gint
gtk_item_factory_item_add_accelerator (GtkWidget	  *widget,
				       guint               accel_signal_id,
				       GtkAccelGroup      *accel_group,
				       guint               accel_key,
				       guint               accel_mods,
				       GtkAccelFlags       accel_flags,
				       GtkItemFactoryItem *item)
{
  if (!item->in_propagation &&
      g_slist_find (item->widgets, widget) &&
      accel_signal_id == gtk_signal_lookup ("activate", GTK_OBJECT_TYPE (widget)))
    {
      item->accelerator_key = accel_key;
      item->accelerator_mods = accel_mods;
      item->modified = TRUE;
      
      gtk_item_factory_propagate_accelerator (item, widget);
    }

  return TRUE;
}

static void
gtk_item_factory_item_remove_accelerator (GtkWidget	     *widget,
					  GtkAccelGroup      *accel_group,
					  guint               accel_key,
					  guint               accel_mods,
					  GtkItemFactoryItem *item)
{
  if (!item->in_propagation &&
      g_slist_find (item->widgets, widget) &&
      item->accelerator_key == accel_key &&
      item->accelerator_mods == accel_mods)
    {
      item->accelerator_key = 0;
      item->accelerator_mods = 0;
      item->modified = TRUE;
      
      gtk_item_factory_propagate_accelerator (item, widget);
    }
}

static void
gtk_item_factory_item_remove_widget (GtkWidget		*widget,
				     GtkItemFactoryItem *item)
{
  item->widgets = g_slist_remove (item->widgets, widget);
  gtk_object_remove_data_by_id (GTK_OBJECT (widget), key_id_item_factory);
  gtk_object_remove_data_by_id (GTK_OBJECT (widget), key_id_item_factory_path);
}

static void
ifactory_cb_data_free (gpointer mem)
{
  g_mem_chunk_free (ifactory_cb_data_chunks, mem);
}

static void
gtk_item_factory_add_item (GtkItemFactory		*ifactory,
			   const gchar			*path,
			   const gchar			*accelerator,
			   GtkItemFactoryCallback	callback,
			   guint			callback_action,
			   gpointer			callback_data,
			   guint			callback_type,
			   gchar			*item_type,
			   GtkWidget			*widget)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;
  gchar *fpath;
  
  g_return_if_fail (widget != NULL);

  class = GTK_ITEM_FACTORY_CLASS (GTK_OBJECT (ifactory)->klass);

  fpath = g_strconcat (ifactory->path, path, NULL);
  item = g_hash_table_lookup (class->item_ht, fpath);

  /* link the widget into its item-entry
   */
  if (!item)
    {
      guint keyval;
      guint mods;

      if (accelerator)
	gtk_accelerator_parse (accelerator, &keyval, &mods);
      else
	{
	  keyval = 0;
	  mods = 0;
	}

      item = g_chunk_new (GtkItemFactoryItem, ifactory_item_chunks);

      item->path = fpath;
      fpath = NULL;
      item->accelerator_key = keyval;
      item->accelerator_mods = mods;
      item->modified = FALSE;
      item->in_propagation = FALSE;
      item->item_type = NULL;
      item->widgets = NULL;
      
      g_hash_table_insert (class->item_ht, item->path, item);
    }
  g_free (fpath);

  if (item->item_type == NULL)
    {
      g_assert (item->widgets == NULL);
      
      if (item_type != ITEM_FACTORY_STRING)
	item->item_type = g_strdup (item_type);
      else
	item->item_type = ITEM_FACTORY_STRING;
    }

  item->widgets = g_slist_prepend (item->widgets, widget);
  gtk_signal_connect (GTK_OBJECT (widget),
		      "destroy",
		      GTK_SIGNAL_FUNC (gtk_item_factory_item_remove_widget),
		      item);

  /* set back pointers for the widget
   */
  gtk_object_set_data_by_id (GTK_OBJECT (widget), key_id_item_factory, ifactory);
  gtk_object_set_data_by_id (GTK_OBJECT (widget), key_id_item_factory_path, item->path);
  gtk_widget_set_name (widget, item->path);

  /* set accelerator group on menu widgets
   */
  if (GTK_IS_MENU (widget))
    gtk_menu_set_accel_group ((GtkMenu*) widget, ifactory->accel_group);

  /* install defined accelerators
   */
  if (gtk_signal_lookup ("activate", GTK_OBJECT_TYPE (widget)))
    {
      if (item->accelerator_key)
	gtk_widget_add_accelerator (widget,
				    "activate",
				    ifactory->accel_group,
				    item->accelerator_key,
				    item->accelerator_mods,
				    GTK_ACCEL_VISIBLE);
      else
	gtk_widget_remove_accelerators (widget,
					"activate",
					TRUE);
    }

  /* keep track of accelerator changes
   */
  gtk_signal_connect_after (GTK_OBJECT (widget),
			    "add-accelerator",
			    GTK_SIGNAL_FUNC (gtk_item_factory_item_add_accelerator),
			    item);
  gtk_signal_connect_after (GTK_OBJECT (widget),
			    "remove-accelerator",
			    GTK_SIGNAL_FUNC (gtk_item_factory_item_remove_accelerator),
			    item);

  /* keep a per-action list of the widgets on the factory
   */
  if (callback_action)
    {
      GtkIFActionLink *link;

      link = g_new (GtkIFActionLink, 1);
      link->widget = widget;
      link->callback_action = callback_action;
      ifactory->widgets_by_action = g_slist_prepend (ifactory->widgets_by_action, link);
    }

  /* connect callback if neccessary
   */
  if (callback)
    {
      GtkIFCBData *data;

      data = g_chunk_new (GtkIFCBData, ifactory_cb_data_chunks);
      data->func = callback;
      data->callback_type = callback_type;
      data->func_data = callback_data;
      data->callback_action = callback_action;

      gtk_object_weakref (GTK_OBJECT (widget),
			  ifactory_cb_data_free,
			  data);
      gtk_signal_connect (GTK_OBJECT (widget),
			  "activate",
			  GTK_SIGNAL_FUNC (gtk_item_factory_callback_marshal),
			  data);
    }
}

void
gtk_item_factory_construct (GtkItemFactory	*ifactory,
			    GtkType		 container_type,
			    const gchar		*path,
			    GtkAccelGroup	*accel_group)
{
  guint len;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (ifactory->accel_group == NULL);
  g_return_if_fail (path != NULL);
  if (!gtk_type_is_a (container_type, gtk_option_menu_get_type ()))
    g_return_if_fail (gtk_type_is_a (container_type, gtk_menu_shell_get_type ()));

  len = strlen (path);

  if (path[0] != '<' && path[len - 1] != '>')
    {
      g_warning ("GtkItemFactory: invalid factory path `%s'", path);
      return;
    }

  if (accel_group)
    {
      ifactory->accel_group = accel_group;
      gtk_accel_group_ref (ifactory->accel_group);
    }
  else
    ifactory->accel_group = gtk_accel_group_new ();

  ifactory->path = g_strdup (path);
  ifactory->widget =
    gtk_widget_new (container_type,
		    "GtkObject::signal::destroy", gtk_widget_destroyed, &ifactory->widget,
		    NULL);
  gtk_object_ref (GTK_OBJECT (ifactory));
  gtk_object_sink (GTK_OBJECT (ifactory));
  gtk_signal_connect_object_while_alive (GTK_OBJECT (ifactory->widget),
					 "destroy",
					 GTK_SIGNAL_FUNC (gtk_widget_destroy),
					 GTK_OBJECT (ifactory));
  gtk_item_factory_add_item (ifactory,
			     "", NULL,
			     NULL, 0, NULL, 0,
			     ITEM_FACTORY_STRING,
			     ifactory->widget);
}

static void
gtk_item_factory_destroy (GtkObject		 *object)
{
  GtkItemFactory *ifactory;
  GSList *slist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (object));

  ifactory = (GtkItemFactory*) object;

  if (ifactory->widget)
    {
      GtkObject *object;

      object = GTK_OBJECT (ifactory->widget);

      gtk_object_ref (object);
      gtk_object_sink (object);
      gtk_object_destroy (object);
      gtk_object_unref (object);

      ifactory->widget = NULL;
    }

  for (slist = ifactory->widgets_by_action; slist; slist = slist->next)
    g_free (slist->data);
  g_slist_free (ifactory->widgets_by_action);
  ifactory->widgets_by_action = NULL;

  parent_class->destroy (object);
}

static void
gtk_item_factory_finalize (GtkObject		  *object)
{
  GtkItemFactory *ifactory;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (object));

  ifactory = GTK_ITEM_FACTORY (object);

  gtk_accel_group_unref (ifactory->accel_group);
  g_free (ifactory->path);
  g_assert (ifactory->widget == NULL);

  parent_class->finalize (object);
}

GtkItemFactory*
gtk_item_factory_from_widget (GtkWidget	       *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gtk_object_get_data_by_id (GTK_OBJECT (widget), key_id_item_factory);
}

gchar*
gtk_item_factory_path_from_widget (GtkWidget	    *widget)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gtk_object_get_data_by_id (GTK_OBJECT (widget), key_id_item_factory_path);
}

typedef struct
{
  GtkPrintFunc	       print_func;
  gpointer	       func_data;
  guint		       modified_only : 1;
  guint		       path_length;
  const gchar	      *path;
} DumpLimiterData;

static void
gtk_item_factory_foreach (gpointer hash_key,
			  gpointer value,
			  gpointer user_data)
{
  GtkItemFactoryItem *item;
  DumpLimiterData *data;
  gchar *string;
  gchar *name;
  gchar comment_prefix[2] = "\000\000";

  item = value;
  data = user_data;

  if ((data->path && strncmp (item->path, data->path, data->path_length)) ||
      (data->modified_only && !item->modified))
    return;

  comment_prefix[0] = gtk_item_factory_class->cpair_comment_single[0];

  name = gtk_accelerator_name (item->accelerator_key, item->accelerator_mods);
  string = g_strconcat (item->modified ? "" : comment_prefix,
			"(menu-path \"",
			hash_key,
			"\" \"",
			name,
			"\")",
			NULL);
  g_free (name);

  data->print_func (data->func_data, string);

  g_free (string);
}

void
gtk_item_factory_dump_rc (const gchar		 *ifactory_path,
			  gboolean		  modified_only,
			  GtkPrintFunc		  print_func,
			  gpointer		  func_data)
{
  DumpLimiterData data;

  g_return_if_fail (print_func != NULL);

  if (!gtk_item_factory_class)
    gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  data.print_func = print_func;
  data.func_data = func_data;
  data.modified_only = (modified_only != FALSE);
  data.path_length = ifactory_path ? strlen (ifactory_path) : 0;
  data.path = ifactory_path;

  g_hash_table_foreach (gtk_item_factory_class->item_ht, gtk_item_factory_foreach, &data);
}

void
gtk_item_factory_create_items (GtkItemFactory	      *ifactory,
			       guint		       n_entries,
			       GtkItemFactoryEntry    *entries,
			       gpointer		       callback_data)
{
  gtk_item_factory_create_items_ac (ifactory, n_entries, entries, callback_data, 1);
}

void
gtk_item_factory_create_items_ac (GtkItemFactory       *ifactory,
				  guint		        n_entries,
				  GtkItemFactoryEntry  *entries,
				  gpointer		callback_data,
				  guint			callback_type)
{
  guint i;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (callback_type >= 1 && callback_type <= 2);

  if (n_entries == 0)
    return;

  g_return_if_fail (entries != NULL);

  for (i = 0; i < n_entries; i++)
    gtk_item_factory_create_item (ifactory, entries + i, callback_data, callback_type);
}

GtkWidget*
gtk_item_factory_get_widget (GtkItemFactory   *ifactory,
			     const gchar      *path)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;
  gchar *fpath;

  g_return_val_if_fail (ifactory != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  class = GTK_ITEM_FACTORY_CLASS (GTK_OBJECT (ifactory)->klass);

  fpath = g_strconcat (ifactory->path, path, NULL);
  item = g_hash_table_lookup (class->item_ht, fpath);
  g_free (fpath);

  if (item)
    {
      GSList *slist;

      for (slist = item->widgets; slist; slist = slist->next)
	{
	  if (gtk_item_factory_from_widget (slist->data) == ifactory)
	    return slist->data;
	}
    }

  return NULL;
}

GtkWidget*
gtk_item_factory_get_widget_by_action (GtkItemFactory   *ifactory,
				       guint	         action)
{
  GSList *slist;

  g_return_val_if_fail (ifactory != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);

  for (slist = ifactory->widgets_by_action; slist; slist = slist->next)
    {
      GtkIFActionLink *link;

      link = slist->data;

      if (link->callback_action == action)
	return link->widget;
    }

  return NULL;
}

void
gtk_item_factory_create_item (GtkItemFactory	     *ifactory,
			      GtkItemFactoryEntry    *entry,
			      gpointer		      callback_data,
			      guint		      callback_type)
{
  GtkWidget *parent;
  GtkWidget *widget;
  GSList *radio_group;
  gchar *parent_path;
  gchar *p;
  guint type_id;
  GtkType type;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (entry != NULL);
  g_return_if_fail (entry->path != NULL);
  g_return_if_fail (entry->path[0] == PARENT_DELIMITER);
  g_return_if_fail (callback_type >= 1 && callback_type <= 2);

  if (!entry->item_type ||
      entry->item_type == 0)
    type_id = key_id_type_item;
  else
    type_id = gtk_object_data_try_key (entry->item_type);

  radio_group = NULL;
  if (type_id == key_id_type_item)
    type = gtk_menu_item_get_type ();
  else if (type_id == key_id_type_title)
    type = gtk_menu_item_get_type ();
  else if (type_id == key_id_type_radio_item)
    type = gtk_radio_menu_item_get_type ();
  else if (type_id == key_id_type_check_item)
    type = gtk_check_menu_item_get_type ();
  else if (type_id == key_id_type_toggle_item)
    type = gtk_check_menu_item_get_type ();
  else if (type_id == key_id_type_separator_item)
    type = gtk_menu_item_get_type ();
  else if (type_id == key_id_type_branch)
    type = gtk_menu_item_get_type ();
  else if (type_id == key_id_type_last_branch)
    type = gtk_menu_item_get_type ();
  else
    {
      GtkWidget *radio_link;

      radio_link = gtk_item_factory_get_widget (ifactory, entry->item_type);
      if (radio_link && GTK_IS_RADIO_MENU_ITEM (radio_link))
	{
	  type = gtk_radio_menu_item_get_type ();
	  radio_group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (radio_link));
	}
      else
	{
	  g_warning ("GtkItemFactory: entry path `%s' has invalid type `%s'",
		     entry->path,
		     entry->item_type);
	  return;
	}
    }
  
  parent_path = g_strdup (entry->path);
  p = strrchr (parent_path, PARENT_DELIMITER);
  if (!p)
    {
      g_warning ("GtkItemFactory: invalid entry path `%s'", entry->path);
      return;
    }
  *p = 0;

  parent = gtk_item_factory_get_widget (ifactory, parent_path);
  if (!parent)
    {
      GtkItemFactoryEntry pentry;

      pentry.path = parent_path;
      pentry.accelerator = NULL;
      pentry.callback = NULL;
      pentry.callback_action = 0;
      pentry.item_type = "<Branch>";

      gtk_item_factory_create_item (ifactory, &pentry, NULL, 1);

      parent = gtk_item_factory_get_widget (ifactory, parent_path);
    }
  g_free (parent_path);

  g_return_if_fail (parent != NULL);
  
  p = strrchr (entry->path, PARENT_DELIMITER);
  p++;
  
  widget = gtk_widget_new (type,
			   "GtkWidget::visible", TRUE,
			   "GtkWidget::sensitive", (type_id != key_id_type_separator_item &&
						    type_id != key_id_type_title),
			   "GtkWidget::parent", parent,
			   NULL);

  if (type == gtk_radio_menu_item_get_type ())
    gtk_radio_menu_item_set_group (GTK_RADIO_MENU_ITEM (widget), radio_group);
  if (GTK_IS_CHECK_MENU_ITEM (widget))
    gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (widget), TRUE);
    
  if (type_id != key_id_type_separator_item && *p)
    {
      GtkWidget *label;
      
      label =
	gtk_widget_new (GTK_TYPE_ACCEL_LABEL,
			"GtkLabel::label", p,
			"GtkWidget::visible", TRUE,
			"GtkWidget::parent", widget,
			"GtkAccelLabel::accel_widget", widget,
			"GtkMisc::xalign", 0.0,
			NULL);
    }
  if (type_id == key_id_type_branch ||
      type_id == key_id_type_last_branch)
    {
      if (type_id == key_id_type_last_branch)
	gtk_menu_item_right_justify (GTK_MENU_ITEM (widget));
	
      parent = widget;
      widget =
	gtk_widget_new (gtk_menu_get_type (),
			NULL);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (parent), widget);
    }	   
  
  gtk_item_factory_add_item (ifactory,
			     entry->path, entry->accelerator,
			     entry->callback, entry->callback_action, callback_data,
			     callback_type,
			     entry->item_type,
			     widget);
}

void
gtk_item_factory_path_delete (const gchar *ifactory_path,
			      const gchar *path)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;
  gchar *fpath;

  g_return_if_fail (ifactory_path != NULL);
  g_return_if_fail (path != NULL);

  class = gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  fpath = g_strconcat (ifactory_path, path, NULL);
  item = g_hash_table_lookup (class->item_ht, fpath);
  g_free (fpath);

  if (item)
    {
      GSList *widget_list;
      GSList *slist;

      widget_list = NULL;
      for (slist = item->widgets; slist; slist = slist->next)
	{
	  GtkWidget *widget;

	  widget = slist->data;
	  widget_list = g_slist_prepend (widget_list, widget);
	  gtk_widget_ref (widget);
	}

      for (slist = widget_list; slist; slist = slist->next)
	{
	  GtkWidget *widget;

	  widget = slist->data;
	  gtk_widget_destroy (widget);
	  gtk_widget_unref (widget);
	}
      g_slist_free (widget_list);
    }
}

void
gtk_item_factory_delete_item (GtkItemFactory         *ifactory,
			      const gchar            *path)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;
  gchar *fpath;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (path != NULL);

  class = GTK_ITEM_FACTORY_CLASS (GTK_OBJECT (ifactory)->klass);

  fpath = g_strconcat (ifactory->path, path, NULL);
  item = g_hash_table_lookup (class->item_ht, fpath);
  g_free (fpath);

  if (item)
    {
      GtkWidget *widget = NULL;
      GSList *slist;

      for (slist = item->widgets; slist; slist = slist->next)
	{
	  widget = slist->data;

	  if (gtk_item_factory_from_widget (widget) == ifactory)
	    break;
	}

      if (slist)
	gtk_widget_destroy (widget);
    }
}

void
gtk_item_factory_delete_entry (GtkItemFactory         *ifactory,
			       GtkItemFactoryEntry    *entry)
{
  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (entry != NULL);

  gtk_item_factory_delete_item (ifactory, entry->path);
}

void
gtk_item_factory_delete_entries (GtkItemFactory         *ifactory,
				 guint                   n_entries,
				 GtkItemFactoryEntry    *entries)
{
  guint i;

  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  if (n_entries > 0)
    g_return_if_fail (entries != NULL);

  for (i = 0; i < n_entries; i++)
    gtk_item_factory_delete_item (ifactory, (entries + i)->path);
}

typedef struct
{
  guint x;
  guint y;
} MenuPos;

static void
gtk_item_factory_menu_pos (GtkMenu  *menu,
			   gint     *x,
			   gint     *y,
			   gpointer  func_data)
{
  MenuPos *mpos = func_data;

  *x = mpos->x;
  *y = mpos->y;
}

gpointer
gtk_item_factory_popup_data_from_widget (GtkWidget     *widget)
{
  GtkItemFactory *ifactory;
  
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  ifactory = gtk_item_factory_from_widget (widget);
  if (ifactory)
    return gtk_object_get_data_by_id (GTK_OBJECT (ifactory), key_id_popup_data);

  return NULL;
}

gpointer
gtk_item_factory_popup_data (GtkItemFactory *ifactory)
{
  g_return_val_if_fail (ifactory != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);

  return gtk_object_get_data_by_id (GTK_OBJECT (ifactory), key_id_popup_data);
}

static void
ifactory_delete_popup_data (GtkObject	   *object,
			    GtkItemFactory *ifactory)
{
  gtk_signal_disconnect_by_func (object,
				 GTK_SIGNAL_FUNC (ifactory_delete_popup_data),
				 ifactory);
  gtk_object_remove_data_by_id (GTK_OBJECT (ifactory), key_id_popup_data);
}

void
gtk_item_factory_popup (GtkItemFactory		*ifactory,
			guint			 x,
			guint			 y,
			guint			 mouse_button,
			guint32			 time)
{
  gtk_item_factory_popup_with_data (ifactory, NULL, NULL, x, y, mouse_button, time);
}

void
gtk_item_factory_popup_with_data (GtkItemFactory	*ifactory,
				  gpointer		 popup_data,
				  GtkDestroyNotify	 destroy,
				  guint			 x,
				  guint			 y,
				  guint			 mouse_button,
				  guint32		 time)
{
  g_return_if_fail (ifactory != NULL);
  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (GTK_IS_MENU (ifactory->widget));

  if (!GTK_WIDGET_VISIBLE (ifactory->widget))
    {
      MenuPos *mpos;

      mpos = gtk_object_get_data_by_id (GTK_OBJECT (ifactory->widget), key_id_if_menu_pos);

      if (!mpos)
	{
	  mpos = g_new0 (MenuPos, 1);
	  gtk_object_set_data_by_id_full (GTK_OBJECT (ifactory->widget),
					  key_id_if_menu_pos,
					  mpos,
					  g_free);
	}

      mpos->x = x;
      mpos->y = y;

      if (popup_data != NULL)
	{
	  gtk_object_set_data_by_id_full (GTK_OBJECT (ifactory),
					  key_id_popup_data,
					  popup_data,
					  destroy);
	  gtk_signal_connect (GTK_OBJECT (ifactory->widget),
			      "selection-done",
			      GTK_SIGNAL_FUNC (ifactory_delete_popup_data),
			      ifactory);
	}

      gtk_menu_popup (GTK_MENU (ifactory->widget),
		      NULL, NULL,
		      gtk_item_factory_menu_pos, mpos,
		      mouse_button, time);
    }
}

static guint
gtk_item_factory_parse_menu_path (GScanner            *scanner,
				  GtkItemFactoryClass *class)
{
  GtkItemFactoryItem *item;
  
  g_scanner_get_next_token (scanner);
  if (scanner->token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  g_scanner_peek_next_token (scanner);
  if (scanner->next_token != G_TOKEN_STRING)
    {
      g_scanner_get_next_token (scanner);
      return G_TOKEN_STRING;
    }

  item = g_hash_table_lookup (class->item_ht, scanner->value.v_string);
  if (!item)
    {
      item = g_chunk_new (GtkItemFactoryItem, ifactory_item_chunks);

      item->path = g_strdup (scanner->value.v_string);
      item->accelerator_key = 0;
      item->accelerator_mods = 0;
      item->modified = FALSE;
      item->in_propagation = FALSE;
      item->item_type = NULL;
      item->widgets = NULL;

      g_hash_table_insert (class->item_ht, item->path, item);
    }
  g_scanner_get_next_token (scanner);
  
  if (!item->in_propagation)
    {
      guint old_keyval;
      guint old_mods;
      
      old_keyval = item->accelerator_key;
      old_mods = item->accelerator_mods;
      gtk_accelerator_parse (scanner->value.v_string,
			     &item->accelerator_key,
			     &item->accelerator_mods);
      if (old_keyval != item->accelerator_key ||
	  old_mods != item->accelerator_mods)
	{
	  item->modified = TRUE;
	  gtk_item_factory_propagate_accelerator (item, NULL);
	}
    }
  
  g_scanner_get_next_token (scanner);
  if (scanner->token != ')')
    return ')';
  else
    return G_TOKEN_NONE;
}

static void
gtk_item_factory_parse_statement (GScanner            *scanner,
				  GtkItemFactoryClass *class)
{
  guint expected_token;
  
  g_scanner_get_next_token (scanner);
  
  if (scanner->token == G_TOKEN_SYMBOL)
    {
      guint (*parser_func) (GScanner*, GtkItemFactoryClass*);

      parser_func = scanner->value.v_symbol;

      /* check whether this is a GtkItemFactory symbol...
       */
      if (parser_func == gtk_item_factory_parse_menu_path)
	expected_token = parser_func (scanner, class);
      else
	expected_token = G_TOKEN_SYMBOL;
    }
  else
    expected_token = G_TOKEN_SYMBOL;

  /* skip rest of statement on errrors
   */
  if (expected_token != G_TOKEN_NONE)
    {
      register guint level;

      level = 1;
      if (scanner->token == ')')
	level--;
      if (scanner->token == '(')
	level++;
      
      while (!g_scanner_eof (scanner) && level > 0)
	{
	  g_scanner_get_next_token (scanner);
	  
	  if (scanner->token == '(')
	    level++;
	  else if (scanner->token == ')')
	    level--;
	}
    }
}

void
gtk_item_factory_parse_rc_string (const gchar	 *rc_string)
{
  GScanner *scanner;

  g_return_if_fail (rc_string != NULL);

  scanner = g_scanner_new (&ifactory_scanner_config);

  g_scanner_input_text (scanner, rc_string, strlen (rc_string));

  gtk_item_factory_parse_rc_scanner (scanner);

  g_scanner_destroy (scanner);
}

void
gtk_item_factory_parse_rc_scanner (GScanner *scanner)
{
  gchar *orig_cpair_comment_single;
  gpointer saved_symbol;

  g_return_if_fail (scanner != NULL);

  if (!gtk_item_factory_class)
    gtk_type_class (GTK_TYPE_ITEM_FACTORY);

  saved_symbol = g_scanner_lookup_symbol (scanner, "menu-path");
  g_scanner_remove_symbol (scanner, "menu-path");
  g_scanner_add_symbol (scanner, "menu-path", gtk_item_factory_parse_menu_path);

  orig_cpair_comment_single = scanner->config->cpair_comment_single;
  scanner->config->cpair_comment_single = gtk_item_factory_class->cpair_comment_single;

  g_scanner_peek_next_token (scanner);

  while (scanner->next_token == '(')
    {
      g_scanner_get_next_token (scanner);

      gtk_item_factory_parse_statement (scanner, gtk_item_factory_class);

      g_scanner_peek_next_token (scanner);
    }

  scanner->config->cpair_comment_single = orig_cpair_comment_single;

  g_scanner_remove_symbol (scanner, "menu-path");
  g_scanner_add_symbol (scanner, "menu-path", saved_symbol);
}

void
gtk_item_factory_parse_rc (const gchar	  *file_name)
{
  gint fd;
  GScanner *scanner;

  g_return_if_fail (file_name != NULL);

  if (!S_ISREG (g_scanner_stat_mode (file_name)))
    return;

  fd = open (file_name, O_RDONLY);
  if (fd < 0)
    return;

  scanner = g_scanner_new (&ifactory_scanner_config);

  g_scanner_input_file (scanner, fd);

  gtk_item_factory_parse_rc_scanner (scanner);

  g_scanner_destroy (scanner);

  close (fd);
}
