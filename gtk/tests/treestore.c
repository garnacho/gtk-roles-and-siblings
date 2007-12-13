/* Extensive GtkTreeStore tests.
 * Copyright (C) 2007  Imendio AB
 * Authors: Kristian Rietveld  <kris@imendio.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* To do:
 *  - All the to do items from liststore.c, plus:
 *  - We only test in the root level, we also need all tests "duplicated"
 *    for child levels.
 *  - And we also need tests for creating these child levels, etc.
 */

#include <glib/gtestutils.h>
#include <gtk/gtk.h>

static inline gboolean
iters_equal (GtkTreeIter *a,
	     GtkTreeIter *b)
{
  if (a->stamp != b->stamp)
    return FALSE;

  if (a->user_data != b->user_data)
    return FALSE;

  /* user_data2 and user_data3 are not used in GtkTreeStore */

  return TRUE;
}

/*
 * Fixture
 */
typedef struct
{
  GtkTreeIter iter[5];
  GtkTreeStore *store;
} TreeStore;

static void
tree_store_setup (TreeStore     *fixture,
		  gconstpointer  test_data)
{
  int i;

  fixture->store = gtk_tree_store_new (1, G_TYPE_INT);

  for (i = 0; i < 5; i++)
    {
      gtk_tree_store_insert (fixture->store, &fixture->iter[i], NULL, i);
      gtk_tree_store_set (fixture->store, &fixture->iter[i], 0, i, -1);
    }
}

static void
tree_store_teardown (TreeStore     *fixture,
		     gconstpointer  test_data)
{
  g_object_unref (fixture->store);
}

/*
 * The actual tests.
 */

static void
check_model (TreeStore *fixture,
	     gint      *new_order,
	     gint       skip)
{
  int i;
  GtkTreePath *path;

  path = gtk_tree_path_new ();
  gtk_tree_path_down (path);

  /* Check validity of the model and validity of the iters-persistent
   * claim.
   */
  for (i = 0; i < 5; i++)
    {
      GtkTreeIter iter;

      if (i == skip)
	continue;

      /* The saved iterator at new_order[i] should match the iterator
       * at i.
       */

      gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store),
			       &iter, path);

      g_assert (gtk_tree_store_iter_is_valid (fixture->store, &iter));
      g_assert (iters_equal (&iter, &fixture->iter[new_order[i]]));

      gtk_tree_path_next (path);
    }

  gtk_tree_path_free (path);
}

/* removal */
static void
tree_store_test_remove_begin (TreeStore     *fixture,
			      gconstpointer  user_data)
{
  int new_order[5] = { -1, 1, 2, 3, 4 };
  GtkTreePath *path;
  GtkTreeIter iter;

  /* Remove node at 0 */
  path = gtk_tree_path_new_from_indices (0, -1);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store), &iter, path);
  gtk_tree_path_free (path);

  g_assert (gtk_tree_store_remove (fixture->store, &iter) == TRUE);
  g_assert (!gtk_tree_store_iter_is_valid (fixture->store, &fixture->iter[0]));
  g_assert (iters_equal (&iter, &fixture->iter[1]));

  check_model (fixture, new_order, 0);
}

static void
tree_store_test_remove_middle (TreeStore     *fixture,
			       gconstpointer  user_data)
{
  int new_order[5] = { 0, 1, -1, 3, 4 };
  GtkTreePath *path;
  GtkTreeIter iter;

  /* Remove node at 2 */
  path = gtk_tree_path_new_from_indices (2, -1);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store), &iter, path);
  gtk_tree_path_free (path);

  g_assert (gtk_tree_store_remove (fixture->store, &iter) == TRUE);
  g_assert (!gtk_tree_store_iter_is_valid (fixture->store, &fixture->iter[2]));
  g_assert (iters_equal (&iter, &fixture->iter[3]));

  check_model (fixture, new_order, 2);
}

static void
tree_store_test_remove_end (TreeStore     *fixture,
			    gconstpointer  user_data)
{
  int new_order[5] = { 0, 1, 2, 3, -1 };
  GtkTreePath *path;
  GtkTreeIter iter;

  /* Remove node at 4 */
  path = gtk_tree_path_new_from_indices (4, -1);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store), &iter, path);
  gtk_tree_path_free (path);

  g_assert (gtk_tree_store_remove (fixture->store, &iter) == FALSE);
  g_assert (!gtk_tree_store_iter_is_valid (fixture->store, &fixture->iter[4]));

  check_model (fixture, new_order, 4);
}

