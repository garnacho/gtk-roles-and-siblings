/* gtktreeselection.h
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

#include "gtktreeselection.h"
#include "gtktreeprivate.h"
#include "gtkrbtree.h"
#include "gtkmarshalers.h"

static void gtk_tree_selection_init              (GtkTreeSelection      *selection);
static void gtk_tree_selection_class_init        (GtkTreeSelectionClass *class);

static void gtk_tree_selection_finalize          (GObject               *object);
static gint gtk_tree_selection_real_select_all   (GtkTreeSelection      *selection);
static gint gtk_tree_selection_real_unselect_all (GtkTreeSelection      *selection);
static gint gtk_tree_selection_real_select_node  (GtkTreeSelection      *selection,
						  GtkRBTree             *tree,
						  GtkRBNode             *node,
						  gboolean               select);

enum
{
  CHANGED,
  LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
static guint tree_selection_signals [LAST_SIGNAL] = { 0 };

GType
gtk_tree_selection_get_type (void)
{
  static GType selection_type = 0;

  if (!selection_type)
    {
      static const GTypeInfo selection_info =
      {
        sizeof (GtkTreeSelectionClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_tree_selection_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkTreeSelection),
	0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_tree_selection_init
      };

      selection_type =
	g_type_register_static (G_TYPE_OBJECT, "GtkTreeSelection",
				&selection_info, 0);
    }

  return selection_type;
}

static void
gtk_tree_selection_class_init (GtkTreeSelectionClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;
  parent_class = g_type_class_peek_parent (class);

  object_class->finalize = gtk_tree_selection_finalize;
  class->changed = NULL;

  tree_selection_signals[CHANGED] =
    g_signal_new ("changed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkTreeSelectionClass, changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gtk_tree_selection_init (GtkTreeSelection *selection)
{
  selection->type = GTK_SELECTION_SINGLE;
}

static void
gtk_tree_selection_finalize (GObject *object)
{
  GtkTreeSelection *selection = GTK_TREE_SELECTION (object);

  if (selection->destroy)
    {
      GtkDestroyNotify d = selection->destroy;

      selection->destroy = NULL;
      d (selection->user_data);
    }

  /* chain parent_class' handler */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * _gtk_tree_selection_new:
 *
 * Creates a new #GtkTreeSelection object.  This function should not be invoked,
 * as each #GtkTreeView will create it's own #GtkTreeSelection.
 *
 * Return value: A newly created #GtkTreeSelection object.
 **/
GtkTreeSelection*
_gtk_tree_selection_new (void)
{
  GtkTreeSelection *selection;

  selection = g_object_new (GTK_TYPE_TREE_SELECTION, NULL);

  return selection;
}

/**
 * _gtk_tree_selection_new_with_tree_view:
 * @tree_view: The #GtkTreeView.
 *
 * Creates a new #GtkTreeSelection object.  This function should not be invoked,
 * as each #GtkTreeView will create it's own #GtkTreeSelection.
 *
 * Return value: A newly created #GtkTreeSelection object.
 **/
GtkTreeSelection*
_gtk_tree_selection_new_with_tree_view (GtkTreeView *tree_view)
{
  GtkTreeSelection *selection;

  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  selection = _gtk_tree_selection_new ();
  _gtk_tree_selection_set_tree_view (selection, tree_view);

  return selection;
}

/**
 * _gtk_tree_selection_set_tree_view:
 * @selection: A #GtkTreeSelection.
 * @tree_view: The #GtkTreeView.
 *
 * Sets the #GtkTreeView of @selection.  This function should not be invoked, as
 * it is used internally by #GtkTreeView.
 **/
void
_gtk_tree_selection_set_tree_view (GtkTreeSelection *selection,
                                   GtkTreeView      *tree_view)
{
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  if (tree_view != NULL)
    g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  selection->tree_view = tree_view;
}

/**
 * gtk_tree_selection_set_mode:
 * @selection: A #GtkTreeSelection.
 * @type: The selection mode
 *
 * Sets the selection mode of the @selection.  If the previous type was
 * #GTK_SELECTION_MULTIPLE, then the anchor is kept selected, if it was
 * previously selected.
 **/
