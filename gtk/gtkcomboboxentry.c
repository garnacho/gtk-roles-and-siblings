/* gtkcomboboxentry.c
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtkcomboboxentry.h>

#include <gtk/gtkentry.h>
#include <gtk/gtkcellrenderertext.h>

struct _GtkComboBoxEntryPrivate
{
  GtkWidget *entry;

  gint text_column;
};

static void gtk_combo_box_entry_class_init       (GtkComboBoxEntryClass *klass);
static void gtk_combo_box_entry_init             (GtkComboBoxEntry      *entry_box);

static void gtk_combo_box_entry_active_changed   (GtkComboBox           *combo_box,
                                                  gpointer               user_data);
static void gtk_combo_box_entry_contents_changed (GtkEntry              *entry,
                                                  gpointer               user_data);


GType
gtk_combo_box_entry_get_type (void)
{
  static GType combo_box_entry_type = 0;

  if (!combo_box_entry_type)
    {
      static const GTypeInfo combo_box_entry_info =
        {
          sizeof (GtkComboBoxEntryClass),
          NULL, /* base_init */
          NULL, /* base_finalize */
          (GClassInitFunc) gtk_combo_box_entry_class_init,
          NULL, /* class_finalize */
          NULL, /* class_data */
          sizeof (GtkComboBoxEntry),
          0,
          (GInstanceInitFunc) gtk_combo_box_entry_init
        };

      combo_box_entry_type = g_type_register_static (GTK_TYPE_COMBO_BOX,
                                                     "GtkComboBoxEntry",
                                                     &combo_box_entry_info,
                                                     0);
    }

  return combo_box_entry_type;
}

static void
gtk_combo_box_entry_class_init (GtkComboBoxEntryClass *klass)
{
  g_type_class_add_private ((GObjectClass *) klass,
                            sizeof (GtkComboBoxEntryPrivate));
}

static void
gtk_combo_box_entry_init (GtkComboBoxEntry *entry_box)
{
  entry_box->priv = GTK_COMBO_BOX_ENTRY_GET_PRIVATE (entry_box);
}

static void
gtk_combo_box_entry_active_changed (GtkComboBox *combo_box,
                                    gpointer     user_data)
{
  gint index;
  GtkComboBoxEntry *entry_box = GTK_COMBO_BOX_ENTRY (combo_box);

  index = gtk_combo_box_get_active (combo_box);

  g_signal_handlers_block_by_func (entry_box->priv->entry,
                                   gtk_combo_box_entry_contents_changed,
                                   combo_box);

  if (index < 0)
    gtk_entry_set_text (GTK_ENTRY (entry_box->priv->entry), "");
  else
    {
      gchar *str = NULL;
      GtkTreeIter iter;
      GtkTreeModel *model;

      model = gtk_combo_box_get_model (combo_box);

      gtk_tree_model_iter_nth_child (model, &iter, NULL, index);
      gtk_tree_model_get (model, &iter,
                          entry_box->priv->text_column, &str,
                          -1);

      gtk_entry_set_text (GTK_ENTRY (entry_box->priv->entry), str);

      g_free (str);
    }

  g_signal_handlers_unblock_by_func (entry_box->priv->entry,
                                     gtk_combo_box_entry_contents_changed,
                                     combo_box);
}

static void
gtk_combo_box_entry_contents_changed (GtkEntry *entry,
                                      gpointer  user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  g_signal_handlers_block_by_func (combo_box,
                                   gtk_combo_box_entry_active_changed,
                                   NULL);
  gtk_combo_box_set_active (combo_box, -1);
  g_signal_handlers_unblock_by_func (combo_box,
                                     gtk_combo_box_entry_active_changed,
                                     NULL);
}

/* public API */
GtkWidget *
gtk_combo_box_entry_new (GtkTreeModel *model,
                         gint          text_column)
{
  GtkWidget *ret;
  GtkCellRenderer *renderer;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (text_column >= 0, NULL);
  g_return_val_if_fail (text_column < gtk_tree_model_get_n_columns (model), NULL);

  ret = g_object_new (gtk_combo_box_entry_get_type (),
                      "model", model,
                      NULL);

  GTK_COMBO_BOX_ENTRY (ret)->priv->entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (ret),
                     GTK_COMBO_BOX_ENTRY (ret)->priv->entry);
  gtk_widget_show (GTK_COMBO_BOX_ENTRY (ret)->priv->entry);

  GTK_COMBO_BOX_ENTRY (ret)->priv->text_column = text_column;
  renderer = gtk_cell_renderer_text_new ();
  gtk_combo_box_pack_start (GTK_COMBO_BOX (ret), renderer, TRUE);
  gtk_combo_box_set_attributes (GTK_COMBO_BOX (ret), renderer,
                                "text", text_column,
                                NULL);

  g_signal_connect (GTK_COMBO_BOX_ENTRY (ret)->priv->entry, "changed",
                    G_CALLBACK (gtk_combo_box_entry_contents_changed), ret);
  g_signal_connect (ret, "changed",
                    G_CALLBACK (gtk_combo_box_entry_active_changed), NULL);

  return ret;
}

gint
gtk_combo_box_entry_get_text_column (GtkComboBoxEntry *entry_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX_ENTRY (entry_box), 0);

  return entry_box->priv->text_column;
}