static void
tree_store_test_clear (TreeStore     *fixture,
		       gconstpointer  user_data)
{
  int i;

  gtk_tree_store_clear (fixture->store);

  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (fixture->store), NULL) == 0);

  for (i = 0; i < 5; i++)
    g_assert (!gtk_tree_store_iter_is_valid (fixture->store, &fixture->iter[i]));
}

/* reorder */

static void
tree_store_test_reorder (TreeStore     *fixture,
			 gconstpointer  user_data)
{
  int new_order[5] = { 4, 1, 0, 2, 3 };

  gtk_tree_store_reorder (fixture->store, NULL, new_order);
  check_model (fixture, new_order, -1);
}

/* swapping */

static void
tree_store_test_swap_begin (TreeStore     *fixture,
		            gconstpointer  user_data)
{
  /* We swap nodes 0 and 1 at the beginning */
  int new_order[5] = { 1, 0, 2, 3, 4 };

  GtkTreeIter iter_a;
  GtkTreeIter iter_b;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_a, "0"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_b, "1"));

  gtk_tree_store_swap (fixture->store, &iter_a, &iter_b);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_swap_middle_next (TreeStore     *fixture,
		                  gconstpointer  user_data)
{
  /* We swap nodes 2 and 3 in the middle that are next to each other */
  int new_order[5] = { 0, 1, 3, 2, 4 };

  GtkTreeIter iter_a;
  GtkTreeIter iter_b;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_a, "2"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_b, "3"));

  gtk_tree_store_swap (fixture->store, &iter_a, &iter_b);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_swap_middle_apart (TreeStore     *fixture,
		                   gconstpointer  user_data)
{
  /* We swap nodes 1 and 3 in the middle that are apart from each other */
  int new_order[5] = { 0, 3, 2, 1, 4 };

  GtkTreeIter iter_a;
  GtkTreeIter iter_b;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_a, "1"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_b, "3"));

  gtk_tree_store_swap (fixture->store, &iter_a, &iter_b);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_swap_end (TreeStore     *fixture,
		          gconstpointer  user_data)
{
  /* We swap nodes 3 and 4 at the end */
  int new_order[5] = { 0, 1, 2, 4, 3 };

  GtkTreeIter iter_a;
  GtkTreeIter iter_b;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_a, "3"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_b, "4"));

  gtk_tree_store_swap (fixture->store, &iter_a, &iter_b);
  check_model (fixture, new_order, -1);
}

/* move after */

static void
tree_store_test_move_after_from_start (TreeStore     *fixture,
				       gconstpointer  user_data)
{
  /* We move node 0 after 2 */
  int new_order[5] = { 1, 2, 0, 3, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "0"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "2"));

  gtk_tree_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_after_next (TreeStore     *fixture,
			         gconstpointer  user_data)
{
  /* We move node 2 after 3 */
  int new_order[5] = { 0, 1, 3, 2, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "3"));

  gtk_tree_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_after_apart (TreeStore     *fixture,
			          gconstpointer  user_data)
{
  /* We move node 1 after 3 */
  int new_order[5] = { 0, 2, 3, 1, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "1"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "3"));

  gtk_tree_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_after_end (TreeStore     *fixture,
			        gconstpointer  user_data)
{
  /* We move node 2 after 4 */
  int new_order[5] = { 0, 1, 3, 4, 2 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "4"));

  gtk_tree_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_after_from_end (TreeStore     *fixture,
			             gconstpointer  user_data)
{
  /* We move node 4 after 1 */
  int new_order[5] = { 0, 1, 4, 2, 3 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "4"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "1"));

  gtk_tree_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_after_change_ends (TreeStore     *fixture,
			                gconstpointer  user_data)
{
  /* We move 0 after 4, this will cause both the head and tail ends to
   * change.
   */
  int new_order[5] = { 1, 2, 3, 4, 0 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "0"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "4"));

  gtk_tree_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_after_NULL (TreeStore     *fixture,
			         gconstpointer  user_data)
{
  /* We move node 2, NULL should prepend */
  int new_order[5] = { 2, 0, 1, 3, 4 };

  GtkTreeIter iter;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));

  gtk_tree_store_move_after (fixture->store, &iter, NULL);
  check_model (fixture, new_order, -1);
}