void
gtk_tree_selection_set_mode (GtkTreeSelection *selection,
			     GtkSelectionMode  type)
{
  GtkTreeSelectionFunc tmp_func;
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  if (selection->type == type)
    return;

  
  if (type == GTK_SELECTION_NONE)
    {
      /* We do this so that we unconditionally unset all rows
       */
      tmp_func = selection->user_func;
      selection->user_func = NULL;
      gtk_tree_selection_unselect_all (selection);
      selection->user_func = tmp_func;

      gtk_tree_row_reference_free (selection->tree_view->priv->anchor);
      selection->tree_view->priv->anchor = NULL;
    }
  else if (type == GTK_SELECTION_SINGLE ||
	   type == GTK_SELECTION_BROWSE)
    {
      GtkRBTree *tree = NULL;
      GtkRBNode *node = NULL;
      gint selected = FALSE;
      GtkTreePath *anchor_path = NULL;

      if (selection->tree_view->priv->anchor)
	{
          anchor_path = gtk_tree_row_reference_get_path (selection->tree_view->priv->anchor);

          if (anchor_path)
            {
              _gtk_tree_view_find_node (selection->tree_view,
                                        anchor_path,
                                        &tree,
                                        &node);

              if (node && GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
                selected = TRUE;
            }
	}

      /* We do this so that we unconditionally unset all rows
       */
      tmp_func = selection->user_func;
      selection->user_func = NULL;
      gtk_tree_selection_unselect_all (selection);
      selection->user_func = tmp_func;

      if (node && selected)
	_gtk_tree_selection_internal_select_node (selection,
						  node,
						  tree,
						  anchor_path,
						  0,
						  FALSE);
      if (anchor_path)
	gtk_tree_path_free (anchor_path);
    }

  selection->type = type;
}

/**
 * gtk_tree_selection_get_mode:
 * @selection: a #GtkTreeSelection
 *
 * Gets the selection mode for @selection. See
 * gtk_tree_selection_set_mode().
 *
 * Return value: the current selection mode
 **/
GtkSelectionMode
gtk_tree_selection_get_mode (GtkTreeSelection *selection)
{
  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), GTK_SELECTION_SINGLE);

  return selection->type;
}

/**
 * gtk_tree_selection_set_select_function:
 * @selection: A #GtkTreeSelection.
 * @func: The selection function.
 * @data: The selection function's data.
 * @destroy: The destroy function for user data.  May be NULL.
 *
 * Sets the selection function.  If set, this function is called before any node
 * is selected or unselected, giving some control over which nodes are selected.
 * The select function should return %TRUE if the state of the node may be toggled,
 * and %FALSE if the state of the node should be left unchanged.
 **/
void
gtk_tree_selection_set_select_function (GtkTreeSelection     *selection,
					GtkTreeSelectionFunc  func,
					gpointer              data,
					GtkDestroyNotify      destroy)
{
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (func != NULL);

  if (selection->destroy)
    {
      GtkDestroyNotify d = selection->destroy;

      selection->destroy = NULL;
      d (selection->user_data);
    }

  selection->user_func = func;
  selection->user_data = data;
  selection->destroy = destroy;
}

/**
 * gtk_tree_selection_get_user_data:
 * @selection: A #GtkTreeSelection.
 *
 * Returns the user data for the selection function.
 *
 * Return value: The user data.
 **/
gpointer
gtk_tree_selection_get_user_data (GtkTreeSelection *selection)
{
  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), NULL);

  return selection->user_data;
}

/**
 * gtk_tree_selection_get_tree_view:
 * @selection: A #GtkTreeSelection
 * 
 * Returns the tree view associated with @selection.
 * 
 * Return value: A #GtkTreeView
 **/
GtkTreeView *
gtk_tree_selection_get_tree_view (GtkTreeSelection *selection)
{
  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), NULL);

  return selection->tree_view;
}

/**
 * gtk_tree_selection_get_selected:
 * @selection: A #GtkTreeSelection.
 * @model: A pointer to set to the #GtkTreeModel, or NULL.
 * @iter: The #GtkTreeIter, or NULL.
 *
 * Sets @iter to the currently selected node if @selection is set to
 * #GTK_SELECTION_SINGLE or #GTK_SELECTION_BROWSE.  @iter may be NULL if you
 * just want to test if @selection has any selected nodes.  @model is filled
 * with the current model as a convenience.  This function will not work if you
 * use @selection is #GTK_SELECTION_MULTIPLE.
 *
 * Return value: TRUE, if there is a selected node.
 **/
gboolean
gtk_tree_selection_get_selected (GtkTreeSelection  *selection,
				 GtkTreeModel     **model,
				 GtkTreeIter       *iter)
{
  GtkRBTree *tree;
  GtkRBNode *node;
  GtkTreePath *anchor_path;
  gboolean retval;
  gboolean found_node;

  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), FALSE);
  g_return_val_if_fail (selection->type != GTK_SELECTION_MULTIPLE, FALSE);
  g_return_val_if_fail (selection->tree_view != NULL, FALSE);
  g_return_val_if_fail (selection->tree_view->priv->model != NULL, FALSE);

  if (model)
    *model = selection->tree_view->priv->model;

  if (selection->tree_view->priv->anchor == NULL)
    return FALSE;

  anchor_path = gtk_tree_row_reference_get_path (selection->tree_view->priv->anchor);

  if (anchor_path == NULL)
    return FALSE;

  retval = FALSE;

  found_node = !_gtk_tree_view_find_node (selection->tree_view,
                                          anchor_path,
                                          &tree,
                                          &node);

  if (found_node && GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    {
      /* we only want to return the anchor if it exists in the rbtree and
       * is selected.
       */
      if (iter == NULL)
	retval = TRUE;
      else
        retval = gtk_tree_model_get_iter (selection->tree_view->priv->model,
                                          iter,
                                          anchor_path);
    }
  else
    {
      /* We don't want to return the anchor if it isn't actually selected.
       */
      retval = FALSE;
    }

  gtk_tree_path_free (anchor_path);

  return retval;
}