/* move before */

static void
tree_store_test_move_before_next (TreeStore     *fixture,
		                  gconstpointer  user_data)
{
  /* We move node 3 before 2 */
  int new_order[5] = { 0, 1, 3, 2, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "3"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "2"));

  gtk_tree_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_before_apart (TreeStore     *fixture,
				   gconstpointer  user_data)
{
  /* We move node 1 before 3 */
  int new_order[5] = { 0, 2, 1, 3, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "1"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "3"));

  gtk_tree_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_before_to_start (TreeStore     *fixture,
				      gconstpointer  user_data)
{
  /* We move node 2 before 0 */
  int new_order[5] = { 2, 0, 1, 3, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "0"));

  gtk_tree_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_before_from_end (TreeStore     *fixture,
			              gconstpointer  user_data)
{
  /* We move node 4 before 2 (replace end) */
  int new_order[5] = { 0, 1, 4, 2, 3 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "4"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "2"));

  gtk_tree_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_before_change_ends (TreeStore     *fixture,
				         gconstpointer  user_data)
{
  /* We move node 4 before 0 */
  int new_order[5] = { 4, 0, 1, 2, 3 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "4"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "0"));

  gtk_tree_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
tree_store_test_move_before_NULL (TreeStore     *fixture,
			          gconstpointer  user_data)
{
  /* We move node 2, NULL should append */
  int new_order[5] = { 0, 1, 3, 4, 2 };

  GtkTreeIter iter;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));

  gtk_tree_store_move_before (fixture->store, &iter, NULL);
  check_model (fixture, new_order, -1);
}

/* main */

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  /* insertion (FIXME) */

  /* setting values (FIXME) */

  /* removal */
  g_test_add ("/tree-store/remove-begin", TreeStore, NULL,
	      tree_store_setup, tree_store_test_remove_begin,
	      tree_store_teardown);
  g_test_add ("/tree-store/remove-middle", TreeStore, NULL,
	      tree_store_setup, tree_store_test_remove_middle,
	      tree_store_teardown);
  g_test_add ("/tree-store/remove-end", TreeStore, NULL,
	      tree_store_setup, tree_store_test_remove_end,
	      tree_store_teardown);

  g_test_add ("/tree-store/clear", TreeStore, NULL,
	      tree_store_setup, tree_store_test_clear,
	      tree_store_teardown);

  /* reordering */
  g_test_add ("/tree-store/reorder", TreeStore, NULL,
	      tree_store_setup, tree_store_test_reorder,
	      tree_store_teardown);

  /* swapping */
  g_test_add ("/tree-store/swap-begin", TreeStore, NULL,
	      tree_store_setup, tree_store_test_swap_begin,
	      tree_store_teardown);
  g_test_add ("/tree-store/swap-middle-next", TreeStore, NULL,
	      tree_store_setup, tree_store_test_swap_middle_next,
	      tree_store_teardown);
  g_test_add ("/tree-store/swap-middle-apart", TreeStore, NULL,
	      tree_store_setup, tree_store_test_swap_middle_apart,
	      tree_store_teardown);
  g_test_add ("/tree-store/swap-end", TreeStore, NULL,
	      tree_store_setup, tree_store_test_swap_end,
	      tree_store_teardown);

  /* moving */
  g_test_add ("/tree-store/move-after-from-start", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_after_from_start,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-after-next", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_after_next,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-after-apart", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_after_apart,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-after-end", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_after_end,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-after-from-end", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_after_from_end,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-after-change-ends", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_after_change_ends,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-after-NULL", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_after_NULL,
	      tree_store_teardown);

  g_test_add ("/tree-store/move-before-next", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_before_next,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-before-apart", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_before_apart,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-before-to-start", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_before_to_start,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-before-from-end", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_before_from_end,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-before-change-ends", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_before_change_ends,
	      tree_store_teardown);
  g_test_add ("/tree-store/move-before-NULL", TreeStore, NULL,
	      tree_store_setup, tree_store_test_move_before_NULL,
	      tree_store_teardown);

  return g_test_run ();
}