/**
 * gtk_tree_selection_get_selected_rows:
 * @selection: A #GtkTreeSelection.
 * @model: A pointer to set to the #GtkTreeModel, or NULL.
 *
 * Creates a list of path of all selected rows. Additionally, if you are
 * planning on modifying the model after calling this function, you may
 * want to convert the returned list into a list of #GtkTreeRowReference<!-- -->s.
 * To do this, you can use gtk_tree_row_reference_new_proxy().
 *
 * To free the return value, use:
 * <informalexample><programlisting>
 * g_list_foreach (list, gtk_tree_path_free, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 *
 * Return value: A #GList containing a #GtkTreePath for each selected row.
 *
 * Since: 2.2
 **/
GList *
gtk_tree_selection_get_selected_rows (GtkTreeSelection   *selection,
                                      GtkTreeModel      **model)
{
  GList *list = NULL;
  GtkRBTree *tree = NULL;
  GtkRBNode *node = NULL;
  GtkTreePath *path;

  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), NULL);
  g_return_val_if_fail (selection->tree_view != NULL, NULL);
  g_return_val_if_fail (selection->tree_view->priv->model != NULL, NULL);

  if (selection->tree_view->priv->tree == NULL ||
      selection->tree_view->priv->tree->root == NULL)
    return NULL;

  if (model)
    *model = selection->tree_view->priv->model;

  if (selection->type == GTK_SELECTION_NONE)
    return NULL;
  else if (selection->type != GTK_SELECTION_MULTIPLE)
    {
      GtkTreeIter iter;

      if (gtk_tree_selection_get_selected (selection, NULL, &iter))
        {
	  GtkTreePath *path;

	  path = gtk_tree_model_get_path (*model, &iter);
	  list = g_list_append (list, path);

	  return list;
	}

      return NULL;
    }

  tree = selection->tree_view->priv->tree;
  node = selection->tree_view->priv->tree->root;

  while (node->left != tree->nil)
    node = node->left;
  path = gtk_tree_path_new_first ();

  do
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	list = g_list_append (list, gtk_tree_path_copy (path));

      if (node->children)
        {
	  tree = node->children;
	  node = tree->root;

	  while (node->left != tree->nil)
	    node = node->left;

	  gtk_tree_path_append_index (path, 0);
	}
      else
        {
	  gboolean done = FALSE;

	  do
	    {
	      node = _gtk_rbtree_next (tree, node);
	      if (node != NULL)
	        {
		  done = TRUE;
		  gtk_tree_path_next (path);
		}
	      else
	        {
		  node = tree->parent_node;
		  tree = tree->parent_tree;

		  if (!tree)
		    {
		      gtk_tree_path_free (path);
		      return list;
		    }

		  gtk_tree_path_up (path);
		}
	    }
	  while (!done);
	}
    }
  while (TRUE);

  gtk_tree_path_free (path);

  return list;
}

static void
gtk_tree_selection_count_selected_rows_helper (GtkRBTree *tree,
					       GtkRBNode *node,
					       gpointer   data)
{
  gint *count = (gint *)data;

  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    (*count)++;

  if (node->children)
    _gtk_rbtree_traverse (node->children, node->children->root,
			  G_PRE_ORDER,
			  gtk_tree_selection_count_selected_rows_helper, data);
}

/**
 * gtk_tree_selection_count_selected_rows:
 * @selection: A #GtkTreeSelection.
 *
 * Returns the number of rows that have been selected in @tree.
 *
 * Return value: The number of rows selected.
 * 
 * Since: 2.2
 **/
gint
gtk_tree_selection_count_selected_rows (GtkTreeSelection *selection)
{
  gint count = 0;
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), 0);
  g_return_val_if_fail (selection->tree_view != NULL, 0);
  g_return_val_if_fail (selection->tree_view->priv->model != NULL, 0);

  if (selection->tree_view->priv->tree == NULL ||
      selection->tree_view->priv->tree->root == NULL)
    return 0;

  if (selection->type == GTK_SELECTION_SINGLE ||
      selection->type == GTK_SELECTION_BROWSE)
    {
      if (gtk_tree_selection_get_selected (selection, NULL, NULL))
	return 1;
      else
	return 0;
    }

  tree = selection->tree_view->priv->tree;
  node = selection->tree_view->priv->tree->root;

  _gtk_rbtree_traverse (selection->tree_view->priv->tree,
                        selection->tree_view->priv->tree->root,
			G_PRE_ORDER,
			gtk_tree_selection_count_selected_rows_helper,
			&count);

  return count;
}

/* gtk_tree_selection_selected_foreach helper */
static void
model_changed (gpointer data)
{
  gboolean *stop = (gboolean *)data;

  *stop = TRUE;
}

/**
 * gtk_tree_selection_selected_foreach:
 * @selection: A #GtkTreeSelection.
 * @func: The function to call for each selected node.
 * @data: user data to pass to the function.
 *
 * Calls a function for each selected node. Note that you cannot modify
 * the tree or selection from within this function. As a result,
 * gtk_tree_selection_get_selected_rows() might be more useful.
 **/
void
gtk_tree_selection_selected_foreach (GtkTreeSelection            *selection,
				     GtkTreeSelectionForeachFunc  func,
				     gpointer                     data)
{
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;
  GtkTreeIter iter;

  guint inserted_id, deleted_id, reordered_id;
  gboolean stop = FALSE, has_next = TRUE, has_parent = TRUE;

  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->model != NULL);

  if (func == NULL ||
      selection->tree_view->priv->tree == NULL ||
      selection->tree_view->priv->tree->root == NULL)
    return;

  if (selection->type == GTK_SELECTION_SINGLE ||
      selection->type == GTK_SELECTION_BROWSE)
    {
      if (gtk_tree_row_reference_valid (selection->tree_view->priv->anchor))
	{
	  path = gtk_tree_row_reference_get_path (selection->tree_view->priv->anchor);
	  gtk_tree_model_get_iter (selection->tree_view->priv->model, &iter, path);
	  (* func) (selection->tree_view->priv->model, path, &iter, data);
	  gtk_tree_path_free (path);
	}
      return;
    }

  tree = selection->tree_view->priv->tree;
  node = selection->tree_view->priv->tree->root;
  
  while (node->left != tree->nil)
    node = node->left;

  /* connect to signals to monitor changes in treemodel */
  inserted_id = g_signal_connect_swapped (selection->tree_view->priv->model,
                                          "row_inserted",
					  G_CALLBACK (model_changed),
				          &stop);
  deleted_id = g_signal_connect_swapped (selection->tree_view->priv->model,
                                         "row_deleted",
					 G_CALLBACK (model_changed),
				         &stop);
  reordered_id = g_signal_connect_swapped (selection->tree_view->priv->model,
                                           "rows_reordered",
					   G_CALLBACK (model_changed),
				           &stop);

  /* find the node internally */
  path = gtk_tree_path_new_first ();
  gtk_tree_model_get_iter (selection->tree_view->priv->model,
			   &iter, path);

  do
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	(* func) (selection->tree_view->priv->model, path, &iter, data);

      if (stop)
	goto out;

      if (node->children)
	{
	  gboolean has_child;
	  GtkTreeIter tmp;

	  tree = node->children;
	  node = tree->root;
	  while (node->left != tree->nil)
	    node = node->left;
	  tmp = iter;
	  has_child = gtk_tree_model_iter_children (selection->tree_view->priv->model, &iter, &tmp);
	  gtk_tree_path_append_index (path, 0);

	  /* we do the sanity check at the bottom of this function */
	  if (!has_child)
	    goto out;
	}
      else
	{
	  gboolean done = FALSE;
	  do
	    {
	      node = _gtk_rbtree_next (tree, node);
	      if (node != NULL)
		{
		  gboolean has_next;

		  has_next = gtk_tree_model_iter_next (selection->tree_view->priv->model, &iter);
		  done = TRUE;
		  gtk_tree_path_next (path);

		  /* we do the sanity check at the bottom of this function */
		  if (!has_next)
		    goto out;
		}
	      else
		{
		  gboolean has_parent;
		  GtkTreeIter tmp_iter = iter;

		  node = tree->parent_node;
		  tree = tree->parent_tree;
		  if (tree == NULL)
		    {
		      /* we've run out of tree */
		      /* We're done with this function */

		      goto out;
		    }

		  has_parent = gtk_tree_model_iter_parent (selection->tree_view->priv->model, &iter, &tmp_iter);
		  gtk_tree_path_up (path);

		  /* we do the sanity check at the bottom of this function */
		  if (!has_parent)
		    goto out;
		}
	    }
	  while (!done);
	}
    }
  while (TRUE);

out:
  if (path)
    gtk_tree_path_free (path);

  g_signal_handler_disconnect (selection->tree_view->priv->model,
                               inserted_id);
  g_signal_handler_disconnect (selection->tree_view->priv->model,
                               deleted_id);
  g_signal_handler_disconnect (selection->tree_view->priv->model,
                               reordered_id);

  /* check if we have to spew a scary message */
  if (!has_next)
    TREE_VIEW_INTERNAL_ASSERT_VOID (has_next);
  if (!has_parent)
    TREE_VIEW_INTERNAL_ASSERT_VOID (has_parent);
  if (stop)
    g_warning
      ("The model has been modified from within gtk_tree_selection_foreach.\n"
       "This function is for observing the selections of the tree only.  If\n"
       "you are trying to get all selected items from the tree, try using\n"
       "gtk_tree_selection_get_selected_rows instead.\n");
}

/**
 * gtk_tree_selection_select_path:
 * @selection: A #GtkTreeSelection.
 * @path: The #GtkTreePath to be selected.
 *
 * Select the row at @path.
 **/
void
gtk_tree_selection_select_path (GtkTreeSelection *selection,
				GtkTreePath      *path)
{
  GtkRBNode *node;
  GtkRBTree *tree;
  GdkModifierType state = 0;
  gboolean ret;

  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (path != NULL);

  ret = _gtk_tree_view_find_node (selection->tree_view,
				  path,
				  &tree,
				  &node);

  if (node == NULL || GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED) ||
      ret == TRUE)
    return;

  if (selection->type == GTK_SELECTION_MULTIPLE)
    state = GDK_CONTROL_MASK;

  _gtk_tree_selection_internal_select_node (selection,
					    node,
					    tree,
					    path,
					    state,
					    FALSE);
}

/**
 * gtk_tree_selection_unselect_path:
 * @selection: A #GtkTreeSelection.
 * @path: The #GtkTreePath to be unselected.
 *
 * Unselects the row at @path.
 **/
void
gtk_tree_selection_unselect_path (GtkTreeSelection *selection,
				  GtkTreePath      *path)
{
  GtkRBNode *node;
  GtkRBTree *tree;
  gboolean ret;

  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (path != NULL);

  ret = _gtk_tree_view_find_node (selection->tree_view,
				  path,
				  &tree,
				  &node);

  if (node == NULL || !GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED) ||
      ret == TRUE)
    return;

  _gtk_tree_selection_internal_select_node (selection,
					    node,
					    tree,
					    path,
					    GDK_CONTROL_MASK,
					    TRUE);
}

/**
 * gtk_tree_selection_select_iter:
 * @selection: A #GtkTreeSelection.
 * @iter: The #GtkTreeIter to be selected.
 *
 * Selects the specified iterator.
 **/
void
gtk_tree_selection_select_iter (GtkTreeSelection *selection,
				GtkTreeIter      *iter)
{
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->model != NULL);
  g_return_if_fail (iter != NULL);

  path = gtk_tree_model_get_path (selection->tree_view->priv->model,
				  iter);

  if (path == NULL)
    return;

  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);
}


/**
 * gtk_tree_selection_unselect_iter:
 * @selection: A #GtkTreeSelection.
 * @iter: The #GtkTreeIter to be unselected.
 *
 * Unselects the specified iterator.
 **/
void
gtk_tree_selection_unselect_iter (GtkTreeSelection *selection,
				  GtkTreeIter      *iter)
{
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->model != NULL);
  g_return_if_fail (iter != NULL);

  path = gtk_tree_model_get_path (selection->tree_view->priv->model,
				  iter);

  if (path == NULL)
    return;

  gtk_tree_selection_unselect_path (selection, path);
  gtk_tree_path_free (path);
}

/**
 * gtk_tree_selection_path_is_selected:
 * @selection: A #GtkTreeSelection.
 * @path: A #GtkTreePath to check selection on.
 * 
 * Returns %TRUE if the row pointed to by @path is currently selected.  If @path
 * does not point to a valid location, %FALSE is returned
 * 
 * Return value: %TRUE if @path is selected.
 **/
gboolean
gtk_tree_selection_path_is_selected (GtkTreeSelection *selection,
				     GtkTreePath      *path)
{
  GtkRBNode *node;
  GtkRBTree *tree;
  gboolean ret;

  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (selection->tree_view != NULL, FALSE);
  g_return_val_if_fail (selection->tree_view->priv->model != NULL, FALSE);

  ret = _gtk_tree_view_find_node (selection->tree_view,
				  path,
				  &tree,
				  &node);

  if ((node == NULL) || !GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED) ||
      ret == TRUE)
    return FALSE;

  return TRUE;
}

/**
 * gtk_tree_selection_iter_is_selected:
 * @selection: A #GtkTreeSelection
 * @iter: A valid #GtkTreeIter
 * 
 * Returns %TRUE if the row at @iter is currently selected.
 * 
 * Return value: %TRUE, if @iter is selected
 **/
gboolean
gtk_tree_selection_iter_is_selected (GtkTreeSelection *selection,
				     GtkTreeIter      *iter)
{
  GtkTreePath *path;
  gboolean retval;

  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (selection->tree_view != NULL, FALSE);
  g_return_val_if_fail (selection->tree_view->priv->model != NULL, FALSE);

  path = gtk_tree_model_get_path (selection->tree_view->priv->model, iter);
  if (path == NULL)
    return FALSE;

  retval = gtk_tree_selection_path_is_selected (selection, path);
  gtk_tree_path_free (path);

  return retval;
}


/* Wish I was in python, right now... */
struct _TempTuple {
  GtkTreeSelection *selection;
  gint dirty;
};

static void
select_all_helper (GtkRBTree  *tree,
		   GtkRBNode  *node,
		   gpointer    data)
{
  struct _TempTuple *tuple = data;

  if (node->children)
    _gtk_rbtree_traverse (node->children,
			  node->children->root,
			  G_PRE_ORDER,
			  select_all_helper,
			  data);
  if (!GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    {
      tuple->dirty = gtk_tree_selection_real_select_node (tuple->selection, tree, node, TRUE) || tuple->dirty;
    }
}


/* We have a real_{un,}select_all function that doesn't emit the signal, so we
 * can use it in other places without fear of the signal being emitted.
 */
static gint
gtk_tree_selection_real_select_all (GtkTreeSelection *selection)
{
  struct _TempTuple *tuple;

  if (selection->tree_view->priv->tree == NULL)
    return FALSE;

  /* Mark all nodes selected */
  tuple = g_new (struct _TempTuple, 1);
  tuple->selection = selection;
  tuple->dirty = FALSE;

  _gtk_rbtree_traverse (selection->tree_view->priv->tree,
			selection->tree_view->priv->tree->root,
			G_PRE_ORDER,
			select_all_helper,
			tuple);
  if (tuple->dirty)
    {
      g_free (tuple);
      return TRUE;
    }
  g_free (tuple);
  return FALSE;
}

/**
 * gtk_tree_selection_select_all:
 * @selection: A #GtkTreeSelection.
 *
 * Selects all the nodes.  @selection is must be set to #GTK_SELECTION_MULTIPLE
 * mode.
 **/
void
gtk_tree_selection_select_all (GtkTreeSelection *selection)
{
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  if (selection->tree_view->priv->tree == NULL)
    return;
  g_return_if_fail (selection->type == GTK_SELECTION_MULTIPLE);

  if (gtk_tree_selection_real_select_all (selection))
    g_signal_emit (selection, tree_selection_signals[CHANGED], 0);
}

static void
unselect_all_helper (GtkRBTree  *tree,
		     GtkRBNode  *node,
		     gpointer    data)
{
  struct _TempTuple *tuple = data;

  if (node->children)
    _gtk_rbtree_traverse (node->children,
			  node->children->root,
			  G_PRE_ORDER,
			  unselect_all_helper,
			  data);
  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    {
      tuple->dirty = gtk_tree_selection_real_select_node (tuple->selection, tree, node, FALSE) || tuple->dirty;
    }
}

static gint
gtk_tree_selection_real_unselect_all (GtkTreeSelection *selection)
{
  struct _TempTuple *tuple;

  if (selection->type == GTK_SELECTION_SINGLE ||
      selection->type == GTK_SELECTION_BROWSE)
    {
      GtkRBTree *tree = NULL;
      GtkRBNode *node = NULL;
      GtkTreePath *anchor_path;

      if (selection->tree_view->priv->anchor == NULL)
	return FALSE;

      anchor_path = gtk_tree_row_reference_get_path (selection->tree_view->priv->anchor);

      if (anchor_path == NULL)
        return FALSE;

      _gtk_tree_view_find_node (selection->tree_view,
                                anchor_path,
				&tree,
				&node);

      gtk_tree_path_free (anchor_path);

      if (tree == NULL)
        return FALSE;

      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	{
	  if (gtk_tree_selection_real_select_node (selection, tree, node, FALSE))
	    {
	      gtk_tree_row_reference_free (selection->tree_view->priv->anchor);
	      selection->tree_view->priv->anchor = NULL;
	      return TRUE;
	    }
	}
      return FALSE;
    }
  else
    {
      tuple = g_new (struct _TempTuple, 1);
      tuple->selection = selection;
      tuple->dirty = FALSE;

      _gtk_rbtree_traverse (selection->tree_view->priv->tree,
                            selection->tree_view->priv->tree->root,
                            G_PRE_ORDER,
                            unselect_all_helper,
                            tuple);

      if (tuple->dirty)
        {
          g_free (tuple);
          return TRUE;
        }
      g_free (tuple);
      return FALSE;
    }
}

/**
 * gtk_tree_selection_unselect_all:
 * @selection: A #GtkTreeSelection.
 *
 * Unselects all the nodes.
 **/
void
gtk_tree_selection_unselect_all (GtkTreeSelection *selection)
{
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  if (selection->tree_view->priv->tree == NULL)
    return;
  
  if (selection->tree_view->priv->tree == NULL)
    return;

  if (gtk_tree_selection_real_unselect_all (selection))
    g_signal_emit (selection, tree_selection_signals[CHANGED], 0);
}

enum
{
  RANGE_SELECT,
  RANGE_UNSELECT
};

static gint
gtk_tree_selection_real_modify_range (GtkTreeSelection *selection,
                                      gint              mode,
				      GtkTreePath      *start_path,
				      GtkTreePath      *end_path)
{
  GtkRBNode *start_node, *end_node;
  GtkRBTree *start_tree, *end_tree;
  GtkTreePath *anchor_path = NULL;
  gboolean dirty = FALSE;

  switch (gtk_tree_path_compare (start_path, end_path))
    {
    case 1:
      _gtk_tree_view_find_node (selection->tree_view,
				end_path,
				&start_tree,
				&start_node);
      _gtk_tree_view_find_node (selection->tree_view,
				start_path,
				&end_tree,
				&end_node);
      anchor_path = end_path;
      break;
    case 0:
      _gtk_tree_view_find_node (selection->tree_view,
				start_path,
				&start_tree,
				&start_node);
      end_tree = start_tree;
      end_node = start_node;
      anchor_path = start_path;
      break;
    case -1:
      _gtk_tree_view_find_node (selection->tree_view,
				start_path,
				&start_tree,
				&start_node);
      _gtk_tree_view_find_node (selection->tree_view,
				end_path,
				&end_tree,
				&end_node);
      anchor_path = start_path;
      break;
    }

  g_return_val_if_fail (start_node != NULL, FALSE);
  g_return_val_if_fail (end_node != NULL, FALSE);

  if (anchor_path)
    {
      if (selection->tree_view->priv->anchor)
	gtk_tree_row_reference_free (selection->tree_view->priv->anchor);

      selection->tree_view->priv->anchor =
	gtk_tree_row_reference_new_proxy (G_OBJECT (selection->tree_view),
	                                  selection->tree_view->priv->model,
					  anchor_path);
    }

  do
    {
      dirty |= gtk_tree_selection_real_select_node (selection, start_tree, start_node, (mode == RANGE_SELECT)?TRUE:FALSE);

      if (start_node == end_node)
	break;

      if (start_node->children)
	{
	  start_tree = start_node->children;
	  start_node = start_tree->root;
	  while (start_node->left != start_tree->nil)
	    start_node = start_node->left;
	}
      else
	{
	  _gtk_rbtree_next_full (start_tree, start_node, &start_tree, &start_node);
	  if (start_tree == NULL)
	    {
	      /* we just ran out of tree.  That means someone passed in bogus values.
	       */
	      return dirty;
	    }
	}
    }
  while (TRUE);

  return dirty;
}

/**
 * gtk_tree_selection_select_range:
 * @selection: A #GtkTreeSelection.
 * @start_path: The initial node of the range.
 * @end_path: The final node of the range.
 *
 * Selects a range of nodes, determined by @start_path and @end_path inclusive.
 **/
void
gtk_tree_selection_select_range (GtkTreeSelection *selection,
				 GtkTreePath      *start_path,
				 GtkTreePath      *end_path)
{
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);

  if (gtk_tree_selection_real_modify_range (selection, RANGE_SELECT, start_path, end_path))
    g_signal_emit (selection, tree_selection_signals[CHANGED], 0);
}

/**
 * gtk_tree_selection_unselect_range:
 * @selection: A #GtkTreeSelection.
 * @start_path: The initial node of the range.
 * @end_path: The initial node of the range.
 *
 * Unselects a range of nodes, determined by @start_path and @end_path
 * inclusive.
 *
 * Since: 2.2
 **/
void
gtk_tree_selection_unselect_range (GtkTreeSelection *selection,
                                   GtkTreePath      *start_path,
				   GtkTreePath      *end_path)
{
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);

  if (gtk_tree_selection_real_modify_range (selection, RANGE_UNSELECT, start_path, end_path))
    g_signal_emit (selection, tree_selection_signals[CHANGED], 0);
}

/* Called internally by gtktreeview.c It handles actually selecting the tree.
 */

/*
 * docs about the 'override_browse_mode', we set this flag when we want to
 * unset select the node and override the select browse mode behaviour (that is
 * 'one node should *always* be selected').
 */
void
_gtk_tree_selection_internal_select_node (GtkTreeSelection *selection,
					  GtkRBNode        *node,
					  GtkRBTree        *tree,
					  GtkTreePath      *path,
					  GdkModifierType   state,
					  gboolean          override_browse_mode)
{
  gint flags;
  gint dirty = FALSE;
  GtkTreePath *anchor_path = NULL;

  if (selection->type == GTK_SELECTION_NONE)
    return;

  if (selection->tree_view->priv->anchor)
    anchor_path = gtk_tree_row_reference_get_path (selection->tree_view->priv->anchor);

  if (selection->type == GTK_SELECTION_SINGLE ||
      selection->type == GTK_SELECTION_BROWSE)
    {
      /* just unselect */
      if (selection->type == GTK_SELECTION_BROWSE && override_browse_mode)
        {
	  dirty = gtk_tree_selection_real_unselect_all (selection);
	}
      /* Did we try to select the same node again? */
      else if (selection->type == GTK_SELECTION_SINGLE &&
	       anchor_path && gtk_tree_path_compare (path, anchor_path) == 0)
	{
	  if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
	    {
	      dirty = gtk_tree_selection_real_unselect_all (selection);
	    }
	}
      else
	{
	  if (anchor_path)
	    {
	      /* We only want to select the new node if we can unselect the old one,
	       * and we can select the new one. */
	      if (selection->user_func)
		{
		  if ((*selection->user_func) (selection, selection->tree_view->priv->model, path,
					       GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED),
					       selection->user_data))
		    dirty = TRUE;
		}
	      else
		{
		  dirty = TRUE;
		}

	      /* if dirty is FALSE, we weren't able to select the new one, otherwise, we try to
	       * unselect the new one
	       */
	      if (dirty)
		dirty = gtk_tree_selection_real_unselect_all (selection);

	      /* if dirty is TRUE at this point, we successfully unselected the
	       * old one, and can then select the new one */
	      if (dirty)
		{
		  if (selection->tree_view->priv->anchor)
		    gtk_tree_row_reference_free (selection->tree_view->priv->anchor);
		  if (gtk_tree_selection_real_select_node (selection, tree, node, TRUE))
		    {
		      selection->tree_view->priv->anchor =
			gtk_tree_row_reference_new_proxy (G_OBJECT (selection->tree_view), selection->tree_view->priv->model, path);
		    }
		}
	    }
	  else
	    {
	      if (gtk_tree_selection_real_select_node (selection, tree, node, TRUE))
		{
		  dirty = TRUE;
		  if (selection->tree_view->priv->anchor)
		    gtk_tree_row_reference_free (selection->tree_view->priv->anchor);

		  selection->tree_view->priv->anchor =
		    gtk_tree_row_reference_new_proxy (G_OBJECT (selection->tree_view), selection->tree_view->priv->model, path);
		}
	    }
	}
    }
  else if (selection->type == GTK_SELECTION_MULTIPLE)
    {
      if (((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) && (anchor_path == NULL))
	{
	  if (selection->tree_view->priv->anchor)
	    gtk_tree_row_reference_free (selection->tree_view->priv->anchor);

	  selection->tree_view->priv->anchor =
	    gtk_tree_row_reference_new_proxy (G_OBJECT (selection->tree_view), selection->tree_view->priv->model, path);
	  dirty = gtk_tree_selection_real_select_node (selection, tree, node, TRUE);
	}
      else if ((state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == (GDK_SHIFT_MASK|GDK_CONTROL_MASK))
	{
	  gtk_tree_selection_select_range (selection,
					   anchor_path,
					   path);
	}
      else if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
	{
	  flags = node->flags;
	  if (selection->tree_view->priv->anchor)
	    gtk_tree_row_reference_free (selection->tree_view->priv->anchor);

	  selection->tree_view->priv->anchor =
	    gtk_tree_row_reference_new_proxy (G_OBJECT (selection->tree_view), selection->tree_view->priv->model, path);

	  if ((flags & GTK_RBNODE_IS_SELECTED) == GTK_RBNODE_IS_SELECTED)
	    dirty |= gtk_tree_selection_real_select_node (selection, tree, node, FALSE);
	  else
	    dirty |= gtk_tree_selection_real_select_node (selection, tree, node, TRUE);
	}
      else if ((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
	{
	  dirty = gtk_tree_selection_real_unselect_all (selection);
	  dirty |= gtk_tree_selection_real_modify_range (selection,
                                                         RANGE_SELECT,
							 anchor_path,
							 path);
	}
      else
	{
	  dirty = gtk_tree_selection_real_unselect_all (selection);

	  if (selection->tree_view->priv->anchor)
	    gtk_tree_row_reference_free (selection->tree_view->priv->anchor);

	  selection->tree_view->priv->anchor =
	    gtk_tree_row_reference_new_proxy (G_OBJECT (selection->tree_view), selection->tree_view->priv->model, path);

	  dirty |= gtk_tree_selection_real_select_node (selection, tree, node, TRUE);
	}
    }

  if (anchor_path)
    gtk_tree_path_free (anchor_path);

  if (dirty)
    g_signal_emit (selection, tree_selection_signals[CHANGED], 0);
}

/* NOTE: Any {un,}selection ever done _MUST_ be done through this function!
 */

static gint
gtk_tree_selection_real_select_node (GtkTreeSelection *selection,
				     GtkRBTree        *tree,
				     GtkRBNode        *node,
				     gboolean          select)
{
  gboolean selected = FALSE;
  GtkTreePath *path = NULL;

  select = !! select;

  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED) != select)
    {
      path = _gtk_tree_view_find_path (selection->tree_view, tree, node);
      if (selection->user_func)
	{
	  if ((*selection->user_func) (selection, selection->tree_view->priv->model, path,
                                       GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED),
                                       selection->user_data))
	    selected = TRUE;
	}
      else
	selected = TRUE;
      gtk_tree_path_free (path);
    }

  if (selected == TRUE)
    {
      node->flags ^= GTK_RBNODE_IS_SELECTED;

      _gtk_tree_view_queue_draw_node (selection->tree_view, tree, node, NULL);
      
      return TRUE;
    }

  return FALSE;
}

